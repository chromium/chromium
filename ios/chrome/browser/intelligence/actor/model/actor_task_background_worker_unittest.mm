// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_task_background_worker.h"

#import <map>
#import <memory>
#import <string>
#import <string_view>
#import <vector>

#import "base/memory/weak_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/actor/core/aggregated_journal.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_configuration.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_context.h"
#import "ios/chrome/app/background_task/features.h"
#import "ios/chrome/browser/intelligence/actor/metrics/actor_background_task_metrics.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util.h"

namespace actor {

namespace {

// Name of the journal event logged when a heartbeat ping fails.
constexpr char kHeartbeatPingFailedEvent[] = "ActorTask::HeartbeatPingFailed";

// Names of the journal events logged when a background task starts and ends.
constexpr char kBackgroundTaskStartedEvent[] =
    "ActorTaskBackgroundWorker::SetContext";
constexpr char kBackgroundTaskFinalizedEvent[] =
    "ActorTaskBackgroundWorker::FinalizeBackgroundTask";

// Fake delegate exposing settable task state to the worker.
class FakeTaskStateDelegate
    : public ActorTaskBackgroundWorker::TaskStateDelegate {
 public:
  ActorTaskState GetTaskState() const override { return state; }
  const std::string& GetTaskTitle() const override { return title; }
  const std::string& GetLastTaskUpdate() const override {
    return last_task_update;
  }

  ActorTaskState state = ActorTaskState::kInit;
  std::string title = "Test Task";
  std::string last_task_update;
};

// Returns a background task context with a no-op expiration handler.
BackgroundContinuedProcessingTaskContext* CreateContext() {
  BackgroundContinuedProcessingTaskConfiguration* config =
      [[BackgroundContinuedProcessingTaskConfiguration alloc]
              initWithTitle:@"Test Task"
                   subtitle:@""
          expirationHandler:^{
          }];
  return [[BackgroundContinuedProcessingTaskContext alloc]
      initWithTaskIdentifier:@"org.chromium.test.task"
               configuration:config
               finishHandler:nil];
}

// Returns the details of the events named `event_name` in `journal`, one
// key-value map per event, in logging order.
std::vector<std::map<std::string, std::string>> JournalEventDetails(
    AggregatedJournal& journal,
    std::string_view event_name) {
  std::vector<std::map<std::string, std::string>> events;
  for (AggregatedJournal::EntryBuffer::Iterator it = journal.Items(); it;
       ++it) {
    const std::unique_ptr<AggregatedJournal::Entry>* entry_ptr = *it;
    if (!entry_ptr || !*entry_ptr || !(*entry_ptr)->data ||
        (*entry_ptr)->data->event != event_name) {
      continue;
    }
    std::map<std::string, std::string>& details = events.emplace_back();
    for (const mojom::JournalDetailsPtr& detail : (*entry_ptr)->data->details) {
      details[detail->key] = detail->value;
    }
  }
  return events;
}

// Returns the number of heartbeat ping failures logged to `journal`.
int CountHeartbeatPingFailures(AggregatedJournal& journal) {
  return JournalEventDetails(journal, kHeartbeatPingFailedEvent).size();
}

}  // namespace

// Tests `ActorTaskBackgroundWorker` against a fake delegate. Every scene
// is foreground active unless a test says otherwise. Skipped when
// backgrounding is unavailable in the current configuration.
class ActorTaskBackgroundWorkerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitWithFeatures(
        {kPageActionMenu, kActorTools, kGeminiActor,
         kEnableBackgroundContinuedProcessing},
        {});
    if (!IsGeminiActorBackgroundingEnabled()) {
      GTEST_SKIP() << "Backgrounding is unavailable in this configuration.";
    }
    worker_ = std::make_unique<ActorTaskBackgroundWorker>(
        &delegate_, ActorTaskId(1), journal_);
  }

  void TearDown() override {
    worker_.reset();
    PlatformTest::TearDown();
  }

  // Adds `web_state` to the worker's observed WebStates.
  void AddControlledWebState(web::WebState* web_state) {
    worker_->OnWebStateAdded(web_state);
  }

  // Creates a visible fake WebState with a main frame, owned by the fixture
  // until `DestroyWebState()` or tear-down.
  web::FakeWebState* CreateShownWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    AttachMainWebFrame(web_state.get());
    web_state->WasShown();
    web::FakeWebState* web_state_ptr = web_state.get();
    owned_web_states_.push_back(std::move(web_state));
    return web_state_ptr;
  }

  // Destroys `web_state`, which must come from `CreateShownWebState()`.
  void DestroyWebState(web::WebState* web_state) {
    std::erase_if(owned_web_states_,
                  [web_state](const std::unique_ptr<web::FakeWebState>& owned) {
                    return owned.get() == web_state;
                  });
  }

  // Sets `task_update` as the last update and transitions the task to acting.
  void StartActing(const std::string& task_update = "Act") {
    delegate_.last_task_update = task_update;
    delegate_.state = ActorTaskState::kActing;
    worker_->OnStateChanged(ActorTaskState::kActing);
  }

  // Attaches a main frame to `web_state` and returns it.
  web::FakeWebFrame* AttachMainWebFrame(web::FakeWebState* web_state) {
    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    auto main_frame = web::FakeWebFrame::CreateMainWebFrame();
    web::FakeWebFrame* main_frame_ptr = main_frame.get();
    frames_manager->AddWebFrame(std::move(main_frame));
    web_state->SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                   std::move(frames_manager));
    return main_frame_ptr;
  }

  // Returns the main frame attached to `web_state`.
  web::FakeWebFrame* MainWebFrame(web::WebState* web_state) {
    return static_cast<web::FakeWebFrame*>(
        web_state->GetWebFramesManager(web::ContentWorld::kIsolatedWorld)
            ->GetMainWebFrame());
  }

  // Returns whether the heartbeat timer is running.
  bool IsHeartbeatTimerRunning() const {
    return worker_->heartbeat_timer_.IsRunning();
  }

  // Returns the worker's current background task context.
  BackgroundContinuedProcessingTaskContext* BackgroundTaskContext() const {
    return worker_->background_task_context_;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  AggregatedJournal journal_;
  FakeTaskStateDelegate delegate_;
  std::unique_ptr<ActorTaskBackgroundWorker> worker_;
  std::vector<std::unique_ptr<web::FakeWebState>> owned_web_states_;
};

