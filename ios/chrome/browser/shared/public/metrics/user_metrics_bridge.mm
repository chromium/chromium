// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/metrics/user_metrics_bridge.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"

using base::RecordAction;
using base::UserMetricsAction;

@implementation UserMetricsUtils

+ (void)recordAction:(NSString*)userAction {
  RecordAction(UserMetricsAction(base::SysNSStringToUTF8(userAction).c_str()));
}

@end
