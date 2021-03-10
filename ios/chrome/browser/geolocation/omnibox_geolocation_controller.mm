// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"

#import <CoreLocation/CoreLocation.h>
#import <UIKit/UIKit.h>

#include <string>

#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/version.h"
#include "components/google/core/common/google_util.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/app/tests_hook.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/geolocation/CLLocation+OmniboxGeolocation.h"
#import "ios/chrome/browser/geolocation/CLLocation+XGeoHeader.h"
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

// Name of the histogram recording AuthorizationAction for an existing user.
const char* const kGeolocationAuthorizationActionExistingUser =
    "Geolocation.AuthorizationActionExistingUser";

// Name of the histogram recording AuthorizationAction for a new user.
const char* const kGeolocationAuthorizationActionNewUser =
    "Geolocation.AuthorizationActionNewUser";

}  // anonymous namespace

@interface OmniboxGeolocationController () <
    CLLocationManagerDelegate,
    OmniboxGeolocationAuthorizationAlertDelegate> {
  CLLocationManager* _locationManager;
  OmniboxGeolocationLocalState* _localState;
  OmniboxGeolocationAuthorizationAlert* _authorizationAlert;

  // Records whether we have deliberately presented the system prompt, so that
  // we can record the user's action in
  // locationManagerDidChangeAuthorization:.
  BOOL _systemPrompt;

  // Records whether we are prompting for a new user, so that we can record the
  // user's action to the right histogram (either
  // kGeolocationAuthorizationActionExistingUser or
  // kGeolocationAuthorizationActionNewUser).
  BOOL _newUser;
}

// Boolean value indicating whether geolocation is enabled for Omnibox queries.
@property(nonatomic, readonly) BOOL enabled;

// Convenience property lazily initializes |localState_|.
@property(nonatomic, readonly) OmniboxGeolocationLocalState* localState;

@property(nonatomic, strong) CLLocationManager* locationManager;

// Returns YES if and only if |url| specifies a page for which we will prompt
// the user to authorize the use of geolocation for Omnibox queries.
- (BOOL)URLIsAuthorizationPromptingURL:(const GURL&)url;

// Returns YES if and only if we should show an alert that prompts the user to
// authorize using geolocation for Omnibox queries.
- (BOOL)shouldShowAuthorizationAlert;
// Shows an alert that prompts the user to authorize using geolocation for
// Omnibox queries.
- (void)showAuthorizationAlertForWebState:(web::WebState*)webState;
// Records |authorizationAction|.
- (void)recordAuthorizationAction:(AuthorizationAction)authorizationAction;

@end

@implementation OmniboxGeolocationController

+ (OmniboxGeolocationController*)sharedInstance {
  static OmniboxGeolocationController* instance =
      [[OmniboxGeolocationController alloc] init];
  return instance;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _locationManager = [[CLLocationManager alloc] init];
    [_locationManager setDelegate:self];
  }
  return self;
}

- (void)triggerSystemPromptForNewUser:(BOOL)newUser {
  if (self.locationServicesEnabled && CLLocationManager.authorizationStatus ==
                                          kCLAuthorizationStatusNotDetermined) {
    // Set |systemPrompt_|, so that
    // locationManagerDidChangeAuthorization: will know to handle any
    // CLAuthorizationStatus changes.
    //
    // TODO(crbug.com/661996): Remove the now useless
    // kAuthorizationStateNotDeterminedSystemPrompt from
    // omnibox_geolocation_local_state.h.
    _systemPrompt = YES;
    self.localState.authorizationState =
        geolocation::kAuthorizationStateNotDeterminedSystemPrompt;

    // Turn on location updates, so that iOS will prompt the user.
    [self requestPermission];
    _newUser = newUser;
  }
}

