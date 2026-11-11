// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_actor_task_store.h"

#import <optional>

#import "base/check.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/memory/raw_ptr.h"
#import "base/not_fatal_until.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/crb_protocol_observers_util.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_form_suggestion.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data_builders.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Point size of the SF symbols of the first step and card items.
constexpr CGFloat kIconSize = 16.0;

// Icon size for SF symbols in accessory items.
constexpr CGFloat kAccessoryIconSize = 24.0;

// Masked password field to be displayed in credential selection card.
NSString* const kMaskedPassword = @"••••••••";

// Completion handler for suggestion selection interventions.
using SuggestionSelectionCompletionHandler = void (
    ^)(ActorFormSuggestion* selectedSuggestion, BOOL shouldStorePermission);

// Creates an `ActuationWorklogItem` card for a non-federated credential
// suggestion.
ActuationWorklogItem* CardItemForNonFederatedCredential(
    FormSuggestion* suggestion) {
  NSString* username = suggestion.value;
  NSString* site = suggestion.displayDescription;
  UIImage* accessoryIcon = suggestion.icon;
  if (!accessoryIcon) {
    // TODO(crbug.com/571139088): Use favicon loader to update favicon.
    accessoryIcon =
        SymbolWithPointSize(SymbolPersonCropCircle, kAccessoryIconSize);
  }

  NSString* cardTitle =
      site.length > 0
          ? l10n_util::GetNSStringF(
                IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_CREDENTIAL_TITLE_WITH_SITE,
                base::SysNSStringToUTF16(site))
          : l10n_util::GetNSString(
                IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_CREDENTIAL_TITLE);
  NSString* cardSubtitle = l10n_util::GetNSString(
      IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_CREDENTIAL_SUBTITLE);

  ActuationWorklogAccessoryItem* credentialAccessory =
      [[ActuationWorklogAccessoryItem alloc] initWithIcon:accessoryIcon
                                                    title:username ?: @""
                                                 subtitle:nil
                                               detailText:kMaskedPassword
                                               hasChevron:NO];

  return [ActuationWorklogItem
      cardItemWithTitle:cardTitle
               subtitle:cardSubtitle
                   icon:SymbolWithPointSize(SymbolPassword, kIconSize)
                 active:YES
          accessoryItem:credentialAccessory];
}

// Creates an `ActuationWorklogItem` card for an address suggestion.
ActuationWorklogItem* CardItemForAddress(FormSuggestion* suggestion) {
  NSString* title = suggestion.value;
  NSString* description = suggestion.displayDescription;

  UIImage* accessoryIcon = nil;
  switch (suggestion.suggestionIconType) {
    case SuggestionIconType::kAccountHome:
      accessoryIcon = SymbolWithPointSize(SymbolHome, kAccessoryIconSize);
      break;
    case SuggestionIconType::kAccountWork:
      accessoryIcon = SymbolWithPointSize(SymbolWork, kAccessoryIconSize);
      break;
    case SuggestionIconType::kContact:
      accessoryIcon =
          SymbolWithPointSize(SymbolPersonCropCircle, kAccessoryIconSize);
      break;
    case SuggestionIconType::kNone:
      accessoryIcon = SymbolWithPointSize(SymbolLocation, kAccessoryIconSize);
      break;
    case SuggestionIconType::kUndoAutofill:
    case SuggestionIconType::kBackupPassword:
      NOTREACHED(base::NotFatalUntil::M165);
      break;
  }

  ActuationWorklogAccessoryItem* addressAccessory =
      [[ActuationWorklogAccessoryItem alloc]
          initWithIcon:accessoryIcon
                 title:title ?: @""
              subtitle:nil
            detailText:description.length > 0 ? description : nil
            hasChevron:NO];

  int cardTitleId =
      (suggestion.suggestionIconType == SuggestionIconType::kContact)
          ? IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_CONTACT_TITLE
          : IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_ADDRESS_TITLE;

  return [ActuationWorklogItem
      cardItemWithTitle:l10n_util::GetNSString(cardTitleId)
               subtitle:nil
                   icon:nil
                 active:YES
          accessoryItem:addressAccessory];
}

