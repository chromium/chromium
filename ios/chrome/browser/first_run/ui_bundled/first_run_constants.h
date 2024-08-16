// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_FIRST_RUN_CONSTANTS_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_FIRST_RUN_CONSTANTS_H_

#import <Foundation/Foundation.h>

namespace first_run {

// The accessibility identifier for the Sign in screen shown in first run.
extern NSString* const kFirstRunSignInScreenAccessibilityIdentifier;

// The accessibility identifier for the Default browser screen shown in first
// run.
extern NSString* const kFirstRunDefaultBrowserScreenAccessibilityIdentifier;

// The accessibility identifier for the Omnibox position choice screen shown in
// first run.
extern NSString* const
    kFirstRunOmniboxPositionChoiceScreenAccessibilityIdentifier;

// URL for the terms of service text.
extern NSString* const kTermsOfServiceURL;

// URL for the metric reporting text.
extern NSString* const kMetricReportingURL;

// Accessibility identifier of the enterprise loading screen.
extern NSString* const kLaunchScreenAccessibilityIdentifier;

// Accessibility identifier of the enterprise loading screen.
extern NSString* const kEnterpriseLoadingScreenAccessibilityIdentifier;

}  // first_run

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_FIRST_RUN_CONSTANTS_H_
