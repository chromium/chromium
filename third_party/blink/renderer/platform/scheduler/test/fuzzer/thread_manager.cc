#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/scheduler/test/fuzzer/thread_manager.h"

#include <algorithm>

#include "base/task/sequence_manager/task_queue.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/scheduler/common/task_priority.h"
#include "third_party/blink/renderer/platform/scheduler/test/fuzzer/thread_pool_manager.h"

namespace base {
namespace sequence_manager {

namespace {

blink::scheduler::TaskPriority ToTaskQueuePriority(
    SequenceManagerTestDescription::QueuePriority priority) {
  using blink::scheduler::TaskPriority;

  static_assert(static_cast<int>(TaskPriority::kPriorityCount) == 11,
                "Number of task priorities has changed in "
                "blink::scheduler::TaskPriority.");

  switch (priority) {
    case SequenceManagerTestDescription::BEST_EFFORT:
      return TaskPriority::kBestEffortPriority;
    case SequenceManagerTestDescription::LOW:
      return TaskPriority::kLowPriority;
    case SequenceManagerTestDescription::LOW_CONTINUATION:
      return TaskPriority::kLowPriorityContinuation;
    case SequenceManagerTestDescription::UNDEFINED:
    case SequenceManagerTestDescription::NORMAL:
      return TaskPriority::kNormalPriority;
    case SequenceManagerTestDescription::NORMAL_CONTINUATION:
      return TaskPriority::kNormalPriorityContinuation;
    case SequenceManagerTestDescription::HIGH:
      return TaskPriority::kHighPriority;
    case SequenceManagerTestDescription::HIGH_CONTINUATION:
      return TaskPriority::kHighPriorityContinuation;
    case SequenceManagerTestDescription::VERY_HIGH:
      return TaskPriority::kVeryHighPriority;
    case SequenceManagerTestDescription::EXTREMELY_HIGH:
      return TaskPriority::kExtremelyHighPriority;
    case SequenceManagerTestDescription::HIGHEST:
      return TaskPriority::kHighestPriority;
    case SequenceManagerTestDescription::CONTROL:
      return TaskPriority::kControlPriority;
  }
}

}  // namespace

ThreadManager::ThreadManager(base::TimeTicks initial_time,
                             SequenceManagerFuzzerProcessor* processor)
    : processor_(processor) {
  DCHECK(processor_);

  test_task_runner_ = WrapRefCounted(
      new TestMockTimeTaskRunner(TestMockTimeTaskRunner::Type::kBoundToThread));

  DCHECK(!(initial_time - base::TimeTicks()).is_zero())
      << "A zero clock is not allowed as empty base::TimeTicks have a special "
         "value "
         "(i.e. base::TimeTicks::is_null())";

  test_task_runner_->AdvanceMockTickClock(initial_time - base::TimeTicks());

  manager_ = SequenceManagerForTest::Create(
      nullptr, SingleThreadTaskRunner::GetCurrentDefault(),
      test_task_runner_->GetMockTickClock(),
      SequenceManager::Settings::Builder()
          .SetPrioritySettings(::blink::scheduler::CreatePrioritySettings())
          .Build());

  TaskQueue::Spec spec = TaskQueue::Spec(QueueName::DEFAULT_TQ);
  task_queues_.emplace_back(
      MakeRefCounted<TaskQueueWithVoters>(manager_->CreateTaskQueue(spec)));
}

ThreadManager::~ThreadManager() = default;

base::TimeTicks ThreadManager::NowTicks() {
  return test_task_runner_->GetMockTickClock()->NowTicks();
}

base::TimeDelta ThreadManager::NextPendingTaskDelay() {
  return std::max(base::Milliseconds(0),
                  test_task_runner_->NextPendingTaskDelay());
}

void ThreadManager::AdvanceMockTickClock(base::TimeDelta delta) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return test_task_runner_->AdvanceMockTickClock(delta);
}

void ThreadManager::ExecuteThread(
    const google::protobuf::RepeatedPtrField<
        SequenceManagerTestDescription::Action>& initial_thread_actions) {
  for (const auto& initial_thread_action : initial_thread_actions) {
    RunAction(initial_thread_action);
  }

  while (NowTicks() < base::TimeTicks::Max()) {
    RunLoop().RunUntilIdle();
    processor_->thread_pool_manager()
        ->AdvanceClockSynchronouslyByPendingTaskDelay(this);
  }

  RunLoop().RunUntilIdle();
  processor_->thread_pool_manager()->ThreadDone();
}

void ThreadManager::RunAction(
    const SequenceManagerTestDescription::Action& action) {
  if (action.has_create_task_queue()) {
    ExecuteCreateTaskQueueAction(action.action_id(),
                                 action.create_task_queue());
  } else if (action.has_set_queue_priority()) {
    ExecuteSetQueuePriorityAction(action.action_id(),
                                  action.set_queue_priority());
  } else if (action.has_set_queue_enabled()) {
    ExecuteSetQueueEnabledAction(action.action_id(),
                                 action.set_queue_enabled());
  } else if (action.has_create_queue_voter()) {
    ExecuteCreateQueueVoterAction(action.action_id(),
                                  action.create_queue_voter());
  } else if (action.has_shutdown_task_queue()) {
    ExecuteShutdownTaskQueueAction(action.action_id(),
                                   action.shutdown_task_queue());
  } else if (action.has_cancel_task()) {
    ExecuteCancelTaskAction(action.action_id(), action.cancel_task());
  } else if (action.has_insert_fence()) {
    ExecuteInsertFenceAction(action.action_id(), action.insert_fence());
  } else if (action.has_remove_fence()) {
    ExecuteRemoveFenceAction(action.action_id(), action.remove_fence());
  } else if (action.has_create_thread()) {
    ExecuteCreateThreadAction(action.action_id(), action.create_thread());
  } else if (action.has_cross_thread_post()) {
    ExecuteCrossThreadPostDelayedTaskAction(action.action_id(),
                                            action.cross_thread_post());
  } else {
    ExecutePostDelayedTaskAction(action.action_id(),
                                 action.post_delayed_task());
  }
}

void ThreadManager::ExecuteCreateThreadAction(
    uint64_t action_id,
    const SequenceManagerTestDescription::CreateThreadAction& action) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  processor_->LogActionForTesting(&ordered_actions_, action_id,
                                  ActionForTest::ActionType::kCreateThread,
                                  NowTicks());

