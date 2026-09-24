// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/assistant/coordinator/assistant_container_commands.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_view_controller.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_updates_observer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_mediator_event_handler.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_ui_state_manager.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_gateway_manager.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_page_context.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_page_state_change_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_session_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_shared_tabs_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper_observer.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_feature_availability.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_prefs.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/zero_state_suggestions/ui/gemini_zero_state_consumer.h"
#import "ios/chrome/browser/intelligence/zero_state_suggestions/zero_state_suggestions_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_gateway_protocol.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"
#import "ui/base/l10n/l10n_util.h"

using enum AssistantContainerDetent;
using ios::provider::GeminiClientMode;
using ios::provider::GeminiDormantReason;
using ios::provider::GeminiViewMode;
using ios::provider::GeminiViewState;

@interface GeminiContainerMediator () <ActorTaskUpdatesObserver,
                                       GeminiContainerUIStateManagerDelegate>

// Called when the active `WebState` changes.
- (void)onActiveWebStateChanged:(web::WebState*)oldActive
                      newActive:(web::WebState*)newActive;

// Called when the page context for `webState` is updated.
- (void)onPageContextUpdated:(web::WebState*)webState;

// Called when the `WebStateList` is destroyed.
- (void)onWebStateListDestroyed;

@end

namespace {

// Internal C++ observer for `WebStateList` active `WebState` changes.
class GeminiContainerMediatorWebStateListObserver
    : public WebStateListObserver {
 public:
  explicit GeminiContainerMediatorWebStateListObserver(
      GeminiContainerMediator* mediator)
      : mediator_(mediator) {}

  ~GeminiContainerMediatorWebStateListObserver() override = default;

  // WebStateListObserver:
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override {
    if (status.active_web_state_change()) {
      [mediator_ onActiveWebStateChanged:status.old_active_web_state
                               newActive:status.new_active_web_state];
    }
  }

  void WebStateListDestroyed(WebStateList* web_state_list) override {
    web_state_list->RemoveObserver(this);
    [mediator_ onWebStateListDestroyed];
  }

 private:
  __weak GeminiContainerMediator* mediator_ = nil;
};

// Internal C++ observer for `GeminiTabHelper` events.
class GeminiContainerMediatorTabHelperObserver
    : public GeminiTabHelperObserver {
 public:
  explicit GeminiContainerMediatorTabHelperObserver(
      GeminiContainerMediator* mediator)
      : mediator_(mediator) {}

  ~GeminiContainerMediatorTabHelperObserver() override = default;

  // GeminiTabHelperObserver:
  void OnPageContextUpdated(web::WebState* web_state) override {
    [mediator_ onPageContextUpdated:web_state];
  }

  void OnGeminiTabHelperDestroyed(GeminiTabHelper* tab_helper) override {
    tab_helper->RemoveObserver(this);
  }

 private:
  __weak GeminiContainerMediator* mediator_ = nil;
};

}  // namespace

@implementation GeminiContainerMediator {
  // WebStateList for the browser.
  raw_ptr<WebStateList> _webStateList;
  // Profile for the browser.
  raw_ptr<ProfileIOS> _profile;
  // Service tracking actor tasks and updates.
  raw_ptr<actor::ActorService> _actorService;
  // Track if we have triggered feature engagement for Gemini Live IPH or New
  // Badge.
  BOOL _hasTriggeredGeminiLiveIPH;
  BOOL _hasTriggeredGeminiLiveNewBadge;
  // State manager for container UI state transitions.
  GeminiContainerUIStateManager* _stateManager;
  // Observer for `WebStateList` active `WebState` changes.
  std::unique_ptr<GeminiContainerMediatorWebStateListObserver>
      _webStateListObserver;
  // Observer for `GeminiTabHelper` events.
  std::unique_ptr<GeminiContainerMediatorTabHelperObserver> _tabHelperObserver;
}

