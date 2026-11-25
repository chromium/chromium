// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_actor_task_store.h"

#import <memory>
#import <optional>

#import "base/memory/raw_ptr.h"
#import "base/test/run_until.h"
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
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_form_suggestion_favicon_data_source.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/intelligence/actor/util/actor_test_utils.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/ui/intelligence_header_item.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state_id.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

// Records the states published by an `ActorTaskDataSource`.
@interface FakeTaskDataSourceObserver : NSObject <ActorTaskDataSourceObserver>
// Latest published state.
@property(nonatomic, strong, readonly) ActuationWorklogState* state;
// Number of published states, including the one sent on attach.
@property(nonatomic, assign, readonly) NSUInteger updateCount;
// Items of the entries of `state`.
@property(nonatomic, readonly) NSArray<ActuationWorklogItem*>* items;
// Shortcuts to the intervention and suggestions of `state`.
@property(nonatomic, readonly) ActuationInterventionData* intervention;
@property(nonatomic, readonly) NSArray<ActorFormSuggestion*>* suggestions;
@property(nonatomic, readonly) NSInteger selectedSuggestionIndex;
@property(nonatomic, readonly) ActuationWorklogItem* suggestionsItem;
@end

@implementation FakeTaskDataSourceObserver

- (void)taskDataSource:(id<ActorTaskDataSource>)dataSource
        didUpdateState:(ActuationWorklogState*)state {
  _updateCount++;
  _state = state;
}

- (NSArray<ActuationWorklogItem*>*)items {
  return [_state.entries valueForKey:@"item"];
}

- (ActuationInterventionData*)intervention {
  return _state.intervention;
}

- (NSArray<ActorFormSuggestion*>*)suggestions {
  return _state.suggestions;
}

- (NSInteger)selectedSuggestionIndex {
  return _state.selectedSuggestionIndex;
}

- (ActuationWorklogItem*)suggestionsItem {
  return _state.suggestionsEntry.item;
}

@end

// Records the state of the task it observes, as reported by the posted task
// updates.
@interface FakeGeminiTaskStateObserver : NSObject <ActorTaskUpdatesObserver>
@property(nonatomic, assign) actor::ActorTaskState state;
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

@end

// Fake favicon data source for `ActorFormSuggestion` in store tests.
@interface FakeStoreFaviconDataSource
    : NSObject <ActorFormSuggestionFaviconDataSource>
@property(nonatomic, strong) UIImage* icon;
@property(nonatomic, assign, getter=isSynchronous) BOOL synchronous;

// Invokes the pending completion handler with `icon`.
- (void)completeLoad;
@end

@implementation FakeStoreFaviconDataSource {
  ActorFormSuggestionFaviconCompletion _completion;
}

- (void)completeLoad {
  if (_completion) {
    _completion(_icon);
  }
}