#pragma mark - Background task context

// Tests that `SetContext()` applies the delegate's last task update.
TEST_F(ActorTaskBackgroundWorkerTest, SetContextAppliesLastTaskUpdate) {
  delegate_.last_task_update = "Foo";
  BackgroundContinuedProcessingTaskContext* context = CreateContext();

  worker_->SetContext(context);

  EXPECT_NSEQ(@"Foo", context.subtitle);
}

// Tests that entering the acting state applies the delegate's last task
// update as the subtitle, ignoring empty updates.
TEST_F(ActorTaskBackgroundWorkerTest, ActingStateRefreshesSubtitle) {
  BackgroundContinuedProcessingTaskContext* context = CreateContext();
  worker_->SetContext(context);

  StartActing("Bar");
  EXPECT_NSEQ(@"Bar", context.subtitle);

  // An empty update keeps the previous subtitle.
  StartActing("");
  EXPECT_NSEQ(@"Bar", context.subtitle);
}

// Tests that `OnWillExecuteTool()` advances the context progress.
TEST_F(ActorTaskBackgroundWorkerTest, OnWillExecuteToolIncrementsProgress) {
  BackgroundContinuedProcessingTaskContext* context = CreateContext();
  worker_->SetContext(context);
  ASSERT_EQ(0, context.completedUnits);

  worker_->OnWillExecuteTool();

  EXPECT_GT(context.completedUnits, 0);
}

// Tests that `OnStopped(true)` completes the context with success and fills
// the remaining progress.
TEST_F(ActorTaskBackgroundWorkerTest, OnStoppedWithSuccess) {
  BackgroundContinuedProcessingTaskContext* context = CreateContext();
  worker_->SetContext(context);
  worker_->OnWillExecuteTool();
  ASSERT_LT(context.fractionCompleted, 1.0);

  worker_->OnStopped(/*success=*/true);

  EXPECT_TRUE(context.completed);
  EXPECT_DOUBLE_EQ(1.0, context.fractionCompleted);
}

// Tests that `OnStopped(false)` completes the context with failure.
TEST_F(ActorTaskBackgroundWorkerTest, OnStoppedWithFailure) {
  BackgroundContinuedProcessingTaskContext* context = CreateContext();
  worker_->SetContext(context);

  worker_->OnStopped(/*success=*/false);

  EXPECT_TRUE(context.completed);
  EXPECT_DOUBLE_EQ(0.0, context.fractionCompleted);
}

// Tests that destroying the worker fails a live context.
TEST_F(ActorTaskBackgroundWorkerTest, DestructorFailsLiveContext) {
  BackgroundContinuedProcessingTaskContext* context = CreateContext();
  worker_->SetContext(context);
  ASSERT_FALSE(context.completed);

  worker_.reset();

  EXPECT_TRUE(context.completed);
  EXPECT_DOUBLE_EQ(0.0, context.fractionCompleted);
}

#pragma mark - State policy

// Tests that waiting on the user completes the context with success, displays
// the wait message as the title with the task's title as subtitle, and leaves
// the progress reached so far unfilled.
TEST_F(ActorTaskBackgroundWorkerTest,
       WaitingOnUserCompletesContextWithoutFillingProgress) {
  BackgroundContinuedProcessingTaskContext* context = CreateContext();
  worker_->SetContext(context);
  worker_->OnWillExecuteTool();
  const double progress = context.fractionCompleted;
  ASSERT_GT(progress, 0.0);
  ASSERT_LT(progress, 1.0);

  delegate_.title = "Book a table";
  delegate_.state = ActorTaskState::kWaitingOnUser;
  worker_->OnStateChanged(ActorTaskState::kWaitingOnUser);

  EXPECT_TRUE(context.completed);
  EXPECT_DOUBLE_EQ(progress, context.fractionCompleted);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_ACTOR_BACKGROUND_TASK_INPUT_NEEDED),
      context.title);
  EXPECT_NSEQ(@"Book a table", context.subtitle);
  EXPECT_EQ(nil, BackgroundTaskContext());
}

// Tests that pausing, by the actor or by the user, completes the context with
// the stopped message as the title, the task's title as subtitle, and leaves
// the progress unfilled.
TEST_F(ActorTaskBackgroundWorkerTest, PausedStatesCompleteContextAsStopped) {
  delegate_.title = "Book a table";
  for (ActorTaskState paused_state :
       {ActorTaskState::kPausedByActor, ActorTaskState::kPausedByUser}) {
    BackgroundContinuedProcessingTaskContext* context = CreateContext();
    worker_->SetContext(context);

    delegate_.state = paused_state;
    worker_->OnStateChanged(paused_state);

    EXPECT_TRUE(context.completed);
    EXPECT_DOUBLE_EQ(0.0, context.fractionCompleted);
    EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_ACTOR_BACKGROUND_TASK_STOPPED),
                context.title);
    EXPECT_NSEQ(@"Book a table", context.subtitle);
    EXPECT_EQ(nil, BackgroundTaskContext());
  }
}

// Tests that the init, acting and reflecting states keep the context.
TEST_F(ActorTaskBackgroundWorkerTest, ActiveStatesKeepContext) {
  BackgroundContinuedProcessingTaskContext* context = CreateContext();
  worker_->SetContext(context);

  for (ActorTaskState active_state :
       {ActorTaskState::kInit, ActorTaskState::kActing,
        ActorTaskState::kReflecting}) {
    worker_->OnStateChanged(active_state);

    EXPECT_FALSE(context.completed);
    EXPECT_NSEQ(context, BackgroundTaskContext());
  }
}

