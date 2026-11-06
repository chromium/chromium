// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_actor_task_store.h"

#import <memory>
#import <optional>

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "base/types/expected.h"
#import "components/actor/core/task_source_info.h"
#import "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/password_manager/core/browser/actor_login/actor_login_types.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_form_suggestion.h"
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

// Records the worklog published by an `ActorTaskDataSource`.
@interface FakeTaskDataSourceObserver : NSObject <ActorTaskDataSourceObserver>
@property(nonatomic, copy) NSString* taskTitle;
@property(nonatomic, assign) actor::ActorTaskState taskState;
@property(nonatomic, strong) NSMutableArray<ActuationWorklogItem*>* items;
@property(nonatomic, assign) std::optional<actor::ToolType> latestToolType;
@property(nonatomic, assign) NSUInteger attachCount;
@property(nonatomic, strong) ActuationInterventionData* intervention;
@property(nonatomic, copy) NSArray<ActorFormSuggestion*>* suggestions;
@property(nonatomic, strong) ActuationWorklogItem* suggestionsItem;
@end

@implementation FakeTaskDataSourceObserver

- (void)taskDataSource:(id<ActorTaskDataSource>)dataSource
    didAttachWithTaskTitle:(NSString*)taskTitle
                 taskState:(actor::ActorTaskState)taskState
                     items:(NSArray<ActuationWorklogItem*>*)items
            latestToolType:(std::optional<actor::ToolType>)latestToolType {
  _attachCount++;
  _taskTitle = [taskTitle copy];
  _taskState = taskState;
  _items = [items mutableCopy];
  _latestToolType = latestToolType;
}

- (void)taskDataSource:(id<ActorTaskDataSource>)dataSource
    didUpdateTaskTitle:(NSString*)taskTitle
             taskState:(actor::ActorTaskState)taskState {
  _taskTitle = [taskTitle copy];
  _taskState = taskState;
}

- (void)taskDataSource:(id<ActorTaskDataSource>)dataSource
         didAppendItem:(ActuationWorklogItem*)item
              toolType:(std::optional<actor::ToolType>)toolType {
  [_items addObject:item];
  _latestToolType = toolType;
}

- (void)taskDataSource:(id<ActorTaskDataSource>)dataSource
    didReplaceLastItem:(ActuationWorklogItem*)item {
  _items[_items.count - 1] = item;
  _latestToolType = std::nullopt;
}

- (void)taskDataSource:(id<ActorTaskDataSource>)dataSource
    didUpdateIntervention:(ActuationInterventionData*)intervention {
  _intervention = intervention;
}

- (void)taskDataSource:(id<ActorTaskDataSource>)dataSource
    didUpdateSuggestions:(NSArray<ActorFormSuggestion*>*)suggestions
                 forItem:(ActuationWorklogItem*)item {
  _suggestions = [suggestions copy];
  _suggestionsItem = item;
}

@end

// Records the state of the task it observes, as reported by the posted task
// updates.
@interface FakeGeminiTaskStateObserver : NSObject <ActorTaskUpdatesObserver>
@property(nonatomic, assign) actor::ActorTaskState state;
@property(nonatomic, assign) BOOL stopped;
@end

@implementation FakeGeminiTaskStateObserver

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

namespace {

using enum actor::ActorTaskState;
using enum actor::ToolType;

// Title of the tasks created by the tests.
constexpr char kTaskTitle[] = "Task Title";
constexpr char kConfirmationTitle[] = "Input needed";
constexpr char kConfirmationMessage[] = "Please confirm";
constexpr char kConfirmationButtonText[] = "Continue";

// Returns the provenance attributed to tasks created by these tests.
actor::TaskSourceInfo TestSource() {
  return actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kTest,
                               /*id=*/std::nullopt);
}