- (instancetype)initWithBrowser:(Browser*)browser
                   actorService:(actor::ActorService*)actorService
                   eventHandler:
                       (GeminiContainerMediatorEventHandler*)eventHandler {
  self = [super init];
  if (self) {
    _eventHandler = eventHandler;
    if (browser) {
      _webStateList = browser->GetWebStateList();
      _profile = browser->GetProfile();
    }
    _actorService = actorService;
    _gatewayManager = [[GeminiGatewayManager alloc] initWithBrowser:browser
                                                  viewStateDelegate:self];
    _stateManager = [[GeminiContainerUIStateManager alloc] init];
    _stateManager.delegate = self;
  }
  return self;
}

#pragma mark - Property Getters and Setters

- (id<BWGGatewayProtocol>)gateway {
  return _gatewayManager.gateway;
}

#pragma mark - Public Methods

- (GeminiConfiguration*)
    createGeminiConfigurationForActiveWebState:(GeminiStartupState*)startupState
                            baseViewController:
                                (UIViewController*)baseViewController {
  if (startupState) {
    _startupState = startupState;
  }

  GeminiTabHelper* geminiTabHelper = [self activeTabHelper];
  if (!geminiTabHelper) {
    return nil;
  }

  GeminiPageContext* initialPageContext =
      geminiTabHelper->GetPartialPageContext();
  [self applyUserPrefsToPageContext:initialPageContext];

  GeminiConfiguration* config =
      [self createGeminiConfigurationWithTabHelper:geminiTabHelper
                                       pageContext:initialPageContext
                                      startupState:startupState];
  if (baseViewController) {
    // TODO(crbug.com/537730178): Delegate the permission prompt request up to
    // a delegate protocol implemented by GeminiContainerCoordinator, which will
    // present the UIAlertController using its own baseViewController.
    [_gatewayManager.pageStateChangeHandler
        setBaseViewController:baseViewController];

    // TODO(crbug.com/535579970): Remove after migration. Embadded floaty
    // doesn't need the baseViewController.
    config.baseViewController = baseViewController;
  }

  return config;
}

- (BOOL)shouldShowSuggestionChipsForEntryPoint:
    (gemini::EntryPoint)entryPoint {
  if (entryPoint == gemini::EntryPoint::AtMemorySearch) {
    return NO;
  }

  GeminiTabHelper* geminiTabHelper = [self activeTabHelper];
  if (!geminiTabHelper) {
    return NO;
  }

  bool shouldShow = geminiTabHelper->ShouldShowSuggestionChips();
  if (IsAppSwitcherAISummarizationEnabled() &&
      entryPoint == gemini::EntryPoint::AppSwitcherAISummarization) {
    shouldShow = false;
  }
  return shouldShow;
}

- (void)fetchZeroStateSuggestions:(GeminiStartupState*)startupState {
  if (!self.zeroStateConsumer || !startupState) {
    return;
  }

  GeminiTabHelper* geminiTabHelper = [self activeTabHelper];
  if (!geminiTabHelper ||
      ![self shouldShowSuggestionChipsForEntryPoint:startupState.entryPoint]) {
    [self.zeroStateConsumer setZeroStateSuggestions:@[]];
    return;
  }

  __weak __typeof(self) weakSelf = self;
  geminiTabHelper->FetchZeroStateSuggestions(
      base::BindOnce(^(NSArray<ZeroStateSuggestion*>* suggestions) {
        [weakSelf.zeroStateConsumer setZeroStateSuggestions:suggestions];
      }));
}

- (BOOL)shouldBlockQuerySubmissionWhileLoadingForEntryPoint:
    (gemini::EntryPoint)entryPoint {
  return entryPoint == gemini::EntryPoint::AppSwitcherAISummarization &&
         IsAppSwitcherAISummarizationEnabled();
}

- (BOOL)shouldShowPageLoadingSnackbarOnOpeningInvocationForEntryPoint:
    (gemini::EntryPoint)entryPoint {
  return entryPoint == gemini::EntryPoint::AppSwitcherAISummarization &&
         IsAppSwitcherAISummarizationEnabled();
}

- (void)connect {
  if (_actorService) {
    _actorService->AddTaskUpdatesObserver(self);
  }
  [self setupInitialUIState];
  [self requestActivePageContextGeneration];
  [self onFloatyInvoked];
}

