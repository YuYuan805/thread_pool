#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
public:
  ~ThreadPool() {
    {
      std::unique_lock<std::mutex> ul(mtx); //
      stop = true; // 到达析构函数执行时机时，线程池将被终止
    }

    con_var.notify_all(); // 唤醒所有线程
    for (auto &t : thread_vector) {
      t.join(); // 等待所有线程执行完毕
    }
  }

  template <class F, class... Args> // 函数可变参数模板
  static void
  addTask(F &&f,
          Args &&...args) { // 使用完美转发：右值引用允许将参数（函数对象 f 和
                            // args...）以它们原始的左值或右值形式传递，
    /// 而不会丧失它们的值类别信息。这意味着你可以在 addTask
    /// 中将参数原封不动地传递给内部的
    /// std::bind，确保不会进行不必要的拷贝或移动操作。

    std::function<void()> task =
        std::bind(std::forward<F>(f), std::forward<Args>(args)...);

    {
      std::unique_lock<std::mutex> ul(mtx);
      task_queue.emplace(std::move(task));
    }

    con_var.notify_one();
  }

  static ThreadPool &getExamples() {
    static ThreadPool threadpool;
    return threadpool;
  }

  ThreadPool(ThreadPool &t) = delete;
  ThreadPool &operator=(ThreadPool &t) = delete;

private:
  ThreadPool() {
    for (int i{}; i < 16; i++) {
      thread_vector.emplace_back(
          [this] { // emplace_back使得对象可以直接在实参列表里构造
            while (true) {
              std::unique_lock<std::mutex> ul(mtx);
              con_var.wait(ul, [this] { // 每个线程在此等候任务队列加入新任务
                return (!task_queue.empty()) ||
                       (stop); // 直到任务队列不为空或线程池被终止
              });

              // 开始执行任务……
              if (stop &&
                  task_queue
                      .empty()) { // 如果是线程池被终止并且任务队列为空了结束函数
                return;
              } else { // 否则就开始执行任务
                std::function<void()> task(std::move(
                    task_queue
                        .front())); // 使用move移动语义移走任务队列的任务到task变量里
                task_queue.pop(); // 删除任务队列里的一个任务
                ul.unlock();      // 解除互斥锁
                task();           // 执行任务
              }
            }
          });
    }
  }

private:
  static std::vector<std::thread> thread_vector;       // 一个线程数组
  static std::queue<std::function<void()>> task_queue; // 一个任务队列
  static std::mutex mtx;                               // 一个互斥锁
  static std::condition_variable con_var;              // 一个条件变量
  static bool stop; // 一个终止变量，表示线程池什么时候终止
};

std::vector<std::thread> ThreadPool::thread_vector;
std::queue<std::function<void()>> ThreadPool::task_queue;
std::mutex ThreadPool::mtx;
std::condition_variable ThreadPool::con_var;
bool ThreadPool::stop;

int main() {
  std::vector<int> arr{22, 3, 54, 554, 35};
  for (int &i : arr)
    ThreadPool::getExamples().addTask([i] {
      std::this_thread::sleep_for(std::chrono::microseconds(i));
      std::cout << i << std::endl;
    });

  return 0;
}