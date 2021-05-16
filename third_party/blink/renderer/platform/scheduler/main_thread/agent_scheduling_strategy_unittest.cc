#include "third_party/blink/renderer/platform/scheduler/main_thread/agent_scheduling_strategy.h"

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_frame_scheduler.h"

namespace blink {
namespace scheduler {

using FeatureAndParams = ::base::test::ScopedFeatureList::FeatureAndParams;
using ShouldUpdatePolicy =
    ::blink::scheduler::AgentSchedulingStrategy::ShouldUpdatePolicy;
using PrioritisationType =
    ::blink::scheduler::MainThreadTaskQueue::QueueTraits::PrioritisationType;

using ::base::FieldTrialParams;
using ::base::sequence_manager::TaskQueue;
using ::base::test::ScopedFeatureList;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::Test;

namespace {

class MockDelegate : public AgentSchedulingStrategy::Delegate {
 public:
  MOCK_METHOD(void,
              OnSetTimer,
              (const FrameSchedulerImpl& frame_scheduler,
               base::TimeDelta delay));
};

class MockFrameSchedulerDelegate : public FrameScheduler::Delegate {
 public:
  MockFrameSchedulerDelegate() {
    ON_CALL(*this, GetAgentClusterId)
        .WillByDefault(ReturnRef(agent_cluster_id_));
  }

  MOCK_METHOD(const base::UnguessableToken&,
              GetAgentClusterId,
              (),
              (const, override));
  MOCK_METHOD(ukm::UkmRecorder*, GetUkmRecorder, ());
  MOCK_METHOD(ukm::SourceId, GetUkmSourceId, ());
  MOCK_METHOD(void, UpdateTaskTime, (base::TimeDelta));
  MOCK_METHOD(void, UpdateActiveSchedulerTrackedFeatures, (uint64_t));

 private:
  base::UnguessableToken agent_cluster_id_ = base::UnguessableToken::Create();
};

class MockFrameScheduler : public FrameSchedulerImpl {
 public:
  explicit MockFrameScheduler(FrameScheduler::FrameType frame_type)
      : FrameSchedulerImpl(/*main_thread_scheduler=*/nullptr,
                           /*parent_page_scheduler=*/nullptr,
                           /*delegate=*/&delegate_,
                           /*blame_context=*/nullptr,
                           /*frame_type=*/frame_type) {
    ON_CALL(*this, IsOrdinary).WillByDefault(Return(true));
  }

  MOCK_METHOD(bool, IsOrdinary, (), (const));

 private:
  NiceMock<MockFrameSchedulerDelegate> delegate_;
};

}  // namespace

class PerAgentSchedulingBaseTest : public Test {
 public:
  explicit PerAgentSchedulingBaseTest(
      const FieldTrialParams experiment_params) {
    feature_list_.InitWithFeaturesAndParameters(
        {{kPerAgentSchedulingExperiments, experiment_params}}, {});
    strategy_ = AgentSchedulingStrategy::Create(delegate_);
    timer_queue_->SetFrameSchedulerForTest(&subframe_);
    non_timer_queue_->SetFrameSchedulerForTest(&subframe_);
  }

