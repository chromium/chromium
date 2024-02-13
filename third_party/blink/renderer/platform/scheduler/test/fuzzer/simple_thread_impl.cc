#include "third_party/blink/renderer/platform/scheduler/test/fuzzer/simple_thread_impl.h"

#include "third_party/blink/renderer/platform/scheduler/test/fuzzer/thread_manager.h"
#include "third_party/blink/renderer/platform/scheduler/test/fuzzer/thread_pool_manager.h"

namespace base {
namespace sequence_manager {

SimpleThreadImpl::SimpleThreadImpl(ThreadPoolManager* thread_pool_manager,
                                   base::TimeTicks initial_time,
                                   ThreadCallback callback)
    : SimpleThread("TestThread"),
      thread_pool_manager_(thread_pool_manager),
      initial_time_(initial_time),
      callback_(std::move(callback)) {
  DCHECK(thread_pool_manager_);
}

void SimpleThreadImpl::Run() {
  std::unique_ptr<ThreadManager> thread_manager =
      std::make_unique<ThreadManager>(initial_time_,
                                      thread_pool_manager_->processor());
  std::move(callback_).Run(thread_manager.get());
  thread_can_shutdown_.Wait();
}

SimpleThreadImpl::~SimpleThreadImpl() {
  thread_can_shutdown_.Signal();
  Join();
}

}  // namespace sequence_manager
}  // namespace base