// Creates an `ActuationWorklogItem` card for a credit card suggestion.
ActuationWorklogItem* CardItemForCreditCard(FormSuggestion* suggestion) {
  NSString* title = suggestion.value;
  NSString* description = suggestion.displayDescription;

  UIImage* accessoryIcon = suggestion.icon;
  if (!accessoryIcon) {
    accessoryIcon = SymbolWithPointSize(SymbolCreditCard, kAccessoryIconSize);
  }

  NSString* cardTitle =
      l10n_util::GetNSString(IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_PAYMENT_TITLE);
  NSString* cardSubtitle = l10n_util::GetNSString(
      IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_PAYMENT_SUBTITLE);

  ActuationWorklogAccessoryItem* paymentAccessory =
      [[ActuationWorklogAccessoryItem alloc]
          initWithIcon:accessoryIcon
                 title:title ?: @""
              subtitle:nil
            detailText:description.length > 0 ? description : nil
            hasChevron:NO];

  return [ActuationWorklogItem
      cardItemWithTitle:cardTitle
               subtitle:cardSubtitle
                   icon:SymbolWithPointSize(SymbolCreditCard, kIconSize)
                 active:YES
          accessoryItem:paymentAccessory];
}

// Creates an `ActuationWorklogItem` card for `suggestion`.
ActuationWorklogItem* CardItemForSuggestion(ActorFormSuggestion* suggestion) {
  switch (suggestion.type) {
    case autofill::SuggestionType::kPasswordEntry:
    case autofill::SuggestionType::kIdentityCredential:
      return CardItemForNonFederatedCredential(suggestion.formSuggestion);
    case autofill::SuggestionType::kAddressEntry:
      return CardItemForAddress(suggestion.formSuggestion);
    case autofill::SuggestionType::kCreditCardEntry:
      return CardItemForCreditCard(suggestion.formSuggestion);
    default:
      NOTREACHED(base::NotFatalUntil::M165);
      return nil;
  }
}

}  // namespace

@implementation GeminiActorTaskStore {
  // Null once disconnected or if the task couldn't be observed.
  raw_ptr<actor::ActorService> _actorService;
  // Observers of the worklog.
  CRBProtocolObservers<ActorTaskDataSourceObserver>* _observers;
  // Title of the task.
  NSString* _taskTitle;
  // Current state of the task.
  actor::ActorTaskState _taskState;
  // Entries of the worklog, in display order.
  NSMutableArray<ActuationWorklogEntry*>* _entries;
  // Latest appended task update, used to drop consecutive duplicates and shown
  // as the current step in the header.
  NSString* _latestTaskUpdate;
  // Pending intervention, if any.
  ActuationInterventionData* _intervention;
  // Completion of `_intervention`, called with the identifier of the action the
  // user resolves it with.
  void (^_interventionCompletion)(InterventionActionIdentifier);
  // Suggestions selectable for `_suggestionsEntry`, if any.
  NSArray<ActorFormSuggestion*>* _suggestions;
  // Entry for which `_suggestions` are selectable.
  ActuationWorklogEntry* _suggestionsEntry;
  // Suggestion of `_suggestions` resolved by the "Allow" action.
  ActorFormSuggestion* _selectedSuggestion;
  // Latest published state, `nil` until the task is registered.
  ActuationWorklogState* _state;
}

- (instancetype)initWithTaskID:(actor::ActorTaskId)taskID
                  actorService:(actor::ActorService*)actorService {
  self = [super init];
  if (self) {
    _taskID = taskID;
    _taskState = actor::ActorTaskState::kInit;
    _observers =
        static_cast<CRBProtocolObservers<ActorTaskDataSourceObserver>*>(
            [CRBProtocolObservers
                observersWithProtocol:@protocol(ActorTaskDataSourceObserver)]);
    _entries = [NSMutableArray array];
    if (actorService && actorService->AddTaskUpdatesObserver(taskID, self)) {
      _actorService = actorService;
      _actorService->SetTaskInterventionDelegate(taskID, self);
    }
  }
  return self;
}

- (void)disconnect {
  // TODO(crbug.com/567035965): A pending intervention or suggestion selection
  // is dropped without an answer, leaving the task in `kWaitingOnUser`.
  // Re-surface it once the store outlives the container.
  if (_actorService) {
    _actorService->SetTaskInterventionDelegate(_taskID, nil);
    _actorService->RemoveTaskUpdatesObserver(_taskID, self);
    _actorService = nullptr;
  }
  _selectedSuggestion = nil;
  [self discardIntervention];
  // The store is discarded after disconnecting, so nothing is published, but
  // the dropped intervention must not be replayed to a later observer.
  _state = nil;
}

#pragma mark - ActorTaskDataSource

- (ActuationWorklogState*)state {
  return _state;
}

- (void)addObserver:(id<ActorTaskDataSourceObserver>)observer {
  CHECK(observer);
  if (actor::ContainsObserver(_observers, observer)) {
    return;
  }
  [_observers addObserver:observer];
  [observer taskDataSource:self didUpdateState:_state];
}

- (void)removeObserver:(id<ActorTaskDataSourceObserver>)observer {
  [_observers removeObserver:observer];
}

#pragma mark - ActorTaskUpdatesObserver