- (BOOL)actorFormSuggestion:(ActorFormSuggestion*)suggestion
    loadFaviconWithCompletion:(ActorFormSuggestionFaviconCompletion)completion {
  if (self.isSynchronous) {
    completion(_icon);
    return YES;
  }
  _completion = [completion copy];
  return YES;
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
    const std::u16string& username = u"user@example.com",
    const std::u16string& display_origin = u"example.com",
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

// Returns a no-op suggestion selection callback.
ActorSuggestionSelectionCallback DoNothingSuggestionCallback() {
  return ^(actor::InterventionDelegateResult<actor::SuggestionSelection>) {
  };
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
  std::optional<actor::InterventionResult<actor::InterruptTaskResult>>
  RequestConfirmation(actor::ActorTaskId task_id) {
    actor::ConfirmationInterrupt confirmation;
    confirmation.title = kConfirmationTitle;
    confirmation.subtitle = kConfirmationMessage;
    confirmation.button_text = kConfirmationButtonText;
    base::test::TestFuture<
        actor::InterventionResult<actor::InterruptTaskResult>>
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

  // Returns the header content of the latest published state for `mode`.
  ActuationHeaderContent* HeaderContent(ActuationWorklogDisplayMode mode) {
    return [observer_.state.headerContent contentForDisplayMode:mode];
  }

  // Returns the identifier of the header's primary item in the latest state.
  NSString* HeaderPrimaryItemIdentifier() {
    return HeaderContent(ActuationWorklogDisplayModeCompact)
        .primaryItem.identifier;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<actor::ActorService> actor_service_ = nullptr;
  actor::ActorTaskId task_id_;
  GeminiActorTaskStore* store_;
  FakeTaskDataSourceObserver* observer_;
};

// Tests that an observer attached before the registration receives no state,
// then a single state with the task title and the Gemini first step.
TEST_F(GeminiActorTaskStoreTest, RegistrationPublishesTitleAndFirstStep) {
  [store_ addObserver:observer_];
  EXPECT_EQ(observer_.state, nil);
  EXPECT_EQ(observer_.updateCount, 1u);

  actor::FlushCurrentSequence();

  EXPECT_EQ(observer_.updateCount, 2u);
  EXPECT_EQ(store_.state, observer_.state);
  EXPECT_TRUE(observer_.state.actuationActive);
  EXPECT_NSEQ(HeaderContent(ActuationWorklogDisplayModeCompact).title,
              @(kTaskTitle));
  ASSERT_EQ(observer_.items.count, 1u);
  EXPECT_EQ(observer_.items[0].style, ActuationWorklogItemStyle::kLabeled);
  EXPECT_NSEQ(
      observer_.items[0].title,
      l10n_util::GetNSString(IDS_IOS_GEMINI_FIRST_ACTUATION_STEP_TITLE));
  EXPECT_EQ(observer_.state.entries[0].chip, nil);
}

// Tests that a non-empty initial task update is appended after the Gemini first
// step, in the same state.
TEST_F(GeminiActorTaskStoreTest, InitialTaskUpdateFollowsFirstStep) {
  [store_ addObserver:observer_];

  [store_ didRegisterAsObserverForTaskID:task_id_
                               taskTitle:@(kTaskTitle)
                              taskUpdate:@"Initial update"
                            currentState:kActing
                               webStates:@[]];

  EXPECT_EQ(observer_.updateCount, 2u);
  ASSERT_EQ(observer_.items.count, 2u);
  EXPECT_EQ(observer_.items[0].style, ActuationWorklogItemStyle::kLabeled);
  EXPECT_NSEQ(observer_.items[1].title, @"Initial update");
}

// Tests that tool executions append entries with the chip of their tool.
TEST_F(GeminiActorTaskStoreTest, ToolExecutionAppendsItem) {
  AttachAndRegister();
  const NSUInteger initialCount = observer_.items.count;

  ExecuteTool(kClick, @"Clicking button");

  ASSERT_EQ(observer_.items.count, initialCount + 1);
  EXPECT_NSEQ(observer_.items.lastObject.title, @"Clicking button");
  EXPECT_EQ(observer_.items.lastObject.style,
            ActuationWorklogItemStyle::kSimple);
  EXPECT_NSEQ(observer_.state.entries.lastObject.chip.text,
              l10n_util::GetNSString(IDS_IOS_ACTOR_WORKLOG_CHIP_CLICKING));
}

// Tests that empty and consecutive duplicate task updates are ignored, without
// publishing a state.
TEST_F(GeminiActorTaskStoreTest, IgnoresEmptyAndDuplicateUpdates) {
  AttachAndRegister();
  const NSUInteger initialCount = observer_.items.count;
  const NSUInteger initialUpdateCount = observer_.updateCount;

  ExecuteTool(kType, @"");
  EXPECT_EQ(observer_.items.count, initialCount);
  EXPECT_EQ(observer_.updateCount, initialUpdateCount);

  ExecuteTool(kClick, @"Step A");
  ExecuteTool(kClick, @"Step A");
  EXPECT_EQ(observer_.items.count, initialCount + 1);
  EXPECT_EQ(observer_.updateCount, initialUpdateCount + 1);

  ExecuteTool(kScroll, @"Step B");
  ExecuteTool(kClick, @"Step A");
  EXPECT_EQ(observer_.items.count, initialCount + 3);
}

// Tests that state changes are published.
TEST_F(GeminiActorTaskStoreTest, StateChangeIsPublished) {
  AttachAndRegister();
  EXPECT_NSEQ(HeaderPrimaryItemIdentifier(),
              kActuationHeaderPauseItemIdentifier);

  [store_ actorTaskWithID:task_id_ didChangeState:kFinished fromState:kActing];

  EXPECT_FALSE(observer_.state.actuationActive);
  EXPECT_NSEQ(HeaderPrimaryItemIdentifier(),
              kActuationHeaderCloseItemIdentifier);
  EXPECT_NSEQ(HeaderContent(ActuationWorklogDisplayModeMinimized).title,
              l10n_util::GetNSString(IDS_IOS_ACTOR_HEADER_TASK_DONE));
}

// Tests that the worklog ends with a single task ended item once the task
// reaches a terminal state.
TEST_F(GeminiActorTaskStoreTest, TerminalStateAppendsTaskEnded) {
  AttachAndRegister();
  const NSUInteger item_count = observer_.items.count;

  [store_ actorTaskWithID:task_id_ didChangeState:kFinished fromState:kActing];
  [store_ actorTaskWithID:task_id_
           didChangeState:kCancelled
                fromState:kFinished];

  ASSERT_EQ(observer_.items.count, item_count + 1);
  EXPECT_NSEQ(observer_.items.lastObject.title,
              l10n_util::GetNSString(IDS_IOS_ACTOR_WORKLOG_TASK_ENDED));
}

// Tests that the minimized header shows the latest task update, and shows it
// again once the task runs after a status was displayed.
TEST_F(GeminiActorTaskStoreTest, HeaderTracksCurrentStep) {
  AttachAndRegister();

  ExecuteTool(kClick, @"Clicking button");
  EXPECT_NSEQ(HeaderContent(ActuationWorklogDisplayModeMinimized).subtitle,
              @"Clicking button");

  [store_ actorTaskWithID:task_id_
           didChangeState:kWaitingOnUser
                fromState:kActing];
  EXPECT_NSEQ(HeaderContent(ActuationWorklogDisplayModeMinimized).title,
              l10n_util::GetNSString(IDS_IOS_ACTOR_HEADER_INPUT_NEEDED));

  [store_ actorTaskWithID:task_id_
           didChangeState:kActing
                fromState:kWaitingOnUser];
  EXPECT_NSEQ(HeaderContent(ActuationWorklogDisplayModeMinimized).subtitle,
              @"Clicking button");
}

// Tests that an observer added mid-task receives the latest state, only once if
// added again, and stops receiving updates once removed.
TEST_F(GeminiActorTaskStoreTest, ObserverRegistration) {
  AttachAndRegister();
  ExecuteTool(kClick, @"Step A");

  FakeTaskDataSourceObserver* lateObserver =
      [[FakeTaskDataSourceObserver alloc] init];
  [store_ addObserver:lateObserver];
  EXPECT_EQ(lateObserver.updateCount, 1u);
  EXPECT_EQ(lateObserver.state, observer_.state);

  [store_ addObserver:lateObserver];
  EXPECT_EQ(lateObserver.updateCount, 1u);

  [store_ removeObserver:lateObserver];
  ExecuteTool(kNavigate, @"Step B");
  EXPECT_EQ(lateObserver.updateCount, 1u);
}

// Tests the user intervention flow.
TEST_F(GeminiActorTaskStoreTest, UserInterventionFlow) {
  AttachAndRegister();
  __block BOOL completion_called = NO;
  [store_ actorTask:task_id_
      requestUserInterventionWithTitle:@"Intervention Title"
                              subtitle:@"Intervention Subtitle"
                            buttonText:@"Continue"
                              callback:^(actor::InterventionDelegateResult<void>
                                             result) {
                                EXPECT_TRUE(result.has_value());
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
// completion and suggestions without invoking the completion.
TEST_F(GeminiActorTaskStoreTest, UserInterventionCancellation) {
  AttachAndRegister();
  __block BOOL suggestions_completion_called = NO;
  [store_ actorTask:task_id_
      selectFromSuggestions:@[ CreateCredentialSuggestion() ]
                   callback:^(actor::InterventionDelegateResult<
                              actor::SuggestionSelection> result) {
                     suggestions_completion_called = YES;
                   }];
  ASSERT_NE(observer_.suggestions, nil);

  __block BOOL intervention_completion_called = NO;
  [store_ actorTask:task_id_
      requestUserInterventionWithTitle:@"Title"
                              subtitle:@"Subtitle"
                            buttonText:@"Continue"
                              callback:^(actor::InterventionDelegateResult<void>
                                             result) {
                                intervention_completion_called = YES;
                              }];
  EXPECT_FALSE(suggestions_completion_called);
  EXPECT_EQ(observer_.suggestions, nil);
  EXPECT_EQ(observer_.selectedSuggestionIndex, NSNotFound);
  EXPECT_EQ(observer_.suggestionsItem, nil);
  EXPECT_NSEQ(observer_.intervention.title, @"Title");

  [store_ disconnect];
  EXPECT_FALSE(intervention_completion_called);
}

// Tests that the intervention and its suggestions are cleared in the published
// state before the completion runs, while the completion still receives the
// selected suggestion.
TEST_F(GeminiActorTaskStoreTest, ResolvingClearsInterventionBeforeCompletion) {
  AttachAndRegister();
  ActorFormSuggestion* suggestion = CreateCredentialSuggestion();
  GeminiActorTaskStore* store = store_;
  __block ActorFormSuggestion* resolved_suggestion = nil;
  __block BOOL cleared_in_completion = NO;
  [store_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
                   callback:^(actor::InterventionDelegateResult<
                              actor::SuggestionSelection> result) {
                     EXPECT_TRUE(result.has_value());
                     if (result.has_value()) {
                       resolved_suggestion = result->suggestion;
                     }
                     cleared_in_completion =
                         !store.state.intervention &&
                         !store.state.suggestions &&
                         store.state.selectedSuggestionIndex == NSNotFound;
                   }];

  [store_ resolveInterventionWithAction:observer_.intervention.actions[1]];

  EXPECT_EQ(resolved_suggestion, suggestion);
  EXPECT_TRUE(cleared_in_completion);
}

// Tests selecting from credentials and allowing autofill. Showing and resolving
// the intervention each publish a single state.
TEST_F(GeminiActorTaskStoreTest, SelectFromCredentialsAllow) {
  AttachAndRegister();
  EXPECT_EQ(observer_.selectedSuggestionIndex, NSNotFound);
  const NSUInteger initialUpdateCount = observer_.updateCount;

  ActorFormSuggestion* suggestion = CreateCredentialSuggestion();

  __block ActorFormSuggestion* selected = nil;
  __block BOOL stored = YES;
  [store_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
                   callback:^(actor::InterventionDelegateResult<
                              actor::SuggestionSelection> result) {
                     EXPECT_TRUE(result.has_value());
                     if (result.has_value()) {
                       selected = result->suggestion;
                       stored = result->should_store_permission;
                     }
                   }];

  EXPECT_EQ(observer_.updateCount, initialUpdateCount + 1);
  ActuationWorklogItem* item = observer_.items.lastObject;
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
  EXPECT_EQ(observer_.selectedSuggestionIndex, 0);
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
  EXPECT_EQ(observer_.updateCount, initialUpdateCount + 2);
  EXPECT_EQ(selected, suggestion);
  EXPECT_FALSE(stored);
  EXPECT_EQ(observer_.intervention, nil);
  EXPECT_EQ(observer_.suggestions, nil);
  EXPECT_EQ(observer_.selectedSuggestionIndex, NSNotFound);
  EXPECT_EQ(observer_.suggestionsItem, nil);
}

// Tests selecting from credentials with the manual sign-in action.
TEST_F(GeminiActorTaskStoreTest, SelectFromCredentialsManualSignIn) {
  AttachAndRegister();

  ActorFormSuggestion* suggestion =
      CreateCredentialSuggestion(u"user@example.com", u"");

  __block ActorFormSuggestion* selected = nil;
  __block BOOL stored = YES;
  __block BOOL completion_called = NO;
  [store_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
                   callback:^(actor::InterventionDelegateResult<
                              actor::SuggestionSelection> result) {
                     completion_called = YES;
                     EXPECT_TRUE(result.has_value());
                     if (result.has_value()) {
                       selected = result->suggestion;
                       stored = result->should_store_permission;
                     }
                   }];

  EXPECT_NSEQ(observer_.items.lastObject.title,
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

// Tests the card and intervention appended to select from addresses.
TEST_F(GeminiActorTaskStoreTest, SelectFromAddresses) {
  AttachAndRegister();

  ActorFormSuggestion* suggestion = CreateAutofillSuggestion(
      "123 Main St", "Springfield, IL",
      autofill::ActorFormFillingRequestedData::kAddress);

  [store_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
                   callback:DoNothingSuggestionCallback()];

  ActuationWorklogItem* item = observer_.items.lastObject;
  EXPECT_NSEQ(item.title, l10n_util::GetNSString(
                              IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_ADDRESS_TITLE));
  EXPECT_EQ(item.subtitle, nil);
  EXPECT_EQ(item.style, ActuationWorklogItemStyle::kCard);
  ASSERT_NE(item.accessoryItem, nil);
  EXPECT_NSEQ(item.accessoryItem.title, @"123 Main St");
  EXPECT_NSEQ(item.accessoryItem.detailText, @"Springfield, IL");

  ASSERT_NE(observer_.intervention, nil);
  ASSERT_EQ(observer_.intervention.actions.count, 2u);
  EXPECT_NSEQ(observer_.intervention.actions[0].title,
              l10n_util::GetNSString(
                  IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_MANUAL_FILL_BUTTON));
}

// Tests the card appended to select from contacts.
TEST_F(GeminiActorTaskStoreTest, SelectFromContacts) {
  AttachAndRegister();

  ActorFormSuggestion* suggestion = CreateAutofillSuggestion(
      "Jane Doe", "jane@example.com",
      autofill::ActorFormFillingRequestedData::kContactInformation);

  [store_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
                   callback:DoNothingSuggestionCallback()];

  ActuationWorklogItem* item = observer_.items.lastObject;
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

  ActorFormSuggestion* suggestion = CreateAutofillSuggestion(
      "Visa", "•••• 1234",
      autofill::ActorFormFillingRequestedData::kCreditCard);

  [store_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
                   callback:DoNothingSuggestionCallback()];

  ActuationWorklogItem* item = observer_.items.lastObject;
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

// Tests that selecting another suggestion replaces only the suggestions card in
// a single state, and that allowing autofill resolves the intervention with it.
TEST_F(GeminiActorTaskStoreTest, SelectSuggestionReplacesCard) {
  AttachAndRegister();

  ActorFormSuggestion* suggestion1 =
      CreateCredentialSuggestion(u"user1@example.com");
  ActorFormSuggestion* suggestion2 =
      CreateCredentialSuggestion(u"user2@example.com");
  __block ActorFormSuggestion* selected = nil;
  [store_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion1, suggestion2 ]
                   callback:^(actor::InterventionDelegateResult<
                              actor::SuggestionSelection> result) {
                     EXPECT_TRUE(result.has_value());
                     if (result.has_value()) {
                       selected = result->suggestion;
                     }
                   }];
  ActuationWorklogEntry* first_entry = observer_.state.entries.firstObject;
  const NSUInteger item_count = observer_.items.count;
  const NSUInteger update_count = observer_.updateCount;
  NSArray<ActorFormSuggestion*>* suggestions = observer_.suggestions;
  EXPECT_NSEQ(observer_.items.lastObject.accessoryItem.title,
              @"user1@example.com");
  EXPECT_EQ(observer_.selectedSuggestionIndex, 0);

  [store_ selectSuggestionAtIndex:1];

  EXPECT_EQ(observer_.updateCount, update_count + 1);
  ASSERT_EQ(observer_.items.count, item_count);
  EXPECT_EQ(observer_.state.entries.firstObject, first_entry);
  EXPECT_NSEQ(observer_.items.lastObject.accessoryItem.title,
              @"user2@example.com");
  // The replaced card has no chip, and the suggestions now target it.
  EXPECT_EQ(observer_.state.entries.lastObject.chip, nil);
  EXPECT_EQ(observer_.suggestionsItem, observer_.items.lastObject);
  EXPECT_EQ(observer_.suggestions, suggestions);
  EXPECT_EQ(observer_.selectedSuggestionIndex, 1);

  [store_ resolveInterventionWithAction:observer_.intervention.actions[1]];
  EXPECT_EQ(selected, suggestion2);
  EXPECT_EQ(observer_.suggestions, nil);
  EXPECT_EQ(observer_.selectedSuggestionIndex, NSNotFound);
}

// Tests that selecting a suggestion while none are selectable is a no-op,
// without publishing a state.
TEST_F(GeminiActorTaskStoreTest, SelectSuggestionWithoutSuggestionsIsNoOp) {
  AttachAndRegister();
  NSArray<ActuationWorklogItem*>* items = observer_.items;
  const NSUInteger update_count = observer_.updateCount;

  [store_ selectSuggestionAtIndex:0];

  EXPECT_NSEQ(observer_.items, items);
  EXPECT_EQ(observer_.updateCount, update_count);
}

// Tests that cancelling a pending suggestion selection dismisses it and its
// card without invoking its completion, so that a new selection can be
// prompted without duplicating the card.
TEST_F(GeminiActorTaskStoreTest, CancelSuggestionSelection) {
  AttachAndRegister();
  const NSUInteger item_count = observer_.items.count;
  ActorFormSuggestion* suggestion = CreateCredentialSuggestion();
  __block BOOL completion_called = NO;
  [store_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
                   callback:^(actor::InterventionDelegateResult<
                              actor::SuggestionSelection> result) {
                     completion_called = YES;
                   }];
  ASSERT_NE(observer_.intervention, nil);
  ASSERT_EQ(observer_.items.count, item_count + 1);

  [store_ actorTaskDidCancelSuggestionSelection:task_id_];

  EXPECT_FALSE(completion_called);
  EXPECT_EQ(observer_.intervention, nil);
  EXPECT_EQ(observer_.suggestions, nil);
  EXPECT_EQ(observer_.suggestionsItem, nil);
  EXPECT_EQ(observer_.items.count, item_count);

  [store_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
                   callback:DoNothingSuggestionCallback()];
  EXPECT_NE(observer_.intervention, nil);
  ASSERT_EQ(observer_.suggestions.count, 1u);
  EXPECT_EQ(observer_.suggestions.firstObject, suggestion);
  EXPECT_EQ(observer_.items.count, item_count + 1);
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
                              callback:^(actor::InterventionDelegateResult<void>
                                             result) {
                                EXPECT_TRUE(result.has_value());
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

// Tests that a synchronously loaded credential favicon is displayed on the
// initial card without replacing it afterward.
TEST_F(GeminiActorTaskStoreTest,
       SelectFromCredentialsUsesSynchronouslyLoadedFavicon) {
  AttachAndRegister();
  const NSUInteger initial_item_count = observer_.items.count;
  const NSUInteger initial_update_count = observer_.updateCount;

  FakeStoreFaviconDataSource* favicon_source =
      [[FakeStoreFaviconDataSource alloc] init];
  UIImage* favicon = [[UIImage alloc] init];
  favicon_source.icon = favicon;
  favicon_source.synchronous = YES;

  ActorFormSuggestion* suggestion =
      CreateCredentialSuggestion(u"user@example.com", u"example.com");
  suggestion.faviconDataSource = favicon_source;

  [store_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
                   callback:DoNothingSuggestionCallback()];

  EXPECT_EQ(observer_.updateCount, initial_update_count + 1);
  ASSERT_EQ(observer_.items.count, initial_item_count + 1);
  EXPECT_EQ(observer_.items.lastObject.accessoryItem.icon, favicon);
}

// Tests that an asynchronously loaded credential favicon replaces the initial
// globe fallback icon on the active card and updates `suggestionsItem`.
TEST_F(GeminiActorTaskStoreTest,
       SelectFromCredentialsUpdatesFaviconAsynchronously) {
  AttachAndRegister();

  FakeStoreFaviconDataSource* favicon_source =
      [[FakeStoreFaviconDataSource alloc] init];
  UIImage* loaded_favicon = [[UIImage alloc] init];
  favicon_source.icon = loaded_favicon;

  ActorFormSuggestion* suggestion =
      CreateCredentialSuggestion(u"user@example.com", u"example.com");
  suggestion.faviconDataSource = favicon_source;

  [store_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion ]
                   callback:DoNothingSuggestionCallback()];

  const NSUInteger item_count = observer_.items.count;
  const NSUInteger update_count = observer_.updateCount;
  ActuationWorklogItem* initial_item = observer_.items.lastObject;
  EXPECT_NSEQ(
      initial_item.accessoryItem.icon,
      SymbolWithConfiguration(SymbolGlobeAmericas,
                              initial_item.accessoryItem.icon.configuration));

  [favicon_source completeLoad];

  EXPECT_EQ(observer_.updateCount, update_count + 1);
  ASSERT_EQ(observer_.items.count, item_count);
  EXPECT_NE(observer_.items.lastObject, initial_item);
  EXPECT_EQ(observer_.items.lastObject.accessoryItem.icon, loaded_favicon);
  EXPECT_EQ(observer_.suggestionsItem, observer_.items.lastObject);

  FakeTaskDataSourceObserver* late_observer =
      [[FakeTaskDataSourceObserver alloc] init];
  [store_ addObserver:late_observer];
  EXPECT_EQ(late_observer.items.lastObject.accessoryItem.icon, loaded_favicon);
  EXPECT_EQ(late_observer.suggestionsItem, late_observer.items.lastObject);
}

// Tests that stale asynchronous favicon completions arriving after reselecting
// another suggestion or resolving the intervention are ignored.
TEST_F(GeminiActorTaskStoreTest,
       SelectFromCredentialsIgnoresStaleAsyncFaviconUpdates) {
  AttachAndRegister();

  FakeStoreFaviconDataSource* favicon_source1 =
      [[FakeStoreFaviconDataSource alloc] init];
  favicon_source1.icon = [[UIImage alloc] init];
  FakeStoreFaviconDataSource* favicon_source2 =
      [[FakeStoreFaviconDataSource alloc] init];
  favicon_source2.icon = [[UIImage alloc] init];
  ActorFormSuggestion* suggestion1 =
      CreateCredentialSuggestion(u"user1@example.com", u"example.com");
  suggestion1.faviconDataSource = favicon_source1;
  ActorFormSuggestion* suggestion2 =
      CreateCredentialSuggestion(u"user2@example.com", u"example.com");
  suggestion2.faviconDataSource = favicon_source2;

  [store_ actorTask:task_id_
      selectFromSuggestions:@[ suggestion1, suggestion2 ]
                   callback:DoNothingSuggestionCallback()];

  [store_ selectSuggestionAtIndex:1];
  const NSUInteger update_count_after_select = observer_.updateCount;
  ActuationWorklogItem* second_item = observer_.items.lastObject;
  EXPECT_NSEQ(second_item.accessoryItem.title, @"user2@example.com");

  // Stale completion for `suggestion1` does not overwrite `suggestion2`'s card.
  [favicon_source1 completeLoad];
  EXPECT_EQ(observer_.updateCount, update_count_after_select);
  EXPECT_EQ(observer_.items.lastObject, second_item);

  // Resolving the intervention ignores a subsequent completion for
  // `suggestion2`.
  [store_ resolveInterventionWithAction:observer_.intervention.actions[1]];
  const NSUInteger update_count_after_resolve = observer_.updateCount;
  [favicon_source2 completeLoad];
  EXPECT_EQ(observer_.updateCount, update_count_after_resolve);
  EXPECT_EQ(observer_.items.lastObject, second_item);
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

// Tests that a clarification interrupt is answered with `kDelegateUnsupported`
// without showing an intervention, and that the task resumes.
TEST_F(GeminiActorTaskStoreTest, ClarificationInterruptIsUnsupported) {
  AttachAndRegister();
  base::test::TestFuture<actor::InterventionResult<actor::InterruptTaskResult>>
      result;
  actor_service_->InterruptTask(
      task_id_, actor::ClarificationInterrupt{.message = "Which size?"},
      result.GetCallback());

  ASSERT_FALSE(result.Get().has_value());
  EXPECT_EQ(result.Get().error(),
            actor::InterventionError::kDelegateUnsupported);
  EXPECT_EQ(observer_.intervention, nil);
  EXPECT_EQ(actor_service_->GetActiveTaskState(), kReflecting);
}

// Tests that pausing the task shows a card resuming the task, without adding a
// worklog entry, and that tapping its button resumes the task.
TEST_F(GeminiActorTaskStoreTest, PauseShowsResumeCard) {
  AttachAndRegister();
  const NSUInteger item_count = observer_.items.count;
  actor_service_->PauseTask(task_id_);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return observer_.intervention != nil; }));
  EXPECT_EQ(actor_service_->GetActiveTaskState(), kPausedByUser);

  // Drain remaining notifications before checking that no entry is added.
  actor::FlushCurrentSequence();
  EXPECT_EQ(observer_.items.count, item_count);
  EXPECT_NSEQ(HeaderPrimaryItemIdentifier(),
              kActuationHeaderCloseItemIdentifier);
  EXPECT_EQ(observer_.intervention.type, ActuationInterventionType::kCard);
  EXPECT_NSEQ(observer_.intervention.title, @"Task paused");
  EXPECT_NSEQ(observer_.intervention.subtitle, @(kTaskTitle));
  ASSERT_EQ(observer_.intervention.actions.count, 1u);
  EXPECT_NSEQ(observer_.intervention.actions[0].title, @"Resume task");

  [store_ resolveInterventionWithAction:observer_.intervention.actions[0]];
  EXPECT_EQ(observer_.intervention, nil);
  // The resume is enqueued as a command.
  actor::FlushCurrentSequence();
  EXPECT_EQ(actor_service_->GetActiveTaskState(), kReflecting);
}

// Tests that the resume card is discarded when the task is resumed from outside
// the worklog.
TEST_F(GeminiActorTaskStoreTest, ResumeFromServiceDiscardsResumeCard) {
  AttachAndRegister();
  actor_service_->PauseTask(task_id_);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return observer_.intervention != nil; }));

  actor_service_->ResumeTask(task_id_);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return observer_.intervention == nil; }));
  EXPECT_EQ(actor_service_->GetActiveTaskState(), kReflecting);
}

// Tests that the resume card is discarded when the paused task stops.
TEST_F(GeminiActorTaskStoreTest, StopDiscardsResumeCard) {
  AttachAndRegister();
  actor_service_->PauseTask(task_id_);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return observer_.intervention != nil; }));

  [store_ stopTask];
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return observer_.intervention == nil; }));
  EXPECT_FALSE(observer_.state.actuationActive);
}

