// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_worklog_mediator.h"

#import <memory>
#import <optional>
#import <utility>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "base/types/expected.h"
#import "components/actor/core/task_source_info.h"
#import "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#import "components/password_manager/core/browser/actor_login/actor_login_types.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_suggestions_consumer.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_updates_observer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/public/actuation_worklog_data_source.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_form_suggestion.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_consumer.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/intelligence/actor/util/actor_test_utils.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
constexpr char kTaskTitle[] = "Task Title";
constexpr char kConfirmationTitle[] = "Input needed";
constexpr char kConfirmationMessage[] = "Please confirm";
constexpr char kConfirmationButtonText[] = "Continue";
NSString* const kFirstStepTitle = @"First step";
NSString* const kFirstStepSubtitle = @"Starting";
using enum actor::ActorTaskState;
using enum actor::ToolType;

// Returns the provenance attributed to tasks created by these tests.
actor::TaskSourceInfo TestSource() {
  return actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kTest,
                               /*id=*/std::nullopt);
}

// Creates an ActorFormSuggestion with password credentials for testing.
ActorFormSuggestion* CreateCredentialSuggestion(
    const std::u16string& username,
    const std::u16string& display_origin) {
  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(1);
  cred.type = actor_login::CredentialType::kPassword;
  cred.username = username;
  cred.display_origin = display_origin;
  return [[ActorFormSuggestion alloc] initWithCredential:cred];
}

// Creates an ActorFormSuggestion with autofill data for testing.
ActorFormSuggestion* CreateAutofillSuggestion(
    const std::string& title,
    const std::string& details,
    autofill::ActorFormFillingRequestedData data_type) {
  autofill::ActorSuggestion suggestion;
  suggestion.id = autofill::ActorSuggestionId(1);
  suggestion.title = title;
  suggestion.details = details;
  return [[ActorFormSuggestion alloc] initWithActorSuggestion:suggestion
                                                     dataType:data_type];
}
}  // namespace

@interface FakeActuationWorklogConsumer : NSObject <ActuationWorklogConsumer>
@property(nonatomic, strong)
    ActuationHeaderContentVariants* headerContentVariants;
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

- (void)setLastItem:(ActuationWorklogItem*)item
               chip:(ActuationWorklogChip*)chip {
  if (_items.count > 0) {
    _items[_items.count - 1] = item;
  } else if (item) {
    [_items addObject:item];
  }
  if (_chips.count > 0) {
    if (chip) {
      _chips[_chips.count - 1] = chip;
    } else {
      [_chips removeLastObject];
    }
  } else if (chip) {
    [_chips addObject:chip];
  }
}

- (void)setIntervention:(ActuationInterventionData*)intervention {
  _intervention = intervention;
}

- (void)reset {
  [_items removeAllObjects];
  [_chips removeAllObjects];
  _headerContentVariants = nil;
  _intervention = nil;
}

@end

// Captures suggestions received from the mediator.
@interface FakeActuationSuggestionsConsumer
    : NSObject <ActuationSuggestionsConsumer>
@property(nonatomic, strong) NSArray<ActorFormSuggestion*>* suggestions;
@property(nonatomic, strong) ActuationWorklogItem* worklogItem;
@property(nonatomic, assign) BOOL clearSuggestionsCalled;
@end

@implementation FakeActuationSuggestionsConsumer

- (void)setSuggestions:(NSArray<ActorFormSuggestion*>*)suggestions
    forActiveWorklogItem:(ActuationWorklogItem*)worklogItem {
  _suggestions = [suggestions copy];
  _worklogItem = worklogItem;
}

- (void)updateActiveWorklogItemForReselectedSuggestion:
            (ActuationWorklogItem*)worklogItem
                                       fromCoordinator:
                                           (ActuationSuggestionsCoordinator*)
                                               coordinator {
  _worklogItem = worklogItem;
}

