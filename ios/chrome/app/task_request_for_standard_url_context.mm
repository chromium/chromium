// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request_for_standard_url_context.h"

#import <UIKit/UIKit.h>

#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/app/task_request_url_context_private.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/intelligence/features/features.h"

namespace {

NSString* const kExternalActionURLHost = @"ChromeExternalAction";
NSString* const kExternalActionDefaultBrowserSettings =
    @"DefaultBrowserSettings";
NSString* const kExternalActionOpenNTP = @"OpenNTP";
NSString* const kExternalActionAppStoreGeminiPromo = @"appstoregeminipromo";
NSString* const kExternalActionAppSwitcherTesting = @"appswitchertesting";

// Records metrics and user actions for external action URLs.
void RecordExternalActionMetrics(NSURL* url) {
  base::RecordAction(base::UserMetricsAction("MobileExternalActionURLOpened"));
  UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram,
                            START_EXTERNAL_ACTION,
                            MOBILE_SESSION_START_ACTION_COUNT);
  base::UmaHistogramEnumeration(kAppLaunchSource,
                                AppLaunchSource::EXTERNAL_ACTION);

  NSArray<NSString*>* pathComponents = url.pathComponents;
  NSString* path = nil;
  if ([pathComponents count] == 2 && [pathComponents[0] isEqualToString:@"/"]) {
    path = pathComponents[1];
  }
  IOSExternalAction action = IOSExternalAction::ACTION_INVALID;
  if ([path isEqualToString:kExternalActionOpenNTP]) {
    base::RecordAction(
        base::UserMetricsAction("MobileExternalActionURLOpenedWithOpenNTP"));
    action = IOSExternalAction::ACTION_OPEN_NTP;
  } else if ([path isEqualToString:kExternalActionDefaultBrowserSettings]) {
    base::RecordAction(base::UserMetricsAction(
        "MobileExternalActionURLOpenedWithDefaultBrowserSettings"));
    if (IsChromeLikelyDefaultBrowser()) {
      action =
          IOSExternalAction::ACTION_SKIPPED_DEFAULT_BROWSER_SETTINGS_FOR_NTP;
    } else {
      action = IOSExternalAction::ACTION_DEFAULT_BROWSER_SETTINGS;
    }
  } else if ([path isEqualToString:kExternalActionAppStoreGeminiPromo]) {
    base::RecordAction(base::UserMetricsAction(
        "MobileExternalActionURLOpenedWithAppStoreGeminiPromo"));
    action = IOSExternalAction::ACTION_APP_STORE_GEMINI_PROMO;
  } else if (IsAppSwitcherAISummarizationEnabled() &&
             [path isEqualToString:kExternalActionAppSwitcherTesting]) {
    action = IOSExternalAction::ACTION_START_GEMINI_AI_SUMMARIZATION;
  }
  base::UmaHistogramEnumeration(kExternalActionHistogram, action);
}

}  // namespace

@implementation TaskRequestForStandardURLContext

- (void)recordStartupMetrics {
  [super recordStartupMetrics];

  NSURL* url = self.URLContext.URL;
  if (!url) {
    return;
  }

  NSString* host = url.host;
  NSString* scheme = url.scheme.lowercaseString;

  if ([host isEqualToString:kExternalActionURLHost]) {
    RecordExternalActionMetrics(url);
  } else if ([scheme isEqualToString:@"file"]) {
    UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram,
                              START_ACTION_OPEN_FILE,
                              MOBILE_SESSION_START_ACTION_COUNT);
  } else {
    MobileSessionStartAction action = START_ACTION_OTHER;
    if ([scheme isEqualToString:@"http"]) {
      action = START_ACTION_OPEN_HTTP_FROM_OS;
      base::RecordAction(
          base::UserMetricsAction("MobileDefaultBrowserViewIntent"));
    } else if ([scheme isEqualToString:@"https"]) {
      action = START_ACTION_OPEN_HTTPS_FROM_OS;
      base::RecordAction(
          base::UserMetricsAction("MobileDefaultBrowserViewIntent"));
    } else {
      BOOL useHttps = [scheme hasSuffix:@"s"];
      action = useHttps ? START_ACTION_OPEN_HTTPS : START_ACTION_OPEN_HTTP;
      base::UmaHistogramEnumeration(kAppLaunchSource,
                                    AppLaunchSource::LINK_OPENED_FROM_APP);
      base::RecordAction(base::UserMetricsAction("MobileFirstPartyViewIntent"));
    }
    UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram, action,
                              MOBILE_SESSION_START_ACTION_COUNT);

    if (action == START_ACTION_OPEN_HTTP_FROM_OS ||
        action == START_ACTION_OPEN_HTTPS_FROM_OS) {
      base::UmaHistogramEnumeration(kAppLaunchSource,
                                    AppLaunchSource::LINK_OPENED_FROM_OS);
      LogOpenHTTPURLFromExternalURL();
    }
  }
}

- (void)handleCommandWithSceneState:(SceneState*)sceneState {
  // TODO(crbug.com/493816082): Add implementation.
}

@end
