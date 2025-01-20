// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/banner_promo/model/default_browser_banner_promo_app_agent.h"

#import "base/ios/crb_protocol_observers.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/google/core/common/google_util.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_observer_bridge.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

@interface DefaultBrowserBannerAppAgentObserverList
    : CRBProtocolObservers <DefaultBrowserBannerAppAgentObserver>
@end
@implementation DefaultBrowserBannerAppAgentObserverList
@end

@interface DefaultBrowserBannerPromoAppAgent () <BrowserListObserver,
                                                 CRWWebStateObserver,
                                                 ProfileStateObserver,
                                                 WebStateListObserving>

@end

@implementation DefaultBrowserBannerPromoAppAgent {
  // Observer bridge for observing browser lists.
  std::unique_ptr<BrowserListObserverBridge> _browserListObserverBridge;

  // Observer bridge for observing web state lists.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;

  // Observer bridge for observing web states.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Stores the last URL visited for each web state to track navigations.
  std::map<web::WebStateID, GURL> _lastNavigatedURLs;

  // Stored observers.
  DefaultBrowserBannerAppAgentObserverList* _observers;

  // Main profile state to use for promo eligibility checking.
  ProfileState* _mainProfileState;

  // Number of times the promo has been displayed in this promo session.
  int _sessionDisplayCount;

  // Whether the promo is currently shown.
  BOOL _promoCurrentlyShown;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _browserListObserverBridge =
        std::make_unique<BrowserListObserverBridge>(self);
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);

    _observers = [DefaultBrowserBannerAppAgentObserverList
        observersWithProtocol:@protocol(DefaultBrowserBannerAppAgentObserver)];
  }
  return self;
}

- (void)addObserver:(id<DefaultBrowserBannerAppAgentObserver>)observer {
  [_observers addObserver:observer];
}

- (void)removeObserver:(id<DefaultBrowserBannerAppAgentObserver>)observer {
  [_observers removeObserver:observer];
}

#pragma mark - Private

- (BOOL)navigationOccuredInWebState:(web::WebState*)webState
                  navigationContext:(web::NavigationContext*)navigationContext {
  auto iterator = _lastNavigatedURLs.find(webState->GetUniqueIdentifier());
  if (iterator == _lastNavigatedURLs.end()) {
    return YES;
  }
  // New url counts as a navigation.
  return iterator->second != navigationContext->GetUrl().GetWithoutRef() ||
         !navigationContext->IsSameDocument();
}

- (void)stopObservingWebState:(web::WebState*)webState {
  webState->RemoveObserver(_webStateObserverBridge.get());
  _lastNavigatedURLs.erase(webState->GetUniqueIdentifier());
}

- (void)startObservingWebState:(web::WebState*)webState {
  webState->AddObserver(_webStateObserverBridge.get());
  const GURL& currentURL = webState->GetLastCommittedURL().GetWithoutRef();
  if (currentURL != GURL()) {
    _lastNavigatedURLs.insert_or_assign(webState->GetUniqueIdentifier(),
                                        currentURL);
  }
}

// Makes sure the promo is shown and alerts observers if this causes a state
// change.
- (void)ensurePromoShown {
  if (!_promoCurrentlyShown) {
    _promoCurrentlyShown = YES;
    [_observers displayPromoFromAppAgent:self];
  }
}

// Makes sure the promo is hidden and alerts observers if this causes a state
// change.
- (void)ensurePromoHidden {
  if (_promoCurrentlyShown) {
    feature_engagement::Tracker* engagementTracker =
        feature_engagement::TrackerFactory::GetForProfile(
            _mainProfileState.profile);
    if (engagementTracker) {
      engagementTracker->Dismissed(
          feature_engagement::kIPHiOSDefaultBrowserBannerPromoFeature);
    }
    _promoCurrentlyShown = NO;
    [_observers hidePromoFromAppAgent:self];
  }
}

// Checks if the promo can be displayed on all currently active pages.
- (BOOL)promoIsSuppressedOnCurrentURLs {
  for (const auto& [webStateID, url] : _lastNavigatedURLs) {
    if (IsUrlNtp(url) || google_util::IsGoogleSearchUrl(url)) {
      return true;
    }
  }
  return false;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    profileStateConnected:(ProfileState*)profileState {
  if (!IsDefaultBrowserBannerPromoEnabled()) {
    return;
  }
  [profileState addObserver:self];
  _mainProfileState = profileState;
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage != ProfileInitStage::kFinal) {
    return;
  }

  BrowserList* browserList =
      BrowserListFactory::GetForProfile(profileState.profile);

  browserList->AddObserver(_browserListObserverBridge.get());

  // Make sure that already-existing browsers get handled.
  for (Browser* browser :
       browserList->BrowsersOfType(BrowserList::BrowserType::kRegular)) {
    [self browserList:browserList browserAdded:browser];
  }
}

#pragma mark - BrowserListObserver

- (void)browserList:(const BrowserList*)browserList
       browserAdded:(Browser*)browser {
  // Only observe web states in regular browsers.
  if (browser->type() != Browser::Type::kRegular) {
    return;
  }

  WebStateList* webStateList = browser->GetWebStateList();
  webStateList->AddObserver(_webStateListObserverBridge.get());

  web::WebState* webState = webStateList->GetActiveWebState();
  if (webState) {
    [self startObservingWebState:webState];
  }
}

- (void)browserList:(const BrowserList*)browserList
     browserRemoved:(Browser*)browser {
  WebStateList* webStateList = browser->GetWebStateList();
  webStateList->RemoveObserver(_webStateListObserverBridge.get());

  web::WebState* webState = webStateList->GetActiveWebState();
  if (webState) {
    [self stopObservingWebState:webState];
  }
}

- (void)browserListWillShutdown:(BrowserList*)browserList {
  // Make sure that already-existing browsers are cleaned up as well.
  for (Browser* browser :
       browserList->BrowsersOfType(BrowserList::BrowserType::kRegular)) {
    [self browserList:browserList browserRemoved:browser];
  }

  browserList->RemoveObserver(_browserListObserverBridge.get());
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  if (!status.active_web_state_change()) {
    return;
  }

  if (status.old_active_web_state) {
    [self stopObservingWebState:status.old_active_web_state];
  }
  if (status.new_active_web_state) {
    [self startObservingWebState:status.new_active_web_state];
  }
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigationContext {
  if (![self navigationOccuredInWebState:webState
                       navigationContext:navigationContext]) {
    return;
  }

  _lastNavigatedURLs.insert_or_assign(
      webState->GetUniqueIdentifier(),
      navigationContext->GetUrl().GetWithoutRef());

  if (_promoCurrentlyShown) {
    // Check if session is over.
    if (IsChromeLikelyDefaultBrowser() ||
        [self promoIsSuppressedOnCurrentURLs] ||
        _sessionDisplayCount >=
            kDefaultBrowserBannerPromoImpressionLimit.Get()) {
      [self ensurePromoHidden];
      return;
    }
    _sessionDisplayCount += 1;
  } else {
    // Check if session should begin.
    if (IsChromeLikelyDefaultBrowser() ||
        [self promoIsSuppressedOnCurrentURLs]) {
      return;
    }

    feature_engagement::Tracker* engagementTracker =
        feature_engagement::TrackerFactory::GetForProfile(
            _mainProfileState.profile);
    if (engagementTracker &&
        engagementTracker->ShouldTriggerHelpUI(
            feature_engagement::kIPHiOSDefaultBrowserBannerPromoFeature)) {
      _sessionDisplayCount = 1;
      [self ensurePromoShown];
    }
  }
}

@end