- (void)onFloatyInvoked {
  [self attachObservers];
}

- (void)onFloatyDismiss {
  feature_engagement::Tracker* tracker =
      _profile ? feature_engagement::TrackerFactory::GetForProfile(_profile)
               : nullptr;
  if (tracker) {
    if (_hasTriggeredGeminiLiveIPH) {
      tracker->Dismissed(feature_engagement::kIPHiOSGeminiLiveIPHFeature);
      _hasTriggeredGeminiLiveIPH = NO;
    }
    if (_hasTriggeredGeminiLiveNewBadge) {
      tracker->Dismissed(feature_engagement::kIPHiOSGeminiLiveNewBadgeFeature);
      _hasTriggeredGeminiLiveNewBadge = NO;
    }
  }

  [self cancelPageContextGeneration];
  [_stateManager reset];
  [self detachObservers];
  ios::provider::ResetGemini();
}

- (void)setConsumer:(id<GeminiContainerConsumer>)consumer {
  CHECK(IsIOSGeminiBottomSheetMigrationEnabled());
  _consumer = consumer;
}

- (void)disconnect {
  [self onFloatyDismiss];

  if (_actorService) {
    _actorService->RemoveTaskUpdatesObserver(self);
    _actorService = nullptr;
  }

  self.zeroStateConsumer = nil;
  _startupState = nil;
  _eventHandler = nullptr;
  _containerHandler = nil;
  _geminiHandler = nil;
  _sharedTabsDelegate = nil;
  _consumer = nil;
  _webStateList = nullptr;
  _profile = nullptr;
  [_gatewayManager disconnect];
  _gatewayManager = nil;
  [_stateManager reset];
  _stateManager.delegate = nil;
}

#pragma mark - ActorTaskUpdatesObserver

- (void)didRegisterAsObserverForTaskID:(actor::ActorTaskId)taskID
                             taskTitle:(NSString*)taskTitle
                            taskUpdate:(NSString*)taskUpdate
                          currentState:(actor::ActorTaskState)state
                             webStates:(NSArray<NSNumber*>*)webStatesIDs {
  [self setActuationActive:!actor::IsTerminalState(state)];
}

- (void)actorTaskWithID:(actor::ActorTaskId)taskID
         didChangeState:(actor::ActorTaskState)newState
              fromState:(actor::ActorTaskState)oldState {
  [self setActuationActive:!actor::IsTerminalState(newState)];
}

- (void)actorTaskDidStopWithID:(actor::ActorTaskId)taskID
                    finalState:(actor::ActorTaskState)finalState {
  [self setActuationActive:NO];
}

#pragma mark - AssistantContainerDelegate

- (void)assistantContainerDidUpdateDetentHeights:
    (AssistantContainerViewController*)container {
  if (_stateManager.currentUIState.actuating) {
    return;
  }
  NSInteger collapsedHeight = [container heightForDetent:kMinimized];
  NSInteger extendedHeight = [container heightForDetent:kMedium];

  if (collapsedHeight > 0 && extendedHeight > 0) {
    ios::provider::UpdateDetentHeights(collapsedHeight, extendedHeight);
  }
}

- (void)assistantContainer:(AssistantContainerViewController*)container
           didChangeDetent:(AssistantContainerDetent)newDetent {
  BOOL minimized = (newDetent == kMinimized);
  if (_stateManager.currentUIState.detent == kMinimized && !minimized) {
    [self requestActivePageContextGeneration];
  }

  [_stateManager updateDetent:newDetent];

  if (_stateManager.currentUIState.actuating) {
    [self.containerHandler setAssistantContainerGrabberHidden:NO animated:YES];
  } else if ([_stateManager shouldBeDismissed]) {
    [self.geminiHandler dismissGeminiFlowWithCompletion:nil];
  }
  [self.consumer setWorklogCompact:minimized];
}

- (void)assistantContainerDidRequestDismissal:
    (AssistantContainerViewController*)container {
  if (_stateManager.currentUIState.actuating) {
    return;
  }
  [self.geminiHandler dismissGeminiFlowWithCompletion:nil];
}