- (void)locationBarDidBecomeFirstResponder:(ChromeBrowserState*)browserState {
  if (self.enabled && browserState && !browserState->IsOffTheRecord()) {
    [self requestPermission];
  }
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
      !self.locationServicesEnabled) {
    return;
  }

  switch (CLLocationManager.authorizationStatus) {
    case kCLAuthorizationStatusNotDetermined:
      // Prompt the user with the iOS system location authorization alert.
      //
      // Set |systemPrompt_|, so that
      // locationManagerDidChangeAuthorization: will know that any
      // CLAuthorizationStatus changes are coming from this specific prompt.
      _systemPrompt = YES;
      self.localState.authorizationState =
          geolocation::kAuthorizationStateNotDeterminedSystemPrompt;
      [self requestPermission];
      break;

    case kCLAuthorizationStatusRestricted:
    case kCLAuthorizationStatusDenied:
      break;

    case kCLAuthorizationStatusAuthorizedAlways:
    case kCLAuthorizationStatusAuthorizedWhenInUse:
      // We might be in state kAuthorizationStateNotDeterminedSystemPrompt here
      // if we presented the iOS system location alert when
      // CLLocationManager.authorizationStatus was
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

- (void)systemPromptSkippedForNewUser {
  _newUser = YES;
}

#pragma mark - Private

- (BOOL)enabled {
  return self.locationServicesEnabled &&
         self.localState.authorizationState ==
             geolocation::kAuthorizationStateAuthorized;
}

- (OmniboxGeolocationLocalState*)localState {
  if (!_localState) {
    _localState = [[OmniboxGeolocationLocalState alloc] init];
  }
  return _localState;
}

- (BOOL)URLIsAuthorizationPromptingURL:(const GURL&)url {
  // Per PRD: "Show a modal dialog upon reaching google.com or a search results
  // page..." However, we only want to do this for domains where we will send
  // location.
  return (google_util::IsGoogleHomePageUrl(url) ||
          google_util::IsGoogleSearchUrl(url)) &&
         [[OmniboxGeolocationConfig sharedInstance] URLHasEligibleDomain:url];
}

// Requests the authorization to use location.
- (void)requestPermission {
  [self.locationManager requestWhenInUseAuthorization];
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
  _authorizationAlert =
      [[OmniboxGeolocationAuthorizationAlert alloc] initWithDelegate:self];
  [_authorizationAlert showAuthorizationAlert];

  self.localState.lastAuthorizationAlertVersion =
      version_info::GetVersionNumber();
}

- (void)recordAuthorizationAction:(AuthorizationAction)authorizationAction {
  if (_newUser) {
    _newUser = NO;

    UMA_HISTOGRAM_ENUMERATION(kGeolocationAuthorizationActionNewUser,
                              authorizationAction, kAuthorizationActionCount);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kGeolocationAuthorizationActionExistingUser,
                              authorizationAction, kAuthorizationActionCount);
  }
}

// Boolean value indicating whether location services are enabled on the
// device.
- (BOOL)locationServicesEnabled {
  return !tests_hook::DisableGeolocation() &&
         [CLLocationManager locationServicesEnabled];
}

#pragma mark - CLLocationManagerDelegate

- (void)locationManagerDidChangeAuthorization:
    (CLLocationManager*)locationManager {
  if (_systemPrompt) {
    switch (CLLocationManager.authorizationStatus) {
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
        _systemPrompt = NO;

        [self recordAuthorizationAction:kAuthorizationActionPermanentlyDenied];
        break;

      case kCLAuthorizationStatusAuthorizedAlways:
      case kCLAuthorizationStatusAuthorizedWhenInUse:
        self.localState.authorizationState =
            geolocation::kAuthorizationStateAuthorized;
        _systemPrompt = NO;

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

  _authorizationAlert = nil;

  [self recordAuthorizationAction:kAuthorizationActionAuthorized];
}

- (void)authorizationAlertDidCancel:
    (OmniboxGeolocationAuthorizationAlert*)authorizationAlert {
  // Leave authorization state as undetermined (not kAuthorizationStateDenied).
  // We won't use location, but we'll still be able to prompt at the next
  // application update.

  _authorizationAlert = nil;

  [self recordAuthorizationAction:kAuthorizationActionDenied];
}

#pragma mark - OmniboxGeolocationController+Testing

- (void)setLocalState:(OmniboxGeolocationLocalState*)localState {
  _localState = localState;
}

@end
