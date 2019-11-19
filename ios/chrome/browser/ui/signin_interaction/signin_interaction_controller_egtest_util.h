// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SIGNIN_INTERACTION_SIGNIN_INTERACTION_CONTROLLER_EGTEST_UTIL_H_
#define IOS_CHROME_BROWSER_UI_SIGNIN_INTERACTION_SIGNIN_INTERACTION_CONTROLLER_EGTEST_UTIL_H_

#import <Foundation/Foundation.h>

@protocol GREYMatcher;

// Taps the button with accessibility label |label|.
void TapButtonWithAccessibilityLabel(NSString* label);

// Taps the button with accessibility labelId |message_id|.
void TapButtonWithLabelId(int message_id);

// Verifies that the Chrome sign-in view is visible.
void VerifyChromeSigninViewVisible();

// Wait until |matcher| is accessible (not nil).
void WaitForMatcher(id<GREYMatcher> matcher);

#endif  // IOS_CHROME_BROWSER_UI_SIGNIN_INTERACTION_SIGNIN_INTERACTION_CONTROLLER_EGTEST_UTIL_H_
