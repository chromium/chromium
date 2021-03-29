// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/geolocation/omnibox_geolocation_controller.h"

#import <CoreLocation/CoreLocation.h>

#include "base/metrics/histogram_macros.h"
#include "components/google/core/common/google_util.h"
#import "ios/chrome/app/tests_hook.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/geolocation/omnibox_geolocation_config.h"
#include "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

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

@interface OmniboxGeolocationController () <CLLocationManagerDelegate> {
  CLLocationManager* _locationManager;

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

@property(nonatomic, strong) CLLocationManager* locationManager;

// Returns YES if and only if |url| specifies a page for which we will prompt
// the user to authorize the use of geolocation for Omnibox queries.
- (BOOL)URLIsAuthorizationPromptingURL:(const GURL&)url;

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

- (void)triggerSystemPrompt {
  if (self.locationServicesEnabled && CLLocationManager.authorizationStatus ==
                                          kCLAuthorizationStatusNotDetermined) {
    _systemPrompt = YES;

    // Turn on location updates, so that iOS will prompt the user.
    [self requestPermission];
    _newUser = YES;
  }
}

- (void)locationBarDidBecomeFirstResponder:(ChromeBrowserState*)browserState {
  if (self.locationServicesEnabled && browserState &&
      !browserState->IsOffTheRecord()) {
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
      [self requestPermission];
      break;

    case kCLAuthorizationStatusRestricted:
    case kCLAuthorizationStatusDenied:
    case kCLAuthorizationStatusAuthorizedAlways:
    case kCLAuthorizationStatusAuthorizedWhenInUse:
      break;
  }
}

- (void)systemPromptSkippedForNewUser {
  _newUser = YES;
}

#pragma mark - Private

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
        _systemPrompt = NO;

        [self recordAuthorizationAction:kAuthorizationActionPermanentlyDenied];
        break;

      case kCLAuthorizationStatusAuthorizedAlways:
      case kCLAuthorizationStatusAuthorizedWhenInUse:
        _systemPrompt = NO;

        [self recordAuthorizationAction:kAuthorizationActionAuthorized];
        break;
    }
  }
}

@end