#pragma mark - GeminiViewStateDelegate

- (void)didSwitchToViewState:(GeminiViewState)viewState {
  if (_eventHandler) {
    _eventHandler->OnViewStateChanged(viewState);
    _eventHandler->SetLastShownViewState(viewState);
  }
}

- (void)didUpdateProcessingStatus:(GeminiClientMode)processingStatus
                        sessionID:(NSString*)sessionID
                   conversationID:(NSString*)conversationID {
  [self didUpdateProcessingStatus:processingStatus
                    dormantReason:GeminiDormantReason::kUnknown
                        sessionID:sessionID
                   conversationID:conversationID];
}

- (void)didUpdateProcessingStatus:(GeminiClientMode)processingStatus
                    dormantReason:(GeminiDormantReason)dormantReason
                        sessionID:(NSString*)sessionID
                   conversationID:(NSString*)conversationID {
  if (_eventHandler) {
    _eventHandler->OnProcessingStatusChanged(processingStatus, dormantReason);
  }

  [_stateManager transitionToProcessingStatus:processingStatus];
  [self updatePageContextForLiveProcessingStatus:processingStatus];
}

- (void)geminiLiveUserDidTapLiveButton {
  if (_eventHandler) {
    _eventHandler->OnLiveButtonTapped();
  }
}

- (void)geminiLiveUserDidPressStopButton {
  if (_eventHandler) {
    _eventHandler->OnGeminiLiveUserDidPressStopButton();
  }
}

- (void)geminiLiveUserDidBargeIn {
  if (_eventHandler) {
    _eventHandler->OnGeminiLiveUserDidBargeIn();
  }
}

- (void)didSwitchToMode:(GeminiViewMode)mode {
  if (_eventHandler) {
    _eventHandler->OnModeChanged(mode);
  }

  [_stateManager transitionToMode:mode];
}

- (void)geminiUIDidAppear {
  if (_eventHandler) {
    _eventHandler->OnGeminiUIDidAppear();
  }
}

- (void)didTapNewChatButton {
  if (!IsIOSGeminiBottomSheetMigrationEnabled()) {
    return;
  }

  [self fetchZeroStateSuggestions:_startupState];
  [_stateManager handleNewChat];
}

- (void)responseCancelledWithReason:(GeminiCancelType)reason {
  if (!IsIOSGeminiBottomSheetMigrationEnabled()) {
    return;
  }

  [_stateManager handleResponseCancellationWithReason:reason];
}

- (void)didTapResponseReadyViewButton {
  if (!IsIOSGeminiBottomSheetMigrationEnabled()) {
    return;
  }
  [self.containerHandler
      animateAssistantContainerToDetent:AssistantContainerDetent::kMedium];
}

- (void)setActuationActive:(BOOL)actuationActive {
  if (_stateManager.currentUIState.actuating == actuationActive) {
    return;
  }
  if (!actuationActive) {
    [self.containerHandler setAssistantContainerMinimizedDetentHeight:
                               kAssistantContainerMinimizedDetentHeight];
  }
  [_stateManager handleActuationStateChanged:actuationActive];
}

#pragma mark - GeminiZeroStateMutator

- (void)geminiZeroStateViewController:
            (GeminiZeroStateViewController*)viewController
                  didSelectSuggestion:(ZeroStateSuggestion*)suggestion {
  if (!_startupState || !suggestion.query.length) {
    return;
  }
  ios::provider::UpdatePromptAction(_startupState.entryPoint, suggestion.query,
                                    YES);
}

#pragma mark - Private

- (void)applyUserPrefsToPageContext:(GeminiPageContext*)geminiPageContext {
  PrefService* prefService = _profile->GetPrefs();
  if (!prefService->GetBoolean(prefs::kIOSBWGPageContentSetting)) {
    geminiPageContext.geminiPageContextAttachmentState =
        ios::provider::GeminiPageContextAttachmentState::kUserDisabled;
  } else {
    // If page context is not disabled by the user, page context is always
    // available and should be attached. Note page context is only partially
    // available (e.g. title, url, favicon) while
    // `GeminiPageContextComputationState` is pending.
    geminiPageContext.geminiPageContextAttachmentState =
        ios::provider::GeminiPageContextAttachmentState::kAttached;
  }
}

