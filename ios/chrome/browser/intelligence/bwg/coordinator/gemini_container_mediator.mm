// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/assistant/coordinator/assistant_container_commands.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_view_controller.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_mediator_event_handler.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_gateway_manager.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_page_context.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_page_state_change_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_session_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_startup_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_feature_availability.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_prefs.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_gateway_protocol.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"
#import "ui/base/l10n/l10n_util.h"

@implementation GeminiContainerMediator {
  // WebStateList for the browser.
  raw_ptr<WebStateList> _webStateList;
  // Profile for the browser.
  raw_ptr<ProfileIOS> _profile;
  // Track if we have triggered feature engagement for Gemini Live IPH or New
  // Badge.
  BOOL _hasTriggeredGeminiLiveIPH;
  BOOL _hasTriggeredGeminiLiveNewBadge;
}

- (instancetype)initWithBrowser:(Browser*)browser
                   eventHandler:
                       (GeminiContainerMediatorEventHandler*)eventHandler {
  self = [super init];
  if (self) {
    _eventHandler = eventHandler;
    if (browser) {
      _webStateList = browser->GetWebStateList();
      _profile = browser->GetProfile();
    }
    _gatewayManager = [[GeminiGatewayManager alloc] initWithBrowser:browser
                                                  viewStateDelegate:self];
    if (self.gateway && browser) {
      [self configureGemini];
    }
  }
  return self;
}

#pragma mark - Property Getters

- (id<BWGGatewayProtocol>)gateway {
  return _gatewayManager.gateway;
}

#pragma mark - Public Methods