// Creates an ActorFormSuggestion with password credentials for testing.
ActorFormSuggestion* CreateCredentialSuggestion(
    const std::u16string& username,
    const std::u16string& display_origin,
    actor_login::CredentialType type = actor_login::CredentialType::kPassword) {
  actor_login::Credential cred;
  cred.id = actor_login::Credential::Id(1);
  cred.type = type;
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

class GeminiActorTaskStoreTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(kActorTools);
    actor::ActorServiceFactory::GetInstance();
    profile_ = TestProfileIOS::Builder().Build();
    actor_service_ = actor::ActorServiceFactory::GetForProfile(profile_.get());
    ASSERT_TRUE(actor_service_);
    task_id_ = CreateTask();
    store_ = [[GeminiActorTaskStore alloc] initWithTaskID:task_id_
                                             actorService:actor_service_];
    observer_ = [[FakeTaskDataSourceObserver alloc] init];
  }

  void TearDown() override {
    [store_ disconnect];
    store_ = nil;
    actor_service_ = nullptr;
    PlatformTest::TearDown();
  }

  actor::ActorTaskId CreateTask() {
    return actor_service_->CreateTask(kTaskTitle, TestSource(),
                                      /*allow_incognito_web_states=*/false);
  }

  // Adds `observer_` and delivers the registration posted by the task.
  void AttachAndRegister() {
    [store_ addObserver:observer_];
    actor::FlushCurrentSequence();
  }

  void ExecuteTool(actor::ToolType tool_type, NSString* update) {
    [store_ actorTaskWithID:task_id_
            willExecuteTool:tool_type
                 taskUpdate:update
                 onWebState:web::WebStateID::FromSerializedValue(1)];
  }

  // Returns an observer registered for the updates of `task_id`. Updates,
  // including the registration itself, are posted.
  FakeGeminiTaskStateObserver* ObserveTaskState(actor::ActorTaskId task_id) {
    FakeGeminiTaskStateObserver* observer =
        [[FakeGeminiTaskStateObserver alloc] init];
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

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<actor::ActorService> actor_service_ = nullptr;
  actor::ActorTaskId task_id_;
  GeminiActorTaskStore* store_;
  FakeTaskDataSourceObserver* observer_;
};

// Tests that an observer attached before the registration receives an empty
// worklog, then the task title and the Gemini first step.
TEST_F(GeminiActorTaskStoreTest, RegistrationPublishesTitleAndFirstStep) {
  [store_ addObserver:observer_];
  EXPECT_EQ(observer_.items.count, 0u);

  actor::FlushCurrentSequence();

  EXPECT_NSEQ(observer_.taskTitle, @(kTaskTitle));
  ASSERT_EQ(observer_.items.count, 1u);
  EXPECT_EQ(observer_.items[0].style, ActuationWorklogItemStyle::kLabeled);
  EXPECT_NSEQ(
      observer_.items[0].title,
      l10n_util::GetNSString(IDS_IOS_GEMINI_FIRST_ACTUATION_STEP_TITLE));
  EXPECT_FALSE(observer_.latestToolType.has_value());
}

// Tests that a non-empty initial task update is appended after the Gemini first
// step.
TEST_F(GeminiActorTaskStoreTest, InitialTaskUpdateFollowsFirstStep) {
  [store_ addObserver:observer_];

  [store_ didRegisterAsObserverForTaskID:task_id_
                               taskTitle:@(kTaskTitle)
                              taskUpdate:@"Initial update"
                            currentState:kActing
                               webStates:@[]];

  ASSERT_EQ(observer_.items.count, 2u);
  EXPECT_EQ(observer_.items[0].style, ActuationWorklogItemStyle::kLabeled);
  EXPECT_NSEQ(observer_.items[1].title, @"Initial update");
}

// Tests that tool executions append items with their tool.
TEST_F(GeminiActorTaskStoreTest, ToolExecutionAppendsItem) {
  AttachAndRegister();
  const NSUInteger initialCount = observer_.items.count;

  ExecuteTool(kClick, @"Clicking button");

  ASSERT_EQ(observer_.items.count, initialCount + 1);
  EXPECT_NSEQ(observer_.items.lastObject.title, @"Clicking button");
  EXPECT_EQ(observer_.items.lastObject.style,
            ActuationWorklogItemStyle::kSimple);
  EXPECT_EQ(observer_.latestToolType, kClick);
}

