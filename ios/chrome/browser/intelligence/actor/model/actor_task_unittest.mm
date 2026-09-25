// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_task.h"

#import "base/functional/callback_helpers.h"
#import "base/strings/string_number_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "base/values.h"
#import "components/actor/core/aggregated_journal.h"
#import "components/actor/core/safety_list_manager.h"
#import "components/origin_gating/core/origin_gating_checker.h"
#import "components/origin_gating/core/origin_gating_configuration.h"
#import "components/origin_gating/core/origin_gating_registration.h"
#import "ios/chrome/app/background_mode_buildflags.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_browser_agent.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_web_state_policy_decider.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_updates_observer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"
#import "ios/chrome/browser/intelligence/actor/util/actor_test_utils.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/origin_gating/model/origin_gating_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
#import "ios/chrome/app/background_task/background_continued_processing_task_configuration.h"  // nogncheck
#import "ios/chrome/app/background_task/background_continued_processing_task_context.h"  // nogncheck
#import "ios/chrome/app/background_task/features.h"  // nogncheck
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#endif

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

#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
@interface TestBackgroundContinuedProcessingTaskContext
    : BackgroundContinuedProcessingTaskContext
@property(nonatomic, assign) NSInteger subtitleUpdateCount;
@end

@implementation TestBackgroundContinuedProcessingTaskContext

- (void)setSubtitle:(NSString*)subtitle {
  _subtitleUpdateCount++;
  [super setSubtitle:subtitle];
}

@end
#endif  // BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)

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

// A FakeWebState subclass that records whether SetKeepRenderProcessAlive was
// called.
class TestKeepAliveWebState : public web::FakeWebState {
 public:
  void SetKeepRenderProcessAlive(bool keep_alive) override {
    keep_render_process_alive_ = keep_alive;
  }
  bool keep_render_process_alive() const { return keep_render_process_alive_; }

 private:
  bool keep_render_process_alive_ = false;
};

}  // namespace

class ActorTaskTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    journal_ = std::make_unique<AggregatedJournal>();
    tool_factory_ = std::make_unique<ActorToolFactory>(profile_.get());

    task_ = std::make_unique<ActorTask>(
        ActorTaskId(1), "Test Task",
        /*allow_incognito_web_states=*/false, journal_.get(),
        tool_factory_.get(), BrowserListFactory::GetForProfile(profile_.get()));
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

#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
  web::FakeWebFrame* AttachMainWebFrame(web::FakeWebState* web_state) {
    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    auto main_frame = web::FakeWebFrame::CreateMainWebFrame();
    web::FakeWebFrame* main_frame_ptr = main_frame.get();
    frames_manager->AddWebFrame(std::move(main_frame));
    web_state->SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                   std::move(frames_manager));
    return main_frame_ptr;
  }

  void TriggerOnActCompleted(ActCallback callback,
                             std::vector<ActionResult> results) {
    task_->OnActCompleted(std::move(callback), std::move(results));
  }

  bool IsHeartbeatTimerRunning() const {
    return task_->heartbeat_timer_.IsRunning();
  }
#endif  // BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)

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
// when non-empty, preserves the cached blurb when given an empty string, and
// provides the latest cached update to subsequent observer registrations.
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

  // An empty task update should not overwrite the previously cached update.
  std::vector<std::unique_ptr<ActorToolRequest>> actions_empty;
  actions_empty.push_back(
      MakeSuccessfulActorToolRequest(web_state->GetUniqueIdentifier()));
  task_->Act(std::move(actions_empty), "", base::DoNothing());

  FakeActorTaskUpdatesObserver* observer_empty =
      [[FakeActorTaskUpdatesObserver alloc] init];
  task_->AddObserver(observer_empty);
  EXPECT_NSEQ(@"First Update", observer_empty.registeredTaskUpdate);

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

// Test that destroying a controlled `WebState` stops actuation on its
// `ActorTabHelper` before removal.
TEST_F(ActorTaskTest, WebStateDestroyedResetsActorTabHelperActuatingState) {
  auto web_state = std::make_unique<web::FakeWebState>();
  ActorTabHelper::CreateForWebState(web_state.get());
  ActorTabHelper* helper = ActorTabHelper::FromWebState(web_state.get());
  ASSERT_NE(helper, nullptr);

  SetTaskState(ActorTaskState::kActing);
  AddControlledWebState(web_state->GetWeakPtr());
  EXPECT_TRUE(helper->IsActuating());

  task_->WebStateDestroyed(web_state.get());
  EXPECT_FALSE(helper->IsActuating());
}

