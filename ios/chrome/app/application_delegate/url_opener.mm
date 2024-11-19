// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/url_opener.h"

#import <Foundation/Foundation.h>

#import "base/metrics/histogram_macros.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/application_delegate/url_opener_params.h"
#import "ios/chrome/app/startup/chrome_app_startup_parameters.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/connection_information.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "url/gurl.h"

namespace {
// Key of the UMA Startup.MobileSessionStartFromApps histogram.
const char* const kUMAMobileSessionStartFromAppsHistogram =
    "Startup.MobileSessionStartFromApps";
// Key of the UMA Startup.ShowDefaultPromoFromApps histogram.
const char* const kUMAShowDefaultPromoFromAppsHistogram =
    "Startup.ShowDefaultPromoFromApps";
}  // namespace

@implementation URLOpener

#pragma mark - ApplicationDelegate - URL Opening methods

+ (BOOL)openURL:(URLOpenerParams*)options
        applicationActive:(BOOL)applicationActive
                tabOpener:(id<TabOpening>)tabOpener
    connectionInformation:(id<ConnectionInformation>)connectionInformation
       startupInformation:(id<StartupInformation>)startupInformation
              prefService:(PrefService*)prefService
                initStage:(ProfileInitStage)initStage {
  NSURL* URL = options.URL;
  NSString* sourceApplication = options.sourceApplication;

  ChromeAppStartupParameters* params =
      [ChromeAppStartupParameters startupParametersWithURL:URL
                                         sourceApplication:sourceApplication];

  if (IsIncognitoModeDisabled(prefService)) {
    params.applicationMode = ApplicationModeForTabOpening::NORMAL;
  } else if (IsIncognitoModeForced(prefService)) {
    params.applicationMode = ApplicationModeForTabOpening::INCOGNITO;
  }

  MobileSessionCallerApp callerApp = [params callerApp];

  UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartFromAppsHistogram, callerApp,
                            MOBILE_SESSION_CALLER_APP_COUNT);

  if (params.postOpeningAction == SHOW_DEFAULT_BROWSER_SETTINGS) {
    UMA_HISTOGRAM_ENUMERATION(kUMAShowDefaultPromoFromAppsHistogram, callerApp,
                              MOBILE_SESSION_CALLER_APP_COUNT);
  }

  if (initStage == ProfileInitStage::kFirstRun) {
    UMA_HISTOGRAM_ENUMERATION("FirstRun.LaunchSource", [params launchSource],
                              first_run::LAUNCH_SIZE);
  } else if (applicationActive) {
    // The app is already active so the applicationDidBecomeActive: method will
    // never be called. Open the requested URL immediately and return YES if
    // the parsed URL was valid.
    if (params) {
      // As applicationDidBecomeActive: will not be called again,
      // _startupParameters will not include the command from openURL.
      // Pass the startup parameters from here.

      // TODO(crbug.com/40938699): Investigate why
      // connectionInformation.startupParamters can be not nil and what to do in
      // that case.
      [connectionInformation setStartupParameters:params];
      ProceduralBlock tabOpenedCompletion = ^{
        [connectionInformation setStartupParameters:nil];
      };

      // TODO(crbug.com/41443029): Exacly the same copy of this code is present
      // in UserActivityBrowserAgent::RouteToCorrectTab()

      GURL gurl;
      GURL virtualURL;
      if ([params completeURL].SchemeIsFile()) {
        // External URL will be loaded by WebState, which expects `completeURL`.
        // Omnibox however suppose to display `externalURL`, which is used as
        // virtual URL.
        gurl = [params completeURL];
        virtualURL = [params externalURL];
      } else {
        gurl = [params externalURL];
      }
      UrlLoadParams urlLoadParams = UrlLoadParams::InNewTab(gurl, virtualURL);

      ApplicationModeForTabOpening targetMode = params.applicationMode;
      // If the call is coming from the app, it should be opened in the current
      // mode to avoid changing mode.
      if (callerApp == CALLER_APP_GOOGLE_CHROME)
        targetMode = ApplicationModeForTabOpening::CURRENT;

      if (params.applicationMode != ApplicationModeForTabOpening::INCOGNITO &&
          [tabOpener URLIsOpenedInRegularMode:urlLoadParams.web_params.url]) {
        // Record metric.
      }

      [tabOpener
          dismissModalsAndMaybeOpenSelectedTabInMode:targetMode
                                   withUrlLoadParams:urlLoadParams
                                      dismissOmnibox:[params
                                                         postOpeningAction] !=
                                                     FOCUS_OMNIBOX
                                          completion:tabOpenedCompletion];

      return YES;
    }
    return NO;
  } else {
    // Don't record the first user action if application is not active.
    [startupInformation resetFirstUserActionRecorder];
  }

  connectionInformation.startupParameters = params;
  return connectionInformation.startupParameters != nil;
}

+ (void)handleLaunchOptions:(URLOpenerParams*)options
                  tabOpener:(id<TabOpening>)tabOpener
      connectionInformation:(id<ConnectionInformation>)connectionInformation
         startupInformation:(id<StartupInformation>)startupInformation
                prefService:(PrefService*)prefService
                  initStage:(ProfileInitStage)initStage {
  if (options.URL) {
    // This method is always called when the SceneState transitions to
    // SceneActivationLevelForegroundActive, and before the handling of
    // startupInformation is done.
    // Pass `NO` as active to avoid double processing.
    [URLOpener openURL:options
            applicationActive:NO
                    tabOpener:tabOpener
        connectionInformation:connectionInformation
           startupInformation:startupInformation
                  prefService:prefService
                    initStage:initStage];
  }
}

@end