- (GeminiConfiguration*)
    createGeminiConfigurationForActiveWebState:(GeminiStartupState*)startupState
                            baseViewController:
                                (UIViewController*)baseViewController {
  web::WebState* webState =
      _webStateList ? _webStateList->GetActiveWebState() : nullptr;
  if (!webState) {
    return nil;
  }

  GeminiTabHelper* geminiTabHelper = GeminiTabHelper::FromWebState(webState);
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

- (void)configureGemini {
  if (!_profile) {
    return;
  }
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(_profile);
  if (!authService || !authService->HasPrimaryIdentity()) {
    return;
  }

  GeminiStartupConfiguration* config =
      [[GeminiStartupConfiguration alloc] init];
  config.authService = authService;
  config.gateway = self.gateway;
  config.imageRemixEnabled =
      gemini::IsFeatureAvailable(gemini::Feature::kImageRemix, _profile);
  config.geminiLiveEnabled =
      gemini::IsFeatureAvailable(gemini::Feature::kLive, _profile);

  ios::provider::ConfigureWithStartupConfiguration(config);
}

- (BOOL)shouldShowSuggestionChipsForEntryPoint:
    (gemini::EntryPoint)entryPoint {
  web::WebState* webState = _webStateList->GetActiveWebState();
  if (!webState) {
    return NO;
  }

  GeminiTabHelper* geminiTabHelper = GeminiTabHelper::FromWebState(webState);
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
}

- (void)setConsumer:(id<GeminiContainerConsumer>)consumer {
  CHECK(IsIOSGeminiBottomSheetMigrationEnabled());
  _consumer = consumer;
  if (_consumer) {
    [self setupInitialUIState];
  }
}

- (void)disconnect {
  [self onFloatyDismiss];

  _eventHandler = nullptr;
  _containerHandler = nil;
  _geminiHandler = nil;
  _consumer = nil;
  _webStateList = nullptr;
  _profile = nullptr;
  [_gatewayManager disconnect];
  _gatewayManager = nil;
}

#pragma mark - AssistantContainerDelegate

- (void)assistantContainerDidUpdateDetentHeights:
    (AssistantContainerViewController*)container {
  NSInteger collapsedHeight =
      [container heightForDetent:AssistantContainerDetent::kMinimized];
  NSInteger extendedHeight =
      [container heightForDetent:AssistantContainerDetent::kMedium];

  if (collapsedHeight > 0 && extendedHeight > 0) {
    ios::provider::UpdateDetentHeights(collapsedHeight, extendedHeight);
  }
}

- (void)assistantContainer:(AssistantContainerViewController*)container
           didChangeDetent:(AssistantContainerDetent)newDetent {
  // Ignore delegate notifications for detent changes that were triggered
  // programmatically. We should not dismiss if the container was minimized
  // programmatically.
  if (newDetent == self.detentSize) {
    return;
  }

  self.detentSize = newDetent;
  if (newDetent == AssistantContainerDetent::kMinimized && self.isZeroState &&
      IsChromeNextIaEnabled()) {
    [self.geminiHandler dismissGeminiFlowWithCompletion:nil];
  }
}

- (void)assistantContainerDidRequestDismissal:
    (AssistantContainerViewController*)container {
  [self.geminiHandler dismissGeminiFlowWithCompletion:nil];
}

#pragma mark - GeminiViewStateDelegate

- (void)didSwitchToViewState:(ios::provider::GeminiViewState)viewState {
  if (_eventHandler) {
    _eventHandler->OnViewStateChanged(viewState);
    _eventHandler->SetLastShownViewState(viewState);
  }
}

- (void)switchToViewState:(ios::provider::GeminiViewState)viewState {
  if (_eventHandler &&
      viewState == ios::provider::GeminiViewState::kCollapsed) {
    _eventHandler->CollapseFloatyIfInvoked();
  }
}

- (void)didUpdateProcessingStatus:
            (ios::provider::GeminiClientMode)processingStatus
                        sessionID:(NSString*)sessionID
                   conversationID:(NSString*)conversationID {
  if (_eventHandler) {
    _eventHandler->OnProcessingStatusChanged(
        processingStatus, ios::provider::GeminiDormantReason::kUnknown);
  }

  if (!IsIOSGeminiBottomSheetMigrationEnabled() ||
      _processingStatus == processingStatus) {
    return;
  }
  _processingStatus = processingStatus;
  [self updateUIState];
}

- (void)
    didUpdateProcessingStatus:(ios::provider::GeminiClientMode)processingStatus
                dormantReason:(ios::provider::GeminiDormantReason)dormantReason
                    sessionID:(NSString*)sessionID
               conversationID:(NSString*)conversationID {
  if (_eventHandler) {
    _eventHandler->OnProcessingStatusChanged(processingStatus, dormantReason);
  }

  if (!IsIOSGeminiBottomSheetMigrationEnabled() ||
      _processingStatus == processingStatus) {
    return;
  }
  _processingStatus = processingStatus;
  [self updateUIState];
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

- (void)didSwitchToMode:(ios::provider::GeminiViewMode)mode {
  if (_eventHandler) {
    _eventHandler->OnModeChanged(mode);
  }

  if (!IsIOSGeminiBottomSheetMigrationEnabled() || _viewMode == mode) {
    return;
  }
  _viewMode = mode;
  [self updateUIState];
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

  // Preserve the detent size that the container already has.
  self.hasGrabber = YES;
  self.zeroState = YES;
}

#pragma mark - Property Setters

- (void)setHasGrabber:(BOOL)hasGrabber {
  if (_hasGrabber == hasGrabber) {
    return;
  }
  _hasGrabber = hasGrabber;
  [self.containerHandler setAssistantContainerGrabberHidden:!hasGrabber
                                                   animated:YES];
}

- (void)setDetentSize:(AssistantContainerDetent)detentSize {
  if (_detentSize == detentSize) {
    return;
  }
  _detentSize = detentSize;
  [self.containerHandler animateAssistantContainerToDetent:detentSize];
}

- (void)setZeroState:(BOOL)zeroState {
  if (_zeroState == zeroState) {
    return;
  }
  _zeroState = zeroState;
  [self.consumer setZeroState:zeroState];
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
      [self shouldShowSuggestionChipsForEntryPoint:startupState.entryPoint];
  if (IsAppSwitcherAISummarizationEnabled() &&
      startupState.isMismatchedAccount) {
    config.shouldShowAccountSnackbar = YES;
  }
  config.contextualCueChipLabel = startupState.prepopulatedPrompt;
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

// Sets up the initial UI state for the container.
- (void)setupInitialUIState {
  // Default values for mode and processing status. Actual values driven by SDK.
  _viewMode = ios::provider::GeminiViewMode::kFloaty;
  _processingStatus = ios::provider::GeminiClientMode::kDormant;

  // Default values for container UI properties. Driven by
  // `GeminiContainerMediator` based on mode and processing status.
  self.detentSize = AssistantContainerDetent::kMedium;
  self.hasGrabber = YES;

  // TODO(crbug.com/545204121): Load previous conversion instead if applicable.
  self.zeroState = YES;

  // In initial zero state the view shouldn't be focused for input.
  [self.consumer dismissKeyboard];
}

// Decides on container UI properties based on the current Gemini view mode and
// processing status.
- (void)updateUIState {
  if (_viewMode == ios::provider::GeminiViewMode::kLive) {
    self.detentSize = AssistantContainerDetent::kMinimized;
    self.hasGrabber = NO;
    self.zeroState = NO;
    return;
  }

  switch (_processingStatus) {
    case ios::provider::GeminiClientMode::kThinking:
      self.detentSize = AssistantContainerDetent::kMinimized;
      self.hasGrabber = NO;
      self.zeroState = NO;
      break;
    case ios::provider::GeminiClientMode::kResponding:
    case ios::provider::GeminiClientMode::kDormant:
    case ios::provider::GeminiClientMode::kPreviousConversationLoading:
      self.detentSize = AssistantContainerDetent::kMedium;
      self.hasGrabber = YES;
      self.zeroState = NO;
      break;
    case ios::provider::GeminiClientMode::kListening:
    case ios::provider::GeminiClientMode::kTranscribing:
    case ios::provider::GeminiClientMode::kUnknown:
      NOTREACHED();
  }
}

@end