TEST_F(ActorTaskTest, WindowIdAndInsertWebState) {
  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());

  // Create a regular browser and register it.
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list->AddBrowser(browser.get());
  ActorBrowserAgent::CreateForBrowser(browser.get());
  TabInsertionBrowserAgent::CreateForBrowser(browser.get());
  ActorBrowserAgent* agent = ActorBrowserAgent::FromBrowser(browser.get());
  int32_t window_id = agent->browser_id().id();

  // Test that we can validate the window ID.
  ToolDelegate* tool_delegate = &task_->engine();
  EXPECT_TRUE(tool_delegate->IsWindowIdValid(window_id));
  EXPECT_FALSE(tool_delegate->IsWindowIdValid(999));

  // Test that we can insert a WebState.
  EXPECT_EQ(0, browser->GetWebStateList()->count());
  web::NavigationManager::WebLoadParams load_params(GURL("chrome://newtab"));

  web::WebState* web_state = tool_delegate->InsertWebState(
      window_id, load_params, false /* in_background */);
  ASSERT_NE(nullptr, web_state);
  EXPECT_EQ(1, browser->GetWebStateList()->count());
  EXPECT_EQ(web_state, browser->GetWebStateList()->GetWebStateAt(0));

  // Test that inserting WebState with invalid window ID returns nullptr.
  EXPECT_EQ(nullptr, tool_delegate->InsertWebState(999, load_params,
                                                   false /* in_background */));

  // Test that inserting WebState when TabInsertionBrowserAgent is missing
  // returns nullptr.
  auto browser_no_agent = std::make_unique<TestBrowser>(profile_.get());
  browser_list->AddBrowser(browser_no_agent.get());
  ActorBrowserAgent::CreateForBrowser(browser_no_agent.get());
  int32_t window_id_no_agent =
      ActorBrowserAgent::FromBrowser(browser_no_agent.get())->browser_id().id();
  EXPECT_EQ(nullptr,
            tool_delegate->InsertWebState(window_id_no_agent, load_params,
                                          false /* in_background */));
}

// Tests that InsertWebState places the new tab immediately next to the
// prompting tab (which is the first controlled WebState of the task).
TEST_F(ActorTaskTest, InsertWebState_AdjacentPlacement) {
  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());
  auto browser = std::make_unique<TestBrowser>(profile_.get());
  browser_list->AddBrowser(browser.get());
  ActorBrowserAgent::CreateForBrowser(browser.get());
  TabInsertionBrowserAgent::CreateForBrowser(browser.get());
  int32_t window_id =
      ActorBrowserAgent::FromBrowser(browser.get())->browser_id().id();

  ToolDelegate* tool_delegate = &task_->engine();

  // 1. Insert WebState A (controlled) at index 0.
  auto web_state_a = std::make_unique<web::FakeWebState>();
  web::WebState* a_ptr = web_state_a.get();
  browser->GetWebStateList()->InsertWebState(std::move(web_state_a));
  AddControlledWebState(a_ptr->GetWeakPtr());

  // 2. Insert WebState B (uncontrolled) at index 1.
  auto web_state_b = std::make_unique<web::FakeWebState>();
  web::WebState* b_ptr = web_state_b.get();
  browser->GetWebStateList()->InsertWebState(std::move(web_state_b));

  EXPECT_EQ(2, browser->GetWebStateList()->count());
  EXPECT_EQ(a_ptr, browser->GetWebStateList()->GetWebStateAt(0));
  EXPECT_EQ(b_ptr, browser->GetWebStateList()->GetWebStateAt(1));

  // 3. Insert WebState C via InsertWebState.
  web::NavigationManager::WebLoadParams load_params(GURL("chrome://newtab"));
  web::WebState* web_state_c = tool_delegate->InsertWebState(
      window_id, load_params, false /* in_background */);
  ASSERT_NE(nullptr, web_state_c);

  // The new tab C should be placed next to A (the prompting tab) at index 1.
  EXPECT_EQ(3, browser->GetWebStateList()->count());
  EXPECT_EQ(a_ptr, browser->GetWebStateList()->GetWebStateAt(0));
  EXPECT_EQ(web_state_c, browser->GetWebStateList()->GetWebStateAt(1));
  EXPECT_EQ(b_ptr, browser->GetWebStateList()->GetWebStateAt(2));
}

