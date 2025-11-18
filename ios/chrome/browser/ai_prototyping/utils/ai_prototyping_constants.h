// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_UTILS_AI_PROTOTYPING_CONSTANTS_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_UTILS_AI_PROTOTYPING_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Constants for UI dimensions of menu pages.
extern const CGFloat kBorderWidth;
extern const CGFloat kCornerRadius;
extern const CGFloat kHorizontalInset;
extern const CGFloat kMainStackTopInset;
extern const CGFloat kMainStackViewSpacing;
extern const CGFloat kResponseContainerHeightMultiplier;
extern const CGFloat kVerticalInset;
extern const CGFloat kButtonStackViewSpacing;
extern const CGFloat kDefaultTemperature;
extern const CGFloat kTemperatureSliderSteps;

// Enum representing the prototyping pages for different features.
enum class AIPrototypingFeature : NSInteger {
  // Represents the `kBlingPrototyping` feature used for flexible model
  // querying.
  kFreeform,
  // TODO(crbug.com/460813653): Remove deprecated TabOrganization constants.
  // Represents the tab organization/grouping feature.
  kTabOrganization,
  // Represents the enhanced calendar feature.
  kEnhancedCalendar,
  // Represents the new smart tab grouping feature.
  kSmartTabGrouping,
};

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_UTILS_AI_PROTOTYPING_CONSTANTS_H_
