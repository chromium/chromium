// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/metrics/histogram_functions_bridge.h"

#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"

using base::UmaHistogramExactLinear;
using base::UmaHistogramMemoryKB;

@implementation HistogramUtils

+ (void)recordHistogram:(NSString*)histogram
             withSample:(NSInteger)sample
               maxValue:(NSInteger)maxValue {
  UmaHistogramExactLinear(base::SysNSStringToUTF8(histogram), sample,
                          maxValue + 1);
}

+ (void)recordHistogram:(NSString*)histogram withMemoryKB:(NSInteger)sample {
  UmaHistogramMemoryKB(base::SysNSStringToUTF8(histogram), sample);
}

@end
