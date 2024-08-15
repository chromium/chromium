// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_HOME_CUSTOMIZATION_METRICS_RECORDER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_HOME_CUSTOMIZATION_METRICS_RECORDER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"

// Metrics recorder for the Home Customization menu.
@interface HomeCustomizationMetricsRecorder : NSObject

// Records the switch of a customization cell being toggled.
+ (void)recordCellToggled:(CustomizationToggleType)toggleType;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_HOME_CUSTOMIZATION_METRICS_RECORDER_H_
