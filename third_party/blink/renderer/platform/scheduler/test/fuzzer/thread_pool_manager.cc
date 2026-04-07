#include "third_party/blink/renderer/platform/scheduler/test/fuzzer/thread_pool_manager.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "third_party/blink/renderer/platform/scheduler/test/fuzzer/sequence_manager_fuzzer_processor.h"
#include "third_party/blink/renderer/platform/scheduler/test/fuzzer/simple_thread_impl.h"
#include "third_party/blink/renderer/platform/scheduler/test/fuzzer/thread_manager.h"

namespace base {
namespace sequence_manager {

ThreadPoolManager::ThreadPoolManager(SequenceManagerFuzzerProcessor* processor,
                                     base::TimeTicks initial_time)
    : processor_(processor),
      next_time_(base::TimeTicks::Max()),
      now_(initial_time),
      ready_to_terminate_(&lock_),
      ready_to_execute_threads_(&lock_),
      ready_to_advance_time_(&lock_),
      initial_threads_created_(false) {
  DCHECK(processor_);
  DCHECK(!initial_time.is_null())
      << "A zero clock is not allowed as empty base::TimeTicks have a special "
         "value "
         "(i.e. base::TimeTicks::is_null())";
}

ThreadPoolManager::~ThreadPoolManager() = default;

void ThreadPoolManager::CreateThread(
    const google::protobuf::RepeatedPtrField<
        SequenceManagerTestDescription::Action>& initial_thread_actions) {
  SimpleThread* thread;
  {
    AutoLock lock(lock_);
    threads_.push_back(std::make_unique<SimpleThreadImpl>(
        this, BindOnce(&ThreadPoolManager::StartThread, Unretained(this),
                       initial_thread_actions)));
    thread = threads_.back().get();
  }
  thread->Start();
}

void ThreadPoolManager::StartThread(
    const google::protobuf::RepeatedPtrField<
        SequenceManagerTestDescription::Action>& initial_thread_actions,
    ThreadManager* thread_manager) {
  {
    AutoLock lock(lock_);
    thread_managers_.push_back(thread_manager);
    while (!initial_threads_created_)
      ready_to_execute_threads_.Wait();
  }
  thread_manager->ExecuteThread(initial_thread_actions);
}

base::TimeTicks ThreadPoolManager::NowTicks() const {
  base::AutoLock lock(lock_);
  return now_;
}

bool ThreadPoolManager::MaybeFastForwardToWakeUp(
    std::optional<sequence_manager::WakeUp> next_wake_up) {
  AutoLock lock(lock_);
  if (next_wake_up) {
    next_time_ = std::min(next_time_, next_wake_up->time);
  }
  AdvanceClockSynchronouslyImpl();
  return !now_.is_max();
}

void ThreadPoolManager::AdvanceClockSynchronouslyToTime(base::TimeTicks time) {
  AutoLock lock(lock_);
  next_time_ = now_;
  AdvanceClockSynchronouslyImpl();
  while (now_ != time) {
    next_time_ = std::min(next_time_, time);
    AdvanceClockSynchronouslyImpl();
  }
}

void ThreadPoolManager::AdvanceClockSynchronouslyImpl() {
  threads_waiting_to_advance_time_++;
  if (threads_waiting_to_advance_time_ == threads_.size()) {
    threads_waiting_to_advance_time_ = 0;
    now_ = std::exchange(next_time_, base::TimeTicks::Max());
    ++barrier_step_;
    ready_to_advance_time_.Broadcast();
  } else {
    uint64_t current_step = barrier_step_;
    while (barrier_step_ == current_step) {
      ready_to_advance_time_.Wait();
    }
  }
}

void ThreadPoolManager::StartInitialThreads() {
  {
    AutoLock lock(lock_);
    initial_threads_created_ = true;
  }
  ready_to_execute_threads_.Broadcast();
}

void ThreadPoolManager::WaitForAllThreads() {
  AutoLock lock(lock_);
  if (threads_.empty())
    return;
  while (threads_ready_to_terminate_ != threads_.size())
    ready_to_terminate_.Wait();
}

void ThreadPoolManager::ThreadDone() {
  AutoLock lock(lock_);
  threads_ready_to_terminate_++;
  if (threads_ready_to_terminate_ == threads_.size()) {
    // Only the main thread waits for this event.
    ready_to_terminate_.Signal();
  }
}

SequenceManagerFuzzerProcessor* ThreadPoolManager::processor() const {
  return processor_;
}

ThreadManager* ThreadPoolManager::GetThreadManagerFor(uint64_t thread_id) {
  AutoLock lock(lock_);
  if (thread_managers_.empty())
    return nullptr;
  int id = thread_id % thread_managers_.size();
  return thread_managers_[id];
}

blink::Vector<ThreadManager*> ThreadPoolManager::GetAllThreadManagers() {
  AutoLock lock(lock_);
  return thread_managers_;
}

}  // namespace sequence_manager
}  // namespace base