// Test that `SetKeepRenderProcessAlive` is enabled for all controlled
// `WebState`s for the entire duration of the task and is reset when the task
// stops or is destroyed.
TEST_F(ActorTaskTest, SetKeepRenderProcessAliveOnControlledWebStates) {
  auto web_state1 = std::make_unique<TestKeepAliveWebState>();
  auto web_state2 = std::make_unique<TestKeepAliveWebState>();

  EXPECT_FALSE(web_state1->keep_render_process_alive());
  EXPECT_FALSE(web_state2->keep_render_process_alive());

  // Add `web_state1` while task is in `kInit`. It should immediately be marked
  // keep-alive for the task duration.
  AddControlledWebState(web_state1->GetWeakPtr());
  EXPECT_TRUE(web_state1->keep_render_process_alive());

  // Transition to `kActing`.
  SetTaskState(ActorTaskState::kActing);
  EXPECT_TRUE(web_state1->keep_render_process_alive());

  // Add `web_state2` while in `kActing`. It should also be marked keep-alive.
  AddControlledWebState(web_state2->GetWeakPtr());
  EXPECT_TRUE(web_state2->keep_render_process_alive());

  // Transition across non-actuating states (`kPausedByUser`, `kReflecting`,
  // `kWaitingOnUser`). Keep-alive should persist for the task duration.
  SetTaskState(ActorTaskState::kPausedByUser);
  EXPECT_TRUE(web_state1->keep_render_process_alive());
  EXPECT_TRUE(web_state2->keep_render_process_alive());

  SetTaskState(ActorTaskState::kReflecting);
  EXPECT_TRUE(web_state1->keep_render_process_alive());
  EXPECT_TRUE(web_state2->keep_render_process_alive());

  SetTaskState(ActorTaskState::kWaitingOnUser);
  EXPECT_TRUE(web_state1->keep_render_process_alive());
  EXPECT_TRUE(web_state2->keep_render_process_alive());

  // Stopping the task should reset keep-alive to false.
  task_->Stop(ActorTaskStoppedReason::kTaskComplete);
  EXPECT_FALSE(web_state1->keep_render_process_alive());
  EXPECT_FALSE(web_state2->keep_render_process_alive());

  // Verify that destroying an active task also resets keep-alive.
  auto web_state3 = std::make_unique<TestKeepAliveWebState>();
  auto scoped_task = std::make_unique<ActorTask>(
      ActorTaskId(42), "Scoped Task",
      /*allow_incognito_web_states=*/false, journal_.get(), tool_factory_.get(),
      BrowserListFactory::GetForProfile(profile_.get()));
  scoped_task->AddControlledWebState(web_state3.get());
  EXPECT_TRUE(web_state3->keep_render_process_alive());

  scoped_task.reset();
  EXPECT_FALSE(web_state3->keep_render_process_alive());
}

#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)

// Test that executing a tool increments background task progress, and
// completing the task completes progress.
TEST_F(ActorTaskTest, BackgroundTaskProgressIncrementsOnToolExecution) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kActorTools, kGeminiActor,
       kEnableBackgroundContinuedProcessing},
      {});

  BackgroundContinuedProcessingTaskConfiguration* config =
      [[BackgroundContinuedProcessingTaskConfiguration alloc]
              initWithTitle:@"Test Task"
                   subtitle:@""
          expirationHandler:^{
          }];
  BackgroundContinuedProcessingTaskContext* context =
      [[BackgroundContinuedProcessingTaskContext alloc]
          initWithTaskIdentifier:@"org.chromium.test.task"
                   configuration:config
                   finishHandler:nil];

  task_->SetBackgroundTaskContext(context);

  EXPECT_DOUBLE_EQ(context.fractionCompleted, 0.0);

  TriggerOnWillExecuteTool(ToolType::kClick,
                           web::WebStateID::FromSerializedValue(1));
  EXPECT_GT(context.completedUnits, 0);

  // Completing the task hits 100% and completes the context.
  task_->Stop(ActorTaskStoppedReason::kTaskComplete);
  EXPECT_DOUBLE_EQ(context.fractionCompleted, 1.0);
  EXPECT_TRUE(context.completed);
}

