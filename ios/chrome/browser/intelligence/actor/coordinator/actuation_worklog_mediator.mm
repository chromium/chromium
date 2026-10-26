// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_worklog_mediator.h"

#import <optional>

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/not_fatal_until.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_suggestions_consumer.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_form_suggestion.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_consumer.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Icon size for SF symbols in chips and labeled items.
constexpr CGFloat kIconSize = 16.0;

// Icon size for SF symbols in accessory items.
constexpr CGFloat kAccessoryIconSize = 24.0;

// Masked password field to be displayed in credential selection card.
NSString* const kMaskedPassword = @"••••••••";

// Identifiers for known intervention actions.
enum class InterventionActionIdentifier : NSInteger {
  // Continue the current task.
  kContinue,
  // "Allow this suggestion" action.
  kAllow,
  // "Manually enter" or "Manual sign in" action.
  kManuallyEnter,
};

// Generates an `ActuationWorklogChip` based on the executed tool type. Returns
// `nil` for tool types that don't display a chip.
ActuationWorklogChip* ChipForToolType(std::optional<actor::ToolType> toolType) {
  if (!toolType) {
    return nil;
  }
  switch (*toolType) {
    case actor::ToolType::kClick:
      return [[ActuationWorklogChip alloc]
          initWithText:l10n_util::GetNSString(
                           IDS_IOS_ACTOR_WORKLOG_CHIP_CLICKING)
                  icon:SymbolWithPointSize(SymbolCursorArrowRays, kIconSize)];
    case actor::ToolType::kType:
    case actor::ToolType::kAttemptFormFilling:
      return [[ActuationWorklogChip alloc]
          initWithText:l10n_util::GetNSString(IDS_IOS_ACTOR_WORKLOG_CHIP_TYPING)
                  icon:SymbolWithPointSize(SymbolKeyboard, kIconSize)];
    case actor::ToolType::kScroll:
    case actor::ToolType::kScrollTo:
      return [[ActuationWorklogChip alloc]
          initWithText:l10n_util::GetNSString(
                           IDS_IOS_ACTOR_WORKLOG_CHIP_SCROLLING)
                  icon:SymbolWithPointSize(SymbolCursorArrowMotionLines,
                                           kIconSize)];
    case actor::ToolType::kAttemptLogin:
      return [[ActuationWorklogChip alloc]
          initWithText:l10n_util::GetNSString(
                           IDS_IOS_ACTOR_WORKLOG_CHIP_FILLING_PASSWORD)
                  icon:SymbolWithPointSize(SymbolKey, kIconSize)];
    case actor::ToolType::kNavigate:
    case actor::ToolType::kBack:
    case actor::ToolType::kForward:
      return [[ActuationWorklogChip alloc]
          initWithText:l10n_util::GetNSString(
                           IDS_IOS_ACTOR_WORKLOG_CHIP_NAVIGATING)
                  icon:SymbolWithPointSize(SymbolGlobe, kIconSize)];
    case actor::ToolType::kWait:
      return [[ActuationWorklogChip alloc]
          initWithText:l10n_util::GetNSString(
                           IDS_IOS_ACTOR_WORKLOG_CHIP_WAITING)
                  icon:SymbolWithPointSize(SymbolHourglass, kIconSize)];
    case actor::ToolType::kWaitZeroDuration:
    case actor::ToolType::kUnknown:
      return nil;
    default:
      return [[ActuationWorklogChip alloc]
          initWithText:l10n_util::GetNSString(
                           IDS_IOS_ACTOR_WORKLOG_CHIP_DEFAULT)
                  icon:SymbolWithPointSize(SymbolCursorArrow, kIconSize)];
  }
}

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

}  // namespace

@implementation ActuationWorklogMediator {
  // Service tracking actor tasks and updates.
  raw_ptr<actor::ActorService> _actorService;
  // Currently observed task ID.
  std::optional<actor::ActorTaskId> _currentTaskId;
  // Latest emitted task update. Deduplicates consecutive identical updates and
  // is displayed as the current step in the minimized header.
  NSString* _latestEmittedTaskUpdate;
  // Title of the observed task.
  NSString* _taskTitle;
  // Current state of the observed task.
  actor::ActorTaskState _taskState;
  // Pending completion block for the intervention action with the given
  // identifier. Only called when a user interacts with the UI associated with
  // it.
  void (^_pendingInterventionCompletion)(InterventionActionIdentifier);
}

