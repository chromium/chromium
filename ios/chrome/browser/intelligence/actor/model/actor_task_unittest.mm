// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_task.h"

#import "base/functional/callback_helpers.h"
#import "base/strings/string_number_conversions.h"
#import "base/token.h"
#import "base/types/strong_alias.h"
#import "components/actor/core/aggregated_journal.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_updates_observer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"
#import "ios/chrome/browser/intelligence/actor/util/actor_test_utils.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface FakeActorTaskUpdatesObserver : NSObject <ActorTaskUpdatesObserver>

@property(nonatomic, assign) BOOL didRegisterCalled;
@property(nonatomic, assign) actor::ActorTaskId registeredTaskId;
@property(nonatomic, copy) NSString* registeredTaskTitle;
@property(nonatomic, copy) NSString* registeredTaskUpdate;
@property(nonatomic, assign) actor::ActorTaskState registeredState;
@property(nonatomic, copy) NSArray<NSNumber*>* registeredWebStates;

@property(nonatomic, assign) BOOL didAddWebStateCalled;
@property(nonatomic, assign) web::WebStateID addedWebStateId;

@property(nonatomic, assign) BOOL didChangeStateCalled;
@property(nonatomic, assign) actor::ActorTaskState newState;
@property(nonatomic, assign) actor::ActorTaskState oldState;

@property(nonatomic, assign) BOOL willExecuteToolCalled;
@property(nonatomic, assign) actor::ToolType toolType;
@property(nonatomic, assign) web::WebStateID toolWebStateId;

@property(nonatomic, assign) BOOL didStopCalled;
@property(nonatomic, assign) actor::ActorTaskState finalState;

@end

@implementation FakeActorTaskUpdatesObserver

- (void)didRegisterAsObserverForTaskID:(actor::ActorTaskId)taskID
                             taskTitle:(NSString*)taskTitle
                            taskUpdate:(NSString*)taskUpdate
                          currentState:(actor::ActorTaskState)state
                             webStates:(NSArray<NSNumber*>*)webStatesIDs {
  _didRegisterCalled = YES;
  _registeredTaskId = taskID;
  _registeredTaskTitle = taskTitle;
  _registeredTaskUpdate = taskUpdate;
  _registeredState = state;
  _registeredWebStates = webStatesIDs;
}

- (void)actorTaskWithID:(actor::ActorTaskId)taskID
         didAddWebState:(web::WebStateID)webStateID {
  _didAddWebStateCalled = YES;
  _addedWebStateId = webStateID;
}

- (void)actorTaskWithID:(actor::ActorTaskId)taskID
         didChangeState:(actor::ActorTaskState)newState
              fromState:(actor::ActorTaskState)oldState {
  _didChangeStateCalled = YES;
  _newState = newState;
  _oldState = oldState;
}

- (void)actorTaskWithID:(actor::ActorTaskId)taskID
        willExecuteTool:(actor::ToolType)toolType
             taskUpdate:(NSString*)taskUpdate
             onWebState:(web::WebStateID)webStateID {
  _willExecuteToolCalled = YES;
  _toolType = toolType;
  _toolWebStateId = webStateID;
}

- (void)actorTaskDidStopWithID:(actor::ActorTaskId)taskID
                    finalState:(actor::ActorTaskState)finalState {
  _didStopCalled = YES;
  _finalState = finalState;
}

@end

@interface BarebonesActorTaskUpdatesObserver
    : NSObject <ActorTaskUpdatesObserver>
@end

@implementation BarebonesActorTaskUpdatesObserver
@end

@interface SelfRemovingActorTaskUpdatesObserver
    : NSObject <ActorTaskUpdatesObserver> {
  raw_ptr<actor::ActorTask> _task;
}
@property(nonatomic, assign) BOOL didChangeStateCalled;
- (instancetype)initWithTask:(actor::ActorTask*)task;
@end

@implementation SelfRemovingActorTaskUpdatesObserver

- (instancetype)initWithTask:(actor::ActorTask*)task {
  self = [super init];
  if (self) {
    _task = task;
  }
  return self;
}

- (void)actorTaskWithID:(actor::ActorTaskId)taskID
         didChangeState:(actor::ActorTaskState)newState
              fromState:(actor::ActorTaskState)oldState {
  _didChangeStateCalled = YES;
  if (_task) {
    _task->RemoveObserver(self);
  }
}

@end