// Test that stopping an ActorTask with `kStoppedByUser` finalizes the
// background task with success (100% progress).
TEST_F(ActorTaskTest, BackgroundTaskStoppedByUser) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kActorTools, kGeminiActor,
       kEnableBackgroundContinuedProcessing},
      {});

  BackgroundContinuedProcessingTaskConfiguration* config =
      [[BackgroundContinuedProcessingTaskConfiguration alloc]
              initWithTitle:@"Test Task"
                   subtitle:@""
          expirationHandler:^{
          }];
  BackgroundContinuedProcessingTaskContext* context =
      [[BackgroundContinuedProcessingTaskContext alloc]
          initWithTaskIdentifier:@"org.chromium.test.task"
                   configuration:config
                   finishHandler:nil];

  task_->SetBackgroundTaskContext(context);
  TriggerOnWillExecuteTool(ToolType::kClick,
                           web::WebStateID::FromSerializedValue(1));
  EXPECT_LT(context.fractionCompleted, 1.0);

  task_->Stop(ActorTaskStoppedReason::kStoppedByUser);

  EXPECT_DOUBLE_EQ(context.fractionCompleted, 1.0);
  EXPECT_TRUE(context.completed);
}

// Test that `Act()` updates the background task context subtitle and ignores
// empty or duplicate task updates.
TEST_F(ActorTaskTest, BackgroundTaskSubtitleUpdate) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kActorTools, kGeminiActor,
       kEnableBackgroundContinuedProcessing},
      {});

  BackgroundContinuedProcessingTaskConfiguration* config =
      [[BackgroundContinuedProcessingTaskConfiguration alloc]
              initWithTitle:@"Test Task"
                   subtitle:@""
          expirationHandler:^{
          }];
  TestBackgroundContinuedProcessingTaskContext* context =
      [[TestBackgroundContinuedProcessingTaskContext alloc]
          initWithTaskIdentifier:@"org.chromium.test.task"
                   configuration:config
                   finishHandler:nil];

  task_->SetBackgroundTaskContext(context);
  EXPECT_EQ(context.subtitleUpdateCount, 0);

  // Empty update should not send a subtitle update.
  task_->Act({}, "", base::DoNothing());
  EXPECT_EQ(context.subtitleUpdateCount, 0);

  // First non-empty update should update the subtitle.
  task_->Act({}, "Foo", base::DoNothing());
  EXPECT_NSEQ(context.subtitle, @"Foo");
  EXPECT_EQ(context.subtitleUpdateCount, 1);

  // Identical update should not send a duplicate subtitle update.
  task_->Act({}, "Foo", base::DoNothing());
  EXPECT_NSEQ(context.subtitle, @"Foo");
  EXPECT_EQ(context.subtitleUpdateCount, 1);

  // Empty update after a valid update should not clear the subtitle or send an
  // update.
  task_->Act({}, "", base::DoNothing());
  EXPECT_NSEQ(context.subtitle, @"Foo");
  EXPECT_EQ(context.subtitleUpdateCount, 1);

  // Identical update after an empty update should still be deduplicated.
  task_->Act({}, "Foo", base::DoNothing());
  EXPECT_NSEQ(context.subtitle, @"Foo");
  EXPECT_EQ(context.subtitleUpdateCount, 1);

  // Distinct non-empty update should update the subtitle.
  task_->Act({}, "Bar", base::DoNothing());
  EXPECT_NSEQ(context.subtitle, @"Bar");
  EXPECT_EQ(context.subtitleUpdateCount, 2);
}

// Test that registering a background task context via
// `SetBackgroundTaskContext()` applies the latest cached non-empty task update.
TEST_F(ActorTaskTest, BackgroundTaskContextUsesCachedTaskUpdateOnRegistration) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kActorTools, kGeminiActor,
       kEnableBackgroundContinuedProcessing},
      {});

  // Execute an action with a non-empty update, followed by one with an empty
  // update before the background task context is attached.
  task_->Act({}, "Foo", base::DoNothing());
  task_->Act({}, "", base::DoNothing());

  BackgroundContinuedProcessingTaskConfiguration* config =
      [[BackgroundContinuedProcessingTaskConfiguration alloc]
              initWithTitle:@"Test Task"
                   subtitle:@""
          expirationHandler:^{
          }];
  TestBackgroundContinuedProcessingTaskContext* context =
      [[TestBackgroundContinuedProcessingTaskContext alloc]
          initWithTaskIdentifier:@"org.chromium.test.task"
                   configuration:config
                   finishHandler:nil];

  task_->SetBackgroundTaskContext(context);
  EXPECT_NSEQ(context.subtitle, @"Foo");
  EXPECT_EQ(context.subtitleUpdateCount, 1);
}