- (instancetype)initWithActorService:(actor::ActorService*)actorService {
  self = [super init];
  if (self) {
    _actorService = actorService;
    _taskState = actor::ActorTaskState::kInit;
  }
  return self;
}

#pragma mark - Public

- (void)startObservingTaskWithID:(actor::ActorTaskId)taskID {
  if (_currentTaskId == taskID) {
    return;
  }
  [self stopObservingTask];
  if (!_actorService) {
    return;
  }

  // Ensure we are brought back at the init state when subscribing to a task.
  _taskState = actor::ActorTaskState::kInit;
  // Tracked right away rather than from the posted
  // `didRegisterAsObserverForTaskID:`, so that `stopObservingTask` still
  // unregisters if called before the registration is delivered.
  if (!_actorService->AddTaskUpdatesObserver(taskID, self)) {
    return;
  }
  _currentTaskId = taskID;
  _actorService->SetTaskInterventionDelegate(taskID, self);
}

- (void)stopObservingTask {
  if (!_currentTaskId) {
    return;
  }
  if (_actorService) {
    _actorService->SetTaskInterventionDelegate(*_currentTaskId, nil);
    _actorService->RemoveTaskUpdatesObserver(*_currentTaskId, self);
  }
  _currentTaskId.reset();
  [self discardPendingIntervention];
  _latestEmittedTaskUpdate = nil;
  _taskTitle = nil;
  [_consumer setActuationActive:NO];
  [_consumer reset];
}

- (void)disconnect {
  // TODO(crbug.com/567035965): A pending intervention is dropped without an
  // answer, leaving the task in `kWaitingOnUser`. Re-surface the intervention
  // once the worklog can be restored.
  [self stopObservingTask];
  _actorService = nullptr;
  _consumer = nil;
  self.suggestionsConsumer = nil;
}

#pragma mark - Private

// Dismisses the active intervention card and discards the pending completion
// handler without calling it (task cancellation, replacement, or disconnect).
- (void)discardPendingIntervention {
  _pendingInterventionCompletion = nil;
  [_consumer setIntervention:nil];
}

// Emits a worklog item and tool chip, deduplicating consecutive updates.
- (void)processUpdateWithTool:(std::optional<actor::ToolType>)toolType
                   taskUpdate:(NSString*)taskUpdate {
  // TODO(crbug.com/555839522): Support updating the tool chip even when the
  // `taskUpdate` is nil.
  if (!_consumer || taskUpdate.length == 0) {
    return;
  }

  // Deduplicate consecutive identical updates.
  // TODO(crbug.com/555839522): Support updating the tool chip even when the
  // `taskUpdate` is deduplicated.
  if ([_latestEmittedTaskUpdate isEqualToString:taskUpdate]) {
    return;
  }
  _latestEmittedTaskUpdate = [taskUpdate copy];

  ActuationWorklogItem* item =
      [ActuationWorklogItem simpleItemWithTitle:taskUpdate active:YES];
  ActuationWorklogChip* chip = ChipForToolType(toolType);
  [_consumer updateWorklogWithItem:item chip:chip animated:YES];
  [self updateHeaderContent];
}

// Pushes the header content for the observed task to the consumer.
- (void)updateHeaderContent {
  ActuationHeaderContent* defaultContent =
      [[ActuationHeaderContent alloc] initWithTitle:_taskTitle subtitle:nil];
  [_consumer setHeaderContentVariants:
                 [[ActuationHeaderContentVariants alloc]
                     initWithMinimizedContent:[self minimizedHeaderContent]
                               compactContent:defaultContent
                              expandedContent:defaultContent]];
}

