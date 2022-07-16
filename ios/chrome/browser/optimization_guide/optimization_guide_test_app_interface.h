// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_TEST_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_TEST_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

#include "components/optimization_guide/proto/hints.pb.h"

class OptimizationGuideTestAppInterfaceWrapper {
 public:
  static void SetOptimizationGuideServiceUrl(NSString* url);
};

// The app interface for optimization guide EG2 tests.
@interface OptimizationGuideTestAppInterface : NSObject

// Sets the hints server URL used by the optimization guide infra.
+ (void)setGetHintsURL:(NSString*)url;

// Sets up component updates hints for testing.
+ (void)setComponentUpdateHints:(NSString*)url;

// Registers the optimization type for which hints should be fetched for.
+ (void)registerOptimizationType:
    (optimization_guide::proto::OptimizationType)type;

@end

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_TEST_APP_INTERFACE_H_
