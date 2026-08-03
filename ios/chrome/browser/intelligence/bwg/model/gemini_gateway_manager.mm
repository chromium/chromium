// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_gateway_manager.h"

#import "base/memory/raw_ptr.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_actuation_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_camera_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_consent_provider_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_link_opening_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_page_state_change_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_session_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_suggestion_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_picker_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_view_state_change_handler.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_feature_availability.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_picker_commands.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_gateway_protocol.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"

@implementation GeminiGatewayManager

- (instancetype)initWithBrowser:(Browser*)browser
                         target:(GeminiViewStateChangeHandlerTarget*)target {
  self = [super init];
  if (self) {
    _gateway = ios::provider::CreateGeminiGateway();
    if (_gateway && browser) {
      [self setUpHandlersWithBrowser:browser target:target];
    }
  }
  return self;
}

- (void)setUpHandlersWithBrowser:(Browser*)browser
                          target:(GeminiViewStateChangeHandlerTarget*)target {
  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  WebStateList* webStateList = browser->GetWebStateList();
  ProfileIOS* profile = browser->GetProfile();

  _linkOpeningHandler = [[GeminiLinkOpeningHandler alloc]
      initWithURLLoader:UrlLoadingBrowserAgent::FromBrowser(browser)
             dispatcher:dispatcher];
  _pageStateChangeHandler = [[GeminiPageStateChangeHandler alloc]
      initWithPrefService:profile->GetPrefs()];
  _gateway.pageStateChangeHandler = _pageStateChangeHandler;

  _sessionHandler = [[GeminiSessionHandler alloc]
      initWithWebStateList:webStateList
                   tracker:feature_engagement::TrackerFactory::GetForProfile(
                               profile)
               prefService:profile->GetPrefs()];

  if (target) {
    _viewStateHandler =
        [[GeminiViewStateChangeHandler alloc] initWithTarget:target];
    _sessionHandler.geminiViewStateDelegate = _viewStateHandler;
    _linkOpeningHandler.geminiViewStateDelegate = _viewStateHandler;
  }

  _gateway.sessionHandler = _sessionHandler;
  _gateway.linkOpeningHandler = _linkOpeningHandler;

  _suggestionHandler =
      [[GeminiSuggestionHandler alloc] initWithWebStateList:webStateList];
  _gateway.suggestionHandler = _suggestionHandler;

  _consentProviderHandler = [[GeminiConsentProviderHandler alloc]
      initWithPrefService:profile->GetPrefs()];
  _gateway.consentProviderHandler = _consentProviderHandler;

  if (gemini::IsFeatureAvailable(gemini::Feature::kImageRemix, profile)) {
    _cameraHandler =
        [[GeminiCameraHandler alloc] initWithPrefService:profile->GetPrefs()];
    _gateway.cameraHandler = _cameraHandler;
  }

  if (IsGeminiMultiTabContextEnabled()) {
    _tabPickerHandler = [[GeminiTabPickerHandler alloc] init];
    _tabPickerHandler.tabPickerHandler =
        static_cast<id<TabPickerCommands>>(dispatcher);
    _tabPickerHandler.snackbarHandler =
        static_cast<id<SnackbarCommands>>(dispatcher);
    _gateway.tabPickerHandler = _tabPickerHandler;
  }

  if (IsGeminiActorEnabled()) {
    _actuationHandler = [[GeminiActuationHandler alloc]
        initWithActorService:actor::ActorServiceFactory::GetForProfile(profile)
                webStateList:webStateList];
    _gateway.actuationHandler = _actuationHandler;
  }
}

- (void)disconnect {
  [_linkOpeningHandler disconnect];
  _linkOpeningHandler = nil;
  [_viewStateHandler disconnect];
  _viewStateHandler = nil;
  [_consentProviderHandler disconnect];
  _consentProviderHandler = nil;
  _sessionHandler = nil;
  _pageStateChangeHandler = nil;
  _cameraHandler = nil;
  _tabPickerHandler = nil;
  _actuationHandler = nil;
  _suggestionHandler = nil;
  _gateway = nil;
}

@end