- (void)clearSuggestions {
  _clearSuggestionsCalled = YES;
  _suggestions = nil;
  _worklogItem = nil;
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

// Data source publishing a worklog driven by the tests. Once it knows the task
// title, its worklog starts with a first step, as after a task registration.
@interface FakeActuationWorklogDataSource
    : NSObject <ActuationWorklogDataSource>
// Attached observer, if any.
@property(nonatomic, weak, readonly) id<ActuationWorklogDataSourceObserver>
    observer;
// Creates a data source for a task titled `taskTitle`, or for an unregistered
// task if `taskTitle` is nil.
- (instancetype)initWithTaskTitle:(NSString*)taskTitle;
// Publishes `taskState`.
- (void)changeTaskState:(actor::ActorTaskState)taskState;
// Publishes `item`, executed with `toolType`.
- (void)appendItem:(ActuationWorklogItem*)item
          toolType:(std::optional<actor::ToolType>)toolType;
@end

@implementation FakeActuationWorklogDataSource {
  NSString* _taskTitle;
  actor::ActorTaskState _taskState;
  NSMutableArray<ActuationWorklogItem*>* _items;
  std::optional<actor::ToolType> _latestToolType;
}

- (instancetype)initWithTaskTitle:(NSString*)taskTitle {
  if ((self = [super init])) {
    _taskTitle = [taskTitle copy];
    _taskState = kInit;
    _items = [NSMutableArray array];
    if (taskTitle) {
      [_items addObject:[ActuationWorklogItem
                            labeledItemWithTitle:kFirstStepTitle
                                        subtitle:kFirstStepSubtitle
                                            icon:nil
                                          active:YES]];
    }
  }
  return self;
}

- (void)addObserver:(id<ActuationWorklogDataSourceObserver>)observer {
  _observer = observer;
  [observer worklogDataSource:self
       didAttachWithTaskTitle:_taskTitle
                    taskState:_taskState
                        items:[_items copy]
               latestToolType:_latestToolType];
}

- (void)removeObserver:(id<ActuationWorklogDataSourceObserver>)observer {
  if (_observer == observer) {
    _observer = nil;
  }
}

- (void)changeTaskState:(actor::ActorTaskState)taskState {
  _taskState = taskState;
  [_observer worklogDataSource:self
            didUpdateTaskTitle:_taskTitle
                     taskState:taskState];
}

- (void)appendItem:(ActuationWorklogItem*)item
          toolType:(std::optional<actor::ToolType>)toolType {
  [_items addObject:item];
  _latestToolType = toolType;
  [_observer worklogDataSource:self didAppendItem:item toolType:toolType];
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
    fake_suggestions_consumer_ =
        [[FakeActuationSuggestionsConsumer alloc] init];
    mediator_ =
        [[ActuationWorklogMediator alloc] initWithActorService:actor_service_];
    mediator_.consumer = fake_consumer_;
    mediator_.suggestionsConsumer = fake_suggestions_consumer_;
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    fake_consumer_ = nil;
    fake_suggestions_consumer_ = nil;
    actor_service_ = nullptr;
    PlatformTest::TearDown();
  }

  // Starts observing `task_id` through a new data source.
  void StartObservingTask(actor::ActorTaskId task_id) {
    data_source_ = [[FakeActuationWorklogDataSource alloc]
        initWithTaskTitle:@(kTaskTitle)];
    [mediator_ startObservingTaskWithID:task_id dataSource:data_source_];
  }

  void ChangeState(actor::ActorTaskState new_state) {
    [data_source_ changeTaskState:new_state];
  }

  void EndActuation() { [mediator_ stopObservingTask]; }

  void ExecuteTool(actor::ToolType tool_type, NSString* update) {
    [data_source_ appendItem:[ActuationWorklogItem simpleItemWithTitle:update
                                                                active:YES]
                    toolType:tool_type];
  }

  // Creates a task and starts observing it, as the Gemini container does
  // synchronously from the task's start notification.
  actor::ActorTaskId CreateAndObserveTask() {
    actor::ActorTaskId task_id = actor_service_->CreateTask(
        kTaskTitle, TestSource(), /*allow_incognito_web_states=*/false);
    StartObservingTask(task_id);
    return task_id;
  }

  // Returns an observer registered for the updates of `task_id`. Updates,
  // including the registration itself, are posted.
  FakeTaskStateObserver* ObserveTaskState(actor::ActorTaskId task_id) {
    FakeTaskStateObserver* observer = [[FakeTaskStateObserver alloc] init];
    actor_service_->AddTaskUpdatesObserver(task_id, observer);
    return observer;
  }

  // Interrupts `task_id` for user confirmation and returns its result, or
  // `std::nullopt` while it awaits the user's answer.
  std::optional<actor::ActorTaskResult<actor::InterruptTaskResult>>
  RequestConfirmation(actor::ActorTaskId task_id) {
    actor::ConfirmationInterrupt confirmation;
    confirmation.title = kConfirmationTitle;
    confirmation.subtitle = kConfirmationMessage;
    confirmation.button_text = kConfirmationButtonText;
    base::test::TestFuture<actor::ActorTaskResult<actor::InterruptTaskResult>>
        result;
    actor_service_->InterruptTask(task_id, std::move(confirmation),
                                  result.GetCallback());
    // Run the interrupt and deliver its updates.
    actor::FlushCurrentSequence();
    if (!result.IsReady()) {
      return std::nullopt;
    }
    return result.Take();
  }

  // Creates and observes a task, then interrupts it for user confirmation.
  actor::ActorTaskId CreateTaskAndRequestConfirmation() {
    actor::ActorTaskId task_id = CreateAndObserveTask();
    RequestConfirmation(task_id);
    return task_id;
  }

  // Returns the header content last pushed to the consumer for `mode`.
  ActuationHeaderContent* HeaderContent(ActuationWorklogDisplayMode mode) {
    return [fake_consumer_.headerContentVariants contentForDisplayMode:mode];
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<actor::ActorService> actor_service_ = nullptr;
  actor::ActorTaskId task_id_;
  FakeActuationWorklogConsumer* fake_consumer_;
  FakeActuationSuggestionsConsumer* fake_suggestions_consumer_;
  FakeActuationWorklogDataSource* data_source_;
  ActuationWorklogMediator* mediator_;
};

// Tests that attaching to the data source displays the task title and its
// worklog.
TEST_F(ActuationWorklogMediatorTest, TestAttachDisplaysWorklog) {
  StartObservingTask(task_id_);

  EXPECT_NSEQ(HeaderContent(ActuationWorklogDisplayModeCompact).title,
              @(kTaskTitle));
  EXPECT_TRUE(fake_consumer_.actuationActive);
  ASSERT_EQ(fake_consumer_.items.count, 1u);
  EXPECT_NSEQ(fake_consumer_.items[0].title, kFirstStepTitle);
  EXPECT_NSEQ(fake_consumer_.items[0].subtitle, kFirstStepSubtitle);
  EXPECT_TRUE(fake_consumer_.items[0].active);
  EXPECT_EQ(fake_consumer_.chips.count, 0u);
}

// Tests that attaching mid-task replays the worklog, with only the latest tool
// displayed as a chip.
TEST_F(ActuationWorklogMediatorTest, TestAttachMidTaskReplaysWorklog) {
  data_source_ =
      [[FakeActuationWorklogDataSource alloc] initWithTaskTitle:@(kTaskTitle)];
  ExecuteTool(kClick, @"Step A");
  ExecuteTool(kNavigate, @"Step B");

  [mediator_ startObservingTaskWithID:task_id_ dataSource:data_source_];

  ASSERT_EQ(fake_consumer_.items.count, 3u);
  EXPECT_NSEQ(fake_consumer_.items.lastObject.title, @"Step B");
  ASSERT_EQ(fake_consumer_.chips.count, 1u);
  EXPECT_NSEQ(fake_consumer_.chips[0].text,
              l10n_util::GetNSString(IDS_IOS_ACTOR_WORKLOG_CHIP_NAVIGATING));
  EXPECT_NSEQ(HeaderContent(ActuationWorklogDisplayModeMinimized).subtitle,
              @"Step B");
  EXPECT_TRUE(fake_consumer_.actuationActive);
}

// Tests that attaching to a finished task displays it as done.
TEST_F(ActuationWorklogMediatorTest, TestAttachToFinishedTaskDisplaysDone) {
  data_source_ =
      [[FakeActuationWorklogDataSource alloc] initWithTaskTitle:@(kTaskTitle)];
  ChangeState(kFinished);

  [mediator_ startObservingTaskWithID:task_id_ dataSource:data_source_];

  EXPECT_FALSE(fake_consumer_.actuationActive);
  EXPECT_NSEQ(HeaderContent(ActuationWorklogDisplayModeMinimized).title,
              l10n_util::GetNSString(IDS_IOS_ACTOR_HEADER_TASK_DONE));
}

// Tests that tool execution emits a chip and timeline item.
TEST_F(ActuationWorklogMediatorTest, TestToolExecutionEmitsChipAndItem) {
  StartObservingTask(task_id_);
  [fake_consumer_.items removeAllObjects];

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

// Tests the different state transitions.
TEST_F(ActuationWorklogMediatorTest, TestTaskLifecycleTransitionsAndReset) {
  EXPECT_FALSE(fake_consumer_.actuationActive);

  StartObservingTask(task_id_);
  EXPECT_TRUE(fake_consumer_.actuationActive);
  ASSERT_EQ(fake_consumer_.items.count, 1u);
  ASSERT_NE(fake_consumer_.headerContentVariants, nil);

  ChangeState(kReflecting);
  EXPECT_TRUE(fake_consumer_.actuationActive);

  ChangeState(kFinished);
  EXPECT_FALSE(fake_consumer_.actuationActive);

  EndActuation();
  EXPECT_FALSE(fake_consumer_.actuationActive);
  EXPECT_EQ(fake_consumer_.items.count, 0u);
  EXPECT_EQ(fake_consumer_.headerContentVariants, nil);
}

// Tests that disconnecting the mediator resets the consumer.
TEST_F(ActuationWorklogMediatorTest, TestDisconnectResetsConsumer) {
  StartObservingTask(task_id_);
  ASSERT_EQ(fake_consumer_.items.count, 1u);
  ASSERT_NE(fake_consumer_.headerContentVariants, nil);

  [mediator_ disconnect];
  EXPECT_EQ(fake_consumer_.items.count, 0u);
  EXPECT_EQ(fake_consumer_.headerContentVariants, nil);
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
  ASSERT_EQ(fake_consumer_.intervention.actions.count, 1u);
  ActuationInterventionAction* action =
      fake_consumer_.intervention.actions.firstObject;
  EXPECT_NSEQ(action.title, @"Continue");
  EXPECT_FALSE(completion_called);

  [mediator_ didTriggerInterventionAction:action];
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

// Tests that nothing is displayed until the data source knows the task title.
TEST_F(ActuationWorklogMediatorTest,
       TestAttachWithoutTaskTitleDisplaysNothing) {
  [mediator_ startObservingTaskWithID:task_id_
                           dataSource:[[FakeActuationWorklogDataSource alloc]
                                          initWithTaskTitle:nil]];

  EXPECT_EQ(fake_consumer_.headerContentVariants, nil);
  EXPECT_FALSE(fake_consumer_.actuationActive);
  EXPECT_EQ(fake_consumer_.items.count, 0u);
}

// Tests that stopping observation detaches the mediator from the data source.
TEST_F(ActuationWorklogMediatorTest, TestStopObservingDetachesFromDataSource) {
  StartObservingTask(task_id_);

  EndActuation();

  EXPECT_FALSE(data_source_.observer);
}

// Tests that a confirmation interrupt is surfaced as an intervention card and
// that tapping its button resumes the task.
TEST_F(ActuationWorklogMediatorTest,
       TestConfirmationInterruptShowsInterventionCard) {
  CreateTaskAndRequestConfirmation();

  EXPECT_EQ(actor_service_->GetActiveTaskState(), kWaitingOnUser);
  ASSERT_NE(fake_consumer_.intervention, nil);
  EXPECT_NSEQ(fake_consumer_.intervention.title, @(kConfirmationTitle));
  EXPECT_NSEQ(fake_consumer_.intervention.subtitle, @(kConfirmationMessage));

  [mediator_ didTriggerInterventionAction:fake_consumer_.intervention.actions
                                              .firstObject];
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
  EXPECT_EQ(RequestConfirmation(task_id),
            base::unexpected(actor::ActorTaskError::kRejected));
  EXPECT_EQ(fake_consumer_.intervention, nil);
}

// Tests that stopping observation detaches the mediator as the task's
// intervention delegate.
TEST_F(ActuationWorklogMediatorTest,
       TestStopObservingTaskClearsInterventionDelegate) {
  actor::ActorTaskId task_id = CreateAndObserveTask();

  [mediator_ stopObservingTask];
  EXPECT_EQ(RequestConfirmation(task_id),
            base::unexpected(actor::ActorTaskError::kRejected));
}

// Tests that observing another task moves the intervention delegate to it.
TEST_F(ActuationWorklogMediatorTest,
       TestStartObservingAnotherTaskMovesInterventionDelegate) {
  actor::ActorTaskId first_task_id = CreateAndObserveTask();
  actor::ActorTaskId second_task_id = CreateAndObserveTask();

  EXPECT_EQ(RequestConfirmation(first_task_id),
            base::unexpected(actor::ActorTaskError::kRejected));
  EXPECT_EQ(RequestConfirmation(second_task_id), std::nullopt);
  EXPECT_NE(fake_consumer_.intervention, nil);
}

// Tests selecting from credentials and allowing autofill.
TEST_F(ActuationWorklogMediatorTest, TestSelectFromCredentialsAllow) {
  StartObservingTask(task_id_);
  [fake_consumer_.items removeAllObjects];

  ActorFormSuggestion* suggestion =
      CreateCredentialSuggestion(u"user@example.com", u"example.com");

  __block ActorFormSuggestion* selected = nil;
  __block BOOL stored = YES;
  [mediator_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
          completionHandler:^(ActorFormSuggestion* s, BOOL storePermission) {
            selected = s;
            stored = storePermission;
          }];

  ASSERT_EQ(fake_consumer_.items.count, 1u);
  ActuationWorklogItem* item = fake_consumer_.items[0];
  EXPECT_NSEQ(item.title,
              l10n_util::GetNSStringF(
                  IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_CREDENTIAL_TITLE_WITH_SITE,
                  u"example.com"));
  EXPECT_NSEQ(item.subtitle,
              l10n_util::GetNSString(
                  IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_CREDENTIAL_SUBTITLE));
  EXPECT_EQ(item.style, ActuationWorklogItemStyle::kCard);
  ASSERT_NE(item.accessoryItem, nil);
  EXPECT_NSEQ(item.accessoryItem.title, @"user@example.com");
  EXPECT_NSEQ(item.accessoryItem.detailText, @"••••••••");

  EXPECT_EQ(fake_suggestions_consumer_.suggestions.count, 1u);
  EXPECT_EQ(fake_suggestions_consumer_.suggestions.firstObject, suggestion);
  EXPECT_EQ(fake_suggestions_consumer_.worklogItem, item);
  EXPECT_FALSE(fake_suggestions_consumer_.clearSuggestionsCalled);

  ASSERT_NE(fake_consumer_.intervention, nil);
  EXPECT_EQ(fake_consumer_.intervention.type, ActuationInterventionType::kRow);
  ASSERT_EQ(fake_consumer_.intervention.actions.count, 2u);
  EXPECT_NSEQ(
      fake_consumer_.intervention.actions[0].title,
      l10n_util::GetNSString(
          IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_CREDENTIAL_MANUAL_FILL_BUTTON));
  EXPECT_NSEQ(
      fake_consumer_.intervention.actions[1].title,
      l10n_util::GetNSString(IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_ALLOW_BUTTON));

  [mediator_
      didTriggerInterventionAction:fake_consumer_.intervention.actions[1]];
  EXPECT_EQ(selected, suggestion);
  EXPECT_FALSE(stored);
  EXPECT_EQ(fake_consumer_.intervention, nil);
  EXPECT_TRUE(fake_suggestions_consumer_.clearSuggestionsCalled);
  EXPECT_EQ(fake_suggestions_consumer_.worklogItem, nil);
}

// Tests that selecting a suggestion via the suggestions coordinator delegate
// updates the selected suggestion used by the completion handler, and updates
// the active worklog item on the suggestions consumer.
TEST_F(ActuationWorklogMediatorTest, TestSelectFromSuggestionsDelegateUpdate) {
  StartObservingTask(task_id_);
  [fake_consumer_.items removeAllObjects];

  ActorFormSuggestion* suggestion1 =
      CreateCredentialSuggestion(u"user1@example.com", u"example.com");
  ActorFormSuggestion* suggestion2 =
      CreateCredentialSuggestion(u"user2@example.com", u"example.com");

  __block ActorFormSuggestion* selected = nil;
  [mediator_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion1, suggestion2 ]
          completionHandler:^(ActorFormSuggestion* s, BOOL storePermission) {
            selected = s;
          }];

  ASSERT_EQ(fake_consumer_.items.count, 1u);
  EXPECT_NSEQ(fake_consumer_.items[0].accessoryItem.title,
              @"user1@example.com");

  [mediator_ suggestionsCoordinator:nil didSelectSuggestion:suggestion2];

  EXPECT_NSEQ(fake_consumer_.items[0].accessoryItem.title,
              @"user2@example.com");
  EXPECT_EQ(fake_suggestions_consumer_.worklogItem, fake_consumer_.items[0]);

  [mediator_
      didTriggerInterventionAction:fake_consumer_.intervention.actions[1]];
  EXPECT_EQ(selected, suggestion2);
}

// Tests selecting from credentials with the manual sign-in action.
TEST_F(ActuationWorklogMediatorTest, TestSelectFromCredentialsManualSignIn) {
  StartObservingTask(task_id_);
  [fake_consumer_.items removeAllObjects];

  ActorFormSuggestion* suggestion =
      CreateCredentialSuggestion(u"user@example.com", u"");

  __block ActorFormSuggestion* selected = nil;
  __block BOOL stored = YES;
  __block BOOL completion_called = NO;
  [mediator_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
          completionHandler:^(ActorFormSuggestion* s, BOOL storePermission) {
            completion_called = YES;
            selected = s;
            stored = storePermission;
          }];

  ASSERT_EQ(fake_consumer_.items.count, 1u);
  EXPECT_NSEQ(fake_consumer_.items[0].title,
              l10n_util::GetNSString(
                  IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_CREDENTIAL_TITLE));
  ASSERT_NE(fake_consumer_.intervention, nil);
  ASSERT_EQ(fake_consumer_.intervention.actions.count, 2u);

  [mediator_
      didTriggerInterventionAction:fake_consumer_.intervention.actions[0]];
  EXPECT_TRUE(completion_called);
  EXPECT_EQ(selected, nil);
  EXPECT_FALSE(stored);
  EXPECT_EQ(fake_consumer_.intervention, nil);
}

// Tests that the suggestions consumer is not cleared when interventions are
// discarded (e.g. task ending), but is detached on disconnect.
TEST_F(ActuationWorklogMediatorTest,
       TestSuggestionsConsumerNotClearedOnDiscardButDetachedOnDisconnect) {
  StartObservingTask(task_id_);
  ActorFormSuggestion* suggestion =
      CreateCredentialSuggestion(u"user@example.com", u"example.com");
  [mediator_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
          completionHandler:nil];

  ASSERT_NE(fake_suggestions_consumer_.worklogItem, nil);
  EXPECT_FALSE(fake_suggestions_consumer_.clearSuggestionsCalled);

  // Ending actuation discards the pending intervention, but suggestions
  // remain available.
  EndActuation();
  EXPECT_FALSE(fake_suggestions_consumer_.clearSuggestionsCalled);
  EXPECT_NE(fake_suggestions_consumer_.worklogItem, nil);

  // Re-observe and disconnect.
  StartObservingTask(task_id_);
  EXPECT_NE(mediator_.suggestionsConsumer, nil);
  [mediator_ disconnect];
  EXPECT_EQ(mediator_.suggestionsConsumer, nil);
}

// Tests that completing a suggestions intervention clears the suggestions, and
// a subsequent suggestions intervention cleanly sets new suggestions.
TEST_F(ActuationWorklogMediatorTest,
       TestStopAndRestartSuggestionsIntervention) {
  StartObservingTask(task_id_);
  [fake_consumer_.items removeAllObjects];

  ActorFormSuggestion* suggestion1 =
      CreateCredentialSuggestion(u"user1@example.com", u"example.com");
  __block ActorFormSuggestion* selected1 = nil;
  [mediator_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion1 ]
          completionHandler:^(ActorFormSuggestion* s, BOOL storePermission) {
            selected1 = s;
          }];

  ASSERT_EQ(fake_consumer_.items.count, 1u);
  EXPECT_EQ(fake_suggestions_consumer_.suggestions.count, 1u);
  EXPECT_EQ(fake_suggestions_consumer_.suggestions.firstObject, suggestion1);
  EXPECT_EQ(fake_suggestions_consumer_.worklogItem, fake_consumer_.items[0]);
  EXPECT_FALSE(fake_suggestions_consumer_.clearSuggestionsCalled);
  ASSERT_NE(fake_consumer_.intervention, nil);

  [mediator_
      didTriggerInterventionAction:fake_consumer_.intervention.actions[1]];
  EXPECT_EQ(selected1, suggestion1);
  EXPECT_EQ(fake_consumer_.intervention, nil);
  EXPECT_TRUE(fake_suggestions_consumer_.clearSuggestionsCalled);
  EXPECT_EQ(fake_suggestions_consumer_.suggestions, nil);
  EXPECT_EQ(fake_suggestions_consumer_.worklogItem, nil);

  fake_suggestions_consumer_.clearSuggestionsCalled = NO;

  ActorFormSuggestion* suggestion2 =
      CreateCredentialSuggestion(u"user2@example.com", u"example.com");
  __block ActorFormSuggestion* selected2 = nil;
  [mediator_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion2 ]
          completionHandler:^(ActorFormSuggestion* s, BOOL storePermission) {
            selected2 = s;
          }];

  ASSERT_EQ(fake_consumer_.items.count, 2u);
  EXPECT_NE(fake_consumer_.items[0], fake_consumer_.items[1]);
  EXPECT_EQ(fake_suggestions_consumer_.suggestions.count, 1u);
  EXPECT_EQ(fake_suggestions_consumer_.suggestions.firstObject, suggestion2);
  EXPECT_EQ(fake_suggestions_consumer_.worklogItem, fake_consumer_.items[1]);
  EXPECT_FALSE(fake_suggestions_consumer_.clearSuggestionsCalled);
  ASSERT_NE(fake_consumer_.intervention, nil);

  [mediator_
      didTriggerInterventionAction:fake_consumer_.intervention.actions[1]];
  EXPECT_EQ(selected2, suggestion2);
  EXPECT_EQ(fake_consumer_.intervention, nil);
  EXPECT_TRUE(fake_suggestions_consumer_.clearSuggestionsCalled);
  EXPECT_EQ(fake_suggestions_consumer_.suggestions, nil);
  EXPECT_EQ(fake_suggestions_consumer_.worklogItem, nil);
}