 protected:
  ScopedFeatureList feature_list_;
  NiceMock<MockDelegate> delegate_{};
  std::unique_ptr<AgentSchedulingStrategy> strategy_;
  NiceMock<MockFrameScheduler> main_frame_{
      FrameScheduler::FrameType::kMainFrame};
  NiceMock<MockFrameScheduler> subframe_{FrameScheduler::FrameType::kSubframe};
  scoped_refptr<MainThreadTaskQueueForTest> timer_queue_{
      new MainThreadTaskQueueForTest(PrioritisationType::kJavaScriptTimer)};
  scoped_refptr<MainThreadTaskQueueForTest> non_timer_queue_{
      new MainThreadTaskQueueForTest(PrioritisationType::kRegular)};
};

class PerAgentDisableTimersUntilTimeoutStrategyTest
    : public PerAgentSchedulingBaseTest {
 public:
  PerAgentDisableTimersUntilTimeoutStrategyTest()
      : PerAgentSchedulingBaseTest({{"queues", "timer-queues"},
                                    {"method", "disable"},
                                    {"signal", "delay"},
                                    {"delay_ms", "50"}}) {}
};

TEST_F(PerAgentDisableTimersUntilTimeoutStrategyTest, RequestsPolicyUpdate) {
  EXPECT_EQ(strategy_->OnFrameAdded(main_frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnMainFrameFirstMeaningfulPaint(main_frame_),
            ShouldUpdatePolicy::kNo);
  EXPECT_EQ(strategy_->OnMainFrameLoad(main_frame_), ShouldUpdatePolicy::kNo);
  EXPECT_EQ(strategy_->OnDelayPassed(main_frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnFrameRemoved(main_frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnDocumentChangedInMainFrame(main_frame_),
            ShouldUpdatePolicy::kYes);
}

TEST_F(PerAgentDisableTimersUntilTimeoutStrategyTest, InitiatesTimer) {
  EXPECT_CALL(delegate_, OnSetTimer(_, base::TimeDelta::FromMilliseconds(50)))
      .Times(1);

  ignore_result(strategy_->OnFrameAdded(main_frame_));
}

TEST_F(PerAgentDisableTimersUntilTimeoutStrategyTest,
       DisablesTimerQueueUntilTimeout) {
  ignore_result(strategy_->OnFrameAdded(main_frame_));
  ignore_result(strategy_->OnMainFrameFirstMeaningfulPaint(main_frame_));

  EXPECT_THAT(strategy_->QueueEnabledState(*timer_queue_),
              testing::Optional(false));
  EXPECT_FALSE(strategy_->QueuePriority(*timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueueEnabledState(*non_timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*non_timer_queue_).has_value());

  ignore_result(strategy_->OnDelayPassed(main_frame_));

  EXPECT_FALSE(strategy_->QueueEnabledState(*timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*timer_queue_).has_value());
}

class PerAgentDisableTimersUntilFMPStrategyTest
    : public PerAgentSchedulingBaseTest {
 public:
  PerAgentDisableTimersUntilFMPStrategyTest()
      : PerAgentSchedulingBaseTest({{"queues", "timer-queues"},
                                    {"method", "disable"},
                                    {"signal", "fmp"}}) {}
};

TEST_F(PerAgentDisableTimersUntilFMPStrategyTest, RequestsPolicyUpdate) {
  EXPECT_EQ(strategy_->OnFrameAdded(main_frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnMainFrameFirstMeaningfulPaint(main_frame_),
            ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnMainFrameLoad(main_frame_), ShouldUpdatePolicy::kNo);
  EXPECT_EQ(strategy_->OnFrameRemoved(main_frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnDocumentChangedInMainFrame(main_frame_),
            ShouldUpdatePolicy::kYes);
  // Only the first input event (since a main frame document was added) should
  // cause a policy update. This is necessary as we may get several input event
  // notifications, but we don't want them to re-calculate priorities as nothing
  // will change.
  EXPECT_EQ(strategy_->OnInputEvent(), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnInputEvent(), ShouldUpdatePolicy::kNo);
}

TEST_F(PerAgentDisableTimersUntilFMPStrategyTest, DisablesTimerQueueUntilFMP) {
  ignore_result(strategy_->OnFrameAdded(main_frame_));

  EXPECT_THAT(strategy_->QueueEnabledState(*timer_queue_),
              testing::Optional(false));
  EXPECT_FALSE(strategy_->QueuePriority(*timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueueEnabledState(*non_timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*non_timer_queue_).has_value());

  ignore_result(strategy_->OnMainFrameFirstMeaningfulPaint(main_frame_));

  EXPECT_FALSE(strategy_->QueueEnabledState(*timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*timer_queue_).has_value());
}

class PerAgentBestEffortPriorityTimersUntilFMPStrategyTest
    : public PerAgentSchedulingBaseTest {
 public:
  PerAgentBestEffortPriorityTimersUntilFMPStrategyTest()
      : PerAgentSchedulingBaseTest({{"queues", "timer-queues"},
                                    {"method", "best-effort"},
                                    {"signal", "fmp"}}) {}
};

TEST_F(PerAgentBestEffortPriorityTimersUntilFMPStrategyTest,
       RequestsPolicyUpdate) {
  EXPECT_EQ(strategy_->OnFrameAdded(main_frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnMainFrameFirstMeaningfulPaint(main_frame_),
            ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnMainFrameLoad(main_frame_), ShouldUpdatePolicy::kNo);
  EXPECT_EQ(strategy_->OnFrameRemoved(main_frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnDocumentChangedInMainFrame(main_frame_),
            ShouldUpdatePolicy::kYes);
  // Only the first input event (since a main frame document was added) should
  // cause a policy update. This is necessary as we may get several input event
  // notifications, but we don't want them to re-calculate priorities as nothing
  // will change.
  EXPECT_EQ(strategy_->OnInputEvent(), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnInputEvent(), ShouldUpdatePolicy::kNo);
}

TEST_F(PerAgentBestEffortPriorityTimersUntilFMPStrategyTest,
       LowersTimerQueuePriorityUntilFMP) {
  ignore_result(strategy_->OnFrameAdded(main_frame_));

  EXPECT_FALSE(strategy_->QueueEnabledState(*timer_queue_).has_value());
  EXPECT_THAT(strategy_->QueuePriority(*timer_queue_),
              testing::Optional(TaskQueue::QueuePriority::kBestEffortPriority));
  EXPECT_FALSE(strategy_->QueueEnabledState(*non_timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*non_timer_queue_).has_value());

  ignore_result(strategy_->OnMainFrameFirstMeaningfulPaint(main_frame_));

  EXPECT_FALSE(strategy_->QueueEnabledState(*timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*timer_queue_).has_value());
}

class PerAgentDisableTimersUntilLoadStrategyTest
    : public PerAgentSchedulingBaseTest {
 public:
  PerAgentDisableTimersUntilLoadStrategyTest()
      : PerAgentSchedulingBaseTest({{"queues", "timer-queues"},
                                    {"method", "disable"},
                                    {"signal", "onload"}}) {}
};

TEST_F(PerAgentDisableTimersUntilLoadStrategyTest, RequestsPolicyUpdate) {
  EXPECT_EQ(strategy_->OnFrameAdded(main_frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnMainFrameFirstMeaningfulPaint(main_frame_),
            ShouldUpdatePolicy::kNo);
  EXPECT_EQ(strategy_->OnMainFrameLoad(main_frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnFrameRemoved(main_frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnDocumentChangedInMainFrame(main_frame_),
            ShouldUpdatePolicy::kYes);
}

TEST_F(PerAgentDisableTimersUntilLoadStrategyTest, DisablesTimerQueue) {
  ignore_result(strategy_->OnFrameAdded(main_frame_));

  EXPECT_THAT(strategy_->QueueEnabledState(*timer_queue_),
              testing::Optional(false));
  EXPECT_FALSE(strategy_->QueuePriority(*timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueueEnabledState(*non_timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*non_timer_queue_).has_value());

  ignore_result(strategy_->OnMainFrameLoad(main_frame_));

  EXPECT_FALSE(strategy_->QueueEnabledState(*timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*timer_queue_).has_value());
}

class PerAgentBestEffortPriorityTimersUntilLoadStrategyTest
    : public PerAgentSchedulingBaseTest {
 public:
  PerAgentBestEffortPriorityTimersUntilLoadStrategyTest()
      : PerAgentSchedulingBaseTest({{"queues", "timer-queues"},
                                    {"method", "best-effort"},
                                    {"signal", "onload"}}) {}
};

TEST_F(PerAgentBestEffortPriorityTimersUntilLoadStrategyTest,
       RequestsPolicyUpdate) {
  EXPECT_EQ(strategy_->OnFrameAdded(main_frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnMainFrameFirstMeaningfulPaint(main_frame_),
            ShouldUpdatePolicy::kNo);
  EXPECT_EQ(strategy_->OnMainFrameLoad(main_frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnFrameRemoved(main_frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnDocumentChangedInMainFrame(main_frame_),
            ShouldUpdatePolicy::kYes);
}

TEST_F(PerAgentBestEffortPriorityTimersUntilLoadStrategyTest,
       LowersTimerQueuePriority) {
  ignore_result(strategy_->OnFrameAdded(main_frame_));

  EXPECT_FALSE(strategy_->QueueEnabledState(*timer_queue_).has_value());
  EXPECT_THAT(strategy_->QueuePriority(*timer_queue_),
              testing::Optional(TaskQueue::QueuePriority::kBestEffortPriority));
  EXPECT_FALSE(strategy_->QueueEnabledState(*non_timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*non_timer_queue_).has_value());

  ignore_result(strategy_->OnMainFrameLoad(main_frame_));

  EXPECT_FALSE(strategy_->QueueEnabledState(*timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*timer_queue_).has_value());
}

class PerAgentDisableAllUntilFMPStrategyTest
    : public PerAgentSchedulingBaseTest {
 public:
  PerAgentDisableAllUntilFMPStrategyTest()
      : PerAgentSchedulingBaseTest({{"queues", "all-queues"},
                                    {"method", "disable"},
                                    {"signal", "fmp"}}) {}
};

TEST_F(PerAgentDisableAllUntilFMPStrategyTest, RequestsPolicyUpdate) {
  EXPECT_EQ(strategy_->OnFrameAdded(main_frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnMainFrameFirstMeaningfulPaint(main_frame_),
            ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnMainFrameLoad(main_frame_), ShouldUpdatePolicy::kNo);
  EXPECT_EQ(strategy_->OnFrameRemoved(main_frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnDocumentChangedInMainFrame(main_frame_),
            ShouldUpdatePolicy::kYes);
  // Only the first input event (since a main frame document was added) should
  // cause a policy update. This is necessary as we may get several input event
  // notifications, but we don't want them to re-calculate priorities as nothing
  // will change.
  EXPECT_EQ(strategy_->OnInputEvent(), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnInputEvent(), ShouldUpdatePolicy::kNo);
}

TEST_F(PerAgentDisableAllUntilFMPStrategyTest, DisablesTimerQueueUntilFMP) {
  ignore_result(strategy_->OnFrameAdded(main_frame_));

  EXPECT_THAT(strategy_->QueueEnabledState(*timer_queue_),
              testing::Optional(false));
  EXPECT_FALSE(strategy_->QueuePriority(*timer_queue_).has_value());
  EXPECT_THAT(strategy_->QueueEnabledState(*non_timer_queue_),
              testing::Optional(false));
  EXPECT_FALSE(strategy_->QueuePriority(*non_timer_queue_).has_value());

  ignore_result(strategy_->OnMainFrameFirstMeaningfulPaint(main_frame_));

  EXPECT_FALSE(strategy_->QueueEnabledState(*timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueueEnabledState(*non_timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*non_timer_queue_).has_value());
}

class PerAgentBestEffortPriorityAllUntilFMPStrategyTest
    : public PerAgentSchedulingBaseTest {
 public:
  PerAgentBestEffortPriorityAllUntilFMPStrategyTest()
      : PerAgentSchedulingBaseTest({{"queues", "all-queues"},
                                    {"method", "best-effort"},
                                    {"signal", "fmp"}}) {}
};

TEST_F(PerAgentBestEffortPriorityAllUntilFMPStrategyTest,
       RequestsPolicyUpdate) {
  EXPECT_EQ(strategy_->OnFrameAdded(main_frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnMainFrameFirstMeaningfulPaint(main_frame_),
            ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnMainFrameLoad(main_frame_), ShouldUpdatePolicy::kNo);
  EXPECT_EQ(strategy_->OnFrameRemoved(main_frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnDocumentChangedInMainFrame(main_frame_),
            ShouldUpdatePolicy::kYes);
  // Only the first input event (since a main frame document was added) should
  // cause a policy update. This is necessary as we may get several input event
  // notifications, but we don't want them to re-calculate priorities as nothing
  // will change.
  EXPECT_EQ(strategy_->OnInputEvent(), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnInputEvent(), ShouldUpdatePolicy::kNo);
}

TEST_F(PerAgentBestEffortPriorityAllUntilFMPStrategyTest,
       LowersTimerQueuePriorityUntilFMP) {
  ignore_result(strategy_->OnFrameAdded(main_frame_));

  EXPECT_FALSE(strategy_->QueueEnabledState(*timer_queue_).has_value());
  EXPECT_THAT(strategy_->QueuePriority(*timer_queue_),
              testing::Optional(TaskQueue::QueuePriority::kBestEffortPriority));
  EXPECT_FALSE(strategy_->QueueEnabledState(*non_timer_queue_).has_value());
  EXPECT_THAT(strategy_->QueuePriority(*non_timer_queue_),
              testing::Optional(TaskQueue::QueuePriority::kBestEffortPriority));

  ignore_result(strategy_->OnMainFrameFirstMeaningfulPaint(main_frame_));

  EXPECT_FALSE(strategy_->QueueEnabledState(*timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueueEnabledState(*non_timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*non_timer_queue_).has_value());
}

class PerAgentDisableAllUntilLoadStrategyTest
    : public PerAgentSchedulingBaseTest {
 public:
  PerAgentDisableAllUntilLoadStrategyTest()
      : PerAgentSchedulingBaseTest({{"queues", "all-queues"},
                                    {"method", "disable"},
                                    {"signal", "onload"}}) {}
};

TEST_F(PerAgentDisableAllUntilLoadStrategyTest, RequestsPolicyUpdate) {
  EXPECT_EQ(strategy_->OnFrameAdded(main_frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnMainFrameFirstMeaningfulPaint(main_frame_),
            ShouldUpdatePolicy::kNo);
  EXPECT_EQ(strategy_->OnMainFrameLoad(main_frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnFrameRemoved(main_frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnDocumentChangedInMainFrame(main_frame_),
            ShouldUpdatePolicy::kYes);
}

TEST_F(PerAgentDisableAllUntilLoadStrategyTest, DisablesTimerQueue) {
  ignore_result(strategy_->OnFrameAdded(main_frame_));

  EXPECT_THAT(strategy_->QueueEnabledState(*timer_queue_),
              testing::Optional(false));
  EXPECT_FALSE(strategy_->QueuePriority(*timer_queue_).has_value());
  EXPECT_THAT(strategy_->QueueEnabledState(*non_timer_queue_),
              testing::Optional(false));
  EXPECT_FALSE(strategy_->QueuePriority(*non_timer_queue_).has_value());

  ignore_result(strategy_->OnMainFrameLoad(main_frame_));

  EXPECT_FALSE(strategy_->QueueEnabledState(*timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueueEnabledState(*non_timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*non_timer_queue_).has_value());
}

class PerAgentBestEffortPriorityAllUntilLoadStrategyTest
    : public PerAgentSchedulingBaseTest {
 public:
  PerAgentBestEffortPriorityAllUntilLoadStrategyTest()
      : PerAgentSchedulingBaseTest({{"queues", "all-queues"},
                                    {"method", "best-effort"},
                                    {"signal", "onload"}}) {}
};

TEST_F(PerAgentBestEffortPriorityAllUntilLoadStrategyTest,
       RequestsPolicyUpdate) {
  EXPECT_EQ(strategy_->OnFrameAdded(main_frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnMainFrameFirstMeaningfulPaint(main_frame_),
            ShouldUpdatePolicy::kNo);
  EXPECT_EQ(strategy_->OnMainFrameLoad(main_frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnFrameRemoved(main_frame_), ShouldUpdatePolicy::kYes);
  EXPECT_EQ(strategy_->OnDocumentChangedInMainFrame(main_frame_),
            ShouldUpdatePolicy::kYes);
}

TEST_F(PerAgentBestEffortPriorityAllUntilLoadStrategyTest,
       LowersTimerQueuePriority) {
  ignore_result(strategy_->OnFrameAdded(main_frame_));

  EXPECT_FALSE(strategy_->QueueEnabledState(*timer_queue_).has_value());
  EXPECT_THAT(strategy_->QueuePriority(*timer_queue_),
              testing::Optional(TaskQueue::QueuePriority::kBestEffortPriority));
  EXPECT_FALSE(strategy_->QueueEnabledState(*non_timer_queue_).has_value());
  EXPECT_THAT(strategy_->QueuePriority(*non_timer_queue_),
              testing::Optional(TaskQueue::QueuePriority::kBestEffortPriority));

  ignore_result(strategy_->OnMainFrameLoad(main_frame_));

  EXPECT_FALSE(strategy_->QueueEnabledState(*timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueueEnabledState(*non_timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*non_timer_queue_).has_value());
}

class PerAgentDefaultIsNoOpStrategyTest : public Test {
 public:
  PerAgentDefaultIsNoOpStrategyTest() {
    timer_queue_->SetFrameSchedulerForTest(&subframe_);
  }

 protected:
  NiceMock<MockDelegate> delegate_{};
  std::unique_ptr<AgentSchedulingStrategy> strategy_ =
      AgentSchedulingStrategy::Create(delegate_);
  MockFrameScheduler main_frame_{FrameScheduler::FrameType::kMainFrame};
  NiceMock<MockFrameScheduler> subframe_{FrameScheduler::FrameType::kSubframe};
  scoped_refptr<MainThreadTaskQueueForTest> timer_queue_{
      new MainThreadTaskQueueForTest(PrioritisationType::kJavaScriptTimer)};
};

TEST_F(PerAgentDefaultIsNoOpStrategyTest, DoesntRequestPolicyUpdate) {
  EXPECT_EQ(strategy_->OnFrameAdded(main_frame_), ShouldUpdatePolicy::kNo);
  EXPECT_EQ(strategy_->OnMainFrameFirstMeaningfulPaint(main_frame_),
            ShouldUpdatePolicy::kNo);
  EXPECT_EQ(strategy_->OnFrameRemoved(main_frame_), ShouldUpdatePolicy::kNo);
  EXPECT_EQ(strategy_->OnDocumentChangedInMainFrame(main_frame_),
            ShouldUpdatePolicy::kNo);
  EXPECT_EQ(strategy_->OnInputEvent(), ShouldUpdatePolicy::kNo);
}

TEST_F(PerAgentDefaultIsNoOpStrategyTest, DoesntModifyPolicyDecisions) {
  EXPECT_FALSE(strategy_->QueueEnabledState(*timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*timer_queue_).has_value());
}

class PerAgentNonOrdinaryPageTest : public PerAgentSchedulingBaseTest {
 public:
  PerAgentNonOrdinaryPageTest()
      : PerAgentSchedulingBaseTest({{"queues", "timer-queues"},
                                    {"method", "disable"},
                                    {"signal", "onload"}}) {
    ON_CALL(non_ordinary_frame_scheduler_, IsOrdinary)
        .WillByDefault(Return(false));
  }

 protected:
  NiceMock<MockFrameScheduler> non_ordinary_frame_scheduler_{
      FrameScheduler::FrameType::kMainFrame};
};

TEST_F(PerAgentNonOrdinaryPageTest, DoesntWaitForNonOrdinaryFrames) {
  EXPECT_EQ(strategy_->OnFrameAdded(non_ordinary_frame_scheduler_),
            ShouldUpdatePolicy::kYes);
  EXPECT_FALSE(strategy_->QueueEnabledState(*timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueueEnabledState(*non_timer_queue_).has_value());
  EXPECT_FALSE(strategy_->QueuePriority(*non_timer_queue_).has_value());
}

}  // namespace scheduler
}  // namespace blink
