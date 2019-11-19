// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"

#import <CoreLocation/CoreLocation.h>
#import <UIKit/UIKit.h>

#include <string>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/version.h"
#include "components/google/core/common/google_util.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/geolocation/CLLocation+OmniboxGeolocation.h"
#import "ios/chrome/browser/geolocation/CLLocation+XGeoHeader.h"
#import "ios/chrome/browser/geolocation/location_manager.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_authorization_alert.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_config.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller+Testing.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_local_state.h"
#import "ios/web/public/browser_state.h"
#include "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Values for the histogram that records whether we sent the X-Geo header for
// an Omnibox query or why we did not do so. These match the definition of
// GeolocationHeaderSentOrNot in Chromium
// src-internal/tools/histograms/histograms.xml.
typedef enum {
  // The user disabled location for Google.com (not used by Chrome iOS).
  kHeaderStateNotSentAuthorizationGoogleDenied = 0,
  // The user has not yet determined Chrome's access to the current device
  // location or Chrome's use of geolocation for Omnibox queries.
  kHeaderStateNotSentAuthorizationNotDetermined,
  // The current device location is not available.
  kHeaderStateNotSentLocationNotAvailable,
  // The current device location is stale.
  kHeaderStateNotSentLocationStale,
  // The X-Geo header was sent.
  kHeaderStateSent,
  // The user denied Chrome from accessing the current device location.
  kHeaderStateNotSentAuthorizationChromeDenied,
  // The user denied Chrome from using geolocation for Omnibox queries.
  kHeaderStateNotSentAuthorizationOmniboxDenied,
  // The user's Google search domain is not whitelisted.
  kHeaderStateNotSentDomainNotWhitelisted,
  // The number of possible of HeaderState values to report.
  kHeaderStateCount,
} HeaderState;

// Values for the histograms that record the user's action when prompted to
// authorize the use of location by Chrome. These match the definition of
// GeolocationAuthorizationAction in Chromium
// src-internal/tools/histograms/histograms.xml.
typedef enum {
  // The user authorized use of location.
  kAuthorizationActionAuthorized = 0,
  // The user permanently denied use of location (Don't Allow).
  kAuthorizationActionPermanentlyDenied,
  // The user denied use of location at this prompt (Not Now).
  kAuthorizationActionDenied,
  // The number of possible AuthorizationAction values to report.
  kAuthorizationActionCount,
} AuthorizationAction;

// Name of the histogram recording HeaderState.
const char* const kGeolocationHeaderSentOrNotHistogram =
    "Geolocation.HeaderSentOrNot";

// Name of the histogram recording AuthorizationAction for an existing user.
const char* const kGeolocationAuthorizationActionExistingUser =
    "Geolocation.AuthorizationActionExistingUser";

// Name of the histogram recording AuthorizationAction for a new user.
const char* const kGeolocationAuthorizationActionNewUser =
    "Geolocation.AuthorizationActionNewUser";

}  // anonymous namespace

@interface OmniboxGeolocationController () <
    CRWWebStateObserver,
    LocationManagerDelegate,
    OmniboxGeolocationAuthorizationAlertDelegate> {
  OmniboxGeolocationLocalState* localState_;
  LocationManager* locationManager_;
  OmniboxGeolocationAuthorizationAlert* authorizationAlert_;

  // Bridge to observe the web state from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> webStateObserverBridge_;

  // Records whether we have deliberately presented the system prompt, so that
  // we can record the user's action in
  // locationManagerDidChangeAuthorizationStatus:.
  BOOL systemPrompt_;

  // Records whether we are prompting for a new user, so that we can record the
  // user's action to the right histogram (either
  // kGeolocationAuthorizationActionExistingUser or
  // kGeolocationAuthorizationActionNewUser).
  BOOL newUser_;
}

// Boolean value indicating whether geolocation is enabled for Omnibox queries.
@property(nonatomic, readonly) BOOL enabled;

// Convenience property lazily initializes |localState_|.
@property(nonatomic, readonly) OmniboxGeolocationLocalState* localState;

// Convenience property lazily initializes |locationManager_|.
@property(nonatomic, readonly) LocationManager* locationManager;

// A pointer to the current active webState to reload in case of authorization
// changes. This WebState will be observed and the pointer will be set to null
// in webStateDestroyed.
@property(nonatomic) web::WebState* webStateToReload;