// Tests selecting from addresses and allowing autofill.
TEST_F(ActuationWorklogMediatorTest, TestSelectFromAddressesAllow) {
  StartObservingTask(task_id_);
  [fake_consumer_.items removeAllObjects];

  ActorFormSuggestion* suggestion = CreateAutofillSuggestion(
      "123 Main St", "Springfield, IL",
      autofill::ActorFormFillingRequestedData::kAddress);

  __block ActorFormSuggestion* selected = nil;
  __block BOOL stored = YES;
  __block BOOL completion_called = NO;
  [mediator_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
          completionHandler:^(ActorFormSuggestion* s, BOOL storePermission) {
            completion_called = YES;
            selected = s;
            stored = storePermission;
          }];

  ASSERT_EQ(fake_consumer_.items.count, 1u);
  ActuationWorklogItem* item = fake_consumer_.items[0];
  EXPECT_NSEQ(item.title, l10n_util::GetNSString(
                              IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_ADDRESS_TITLE));
  EXPECT_EQ(item.subtitle, nil);
  EXPECT_EQ(item.style, ActuationWorklogItemStyle::kCard);
  ASSERT_NE(item.accessoryItem, nil);
  EXPECT_NSEQ(item.accessoryItem.title, @"123 Main St");
  EXPECT_NSEQ(item.accessoryItem.detailText, @"Springfield, IL");

  ASSERT_NE(fake_consumer_.intervention, nil);
  EXPECT_EQ(fake_consumer_.intervention.type, ActuationInterventionType::kRow);
  ASSERT_EQ(fake_consumer_.intervention.actions.count, 2u);
  EXPECT_NSEQ(fake_consumer_.intervention.actions[0].title,
              l10n_util::GetNSString(
                  IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_MANUAL_FILL_BUTTON));
  EXPECT_NSEQ(
      fake_consumer_.intervention.actions[1].title,
      l10n_util::GetNSString(IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_ALLOW_BUTTON));
  EXPECT_FALSE(completion_called);

  [mediator_
      didTriggerInterventionAction:fake_consumer_.intervention.actions[1]];
  EXPECT_TRUE(completion_called);
  EXPECT_EQ(selected, suggestion);
  EXPECT_FALSE(stored);
  EXPECT_EQ(fake_consumer_.intervention, nil);
}

