// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_SWITCHER_TEST_TEST_APP_SWITCHER_HELPER_H_
#define IOS_CHROME_BROWSER_APP_SWITCHER_TEST_TEST_APP_SWITCHER_HELPER_H_

#import <Foundation/Foundation.h>

#import <string_view>

#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/test/providers/app_switcher/test_app_switcher.h"

// Test URL string constants.
inline constexpr std::string_view kIncognitoModeUrl =
    "http://www.incognito.com";
inline constexpr std::string_view kErrorUrl = "http://www.error.com";
inline constexpr std::string_view kTimeOutErrorUrl =
    "http://www.timeOutError.com";
inline constexpr std::string_view kAISummarizationUrl =
    "http://www.summarize.com";

// Helper conforming to AppSwitcherProviderTestHelper for unit testing.
@interface TestAppSwitcherProviderTestHelper
    : NSObject <AppSwitcherProviderTestHelper>

// Sets the application mode.
- (void)setMode:(ApplicationModeForTabOpening)mode;

// Returns the application mode.
- (ApplicationModeForTabOpening)mode;

@end

#endif  // IOS_CHROME_BROWSER_APP_SWITCHER_TEST_TEST_APP_SWITCHER_HELPER_H_