// Test that stopping an ActorTask with `kShutdown` finalizes the background
// task with failure (does not reach 100%).
TEST_F(ActorTaskTest, BackgroundTaskStoppedWithShutdown) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kActorTools, kGeminiActor,
       kEnableBackgroundContinuedProcessing},
      {});

  BackgroundContinuedProcessingTaskConfiguration* config =
      [[BackgroundContinuedProcessingTaskConfiguration alloc]
              initWithTitle:@"Test Task"
                   subtitle:@""
          expirationHandler:^{
          }];
  BackgroundContinuedProcessingTaskContext* context =
      [[BackgroundContinuedProcessingTaskContext alloc]
          initWithTaskIdentifier:@"org.chromium.test.task"
                   configuration:config
                   finishHandler:nil];

  task_->SetBackgroundTaskContext(context);

  FakeActorTaskUpdatesObserver* observer =
      [[FakeActorTaskUpdatesObserver alloc] init];
  task_->AddObserver(observer);

  task_->Stop(ActorTaskStoppedReason::kShutdown);

  EXPECT_TRUE(observer.didStopCalled);
  EXPECT_EQ(observer.finalState, ActorTaskState::kInit);
  EXPECT_TRUE(context.completed);
  EXPECT_DOUBLE_EQ(context.fractionCompleted, 0.0);
}

// Test that destroying `ActorTask` finalizes the background task.
TEST_F(ActorTaskTest, DestructorFinalizesBackgroundTask) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kActorTools, kGeminiActor,
       kEnableBackgroundContinuedProcessing},
      {});

  BackgroundContinuedProcessingTaskConfiguration* config =
      [[BackgroundContinuedProcessingTaskConfiguration alloc]
              initWithTitle:@"Test Task"
                   subtitle:@""
          expirationHandler:^{
          }];
  BackgroundContinuedProcessingTaskContext* context =
      [[BackgroundContinuedProcessingTaskContext alloc]
          initWithTaskIdentifier:@"org.chromium.test.task"
                   configuration:config
                   finishHandler:nil];

  auto task = std::make_unique<ActorTask>(
      ActorTaskId(1), "Test Task",
      /*allow_incognito_web_states=*/false, journal_.get(), tool_factory_.get(),
      BrowserListFactory::GetForProfile(profile_.get()));
  task->SetBackgroundTaskContext(context);
  EXPECT_FALSE(context.completed);

  task.reset();

  EXPECT_TRUE(context.completed);
}

// Test that adding a controlled WebState starts the 400ms heartbeat timer
// immediately to keep WebContent processes alive, and sends periodic JavaScript
// pings.
TEST_F(ActorTaskTest, HeartbeatStartsOnActAndPings) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kActorTools, kGeminiActor,
       kEnableBackgroundContinuedProcessing},
      {});

  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebFrame* main_frame = AttachMainWebFrame(web_state.get());
  ASSERT_TRUE(main_frame);

  EXPECT_FALSE(IsHeartbeatTimerRunning());

  AddControlledWebState(web_state->GetWeakPtr());
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  task_->Act({}, "Starting actuation", base::DoNothing());
  EXPECT_TRUE(IsHeartbeatTimerRunning());
  EXPECT_EQ(main_frame->GetJavaScriptCallHistory().size(), 0u);

  // Fast forward 399ms; timer should not have fired yet.
  task_environment_.FastForwardBy(base::Milliseconds(399));
  EXPECT_EQ(main_frame->GetJavaScriptCallHistory().size(), 0u);

  // Fast forward 1ms (total 400ms); first ping should have executed.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(main_frame->GetJavaScriptCallHistory().size(), 1u);
  EXPECT_EQ(main_frame->GetLastJavaScriptCall(), u";");

  // Fast forward another 800ms; two more pings should have executed.
  task_environment_.FastForwardBy(base::Milliseconds(800));
  EXPECT_EQ(main_frame->GetJavaScriptCallHistory().size(), 3u);
}

