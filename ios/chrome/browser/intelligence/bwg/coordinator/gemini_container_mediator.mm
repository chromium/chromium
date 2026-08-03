// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
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
                         target:(GeminiViewStateChangeHandlerTarget*)target {
  self = [super init];
  if (self) {
    if (browser) {
      _webStateList = browser->GetWebStateList();
      _profile = browser->GetProfile();
    }
    _gatewayManager = [[GeminiGatewayManager alloc] initWithBrowser:browser
                                                             target:target];
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

- (void)disconnect {
  [self onFloatyDismiss];

  _webStateList = nullptr;
  _profile = nullptr;
  [_gatewayManager disconnect];
  _gatewayManager = nil;
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
  config.contextualCueChipLabel = startupState.prepopulatedPrompt;
  config.entryPoint = startupState.entryPoint;
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

@end
