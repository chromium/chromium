// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/metrics/histogram_functions_bridge.h"

#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"

using base::UmaHistogramExactLinear;

@implementation HistogramUtils

+ (void)RecordHistogram:(NSString*)histogram
             withSample:(NSInteger)sample
               maxValue:(NSInteger)maxValue {
  UmaHistogramExactLinear(base::SysNSStringToUTF8(histogram), sample,
                          maxValue + 1);
}

@end