namespace actor {

namespace {

// Returns all raw log entries in the journal for testing.
std::vector<mojom::JournalEntryPtr> GetLogsForTesting(
    AggregatedJournal* journal) {
  std::vector<mojom::JournalEntryPtr> result;
  for (AggregatedJournal::EntryBuffer::Iterator it = journal->Items(); it;
       ++it) {
    const std::unique_ptr<AggregatedJournal::Entry>* entry_ptr = *it;
    if (entry_ptr && *entry_ptr && (*entry_ptr)->data) {
      result.push_back((*entry_ptr)->data->Clone());
    }
  }
  return result;
}

}  // namespace

class ActorTaskTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    journal_ = std::make_unique<AggregatedJournal>();
    tool_factory_ = std::make_unique<ActorToolFactory>(profile_.get());
    task_ = std::make_unique<ActorTask>(ActorTaskId(1), "Test Task",
                                        /*allow_incognito_web_states=*/false,
                                        journal_.get(), tool_factory_.get());
  }

  void TearDown() override {
    task_.reset();
    tool_factory_.reset();
    journal_.reset();
    profile_.reset();
    PlatformTest::TearDown();
  }

  void AddControlledWebState(base::WeakPtr<web::WebState> web_state) {
    task_->AddControlledWebState(web_state.get());
  }

  const std::vector<base::WeakPtr<web::WebState>>& GetControlledWebStates()
      const {
    return task_->controlled_web_states_;
  }

  void SetTaskState(ActorTaskState state) { task_->SetState(state); }

  void TriggerOnWillExecuteTool(ToolType tool_type,
                                web::WebStateID web_state_id) {
    task_->OnWillExecuteTool(tool_type, web_state_id);
  }

  void TriggerOnPageLoadedTimeout() { task_->OnPageLoadedTimeout(); }

  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<AggregatedJournal> journal_;
  std::unique_ptr<ActorToolFactory> tool_factory_;
  std::unique_ptr<ActorTask> task_;
};

// Tests that the task correctly identifies if it is controlling a given
// WebState. It also verifies that it handles destroyed WebStates and null
// pointers gracefully.
TEST_F(ActorTaskTest, IsControllingWebState) {
  std::unique_ptr<web::FakeWebState> web_state1 =
      std::make_unique<web::FakeWebState>();

  std::unique_ptr<web::FakeWebState> web_state2 =
      std::make_unique<web::FakeWebState>();

  // web_state1 is added, simulate it being controlled.
  AddControlledWebState(web_state1->GetWeakPtr());

  EXPECT_TRUE(task_->IsControllingWebState(web_state1.get()));
  EXPECT_FALSE(task_->IsControllingWebState(web_state2.get()));

  // Test that when the web state is destroyed, it returns false instead of
  // crashing.
  web_state1.reset();

  // Create a third fake webstate to pass as parameter, just to ensure it safely
  // walks past the now-destroyed weak pointer.
  std::unique_ptr<web::FakeWebState> web_state3 =
      std::make_unique<web::FakeWebState>();

  EXPECT_FALSE(task_->IsControllingWebState(web_state3.get()));

  // Test that passing nullptr returns false gracefully.
  EXPECT_FALSE(task_->IsControllingWebState(nullptr));
}

// Tests that the getter for controlled WebStates returns the correct list of
// WebStates.
TEST_F(ActorTaskTest, ControlledWebStatesGetter) {
  std::unique_ptr<web::FakeWebState> web_state =
      std::make_unique<web::FakeWebState>();
  AddControlledWebState(web_state->GetWeakPtr());

  const auto& controlled_states = GetControlledWebStates();
  EXPECT_EQ(1u, controlled_states.size());
  EXPECT_EQ(web_state.get(), controlled_states[0].get());
}

// Tests that SetState updates the state and logs to the journal.
TEST_F(ActorTaskTest, SetState) {
  SetTaskState(ActorTaskState::kActing);

  EXPECT_EQ(ActorTaskState::kActing, task_->GetState());

  std::vector<mojom::JournalEntryPtr> logs = GetLogsForTesting(journal_.get());
  ASSERT_EQ(1u, logs.size());
  EXPECT_EQ("ActorTask::SetState", logs[0]->event);

  ASSERT_EQ(2u, logs[0]->details.size());
  EXPECT_EQ("current_state", logs[0]->details[0]->key);
  EXPECT_EQ("Init", logs[0]->details[0]->value);
  EXPECT_EQ("new_state", logs[0]->details[1]->key);
  EXPECT_EQ("Acting", logs[0]->details[1]->value);
}