// Returns YES if and only if |url| and |transition| specify an Omnibox query
// that is eligible for geolocation.
- (BOOL)URLIsEligibleQueryURL:(const GURL&)url
                   transition:(ui::PageTransition)transition;

// Returns YES if and only if |url| and |transition| specify an Omnibox query.
//
// Note: URLIsQueryURL:transition: is more liberal than
// URLIsEligibleQueryURL:transition:. Use URLIsEligibleQueryURL:transition: and
// not URLIsQueryURL:transition: to test Omnibox query URLs with respect to
// sending location to Google.
- (BOOL)URLIsQueryURL:(const GURL&)url
           transition:(ui::PageTransition)transition;

// Returns YES if and only if |url| specifies a page for which we will prompt
// the user to authorize the use of geolocation for Omnibox queries.
- (BOOL)URLIsAuthorizationPromptingURL:(const GURL&)url;

// Starts updating device location if needed.
- (void)startUpdatingLocation;
// Stops updating device location.
- (void)stopUpdatingLocation;
// If the current location is not stale, then adds the current location to the
// current session entry for |webState| and reloads |webState|. If the current
// location is stale, then does nothing.
- (void)addLocationAndReloadWebState:(web::WebState*)webState;
// Returns YES if and only if we should show an alert that prompts the user to
// authorize using geolocation for Omnibox queries.
- (BOOL)shouldShowAuthorizationAlert;
// Shows an alert that prompts the user to authorize using geolocation for
// Omnibox queries. Sets |webStateToReload| from |webState|, so that we can
// reload |webState| if the user authorizes using geolocation.
- (void)showAuthorizationAlertForWebState:(web::WebState*)webState;
// Records |headerState| for the |kGeolocationHeaderSentOrNotHistogram|
// histogram.
- (void)recordHeaderState:(HeaderState)headerState;
// Records |authorizationAction|.
- (void)recordAuthorizationAction:(AuthorizationAction)authorizationAction;

@end

@implementation OmniboxGeolocationController

+ (OmniboxGeolocationController*)sharedInstance {
  static OmniboxGeolocationController* instance =
      [[OmniboxGeolocationController alloc] init];
  return instance;
}

- (void)triggerSystemPromptForNewUser:(BOOL)newUser {
  if (self.locationManager.locationServicesEnabled &&
      self.locationManager.authorizationStatus ==
          kCLAuthorizationStatusNotDetermined) {
    // Set |systemPrompt_|, so that
    // locationManagerDidChangeAuthorizationStatus: will know to handle any
    // CLAuthorizationStatus changes.
    //
    // TODO(crbug.com/661996): Remove the now useless
    // kAuthorizationStateNotDeterminedSystemPrompt from
    // omnibox_geolocation_local_state.h.
    systemPrompt_ = YES;
    self.localState.authorizationState =
        geolocation::kAuthorizationStateNotDeterminedSystemPrompt;

    // Turn on location updates, so that iOS will prompt the user.
    [self startUpdatingLocation];
    self.webStateToReload = nullptr;
    newUser_ = newUser;
  }
}

- (void)locationBarDidBecomeFirstResponder:
    (ios::ChromeBrowserState*)browserState {
  if (self.enabled && browserState && !browserState->IsOffTheRecord()) {
    [self startUpdatingLocation];
  }
}

- (void)locationBarDidResignFirstResponder:
    (ios::ChromeBrowserState*)browserState {
  // It's always okay to stop updating location.
  [self stopUpdatingLocation];
}

- (void)locationBarDidSubmitURL {
  // Stop updating the location when the user submits a query from the Omnibox.
  // We're not interested in further updates until the next time the user puts
  // the focus on the Omnbox.
  [self stopUpdatingLocation];
}

