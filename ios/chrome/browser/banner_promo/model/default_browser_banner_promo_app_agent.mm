// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/banner_promo/model/default_browser_banner_promo_app_agent.h"

#import "base/ios/crb_protocol_observers.h"
#import "base/metrics/histogram_functions.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/google/core/common/google_util.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

// Simple struct to store observed data about SceneStates.
struct SceneStateData {
  bool isForeground;
  bool isIncognitoContentVisible;
  bool isUIEnabled;
};

@interface DefaultBrowserBannerAppAgentObserverList
    : CRBProtocolObservers <DefaultBrowserBannerAppAgentObserver>
@end
@implementation DefaultBrowserBannerAppAgentObserverList
@end

@interface DefaultBrowserBannerPromoAppAgent () <CRWWebStateObserver,
                                                 WebStateListObserving>

@end

@implementation DefaultBrowserBannerPromoAppAgent {
  // Observer bridge for observing web state lists.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;

  // Observer bridge for observing web states.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Stores the last URL visited for each web state to track navigations.
  std::map<web::WebStateID, GURL> _lastNavigatedURLs;

  // Stores the last observed data for scene states to help determine when the
  // observed data changes.
  std::map<SceneState*, SceneStateData> _sceneStateDatas;

  // Stored observers.
  DefaultBrowserBannerAppAgentObserverList* _observers;

  // Main profile state to use for promo eligibility checking.
  ProfileState* _mainProfileState;

  // Number of times the promo has been displayed in this promo session.
  int _sessionDisplayCount;

  // Sometimes the promo is hidden and re-shown. The Engagement Tracker should
  // only be informed of the first dismissal.
  BOOL _shouldAlertEngagementTrackerOfDismissal;
}