// Tests that terminal states don't finalize the context in `OnStateChanged()`,
// so that `OnStopped()` can report the actual outcome.
TEST_F(ActorTaskBackgroundWorkerTest, TerminalStatesLeaveContextToStop) {
  BackgroundContinuedProcessingTaskContext* context = CreateContext();
  worker_->SetContext(context);

  worker_->OnStateChanged(ActorTaskState::kCancelled);
  EXPECT_FALSE(context.completed);
  EXPECT_NSEQ(context, BackgroundTaskContext());

  worker_->OnStopped(/*success=*/false);
  EXPECT_TRUE(context.completed);
  EXPECT_DOUBLE_EQ(0.0, context.fractionCompleted);
  EXPECT_EQ(nil, BackgroundTaskContext());
}

// Tests that an already completed (e.g. expired) context is only cleared when
// entering a state that waits on the user, without being completed again.
TEST_F(ActorTaskBackgroundWorkerTest,
       CompletedContextIsClearedOnWaitingOnUser) {
  BackgroundContinuedProcessingTaskContext* context = CreateContext();
  worker_->SetContext(context);
  [context setTaskCompletedWithSuccess:NO];

  delegate_.state = ActorTaskState::kPausedByActor;
  worker_->OnStateChanged(ActorTaskState::kPausedByActor);

  // Neither the title nor the subtitle is updated, so the context wasn't
  // completed again.
  EXPECT_TRUE(context.completed);
  EXPECT_NSEQ(@"Test Task", context.title);
  EXPECT_NSEQ(@"", context.subtitle);
  EXPECT_DOUBLE_EQ(0.0, context.fractionCompleted);
  EXPECT_EQ(nil, BackgroundTaskContext());
}

// Tests that the worker is inert when backgrounding is disabled at
// runtime: no heartbeat, no pings, and the context is ignored.
TEST_F(ActorTaskBackgroundWorkerTest, InertWhenBackgroundingDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(
      kEnableBackgroundContinuedProcessing);
  ASSERT_FALSE(IsGeminiActorBackgroundingEnabled());

  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebFrame* main_frame = AttachMainWebFrame(web_state.get());
  AddControlledWebState(web_state.get());
  StartActing();
  EXPECT_FALSE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(800));
  EXPECT_EQ(0u, main_frame->GetJavaScriptCallHistory().size());

  // The context is ignored, so stopping must not complete it.
  delegate_.last_task_update = "Foo";
  BackgroundContinuedProcessingTaskContext* context = CreateContext();
  worker_->SetContext(context);
  worker_->OnStopped(/*success=*/true);
  EXPECT_FALSE(context.completed);
  EXPECT_NSEQ(@"", context.subtitle);
}

#pragma mark - Metrics and journal

// Tests that stopping the task with success records the succeeded end reason
// and journals the end with full progress.
TEST_F(ActorTaskBackgroundWorkerTest, RecordsSucceededEndReason) {
  base::HistogramTester histogram_tester;
  worker_->SetContext(CreateContext());

  worker_->OnStopped(/*success=*/true);

  histogram_tester.ExpectUniqueSample(kActorBackgroundTaskEndReasonHistogram,
                                      ActorBackgroundTaskEndReason::kSucceeded,
                                      1);
  std::vector<std::map<std::string, std::string>> events =
      JournalEventDetails(journal_, kBackgroundTaskFinalizedEvent);
  ASSERT_EQ(1u, events.size());
  EXPECT_EQ("succeeded", events[0]["end_reason"]);
  EXPECT_EQ("1", events[0]["progress"]);
}

// Tests that stopping the task with failure records the failed end reason.
TEST_F(ActorTaskBackgroundWorkerTest, RecordsFailedEndReason) {
  base::HistogramTester histogram_tester;
  worker_->SetContext(CreateContext());

  worker_->OnStopped(/*success=*/false);

  histogram_tester.ExpectUniqueSample(kActorBackgroundTaskEndReasonHistogram,
                                      ActorBackgroundTaskEndReason::kFailed, 1);
  std::vector<std::map<std::string, std::string>> events =
      JournalEventDetails(journal_, kBackgroundTaskFinalizedEvent);
  ASSERT_EQ(1u, events.size());
  EXPECT_EQ("failed", events[0]["end_reason"]);
}

// Tests that entering a state that waits on the user records the
// waiting-on-user end reason.
TEST_F(ActorTaskBackgroundWorkerTest, RecordsWaitingOnUserEndReason) {
  base::HistogramTester histogram_tester;
  worker_->SetContext(CreateContext());

  delegate_.state = ActorTaskState::kWaitingOnUser;
  worker_->OnStateChanged(ActorTaskState::kWaitingOnUser);

  histogram_tester.ExpectUniqueSample(
      kActorBackgroundTaskEndReasonHistogram,
      ActorBackgroundTaskEndReason::kWaitingOnUser, 1);
  std::vector<std::map<std::string, std::string>> events =
      JournalEventDetails(journal_, kBackgroundTaskFinalizedEvent);
  ASSERT_EQ(1u, events.size());
  EXPECT_EQ("waiting_on_user", events[0]["end_reason"]);
}

// Tests that a context that already expired is reported as such, whichever
// path finalizes it, with its progress at the time it was completed.
TEST_F(ActorTaskBackgroundWorkerTest, RecordsExpiredEndReason) {
  base::HistogramTester histogram_tester;
  BackgroundContinuedProcessingTaskContext* context = CreateContext();
  worker_->SetContext(context);
  [context setTaskCompletedWithSuccess:NO];

  worker_->OnStopped(/*success=*/true);

  histogram_tester.ExpectUniqueSample(kActorBackgroundTaskEndReasonHistogram,
                                      ActorBackgroundTaskEndReason::kExpired,
                                      1);
  std::vector<std::map<std::string, std::string>> events =
      JournalEventDetails(journal_, kBackgroundTaskFinalizedEvent);
  ASSERT_EQ(1u, events.size());
  EXPECT_EQ("expired", events[0]["end_reason"]);
  EXPECT_EQ("0", events[0]["progress"]);
}