// Tests that adding a new controlled web state notifies observers.
TEST_F(ActorTaskTest, AddControlledWebStateNotifiesObserver) {
  FakeActorTaskUpdatesObserver* observer =
      [[FakeActorTaskUpdatesObserver alloc] init];
  task_->AddObserver(observer);

  std::unique_ptr<web::FakeWebState> web_state =
      std::make_unique<web::FakeWebState>();

  observer.didAddWebStateCalled = NO;
  AddControlledWebState(web_state->GetWeakPtr());

  EXPECT_TRUE(observer.didAddWebStateCalled);
  EXPECT_EQ(web_state->GetUniqueIdentifier().identifier(),
            observer.addedWebStateId.identifier());
}

// Tests that AddControlledWebState correctly adds a WebState to the controlled
// list and notifies observers.
TEST_F(ActorTaskTest, AddControlledWebState) {
  FakeActorTaskUpdatesObserver* observer =
      [[FakeActorTaskUpdatesObserver alloc] init];
  task_->AddObserver(observer);

  std::unique_ptr<web::FakeWebState> web_state =
      std::make_unique<web::FakeWebState>();

  observer.didAddWebStateCalled = NO;
  task_->AddControlledWebState(web_state.get());

  EXPECT_TRUE(observer.didAddWebStateCalled);
  EXPECT_EQ(web_state->GetUniqueIdentifier().identifier(),
            observer.addedWebStateId.identifier());

  const auto& controlled_states = GetControlledWebStates();
  EXPECT_EQ(1u, controlled_states.size());
  EXPECT_EQ(web_state.get(), controlled_states[0].get());

  std::vector<mojom::JournalEntryPtr> logs = GetLogsForTesting(journal_.get());
  ASSERT_EQ(1u, logs.size());
  EXPECT_EQ("ActorTask::AddControlledWebState", logs[0]->event);
  ASSERT_EQ(1u, logs[0]->details.size());
  EXPECT_EQ("web_state_id", logs[0]->details[0]->key);
  EXPECT_EQ(base::NumberToString(web_state->GetUniqueIdentifier().identifier()),
            logs[0]->details[0]->value);

  // Test adding nullptr or duplicate.
  observer.didAddWebStateCalled = NO;
  task_->AddControlledWebState(nullptr);
  task_->AddControlledWebState(web_state.get());
  EXPECT_FALSE(observer.didAddWebStateCalled);
  EXPECT_EQ(1u, GetControlledWebStates().size());
  EXPECT_EQ(1u, GetLogsForTesting(journal_.get()).size());
}

// Tests that AddObserver registers the observer and immediately sends the
// current state and controlled web states.
TEST_F(ActorTaskTest, AddObserverTriggersImmediateSync) {
  std::unique_ptr<web::FakeWebState> web_state =
      std::make_unique<web::FakeWebState>();
  AddControlledWebState(web_state->GetWeakPtr());

  std::vector<std::unique_ptr<ActorToolRequest>> actions;
  actions.push_back(
      MakeSuccessfulActorToolRequest(web_state->GetUniqueIdentifier()));

  task_->Act(std::move(actions), "Performing some actions", base::DoNothing());

  FakeActorTaskUpdatesObserver* observer =
      [[FakeActorTaskUpdatesObserver alloc] init];
  EXPECT_FALSE(observer.didRegisterCalled);

  task_->AddObserver(observer);

  EXPECT_TRUE(observer.didRegisterCalled);
  EXPECT_EQ(task_->GetState(), observer.registeredState);
  EXPECT_EQ(ActorTaskId(1), observer.registeredTaskId);
  EXPECT_NSEQ(@"Test Task", observer.registeredTaskTitle);
  EXPECT_NSEQ(@"Performing some actions", observer.registeredTaskUpdate);
  ASSERT_EQ(1u, observer.registeredWebStates.count);
  EXPECT_NSEQ(@(web_state->GetUniqueIdentifier().identifier()),
              observer.registeredWebStates[0]);
}

// Tests that registering an observer before `Act` is ever called (no task
// update cached) works successfully and provides an empty update string.
TEST_F(ActorTaskTest, AddObserverBeforeActHasEmptyUpdate) {
  FakeActorTaskUpdatesObserver* observer =
      [[FakeActorTaskUpdatesObserver alloc] init];
  EXPECT_FALSE(observer.didRegisterCalled);

  task_->AddObserver(observer);

  EXPECT_TRUE(observer.didRegisterCalled);
  EXPECT_NSEQ(@"", observer.registeredTaskUpdate);
}