// Tests that stopping the task drops a pending suggestion selection and its
// unanswered card without invoking its completion.
TEST_F(GeminiActorTaskStoreTest, StopDiscardsPendingSuggestionCard) {
  AttachAndRegister();
  const NSUInteger item_count = observer_.items.count;
  __block BOOL completion_called = NO;
  [store_ actorTask:task_id_
      selectFromSuggestions:@[ CreateCredentialSuggestion() ]
                   callback:^(actor::InterventionDelegateResult<
                              actor::SuggestionSelection> result) {
                     completion_called = YES;
                   }];
  ASSERT_NE(observer_.intervention, nil);
  ASSERT_EQ(observer_.items.count, item_count + 1);

  [store_ stopTask];
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return observer_.intervention == nil; }));
  EXPECT_EQ(observer_.suggestions, nil);
  // The card is replaced by the task ended item.
  ASSERT_EQ(observer_.items.count, item_count + 1);
  EXPECT_NSEQ(observer_.items.lastObject.title,
              l10n_util::GetNSString(IDS_IOS_ACTOR_WORKLOG_TASK_ENDED));
  EXPECT_FALSE(completion_called);
}

// Tests that a store registering on a task already paused shows the resume
// card.
TEST_F(GeminiActorTaskStoreTest, RegisterOnPausedTaskShowsResumeCard) {
  [store_ disconnect];
  actor_service_->PauseTask(task_id_);
  actor::FlushCurrentSequence();
  ASSERT_EQ(actor_service_->GetActiveTaskState(), kPausedByUser);

  store_ = [[GeminiActorTaskStore alloc] initWithTaskID:task_id_
                                           actorService:actor_service_];
  AttachAndRegister();
  ASSERT_NE(observer_.intervention, nil);
  EXPECT_NSEQ(observer_.intervention.title, @"Task paused");
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

  EXPECT_EQ(observer.state, kWaitingOnUser);
}