- (BOOL)addLocationToNavigationItem:(web::NavigationItem*)item
                       browserState:(web::BrowserState*)browserState {
  // If this is incognito mode or is not an Omnibox query, then do nothing.
  //
  // Check the URL with URLIsQueryURL:transition: here and not
  // URLIsEligibleQueryURL:transition:, because we want to log the cases where
  // we did not send the X-Geo header due to the Google search domain not being
  // whitelisted.
  DCHECK(item);
  const GURL& url = item->GetURL();
  if (!browserState || browserState->IsOffTheRecord() ||
      ![self URLIsQueryURL:url transition:item->GetTransitionType()]) {
    return NO;
  }

  if (![[OmniboxGeolocationConfig sharedInstance] URLHasEligibleDomain:url]) {
    [self recordHeaderState:kHeaderStateNotSentDomainNotWhitelisted];
    return NO;
  }

  // At this point, we should only have Omnibox query URLs that are eligible
  // for geolocation.
  DCHECK([self URLIsEligibleQueryURL:url transition:item->GetTransitionType()]);

  HeaderState headerState;
  if (!self.locationManager.locationServicesEnabled) {
    headerState = kHeaderStateNotSentAuthorizationChromeDenied;
  } else {
    switch (self.localState.authorizationState) {
      case geolocation::kAuthorizationStateNotDeterminedWaiting:
      case geolocation::kAuthorizationStateNotDeterminedSystemPrompt:
        if (self.locationManager.authorizationStatus ==
                kCLAuthorizationStatusNotDetermined ||
            [self shouldShowAuthorizationAlert]) {
          headerState = kHeaderStateNotSentAuthorizationNotDetermined;
        } else {
          DCHECK(self.locationManager.authorizationStatus ==
                     kCLAuthorizationStatusAuthorizedAlways ||
                 self.locationManager.authorizationStatus ==
                     kCLAuthorizationStatusAuthorizedWhenInUse);
          headerState = kHeaderStateNotSentAuthorizationOmniboxDenied;
        }
        break;

      case geolocation::kAuthorizationStateDenied:
        switch (self.locationManager.authorizationStatus) {
          case kCLAuthorizationStatusNotDetermined:
            NOTREACHED();
            // To keep the compiler quiet about headerState not being
            // initialized in this switch case.
            headerState = kHeaderStateNotSentAuthorizationChromeDenied;
            break;
          case kCLAuthorizationStatusRestricted:
          case kCLAuthorizationStatusDenied:
            headerState = kHeaderStateNotSentAuthorizationChromeDenied;
            break;
          case kCLAuthorizationStatusAuthorizedAlways:
          case kCLAuthorizationStatusAuthorizedWhenInUse:
            headerState = kHeaderStateNotSentAuthorizationOmniboxDenied;
            break;
        }
        break;

      case geolocation::kAuthorizationStateAuthorized: {
        DCHECK(self.enabled);
        CLLocation* currentLocation = [self.locationManager currentLocation];
        if (!currentLocation) {
          headerState = kHeaderStateNotSentLocationNotAvailable;
        } else if (![currentLocation cr_isFreshEnough]) {
          headerState = kHeaderStateNotSentLocationStale;
        } else {
          NSDictionary* locationHTTPHeaders =
              @{ @"X-Geo" : [currentLocation cr_xGeoString] };
          item->AddHttpRequestHeaders(locationHTTPHeaders);
          headerState = kHeaderStateSent;
        }
        break;
      }
    }
  }

  [self recordHeaderState:headerState];
  return headerState == kHeaderStateSent;
}

- (void)finishPageLoadForWebState:(web::WebState*)webState
                      loadSuccess:(BOOL)loadSuccess {
  if (!loadSuccess || !webState->GetBrowserState() ||
      webState->GetBrowserState()->IsOffTheRecord()) {
    return;
  }

  web::NavigationItem* item =
      webState->GetNavigationManager()->GetVisibleItem();

  if (!item) {
    // TODO(crbug.com/899827): remove this early return once committed
    // navigation item always exists after WebStateObserver::PageLoaded.
    return;
  }

  if (![self URLIsAuthorizationPromptingURL:item->GetURL()] ||
      !self.locationManager.locationServicesEnabled) {
    return;
  }

  switch (self.locationManager.authorizationStatus) {
    case kCLAuthorizationStatusNotDetermined:
      // Prompt the user with the iOS system location authorization alert.
      //
      // Set |systemPrompt_|, so that
      // locationManagerDidChangeAuthorizationStatus: will know that any
      // CLAuthorizationStatus changes are coming from this specific prompt.
      systemPrompt_ = YES;
      self.localState.authorizationState =
          geolocation::kAuthorizationStateNotDeterminedSystemPrompt;
      [self startUpdatingLocation];

      // Save this webState in case we're able to transition to
      // kAuthorizationStateAuthorized.
      self.webStateToReload = webState;
      break;

    case kCLAuthorizationStatusRestricted:
    case kCLAuthorizationStatusDenied:
      break;

    case kCLAuthorizationStatusAuthorizedAlways:
    case kCLAuthorizationStatusAuthorizedWhenInUse:
      // We might be in state kAuthorizationStateNotDeterminedSystemPrompt here
      // if we presented the iOS system location alert when
      // [CLLocationManager authorizationStatus] was
      // kCLAuthorizationStatusNotDetermined but the user managed to authorize
      // the app through some other flow; this might happen if the user
      // backgrounded the app or the app crashed. If so, then reset the state.
      if (self.localState.authorizationState ==
          geolocation::kAuthorizationStateNotDeterminedSystemPrompt) {
        self.localState.authorizationState =
            geolocation::kAuthorizationStateNotDeterminedWaiting;
      }
      // If the user has authorized the app to use location but not yet
      // explicitly authorized or denied using geolocation for Omnibox queries,
      // then present an alert.
      if (self.localState.authorizationState ==
              geolocation::kAuthorizationStateNotDeterminedWaiting &&
          [self shouldShowAuthorizationAlert]) {
        [self showAuthorizationAlertForWebState:webState];
      }
      break;
  }
}