// Test that `Act()` starts the heartbeat timer if it was not already running.
TEST_F(ActorTaskTest, HeartbeatStartsOnAct) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Initialize without backgrounding to verify timer does not start initially.
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kActorTools, kGeminiActor},
      {kEnableBackgroundContinuedProcessing});

  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebFrame* main_frame = AttachMainWebFrame(web_state.get());
  ASSERT_TRUE(main_frame);

  AddControlledWebState(web_state->GetWeakPtr());
  task_->Act({}, "Act without backgrounding", base::DoNothing());
  EXPECT_FALSE(IsHeartbeatTimerRunning());

  // Now enable background continued processing.
  scoped_feature_list.Reset();
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kActorTools, kGeminiActor,
       kEnableBackgroundContinuedProcessing},
      {});

  // Calling `Act()` should start the timer.
  task_->Act({}, "Starting actuation", base::DoNothing());
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(main_frame->GetJavaScriptCallHistory().size(), 1u);
  EXPECT_EQ(main_frame->GetLastJavaScriptCall(), u";");
}

// Test that the heartbeat timer continues running while the task is reflecting.
TEST_F(ActorTaskTest, HeartbeatPersistsDuringReflecting) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kActorTools, kGeminiActor,
       kEnableBackgroundContinuedProcessing},
      {});

  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebFrame* main_frame = AttachMainWebFrame(web_state.get());
  ASSERT_TRUE(main_frame);

  AddControlledWebState(web_state->GetWeakPtr());
  task_->Act({}, "Executing tool", base::DoNothing());
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  // Trigger completion of tool execution, transitioning to reflecting.
  TriggerOnActCompleted(base::DoNothing(), {});
  EXPECT_EQ(task_->GetState(), ActorTaskState::kReflecting);
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  // Verify pings continue while in reflecting state.
  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(main_frame->GetJavaScriptCallHistory().size(), 1u);
  EXPECT_EQ(main_frame->GetLastJavaScriptCall(), u";");
}

// Test that stopping the task stops the heartbeat timer.
TEST_F(ActorTaskTest, HeartbeatStopsOnTaskStop) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kActorTools, kGeminiActor,
       kEnableBackgroundContinuedProcessing},
      {});

  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebFrame* main_frame = AttachMainWebFrame(web_state.get());
  ASSERT_TRUE(main_frame);

  AddControlledWebState(web_state->GetWeakPtr());
  task_->Act({}, "Act", base::DoNothing());
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(main_frame->GetJavaScriptCallHistory().size(), 1u);

  // Stop the task.
  task_->Stop(ActorTaskStoppedReason::kTaskComplete);
  EXPECT_FALSE(IsHeartbeatTimerRunning());

  // Verify no further pings occur after stopping.
  task_environment_.FastForwardBy(base::Milliseconds(800));
  EXPECT_EQ(main_frame->GetJavaScriptCallHistory().size(), 1u);
}

// Test that pausing the task does not stop the heartbeat timer so that
// WebContent processes remain alive throughout the entire duration of the task.
TEST_F(ActorTaskTest, HeartbeatPersistsDuringPause) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kActorTools, kGeminiActor,
       kEnableBackgroundContinuedProcessing},
      {});

  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebFrame* main_frame = AttachMainWebFrame(web_state.get());
  ASSERT_TRUE(main_frame);

  AddControlledWebState(web_state->GetWeakPtr());
  task_->Act({}, "Act", base::DoNothing());
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(main_frame->GetJavaScriptCallHistory().size(), 1u);

  // Pause the task. The heartbeat timer must remain running.
  task_->Pause(/*from_actor=*/false);
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(main_frame->GetJavaScriptCallHistory().size(), 2u);

  // Resume the task.
  task_->Resume();
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(main_frame->GetJavaScriptCallHistory().size(), 3u);
}

// Test that interrupting the task to wait on user input does not stop the
// heartbeat timer.
TEST_F(ActorTaskTest, HeartbeatPersistsDuringWaitingOnUser) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kActorTools, kGeminiActor,
       kEnableBackgroundContinuedProcessing},
      {});

  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebFrame* main_frame = AttachMainWebFrame(web_state.get());
  ASSERT_TRUE(main_frame);

  AddControlledWebState(web_state->GetWeakPtr());
  task_->Act({}, "Act", base::DoNothing());
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  // Interrupt the task to wait on user input.
  task_->Interrupt(/*retain_user_control=*/true,
                   ActorTaskInterruptReason::kWaitingUserConfirmation);
  EXPECT_EQ(task_->GetState(), ActorTaskState::kWaitingOnUser);
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(main_frame->GetJavaScriptCallHistory().size(), 1u);

  // Uninterrupt resumes the task.
  task_->Uninterrupt(ActorTaskState::kActing);
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(main_frame->GetJavaScriptCallHistory().size(), 2u);
}