- (void)didRegisterAsObserverForTaskID:(actor::ActorTaskId)taskID
                             taskTitle:(NSString*)taskTitle
                            taskUpdate:(NSString*)taskUpdate
                          currentState:(actor::ActorTaskState)state
                             webStates:(NSArray<NSNumber*>*)webStatesIDs {
  _taskTitle = [taskTitle copy];
  _taskState = state;

  ActuationWorklogItem* firstItem = [ActuationWorklogItem
      labeledItemWithTitle:l10n_util::GetNSString(
                               IDS_IOS_GEMINI_FIRST_ACTUATION_STEP_TITLE)
                  subtitle:l10n_util::GetNSString(
                               IDS_IOS_GEMINI_FIRST_ACTUATION_STEP_SUBTITLE)
                      icon:SymbolWithPointSize(SymbolPlayFill, kIconSize)
                    active:YES];
  [self appendItem:firstItem toolType:std::nullopt];

  // The initial task update follows the Gemini first step.
  [self appendTaskUpdate:taskUpdate toolType:std::nullopt];
  [self publishState];
}

- (void)actorTaskWithID:(actor::ActorTaskId)taskID
         didChangeState:(actor::ActorTaskState)newState
              fromState:(actor::ActorTaskState)oldState {
  _taskState = newState;
  [self publishState];
}

- (void)actorTaskWithID:(actor::ActorTaskId)taskID
        willExecuteTool:(actor::ToolType)toolType
             taskUpdate:(NSString*)taskUpdate
             onWebState:(web::WebStateID)webStateID {
  if ([self appendTaskUpdate:taskUpdate toolType:toolType]) {
    [self publishState];
  }
}

#pragma mark - ActorTaskInterventionDelegate

- (void)actorTask:(actor::ActorTaskId)taskID
    selectFromSuggestions:(NSArray<ActorFormSuggestion*>*)suggestions
        completionHandler:
            (void (^)(ActorFormSuggestion* selectedSuggestion,
                      BOOL shouldStorePermission))completionHandler {
  CHECK(!_interventionCompletion, base::NotFatalUntil::M165);

  _selectedSuggestion = suggestions.firstObject;
  CHECK(_selectedSuggestion.formSuggestion, base::NotFatalUntil::M165);

  NSString* manuallyEnterButtonText;
  switch (_selectedSuggestion.type) {
    case autofill::SuggestionType::kPasswordEntry:
    case autofill::SuggestionType::kIdentityCredential:
      manuallyEnterButtonText = l10n_util::GetNSString(
          IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_CREDENTIAL_MANUAL_FILL_BUTTON);
      break;
    case autofill::SuggestionType::kAddressEntry:
    case autofill::SuggestionType::kCreditCardEntry:
      manuallyEnterButtonText = l10n_util::GetNSString(
          IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_MANUAL_FILL_BUTTON);
      break;
    default:
      NOTREACHED(base::NotFatalUntil::M165);
      break;
  }

  ActuationWorklogItem* cardItem = CardItemForSuggestion(_selectedSuggestion);
  if (cardItem) {
    _suggestionsEntry = [self appendItem:cardItem toolType:std::nullopt];
    _suggestions = [suggestions copy];
  }
  if (completionHandler) {
    __weak __typeof(self) weakSelf = self;
    _interventionCompletion = ^(InterventionActionIdentifier actionIdentifier) {
      [weakSelf resolveSuggestionsWithAction:actionIdentifier
                           completionHandler:completionHandler];
    };
  }

  // TODO(crbug.com/571139088): Prefix the secondary button with the SF symbol
  // "hand.tap".
  ActuationInterventionAction* manualAction =
      [[ActuationInterventionAction alloc]
          initWithIdentifier:InterventionActionIdentifier::kManuallyEnter
                       title:manuallyEnterButtonText];
  ActuationInterventionAction* allowAction = [[ActuationInterventionAction
      alloc]
      initWithIdentifier:InterventionActionIdentifier::kAllow
                   title:l10n_util::GetNSString(
                             IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_ALLOW_BUTTON)];
  _intervention = [ActuationInterventionData
      rowItemWithActions:@[ manualAction, allowAction ]];
  [self publishState];
}

- (void)actorTaskDidCancelSuggestionSelection:(actor::ActorTaskId)taskID {
  if ([self cancelSuggestionSelection]) {
    [self publishState];
  }
}