// Tests that empty and consecutive duplicate task updates are ignored.
TEST_F(GeminiActorTaskStoreTest, IgnoresEmptyAndDuplicateUpdates) {
  AttachAndRegister();
  const NSUInteger initialCount = observer_.items.count;

  ExecuteTool(kType, @"");
  EXPECT_EQ(observer_.items.count, initialCount);

  ExecuteTool(kClick, @"Step A");
  ExecuteTool(kClick, @"Step A");
  EXPECT_EQ(observer_.items.count, initialCount + 1);

  ExecuteTool(kScroll, @"Step B");
  ExecuteTool(kClick, @"Step A");
  EXPECT_EQ(observer_.items.count, initialCount + 3);
}

// Tests that state changes are published.
TEST_F(GeminiActorTaskStoreTest, StateChangeIsPublished) {
  AttachAndRegister();

  [store_ actorTaskWithID:task_id_
           didChangeState:kPausedByUser
                fromState:kActing];

  EXPECT_EQ(observer_.taskState, kPausedByUser);
  EXPECT_NSEQ(observer_.taskTitle, @(kTaskTitle));
}

// Tests that an observer added mid-task receives the whole worklog.
TEST_F(GeminiActorTaskStoreTest, LateObserverReceivesWorklog) {
  AttachAndRegister();
  const NSUInteger initialCount = observer_.items.count;
  ExecuteTool(kClick, @"Step A");
  ExecuteTool(kNavigate, @"Step B");
  [store_ actorTaskWithID:task_id_
           didChangeState:kPausedByUser
                fromState:kActing];

  FakeTaskDataSourceObserver* lateObserver =
      [[FakeTaskDataSourceObserver alloc] init];
  [store_ addObserver:lateObserver];

  EXPECT_EQ(lateObserver.attachCount, 1u);
  EXPECT_NSEQ(lateObserver.taskTitle, @(kTaskTitle));
  EXPECT_EQ(lateObserver.taskState, kPausedByUser);
  ASSERT_EQ(lateObserver.items.count, initialCount + 2);
  EXPECT_NSEQ(lateObserver.items.lastObject.title, @"Step B");
  EXPECT_EQ(lateObserver.latestToolType, kNavigate);
}

// Tests that removed observers stop receiving updates.
TEST_F(GeminiActorTaskStoreTest, RemovedObserverStopsReceivingUpdates) {
  AttachAndRegister();
  const NSUInteger initialCount = observer_.items.count;
  [store_ removeObserver:observer_];

  ExecuteTool(kClick, @"Step A");

  EXPECT_EQ(observer_.items.count, initialCount);
}

// Tests that adding an observer twice only sends it the worklog once.
TEST_F(GeminiActorTaskStoreTest, AddingObserverTwiceSendsWorklogOnce) {
  AttachAndRegister();

  [store_ addObserver:observer_];

  EXPECT_EQ(observer_.attachCount, 1u);
}

// Tests the user intervention flow.
TEST_F(GeminiActorTaskStoreTest, UserInterventionFlow) {
  AttachAndRegister();
  __block BOOL completion_called = NO;
  [store_ actorTask:task_id_
      requestUserInterventionWithTitle:@"Intervention Title"
                              subtitle:@"Intervention Subtitle"
                            buttonText:@"Continue"
                     completionHandler:^{
                       completion_called = YES;
                     }];

  ASSERT_NE(observer_.intervention, nil);
  EXPECT_NSEQ(observer_.intervention.title, @"Intervention Title");
  EXPECT_NSEQ(observer_.intervention.subtitle, @"Intervention Subtitle");
  ASSERT_EQ(observer_.intervention.actions.count, 1u);
  ActuationInterventionAction* action =
      observer_.intervention.actions.firstObject;
  EXPECT_NSEQ(action.title, @"Continue");
  EXPECT_FALSE(completion_called);

  [store_ resolveInterventionWithAction:action];
  EXPECT_TRUE(completion_called);
  EXPECT_EQ(observer_.intervention, nil);
}