// Tests that SetState notifies the observer.
TEST_F(ActorTaskTest, SetStateNotifiesObserver) {
  FakeActorTaskUpdatesObserver* observer =
      [[FakeActorTaskUpdatesObserver alloc] init];
  task_->AddObserver(observer);

  observer.didChangeStateCalled = NO;
  SetTaskState(ActorTaskState::kActing);

  EXPECT_TRUE(observer.didChangeStateCalled);
  EXPECT_EQ(ActorTaskState::kActing, observer.newState);
  EXPECT_EQ(ActorTaskState::kInit, observer.oldState);
}

// Tests that RemoveObserver stops updates.
TEST_F(ActorTaskTest, RemoveObserverStopsUpdates) {
  FakeActorTaskUpdatesObserver* observer =
      [[FakeActorTaskUpdatesObserver alloc] init];
  task_->AddObserver(observer);

  task_->RemoveObserver(observer);

  observer.didChangeStateCalled = NO;
  SetTaskState(ActorTaskState::kActing);

  EXPECT_FALSE(observer.didChangeStateCalled);
}

// Tests that OnWillExecuteTool propagates execution updates to registered
// observers, covering both mapped tools and fallback unmapped tools.
TEST_F(ActorTaskTest, OnWillExecuteToolNotifiesObserver) {
  std::unique_ptr<web::FakeWebState> web_state =
      std::make_unique<web::FakeWebState>();
  AddControlledWebState(web_state->GetWeakPtr());

  std::vector<std::unique_ptr<ActorToolRequest>> actions;
  actions.push_back(
      MakeSuccessfulActorToolRequest(web_state->GetUniqueIdentifier()));

  task_->Act(std::move(actions), "Acting update", base::DoNothing());

  FakeActorTaskUpdatesObserver* observer =
      [[FakeActorTaskUpdatesObserver alloc] init];
  task_->AddObserver(observer);

  // 1. Test a successfully mapped tool execution.
  observer.willExecuteToolCalled = NO;
  TriggerOnWillExecuteTool(ToolType::kNavigate,
                           web_state->GetUniqueIdentifier());

  EXPECT_TRUE(observer.willExecuteToolCalled);
  EXPECT_EQ(ToolType::kNavigate, observer.toolType);
  EXPECT_EQ(web_state->GetUniqueIdentifier().identifier(),
            observer.toolWebStateId.identifier());

  // 2. Test an unmapped/fallback tool execution.
  observer.willExecuteToolCalled = NO;
  TriggerOnWillExecuteTool(ToolType::kUnknown,
                           web_state->GetUniqueIdentifier());

  EXPECT_TRUE(observer.willExecuteToolCalled);
  EXPECT_EQ(ToolType::kUnknown, observer.toolType);
  EXPECT_EQ(web_state->GetUniqueIdentifier().identifier(),
            observer.toolWebStateId.identifier());
}

// Tests that multiple observers are notified successfully.
TEST_F(ActorTaskTest, MultipleObserversNotified) {
  FakeActorTaskUpdatesObserver* observer1 =
      [[FakeActorTaskUpdatesObserver alloc] init];
  FakeActorTaskUpdatesObserver* observer2 =
      [[FakeActorTaskUpdatesObserver alloc] init];

  task_->AddObserver(observer1);
  task_->AddObserver(observer2);

  observer1.didChangeStateCalled = NO;
  observer2.didChangeStateCalled = NO;

  SetTaskState(ActorTaskState::kActing);

  EXPECT_TRUE(observer1.didChangeStateCalled);
  EXPECT_TRUE(observer2.didChangeStateCalled);
}

// Tests that registering a new observer only fires didRegister on that new
// observer, and does not re-notify already registered observers.
TEST_F(ActorTaskTest, NewObserverRegistrationIsIsolated) {
  FakeActorTaskUpdatesObserver* observer1 =
      [[FakeActorTaskUpdatesObserver alloc] init];
  task_->AddObserver(observer1);
  EXPECT_TRUE(observer1.didRegisterCalled);

  observer1.didRegisterCalled = NO;

  FakeActorTaskUpdatesObserver* observer2 =
      [[FakeActorTaskUpdatesObserver alloc] init];
  task_->AddObserver(observer2);

  EXPECT_TRUE(observer2.didRegisterCalled);
  EXPECT_FALSE(observer1.didRegisterCalled);
}

