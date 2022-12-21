// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_METRIC_KIT_SUBSCRIBER_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_METRIC_KIT_SUBSCRIBER_H_

#import <Foundation/Foundation.h>
#import <MetricKit/MetricKit.h>

// A subscriber that save MetricKit reports to the application document
// directory.
@interface MetricKitSubscriber : NSObject <MXMetricManagerSubscriber>
+ (instancetype)sharedInstance;

// Creates an extended launch task used by MetricKit to track startup time up
// until a client-defined stopping point. This must be called before the first
// scene becomes active.
+ (void)createExtendedLaunchTask;

// Ends the extended launch task created in `createExtendedLaunchTask`.
+ (void)endExtendedLaunchTask;

// Whether the MetricKit collection is enabled.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;
@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_METRIC_KIT_SUBSCRIBER_H_