// Tests that a context that was cancelled from the Live Activity is reported
// as cancelled, whichever path finalizes it, with its progress at the time it
// was completed.
TEST_F(ActorTaskBackgroundWorkerTest, RecordsCancelledEndReason) {
  base::HistogramTester histogram_tester;
  BackgroundContinuedProcessingTaskContext* context =
      OCMPartialMock(CreateContext());
  OCMStub([context isCancelled]).andReturn(YES);
  worker_->SetContext(context);
  [context setTaskCompletedWithSuccess:NO];

  worker_->OnStopped(/*success=*/true);

  histogram_tester.ExpectUniqueSample(kActorBackgroundTaskEndReasonHistogram,
                                      ActorBackgroundTaskEndReason::kCancelled,
                                      1);
  std::vector<std::map<std::string, std::string>> events =
      JournalEventDetails(journal_, kBackgroundTaskFinalizedEvent);
  ASSERT_EQ(1u, events.size());
  EXPECT_EQ("cancelled", events[0]["end_reason"]);
  EXPECT_EQ("0", events[0]["progress"]);
}

// Tests that destroying the worker with a live context records the
// destroyed end reason.
TEST_F(ActorTaskBackgroundWorkerTest, RecordsDestroyedEndReason) {
  base::HistogramTester histogram_tester;
  worker_->SetContext(CreateContext());

  worker_.reset();

  histogram_tester.ExpectUniqueSample(kActorBackgroundTaskEndReasonHistogram,
                                      ActorBackgroundTaskEndReason::kDestroyed,
                                      1);
  std::vector<std::map<std::string, std::string>> events =
      JournalEventDetails(journal_, kBackgroundTaskFinalizedEvent);
  ASSERT_EQ(1u, events.size());
  EXPECT_EQ("destroyed", events[0]["end_reason"]);
}

// Tests that the duration from receiving the context to ending the background
// task is recorded.
TEST_F(ActorTaskBackgroundWorkerTest, RecordsDuration) {
  base::HistogramTester histogram_tester;
  worker_->SetContext(CreateContext());

  task_environment_.FastForwardBy(base::Seconds(42));
  worker_->OnStopped(/*success=*/true);

  histogram_tester.ExpectUniqueTimeSample(kActorBackgroundTaskDurationHistogram,
                                          base::Seconds(42), 1);
}

// Tests that nothing is recorded or journaled when finalizing without a
// context.
TEST_F(ActorTaskBackgroundWorkerTest, RecordsNothingWithoutContext) {
  base::HistogramTester histogram_tester;

  worker_->OnStateChanged(ActorTaskState::kPausedByUser);
  worker_->OnStopped(/*success=*/false);
  worker_.reset();

  histogram_tester.ExpectTotalCount(kActorBackgroundTaskEndReasonHistogram, 0);
  histogram_tester.ExpectTotalCount(kActorBackgroundTaskDurationHistogram, 0);
  EXPECT_TRUE(
      JournalEventDetails(journal_, kBackgroundTaskFinalizedEvent).empty());
}

// Tests that a finalized context is recorded once even if later hooks fire.
TEST_F(ActorTaskBackgroundWorkerTest, RecordsContextOnce) {
  base::HistogramTester histogram_tester;
  worker_->SetContext(CreateContext());

  delegate_.state = ActorTaskState::kWaitingOnUser;
  worker_->OnStateChanged(ActorTaskState::kWaitingOnUser);
  worker_->OnStopped(/*success=*/false);
  worker_.reset();

  histogram_tester.ExpectTotalCount(kActorBackgroundTaskEndReasonHistogram, 1);
  histogram_tester.ExpectTotalCount(kActorBackgroundTaskDurationHistogram, 1);
  EXPECT_EQ(
      1u, JournalEventDetails(journal_, kBackgroundTaskFinalizedEvent).size());
}

// Tests that receiving a context journals the start of the background task.
TEST_F(ActorTaskBackgroundWorkerTest, JournalsBackgroundTaskStart) {
  worker_->SetContext(CreateContext());

  std::vector<std::map<std::string, std::string>> events =
      JournalEventDetails(journal_, kBackgroundTaskStartedEvent);
  ASSERT_EQ(1u, events.size());
  EXPECT_EQ("background_task_started", events[0]["status"]);
}

#pragma mark - Accessors

// Tests that `task_id()` reflects the constructor argument and `title()` the
// delegate's title.
TEST_F(ActorTaskBackgroundWorkerTest, TaskIdAndTitleAccessors) {
  EXPECT_EQ(ActorTaskId(1), worker_->task_id());
  EXPECT_EQ("Test Task", worker_->title());

  delegate_.title = "Renamed";
  EXPECT_EQ("Renamed", worker_->title());
}

// Tests that a weak pointer to the worker is invalidated by its
// destruction.
TEST_F(ActorTaskBackgroundWorkerTest, WeakPtrInvalidatedOnDestruction) {
  base::WeakPtr<ActorTaskBackgroundWorker> weak_worker = worker_->GetWeakPtr();
  ASSERT_TRUE(weak_worker);
  EXPECT_EQ(worker_.get(), weak_worker.get());

  worker_.reset();

  EXPECT_FALSE(weak_worker);
}

#pragma mark - ShouldRequestBackgroundTask

