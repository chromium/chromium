// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_worklog_mediator.h"

#import <memory>
#import <optional>

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/actor/core/task_source_info.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_updates_observer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_consumer.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/intelligence/actor/util/actor_test_utils.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state_id.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
constexpr char kTaskTitle[] = "Task Title";
constexpr char kConfirmationMessage[] = "Please confirm";
using enum actor::ActorTaskState;
using enum actor::ToolType;

// Returns the provenance attributed to tasks created by these tests.
actor::TaskSourceInfo TestSource() {
  return actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kTest,
                               /*id=*/std::nullopt);
}
}  // namespace

@interface FakeActuationWorklogConsumer : NSObject <ActuationWorklogConsumer>
@property(nonatomic, copy) NSString* taskTitle;
@property(nonatomic, assign) BOOL actuationActive;
@property(nonatomic, strong) NSMutableArray<ActuationWorklogItem*>* items;
@property(nonatomic, strong) NSMutableArray<ActuationWorklogChip*>* chips;
@property(nonatomic, strong) ActuationInterventionData* intervention;
@end

@implementation FakeActuationWorklogConsumer

- (instancetype)init {
  if ((self = [super init])) {
    _items = [NSMutableArray array];
    _chips = [NSMutableArray array];
  }
  return self;
}

- (void)updateWorklogWithItem:(ActuationWorklogItem*)item
                         chip:(ActuationWorklogChip*)chip
                     animated:(BOOL)animated {
  if (item) {
    [_items addObject:item];
  }
  if (chip) {
    [_chips addObject:chip];
  }
}

- (void)setIntervention:(ActuationInterventionData*)intervention {
  _intervention = intervention;
}

- (void)reset {
  [_items removeAllObjects];
  [_chips removeAllObjects];
  _taskTitle = nil;
  _intervention = nil;
}

@end

// Records the state of the task it observes, as reported by the posted task
// updates.
@interface FakeTaskStateObserver : NSObject <ActorTaskUpdatesObserver>
@property(nonatomic, assign) actor::ActorTaskState state;
@property(nonatomic, assign) BOOL stopped;
@end

@implementation FakeTaskStateObserver

- (void)didRegisterAsObserverForTaskID:(actor::ActorTaskId)taskID
                             taskTitle:(NSString*)taskTitle
                            taskUpdate:(NSString*)taskUpdate
                          currentState:(actor::ActorTaskState)state
                             webStates:(NSArray<NSNumber*>*)webStatesIDs {
  _state = state;
}

- (void)actorTaskWithID:(actor::ActorTaskId)taskID
         didChangeState:(actor::ActorTaskState)newState
              fromState:(actor::ActorTaskState)oldState {
  _state = newState;
}

- (void)actorTaskDidStopWithID:(actor::ActorTaskId)taskID
                    finalState:(actor::ActorTaskState)finalState {
  _state = finalState;
  _stopped = YES;
}

@end

class ActuationWorklogMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(kActorTools);
    actor::ActorServiceFactory::GetInstance();
    profile_ = TestProfileIOS::Builder().Build();
    actor_service_ = actor::ActorServiceFactory::GetForProfile(profile_.get());
    ASSERT_TRUE(actor_service_);
    task_id_ = actor_service_->CreateTask(kTaskTitle, TestSource(),
                                          /*allow_incognito_web_states=*/false);

    fake_consumer_ = [[FakeActuationWorklogConsumer alloc] init];
    mediator_ =
        [[ActuationWorklogMediator alloc] initWithActorService:actor_service_];
    mediator_.consumer = fake_consumer_;
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    fake_consumer_ = nil;
    actor_service_ = nullptr;
    PlatformTest::TearDown();
  }

  // Starts observing `task_id` and waits for the registration posted by the
  // task.
  void StartObservingTask(actor::ActorTaskId task_id) {
    [mediator_ startObservingTaskWithID:task_id];
    actor::FlushCurrentSequence();
  }

  void ChangeState(actor::ActorTaskState new_state,
                   actor::ActorTaskState old_state = kActing) {
    [mediator_ actorTaskWithID:task_id_
                didChangeState:new_state
                     fromState:old_state];
  }

  void EndActuation() { [mediator_ stopObservingTask]; }

  void ExecuteTool(actor::ToolType tool_type, NSString* update = @"") {
    [mediator_ actorTaskWithID:task_id_
               willExecuteTool:tool_type
                    taskUpdate:update
                    onWebState:web::WebStateID::FromSerializedValue(1)];
  }

  // Creates a task and starts observing it, as the Gemini container does
  // synchronously from the task's start notification.
  actor::ActorTaskId CreateAndObserveTask() {
    actor::ActorTaskId task_id = actor_service_->CreateTask(
        kTaskTitle, TestSource(), /*allow_incognito_web_states=*/false);
    [mediator_ startObservingTaskWithID:task_id];
    return task_id;
  }

  // Returns an observer registered for the updates of `task_id`. Updates,
  // including the registration itself, are posted.
  FakeTaskStateObserver* ObserveTaskState(actor::ActorTaskId task_id) {
    FakeTaskStateObserver* observer = [[FakeTaskStateObserver alloc] init];
    actor_service_->AddTaskUpdatesObserver(task_id, observer);
    return observer;
  }

  // Interrupts `task_id` for user confirmation and returns whether the
  // interrupt was accepted, i.e. the task now waits on the user. `ActorTask`
  // stops a task that has no intervention delegate to prompt instead.
  bool RequestConfirmation(actor::ActorTaskId task_id) {
    FakeTaskStateObserver* observer = ObserveTaskState(task_id);
    actor_service_->InterruptTask(
        task_id, actor::ActorTaskInterruptReason::kWaitingUserConfirmation,
        kConfirmationMessage);
    // Delivers the posted state updates and intervention prompt.
    actor::FlushCurrentSequence();
    return !observer.stopped && observer.state == kWaitingOnUser;
  }

  // Creates and observes a task, interrupts it for user confirmation, then
  // delivers the posted intervention prompt.
  actor::ActorTaskId CreateTaskAndRequestConfirmation() {
    actor::ActorTaskId task_id = CreateAndObserveTask();
    RequestConfirmation(task_id);
    return task_id;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<actor::ActorService> actor_service_ = nullptr;
  actor::ActorTaskId task_id_;
  FakeActuationWorklogConsumer* fake_consumer_;
  ActuationWorklogMediator* mediator_;
};