  processor_->thread_pool_manager()->CreateThread(
      action.initial_thread_actions(), NowTicks());
}

void ThreadManager::ExecuteCreateTaskQueueAction(
    uint64_t action_id,
    const SequenceManagerTestDescription::CreateTaskQueueAction& action) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  processor_->LogActionForTesting(&ordered_actions_, action_id,
                                  ActionForTest::ActionType::kCreateTaskQueue,
                                  NowTicks());

  TaskQueue::Spec spec = TaskQueue::Spec(QueueName::TEST_TQ);

  TaskQueue* chosen_task_queue;
  {
    AutoLock lock(lock_);
    task_queues_.emplace_back(
        MakeRefCounted<TaskQueueWithVoters>(manager_->CreateTaskQueue(spec)));
    chosen_task_queue = task_queues_.back()->queue.get();
  }
  chosen_task_queue->SetQueuePriority(
      ToTaskQueuePriority(action.initial_priority()));
}

void ThreadManager::ExecutePostDelayedTaskAction(
    uint64_t action_id,
    const SequenceManagerTestDescription::PostDelayedTaskAction& action) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  processor_->LogActionForTesting(&ordered_actions_, action_id,
                                  ActionForTest::ActionType::kPostDelayedTask,
                                  NowTicks());

  PostDelayedTask(action.task_queue_id(), action.delay_ms(), action.task());
}

void ThreadManager::ExecuteCrossThreadPostDelayedTaskAction(
    uint64_t action_id,
    const SequenceManagerTestDescription::CrossThreadPostDelayedTaskAction&
        action) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  processor_->LogActionForTesting(
      &ordered_actions_, action_id,
      ActionForTest::ActionType::kCrossThreadPostDelayedTask, NowTicks());

  processor_->thread_pool_manager()
      ->GetThreadManagerFor(action.thread_id())
      ->PostDelayedTask(action.task_queue_id(), action.delay_ms(),
                        action.task());
}