// Returns the header content for the minimized display mode. While the task
// runs, the current step is shown below the task title. Otherwise, a status
// message replaces the title and the task title moves below it.
- (ActuationHeaderContent*)minimizedHeaderContent {
  switch (_taskState) {
    case actor::ActorTaskState::kInit:
    case actor::ActorTaskState::kActing:
    case actor::ActorTaskState::kReflecting:
    case actor::ActorTaskState::kPausedByActor:
    case actor::ActorTaskState::kPausedByUser:
      // TODO(crbug.com/571141885): Show a placeholder subtitle for the `kInit`
      // state, since at that point we don't have a task update yet.
      return [[ActuationHeaderContent alloc]
          initWithTitle:_taskTitle
               subtitle:_latestEmittedTaskUpdate];
    case actor::ActorTaskState::kWaitingOnUser:
      return [self contentWithStatus:IDS_IOS_ACTOR_HEADER_INPUT_NEEDED];
    case actor::ActorTaskState::kFinished:
      return [self contentWithStatus:IDS_IOS_ACTOR_HEADER_TASK_DONE];
    // `kCancelled` is meant to only represent user cancellations. Other stop
    // reasons are expected to map to `kFinished` or `kFailed` once
    // crbug.com/532978481 is fixed.
    case actor::ActorTaskState::kCancelled:
      return [self contentWithStatus:IDS_IOS_ACTOR_HEADER_TASK_STOPPED];
    case actor::ActorTaskState::kFailed:
      return [self contentWithStatus:IDS_IOS_ACTOR_HEADER_TASK_FAILED];
  }
}

// Returns header content with a status message above the task title.
- (ActuationHeaderContent*)contentWithStatus:(int)messageID {
  return [[ActuationHeaderContent alloc]
      initWithTitle:l10n_util::GetNSString(messageID)
           subtitle:_taskTitle];
}

#pragma mark - ActorTaskUpdatesObserver

- (void)didRegisterAsObserverForTaskID:(actor::ActorTaskId)taskID
                             taskTitle:(NSString*)taskTitle
                            taskUpdate:(NSString*)taskUpdate
                          currentState:(actor::ActorTaskState)state
                             webStates:(NSArray<NSNumber*>*)webStatesIDs {
  if (_currentTaskId != taskID) {
    return;
  }
  ActuationWorklogItem* initialItem = [ActuationWorklogItem
      labeledItemWithTitle:l10n_util::GetNSString(
                               IDS_IOS_GEMINI_FIRST_ACTUATION_STEP_TITLE)
                  subtitle:l10n_util::GetNSString(
                               IDS_IOS_GEMINI_FIRST_ACTUATION_STEP_SUBTITLE)
                      icon:SymbolWithPointSize(SymbolPlayFill, kIconSize)
                    active:YES];
  _taskTitle = [taskTitle copy];
  _taskState = state;
  [self updateHeaderContent];
  [_consumer setActuationActive:!actor::IsTerminalState(state)];
  [_consumer updateWorklogWithItem:initialItem chip:nil animated:YES];

  // Use the initial task update on top of our default start task update.
  [self processUpdateWithTool:std::nullopt taskUpdate:taskUpdate];
}

- (void)actorTaskWithID:(actor::ActorTaskId)taskID
         didChangeState:(actor::ActorTaskState)newState
              fromState:(actor::ActorTaskState)oldState {
  if (_currentTaskId != taskID) {
    return;
  }
  _taskState = newState;
  [_consumer setActuationActive:!actor::IsTerminalState(newState)];
  [self updateHeaderContent];
}

- (void)actorTaskWithID:(actor::ActorTaskId)taskID
        willExecuteTool:(actor::ToolType)toolType
             taskUpdate:(NSString*)taskUpdate
             onWebState:(web::WebStateID)webStateID {
  if (_currentTaskId != taskID) {
    return;
  }
  [self processUpdateWithTool:toolType taskUpdate:taskUpdate];
}

#pragma mark - ActorTaskInterventionDelegate