// Tests that replacing or disconnecting an intervention discards the pending
// completion without invoking it.
TEST_F(GeminiActorTaskStoreTest, UserInterventionCancellation) {
  AttachAndRegister();
  __block BOOL first_completion_called = NO;
  [store_ actorTask:task_id_
      requestUserInterventionWithTitle:@"First Title"
                              subtitle:@"First Subtitle"
                            buttonText:@"Action 1"
                     completionHandler:^{
                       first_completion_called = YES;
                     }];

  ASSERT_NE(observer_.intervention, nil);
  EXPECT_FALSE(first_completion_called);

  // A second intervention arrives before user acts on the first.
  __block BOOL second_completion_called = NO;
  [store_ actorTask:task_id_
      requestUserInterventionWithTitle:@"Second Title"
                              subtitle:@"Second Subtitle"
                            buttonText:@"Action 2"
                     completionHandler:^{
                       second_completion_called = YES;
                     }];

  // First completion must be discarded without invocation.
  EXPECT_FALSE(first_completion_called);
  EXPECT_FALSE(second_completion_called);
  EXPECT_NSEQ(observer_.intervention.title, @"Second Title");

  // Disconnecting discards the pending completion.
  [store_ disconnect];
  EXPECT_FALSE(second_completion_called);
}

// Tests that an observer added while an intervention is pending receives it,
// with the selectable suggestions.
TEST_F(GeminiActorTaskStoreTest, LateObserverReceivesPendingIntervention) {
  AttachAndRegister();
  ActorFormSuggestion* suggestion =
      CreateCredentialSuggestion(u"user@example.com", u"example.com");
  [store_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
          completionHandler:nil];

  FakeTaskDataSourceObserver* lateObserver =
      [[FakeTaskDataSourceObserver alloc] init];
  [store_ addObserver:lateObserver];

  ASSERT_NE(lateObserver.intervention, nil);
  EXPECT_EQ(lateObserver.intervention, observer_.intervention);
  EXPECT_NSEQ(lateObserver.suggestions, @[ suggestion ]);
  EXPECT_EQ(lateObserver.suggestionsItem, lateObserver.items.lastObject);
}

// Tests selecting from credentials and allowing autofill.
TEST_F(GeminiActorTaskStoreTest, SelectFromCredentialsAllow) {
  AttachAndRegister();
  [observer_.items removeAllObjects];

  ActorFormSuggestion* suggestion =
      CreateCredentialSuggestion(u"user@example.com", u"example.com");

  __block ActorFormSuggestion* selected = nil;
  __block BOOL stored = YES;
  [store_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
          completionHandler:^(ActorFormSuggestion* s, BOOL storePermission) {
            selected = s;
            stored = storePermission;
          }];

  ASSERT_EQ(observer_.items.count, 1u);
  ActuationWorklogItem* item = observer_.items[0];
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

  EXPECT_EQ(observer_.suggestions.count, 1u);
  EXPECT_EQ(observer_.suggestions.firstObject, suggestion);
  EXPECT_EQ(observer_.suggestionsItem, item);

  ASSERT_NE(observer_.intervention, nil);
  EXPECT_EQ(observer_.intervention.type, ActuationInterventionType::kRow);
  ASSERT_EQ(observer_.intervention.actions.count, 2u);
  EXPECT_NSEQ(
      observer_.intervention.actions[0].title,
      l10n_util::GetNSString(
          IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_CREDENTIAL_MANUAL_FILL_BUTTON));
  EXPECT_NSEQ(
      observer_.intervention.actions[1].title,
      l10n_util::GetNSString(IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_ALLOW_BUTTON));

  [store_ resolveInterventionWithAction:observer_.intervention.actions[1]];
  EXPECT_EQ(selected, suggestion);
  EXPECT_FALSE(stored);
  EXPECT_EQ(observer_.intervention, nil);
  EXPECT_EQ(observer_.suggestions, nil);
  EXPECT_EQ(observer_.suggestionsItem, nil);
}