#pragma mark - Private

- (BOOL)enabled {
  return self.locationManager.locationServicesEnabled &&
         self.localState.authorizationState ==
             geolocation::kAuthorizationStateAuthorized;
}

- (OmniboxGeolocationLocalState*)localState {
  if (!localState_) {
    localState_ = [[OmniboxGeolocationLocalState alloc]
        initWithLocationManager:self.locationManager];
  }
  return localState_;
}

- (LocationManager*)locationManager {
  if (!locationManager_) {
    locationManager_ = [[LocationManager alloc] init];
    [locationManager_ setDelegate:self];
  }
  return locationManager_;
}

- (void)setWebStateToReload:(web::WebState*)webState {
  if (webState == _webStateToReload)
    return;

  if (!webStateObserverBridge_) {
    webStateObserverBridge_ =
        std::make_unique<web::WebStateObserverBridge>(self);
  }
  if (_webStateToReload)
    _webStateToReload->RemoveObserver(webStateObserverBridge_.get());
  if (webState)
    webState->AddObserver(webStateObserverBridge_.get());
  _webStateToReload = webState;
}

- (BOOL)URLIsEligibleQueryURL:(const GURL&)url
                   transition:(ui::PageTransition)transition {
  return [self URLIsQueryURL:url transition:transition] &&
         [[OmniboxGeolocationConfig sharedInstance] URLHasEligibleDomain:url];
}

- (BOOL)URLIsQueryURL:(const GURL&)url
           transition:(ui::PageTransition)transition {
  if (google_util::IsGoogleSearchUrl(url) &&
      (transition & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) != 0) {
    ui::PageTransition coreTransition = static_cast<ui::PageTransition>(
        transition & ui::PAGE_TRANSITION_CORE_MASK);
    if (PageTransitionCoreTypeIs(coreTransition,
                                 ui::PAGE_TRANSITION_GENERATED) ||
        PageTransitionCoreTypeIs(coreTransition, ui::PAGE_TRANSITION_RELOAD)) {
      return YES;
    }
  }
  return NO;
}

- (BOOL)URLIsAuthorizationPromptingURL:(const GURL&)url {
  // Per PRD: "Show a modal dialog upon reaching google.com or a search results
  // page..." However, we only want to do this for domains where we will send
  // location.
  return (google_util::IsGoogleHomePageUrl(url) ||
          google_util::IsGoogleSearchUrl(url)) &&
         [[OmniboxGeolocationConfig sharedInstance] URLHasEligibleDomain:url];
}

- (void)startUpdatingLocation {
  // Note that GeolocationUpdater will stop itself automatically after 5
  // seconds.
  [self.locationManager startUpdatingLocation];
}

- (void)stopUpdatingLocation {
  // Note that we don't need to initialize |locationManager_| here. If it's
  // nil, then it's not running.
  [locationManager_ stopUpdatingLocation];
}

- (void)addLocationAndReloadWebState:(web::WebState*)webState {
  if (self.enabled && webState) {
    // Make sure that GeolocationUpdater is running the first time we request
    // the current location.
    //
    // If GeolocationUpdater is not running, then it returns nil for the
    // current location. That's normally okay, because we cache the most recent
    // location in LocationManager. However, we arrive here when the user first
    // authorizes us to use location, so we may not have ever started
    // GeolocationUpdater.
    [self startUpdatingLocation];

    web::NavigationManager* navigationManager =
        webState->GetNavigationManager();
    web::NavigationItem* item = navigationManager->GetVisibleItem();
    if (item &&
        [self addLocationToNavigationItem:item
                             browserState:webState->GetBrowserState()]) {
      navigationManager->Reload(web::ReloadType::NORMAL,
                                false /* check_for_repost */);
    }
  }
}

