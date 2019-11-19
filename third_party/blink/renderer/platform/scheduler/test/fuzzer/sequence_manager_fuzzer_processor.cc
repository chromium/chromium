#include "third_party/blink/renderer/platform/scheduler/test/fuzzer/sequence_manager_fuzzer_processor.h"

#include "third_party/blink/renderer/platform/scheduler/test/fuzzer/simple_thread_impl.h"
#include "third_party/blink/renderer/platform/scheduler/test/fuzzer/thread_manager.h"
#include "third_party/blink/renderer/platform/scheduler/test/fuzzer/thread_pool_manager.h"

namespace base {
namespace sequence_manager {

void SequenceManagerFuzzerProcessor::ParseAndRun(
    const SequenceManagerTestDescription& description) {
  SequenceManagerFuzzerProcessor processor;
  processor.RunTest(description);
}

SequenceManagerFuzzerProcessor::SequenceManagerFuzzerProcessor()
    : SequenceManagerFuzzerProcessor(false) {}

SequenceManagerFuzzerProcessor::SequenceManagerFuzzerProcessor(
    bool log_for_testing)
    : log_for_testing_(log_for_testing),
      initial_time_(base::TimeTicks() + base::TimeDelta::FromMilliseconds(1)),
      thread_pool_manager_(std::make_unique<ThreadPoolManager>(this)),
      main_thread_manager_(
          std::make_unique<ThreadManager>(initial_time_, this)) {}

SequenceManagerFuzzerProcessor::~SequenceManagerFuzzerProcessor() = default;

void SequenceManagerFuzzerProcessor::RunTest(
    const SequenceManagerTestDescription& description) {
  for (const auto& initial_action : description.main_thread_actions()) {
    main_thread_manager_->ExecuteCreateThreadAction(
        initial_action.action_id(), initial_action.create_thread());
  }

  thread_pool_manager_->StartInitialThreads();

  thread_pool_manager_->WaitForAllThreads();

  if (log_for_testing_) {
    ordered_actions_.emplace_back(main_thread_manager_->ordered_actions());
    ordered_tasks_.emplace_back(main_thread_manager_->ordered_tasks());

    for (ThreadManager* thread_manager :
         thread_pool_manager_->GetAllThreadManagers()) {
      ordered_actions_.emplace_back(thread_manager->ordered_actions());
      ordered_tasks_.emplace_back(thread_manager->ordered_tasks());
    }
  }
}

void SequenceManagerFuzzerProcessor::LogTaskForTesting(
    Vector<TaskForTest>* ordered_tasks,
    uint64_t task_id,
    base::TimeTicks start_time,
    base::TimeTicks end_time) {
  if (!log_for_testing_)
    return;

  uint64_t start_time_ms = (start_time - initial_time_).InMilliseconds();
  uint64_t end_time_ms = (end_time - initial_time_).InMilliseconds();

  ordered_tasks->emplace_back(task_id, start_time_ms, end_time_ms);
}

void SequenceManagerFuzzerProcessor::LogActionForTesting(
    Vector<ActionForTest>* ordered_actions,
    uint64_t action_id,
    ActionForTest::ActionType type,
    base::TimeTicks start_time) {
  if (!log_for_testing_)
    return;

  ordered_actions->emplace_back(action_id, type,
                                (start_time - initial_time_).InMilliseconds());
}

const Vector<Vector<SequenceManagerFuzzerProcessor::TaskForTest>>&
SequenceManagerFuzzerProcessor::ordered_tasks() const {
  return ordered_tasks_;
}

const Vector<Vector<SequenceManagerFuzzerProcessor::ActionForTest>>&
SequenceManagerFuzzerProcessor::ordered_actions() const {
  return ordered_actions_;
}

SequenceManagerFuzzerProcessor::TaskForTest::TaskForTest(uint64_t id,
                                                         uint64_t start_time_ms,
                                                         uint64_t end_time_ms)
    : task_id(id), start_time_ms(start_time_ms), end_time_ms(end_time_ms) {}

bool SequenceManagerFuzzerProcessor::TaskForTest::operator==(
    const TaskForTest& rhs) const {
  return task_id == rhs.task_id && start_time_ms == rhs.start_time_ms &&
         end_time_ms == rhs.end_time_ms;
}

SequenceManagerFuzzerProcessor::ActionForTest::ActionForTest(
    uint64_t id,
    ActionType type,
    uint64_t start_time_ms)
    : action_id(id), action_type(type), start_time_ms(start_time_ms) {}

bool SequenceManagerFuzzerProcessor::ActionForTest::operator==(
    const ActionForTest& rhs) const {
  return action_id == rhs.action_id && action_type == rhs.action_type &&
         start_time_ms == rhs.start_time_ms;
}

ThreadPoolManager* SequenceManagerFuzzerProcessor::thread_pool_manager() const {
  return thread_pool_manager_.get();
}

}  // namespace sequence_manager
}  // namespace base