- (GeminiConfiguration*)
    createGeminiConfigurationWithTabHelper:(GeminiTabHelper*)geminiTabHelper
                               pageContext:(GeminiPageContext*)pageContext
                              startupState:(GeminiStartupState*)startupState {
  GeminiConfiguration* config = [[GeminiConfiguration alloc] init];
  config.authService = AuthenticationServiceFactory::GetForProfile(_profile);
  config.singleSignOnService =
      GetApplicationContext()->GetSingleSignOnService();
  config.gateway = self.gateway;
  config.gateway.sessionHandler.isFirstSession = startupState.isFirstSession;
  config.imageAttachment = startupState.imageAttachment;

  config.clientID = base::SysUTF8ToNSString(geminiTabHelper->GetClientId());
  std::optional<std::string> maybeServerId =
      gemini::GetConversationId(_profile->GetPrefs());
  config.serverID =
      maybeServerId ? base::SysUTF8ToNSString(*maybeServerId) : nil;
  config.shouldAnimatePresentation = YES;
  config.lastInteractionURLDifferent =
      geminiTabHelper->IsLastInteractionUrlDifferent();
  config.shouldShowSuggestionChips =
      [self shouldShowSuggestionChipsForEntryPoint:startupState.entryPoint] &&
      !IsIOSGeminiBottomSheetMigrationEnabled();
  if (IsAppSwitcherAISummarizationEnabled() &&
      startupState.isMismatchedAccount) {
    config.shouldShowAccountSnackbar = YES;
  }
  config.contextualCueChipLabel = startupState.prepopulatedPrompt;
  config.shouldAutoSubmit = startupState.shouldAutoSubmit;
  config.entryPoint = startupState.entryPoint;
  config.blockQuerySubmissionWhileLoading =
      [self shouldBlockQuerySubmissionWhileLoadingForEntryPoint:
          startupState.entryPoint];
  RecordBlockQuerySubmissionWhileLoading(
      config.blockQuerySubmissionWhileLoading);
  config.showPageLoadingSnackbarOnOpeningInvocation =
      [self shouldShowPageLoadingSnackbarOnOpeningInvocationForEntryPoint:
          startupState.entryPoint];
  RecordShowPageLoadingSnackbarOnOpeningInvocation(
      config.showPageLoadingSnackbarOnOpeningInvocation);
  config.imageRemixIPHShouldShow =
      startupState.entryPoint == gemini::EntryPoint::ImageRemixIPH;

  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(_profile);
  // Only trigger and show the IPH/new badge if Gemini Live is available for
  // the current user.
  if (tracker && gemini::IsFeatureAvailable(gemini::Feature::kLive, _profile)) {
    config.shouldShowGeminiLiveIPH = tracker->ShouldTriggerHelpUI(
        feature_engagement::kIPHiOSGeminiLiveIPHFeature);
    config.shouldShowGeminiLiveNewBadge = tracker->ShouldTriggerHelpUI(
        feature_engagement::kIPHiOSGeminiLiveNewBadgeFeature);
    _hasTriggeredGeminiLiveIPH = config.shouldShowGeminiLiveIPH;
    _hasTriggeredGeminiLiveNewBadge = config.shouldShowGeminiLiveNewBadge;
  } else {
    config.shouldShowGeminiLiveIPH = NO;
    config.shouldShowGeminiLiveNewBadge = NO;
  }
  config.geminiLiveIPHText = l10n_util::GetNSString(IDS_IOS_GEMINI_LIVE_IPH);

  config.geminiLocationPermissionState =
      ios::provider::GeminiLocationPermissionState::kUnknown;
  config.pageContext = pageContext;
  GeminiService* geminiService = GeminiServiceFactory::GetForProfile(_profile);
  config.needsAccountCapabilityRestriction =
      geminiService && geminiService->HasGeminiInChromeCapability() &&
      !geminiService->HasModelExecutionCapability();

  return config;
}

