// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request_for_xcallback_url_context.h"

#import <UIKit/UIKit.h>

#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "ios/chrome/app/startup/app_launch_metrics.h"
#import "ios/chrome/app/task_request_url_context_private.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

@implementation TaskRequestForXCallbackURLContext

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

@end
