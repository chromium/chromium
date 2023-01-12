#include "third_party/blink/renderer/platform/scheduler/test/fuzzer/thread_pool_manager.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "third_party/blink/renderer/platform/scheduler/test/fuzzer/sequence_manager_fuzzer_processor.h"
#include "third_party/blink/renderer/platform/scheduler/test/fuzzer/simple_thread_impl.h"
#include "third_party/blink/renderer/platform/scheduler/test/fuzzer/thread_manager.h"

namespace base {
namespace sequence_manager {

ThreadPoolManager::ThreadPoolManager(SequenceManagerFuzzerProcessor* processor)
    : processor_(processor),
      next_time_(base::TimeTicks::Max()),
      ready_to_compute_time_(&lock_),
      ready_to_advance_time_(&lock_),
      ready_to_terminate_(&lock_),
      ready_to_execute_threads_(&lock_),
      ready_for_next_round_(&lock_),
      threads_waiting_to_compute_time_(0),
      threads_waiting_to_advance_time_(0),
      threads_ready_for_next_round_(0),
      threads_ready_to_terminate_(0),
      all_threads_ready_(true),
      initial_threads_created_(false) {
  DCHECK(processor_);
}

ThreadPoolManager::~ThreadPoolManager() = default;

void ThreadPoolManager::CreateThread(
    const google::protobuf::RepeatedPtrField<
        SequenceManagerTestDescription::Action>& initial_thread_actions,
    base::TimeTicks time) {
  SimpleThread* thread;
  {
    AutoLock lock(lock_);
    threads_.push_back(std::make_unique<SimpleThreadImpl>(
        this, time,
        BindOnce(&ThreadPoolManager::StartThread, Unretained(this),
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

void ThreadPoolManager::AdvanceClockSynchronouslyByPendingTaskDelay(
    ThreadManager* thread_manager) {
  ThreadReadyToComputeTime();

  {
    AutoLock lock(lock_);
    while (threads_waiting_to_compute_time_ != threads_.size())
      ready_to_compute_time_.Wait();
    next_time_ =
        std::min(next_time_, thread_manager->NowTicks() +
                                 thread_manager->NextPendingTaskDelay());
    threads_waiting_to_advance_time_++;
    if (threads_waiting_to_advance_time_ == threads_.size()) {
      threads_waiting_to_compute_time_ = 0;
      ready_to_advance_time_.Broadcast();
    }
  }

  AdvanceThreadClock(thread_manager);
}

void ThreadPoolManager::AdvanceClockSynchronouslyToTime(
    ThreadManager* thread_manager,
    base::TimeTicks time) {
  ThreadReadyToComputeTime();
  {
    AutoLock lock(lock_);
    while (threads_waiting_to_compute_time_ != threads_.size())
      ready_to_compute_time_.Wait();
    next_time_ = std::min(next_time_, time);
    threads_waiting_to_advance_time_++;
    if (threads_waiting_to_advance_time_ == threads_.size()) {
      threads_waiting_to_compute_time_ = 0;
      ready_to_advance_time_.Broadcast();
    }
  }
  AdvanceThreadClock(thread_manager);
}

void ThreadPoolManager::ThreadReadyToComputeTime() {
  AutoLock lock(lock_);
  while (!all_threads_ready_)
    ready_for_next_round_.Wait();
  threads_waiting_to_compute_time_++;
  if (threads_waiting_to_compute_time_ == threads_.size()) {
    all_threads_ready_ = false;
    ready_to_compute_time_.Broadcast();
  }
}

void ThreadPoolManager::AdvanceThreadClock(ThreadManager* thread_manager) {
  AutoLock lock(lock_);
  while (threads_waiting_to_advance_time_ != threads_.size())
    ready_to_advance_time_.Wait();
  thread_manager->AdvanceMockTickClock(next_time_ - thread_manager->NowTicks());
  threads_ready_for_next_round_++;
  if (threads_ready_for_next_round_ == threads_.size()) {
    threads_waiting_to_advance_time_ = 0;
    threads_ready_for_next_round_ = 0;
    all_threads_ready_ = true;
    next_time_ = base::TimeTicks::Max();
    ready_for_next_round_.Broadcast();
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
  if (threads_.empty())
    return;
  AutoLock lock(lock_);
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

Vector<ThreadManager*> ThreadPoolManager::GetAllThreadManagers() {
  AutoLock lock(lock_);
  return thread_managers_;
}

}  // namespace sequence_manager
}  // namespace base
