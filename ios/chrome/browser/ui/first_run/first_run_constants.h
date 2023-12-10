// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_CONSTANTS_H_

#import <Foundation/Foundation.h>

namespace first_run {

// The accessibility identifier for the Welcome screen shown in first run.
extern NSString* const kFirstRunWelcomeScreenAccessibilityIdentifier;

// The accessibility identifier for the Sign in screen shown in first run.
extern NSString* const kFirstRunSignInScreenAccessibilityIdentifier;

// The accessibility identifier for the Legacy Sign in screen shown in first
// run.
extern NSString* const kFirstRunLegacySignInScreenAccessibilityIdentifier;

// The accessibility identifier for the Sync screen shown in first run.
extern NSString* const kFirstRunSyncScreenAccessibilityIdentifier;

// The accessibility identifier for the Choice screen title;
extern NSString* const kSearchEngineChoiceTitleAccessibilityIdentifier;

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

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_CONSTANTS_H_