void ThreadManager::PostDelayedTask(
    uint64_t task_queue_id,
    uint32_t delay_ms,
    const SequenceManagerTestDescription::Task& task) {
  // PostDelayedTask can be called cross-thread, which can race with destroying
  // the task queue on the thread on which ThreadManager lives. Instead of
  // accessing the queue, get the task runner, which is synchronized with task
  // queue destruction.
  scoped_refptr<SingleThreadTaskRunner> chosen_task_runner =
      GetTaskRunnerFor(task_queue_id);

  std::unique_ptr<Task> pending_task = std::make_unique<Task>(this);

  // TODO(farahcharab) After adding non-nestable/nestable tasks, fix this to
  // PostNonNestableDelayedTask for the former and PostDelayedTask for the
  // latter.
  chosen_task_runner->PostDelayedTask(
      FROM_HERE,
      BindOnce(&Task::Execute, pending_task->weak_ptr_factory_.GetWeakPtr(),
               task),
      base::Milliseconds(delay_ms));

  {
    AutoLock lock(lock_);
    pending_tasks_.push_back(std::move(pending_task));
  }
}

void ThreadManager::ExecuteSetQueuePriorityAction(
    uint64_t action_id,
    const SequenceManagerTestDescription::SetQueuePriorityAction& action) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  processor_->LogActionForTesting(&ordered_actions_, action_id,
                                  ActionForTest::ActionType::kSetQueuePriority,
                                  NowTicks());

  scoped_refptr<TaskQueueWithVoters> chosen_task_queue =
      GetTaskQueueFor(action.task_queue_id());
  chosen_task_queue->queue->SetQueuePriority(
      ToTaskQueuePriority(action.priority()));
}

void ThreadManager::ExecuteSetQueueEnabledAction(
    uint64_t action_id,
    const SequenceManagerTestDescription::SetQueueEnabledAction& action) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  processor_->LogActionForTesting(&ordered_actions_, action_id,
                                  ActionForTest::ActionType::kSetQueueEnabled,
                                  NowTicks());

  scoped_refptr<TaskQueueWithVoters> chosen_task_queue =
      GetTaskQueueFor(action.task_queue_id());

  if (chosen_task_queue->voters.empty()) {
    chosen_task_queue->voters.push_back(
        chosen_task_queue->queue.get()->CreateQueueEnabledVoter());
  }

  wtf_size_t voter_index = action.voter_id() % chosen_task_queue->voters.size();
  chosen_task_queue->voters[voter_index]->SetVoteToEnable(action.enabled());
}

void ThreadManager::ExecuteCreateQueueVoterAction(
    uint64_t action_id,
    const SequenceManagerTestDescription::CreateQueueVoterAction& action) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  processor_->LogActionForTesting(&ordered_actions_, action_id,
                                  ActionForTest::ActionType::kCreateQueueVoter,
                                  NowTicks());

  scoped_refptr<TaskQueueWithVoters> chosen_task_queue =
      GetTaskQueueFor(action.task_queue_id());
  chosen_task_queue->voters.push_back(
      chosen_task_queue->queue.get()->CreateQueueEnabledVoter());
}

void ThreadManager::ExecuteShutdownTaskQueueAction(
    uint64_t action_id,
    const SequenceManagerTestDescription::ShutdownTaskQueueAction& action) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  processor_->LogActionForTesting(&ordered_actions_, action_id,
                                  ActionForTest::ActionType::kShutdownTaskQueue,
                                  NowTicks());

  // The shutdown needs to happen with the lock held to prevent cross-thread
  // task posting from grabbing a dangling pointer.
  AutoLock lock(lock_);
  // We always want to have a default task queue.
  if (task_queues_.size() > 1) {
    wtf_size_t queue_index = action.task_queue_id() % task_queues_.size();
    task_queues_[queue_index].reset();
    task_queues_.erase(task_queues_.begin() + queue_index);
  }
}

void ThreadManager::ExecuteCancelTaskAction(
    uint64_t action_id,
    const SequenceManagerTestDescription::CancelTaskAction& action) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  processor_->LogActionForTesting(&ordered_actions_, action_id,
                                  ActionForTest::ActionType::kCancelTask,
                                  NowTicks());

  AutoLock lock(lock_);
  if (!pending_tasks_.empty()) {
    wtf_size_t task_index = action.task_id() % pending_tasks_.size();
    pending_tasks_[task_index]->weak_ptr_factory_.InvalidateWeakPtrs();

    // If it is already running, it is a parent task and will be deleted when
    // it is done.
    if (!pending_tasks_[task_index]->is_running_) {
      pending_tasks_.erase(pending_tasks_.begin() + task_index);
    }
  }
}

