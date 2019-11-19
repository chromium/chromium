// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/url_opener.h"

#import <Foundation/Foundation.h>

#include "base/metrics/histogram_macros.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#include "ios/chrome/app/startup/chrome_app_startup_parameters.h"
#import "ios/chrome/browser/chrome_url_util.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Key of the UMA Startup.MobileSessionStartFromApps histogram.
const char* const kUMAMobileSessionStartFromAppsHistogram =
    "Startup.MobileSessionStartFromApps";
}  // namespace

@implementation URLOpener

#pragma mark - ApplicationDelegate - URL Opening methods

+ (BOOL)openURL:(NSURL*)url
     applicationActive:(BOOL)applicationActive
               options:(NSDictionary<NSString*, id>*)options
             tabOpener:(id<TabOpening>)tabOpener
    startupInformation:(id<StartupInformation>)startupInformation {
  NSString* sourceApplication =
      options[UIApplicationOpenURLOptionsSourceApplicationKey];
  ChromeAppStartupParameters* params = [ChromeAppStartupParameters
      newChromeAppStartupParametersWithURL:url
                     fromSourceApplication:sourceApplication];

  MobileSessionCallerApp callerApp = [params callerApp];

  UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartFromAppsHistogram, callerApp,
                            MOBILE_SESSION_CALLER_APP_COUNT);

  if (startupInformation.isPresentingFirstRunUI) {
    UMA_HISTOGRAM_ENUMERATION("FirstRun.LaunchSource", [params launchSource],
                              first_run::LAUNCH_SIZE);
  }

  if (applicationActive) {
    // The app is already active so the applicationDidBecomeActive: method will
    // never be called. Open the requested URL immediately and return YES if
    // the parsed URL was valid.
    if (params) {
      // As applicationDidBecomeActive: will not be called again,
      // _startupParameters will not include the command from openURL.
      // Pass the startup parameters from here.
      DCHECK(!startupInformation.startupParameters);
      [startupInformation setStartupParameters:params];
      ProceduralBlock tabOpenedCompletion = ^{
        [startupInformation setStartupParameters:nil];
      };

      // TODO(crbug.com/935019): Exacly the same copy of this code is present in
      // +[UserAcrtivityHandler
      // handleStartupParametersWithTabOpener:startupInformation:interfaceProvider:]

      GURL URL;
      GURL virtualURL;
      if ([params completeURL].SchemeIsFile()) {
        // External URL will be loaded by WebState, which expects |completeURL|.
        // Omnibox however suppose to display |externalURL|, which is used as
        // virtual URL.
        URL = [params completeURL];
        virtualURL = [params externalURL];
      } else {
        URL = [params externalURL];
      }
      UrlLoadParams urlLoadParams = UrlLoadParams::InNewTab(URL, virtualURL);

      ApplicationModeForTabOpening targetMode =
          [params launchInIncognito] ? ApplicationModeForTabOpening::INCOGNITO
                                     : ApplicationModeForTabOpening::NORMAL;
      // If the call is coming from the app, it should be opened in the current
      // mode to avoid changing mode.
      if (callerApp == CALLER_APP_GOOGLE_CHROME)
        targetMode = ApplicationModeForTabOpening::CURRENT;

      if (![params launchInIncognito] &&
          [tabOpener URLIsOpenedInRegularMode:urlLoadParams.web_params.url]) {
        // Record metric.
      }

      [tabOpener
          dismissModalsAndOpenSelectedTabInMode:targetMode
                              withUrlLoadParams:urlLoadParams
                                 dismissOmnibox:[params postOpeningAction] !=
                                                FOCUS_OMNIBOX
                                     completion:tabOpenedCompletion];
      return YES;
    }
    return NO;
  }

  // Don't record the first user action.
  [startupInformation resetFirstUserActionRecorder];

  startupInformation.startupParameters = params;
  return startupInformation.startupParameters != nil;
}

+ (void)handleLaunchOptions:(NSDictionary*)launchOptions
          applicationActive:(BOOL)applicationActive
                  tabOpener:(id<TabOpening>)tabOpener
         startupInformation:(id<StartupInformation>)startupInformation
                   appState:(AppState*)appState {
  NSURL* url = launchOptions[UIApplicationLaunchOptionsURLKey];

  if (url) {
    NSMutableDictionary<NSString*, id>* options =
        [[NSMutableDictionary alloc] init];
    NSString* sourceApplication =
        launchOptions[UIApplicationLaunchOptionsSourceApplicationKey];
    if (sourceApplication) {
      options[UIApplicationOpenURLOptionsSourceApplicationKey] =
          sourceApplication;
    }

    BOOL openURLResult = [URLOpener openURL:url
                          applicationActive:applicationActive
                                    options:options
                                  tabOpener:tabOpener
                         startupInformation:startupInformation];
    [appState launchFromURLHandled:openURLResult];
  }
}

@end