// Tests observer registration forwards the title and emits the initial step.
TEST_F(ActuationWorklogMediatorTest, TestRegisterObserverEmitsInitialStep) {
  StartObservingTask(task_id_);

  EXPECT_NSEQ(fake_consumer_.taskTitle, @(kTaskTitle));
  EXPECT_TRUE(fake_consumer_.actuationActive);
  ASSERT_EQ(fake_consumer_.items.count, 1u);
  EXPECT_GT(fake_consumer_.items[0].title.length, 0u);
  EXPECT_GT(fake_consumer_.items[0].subtitle.length, 0u);
  EXPECT_TRUE(fake_consumer_.items[0].active);
  EXPECT_EQ(fake_consumer_.chips.count, 0u);
}

// Tests that tool execution emits a chip and timeline item.
TEST_F(ActuationWorklogMediatorTest, TestToolExecutionEmitsChipAndItem) {
  StartObservingTask(task_id_);
  [fake_consumer_.items removeAllObjects];

  // Empty updates are ignored.
  ExecuteTool(kType, @"");
  EXPECT_EQ(fake_consumer_.items.count, 0u);
  EXPECT_EQ(fake_consumer_.chips.count, 0u);

  ExecuteTool(kClick, @"Clicking button");
  ASSERT_EQ(fake_consumer_.items.count, 1u);
  EXPECT_NSEQ(fake_consumer_.items[0].title, @"Clicking button");
  ASSERT_EQ(fake_consumer_.chips.count, 1u);
  EXPECT_GT(fake_consumer_.chips[0].text.length, 0u);

  ExecuteTool(kNavigate, @"Navigating to page");
  ASSERT_EQ(fake_consumer_.items.count, 2u);
  EXPECT_NSEQ(fake_consumer_.items[1].title, @"Navigating to page");
  ASSERT_EQ(fake_consumer_.chips.count, 2u);
  EXPECT_GT(fake_consumer_.chips[1].text.length, 0u);

  // Unmapped tools emit the catch-all fallback chip.
  ExecuteTool(kSelect, @"Selecting item");
  ASSERT_EQ(fake_consumer_.items.count, 3u);
  EXPECT_NSEQ(fake_consumer_.items[2].title, @"Selecting item");
  ASSERT_EQ(fake_consumer_.chips.count, 3u);
  EXPECT_NSEQ(fake_consumer_.chips[2].text,
              l10n_util::GetNSString(IDS_IOS_ACTOR_WORKLOG_CHIP_DEFAULT));

  // Tools without chips (e.g. kWaitZeroDuration) do not emit a chip.
  ExecuteTool(kWaitZeroDuration, @"Stabilizing");
  ASSERT_EQ(fake_consumer_.items.count, 4u);
  EXPECT_NSEQ(fake_consumer_.items[3].title, @"Stabilizing");
  EXPECT_EQ(fake_consumer_.chips.count, 3u);
}