// Tests that disconnecting clears the state and releases the intervention
// delegate, so a later confirmation interrupt is rejected.
TEST_F(GeminiActorTaskStoreTest, DisconnectClearsInterventionDelegate) {
  AttachAndRegister();

  [store_ disconnect];
  EXPECT_EQ(store_.state, nil);
  EXPECT_EQ(RequestConfirmation(task_id_),
            base::unexpected(actor::InterventionError::kTaskRejected));
}

// Tests that stopping the task cancels it, and is a no-op once disconnected.
TEST_F(GeminiActorTaskStoreTest, StopTaskCancelsTask) {
  AttachAndRegister();
  FakeGeminiTaskStateObserver* observer = ObserveTaskState(task_id_);

  [store_ disconnect];
  [store_ stopTask];
  actor::FlushCurrentSequence();
  EXPECT_EQ(observer.state, kInit);

  store_ = [[GeminiActorTaskStore alloc] initWithTaskID:task_id_
                                           actorService:actor_service_];
  [store_ stopTask];
  actor::FlushCurrentSequence();
  EXPECT_EQ(observer.state, kCancelled);
}

// Tests that pausing the task while a confirmation is pending replaces its card
// with the paused card.
TEST_F(GeminiActorTaskStoreTest, PauseTaskWhileConfirmingShowsResumeCard) {
  AttachAndRegister();
  ASSERT_EQ(RequestConfirmation(task_id_), std::nullopt);
  ASSERT_NSEQ(observer_.intervention.title, @(kConfirmationTitle));
  ASSERT_NSEQ(HeaderPrimaryItemIdentifier(),
              kActuationHeaderPauseItemIdentifier);

  [store_ pauseTask];
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return [HeaderPrimaryItemIdentifier()
        isEqualToString:kActuationHeaderCloseItemIdentifier];
  }));

  EXPECT_EQ(actor_service_->GetActiveTaskState(), kPausedByUser);
  EXPECT_NSEQ(observer_.intervention.title, @"Task paused");
}