// Tests selecting from credentials with the manual sign-in action.
TEST_F(GeminiActorTaskStoreTest, SelectFromCredentialsManualSignIn) {
  AttachAndRegister();
  [observer_.items removeAllObjects];

  ActorFormSuggestion* suggestion =
      CreateCredentialSuggestion(u"user@example.com", u"");

  __block ActorFormSuggestion* selected = nil;
  __block BOOL stored = YES;
  __block BOOL completion_called = NO;
  [store_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
          completionHandler:^(ActorFormSuggestion* s, BOOL storePermission) {
            completion_called = YES;
            selected = s;
            stored = storePermission;
          }];

  ASSERT_EQ(observer_.items.count, 1u);
  EXPECT_NSEQ(observer_.items[0].title,
              l10n_util::GetNSString(
                  IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_CREDENTIAL_TITLE));
  ASSERT_NE(observer_.intervention, nil);
  ASSERT_EQ(observer_.intervention.actions.count, 2u);

  [store_ resolveInterventionWithAction:observer_.intervention.actions[0]];
  EXPECT_TRUE(completion_called);
  EXPECT_EQ(selected, nil);
  EXPECT_FALSE(stored);
  EXPECT_EQ(observer_.intervention, nil);
}

// Tests selecting from addresses and allowing autofill.
TEST_F(GeminiActorTaskStoreTest, SelectFromAddressesAllow) {
  AttachAndRegister();
  [observer_.items removeAllObjects];

  ActorFormSuggestion* suggestion = CreateAutofillSuggestion(
      "123 Main St", "Springfield, IL",
      autofill::ActorFormFillingRequestedData::kAddress);

  __block ActorFormSuggestion* selected = nil;
  __block BOOL stored = YES;
  __block BOOL completion_called = NO;
  [store_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
          completionHandler:^(ActorFormSuggestion* s, BOOL storePermission) {
            completion_called = YES;
            selected = s;
            stored = storePermission;
          }];

  ASSERT_EQ(observer_.items.count, 1u);
  ActuationWorklogItem* item = observer_.items[0];
  EXPECT_NSEQ(item.title, l10n_util::GetNSString(
                              IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_ADDRESS_TITLE));
  EXPECT_EQ(item.subtitle, nil);
  EXPECT_EQ(item.style, ActuationWorklogItemStyle::kCard);
  ASSERT_NE(item.accessoryItem, nil);
  EXPECT_NSEQ(item.accessoryItem.title, @"123 Main St");
  EXPECT_NSEQ(item.accessoryItem.detailText, @"Springfield, IL");

  ASSERT_NE(observer_.intervention, nil);
  EXPECT_EQ(observer_.intervention.type, ActuationInterventionType::kRow);
  ASSERT_EQ(observer_.intervention.actions.count, 2u);
  EXPECT_NSEQ(observer_.intervention.actions[0].title,
              l10n_util::GetNSString(
                  IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_MANUAL_FILL_BUTTON));
  EXPECT_NSEQ(
      observer_.intervention.actions[1].title,
      l10n_util::GetNSString(IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_ALLOW_BUTTON));
  EXPECT_FALSE(completion_called);

  [store_ resolveInterventionWithAction:observer_.intervention.actions[1]];
  EXPECT_TRUE(completion_called);
  EXPECT_EQ(selected, suggestion);
  EXPECT_FALSE(stored);
  EXPECT_EQ(observer_.intervention, nil);
}