// Tests that a background task is requested only in states that keep one
// alive (init, acting, reflecting), recording the boolean request eligibility
// and `kStateDoesNotKeepAlive` when ineligible.
TEST_F(ActorTaskBackgroundWorkerTest, ShouldRequestBackgroundTaskPerState) {
  const struct {
    ActorTaskState state;
    bool expected;
  } kCases[] = {
      {ActorTaskState::kInit, true},
      {ActorTaskState::kActing, true},
      {ActorTaskState::kReflecting, true},
      {ActorTaskState::kWaitingOnUser, false},
      {ActorTaskState::kPausedByActor, false},
      {ActorTaskState::kPausedByUser, false},
      {ActorTaskState::kCancelled, false},
      {ActorTaskState::kFinished, false},
      {ActorTaskState::kFailed, false},
  };
  for (const auto& test_case : kCases) {
    base::HistogramTester histogram_tester;
    delegate_.state = test_case.state;
    EXPECT_EQ(test_case.expected, worker_->ShouldRequestBackgroundTask())
        << "state: " << static_cast<int>(test_case.state);
    histogram_tester.ExpectUniqueSample(
        kActorBackgroundTaskRequestEligibilityHistogram, test_case.expected, 1);
    if (test_case.expected) {
      histogram_tester.ExpectTotalCount(
          kActorBackgroundTaskIneligibilityReasonHistogram, 0);
    } else {
      histogram_tester.ExpectUniqueSample(
          kActorBackgroundTaskIneligibilityReasonHistogram,
          ActorBackgroundTaskIneligibilityReason::kStateDoesNotKeepAlive, 1);
    }
  }
}

// Tests that a live context blocks a new request, while a completed (e.g.
// expired) context does not, recording the corresponding request eligibility
// and `kAlreadyHasContext`.
TEST_F(ActorTaskBackgroundWorkerTest,
       ShouldRequestBackgroundTaskDependsOnContextLiveness) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(worker_->ShouldRequestBackgroundTask());
  histogram_tester.ExpectUniqueSample(
      kActorBackgroundTaskRequestEligibilityHistogram, true, 1);
  histogram_tester.ExpectTotalCount(
      kActorBackgroundTaskIneligibilityReasonHistogram, 0);

  BackgroundContinuedProcessingTaskContext* context = CreateContext();
  worker_->SetContext(context);
  EXPECT_FALSE(worker_->ShouldRequestBackgroundTask());
  histogram_tester.ExpectBucketCount(
      kActorBackgroundTaskRequestEligibilityHistogram, false, 1);
  histogram_tester.ExpectUniqueSample(
      kActorBackgroundTaskIneligibilityReasonHistogram,
      ActorBackgroundTaskIneligibilityReason::kAlreadyHasContext, 1);

  [context setTaskCompletedWithSuccess:NO];
  EXPECT_TRUE(worker_->ShouldRequestBackgroundTask());
  histogram_tester.ExpectBucketCount(
      kActorBackgroundTaskRequestEligibilityHistogram, true, 2);
  histogram_tester.ExpectTotalCount(
      kActorBackgroundTaskRequestEligibilityHistogram, 3);
  histogram_tester.ExpectTotalCount(
      kActorBackgroundTaskIneligibilityReasonHistogram, 1);
}

// Tests that no background task is requested when backgrounding is disabled,
// even in a state that keeps one alive, and that all applicable ineligibility
// reasons are recorded when multiple conditions fail.
TEST_F(ActorTaskBackgroundWorkerTest,
       ShouldRequestBackgroundTaskFalseWhenDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(
      kEnableBackgroundContinuedProcessing);
  ASSERT_FALSE(IsGeminiActorBackgroundingEnabled());
  base::HistogramTester histogram_tester;
  delegate_.state = ActorTaskState::kActing;

  EXPECT_FALSE(worker_->ShouldRequestBackgroundTask());
  histogram_tester.ExpectUniqueSample(
      kActorBackgroundTaskRequestEligibilityHistogram, false, 1);
  histogram_tester.ExpectUniqueSample(
      kActorBackgroundTaskIneligibilityReasonHistogram,
      ActorBackgroundTaskIneligibilityReason::kBackgroundingDisabled, 1);

  // When both backgrounding is disabled and the state does not keep a
  // background task alive, both ineligibility reasons are recorded while
  // eligibility is still recorded once.
  delegate_.state = ActorTaskState::kWaitingOnUser;
  EXPECT_FALSE(worker_->ShouldRequestBackgroundTask());
  histogram_tester.ExpectUniqueSample(
      kActorBackgroundTaskRequestEligibilityHistogram, false, 2);
  histogram_tester.ExpectBucketCount(
      kActorBackgroundTaskIneligibilityReasonHistogram,
      ActorBackgroundTaskIneligibilityReason::kBackgroundingDisabled, 2);
  histogram_tester.ExpectBucketCount(
      kActorBackgroundTaskIneligibilityReasonHistogram,
      ActorBackgroundTaskIneligibilityReason::kStateDoesNotKeepAlive, 1);
  histogram_tester.ExpectTotalCount(
      kActorBackgroundTaskIneligibilityReasonHistogram, 3);
}

#pragma mark - Heartbeat

// Tests that adding a controlled WebState starts the 400ms heartbeat timer
// immediately to keep WebContent processes alive, and sends periodic
// JavaScript pings.
TEST_F(ActorTaskBackgroundWorkerTest, HeartbeatStartsOnWebStateAdded) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebFrame* main_frame = AttachMainWebFrame(web_state.get());
  ASSERT_TRUE(main_frame);

  EXPECT_FALSE(IsHeartbeatTimerRunning());

  AddControlledWebState(web_state.get());
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  StartActing("Starting actuation");
  EXPECT_TRUE(IsHeartbeatTimerRunning());
  EXPECT_EQ(0u, main_frame->GetJavaScriptCallHistory().size());

  // Fast forward 399ms; timer should not have fired yet.
  task_environment_.FastForwardBy(base::Milliseconds(399));
  EXPECT_EQ(0u, main_frame->GetJavaScriptCallHistory().size());

  // Fast forward 1ms (total 400ms); first ping should have executed.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(1u, main_frame->GetJavaScriptCallHistory().size());
  EXPECT_EQ(u";", main_frame->GetLastJavaScriptCall());

  // Fast forward another 800ms; two more pings should have executed.
  task_environment_.FastForwardBy(base::Milliseconds(800));
  EXPECT_EQ(3u, main_frame->GetJavaScriptCallHistory().size());
}