- (void)actorTask:(actor::ActorTaskId)taskID
    selectFromSuggestions:(NSArray<ActorFormSuggestion*>*)suggestions
        completionHandler:
            (void (^)(ActorFormSuggestion* selectedSuggestion,
                      BOOL shouldStorePermission))completionHandler {
  CHECK(_currentTaskId == taskID, base::NotFatalUntil::M165);
  CHECK(!_pendingInterventionCompletion, base::NotFatalUntil::M165);

  ActorFormSuggestion* firstSuggestion = suggestions.firstObject;
  CHECK(firstSuggestion.formSuggestion, base::NotFatalUntil::M165);

  ActuationWorklogItem* cardItem = nil;
  NSString* manuallyEnterButtonText;
  switch (firstSuggestion.type) {
    case autofill::SuggestionType::kPasswordEntry:
      cardItem =
          CardItemForNonFederatedCredential(firstSuggestion.formSuggestion);
      manuallyEnterButtonText = l10n_util::GetNSString(
          IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_CREDENTIAL_MANUAL_FILL_BUTTON);
      break;
    case autofill::SuggestionType::kAddressEntry:
      cardItem = CardItemForAddress(firstSuggestion.formSuggestion);
      manuallyEnterButtonText = l10n_util::GetNSString(
          IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_MANUAL_FILL_BUTTON);
      break;
    case autofill::SuggestionType::kCreditCardEntry:
      cardItem = CardItemForCreditCard(firstSuggestion.formSuggestion);
      manuallyEnterButtonText = l10n_util::GetNSString(
          IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_MANUAL_FILL_BUTTON);
      break;
    default:
      NOTREACHED(base::NotFatalUntil::M165);
      break;
  }

  [_consumer updateWorklogWithItem:cardItem chip:nil animated:YES];

  __weak id<ActuationSuggestionsConsumer> suggestionsConsumer =
      self.suggestionsConsumer;
  [suggestionsConsumer setSuggestions:suggestions
                 forActiveWorklogItem:cardItem];
  if (completionHandler) {
    _pendingInterventionCompletion =
        ^(InterventionActionIdentifier actionIdentifier) {
          // TODO(crbug.com/571139088): Implement credential picker modal and
          // execute with the chose suggestion.
          ActorFormSuggestion* selected =
              (actionIdentifier == InterventionActionIdentifier::kAllow)
                  ? firstSuggestion
                  : nil;
          completionHandler(selected, NO);
          [suggestionsConsumer clearSuggestions];
        };
  }

  // TODO(crbug.com/571139088): Prefix the secondary button with the SF symbol
  // "hand.tap".
  ActuationInterventionAction* manualAction =
      [[ActuationInterventionAction alloc]
          initWithIdentifier:static_cast<NSInteger>(
                                 InterventionActionIdentifier::kManuallyEnter)
                       title:manuallyEnterButtonText];
  ActuationInterventionAction* allowAction = [[ActuationInterventionAction
      alloc]
      initWithIdentifier:static_cast<NSInteger>(
                             InterventionActionIdentifier::kAllow)
                   title:l10n_util::GetNSString(
                             IDS_IOS_ACTOR_WORKLOG_SUGGESTIONS_ALLOW_BUTTON)];
  ActuationInterventionData* data = [ActuationInterventionData
      rowItemWithActions:@[ manualAction, allowAction ]];
  [_consumer setIntervention:data];
}

- (void)actorTask:(actor::ActorTaskId)taskID
    requestUserInterventionWithTitle:(NSString*)title
                            subtitle:(NSString*)subtitle
                          buttonText:(NSString*)buttonText
                   completionHandler:(void (^)(void))completionHandler {
  CHECK(_currentTaskId == taskID, base::NotFatalUntil::M165);
  [self discardPendingIntervention];
  if (completionHandler) {
    _pendingInterventionCompletion =
        ^(InterventionActionIdentifier actionIdentifier) {
          if (actionIdentifier == InterventionActionIdentifier::kContinue) {
            completionHandler();
          }
        };
  }

  ActuationInterventionAction* action = [[ActuationInterventionAction alloc]
      initWithIdentifier:static_cast<NSInteger>(
                             InterventionActionIdentifier::kContinue)
                   title:buttonText];
  ActuationInterventionData* data =
      [ActuationInterventionData cardItemWithTitle:title
                                          subtitle:subtitle
                                            action:action];
  [_consumer setIntervention:data];
}

#pragma mark - ActuationWorklogMutator

- (void)stopActuation {
  if (!_actorService || !_currentTaskId) {
    return;
  }
  _actorService->StopTask(*_currentTaskId,
                          actor::ActorTaskStoppedReason::kStoppedByUser);
}

- (void)didTriggerInterventionAction:(ActuationInterventionAction*)action {
  void (^completion)(InterventionActionIdentifier) =
      _pendingInterventionCompletion;
  [self discardPendingIntervention];
  if (completion) {
    completion(static_cast<InterventionActionIdentifier>(action.identifier));
  }
}

@end