// Tests the card appended to select from contacts.
TEST_F(GeminiActorTaskStoreTest, SelectFromContacts) {
  AttachAndRegister();
  [observer_.items removeAllObjects];

  ActorFormSuggestion* suggestion = CreateAutofillSuggestion(
      "Jane Doe", "jane@example.com",
      autofill::ActorFormFillingRequestedData::kContactInformation);

  [store_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
          completionHandler:nil];

  ASSERT_EQ(observer_.items.count, 1u);
  ActuationWorklogItem* item = observer_.items[0];
  EXPECT_NSEQ(item.title, l10n_util::GetNSString(
                              IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_CONTACT_TITLE));
  EXPECT_EQ(item.subtitle, nil);
  EXPECT_EQ(item.style, ActuationWorklogItemStyle::kCard);
  ASSERT_NE(item.accessoryItem, nil);
  EXPECT_NSEQ(item.accessoryItem.title, @"Jane Doe");
  EXPECT_NSEQ(item.accessoryItem.detailText, @"jane@example.com");
}

// Tests the card appended to select from payments.
TEST_F(GeminiActorTaskStoreTest, SelectFromPayments) {
  AttachAndRegister();
  [observer_.items removeAllObjects];

  ActorFormSuggestion* suggestion = CreateAutofillSuggestion(
      "Visa", "•••• 1234",
      autofill::ActorFormFillingRequestedData::kCreditCard);

  [store_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
          completionHandler:nil];

  ASSERT_EQ(observer_.items.count, 1u);
  ActuationWorklogItem* item = observer_.items[0];
  EXPECT_NSEQ(item.title, l10n_util::GetNSString(
                              IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_PAYMENT_TITLE));
  EXPECT_NSEQ(item.subtitle,
              l10n_util::GetNSString(
                  IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_PAYMENT_SUBTITLE));
  EXPECT_EQ(item.style, ActuationWorklogItemStyle::kCard);
  ASSERT_NE(item.accessoryItem, nil);
  EXPECT_NSEQ(item.accessoryItem.title, @"Visa");
  EXPECT_NSEQ(item.accessoryItem.detailText, @"•••• 1234");
}

// Tests that federated credentials are displayed as credential cards.
TEST_F(GeminiActorTaskStoreTest, SelectFromFederatedCredentials) {
  AttachAndRegister();
  [observer_.items removeAllObjects];

  ActorFormSuggestion* suggestion =
      CreateCredentialSuggestion(u"user@example.com", u"example.com",
                                 actor_login::CredentialType::kFederated);
  ASSERT_EQ(suggestion.type, autofill::SuggestionType::kIdentityCredential);

  [store_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
          completionHandler:nil];

  ASSERT_EQ(observer_.items.count, 1u);
  EXPECT_NSEQ(observer_.items[0].accessoryItem.title, @"user@example.com");
  ASSERT_NE(observer_.intervention, nil);
  ASSERT_EQ(observer_.intervention.actions.count, 2u);
  EXPECT_NSEQ(
      observer_.intervention.actions[0].title,
      l10n_util::GetNSString(
          IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_CREDENTIAL_MANUAL_FILL_BUTTON));
}

// Tests that selecting another suggestion replaces the suggestions card, and
// that allowing autofill resolves the intervention with it.
TEST_F(GeminiActorTaskStoreTest, SelectSuggestionReplacesCard) {
  AttachAndRegister();
  [observer_.items removeAllObjects];

  ActorFormSuggestion* suggestion1 =
      CreateCredentialSuggestion(u"user1@example.com", u"example.com");
  ActorFormSuggestion* suggestion2 =
      CreateCredentialSuggestion(u"user2@example.com", u"example.com");
  __block ActorFormSuggestion* selected = nil;
  [store_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion1, suggestion2 ]
          completionHandler:^(ActorFormSuggestion* s, BOOL storePermission) {
            selected = s;
          }];
  ASSERT_EQ(observer_.items.count, 1u);
  EXPECT_NSEQ(observer_.items[0].accessoryItem.title, @"user1@example.com");

  [store_ selectSuggestion:suggestion2];

  ASSERT_EQ(observer_.items.count, 1u);
  EXPECT_NSEQ(observer_.items[0].accessoryItem.title, @"user2@example.com");

  // A late observer receives the replaced card as the suggestions card.
  FakeTaskDataSourceObserver* lateObserver =
      [[FakeTaskDataSourceObserver alloc] init];
  [store_ addObserver:lateObserver];
  EXPECT_EQ(lateObserver.items.lastObject, observer_.items[0]);
  EXPECT_EQ(lateObserver.suggestionsItem, observer_.items[0]);

  [store_ resolveInterventionWithAction:observer_.intervention.actions[1]];
  EXPECT_EQ(selected, suggestion2);
  EXPECT_EQ(observer_.suggestions, nil);
}