// Tests that entering the acting state starts the heartbeat timer if it was
// not already running.
TEST_F(ActorTaskBackgroundWorkerTest, HeartbeatStartsOnActingState) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Temporarily disable backgrounding to verify timer does not start initially.
  scoped_feature_list.InitAndDisableFeature(
      kEnableBackgroundContinuedProcessing);

  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebFrame* main_frame = AttachMainWebFrame(web_state.get());
  ASSERT_TRUE(main_frame);

  AddControlledWebState(web_state.get());
  StartActing("Act without backgrounding");
  EXPECT_FALSE(IsHeartbeatTimerRunning());

  // Resetting the local override restores the fixture-level features, which
  // enable background continued processing.
  scoped_feature_list.Reset();

  // Entering the acting state again should start the timer.
  StartActing("Starting actuation");
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(1u, main_frame->GetJavaScriptCallHistory().size());
  EXPECT_EQ(u";", main_frame->GetLastJavaScriptCall());
}

// Tests that the heartbeat timer continues running while the task is
// reflecting.
TEST_F(ActorTaskBackgroundWorkerTest, HeartbeatPersistsDuringReflecting) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebFrame* main_frame = AttachMainWebFrame(web_state.get());
  ASSERT_TRUE(main_frame);

  AddControlledWebState(web_state.get());
  StartActing("Executing tool");
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  // Tool execution completes, transitioning the task to reflecting.
  delegate_.state = ActorTaskState::kReflecting;
  worker_->OnStateChanged(ActorTaskState::kReflecting);
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  // Verify pings continue while in reflecting state.
  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(1u, main_frame->GetJavaScriptCallHistory().size());
  EXPECT_EQ(u";", main_frame->GetLastJavaScriptCall());
}

// Tests that a terminal state stops the heartbeat, and that the heartbeat
// does not start while the task is terminal.
TEST_F(ActorTaskBackgroundWorkerTest, HeartbeatStartsAndStopsOnTerminalState) {
  // No controlled WebState yet, so the heartbeat must not start.
  StartActing();
  EXPECT_FALSE(IsHeartbeatTimerRunning());

  auto web_state = std::make_unique<web::FakeWebState>();
  AttachMainWebFrame(web_state.get());
  AddControlledWebState(web_state.get());
  StartActing();
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  // A terminal state stops the heartbeat.
  delegate_.state = ActorTaskState::kFinished;
  worker_->OnStateChanged(ActorTaskState::kFinished);
  EXPECT_FALSE(IsHeartbeatTimerRunning());

  // Events received while terminal must not restart it.
  worker_->OnWebStateAdded(web_state.get());
  worker_->OnStateChanged(ActorTaskState::kActing);
  web_state->WasHidden();
  worker_->SetAllScenesForegroundActive(false);
  EXPECT_FALSE(IsHeartbeatTimerRunning());
}

// Tests that stopping the task stops the heartbeat timer.
TEST_F(ActorTaskBackgroundWorkerTest, HeartbeatStopsOnStopped) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebFrame* main_frame = AttachMainWebFrame(web_state.get());
  ASSERT_TRUE(main_frame);

  AddControlledWebState(web_state.get());
  StartActing();
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(1u, main_frame->GetJavaScriptCallHistory().size());

  // Stop the task.
  worker_->OnStopped(/*success=*/true);
  EXPECT_FALSE(IsHeartbeatTimerRunning());

  // Verify no further pings occur after stopping.
  task_environment_.FastForwardBy(base::Milliseconds(800));
  EXPECT_EQ(1u, main_frame->GetJavaScriptCallHistory().size());
}

// Tests that pausing the task does not stop the heartbeat timer so that
// WebContent processes remain alive throughout the entire duration of the
// task, and that resuming keeps it running.
TEST_F(ActorTaskBackgroundWorkerTest, HeartbeatPersistsDuringPause) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebFrame* main_frame = AttachMainWebFrame(web_state.get());
  ASSERT_TRUE(main_frame);

  AddControlledWebState(web_state.get());
  StartActing();
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(1u, main_frame->GetJavaScriptCallHistory().size());

  // Pause the task. The heartbeat timer must remain running.
  delegate_.state = ActorTaskState::kPausedByUser;
  worker_->OnStateChanged(ActorTaskState::kPausedByUser);
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(2u, main_frame->GetJavaScriptCallHistory().size());

  // Resuming returns the task to acting.
  delegate_.state = ActorTaskState::kActing;
  worker_->OnStateChanged(ActorTaskState::kActing);
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(3u, main_frame->GetJavaScriptCallHistory().size());
}

// Tests that waiting on user input does not stop the heartbeat timer.
TEST_F(ActorTaskBackgroundWorkerTest, HeartbeatPersistsDuringWaitingOnUser) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebFrame* main_frame = AttachMainWebFrame(web_state.get());
  ASSERT_TRUE(main_frame);

  AddControlledWebState(web_state.get());
  StartActing();
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  // The task is interrupted to wait on user input.
  delegate_.state = ActorTaskState::kWaitingOnUser;
  worker_->OnStateChanged(ActorTaskState::kWaitingOnUser);
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(1u, main_frame->GetJavaScriptCallHistory().size());

  // Uninterrupting resumes the task.
  delegate_.state = ActorTaskState::kActing;
  worker_->OnStateChanged(ActorTaskState::kActing);
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(2u, main_frame->GetJavaScriptCallHistory().size());
}