// Tests selecting from contacts and verifying that ending observation or
// disconnecting discards the pending completion.
TEST_F(ActuationWorklogMediatorTest,
       TestSelectFromContactsDiscardedOnObservationEnd) {
  StartObservingTask(task_id_);
  [fake_consumer_.items removeAllObjects];

  ActorFormSuggestion* suggestion = CreateAutofillSuggestion(
      "Jane Doe", "jane@example.com",
      autofill::ActorFormFillingRequestedData::kContactInformation);

  __block BOOL completion_called = NO;
  [mediator_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
          completionHandler:^(ActorFormSuggestion* s, BOOL storePermission) {
            completion_called = YES;
          }];

  ASSERT_EQ(fake_consumer_.items.count, 1u);
  ActuationWorklogItem* item = fake_consumer_.items[0];
  EXPECT_NSEQ(item.title, l10n_util::GetNSString(
                              IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_CONTACT_TITLE));
  EXPECT_EQ(item.subtitle, nil);
  EXPECT_EQ(item.style, ActuationWorklogItemStyle::kCard);
  ASSERT_NE(item.accessoryItem, nil);
  EXPECT_NSEQ(item.accessoryItem.title, @"Jane Doe");
  EXPECT_NSEQ(item.accessoryItem.detailText, @"jane@example.com");

  ASSERT_NE(fake_consumer_.intervention, nil);
  EXPECT_EQ(fake_consumer_.intervention.type, ActuationInterventionType::kRow);
  ASSERT_EQ(fake_consumer_.intervention.actions.count, 2u);
  EXPECT_NSEQ(fake_consumer_.intervention.actions[0].title,
              l10n_util::GetNSString(
                  IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_MANUAL_FILL_BUTTON));
  EXPECT_NSEQ(
      fake_consumer_.intervention.actions[1].title,
      l10n_util::GetNSString(IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_ALLOW_BUTTON));
  EXPECT_FALSE(completion_called);

  // Stopping observation discards the completion handler and clears UI.
  EndActuation();
  EXPECT_FALSE(completion_called);
  EXPECT_EQ(fake_consumer_.intervention, nil);

  // Restart observation and trigger a second suggestion.
  StartObservingTask(task_id_);
  __block BOOL second_completion_called = NO;
  [mediator_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
          completionHandler:^(ActorFormSuggestion* s, BOOL storePermission) {
            second_completion_called = YES;
          }];

  ASSERT_NE(fake_consumer_.intervention, nil);
  EXPECT_FALSE(second_completion_called);

  // Disconnecting also discards the completion handler and clears UI.
  [mediator_ disconnect];
  EXPECT_FALSE(second_completion_called);
  EXPECT_EQ(fake_consumer_.intervention, nil);
}