// Tests that pausing the task after disconnecting is a no-op.
TEST_F(GeminiActorTaskStoreTest, PauseTaskAfterDisconnectIsNoOp) {
  AttachAndRegister();
  FakeGeminiTaskStateObserver* observer = ObserveTaskState(task_id_);

  [store_ disconnect];
  [store_ pauseTask];
  actor::FlushCurrentSequence();

  EXPECT_NE(observer.state, kPausedByUser);
  EXPECT_NE(actor_service_->GetActiveTaskState(), kPausedByUser);
}

// Tests the navigation confirmation flow when the user taps "Continue".
TEST_F(GeminiActorTaskStoreTest, NavigationConfirmationContinue) {
  AttachAndRegister();
  const NSUInteger item_count = observer_.items.count;
  __block std::optional<actor::InterventionDelegateResult<bool>> result;
  [store_ actorTask:task_id_
      requestNavigationConfirmationForDomain:@"healthequity.com"
                                    callback:^(
                                        actor::InterventionDelegateResult<bool>
                                            callback_result) {
                                      result = callback_result;
                                    }];

  ASSERT_EQ(observer_.items.count, item_count + 1);
  ActuationWorklogItem* item = observer_.items.lastObject;
  ASSERT_NE(item, nil);
  EXPECT_EQ(item.style, ActuationWorklogItemStyle::kLabeled);
  EXPECT_NSEQ(item.title,
              l10n_util::GetNSStringF(
                  IDS_IOS_ACTOR_WORKLOG_NAVIGATION_CONFIRMATION_TITLE,
                  u"healthequity.com"));
  EXPECT_NSEQ(item.subtitle,
              l10n_util::GetNSString(
                  IDS_IOS_ACTOR_WORKLOG_NAVIGATION_CONFIRMATION_SUBTITLE));

  ASSERT_NE(observer_.intervention, nil);
  EXPECT_EQ(observer_.intervention.type, ActuationInterventionType::kRow);
  ASSERT_EQ(observer_.intervention.actions.count, 2u);
  EXPECT_EQ(observer_.intervention.actions[0].identifier,
            InterventionActionIdentifier::kDecline);
  EXPECT_EQ(observer_.intervention.actions[1].identifier,
            InterventionActionIdentifier::kContinue);

  // Tap "Continue" (index 1). The answered entry stays in the worklog.
  [store_ resolveInterventionWithAction:observer_.intervention.actions[1]];
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, base::ok(true));
  EXPECT_EQ(observer_.intervention, nil);
  EXPECT_EQ(observer_.items.count, item_count + 1);
}