- (BOOL)shouldShowAuthorizationAlert {
  base::Version previousVersion(self.localState.lastAuthorizationAlertVersion);
  if (!previousVersion.IsValid())
    return YES;

  const base::Version& currentVersion = version_info::GetVersion();
  DCHECK(currentVersion.IsValid());
  return currentVersion.components()[0] != previousVersion.components()[0];
}

- (void)showAuthorizationAlertForWebState:(web::WebState*)webState {
  // Save this webState in case we're able to transition to
  // kAuthorizationStateAuthorized.
  self.webStateToReload = webState;

  authorizationAlert_ =
      [[OmniboxGeolocationAuthorizationAlert alloc] initWithDelegate:self];
  [authorizationAlert_ showAuthorizationAlert];

  self.localState.lastAuthorizationAlertVersion =
      version_info::GetVersionNumber();
}

- (void)recordHeaderState:(HeaderState)headerState {
  UMA_HISTOGRAM_ENUMERATION(kGeolocationHeaderSentOrNotHistogram, headerState,
                            kHeaderStateCount);
}

- (void)recordAuthorizationAction:(AuthorizationAction)authorizationAction {
  if (newUser_) {
    newUser_ = NO;

    UMA_HISTOGRAM_ENUMERATION(kGeolocationAuthorizationActionNewUser,
                              authorizationAction, kAuthorizationActionCount);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kGeolocationAuthorizationActionExistingUser,
                              authorizationAction, kAuthorizationActionCount);
  }
}

#pragma mark - LocationManagerDelegate

- (void)locationManagerDidChangeAuthorizationStatus:
    (LocationManager*)locationManager {
  if (systemPrompt_) {
    switch (self.locationManager.authorizationStatus) {
      case kCLAuthorizationStatusNotDetermined:
        // We may get a spurious notification about a transition to
        // |kCLAuthorizationStatusNotDetermined| when we first start location
        // services. Ignore it and don't reset |systemPrompt_| until we get a
        // real change.
        break;

      case kCLAuthorizationStatusRestricted:
      case kCLAuthorizationStatusDenied:
        self.localState.authorizationState =
            geolocation::kAuthorizationStateDenied;
        systemPrompt_ = NO;

        [self recordAuthorizationAction:kAuthorizationActionPermanentlyDenied];
        break;

      case kCLAuthorizationStatusAuthorizedAlways:
      case kCLAuthorizationStatusAuthorizedWhenInUse:
        self.localState.authorizationState =
            geolocation::kAuthorizationStateAuthorized;
        systemPrompt_ = NO;

        [self addLocationAndReloadWebState:self.webStateToReload];
        self.webStateToReload = nullptr;

        [self recordAuthorizationAction:kAuthorizationActionAuthorized];
        break;
    }
  }
}

#pragma mark - OmniboxGeolocationAuthorizationAlertDelegate

- (void)authorizationAlertDidAuthorize:
    (OmniboxGeolocationAuthorizationAlert*)authorizationAlert {
  self.localState.authorizationState =
      geolocation::kAuthorizationStateAuthorized;

  [self addLocationAndReloadWebState:self.webStateToReload];

  authorizationAlert_ = nil;
  self.webStateToReload = nullptr;

  [self recordAuthorizationAction:kAuthorizationActionAuthorized];
}

- (void)authorizationAlertDidCancel:
    (OmniboxGeolocationAuthorizationAlert*)authorizationAlert {
  // Leave authorization state as undetermined (not kAuthorizationStateDenied).
  // We won't use location, but we'll still be able to prompt at the next
  // application update.

  authorizationAlert_ = nil;
  self.webStateToReload = nullptr;

  [self recordAuthorizationAction:kAuthorizationActionDenied];
}

#pragma mark - OmniboxGeolocationController+Testing

- (void)setLocalState:(OmniboxGeolocationLocalState*)localState {
  localState_ = localState;
}

- (void)setLocationManager:(LocationManager*)locationManager {
  locationManager_ = locationManager;
}

#pragma mark - CRWWebStateObserver Methods

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(webState, _webStateToReload);
  self.webStateToReload = nullptr;
}

@end
