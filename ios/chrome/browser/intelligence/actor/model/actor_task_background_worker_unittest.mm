// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_task_background_worker.h"

#import <memory>
#import <string>
#import <vector>

#import "base/memory/weak_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/actor/core/aggregated_journal.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_configuration.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_context.h"
#import "ios/chrome/app/background_task/features.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace actor {

namespace {

// Name of the journal event logged when a heartbeat ping fails.
constexpr char kHeartbeatPingFailedEvent[] = "ActorTask::HeartbeatPingFailed";

// Fake delegate exposing settable task state to the worker. Like
// `ActorTask`, it only returns controlled WebStates that are still alive.
class FakeTaskStateDelegate
    : public ActorTaskBackgroundWorker::TaskStateDelegate {
 public:
  std::vector<web::WebState*> GetControlledWebStates() override {
    std::vector<web::WebState*> live_web_states;
    for (const base::WeakPtr<web::WebState>& web_state : web_states) {
      if (web_state) {
        live_web_states.push_back(web_state.get());
      }
    }
    return live_web_states;
  }
  ActorTaskState GetTaskState() const override { return state; }
  const std::string& GetTaskTitle() const override { return title; }
  const std::string& GetLastTaskUpdate() const override {
    return last_task_update;
  }

  std::vector<base::WeakPtr<web::WebState>> web_states;
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

// Returns the number of heartbeat ping failures logged to `journal`.
int CountHeartbeatPingFailures(AggregatedJournal& journal) {
  int failures = 0;
  for (AggregatedJournal::EntryBuffer::Iterator it = journal.Items(); it;
       ++it) {
    const std::unique_ptr<AggregatedJournal::Entry>* entry_ptr = *it;
    if (entry_ptr && *entry_ptr && (*entry_ptr)->data &&
        (*entry_ptr)->data->event == kHeartbeatPingFailedEvent) {
      failures++;
    }
  }
  return failures;
}

}  // namespace

// Tests `ActorTaskBackgroundWorker` against a fake delegate. Skipped when
// backgrounding is unavailable in the current configuration.
class ActorTaskBackgroundWorkerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitWithFeatures(
        {kPageActionMenu, kActorTools, kGeminiClientMigration, kGeminiActor,
         kEnableBackgroundContinuedProcessing},
        {});
    if (!IsGeminiActorBackgroundingEnabled()) {
      GTEST_SKIP() << "Backgrounding is unavailable in this configuration.";
    }
    worker_ = std::make_unique<ActorTaskBackgroundWorker>(
        &delegate_, ActorTaskId(1), journal_);
  }

  // Adds `web_state` to the delegate's controlled WebStates.
  void AddControlledWebState(web::WebState* web_state) {
    delegate_.web_states.push_back(web_state->GetWeakPtr());
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

// Tests that `OnStopped(true)` completes the context with success.
TEST_F(ActorTaskBackgroundWorkerTest, OnStoppedWithSuccess) {
  BackgroundContinuedProcessingTaskContext* context = CreateContext();
  worker_->SetContext(context);

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

// Tests that entering a state that waits on the user or pauses completes the
// context with success and clears it.
TEST_F(ActorTaskBackgroundWorkerTest, LongWaitStatesCompleteContext) {
  for (ActorTaskState long_wait_state :
       {ActorTaskState::kWaitingOnUser, ActorTaskState::kPausedByActor,
        ActorTaskState::kPausedByUser}) {
    BackgroundContinuedProcessingTaskContext* context = CreateContext();
    worker_->SetContext(context);

    worker_->OnStateChanged(long_wait_state);

    EXPECT_TRUE(context.completed);
    EXPECT_DOUBLE_EQ(1.0, context.fractionCompleted);
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
// entering a long-wait state, without being completed again.
TEST_F(ActorTaskBackgroundWorkerTest, CompletedContextIsClearedOnLongWait) {
  BackgroundContinuedProcessingTaskContext* context = CreateContext();
  worker_->SetContext(context);
  [context setTaskCompletedWithSuccess:NO];

  worker_->OnStateChanged(ActorTaskState::kPausedByActor);

  // Progress isn't filled, so the context wasn't completed again.
  EXPECT_TRUE(context.completed);
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
  worker_->OnWebStateAdded();
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
// alive (init, acting, reflecting).
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
    delegate_.state = test_case.state;
    EXPECT_EQ(test_case.expected, worker_->ShouldRequestBackgroundTask())
        << "state: " << static_cast<int>(test_case.state);
  }
}

// Tests that a live context blocks a new request, while a completed (e.g.
// expired) context does not.
TEST_F(ActorTaskBackgroundWorkerTest,
       ShouldRequestBackgroundTaskDependsOnContextLiveness) {
  ASSERT_TRUE(worker_->ShouldRequestBackgroundTask());

  BackgroundContinuedProcessingTaskContext* context = CreateContext();
  worker_->SetContext(context);
  EXPECT_FALSE(worker_->ShouldRequestBackgroundTask());

  [context setTaskCompletedWithSuccess:NO];
  EXPECT_TRUE(worker_->ShouldRequestBackgroundTask());
}

// Tests that no background task is requested when backgrounding is disabled,
// even in a state that keeps one alive.
TEST_F(ActorTaskBackgroundWorkerTest,
       ShouldRequestBackgroundTaskFalseWhenDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(
      kEnableBackgroundContinuedProcessing);
  ASSERT_FALSE(IsGeminiActorBackgroundingEnabled());
  delegate_.state = ActorTaskState::kActing;

  EXPECT_FALSE(worker_->ShouldRequestBackgroundTask());
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
  worker_->OnWebStateAdded();
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
  worker_->OnWebStateAdded();
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
  worker_->OnWebStateAdded();
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
  worker_->OnWebStateAdded();
  worker_->OnStateChanged(ActorTaskState::kActing);
  EXPECT_FALSE(IsHeartbeatTimerRunning());
}

// Tests that stopping the task stops the heartbeat timer.
TEST_F(ActorTaskBackgroundWorkerTest, HeartbeatStopsOnStopped) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebFrame* main_frame = AttachMainWebFrame(web_state.get());
  ASSERT_TRUE(main_frame);

  AddControlledWebState(web_state.get());
  worker_->OnWebStateAdded();
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
  worker_->OnWebStateAdded();
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
  worker_->OnWebStateAdded();
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

// Tests that losing all controlled WebStates stops the heartbeat timer, both
// on the next ping and on `OnWebStateDestroyed()`, and that adding a WebState
// restarts it.
TEST_F(ActorTaskBackgroundWorkerTest, HeartbeatStopsWhenAllWebStatesDestroyed) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebFrame* main_frame = AttachMainWebFrame(web_state.get());
  ASSERT_TRUE(main_frame);

  AddControlledWebState(web_state.get());
  worker_->OnWebStateAdded();
  StartActing();
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(1u, main_frame->GetJavaScriptCallHistory().size());

  // Destroy the WebState.
  web_state.reset();

  // On the next interval, `SendHeartbeatPing()` detects that no valid
  // WebStates remain and stops the timer.
  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_FALSE(IsHeartbeatTimerRunning());

  // Adding a new WebState while the task is actuating resumes the heartbeat
  // timer.
  auto web_state2 = std::make_unique<web::FakeWebState>();
  AttachMainWebFrame(web_state2.get());
  AddControlledWebState(web_state2.get());
  worker_->OnWebStateAdded();
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  // Destroying the last WebState stops the timer immediately.
  web_state2.reset();
  worker_->OnWebStateDestroyed();
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
  worker_->OnWebStateAdded();
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
  worker_->OnWebStateAdded();
  StartActing();
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(1u, main_frame->GetJavaScriptCallHistory().size());

  EXPECT_EQ(0, CountHeartbeatPingFailures(journal_));
}

// Tests that the heartbeat timer is not started when the backgrounding feature
// parameter is disabled, even though the killswitch is enabled.
TEST_F(ActorTaskBackgroundWorkerTest,
       HeartbeatDisabledWhenFeatureParamDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kGeminiActor, {{kGeminiActorBackgroundingParam, "false"}});

  auto web_state = std::make_unique<web::FakeWebState>();
  AttachMainWebFrame(web_state.get());

  AddControlledWebState(web_state.get());
  worker_->OnWebStateAdded();
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
  worker_->OnWebStateAdded();
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
  worker_->OnWebStateAdded();
  AddControlledWebState(web_state2.get());
  worker_->OnWebStateAdded();

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

  // Destroy the second WebState.
  web_state2.reset();

  // On the next interval, all WebStates have expired, so timer stops.
  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_FALSE(IsHeartbeatTimerRunning());
}

}  // namespace actor