- (instancetype)init {
  self = [super init];
  if (self) {
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

- (void)setUICurrentlySupportsPromo:(BOOL)UICurrentlySupportsPromo {
  if (_UICurrentlySupportsPromo == UICurrentlySupportsPromo) {
    return;
  }

  _UICurrentlySupportsPromo = UICurrentlySupportsPromo;

  [self updatePromoState];
}

- (void)promoTapped {
  base::UmaHistogramCounts100("IOS.DefaultBrowserBannerPromo.Tapped",
                              _sessionDisplayCount);
  base::UmaHistogramEnumeration(
      "IOS.DefaultBrowserBannerPromo.PromoSessionEnded",
      IOSDefaultBrowserBannerPromoPromoSessionEndedReason::kUserTappedPromo);
  [self endPromoSession];
}

- (void)promoCloseButtonTapped {
  base::UmaHistogramCounts100("IOS.DefaultBrowserBannerPromo.ManuallyDismissed",
                              _sessionDisplayCount);
  base::UmaHistogramEnumeration(
      "IOS.DefaultBrowserBannerPromo.PromoSessionEnded",
      IOSDefaultBrowserBannerPromoPromoSessionEndedReason::kUserClosed);
  [self endPromoSession];
}

#pragma mark - Private

- (BOOL)navigationOccuredInWebState:(web::WebState*)webState
                  navigationContext:(web::NavigationContext*)navigationContext {
  auto iterator = _lastNavigatedURLs.find(webState->GetUniqueIdentifier());
  // New tabs are added with a blank GURL. Those should automatically count as
  // a navigation.
  if (iterator == _lastNavigatedURLs.end() || iterator->second == GURL()) {
    return YES;
  }

  // New url counts as a navigation.
  return iterator->second != navigationContext->GetUrl().GetWithoutRef() ||
         !navigationContext->IsSameDocument();
}

// Observes the given webstate and handles other tracking related to this.
- (void)startObservingWebState:(web::WebState*)webState {
  webState->AddObserver(_webStateObserverBridge.get());
  const GURL& currentURL = webState->GetLastCommittedURL().GetWithoutRef();
  _lastNavigatedURLs.insert_or_assign(webState->GetUniqueIdentifier(),
                                      currentURL);
}

// Stops observing the given webstate and cleans up other tracking related to
// this.
- (void)stopObservingWebState:(web::WebState*)webState {
  webState->RemoveObserver(_webStateObserverBridge.get());
  _lastNavigatedURLs.erase(webState->GetUniqueIdentifier());
}

// Handles scene state changes and updates the UI and observations as necessary.
- (void)sceneStateChangedData:(SceneState*)sceneState {
  if (!_sceneStateDatas[sceneState].isUIEnabled) {
    return;
  }

  Browser* mainBrowser =
      sceneState.browserProviderInterface.mainBrowserProvider.browser;
  if (!mainBrowser) {
    return;
  }

  WebStateList* webStateList = mainBrowser->GetWebStateList();
  if (!webStateList) {
    return;
  }

  web::WebState* activeMainWebState = webStateList->GetActiveWebState();

  if (_sceneStateDatas[sceneState].isForeground &&
      !_sceneStateDatas[sceneState].isIncognitoContentVisible) {
    webStateList->AddObserver(_webStateListObserverBridge.get());
    // Sometimes, like when opening a link in a new window, the scene state
    // doesn't start with an active web state. In this case, the first active
    // web state will be observed via the WebStateList observer methods.
    if (activeMainWebState) {
      [self startObservingWebState:activeMainWebState];
      [self updatePromoState];
    }
  } else {
    webStateList->RemoveObserver(_webStateListObserverBridge.get());
    if (activeMainWebState) {
      [self stopObservingWebState:activeMainWebState];
    }

    [self updatePromoState];
  }
}

// Makes sure the promo is shown and alerts observers if this causes a state
// change.
- (void)ensurePromoShown {
  if (!self.promoCurrentlyShown) {
    self.promoCurrentlyShown = YES;
    [_observers displayPromoFromAppAgent:self];
  }
}

// Makes sure the promo is hidden and alerts observers if this causes a state
// change.
- (void)ensurePromoHidden {
  if (self.promoCurrentlyShown) {
    feature_engagement::Tracker* engagementTracker =
        feature_engagement::TrackerFactory::GetForProfile(
            _mainProfileState.profile);
    if (engagementTracker && _shouldAlertEngagementTrackerOfDismissal) {
      _shouldAlertEngagementTrackerOfDismissal = NO;
      engagementTracker->Dismissed(
          feature_engagement::kIPHiOSDefaultBrowserBannerPromoFeature);
    }
    self.promoCurrentlyShown = NO;
    [_observers hidePromoFromAppAgent:self];
  }
}

// Ends any in-progress promo session and makes sure the promo UI is hidden.
- (void)endPromoSession {
  _sessionDisplayCount = 0;
  [self ensurePromoHidden];
}

// Checks if the promo can be displayed on all currently active pages.
- (BOOL)promoIsSuppressedOnCurrentURLs {
  // There must be at least one active URL to have a promo.
  if (_lastNavigatedURLs.size() == 0) {
    return true;
  }

  for (const auto& [webStateID, url] : _lastNavigatedURLs) {
    if (IsUrlNtp(url) || google_util::IsGoogleSearchUrl(url)) {
      return true;
    }
  }
  return false;
}

// Updates the promo state based on the current active URLs.
- (void)updatePromoState {
  for (const auto& [webStateID, url] : _lastNavigatedURLs) {
    // Tabs opened in new windows can start with a blank URL. Hold off on
    // changing state until it gets filled in from a navigation
    if (url == GURL()) {
      return;
    }
  }

  if (!self.UICurrentlySupportsPromo) {
    [self ensurePromoHidden];
    return;
  }

  // Check if an in-progress session should end.
  if ([self maybeEndInProgressPromoSession]) {
    return;
  }

  // There could be an in-progress promo session (display count > 0) even while
  // the promo is not currently shown when `UICurrentlySupportsPromo` is toggled
  // on.
  if (_sessionDisplayCount > 0) {
    _sessionDisplayCount += 1;
    [self ensurePromoShown];
    base::UmaHistogramCounts100("IOS.DefaultBrowserBannerPromo.Shown",
                                _sessionDisplayCount);
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
      _shouldAlertEngagementTrackerOfDismissal = YES;
      [self ensurePromoShown];
      base::UmaHistogramCounts100("IOS.DefaultBrowserBannerPromo.Shown",
                                  _sessionDisplayCount);
    }
  }
}

// Checks if there is an in-progress promo session and if it should be ended
// based on current stats. Returns `YES` if a session was ended, and `NO` if
// there was no in-progress session or it did not end.
- (BOOL)maybeEndInProgressPromoSession {
  if (_sessionDisplayCount == 0) {
    return NO;
  }

  // Figure out which metric to log. User interactions (close button, regular
  // tap) are handled elsewhere.
  if (IsChromeLikelyDefaultBrowser()) {
    base::UmaHistogramEnumeration(
        "IOS.DefaultBrowserBannerPromo.PromoSessionEnded",
        IOSDefaultBrowserBannerPromoPromoSessionEndedReason::kChromeNowDefault);
  } else if ([self promoIsSuppressedOnCurrentURLs]) {
    for (const auto& [webStateID, url] : _lastNavigatedURLs) {
      if (IsUrlNtp(url)) {
        base::UmaHistogramEnumeration(
            "IOS.DefaultBrowserBannerPromo.PromoSessionEnded",
            IOSDefaultBrowserBannerPromoPromoSessionEndedReason::
                kNavigationToNTP);
        break;
      } else if (google_util::IsGoogleSearchUrl(url)) {
        base::UmaHistogramEnumeration(
            "IOS.DefaultBrowserBannerPromo.PromoSessionEnded",
            IOSDefaultBrowserBannerPromoPromoSessionEndedReason::
                kNavigationToSRP);
        break;
      }
    }
  } else if (self.promoCurrentlyShown &&
             _sessionDisplayCount >=
                 kDefaultBrowserBannerPromoImpressionLimit.Get()) {
    // Session only ends due to meeting impression limit if the promo is
    // currently shown.
    base::UmaHistogramEnumeration(
        "IOS.DefaultBrowserBannerPromo.PromoSessionEnded",
        IOSDefaultBrowserBannerPromoPromoSessionEndedReason::kImpressionsMet);
  } else {
    // Otherwise, session should not end.
    return NO;
  }

  [self endPromoSession];
  return YES;
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState sceneConnected:(SceneState*)sceneState {
  [super appState:appState sceneConnected:sceneState];
  _sceneStateDatas[sceneState] = {
      .isForeground =
          sceneState.activationLevel >= SceneActivationLevelForegroundInactive,
      .isIncognitoContentVisible = sceneState.incognitoContentVisible,
      .isUIEnabled = sceneState.UIEnabled};
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    profileStateConnected:(ProfileState*)profileState {
  if (!IsDefaultBrowserBannerPromoEnabled()) {
    return;
  }
  _mainProfileState = profileState;
}

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  [super sceneState:sceneState transitionedToActivationLevel:level];
  if (!IsDefaultBrowserBannerPromoEnabled()) {
    return;
  }

  BOOL sceneIsForeground = level >= SceneActivationLevelForegroundInactive;

  // If no change in foreground state has happened, stop here.
  if (_sceneStateDatas[sceneState].isForeground == sceneIsForeground) {
    return;
  }

  _sceneStateDatas[sceneState].isForeground = sceneIsForeground;

  [self sceneStateChangedData:sceneState];
}

- (void)sceneState:(SceneState*)sceneState
    isDisplayingIncognitoContent:(BOOL)incognitoContentVisible {
  if (!IsDefaultBrowserBannerPromoEnabled()) {
    return;
  }

  if (_sceneStateDatas[sceneState].isIncognitoContentVisible ==
      incognitoContentVisible) {
    return;
  }

  _sceneStateDatas[sceneState].isIncognitoContentVisible =
      incognitoContentVisible;

  [self sceneStateChangedData:sceneState];
}

- (void)sceneStateDidEnableUI:(SceneState*)sceneState {
  if (!IsDefaultBrowserBannerPromoEnabled()) {
    return;
  }

  _sceneStateDatas[sceneState].isUIEnabled = true;

  // If the scene is not in the foreground yet, skip this change.
  if (!_sceneStateDatas[sceneState].isForeground) {
    return;
  }

  [self sceneStateChangedData:sceneState];
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

  // Make sure the promo state is correct now that a new web state is active.
  [self updatePromoState];
}

- (void)webStateListDestroyed:(WebStateList*)webStateList {
  webStateList->RemoveObserver(_webStateListObserverBridge.get());

  web::WebState* activeWebState = webStateList->GetActiveWebState();
  if (activeWebState) {
    [self stopObservingWebState:activeWebState];
  }

  [self updatePromoState];
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

  [self updatePromoState];
}

@end