- (void)actorTask:(actor::ActorTaskId)taskID
    requestUserInterventionWithTitle:(NSString*)title
                            subtitle:(NSString*)subtitle
                          buttonText:(NSString*)buttonText
                   completionHandler:(void (^)(void))completionHandler {
  [self cancelSuggestionSelection];
  [self discardIntervention];
  if (completionHandler) {
    _interventionCompletion = ^(InterventionActionIdentifier actionIdentifier) {
      if (actionIdentifier == InterventionActionIdentifier::kContinue) {
        completionHandler();
      }
    };
  }

  ActuationInterventionAction* action = [[ActuationInterventionAction alloc]
      initWithIdentifier:InterventionActionIdentifier::kContinue
                   title:buttonText];
  _intervention = [ActuationInterventionData cardItemWithTitle:title
                                                      subtitle:subtitle
                                                        action:action];
  [self publishState];
}

#pragma mark - ActorTaskActionHandler

- (void)resolveInterventionWithAction:(ActuationInterventionAction*)action {
  if (!_intervention) {
    return;
  }
  void (^completion)(InterventionActionIdentifier) = _interventionCompletion;
  [self discardIntervention];
  [self publishState];
  // Called last, as it may synchronously resume the task.
  if (completion) {
    completion(action.identifier);
  }
}

- (void)selectSuggestion:(ActorFormSuggestion*)suggestion {
  // While the task waits on the user, the suggestions card is the last entry.
  if (!suggestion || !_suggestionsEntry ||
      _entries.lastObject != _suggestionsEntry) {
    return;
  }
  ActuationWorklogItem* item = CardItemForSuggestion(suggestion);
  if (!item) {
    return;
  }
  _selectedSuggestion = suggestion;
  // Like the card it replaces, the new card has no tool chip.
  _suggestionsEntry = [[ActuationWorklogEntry alloc] initWithItem:item
                                                             chip:nil];
  _entries[_entries.count - 1] = _suggestionsEntry;
  [self publishState];
}

- (void)stopTask {
  if (!_actorService) {
    return;
  }
  _actorService->StopTask(_taskID,
                          actor::ActorTaskStoppedReason::kStoppedByUser);
}

#pragma mark - Private

// Resolves the pending suggestions intervention with `action` through
// `completionHandler`. The suggestions are already cleared by
// `resolveInterventionWithAction:`.
- (void)resolveSuggestionsWithAction:(InterventionActionIdentifier)action
                   completionHandler:
                       (SuggestionSelectionCompletionHandler)completionHandler {
  ActorFormSuggestion* selected =
      (action == InterventionActionIdentifier::kAllow) ? _selectedSuggestion
                                                       : nil;
  _selectedSuggestion = nil;
  completionHandler(selected, NO);
}

// Appends an item for `taskUpdate` unless empty or a consecutive duplicate.
// Returns whether an item was appended.
- (BOOL)appendTaskUpdate:(NSString*)taskUpdate
                toolType:(std::optional<actor::ToolType>)toolType {
  // TODO(crbug.com/555839522): Support updating the tool chip even when the
  // `taskUpdate` is empty or deduplicated.
  if (taskUpdate.length == 0 ||
      [_latestTaskUpdate isEqualToString:taskUpdate]) {
    return NO;
  }
  _latestTaskUpdate = [taskUpdate copy];
  [self appendItem:[ActuationWorklogItem simpleItemWithTitle:taskUpdate
                                                      active:YES]
          toolType:toolType];
  return YES;
}

// Appends an entry for `item` with the chip of `toolType`, and returns it.
- (ActuationWorklogEntry*)appendItem:(ActuationWorklogItem*)item
                            toolType:(std::optional<actor::ToolType>)toolType {
  ActuationWorklogEntry* entry = [[ActuationWorklogEntry alloc]
      initWithItem:item
              chip:ActuationWorklogChipForToolType(toolType)];
  [_entries addObject:entry];
  return entry;
}

// Drops any pending suggestion selection without calling its completion, and
// returns whether one was pending (`_selectedSuggestion` is `nil` otherwise).
- (BOOL)cancelSuggestionSelection {
  if (!_selectedSuggestion) {
    return NO;
  }
  _selectedSuggestion = nil;
  [self discardIntervention];
  return YES;
}

// Clears the pending intervention and its suggestions, and drops its
// completion without calling it.
- (void)discardIntervention {
  _interventionCompletion = nil;
  _intervention = nil;
  _suggestions = nil;
  _suggestionsEntry = nil;
}

// Builds the state from the fields and sends it to the observers. Called once
// per change, so that observers are notified once.
- (void)publishState {
  _state = [[ActuationWorklogState alloc]
      initWithActuationActive:!actor::IsTerminalState(_taskState)
                headerContent:ActuationHeaderContentVariantsForTask(
                                  _taskTitle, _taskState, _latestTaskUpdate)
                      entries:_entries
                 intervention:_intervention
                  suggestions:_suggestions
             suggestionsEntry:_suggestionsEntry];
  [_observers taskDataSource:self didUpdateState:_state];
}

@end
