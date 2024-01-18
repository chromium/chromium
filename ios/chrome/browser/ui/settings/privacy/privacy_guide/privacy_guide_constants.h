// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_CONSTANTS_H_

#import <Foundation/Foundation.h>

// The possible step types that can be displayed as part of the Privacy Guide.
enum PrivacyGuideStepType : NSInteger {
  kPrivacyGuideWelcomeStep,
  kPrivacyGuideURLUsageStep,
};

// The accessibility identifier of the Privacy Guide wide navigation bar.
extern NSString* const kPrivacyGuideNavigationBarViewID;

// The accessibility identifier for the Welcome step view.
extern NSString* const kPrivacyGuideWelcomeViewID;

// The accessibility identifier for the URL usage step view.
extern NSString* const kPrivacyGuideURLUsageViewID;

// The accessibility identifier for the URL usage switch.
extern NSString* const kPrivacyGuideURLUsageSwitchID;

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_GUIDE_PRIVACY_GUIDE_CONSTANTS_H_