#pragma mark - GeminiContainerUIStateManagerDelegate

- (void)didChangeUIState:(GeminiContainerUIState)containerUIState {
  [self.containerHandler
      animateAssistantContainerToDetent:containerUIState.detent];
  [self.containerHandler
      setAssistantContainerGrabberHidden:!containerUIState.hasGrabber
                                animated:YES];
  [self.consumer updateZeroStateVisibility:containerUIState.zeroStateVisible];
  [self.consumer setWorklogCompact:(containerUIState.detent == kMinimized)];
  [self.consumer setActuationActive:containerUIState.actuating];
}

#pragma mark - GeminiContainerMutator

- (void)containerKeyboardDidShowWithDuration:(NSTimeInterval)duration
                                       curve:(UIViewAnimationCurve)curve {
  [self.containerHandler
      animateAssistantContainerToDetent:AssistantContainerDetent::kLarge
                               duration:duration
                                  curve:curve];
}

- (void)containerDidChangeActuationHeight:(CGFloat)height {
  if (!_stateManager.currentUIState.actuating) {
    return;
  }
  [self.containerHandler
      setAssistantContainerMinimizedDetentHeight:ceil(height)];
  if (_stateManager.currentUIState.detent ==
      AssistantContainerDetent::kMinimized) {
    [self.containerHandler
        animateAssistantContainerToDetent:AssistantContainerDetent::kMinimized];
  }
}

#pragma mark - Private

// Sets up the initial UI state for the container.
- (void)setupInitialUIState {
  [self fetchZeroStateSuggestions:_startupState];

  [_stateManager setupInitialUIState];

  // In initial zero state the view shouldn't be focused for input.
  [self.consumer dismissKeyboard];
}

- (GeminiTabHelper*)activeTabHelper {
  web::WebState* activeWebState =
      _webStateList ? _webStateList->GetActiveWebState() : nullptr;
  return activeWebState ? GeminiTabHelper::FromWebState(activeWebState)
                        : nullptr;
}

#pragma mark - Page Context

// Updates `page_context`'s computation and attachment states based on active
// page eligibility and user preferences.
- (void)updatePageContextState:(GeminiPageContext*)pageContext {
  GeminiTabHelper* tabHelper = [self activeTabHelper];
  bool isEligible = tabHelper && tabHelper->IsGeminiChatAvailableForWebState();

  // Handle programmatic blocking/detachment for ineligible or hidden pages.
  if (!isEligible) {
    pageContext.geminiPageContextComputationState =
        ios::provider::GeminiPageContextComputationState::kBlocked;
    pageContext.geminiPageContextAttachmentState =
        ios::provider::GetCurrentPageContextAttachmentState();
    pageContext.uniquePageContext = nullptr;
    return;
  }

  // Apply user settings.
  [self applyUserPrefsToPageContext:pageContext];

  // Persists manual detachment across navigations. If the user explicitly
  // detached the context via the paperclip UI, respect that choice over the
  // default attached state.
  if (pageContext.geminiPageContextAttachmentState ==
          ios::provider::GeminiPageContextAttachmentState::kAttached &&
      ios::provider::GetCurrentPageContextAttachmentState() ==
          ios::provider::GeminiPageContextAttachmentState::kDetached) {
    pageContext.geminiPageContextAttachmentState =
        ios::provider::GeminiPageContextAttachmentState::kDetached;
  }
}

- (void)cancelPageContextGeneration {
  GeminiTabHelper* tabHelper = [self activeTabHelper];
  if (tabHelper) {
    tabHelper->CancelPageContextGeneration();
  }
}

- (void)requestActivePageContextGeneration {
  GeminiTabHelper* tabHelper = [self activeTabHelper];
  if (!tabHelper) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  tabHelper->GeneratePageContext(
      base::BindRepeating(^(GeminiPageContext* activePageContext) {
        [weakSelf propagatePageContext:activePageContext];
      }));

  // Show page attachment UI chip every time full page context generation is
  // requested.
  ios::provider::RequestUIChange(
      ios::provider::GeminiUIElementType::kContextAttachment);
}

