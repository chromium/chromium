// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_METRICS_USER_METRICS_BRIDGE_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_METRICS_USER_METRICS_BRIDGE_H_

#import <UIKit/UIKit.h>

// An Objective-C wrapper around C++ user metrics methods.
@interface UserMetricsUtils : NSObject

// Records that the user performed an action.
+ (void)recordAction:(NSString*)userAction;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_METRICS_USER_METRICS_BRIDGE_H_
