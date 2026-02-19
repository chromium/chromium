// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SAFETY_CHECK_UI_SAFETY_CHECK_CONFIG_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SAFETY_CHECK_UI_SAFETY_CHECK_CONFIG_H_

#import <optional>

#import "base/time/time.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/icon_detail_view.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/icon_detail_view_config.h"

enum class PasswordSafetyCheckState;
enum class RunningSafetyCheckState;
enum class SafeBrowsingSafetyCheckState;
@protocol SafetyCheckAudience;
enum class SafetyCheckItemType;
enum class UpdateChromeSafetyCheckState;

// Helper class to contain the current Safety Check configuration.
@interface SafetyCheckConfig : IconDetailViewConfig <IconDetailViewTapDelegate>

// The updates to properties must be reflected in the copy method.
// LINT.IfChange(Copy)
// The current state of the Update Chrome check.
@property(nonatomic, readwrite) UpdateChromeSafetyCheckState updateChromeState;

// The current state of the Password check.
@property(nonatomic, readwrite) PasswordSafetyCheckState passwordState;

// The current state of the Safe Browsing check.
@property(nonatomic, readwrite) SafeBrowsingSafetyCheckState safeBrowsingState;

// The Safety Check running state.
@property(nonatomic, readwrite) RunningSafetyCheckState runningState;

// The number of weak passwords found by the Password check.
@property(nonatomic, assign) NSInteger weakPasswordsCount;

// The number of reused passwords found by the Password check.
@property(nonatomic, assign) NSInteger reusedPasswordsCount;

// The number of compromised passwords found by the Password check.
@property(nonatomic, assign) NSInteger compromisedPasswordsCount;

// The last run time of the Safety Check.
@property(nonatomic, assign) std::optional<base::Time> lastRunTime;

// The object that should handle user events.
@property(nonatomic, weak) id<SafetyCheckAudience> audience;

// The item type that this safety check state should be configured for.
@property(nonatomic, assign) SafetyCheckItemType itemType;
// LINT.ThenChange(safety_check_config.mm:Copy)

// Initializes a `SafetyCheckConfig` with `updateChromeState`, `passwordState`,
// `safeBrowsingState`, and `runningState`.
- (instancetype)
    initWithUpdateChromeState:(UpdateChromeSafetyCheckState)updateChromeState
                passwordState:(PasswordSafetyCheckState)passwordState
            safeBrowsingState:(SafeBrowsingSafetyCheckState)safeBrowsingState
                 runningState:(RunningSafetyCheckState)runningState;

// Returns the number of check issues found.
- (NSUInteger)numberOfIssues;

// Returns `true` if any of the safety check components are currently running.
- (BOOL)isRunning;

// Returns `true` if all of the safety check components are in the default
// state.
- (BOOL)isDefault;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SAFETY_CHECK_UI_SAFETY_CHECK_CONFIG_H_
