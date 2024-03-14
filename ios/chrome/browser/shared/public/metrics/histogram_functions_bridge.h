// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_METRICS_HISTOGRAM_FUNCTIONS_BRIDGE_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_METRICS_HISTOGRAM_FUNCTIONS_BRIDGE_H_

#import <UIKit/UIKit.h>

// An Objective-C wrapper around C++ user metrics methods.
@interface HistogramUtils : NSObject

/// Records that the user performed an action.
/// `histogram`: The name of the histogram.
/// `sample`: The enum value that we want to record.
/// `maxValue:` The last value of the recorded enum.
+ (void)recordHistogram:(NSString*)histogram
             withSample:(NSInteger)sample
               maxValue:(NSInteger)maxValue;

// Records the memory related histogram.
+ (void)recordHistogram:(NSString*)histogram withMemoryKB:(NSInteger)sample;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_METRICS_HISTOGRAM_FUNCTIONS_BRIDGE_H_
