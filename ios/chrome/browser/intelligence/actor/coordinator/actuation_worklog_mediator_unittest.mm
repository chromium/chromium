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
#import "components/actor/core/task_source_info.h"
#import "components/password_manager/core/browser/actor_login/actor_login_types.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_suggestions_consumer.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_data_source.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_updates_observer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
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
    (ActuationWorklogItem*)worklogItem {
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

// Data source publishing a worklog driven by the tests, and recording the
// resolved interventions. Once it knows the task title, its worklog starts with
// a first step, as after a task registration.
@interface FakeActorTaskDataSource
    : NSObject <ActorTaskActionHandler, ActorTaskDataSource>
// Attached observer, if any.
@property(nonatomic, weak, readonly) id<ActorTaskDataSourceObserver> observer;
// Action the latest intervention was resolved with, if any.
@property(nonatomic, strong, readonly)
    ActuationInterventionAction* resolvedAction;
// Latest selected suggestion, if any.
@property(nonatomic, strong, readonly) ActorFormSuggestion* selectedSuggestion;
// Creates a data source for a task titled `taskTitle`, or for an unregistered
// task if `taskTitle` is nil.
- (instancetype)initWithTaskTitle:(NSString*)taskTitle;
// Publishes `taskState`.
- (void)changeTaskState:(actor::ActorTaskState)taskState;
// Publishes `item`, executed with `toolType`.
- (void)appendItem:(ActuationWorklogItem*)item
          toolType:(std::optional<actor::ToolType>)toolType;
// Publishes `item` as the replacement of the last item.
- (void)replaceLastItem:(ActuationWorklogItem*)item;
// Publishes `intervention`.
- (void)updateIntervention:(ActuationInterventionData*)intervention;
// Publishes `suggestions` for `item`.
- (void)updateSuggestions:(NSArray<ActorFormSuggestion*>*)suggestions
                  forItem:(ActuationWorklogItem*)item;
@end

@implementation FakeActorTaskDataSource {
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

- (void)addObserver:(id<ActorTaskDataSourceObserver>)observer {
  _observer = observer;
  [observer taskDataSource:self
      didAttachWithTaskTitle:_taskTitle
                   taskState:_taskState
                       items:[_items copy]
              latestToolType:_latestToolType];
}

- (void)removeObserver:(id<ActorTaskDataSourceObserver>)observer {
  if (_observer == observer) {
    _observer = nil;
  }
}

- (void)changeTaskState:(actor::ActorTaskState)taskState {
  _taskState = taskState;
  [_observer taskDataSource:self
         didUpdateTaskTitle:_taskTitle
                  taskState:taskState];
}

- (void)appendItem:(ActuationWorklogItem*)item
          toolType:(std::optional<actor::ToolType>)toolType {
  [_items addObject:item];
  _latestToolType = toolType;
  [_observer taskDataSource:self didAppendItem:item toolType:toolType];
}

- (void)replaceLastItem:(ActuationWorklogItem*)item {
  _items[_items.count - 1] = item;
  _latestToolType = std::nullopt;
  [_observer taskDataSource:self didReplaceLastItem:item];
}

- (void)updateIntervention:(ActuationInterventionData*)intervention {
  [_observer taskDataSource:self didUpdateIntervention:intervention];
}

- (void)updateSuggestions:(NSArray<ActorFormSuggestion*>*)suggestions
                  forItem:(ActuationWorklogItem*)item {
  [_observer taskDataSource:self didUpdateSuggestions:suggestions forItem:item];
}

- (void)resolveInterventionWithAction:(ActuationInterventionAction*)action {
  _resolvedAction = action;
}

- (void)selectSuggestion:(ActorFormSuggestion*)suggestion {
  _selectedSuggestion = suggestion;
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
    data_source_ =
        [[FakeActorTaskDataSource alloc] initWithTaskTitle:@(kTaskTitle)];
    StartObservingDataSource(task_id);
  }

  // Starts observing `task_id` through `data_source_`.
  void StartObservingDataSource(actor::ActorTaskId task_id) {
    [mediator_ startObservingTaskWithID:task_id
                             dataSource:data_source_
                          actionHandler:data_source_];
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

  // Returns an observer registered for the updates of `task_id`. Updates,
  // including the registration itself, are posted.
  FakeTaskStateObserver* ObserveTaskState(actor::ActorTaskId task_id) {
    FakeTaskStateObserver* observer = [[FakeTaskStateObserver alloc] init];
    actor_service_->AddTaskUpdatesObserver(task_id, observer);
    return observer;
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
  FakeActorTaskDataSource* data_source_;
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
      [[FakeActorTaskDataSource alloc] initWithTaskTitle:@(kTaskTitle)];
  ExecuteTool(kClick, @"Step A");
  ExecuteTool(kNavigate, @"Step B");

  StartObservingDataSource(task_id_);

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
      [[FakeActorTaskDataSource alloc] initWithTaskTitle:@(kTaskTitle)];
  ChangeState(kFinished);

  StartObservingDataSource(task_id_);

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

// Tests that nothing is displayed until the data source knows the task title.
TEST_F(ActuationWorklogMediatorTest,
       TestAttachWithoutTaskTitleDisplaysNothing) {
  data_source_ = [[FakeActorTaskDataSource alloc] initWithTaskTitle:nil];
  StartObservingDataSource(task_id_);

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

// Tests that the published intervention is displayed, and cleared once the
// data source clears it.
TEST_F(ActuationWorklogMediatorTest, TestPublishedInterventionIsDisplayed) {
  StartObservingTask(task_id_);
  ActuationInterventionData* intervention = [ActuationInterventionData
      cardItemWithTitle:@"Title"
               subtitle:@"Subtitle"
                 action:[[ActuationInterventionAction alloc]
                            initWithIdentifier:InterventionActionIdentifier::
                                                   kContinue
                                         title:@"Continue"]];

  [data_source_ updateIntervention:intervention];
  EXPECT_EQ(fake_consumer_.intervention, intervention);

  [data_source_ updateIntervention:nil];
  EXPECT_EQ(fake_consumer_.intervention, nil);
}

// Tests that the published suggestions are forwarded to the suggestions
// consumer, and cleared once the data source clears them.
TEST_F(ActuationWorklogMediatorTest, TestPublishedSuggestionsAreForwarded) {
  StartObservingTask(task_id_);
  NSArray<ActorFormSuggestion*>* suggestions =
      @[ CreateCredentialSuggestion(u"user@example.com", u"example.com") ];
  ActuationWorklogItem* item = [ActuationWorklogItem cardItemWithTitle:@"Card"
                                                              subtitle:nil
                                                                  icon:nil
                                                                active:YES];

  [data_source_ updateSuggestions:suggestions forItem:item];
  EXPECT_NSEQ(fake_suggestions_consumer_.suggestions, suggestions);
  EXPECT_EQ(fake_suggestions_consumer_.worklogItem, item);

  [data_source_ updateSuggestions:nil forItem:nil];
  EXPECT_TRUE(fake_suggestions_consumer_.clearSuggestionsCalled);
}

// Tests that a published replacement of the last item replaces the displayed
// last item and the active worklog item of the suggestions.
TEST_F(ActuationWorklogMediatorTest, TestPublishedLastItemReplacement) {
  StartObservingTask(task_id_);
  ActuationWorklogItem* card = [ActuationWorklogItem cardItemWithTitle:@"Card"
                                                              subtitle:nil
                                                                  icon:nil
                                                                active:YES];
  [data_source_ appendItem:card toolType:std::nullopt];
  NSUInteger item_count = fake_consumer_.items.count;
  ActuationWorklogItem* replacement =
      [ActuationWorklogItem cardItemWithTitle:@"Replacement"
                                     subtitle:nil
                                         icon:nil
                                       active:YES];

  [data_source_ replaceLastItem:replacement];

  ASSERT_EQ(fake_consumer_.items.count, item_count);
  EXPECT_EQ(fake_consumer_.items.lastObject, replacement);
  EXPECT_EQ(fake_suggestions_consumer_.worklogItem, replacement);
}

// Tests that stopping actuation stops the task.
TEST_F(ActuationWorklogMediatorTest, TestStopActuationStopsTask) {
  StartObservingTask(task_id_);
  FakeTaskStateObserver* observer = ObserveTaskState(task_id_);

  [mediator_ stopActuation];
  actor::FlushCurrentSequence();

  EXPECT_TRUE(observer.stopped);
  EXPECT_EQ(observer.state, kCancelled);
}

// Tests that intervention actions are forwarded to the action handler.
TEST_F(ActuationWorklogMediatorTest, TestInterventionActionIsForwarded) {
  StartObservingTask(task_id_);
  ActuationInterventionAction* action = [[ActuationInterventionAction alloc]
      initWithIdentifier:InterventionActionIdentifier::kContinue
                   title:@"Continue"];

  [mediator_ didTriggerInterventionAction:action];

  EXPECT_EQ(data_source_.resolvedAction, action);
}

// Tests that a suggestion selected in the suggestions coordinator is forwarded
// to the action handler.
TEST_F(ActuationWorklogMediatorTest, TestSelectedSuggestionIsForwarded) {
  StartObservingTask(task_id_);
  ActorFormSuggestion* suggestion =
      CreateCredentialSuggestion(u"user@example.com", u"example.com");

  [mediator_ suggestionsCoordinator:nil didSelectSuggestion:suggestion];

  EXPECT_EQ(data_source_.selectedSuggestion, suggestion);
}

// Tests that the suggestions consumer is not cleared when observation stops,
// and is detached on disconnect.
TEST_F(ActuationWorklogMediatorTest,
       TestSuggestionsConsumerNotClearedOnStopAndDetachedOnDisconnect) {
  StartObservingTask(task_id_);

  EndActuation();
  EXPECT_FALSE(fake_suggestions_consumer_.clearSuggestionsCalled);

  [mediator_ disconnect];
  EXPECT_EQ(mediator_.suggestionsConsumer, nil);
}