// Tests selecting from payments and verifying that stopping actuation
// cancels the task without invoking the completion.
TEST_F(ActuationWorklogMediatorTest,
       TestSelectFromPaymentsCancelledOnStopActuation) {
  StartObservingTask(task_id_);
  [fake_consumer_.items removeAllObjects];
  FakeTaskStateObserver* observer = ObserveTaskState(task_id_);

  ActorFormSuggestion* suggestion = CreateAutofillSuggestion(
      "Visa", "•••• 1234",
      autofill::ActorFormFillingRequestedData::kCreditCard);

  __block BOOL completion_called = NO;
  [mediator_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
          completionHandler:^(ActorFormSuggestion* s, BOOL storePermission) {
            completion_called = YES;
          }];

  ASSERT_EQ(fake_consumer_.items.count, 1u);
  ActuationWorklogItem* item = fake_consumer_.items[0];
  EXPECT_NSEQ(item.title, l10n_util::GetNSString(
                              IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_PAYMENT_TITLE));
  EXPECT_NSEQ(item.subtitle,
              l10n_util::GetNSString(
                  IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_PAYMENT_SUBTITLE));
  EXPECT_EQ(item.style, ActuationWorklogItemStyle::kCard);
  ASSERT_NE(item.accessoryItem, nil);
  EXPECT_NSEQ(item.accessoryItem.title, @"Visa");
  EXPECT_NSEQ(item.accessoryItem.detailText, @"•••• 1234");

  ASSERT_NE(fake_consumer_.intervention, nil);
  EXPECT_EQ(fake_consumer_.intervention.type, ActuationInterventionType::kRow);
  ASSERT_EQ(fake_consumer_.intervention.actions.count, 2u);
  EXPECT_NSEQ(fake_consumer_.intervention.actions[0].title,
              l10n_util::GetNSString(
                  IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_MANUAL_FILL_BUTTON));
  EXPECT_NSEQ(
      fake_consumer_.intervention.actions[1].title,
      l10n_util::GetNSString(IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_ALLOW_BUTTON));
  EXPECT_FALSE(completion_called);

  [mediator_ stopActuation];
  actor::FlushCurrentSequence();

  EXPECT_TRUE(observer.stopped);
  EXPECT_EQ(observer.state, kCancelled);
  EXPECT_EQ(actor_service_->GetActiveTaskState(), std::nullopt);
  EXPECT_FALSE(completion_called);

  // Ending observation clears consumer intervention.
  EndActuation();
  EXPECT_EQ(fake_consumer_.intervention, nil);
  EXPECT_FALSE(completion_called);
}