// Tests the navigation confirmation flow when the user taps "Don't use this
// site".
TEST_F(GeminiActorTaskStoreTest, NavigationConfirmationDecline) {
  AttachAndRegister();
  const NSUInteger item_count = observer_.items.count;
  __block std::optional<actor::InterventionDelegateResult<bool>> result;
  [store_ actorTask:task_id_
      requestNavigationConfirmationForDomain:@"healthequity.com"
                                    callback:^(
                                        actor::InterventionDelegateResult<bool>
                                            callback_result) {
                                      result = callback_result;
                                    }];

  ASSERT_EQ(observer_.items.count, item_count + 1);
  ASSERT_NE(observer_.intervention, nil);
  ASSERT_EQ(observer_.intervention.actions.count, 2u);

  // Tap "Don't use this site" (index 0). The answered entry stays in the
  // worklog.
  [store_ resolveInterventionWithAction:observer_.intervention.actions[0]];
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, base::ok(false));
  EXPECT_EQ(observer_.intervention, nil);
  EXPECT_EQ(observer_.items.count, item_count + 1);
}

// Tests that stopping the task drops a pending navigation confirmation and
// removes its unanswered worklog entry without invoking its callback.
TEST_F(GeminiActorTaskStoreTest,
       StopDiscardsPendingNavigationConfirmationEntry) {
  AttachAndRegister();
  const NSUInteger item_count = observer_.items.count;
  __block BOOL callback_called = NO;
  [store_ actorTask:task_id_
      requestNavigationConfirmationForDomain:@"healthequity.com"
                                    callback:^(
                                        actor::InterventionDelegateResult<
                                            bool>) {
                                      callback_called = YES;
                                    }];
  ASSERT_NE(observer_.intervention, nil);
  ASSERT_EQ(observer_.items.count, item_count + 1);

  [store_ stopTask];
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return observer_.intervention == nil; }));
  // The unanswered confirmation entry is removed and replaced by the task ended
  // item.
  ASSERT_EQ(observer_.items.count, item_count + 1);
  EXPECT_NSEQ(observer_.items.lastObject.title,
              l10n_util::GetNSString(IDS_IOS_ACTOR_WORKLOG_TASK_ENDED));
  EXPECT_FALSE(callback_called);
}

// Tests that pausing the task drops a pending navigation confirmation and
// removes its unanswered worklog entry without invoking its callback.
TEST_F(GeminiActorTaskStoreTest,
       PauseDiscardsPendingNavigationConfirmationEntry) {
  AttachAndRegister();
  const NSUInteger item_count = observer_.items.count;
  __block BOOL callback_called = NO;
  [store_ actorTask:task_id_
      requestNavigationConfirmationForDomain:@"healthequity.com"
                                    callback:^(
                                        actor::InterventionDelegateResult<
                                            bool>) {
                                      callback_called = YES;
                                    }];
  ASSERT_NE(observer_.intervention, nil);
  ASSERT_EQ(observer_.items.count, item_count + 1);

  [store_ pauseTask];
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return [HeaderPrimaryItemIdentifier()
        isEqualToString:kActuationHeaderCloseItemIdentifier];
  }));
  EXPECT_NSEQ(observer_.intervention.title, @"Task paused");
  EXPECT_EQ(observer_.items.count, item_count);
  EXPECT_FALSE(callback_called);
}

}  // namespace