- (void)propagatePageContext:(GeminiPageContext*)pageContext {
  [self updatePageContextState:pageContext];
  [self.sharedTabsDelegate saveActivePageContextToSharedTabs:pageContext];

  ios::provider::UpdateActivePageContext(
      pageContext, [self.sharedTabsDelegate inactiveSharedTabs]);
}

- (void)updateFloatyWithPartialPageContext {
  GeminiTabHelper* tabHelper = [self activeTabHelper];
  if (!tabHelper) {
    return;
  }

  GeminiPageContext* activePageContext = tabHelper->GetPartialPageContext();
  [self propagatePageContext:activePageContext];
}

- (void)onActiveWebStateChanged:(web::WebState*)oldActive
                      newActive:(web::WebState*)newActive {
  if (oldActive) {
    GeminiTabHelper* oldTabHelper = GeminiTabHelper::FromWebState(oldActive);
    if (oldTabHelper && _tabHelperObserver) {
      oldTabHelper->RemoveObserver(_tabHelperObserver.get());
    }
  }

  if (newActive) {
    [self.sharedTabsDelegate updateSharedTabsForActiveWebState:newActive];
    GeminiTabHelper* newTabHelper = GeminiTabHelper::FromWebState(newActive);
    if (newTabHelper && _tabHelperObserver) {
      newTabHelper->AddObserver(_tabHelperObserver.get());
      [self onPageContextUpdated:newActive];
    }
  }
}

- (void)onPageContextUpdated:(web::WebState*)webState {
  // Update page context for Gemini Live only when the user is not speaking,
  // as when they start wording their query, the page context should be locked
  // in. Since we don't get a signal for user speaking, `kTranscribing` is used
  // as a proxy.
  if ([self isInGeminiLiveMode] &&
      _stateManager.processingStatus == GeminiClientMode::kTranscribing) {
    return;
  }

  // Make sure the given web_state is the active web state.
  web::WebState* activeWebState =
      _webStateList ? _webStateList->GetActiveWebState() : nullptr;
  if (!activeWebState || activeWebState != webState) {
    return;
  }

  [self updateFloatyWithPartialPageContext];
}

- (void)onWebStateListDestroyed {
  _webStateList = nullptr;
}

- (void)attachObservers {
  if (!_webStateList || _webStateListObserver) {
    return;
  }
  if (!_tabHelperObserver) {
    _tabHelperObserver =
        std::make_unique<GeminiContainerMediatorTabHelperObserver>(self);
  }
  _webStateListObserver =
      std::make_unique<GeminiContainerMediatorWebStateListObserver>(self);
  _webStateList->AddObserver(_webStateListObserver.get());
  if (GeminiTabHelper* activeTabHelper = [self activeTabHelper]) {
    activeTabHelper->AddObserver(_tabHelperObserver.get());
  }
}

- (void)detachObservers {
  GeminiTabHelper* activeTabHelper = [self activeTabHelper];
  if (activeTabHelper && _tabHelperObserver) {
    activeTabHelper->RemoveObserver(_tabHelperObserver.get());
  }
  if (_webStateList && _webStateListObserver) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
  }
  _tabHelperObserver.reset();
  _webStateListObserver.reset();
}

#pragma mark - Gemini Live

- (BOOL)isInGeminiLiveMode {
  return _stateManager.viewMode == GeminiViewMode::kLive &&
         gemini::IsFeatureAvailable(gemini::Feature::kLive, _profile);
}

// Updates page context for Gemini Live based on `processingStatus` changes.
- (void)updatePageContextForLiveProcessingStatus:
    (GeminiClientMode)processingStatus {
  if (![self isInGeminiLiveMode]) {
    return;
  }

  switch (processingStatus) {
    case GeminiClientMode::kTranscribing:
      [self requestActivePageContextGeneration];
      break;
    case GeminiClientMode::kResponding:
      // Update partial page context (i.e., live sharing context label) when
      // transitioning out of the transcribing (i.e., speaking) state.
      [self updateFloatyWithPartialPageContext];
      break;
    default:
      break;
  }
}

@end