// Test that destroying all controlled WebStates stops the heartbeat timer.
TEST_F(ActorTaskTest, HeartbeatStopsWhenAllWebStatesDestroyed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kActorTools, kGeminiActor,
       kEnableBackgroundContinuedProcessing},
      {});

  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebFrame* main_frame = AttachMainWebFrame(web_state.get());
  ASSERT_TRUE(main_frame);

  AddControlledWebState(web_state->GetWeakPtr());
  task_->Act({}, "Act", base::DoNothing());
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(main_frame->GetJavaScriptCallHistory().size(), 1u);

  // Destroy the WebState.
  web_state.reset();

  // On the next interval, SendHeartbeatPing detects that no valid WebStates
  // remain, prunes expired weak references, and stops the timer.
  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_FALSE(IsHeartbeatTimerRunning());

  // Adding a new WebState while the task is actuating resumes the heartbeat
  // timer.
  auto web_state2 = std::make_unique<web::FakeWebState>();
  AttachMainWebFrame(web_state2.get());
  AddControlledWebState(web_state2->GetWeakPtr());
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  task_->WebStateDestroyed(web_state2.get());
  EXPECT_FALSE(IsHeartbeatTimerRunning());
}

// Test that heartbeat pings are fire-and-forget, logging failures to the
// journal without stopping the task.
TEST_F(ActorTaskTest, HeartbeatPingsFireAndForget) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kActorTools, kGeminiActor,
       kEnableBackgroundContinuedProcessing},
      {});

  auto web_state = std::make_unique<web::FakeWebState>();
  // Note: Do not add a result for executed JS so FakeWebFrame generates an
  // error.
  web::FakeWebFrame* main_frame = AttachMainWebFrame(web_state.get());
  ASSERT_TRUE(main_frame);

  FakeActorTaskUpdatesObserver* observer =
      [[FakeActorTaskUpdatesObserver alloc] init];
  task_->AddObserver(observer);

  AddControlledWebState(web_state->GetWeakPtr());
  task_->Act({}, "Act", base::DoNothing());
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  // Fast forward across 10 intervals (4000ms total).
  task_environment_.FastForwardBy(base::Milliseconds(4000));
  EXPECT_EQ(main_frame->GetJavaScriptCallHistory().size(), 10u);
  EXPECT_EQ(main_frame->GetLastJavaScriptCall(), u";");
  EXPECT_FALSE(observer.didStopCalled);
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  // Verify journal logs recorded the failures.
  std::vector<mojom::JournalEntryPtr> logs = GetLogsForTesting(journal_.get());
  int failure_events = 0;
  for (const auto& entry : logs) {
    if (entry->event == "ActorTask::HeartbeatPingFailed") {
      failure_events++;
    }
  }
  EXPECT_EQ(failure_events, 10);
}

// Test that successful heartbeat pings do not log failure events to the
// journal.
TEST_F(ActorTaskTest, HeartbeatSuccessfulPingDoesNotLogFailure) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kActorTools, kGeminiActor,
       kEnableBackgroundContinuedProcessing},
      {});

  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebFrame* main_frame = AttachMainWebFrame(web_state.get());
  ASSERT_TRUE(main_frame);
  base::Value success_result;
  main_frame->AddResultForExecutedJs(&success_result, u";");

  AddControlledWebState(web_state->GetWeakPtr());
  task_->Act({}, "Act", base::DoNothing());
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(main_frame->GetJavaScriptCallHistory().size(), 1u);

  std::vector<mojom::JournalEntryPtr> logs = GetLogsForTesting(journal_.get());
  for (const auto& entry : logs) {
    EXPECT_NE(entry->event, "ActorTask::HeartbeatPingFailed");
  }
}