// Tests that destroying the last controlled WebState stops the heartbeat
// timer immediately, through the worker's own observation, and that
// adding a WebState restarts it.
TEST_F(ActorTaskBackgroundWorkerTest, HeartbeatStopsWhenAllWebStatesDestroyed) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebFrame* main_frame = AttachMainWebFrame(web_state.get());
  ASSERT_TRUE(main_frame);

  AddControlledWebState(web_state.get());
  StartActing();
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(1u, main_frame->GetJavaScriptCallHistory().size());

  // Destroying the WebState stops the timer immediately.
  web_state.reset();
  EXPECT_FALSE(IsHeartbeatTimerRunning());

  // Adding a new WebState while the task is actuating resumes the heartbeat
  // timer.
  auto web_state2 = std::make_unique<web::FakeWebState>();
  AttachMainWebFrame(web_state2.get());
  AddControlledWebState(web_state2.get());
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  web_state2.reset();
  EXPECT_FALSE(IsHeartbeatTimerRunning());
}

// Tests that heartbeat pings are fire-and-forget, logging failures to the
// journal without stopping the heartbeat.
TEST_F(ActorTaskBackgroundWorkerTest, HeartbeatPingsFireAndForget) {
  auto web_state = std::make_unique<web::FakeWebState>();
  // Note: Do not add a result for executed JS so FakeWebFrame generates an
  // error.
  web::FakeWebFrame* main_frame = AttachMainWebFrame(web_state.get());
  ASSERT_TRUE(main_frame);

  AddControlledWebState(web_state.get());
  StartActing();
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  // Fast forward across 10 intervals (4000ms total).
  task_environment_.FastForwardBy(base::Milliseconds(4000));
  EXPECT_EQ(10u, main_frame->GetJavaScriptCallHistory().size());
  EXPECT_EQ(u";", main_frame->GetLastJavaScriptCall());
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  // Verify journal logs recorded the failures.
  EXPECT_EQ(10, CountHeartbeatPingFailures(journal_));
}

// Tests that successful heartbeat pings do not log failure events to the
// journal.
TEST_F(ActorTaskBackgroundWorkerTest,
       HeartbeatSuccessfulPingDoesNotLogFailure) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebFrame* main_frame = AttachMainWebFrame(web_state.get());
  ASSERT_TRUE(main_frame);
  base::Value success_result;
  main_frame->AddResultForExecutedJs(&success_result, u";");

  AddControlledWebState(web_state.get());
  StartActing();
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(1u, main_frame->GetJavaScriptCallHistory().size());

  EXPECT_EQ(0, CountHeartbeatPingFailures(journal_));
}

// Tests that the heartbeat timer is not started when the backgrounding feature
// parameter is disabled, even though `kEnableBackgroundContinuedProcessing` is
// enabled.
TEST_F(ActorTaskBackgroundWorkerTest,
       HeartbeatDisabledWhenFeatureParamDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kGeminiActor, {{kGeminiActorBackgroundingParam, "false"}});

  auto web_state = std::make_unique<web::FakeWebState>();
  AttachMainWebFrame(web_state.get());

  AddControlledWebState(web_state.get());
  EXPECT_FALSE(IsHeartbeatTimerRunning());

  StartActing("Act with backgrounding disabled");
  EXPECT_FALSE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(800));
  EXPECT_FALSE(IsHeartbeatTimerRunning());
}

// Tests that the heartbeat timer uses the interval configured via the feature
// parameter.
TEST_F(ActorTaskBackgroundWorkerTest, HeartbeatUsesConfiguredInterval) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kGeminiActor,
      {{kGeminiActorBackgroundWebStateKeepAliveHeartbeatIntervalParam,
        "250ms"}});

  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebFrame* main_frame = AttachMainWebFrame(web_state.get());
  ASSERT_TRUE(main_frame);

  AddControlledWebState(web_state.get());
  StartActing();
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(249));
  EXPECT_EQ(0u, main_frame->GetJavaScriptCallHistory().size());

  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(1u, main_frame->GetJavaScriptCallHistory().size());
}

// Tests that heartbeat pings are dispatched to multiple controlled WebStates,
// and that destroying one WebState keeps the timer active for the remaining
// valid WebStates until all are destroyed.
TEST_F(ActorTaskBackgroundWorkerTest, HeartbeatMultipleWebStates) {
  auto web_state1 = std::make_unique<web::FakeWebState>();
  web::FakeWebFrame* main_frame1 = AttachMainWebFrame(web_state1.get());
  ASSERT_TRUE(main_frame1);
  base::Value success_result;
  main_frame1->AddResultForExecutedJs(&success_result, u";");

  auto web_state2 = std::make_unique<web::FakeWebState>();
  web::FakeWebFrame* main_frame2 = AttachMainWebFrame(web_state2.get());
  ASSERT_TRUE(main_frame2);

  AddControlledWebState(web_state1.get());
  AddControlledWebState(web_state2.get());

  StartActing("Act across multiple WebStates");
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  // Fast-forward one interval; both frames receive a ping.
  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(1u, main_frame1->GetJavaScriptCallHistory().size());
  EXPECT_EQ(1u, main_frame2->GetJavaScriptCallHistory().size());
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  // Destroy only the first WebState.
  web_state1.reset();

  // Fast-forward another interval; heartbeat should still be running for
  // `web_state2`.
  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_TRUE(IsHeartbeatTimerRunning());
  EXPECT_EQ(2u, main_frame2->GetJavaScriptCallHistory().size());

  // Destroying the second WebState stops the timer immediately.
  web_state2.reset();
  EXPECT_FALSE(IsHeartbeatTimerRunning());
}

#pragma mark - Heartbeat visibility policy

