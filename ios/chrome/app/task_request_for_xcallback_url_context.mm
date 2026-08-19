// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request_for_xcallback_url_context.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/app/task_request_url_context_private.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

namespace {

// Returns the command dispatch time from the shared defaults.
NSDate* ExtractCommandTimeFromSharedDefaults(UIOpenURLContext* URLContext) {
  NSString* action = [URLContext.URL path];
  if (![action isEqualToString:
                   [NSString stringWithFormat:
                                 @"/%s",
                                 app_group::kChromeAppGroupXCallbackCommand]]) {
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

@implementation TaskRequestForXCallbackURLContext {
  NSDate* _commandTime;
}

- (NSDate*)commandTime {
  if (!_commandTime) {
    _commandTime = ExtractCommandTimeFromSharedDefaults(self.URLContext);
  }
  return _commandTime;
}

- (void)recordStartupMetrics {
  [super recordStartupMetrics];

  base::UmaHistogramEnumeration(kAppLaunchSource, AppLaunchSource::X_CALLBACK);

  NSString* action = [self.URLContext.URL path];
  if ([action isEqualToString:
                  [NSString stringWithFormat:
                                @"/%s",
                                app_group::kChromeAppGroupXCallbackCommand]]) {
    UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartActionHistogram,
                              START_ACTION_XCALLBACK_APPGROUP_COMMAND,
                              MOBILE_SESSION_START_ACTION_COUNT);
    if (self.commandTime) {
      NSTimeInterval delay =
          [[NSDate date] timeIntervalSinceDate:self.commandTime];
      UMA_HISTOGRAM_COUNTS_100("Startup.ApplicationGroupCommandDelay", delay);
    }
  } else if ([action isEqualToString:@"/open"]) {
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
  // TODO(crbug.com/493816082): Add implementation.
}

@end
