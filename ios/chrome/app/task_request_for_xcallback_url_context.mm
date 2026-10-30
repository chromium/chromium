// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request_for_xcallback_url_context.h"

#import <UIKit/UIKit.h>

#import <map>
#import <string>

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/app/startup/app_startup_utils.h"
#import "ios/chrome/app/task_request_url_context_private.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/x_callback_url.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

namespace {

// Maps the `poa` query parameter of the `/open` command to the post-opening
// action to trigger once the tab is opened.
TabOpeningPostOpeningAction XCallbackPoaToPostOpeningAction(
    const std::string& poa_param) {
  if (poa_param == kPostOpeningActionDefaultBrowserSettings) {
    return TabOpeningPostOpeningAction::SHOW_DEFAULT_BROWSER_SETTINGS;
  }
  return TabOpeningPostOpeningAction::NO_ACTION;
}

// Returns the command dispatch time from the shared defaults.
NSDate* ExtractCommandTimeFromSharedDefaults(UIOpenURLContext* URLContext) {
  if (!IsXCallbackAppGroupCommandPath([URLContext.URL path])) {
    return nil;
  }
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  NSString* commandDictionaryPreference =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandPreference);
  NSDictionary* commandDictionary = base::apple::ObjCCast<NSDictionary>(
      [sharedDefaults objectForKey:commandDictionaryPreference]);
  if (!commandDictionary) {
    return nil;
  }
  NSString* commandTimePreference =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandTimePreference);
  return base::apple::ObjCCast<NSDate>(
      [commandDictionary objectForKey:commandTimePreference]);
}

}  // namespace

@implementation TaskRequestForXCallbackURLContext

- (instancetype)initWithURLContext:(UIOpenURLContext*)URLContext
                        sceneState:(SceneState*)sceneState
                       isColdStart:(BOOL)isColdStart {
  if ((self = [super initWithURLContext:URLContext
                             sceneState:sceneState
                            isColdStart:isColdStart])) {
    _commandTime = ExtractCommandTimeFromSharedDefaults(URLContext);
  }
  return self;
}

- (void)recordMetrics {
  [super recordMetrics];

  base::UmaHistogramEnumeration(kAppLaunchSource, AppLaunchSource::X_CALLBACK);

  NSString* action = [self.URLContext.URL path];
  if (IsXCallbackAppGroupCommandPath(action)) {
    UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram,
                              START_ACTION_XCALLBACK_APPGROUP_COMMAND,
                              MOBILE_SESSION_START_ACTION_COUNT);
    if (self.commandTime) {
      NSTimeInterval delay =
          [[NSDate date] timeIntervalSinceDate:self.commandTime];
      UMA_HISTOGRAM_COUNTS_100("Startup.ApplicationGroupCommandDelay", delay);
    }
  } else if (IsXCallbackOpenCommandPath(action)) {
    UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram,
                              START_ACTION_XCALLBACK_OPEN,
                              MOBILE_SESSION_START_ACTION_COUNT);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram,
                              START_ACTION_XCALLBACK_OTHER,
                              MOBILE_SESSION_START_ACTION_COUNT);
  }
}

- (void)handleCommandWithSceneState:(SceneState*)sceneState {
  if (IsXCallbackOpenCommandPath([self.URLContext.URL path])) {
    [self handleOpenCommandWithSceneState:sceneState];
  }
  // TODO(crbug.com/493816082): Handle the `/app-group-command` command.
}

#pragma mark - Private

// Opens the http(s) URL passed in the `url` query parameter of the `/open`
// command. Requests with a missing or non-http(s) URL are discarded.
- (void)handleOpenCommandWithSceneState:(SceneState*)sceneState {
  std::map<std::string, std::string> parameters =
      ExtractQueryParametersFromXCallbackURL(
          net::GURLWithNSURL(self.URLContext.URL));
  GURL externalURL(parameters[kXCallbackOpenCommandURLParameter]);
  if (!externalURL.is_valid() || !externalURL.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  [self openTabWithSceneState:sceneState
                  externalURL:externalURL
                   virtualURL:GURL()
                   targetMode:ApplicationModeForTabOpening::UNDETERMINED
            postOpeningAction:XCallbackPoaToPostOpeningAction(
                                  parameters[kPostOpeningActionParameter])
        postOpeningCompletion:nil
             fromWidgetOrSiri:NO];
}

@end