// Tests that no heartbeat runs while every controlled WebState is visible and
// every scene is foreground active.
TEST_F(ActorTaskBackgroundWorkerTest, NoHeartbeatWhenAllWebStatesShown) {
  web::FakeWebState* web_state = CreateShownWebState();

  AddControlledWebState(web_state);
  StartActing();
  EXPECT_FALSE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(800));
  EXPECT_EQ(0u, MainWebFrame(web_state)->GetJavaScriptCallHistory().size());
}

// Tests that the heartbeat follows the scenes for a visible WebState: it
// starts once a scene is no longer foreground active, since the WebState may
// be in it, and stops once every scene is foreground active again.
TEST_F(ActorTaskBackgroundWorkerTest, HeartbeatFollowsScenesForegroundActive) {
  web::FakeWebState* web_state = CreateShownWebState();
  web::FakeWebFrame* main_frame = MainWebFrame(web_state);
  AddControlledWebState(web_state);
  StartActing();
  ASSERT_FALSE(IsHeartbeatTimerRunning());

  worker_->SetAllScenesForegroundActive(false);
  EXPECT_TRUE(IsHeartbeatTimerRunning());
  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(1u, main_frame->GetJavaScriptCallHistory().size());

  worker_->SetAllScenesForegroundActive(true);
  EXPECT_FALSE(IsHeartbeatTimerRunning());
  task_environment_.FastForwardBy(base::Milliseconds(800));
  EXPECT_EQ(1u, main_frame->GetJavaScriptCallHistory().size());
}

// Tests that a scene that is not foreground active does not run the heartbeat
// on its own: a controlled WebState is needed for pings to make sense.
TEST_F(ActorTaskBackgroundWorkerTest, NoHeartbeatWithoutControlledWebStates) {
  worker_->SetAllScenesForegroundActive(false);
  StartActing();
  EXPECT_FALSE(IsHeartbeatTimerRunning());

  // The heartbeat starts as soon as a WebState is controlled.
  web::FakeWebState* web_state = CreateShownWebState();
  AddControlledWebState(web_state);
  EXPECT_TRUE(IsHeartbeatTimerRunning());
}

// Tests that the heartbeat follows the WebState's own visibility while every
// scene is foreground active, through the worker's observation.
TEST_F(ActorTaskBackgroundWorkerTest, HeartbeatFollowsWebStateVisibility) {
  web::FakeWebState* web_state = CreateShownWebState();
  web::FakeWebFrame* main_frame = MainWebFrame(web_state);
  AddControlledWebState(web_state);
  StartActing();
  ASSERT_FALSE(IsHeartbeatTimerRunning());

  // Hiding the tab (e.g. switching to another tab) starts the heartbeat.
  web_state->WasHidden();
  EXPECT_TRUE(IsHeartbeatTimerRunning());
  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(1u, main_frame->GetJavaScriptCallHistory().size());

  // Showing it again stops the heartbeat.
  web_state->WasShown();
  EXPECT_FALSE(IsHeartbeatTimerRunning());
  task_environment_.FastForwardBy(base::Milliseconds(800));
  EXPECT_EQ(1u, main_frame->GetJavaScriptCallHistory().size());
}

// Tests that a single hidden WebState is enough to run the heartbeat, which
// pings every controlled WebState, shown or not.
TEST_F(ActorTaskBackgroundWorkerTest, HeartbeatRunsWhileAnyWebStateHidden) {
  web::FakeWebState* shown_web_state = CreateShownWebState();
  web::FakeWebState* hidden_web_state = CreateShownWebState();
  hidden_web_state->WasHidden();

  AddControlledWebState(shown_web_state);
  EXPECT_FALSE(IsHeartbeatTimerRunning());
  AddControlledWebState(hidden_web_state);
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(1u,
            MainWebFrame(shown_web_state)->GetJavaScriptCallHistory().size());
  EXPECT_EQ(1u,
            MainWebFrame(hidden_web_state)->GetJavaScriptCallHistory().size());

  // Showing the hidden tab leaves every tab shown, so the heartbeat stops.
  hidden_web_state->WasShown();
  EXPECT_FALSE(IsHeartbeatTimerRunning());
}

// Tests that a ping re-evaluates the policy: the heartbeat stops on its next
// tick once the task is terminal, even without a notification.
TEST_F(ActorTaskBackgroundWorkerTest, HeartbeatStopsOnTickOnceTerminal) {
  web::FakeWebState* web_state = CreateShownWebState();
  AddControlledWebState(web_state);
  worker_->SetAllScenesForegroundActive(false);
  StartActing();
  ASSERT_TRUE(IsHeartbeatTimerRunning());

  // Finish the task without notifying the worker.
  delegate_.state = ActorTaskState::kFinished;
  task_environment_.FastForwardBy(base::Milliseconds(400));

  EXPECT_FALSE(IsHeartbeatTimerRunning());
  EXPECT_EQ(0u, MainWebFrame(web_state)->GetJavaScriptCallHistory().size());
}

// Tests that a destroyed WebState no longer counts: destroying the only hidden
// WebState stops the heartbeat while a shown one remains, without crashing.
TEST_F(ActorTaskBackgroundWorkerTest, DestroyedWebStateNoLongerRunsHeartbeat) {
  web::FakeWebState* shown_web_state = CreateShownWebState();
  auto hidden_web_state = std::make_unique<web::FakeWebState>();
  AttachMainWebFrame(hidden_web_state.get());
  AddControlledWebState(shown_web_state);
  AddControlledWebState(hidden_web_state.get());
  StartActing();
  ASSERT_TRUE(IsHeartbeatTimerRunning());

  hidden_web_state.reset();
  EXPECT_FALSE(IsHeartbeatTimerRunning());

  // Hiding the remaining tab restarts it, and destroying it stops it.
  shown_web_state->WasHidden();
  EXPECT_TRUE(IsHeartbeatTimerRunning());
  DestroyWebState(shown_web_state);
  EXPECT_FALSE(IsHeartbeatTimerRunning());
}

}  // namespace actor