// Tests that selecting a suggestion while none are selectable is a no-op.
TEST_F(GeminiActorTaskStoreTest, SelectSuggestionWithoutSuggestionsIsNoOp) {
  AttachAndRegister();
  NSArray<ActuationWorklogItem*>* items = [observer_.items copy];

  [store_ selectSuggestion:CreateCredentialSuggestion(u"user@example.com",
                                                      u"example.com")];

  EXPECT_NSEQ(observer_.items, items);
}

// Tests that cancelling a pending suggestion selection dismisses it without
// invoking its completion, so that a new selection can be prompted.
TEST_F(GeminiActorTaskStoreTest, CancelSuggestionSelection) {
  AttachAndRegister();
  ActorFormSuggestion* suggestion =
      CreateCredentialSuggestion(u"user@example.com", u"example.com");
  __block BOOL completion_called = NO;
  [store_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
          completionHandler:^(ActorFormSuggestion* selectedSuggestion,
                              BOOL storePermission) {
            completion_called = YES;
          }];
  ASSERT_NE(observer_.intervention, nil);

  [store_ actorTaskDidCancelSuggestionSelection:task_id_];

  EXPECT_FALSE(completion_called);
  EXPECT_EQ(observer_.intervention, nil);
  EXPECT_EQ(observer_.suggestions, nil);
  EXPECT_EQ(observer_.suggestionsItem, nil);

  [store_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
          completionHandler:nil];
  EXPECT_NE(observer_.intervention, nil);
  ASSERT_EQ(observer_.suggestions.count, 1u);
  EXPECT_EQ(observer_.suggestions.firstObject, suggestion);
}

// Tests that cancelling a suggestion selection leaves a pending user
// intervention untouched.
TEST_F(GeminiActorTaskStoreTest,
       CancelSuggestionSelectionKeepsUserIntervention) {
  AttachAndRegister();
  __block BOOL completion_called = NO;
  [store_ actorTask:task_id_
      requestUserInterventionWithTitle:@"Title"
                              subtitle:@"Subtitle"
                            buttonText:@"Continue"
                     completionHandler:^{
                       completion_called = YES;
                     }];
  ActuationInterventionData* intervention = observer_.intervention;
  ASSERT_NE(intervention, nil);

  [store_ actorTaskDidCancelSuggestionSelection:task_id_];

  EXPECT_EQ(observer_.intervention, intervention);
  [store_
      resolveInterventionWithAction:observer_.intervention.actions.firstObject];
  EXPECT_TRUE(completion_called);
}

// Tests that a user intervention replacing a pending suggestion selection
// drops the suggestions without invoking the selection completion.
TEST_F(GeminiActorTaskStoreTest, UserInterventionDropsPendingSuggestions) {
  AttachAndRegister();
  ActorFormSuggestion* suggestion =
      CreateCredentialSuggestion(u"user@example.com", u"example.com");
  __block BOOL completion_called = NO;
  [store_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
          completionHandler:^(ActorFormSuggestion* selectedSuggestion,
                              BOOL storePermission) {
            completion_called = YES;
          }];
  ASSERT_EQ(observer_.suggestions.count, 1u);

  [store_ actorTask:task_id_
      requestUserInterventionWithTitle:@"Title"
                              subtitle:@"Subtitle"
                            buttonText:@"Continue"
                     completionHandler:nil];

  EXPECT_FALSE(completion_called);
  EXPECT_EQ(observer_.suggestions, nil);
  EXPECT_EQ(observer_.suggestionsItem, nil);
  EXPECT_NSEQ(observer_.intervention.title, @"Title");
}