// Test that the heartbeat timer is not started when the backgrounding feature
// parameter is disabled.
TEST_F(ActorTaskTest, HeartbeatDisabledWhenFeatureParamDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{kGeminiActor, {{kGeminiActorBackgroundingParam, "false"}}},
       {kPageActionMenu, {}},
       {kActorTools, {}},
       {kEnableBackgroundContinuedProcessing, {}}},
      {});

  EXPECT_FALSE(IsGeminiActorBackgroundingEnabled());

  auto web_state = std::make_unique<web::FakeWebState>();
  AttachMainWebFrame(web_state.get());

  AddControlledWebState(web_state->GetWeakPtr());
  EXPECT_FALSE(IsHeartbeatTimerRunning());

  task_->Act({}, "Act with backgrounding disabled", base::DoNothing());
  EXPECT_FALSE(IsHeartbeatTimerRunning());

  task_environment_.FastForwardBy(base::Milliseconds(800));
  EXPECT_FALSE(IsHeartbeatTimerRunning());
}

// Test that heartbeat pings are dispatched to multiple controlled WebStates,
// and that destroying one WebState keeps the timer active for the remaining
// valid WebStates until all are destroyed.
TEST_F(ActorTaskTest, HeartbeatMultipleWebStates) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kPageActionMenu, kActorTools, kGeminiActor,
       kEnableBackgroundContinuedProcessing},
      {});

  auto web_state1 = std::make_unique<web::FakeWebState>();
  web::FakeWebFrame* main_frame1 = AttachMainWebFrame(web_state1.get());
  ASSERT_TRUE(main_frame1);
  base::Value success_result;
  main_frame1->AddResultForExecutedJs(&success_result, u";");

  auto web_state2 = std::make_unique<web::FakeWebState>();
  web::FakeWebFrame* main_frame2 = AttachMainWebFrame(web_state2.get());
  ASSERT_TRUE(main_frame2);

  AddControlledWebState(web_state1->GetWeakPtr());
  AddControlledWebState(web_state2->GetWeakPtr());

  task_->Act({}, "Act across multiple WebStates", base::DoNothing());
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  // Fast-forward one interval; both frames receive a ping.
  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(main_frame1->GetJavaScriptCallHistory().size(), 1u);
  EXPECT_EQ(main_frame2->GetJavaScriptCallHistory().size(), 1u);
  EXPECT_TRUE(IsHeartbeatTimerRunning());

  // Destroy only the first WebState.
  web_state1.reset();

  // Fast-forward another interval; heartbeat should still be running for
  // `web_state2`.
  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_TRUE(IsHeartbeatTimerRunning());
  EXPECT_EQ(main_frame2->GetJavaScriptCallHistory().size(), 2u);

  // Destroy the second WebState.
  web_state2.reset();

  // On the next interval, all WebStates have expired, so timer stops.
  task_environment_.FastForwardBy(base::Milliseconds(400));
  EXPECT_FALSE(IsHeartbeatTimerRunning());
}

#endif  // BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)

// Tests that AddControlledWebState attaches the policy decider to the WebState
// and cancels navigation requests when the policy decider blocks the request.
TEST_F(ActorTaskTest,
       Test_AddControlledWebState_Attaches_PolicyDecider_And_Cancels) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorOriginGating);

  const std::string mock_rules_json = R"json({
    "navigation_blocked": [
      {"from": "https://malicious.com", "to": "https://malicious.com"}
    ]
  })json";
  actor::ParseSafetyListsForTesting(actor::SafetyListManager::GetInstance(),
                                    mock_rules_json);

  auto task = std::make_unique<ActorTask>(
      ActorTaskId(1), "Test Task",
      /*allow_incognito_web_states=*/false, journal_.get(), tool_factory_.get(),
      BrowserListFactory::GetForProfile(profile_.get()));

  auto web_state = std::make_unique<web::FakeWebState>();
  task->AddControlledWebState(web_state.get());

  NSURLRequest* request = [NSURLRequest
      requestWithURL:[NSURL URLWithString:@"https://malicious.com"]];
  const web::WebStatePolicyDecider::RequestInfo request_info(
      ui::PageTransition::PAGE_TRANSITION_LINK,
      /*target_frame_is_main=*/true,
      /*target_frame_is_cross_origin=*/false,
      /*target_window_is_cross_origin=*/false,
      /*is_user_initiated=*/false,
      /*user_tapped_recently=*/false);

  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision>
      decision_future;
  web_state->ShouldAllowRequest(request, request_info,
                                decision_future.GetCallback());

  EXPECT_TRUE(decision_future.Get().ShouldCancelNavigation());

  actor::ParseSafetyListsForTesting(actor::SafetyListManager::GetInstance(),
                                    "{}");
}
}  // namespace actor
