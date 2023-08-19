// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_STATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_STATE_H_

#import <UIKit/UIKit.h>

enum class UpdateChromeSafetyCheckState;
enum class PasswordSafetyCheckState;
enum class SafeBrowsingSafetyCheckState;
enum class RunningSafetyCheckState;

// Helper class to contain the current Safety Check state.
@interface SafetyCheckState : NSObject

// Initializes a `SafetyCheckState` with `updateChromeState`, `passwordState`,
// `safeBrowsingState`, and `runningState`.
- (instancetype)
    initWithUpdateChromeState:(UpdateChromeSafetyCheckState)updateChromeState
                passwordState:(PasswordSafetyCheckState)passwordState
            safeBrowsingState:(SafeBrowsingSafetyCheckState)safeBrowsingState
                 runningState:(RunningSafetyCheckState)runningState;

// The current state of the Update Chrome check.
@property(nonatomic, readonly) UpdateChromeSafetyCheckState updateChromeState;

// The current state of the Password check.
@property(nonatomic, readonly) PasswordSafetyCheckState passwordState;

// The current state of the Safe Browsing check.
@property(nonatomic, readonly) SafeBrowsingSafetyCheckState safeBrowsingState;

// The Safety Check running state.
@property(nonatomic, readonly) RunningSafetyCheckState runningState;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_STATE_H_