// Tests that calling Act multiple times updates the cached task update blurb
// and that subsequent observer registrations receive the latest cached update.
TEST_F(ActorTaskTest, CachesLatestTaskUpdateAcrossActs) {
  std::unique_ptr<web::FakeWebState> web_state =
      std::make_unique<web::FakeWebState>();
  AddControlledWebState(web_state->GetWeakPtr());

  std::vector<std::unique_ptr<ActorToolRequest>> actions_1;
  actions_1.push_back(
      MakeSuccessfulActorToolRequest(web_state->GetUniqueIdentifier()));
  task_->Act(std::move(actions_1), "First Update", base::DoNothing());

  FakeActorTaskUpdatesObserver* observer1 =
      [[FakeActorTaskUpdatesObserver alloc] init];
  task_->AddObserver(observer1);
  EXPECT_NSEQ(@"First Update", observer1.registeredTaskUpdate);

  std::vector<std::unique_ptr<ActorToolRequest>> actions_2;
  actions_2.push_back(
      MakeSuccessfulActorToolRequest(web_state->GetUniqueIdentifier()));
  task_->Act(std::move(actions_2), "Second Update", base::DoNothing());

  FakeActorTaskUpdatesObserver* observer2 =
      [[FakeActorTaskUpdatesObserver alloc] init];
  task_->AddObserver(observer2);
  EXPECT_NSEQ(@"Second Update", observer2.registeredTaskUpdate);
}

// Tests that optional protocol methods are safely ignored for observers that
// do not implement them.
TEST_F(ActorTaskTest, OptionalMethodsGracefullyIgnored) {
  BarebonesActorTaskUpdatesObserver* observer =
      [[BarebonesActorTaskUpdatesObserver alloc] init];

  task_->AddObserver(observer);

  EXPECT_NO_FATAL_FAILURE({ SetTaskState(ActorTaskState::kActing); });

  EXPECT_NO_FATAL_FAILURE({
    TriggerOnWillExecuteTool(ToolType::kNavigate,
                             web::WebStateID::FromSerializedValue(123));
  });

  task_->RemoveObserver(observer);
}

// Tests that an observer can safely unregister itself from within an active
// notification callback without causing reentrancy crashes or undefined
// behavior.
TEST_F(ActorTaskTest, SafeSelfRemovalDuringNotification) {
  // Autorelease pool forces the observer (and its raw_ptr member) to deallocate
  // before `TearDown` destroys `task_`, preventing dangling raw_ptr errors.
  @autoreleasepool {
    SelfRemovingActorTaskUpdatesObserver* observer =
        [[SelfRemovingActorTaskUpdatesObserver alloc] initWithTask:task_.get()];

    task_->AddObserver(observer);
    EXPECT_FALSE(observer.didChangeStateCalled);

    EXPECT_NO_FATAL_FAILURE({ SetTaskState(ActorTaskState::kActing); });

    EXPECT_TRUE(observer.didChangeStateCalled);

    observer.didChangeStateCalled = NO;
    SetTaskState(ActorTaskState::kFinished);
    EXPECT_FALSE(observer.didChangeStateCalled);
  }
}

// Test that successful execution of Act transitions the state to reflecting
// before the Act completion callback is executed.
TEST_F(ActorTaskTest, StateTransitionsToReflectingBeforeCallback) {
  std::unique_ptr<web::FakeWebState> web_state =
      std::make_unique<web::FakeWebState>();
  AddControlledWebState(web_state->GetWeakPtr());

  std::vector<std::unique_ptr<ActorToolRequest>> actions;
  actions.push_back(
      MakeSuccessfulActorToolRequest(web_state->GetUniqueIdentifier()));

  bool callback_executed = false;
  ActorTaskState state_in_callback = ActorTaskState::kInit;

  task_->Act(
      std::move(actions), "Performing actions",
      base::BindOnce(
          [](bool* executed, ActorTaskState* state, const ActorTask* task,
             std::vector<ActionResult> results) {
            *executed = true;
            *state = task->GetState();
          },
          base::Unretained(&callback_executed),
          base::Unretained(&state_in_callback), base::Unretained(task_.get())));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(callback_executed);
  EXPECT_EQ(ActorTaskState::kReflecting, state_in_callback);
}