// Tests that while the task runs, the minimized header shows the task title
// above the current step, and the other modes only show the task title.
TEST_F(ActuationWorklogMediatorTest, TestHeaderContentWhileRunning) {
  StartObservingTask(task_id_);

  ActuationHeaderContent* minimized =
      HeaderContent(ActuationWorklogDisplayModeMinimized);
  EXPECT_NSEQ(minimized.title, @(kTaskTitle));
  EXPECT_EQ(minimized.subtitle, nil);

  ExecuteTool(kClick, @"Clicking button");
  minimized = HeaderContent(ActuationWorklogDisplayModeMinimized);
  EXPECT_NSEQ(minimized.title, @(kTaskTitle));
  EXPECT_NSEQ(minimized.subtitle, @"Clicking button");

  for (ActuationWorklogDisplayMode mode :
       {ActuationWorklogDisplayModeCompact,
        ActuationWorklogDisplayModeExpanded}) {
    SCOPED_TRACE(mode);
    ActuationHeaderContent* content = HeaderContent(mode);
    EXPECT_NSEQ(content.title, @(kTaskTitle));
    EXPECT_EQ(content.subtitle, nil);
  }
}

// Tests that the minimized header shows the status above the task title for
// states with a status, and shows the current step again once the task runs.
TEST_F(ActuationWorklogMediatorTest, TestMinimizedHeaderContentShowsStatus) {
  StartObservingTask(task_id_);
  ExecuteTool(kClick, @"Clicking button");

  const std::pair<actor::ActorTaskState, int> kStatuses[] = {
      {kWaitingOnUser, IDS_IOS_ACTOR_HEADER_INPUT_NEEDED},
      {kFinished, IDS_IOS_ACTOR_HEADER_TASK_DONE},
      {kCancelled, IDS_IOS_ACTOR_HEADER_TASK_STOPPED},
      {kFailed, IDS_IOS_ACTOR_HEADER_TASK_FAILED},
  };
  for (const auto& [state, message_id] : kStatuses) {
    SCOPED_TRACE(static_cast<int>(state));
    ChangeState(state);
    ActuationHeaderContent* minimized =
        HeaderContent(ActuationWorklogDisplayModeMinimized);
    EXPECT_NSEQ(minimized.title, l10n_util::GetNSString(message_id));
    EXPECT_NSEQ(minimized.subtitle, @(kTaskTitle));
    EXPECT_NSEQ(HeaderContent(ActuationWorklogDisplayModeCompact).title,
                @(kTaskTitle));
  }

  ChangeState(kActing);
  ActuationHeaderContent* minimized =
      HeaderContent(ActuationWorklogDisplayModeMinimized);
  EXPECT_NSEQ(minimized.title, @(kTaskTitle));
  EXPECT_NSEQ(minimized.subtitle, @"Clicking button");
}