// Tests that consecutive duplicate task updates are deduplicated.
TEST_F(ActuationWorklogMediatorTest, TestConsecutiveDeduplication) {
  StartObservingTask(task_id_);
  [fake_consumer_.items removeAllObjects];

  ExecuteTool(kClick, @"Action A");
  ExecuteTool(kScroll, @"Action A");
  ExecuteTool(kType, @"Action B");
  ExecuteTool(kClick, @"Action A");

  ASSERT_EQ(fake_consumer_.items.count, 3u);
  EXPECT_NSEQ(fake_consumer_.items[0].title, @"Action A");
  EXPECT_NSEQ(fake_consumer_.items[1].title, @"Action B");
  EXPECT_NSEQ(fake_consumer_.items[2].title, @"Action A");
}

// Tests the different state transitions.
TEST_F(ActuationWorklogMediatorTest, TestTaskLifecycleTransitionsAndReset) {
  EXPECT_FALSE(fake_consumer_.actuationActive);

  StartObservingTask(task_id_);
  EXPECT_TRUE(fake_consumer_.actuationActive);
  ASSERT_EQ(fake_consumer_.items.count, 1u);
  ASSERT_NE(fake_consumer_.taskTitle, nil);

  ChangeState(kReflecting);
  EXPECT_TRUE(fake_consumer_.actuationActive);

  EndActuation();
  EXPECT_FALSE(fake_consumer_.actuationActive);
  EXPECT_EQ(fake_consumer_.items.count, 0u);
  EXPECT_EQ(fake_consumer_.taskTitle, nil);
}

// Tests that disconnecting the mediator resets the consumer.
TEST_F(ActuationWorklogMediatorTest, TestDisconnectResetsConsumer) {
  StartObservingTask(task_id_);
  ASSERT_EQ(fake_consumer_.items.count, 1u);
  ASSERT_NE(fake_consumer_.taskTitle, nil);

  [mediator_ disconnect];
  EXPECT_EQ(fake_consumer_.items.count, 0u);
  EXPECT_EQ(fake_consumer_.taskTitle, nil);
}

// Tests the user intervention flow.
TEST_F(ActuationWorklogMediatorTest, TestUserInterventionFlow) {
  StartObservingTask(task_id_);
  __block BOOL completion_called = NO;
  [mediator_ actorTask:task_id_
      requestUserInterventionWithTitle:@"Intervention Title"
                              subtitle:@"Intervention Subtitle"
                            buttonText:@"Continue"
                     completionHandler:^{
                       completion_called = YES;
                     }];

  ASSERT_NE(fake_consumer_.intervention, nil);
  EXPECT_NSEQ(fake_consumer_.intervention.title, @"Intervention Title");
  EXPECT_NSEQ(fake_consumer_.intervention.subtitle, @"Intervention Subtitle");
  EXPECT_NSEQ(fake_consumer_.intervention.primaryButtonText, @"Continue");
  EXPECT_FALSE(completion_called);

  [mediator_
      didTriggerInterventionAction:ActuationInterventionAction::kPrimary];
  EXPECT_TRUE(completion_called);
  EXPECT_EQ(fake_consumer_.intervention, nil);
}

// Tests that cancelling, replacing, or disconnecting an intervention discards
// the pending completion without invoking it.
TEST_F(ActuationWorklogMediatorTest, TestUserInterventionCancellation) {
  StartObservingTask(task_id_);
  __block BOOL first_completion_called = NO;
  [mediator_ actorTask:task_id_
      requestUserInterventionWithTitle:@"First Title"
                              subtitle:@"First Subtitle"
                            buttonText:@"Action 1"
                     completionHandler:^{
                       first_completion_called = YES;
                     }];

  ASSERT_NE(fake_consumer_.intervention, nil);
  EXPECT_FALSE(first_completion_called);

  // A second intervention arrives before user acts on the first.
  __block BOOL second_completion_called = NO;
  [mediator_ actorTask:task_id_
      requestUserInterventionWithTitle:@"Second Title"
                              subtitle:@"Second Subtitle"
                            buttonText:@"Action 2"
                     completionHandler:^{
                       second_completion_called = YES;
                     }];

  // First completion must be discarded without invocation.
  EXPECT_FALSE(first_completion_called);
  EXPECT_FALSE(second_completion_called);
  EXPECT_NSEQ(fake_consumer_.intervention.title, @"Second Title");

  // Task stoppage discards pending completion and clears consumer.
  EndActuation();
  EXPECT_FALSE(second_completion_called);
  EXPECT_EQ(fake_consumer_.intervention, nil);

  // A third intervention followed by disconnect should also discard completion.
  StartObservingTask(task_id_);
  __block BOOL third_completion_called = NO;
  [mediator_ actorTask:task_id_
      requestUserInterventionWithTitle:@"Third Title"
                              subtitle:@"Third Subtitle"
                            buttonText:@"Action 3"
                     completionHandler:^{
                       third_completion_called = YES;
                     }];
  ASSERT_NE(fake_consumer_.intervention, nil);
  [mediator_ disconnect];
  EXPECT_FALSE(third_completion_called);
  EXPECT_EQ(fake_consumer_.intervention, nil);
}

