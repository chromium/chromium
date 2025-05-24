// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/banner_promo/model/default_browser_banner_promo_app_agent.h"

#import "base/ios/crb_protocol_observers.h"
#import "base/metrics/histogram_functions.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/google/core/common/google_util.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/banner_promo/model/default_browser_banner_promo_scene_observer.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "url/gurl.h"

@interface DefaultBrowserBannerAppAgentObserverList
    : CRBProtocolObservers <DefaultBrowserBannerAppAgentObserver>
@end
@implementation DefaultBrowserBannerAppAgentObserverList
@end

@implementation DefaultBrowserBannerPromoAppAgent {
  // Stores the scene observer for each scene.
  NSMapTable<SceneState*, DefaultBrowserBannerPromoSceneObserver*>*
      _sceneObservers;

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
    _sceneObservers = [NSMapTable weakToStrongObjectsMapTable];

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

- (void)updatePromoState {
  for (const auto& url : [self lastNavigatedURLs]) {
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

- (void)onSceneDisconnected:(SceneState*)sceneState {
  [_sceneObservers removeObjectForKey:sceneState];
}

- (void)setAppState:(AppState*)appState {
  [super setAppState:appState];
  for (SceneState* sceneState : [self.appState connectedScenes]) {
    [self appState:self.appState sceneConnected:sceneState];
  }
}

#pragma mark - Private

// Returns all the last navigated URLs from all of the current active scenes.
- (std::vector<GURL>)lastNavigatedURLs {
  std::vector<GURL> urls;
  for (SceneState* sceneState : _sceneObservers) {
    DefaultBrowserBannerPromoSceneObserver* observer =
        [_sceneObservers objectForKey:sceneState];
    if (observer.lastNavigatedURL) {
      urls.push_back(observer.lastNavigatedURL.value());
    }
  }
  return urls;
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
  std::vector<GURL> lastNavigatedURLs = [self lastNavigatedURLs];
  // There must be at least one active URL to have a promo.
  if (lastNavigatedURLs.size() == 0) {
    return true;
  }

  for (const auto& url : lastNavigatedURLs) {
    if (IsUrlNtp(url) || google_util::IsGoogleSearchUrl(url)) {
      return true;
    }
  }
  return false;
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
    for (const auto& url : [self lastNavigatedURLs]) {
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
  } else if (_sessionDisplayCount >=
             kDefaultBrowserBannerPromoImpressionLimit.Get()) {
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

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    profileStateConnected:(ProfileState*)profileState {
  if (!IsDefaultBrowserBannerPromoEnabled()) {
    return;
  }
  _mainProfileState = profileState;
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState sceneConnected:(SceneState*)sceneState {
  [super appState:appState sceneConnected:sceneState];
  DefaultBrowserBannerPromoSceneObserver* observer =
      [[DefaultBrowserBannerPromoSceneObserver alloc]
          initWithSceneState:sceneState
                    appAgent:self];
  [_sceneObservers setObject:observer forKey:sceneState];
}

@end
