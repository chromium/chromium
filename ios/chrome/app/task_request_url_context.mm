// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request_url_context.h"

#import <optional>

#import "base/apple/bundle_locations.h"
#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/url_opener.h"
#import "ios/chrome/app/application_delegate/url_opener_params.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/app/task_request_for_standard_url_context.h"
#import "ios/chrome/app/task_request_for_widget_url_context.h"
#import "ios/chrome/app/task_request_for_xcallback_url_context.h"
#import "ios/chrome/app/task_request_private.h"
#import "ios/chrome/app/task_request_url_context_private.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/app_group/widget_constants.h"
#import "ios/chrome/common/x_callback_url.h"
#import "net/base/apple/url_conversions.h"

namespace {

// Returns the MobileSessionCallerApp for the specified `source_app` and `url`.
MobileSessionCallerApp GetCallerApp(NSString* source_app, NSURL* url) {
  if (![source_app length]) {
    if ([url.scheme isEqualToString:@"http"] ||
        [url.scheme isEqualToString:@"https"]) {
      return CALLER_APP_THIRD_PARTY;
    }
    return CALLER_APP_NOT_AVAILABLE;
  }

  if ([source_app
          isEqualToString:[base::apple::FrameworkBundle() bundleIdentifier]]) {
    return CALLER_APP_GOOGLE_CHROME;
  }
  if ([source_app isEqualToString:@"com.google.GoogleMobile"]) {
    return CALLER_APP_GOOGLE_SEARCH;
  }
  if ([source_app isEqualToString:@"com.google.Gmail"]) {
    return CALLER_APP_GOOGLE_GMAIL;
  }
  if ([source_app isEqualToString:@"com.google.Plus"]) {
    return CALLER_APP_GOOGLE_PLUS;
  }
  if ([source_app isEqualToString:@"com.google.Drive"]) {
    return CALLER_APP_GOOGLE_DRIVE;
  }
  if ([source_app isEqualToString:@"com.google.b612"]) {
    return CALLER_APP_GOOGLE_EARTH;
  }
  if ([source_app isEqualToString:@"com.google.ios.youtube"]) {
    return CALLER_APP_GOOGLE_YOUTUBE;
  }
  if ([source_app isEqualToString:@"com.google.Maps"]) {
    return CALLER_APP_GOOGLE_MAPS;
  }
  if ([source_app hasPrefix:@"com.google."]) {
    return CALLER_APP_GOOGLE_OTHER;
  }
  if ([source_app isEqualToString:@"com.apple.mobilesafari"]) {
    return CALLER_APP_APPLE_MOBILESAFARI;
  }
  if ([source_app hasPrefix:@"com.apple."]) {
    return CALLER_APP_APPLE_OTHER;
  }

  return CALLER_APP_OTHER;
}

// Returns the launch source for first run metrics.
first_run::ExternalLaunch GetLaunchSource(MobileSessionCallerApp caller_app,
                                          NSURL* url) {
  if (caller_app != CALLER_APP_APPLE_MOBILESAFARI) {
    return first_run::LAUNCH_BY_OTHERS;
  }

  NSString* query = url.query;
  if (![query length]) {
    return first_run::LAUNCH_BY_MOBILESAFARI;
  }

  // Look for `safarisab` (Smart App Banner key) anywhere in the query string.
  NSRange found = [query rangeOfString:@"safarisab"];
  if (found.location == NSNotFound) {
    return first_run::LAUNCH_BY_MOBILESAFARI;
  }

  if (found.location + found.length < [query length]) {
    unichar char_after =
        [query characterAtIndex:(found.location + found.length)];
    if (char_after != '&' && char_after != '=') {
      return first_run::LAUNCH_BY_MOBILESAFARI;
    }
  }
  if (found.location > 0) {
    unichar char_before = [query characterAtIndex:(found.location - 1)];
    if (char_before != '&') {
      return first_run::LAUNCH_BY_MOBILESAFARI;
    }
  }
  return first_run::LAUNCH_BY_SMARTAPPBANNER;
}

// Returns whether the URL specifies opening default browser settings.
bool IsShowDefaultBrowserSettings(NSURL* url) {
  NSURLComponents* components = [NSURLComponents componentsWithURL:url
                                           resolvingAgainstBaseURL:NO];
  for (NSURLQueryItem* item in components.queryItems) {
    if ([item.name isEqualToString:@"poa"] &&
        [item.value isEqualToString:@"default-browser-settings"]) {
      return true;
    }
  }
  return false;
}

// Records metrics for opening a URL context at runtime.
void RecordRuntimeMetrics(UIOpenURLContext* url_context, bool is_first_run) {
  NSURL* url = url_context.URL;
  NSString* source_application = url_context.options.sourceApplication;

  MobileSessionCallerApp caller_app = GetCallerApp(source_application, url);

  if (is_first_run) {
    base::UmaHistogramEnumeration("FirstRun.LaunchSource",
                                  GetLaunchSource(caller_app, url),
                                  first_run::LAUNCH_SIZE);
  }
}

}  // namespace