void ThreadManager::ExecuteInsertFenceAction(
    uint64_t action_id,
    const SequenceManagerTestDescription::InsertFenceAction& action) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  processor_->LogActionForTesting(&ordered_actions_, action_id,
                                  ActionForTest::ActionType::kInsertFence,
                                  NowTicks());

  scoped_refptr<TaskQueueWithVoters> chosen_task_queue =
      GetTaskQueueFor(action.task_queue_id());

  if (action.position() ==
      SequenceManagerTestDescription::InsertFenceAction::NOW) {
    chosen_task_queue->queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
  } else {
    chosen_task_queue->queue->InsertFence(
        TaskQueue::InsertFencePosition::kBeginningOfTime);
  }
}

void ThreadManager::ExecuteRemoveFenceAction(
    uint64_t action_id,
    const SequenceManagerTestDescription::RemoveFenceAction& action) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  processor_->LogActionForTesting(&ordered_actions_, action_id,
                                  ActionForTest::ActionType::kRemoveFence,
                                  NowTicks());

  scoped_refptr<TaskQueueWithVoters> chosen_task_queue =
      GetTaskQueueFor(action.task_queue_id());
  chosen_task_queue->queue->RemoveFence();
}

void ThreadManager::ExecuteTask(
    const SequenceManagerTestDescription::Task& task) {
  base::TimeTicks start_time = NowTicks();

  // We can limit the depth of the nested post delayed action when processing
  // the proto.
  for (const auto& task_action : task.actions()) {
    // TODO(farahcharab) Add run loop to deal with nested tasks later. So far,
    // we are assuming tasks are non-nestable.
    RunAction(task_action);
  }

  base::TimeTicks end_time = NowTicks();

  base::TimeTicks next_time =
      start_time +
      std::max(base::TimeDelta(), base::Milliseconds(task.duration_ms()) -
                                      (end_time - start_time));

  while (NowTicks() != next_time) {
    processor_->thread_pool_manager()->AdvanceClockSynchronouslyToTime(
        this, next_time);
  }

  processor_->LogTaskForTesting(&ordered_tasks_, task.task_id(), start_time,
                                NowTicks());
}

void ThreadManager::DeleteTask(Task* task) {
  AutoLock lock(lock_);
  wtf_size_t i = 0;
  while (i < pending_tasks_.size() && task != pending_tasks_[i].get()) {
    i++;
  }
  if (i < pending_tasks_.size())
    pending_tasks_.erase(pending_tasks_.begin() + i);
}

scoped_refptr<TaskQueueWithVoters> ThreadManager::GetTaskQueueFor(
    uint64_t task_queue_id) {
  AutoLock lock(lock_);
  DCHECK(!task_queues_.empty());
  return task_queues_[task_queue_id % task_queues_.size()].get();
}

scoped_refptr<SingleThreadTaskRunner> ThreadManager::GetTaskRunnerFor(
    uint64_t task_queue_id) {
  AutoLock lock(lock_);
  DCHECK(!task_queues_.empty());
  return task_queues_[task_queue_id % task_queues_.size()]
      ->queue->task_runner();
}

const Vector<SequenceManagerFuzzerProcessor::TaskForTest>&
ThreadManager::ordered_tasks() const {
  return ordered_tasks_;
}

const Vector<SequenceManagerFuzzerProcessor::ActionForTest>&
ThreadManager::ordered_actions() const {
  return ordered_actions_;
}

ThreadManager::Task::Task(ThreadManager* thread_manager)
    : is_running_(false), thread_manager_(thread_manager) {
  DCHECK(thread_manager_);
}

void ThreadManager::Task::Execute(
    const SequenceManagerTestDescription::Task& task) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_manager_->thread_checker_);
  is_running_ = true;
  thread_manager_->ExecuteTask(task);
  thread_manager_->DeleteTask(this);
}

}  // namespace sequence_manager
}  // namespace base