// Tests that starting to observe a task delivers its posted registration.
TEST_F(ActuationWorklogMediatorTest, TestStartObservingDeliversRegistration) {
  [mediator_ startObservingTaskWithID:task_id_];
  // The registration is posted, so nothing is shown yet.
  EXPECT_EQ(fake_consumer_.items.count, 0u);

  actor::FlushCurrentSequence();
  EXPECT_NSEQ(fake_consumer_.taskTitle, @(kTaskTitle));
  EXPECT_TRUE(fake_consumer_.actuationActive);
  EXPECT_EQ(fake_consumer_.items.count, 1u);
}

// Tests that stopping observation before the posted registration is delivered
// unregisters the mediator, so the registration never reaches the consumer.
TEST_F(ActuationWorklogMediatorTest,
       TestStopObservingBeforeRegistrationUnregistersObserver) {
  [mediator_ startObservingTaskWithID:task_id_];
  EndActuation();

  actor::FlushCurrentSequence();
  EXPECT_EQ(fake_consumer_.taskTitle, nil);
  EXPECT_FALSE(fake_consumer_.actuationActive);
  EXPECT_EQ(fake_consumer_.items.count, 0u);
}

// Tests that a confirmation interrupt is surfaced as an intervention card and
// that tapping its button resumes the task.
TEST_F(ActuationWorklogMediatorTest,
       TestConfirmationInterruptShowsInterventionCard) {
  CreateTaskAndRequestConfirmation();

  EXPECT_EQ(actor_service_->GetActiveTaskState(), kWaitingOnUser);
  ASSERT_NE(fake_consumer_.intervention, nil);
  EXPECT_NSEQ(fake_consumer_.intervention.title, @(kConfirmationMessage));

  [mediator_
      didTriggerInterventionAction:ActuationInterventionAction::kPrimary];
  EXPECT_EQ(fake_consumer_.intervention, nil);
  // The intervention completion is posted back to the task.
  EXPECT_EQ(actor_service_->GetActiveTaskState(), kWaitingOnUser);
  actor::FlushCurrentSequence();
  EXPECT_EQ(actor_service_->GetActiveTaskState(), kReflecting);
}

// Tests that disconnecting with a pending intervention leaves the task waiting
// on the user instead of stopping it.
TEST_F(ActuationWorklogMediatorTest, TestDisconnectKeepsTaskWaitingOnUser) {
  actor::ActorTaskId task_id = CreateTaskAndRequestConfirmation();
  ASSERT_NE(fake_consumer_.intervention, nil);
  FakeTaskStateObserver* observer = ObserveTaskState(task_id);

  [mediator_ disconnect];
  actor::FlushCurrentSequence();

  EXPECT_FALSE(observer.stopped);
  EXPECT_EQ(observer.state, kWaitingOnUser);
}

// Tests that disconnecting releases the intervention delegate, so a later
// confirmation interrupt is rejected instead of targeting a detached UI.
TEST_F(ActuationWorklogMediatorTest, TestDisconnectClearsInterventionDelegate) {
  actor::ActorTaskId task_id = CreateAndObserveTask();

  [mediator_ disconnect];
  EXPECT_FALSE(RequestConfirmation(task_id));
  EXPECT_EQ(fake_consumer_.intervention, nil);
}

// Tests that stopping observation, even before the posted observer
// registration arrives, detaches the mediator as the task's intervention
// delegate.
TEST_F(ActuationWorklogMediatorTest,
       TestStopObservingTaskClearsInterventionDelegate) {
  actor::ActorTaskId task_id = CreateAndObserveTask();

  [mediator_ stopObservingTask];
  EXPECT_FALSE(RequestConfirmation(task_id));
}

// Tests that observing another task moves the intervention delegate to it.
TEST_F(ActuationWorklogMediatorTest,
       TestStartObservingAnotherTaskMovesInterventionDelegate) {
  actor::ActorTaskId first_task_id = CreateAndObserveTask();
  actor::ActorTaskId second_task_id = CreateAndObserveTask();

  EXPECT_FALSE(RequestConfirmation(first_task_id));
  EXPECT_TRUE(RequestConfirmation(second_task_id));
  EXPECT_NE(fake_consumer_.intervention, nil);
}