@interface TaskRequestForURLContext ()

@property(nonatomic, assign, readonly) GURL parsedURL;
@property(nonatomic, assign, readonly) MobileSessionCallerApp callerApp;

@end

@implementation TaskRequestForURLContext

+ (instancetype)taskRequestWithURLContext:(UIOpenURLContext*)URLContext
                               sceneState:(SceneState*)sceneState
                              isColdStart:(BOOL)isColdStart {
  NSURL* url = URLContext.URL;
  if ([url.scheme isEqualToString:kWidgetKitSchemeChrome]) {
    return
        [[TaskRequestForWidgetURLContext alloc] initWithURLContext:URLContext
                                                        sceneState:sceneState
                                                       isColdStart:isColdStart];
  }
  if (IsXCallbackURL(net::GURLWithNSURL(url))) {
    return [[TaskRequestForXCallbackURLContext alloc]
        initWithURLContext:URLContext
                sceneState:sceneState
               isColdStart:isColdStart];
  }
  return
      [[TaskRequestForStandardURLContext alloc] initWithURLContext:URLContext
                                                        sceneState:sceneState
                                                       isColdStart:isColdStart];
}

- (instancetype)initWithURLContext:(UIOpenURLContext*)URLContext
                        sceneState:(SceneState*)sceneState
                       isColdStart:(BOOL)isColdStart {
  if ((self = [super initWithSceneState:sceneState isColdStart:isColdStart])) {
    _URLContext = URLContext;
    _parsedURL = net::GURLWithNSURL(_URLContext.URL);
    _callerApp =
        GetCallerApp(_URLContext.options.sourceApplication, _URLContext.URL);
    [self extractGaiaID];
    [self recordStartupMetrics];
  }
  return self;
}

- (void)recordStartupMetrics {
  base::UmaHistogramEnumeration("Startup.MobileSessionStartFromApps",
                                _callerApp, MOBILE_SESSION_CALLER_APP_COUNT);

  if (IsShowDefaultBrowserSettings(_URLContext.URL)) {
    base::UmaHistogramEnumeration("Startup.ShowDefaultPromoFromApps",
                                  _callerApp, MOBILE_SESSION_CALLER_APP_COUNT);
  }
}

- (void)execute {
  SceneState* sceneState = [self sceneStateFromSessionID];
  CHECK(sceneState);

  const BOOL isFirstRun =
      sceneState.profileState.appState.startupInformation.isFirstRun;
  RecordRuntimeMetrics(_URLContext, isFirstRun);

  if (!self.isColdStart) {
    NSSet* URLContextSet = [NSSet setWithObject:_URLContext];
    // If the SystemIdentityManager handles the URL context, return early to
    // avoid opening the URL twice.
    if (GetApplicationContext()
            ->GetSystemIdentityManager()
            ->HandleSessionOpenURLContexts(sceneState.scene, URLContextSet)) {
      return;
    }
  }

  ProfileState* profileState = sceneState.profileState;
  URLOpenerParams* options =
      [[URLOpenerParams alloc] initWithUIOpenURLContext:_URLContext];
  [URLOpener openURL:options
          applicationActive:YES
                  tabOpener:sceneState.controller
      connectionInformation:sceneState.controller
         startupInformation:profileState.startupInformation
                prefService:profileState.profile->GetPrefs()
                  initStage:profileState.initStage];
}

#pragma mark - Private

- (void)extractGaiaID {
  NSURL* URL = _URLContext.URL;

  // Only widgets and share extension support profile/account switching when
  // handling an intent.
  bool isWidget = [URL.scheme isEqualToString:@"chromewidgetkit"];
  bool isShareExtension = [URL.path
      isEqualToString:
          [NSString
              stringWithFormat:@"/%s",
                               app_group::kChromeAppGroupXCallbackCommand]];

  if (!isWidget && !isShareExtension) {
    return;
  }

  // TODO(crbug.com/493826640): Investigate possible ways to check the gaia_id
  // when the task is executed (to be able to use GURL).
  NSURLComponents* URLComponents = [NSURLComponents componentsWithURL:URL
                                              resolvingAgainstBaseURL:NO];
  NSArray<NSURLQueryItem*>* queryItems = URLComponents.queryItems;
  NSString* gaiaIDKey =
      base::SysUTF8ToNSString(app_group::kGaiaIDQueryItemName);
  for (NSURLQueryItem* item in queryItems) {
    if ([item.name isEqualToString:gaiaIDKey]) {
      self.gaiaID = item.value;
      break;
    }
  }
}

@end