// Test that deferred execution of Act transitions the state to reflecting
// before the Act completion callback is executed when page loading completes.
TEST_F(ActorTaskTest, StateTransitionsToReflectingBeforeDeferredCallback) {
  std::unique_ptr<web::FakeWebState> web_state =
      std::make_unique<web::FakeWebState>();
  web_state->SetLoading(true);
  AddControlledWebState(web_state->GetWeakPtr());

  std::vector<std::unique_ptr<ActorToolRequest>> actions;
  actions.push_back(
      MakeSuccessfulActorToolRequest(web_state->GetUniqueIdentifier()));

  bool callback_executed = false;
  ActorTaskState state_in_callback = ActorTaskState::kInit;

  task_->Act(
      std::move(actions), "Performing actions on loading state",
      base::BindOnce(
          [](bool* executed, ActorTaskState* state, const ActorTask* task,
             std::vector<ActionResult> results) {
            *executed = true;
            *state = task->GetState();
          },
          base::Unretained(&callback_executed),
          base::Unretained(&state_in_callback), base::Unretained(task_.get())));

  EXPECT_FALSE(callback_executed);

  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_FALSE(callback_executed);

  // Stop loading to trigger the deferred callback.
  web_state->SetLoading(false);

  EXPECT_TRUE(callback_executed);
  EXPECT_EQ(ActorTaskState::kReflecting, state_in_callback);
}

// Test that deferred execution of Act transitions the state to reflecting
// before the Act completion callback is executed when page loading times out.
TEST_F(ActorTaskTest, StateTransitionsToReflectingBeforeTimeoutCallback) {
  std::unique_ptr<web::FakeWebState> web_state =
      std::make_unique<web::FakeWebState>();
  web_state->SetLoading(true);
  AddControlledWebState(web_state->GetWeakPtr());

  std::vector<std::unique_ptr<ActorToolRequest>> actions;
  actions.push_back(
      MakeSuccessfulActorToolRequest(web_state->GetUniqueIdentifier()));

  bool callback_executed = false;
  ActorTaskState state_in_callback = ActorTaskState::kInit;

  task_->Act(
      std::move(actions), "Performing actions on loading state",
      base::BindOnce(
          [](bool* executed, ActorTaskState* state, const ActorTask* task,
             std::vector<ActionResult> results) {
            *executed = true;
            *state = task->GetState();
          },
          base::Unretained(&callback_executed),
          base::Unretained(&state_in_callback), base::Unretained(task_.get())));

  EXPECT_FALSE(callback_executed);

  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_FALSE(callback_executed);

  // Directly trigger the page load timeout callback.
  TriggerOnPageLoadedTimeout();

  EXPECT_TRUE(callback_executed);
  EXPECT_EQ(ActorTaskState::kReflecting, state_in_callback);
}

// Test that calling `Stop()` on the task stops actuation on controlled
// `WebState`s.
TEST_F(ActorTaskTest, StopSetsActuatingStateToFalse) {
  auto web_state = std::make_unique<web::FakeWebState>();
  ActorTabHelper::CreateForWebState(web_state.get());
  ActorTabHelper* helper = ActorTabHelper::FromWebState(web_state.get());
  ASSERT_NE(helper, nullptr);
  EXPECT_FALSE(helper->IsActuating());

  SetTaskState(ActorTaskState::kActing);
  AddControlledWebState(web_state->GetWeakPtr());
  EXPECT_TRUE(helper->IsActuating());

  task_->Stop(ActorTaskStoppedReason::kTaskComplete);
  EXPECT_FALSE(helper->IsActuating());
}

// Test that adding a controlled `WebState` with `ActorTabHelper` sets actuating
// to true, and destroying the task stops actuation without removing the helper.
TEST_F(ActorTaskTest, ActorTabHelperActuatingState) {
  auto web_state = std::make_unique<web::FakeWebState>();
  ActorTabHelper::CreateForWebState(web_state.get());
  ActorTabHelper* helper = ActorTabHelper::FromWebState(web_state.get());
  ASSERT_NE(helper, nullptr);
  EXPECT_FALSE(helper->IsActuating());

  SetTaskState(ActorTaskState::kActing);
  AddControlledWebState(web_state->GetWeakPtr());
  EXPECT_TRUE(helper->IsActuating());

  task_.reset();
  EXPECT_NE(ActorTabHelper::FromWebState(web_state.get()), nullptr);
  EXPECT_FALSE(helper->IsActuating());
}

}  // namespace actor
