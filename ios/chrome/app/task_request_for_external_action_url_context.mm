// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request_for_external_action_url_context.h"

#import <UIKit/UIKit.h>

#import <string>

#import "base/check_deref.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/app/startup/app_startup_utils.h"
#import "ios/chrome/app/task_request_url_context_private.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/coordinator/default_browser_promo/non_modal_default_browser_promo_scheduler_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/url_util.h"
#import "url/gurl.h"

NSString* const kExternalActionURLHost = @"ChromeExternalAction";

namespace {

NSString* const kExternalActionDefaultBrowserSettings =
    @"DefaultBrowserSettings";
NSString* const kExternalActionOpenNTP = @"OpenNTP";
NSString* const kExternalActionAppStoreGeminiPromo = @"appstoregeminipromo";
NSString* const kExternalActionAppSwitcherTesting = @"appswitchertesting";

// Returns the single path component of `url` if it has the format
// "/<component>", or nil if the path is invalid or has multiple segments.
NSString* ExtractSinglePathComponent(NSURL* url) {
  NSArray<NSString*>* path_components = url.pathComponents;
  if ([path_components count] != 2 ||
      ![path_components[0] isEqualToString:@"/"]) {
    return nil;
  }
  return path_components[1];
}

}  // namespace

@implementation TaskRequestForExternalActionURLContext

- (void)recordMetrics {
  [super recordMetrics];

  base::RecordAction(base::UserMetricsAction("MobileExternalActionURLOpened"));
  UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram,
                            START_EXTERNAL_ACTION,
                            MOBILE_SESSION_START_ACTION_COUNT);
  base::UmaHistogramEnumeration(kAppLaunchSource,
                                AppLaunchSource::EXTERNAL_ACTION);

  NSString* path = ExtractSinglePathComponent(self.URLContext.URL);
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
  } else if (IsAppSwitcherAISummarizationTestingExternalURLEnabled() &&
             [path isEqualToString:kExternalActionAppSwitcherTesting]) {
    action = IOSExternalAction::ACTION_START_GEMINI_AI_SUMMARIZATION;
  }
  base::UmaHistogramEnumeration(kExternalActionHistogram, action);
}

- (void)handleCommandWithSceneState:(SceneState*)sceneState {
  NSURL* url = self.URLContext.URL;
  NSString* path = ExtractSinglePathComponent(url);
  GURL externalGURL;
  TabOpeningPostOpeningAction postOpeningAction =
      TabOpeningPostOpeningAction::NO_ACTION;

  if ([path isEqualToString:kExternalActionOpenNTP]) {
    externalGURL = GURL(kChromeUINewTabURL);
  } else if ([path isEqualToString:kExternalActionDefaultBrowserSettings]) {
    // If Chrome is already set as default browser, just open the NTP.
    if (IsChromeLikelyDefaultBrowser()) {
      externalGURL = GURL(kChromeUINewTabURL);
    } else {
      postOpeningAction =
          TabOpeningPostOpeningAction::EXTERNAL_ACTION_SHOW_BROWSER_SETTINGS;
    }
  } else if ([path isEqualToString:kExternalActionAppStoreGeminiPromo]) {
    externalGURL = GURL(kGeminiAppStorePromoURL);
    postOpeningAction = TabOpeningPostOpeningAction::TRIGGER_GEMINI_PROMO;
    ProfileIOS* profile = sceneState.profileState.profile;
    CHECK_DEREF(profile).GetPrefs()->SetBoolean(
        prefs::kAppStoreGeminiPromoTriggered, true);
  } else if (IsAppSwitcherAISummarizationTestingExternalURLEnabled() &&
             [path isEqualToString:kExternalActionAppSwitcherTesting]) {
    // TODO(crbug.com/527016607): Remove this entire testing path when the
    // feature is enabled by default.
    std::string queryUrlString;
    net::GetValueForKeyInQuery(net::GURLWithNSURL(url), "url", &queryUrlString);
    GURL queryUrl(queryUrlString);

    externalGURL = queryUrl.SchemeIsHTTPOrHTTPS()
                       ? queryUrl
                       : GURL(kGeminiAppStorePromoURL);
    postOpeningAction =
        TabOpeningPostOpeningAction::START_GEMINI_AI_SUMMARIZATION;
  } else {
    // An unrecognized or invalid external action is discarded without opening
    // a tab.
    return;
  }

  // External actions are only opened via Chrome's own schemes, which count as
  // a first-party signal for the Default Browser promo only for first-party
  // caller apps.
  if (IsCallerAppFirstParty(self.callerApp)) {
    [[NonModalDefaultBrowserPromoSchedulerSceneAgent agentFromScene:sceneState]
        logUserEnteredAppViaFirstPartyScheme];
  }

  [self openTabWithSceneState:sceneState
                urlLoadParams:UrlLoadParams::InNewTab(externalGURL)
                   targetMode:ApplicationModeForTabOpening::UNDETERMINED
            postOpeningAction:postOpeningAction
        postOpeningCompletion:nil];
}

@end