// Tests that disconnecting drops a pending suggestion selection, so that it is
// not replayed to a later observer.
TEST_F(GeminiActorTaskStoreTest, DisconnectDropsPendingSuggestions) {
  AttachAndRegister();
  ActorFormSuggestion* suggestion =
      CreateCredentialSuggestion(u"user@example.com", u"example.com");
  [store_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
          completionHandler:nil];
  ASSERT_EQ(observer_.suggestions.count, 1u);

  [store_ disconnect];

  FakeTaskDataSourceObserver* lateObserver =
      [[FakeTaskDataSourceObserver alloc] init];
  [store_ addObserver:lateObserver];
  EXPECT_EQ(lateObserver.intervention, nil);
  EXPECT_EQ(lateObserver.suggestions, nil);
}

// Tests that a confirmation interrupt is surfaced as an intervention card and
// that tapping its button resumes the task.
TEST_F(GeminiActorTaskStoreTest, ConfirmationInterruptShowsInterventionCard) {
  AttachAndRegister();
  ASSERT_EQ(RequestConfirmation(task_id_), std::nullopt);

  EXPECT_EQ(actor_service_->GetActiveTaskState(), kWaitingOnUser);
  ASSERT_NE(observer_.intervention, nil);
  EXPECT_NSEQ(observer_.intervention.title, @(kConfirmationTitle));
  EXPECT_NSEQ(observer_.intervention.subtitle, @(kConfirmationMessage));

  [store_
      resolveInterventionWithAction:observer_.intervention.actions.firstObject];
  EXPECT_EQ(observer_.intervention, nil);
  // The intervention completion is posted back to the task.
  EXPECT_EQ(actor_service_->GetActiveTaskState(), kWaitingOnUser);
  actor::FlushCurrentSequence();
  EXPECT_EQ(actor_service_->GetActiveTaskState(), kReflecting);
}

// Tests that disconnecting with a pending intervention leaves the task waiting
// on the user instead of stopping it.
TEST_F(GeminiActorTaskStoreTest, DisconnectKeepsTaskWaitingOnUser) {
  AttachAndRegister();
  ASSERT_EQ(RequestConfirmation(task_id_), std::nullopt);
  ASSERT_NE(observer_.intervention, nil);
  FakeGeminiTaskStateObserver* observer = ObserveTaskState(task_id_);

  [store_ disconnect];
  actor::FlushCurrentSequence();

  EXPECT_FALSE(observer.stopped);
  EXPECT_EQ(observer.state, kWaitingOnUser);
}

// Tests that disconnecting releases the intervention delegate, so a later
// confirmation interrupt is rejected instead of targeting a detached UI.
TEST_F(GeminiActorTaskStoreTest, DisconnectClearsInterventionDelegate) {
  AttachAndRegister();

  [store_ disconnect];
  EXPECT_EQ(RequestConfirmation(task_id_),
            base::unexpected(actor::ActorTaskError::kRejected));
  EXPECT_EQ(observer_.intervention, nil);
}

// Tests that stopping the task cancels it.
TEST_F(GeminiActorTaskStoreTest, StopTaskCancelsTask) {
  AttachAndRegister();
  FakeGeminiTaskStateObserver* observer = ObserveTaskState(task_id_);

  [store_ stopTask];
  actor::FlushCurrentSequence();

  EXPECT_TRUE(observer.stopped);
  EXPECT_EQ(observer.state, kCancelled);
}

// Tests that stopping the task after disconnecting is a no-op.
TEST_F(GeminiActorTaskStoreTest, StopTaskAfterDisconnectIsNoOp) {
  AttachAndRegister();
  FakeGeminiTaskStateObserver* observer = ObserveTaskState(task_id_);

  [store_ disconnect];
  [store_ stopTask];
  actor::FlushCurrentSequence();

  EXPECT_FALSE(observer.stopped);
}

}  // namespace
