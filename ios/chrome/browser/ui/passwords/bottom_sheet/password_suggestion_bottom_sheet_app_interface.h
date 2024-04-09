// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_APP_INTERFACE_H_

#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"

// PasswordSuggestionBottomSheetAppInterface contains the app-side
// implementation for helpers. These helpers are compiled into
// the app binary and can be called from either app or test code.
@interface PasswordSuggestionBottomSheetAppInterface : NSObject

// Sets a re-authentication mock (i.e. what asks user for fingerprint to
// view password) and its options for next test.
+ (void)setUpMockReauthenticationModule;
+ (void)mockReauthenticationModuleExpectedResult:
    (ReauthenticationResult)expectedResult;
+ (void)removeMockReauthenticationModule;

// Sets the pref recording the number of times the bottom sheet was dismissed.
// Used to either suppress or force the bottom sheet to appear in tests.
+ (void)setDismissCount:(int)dismissCount;

// Sets the pref recording the number of times the bottom sheet was dismissed to
// the maximum number allowed, so that the bottom sheet is effectively disabled.
+ (void)disableBottomSheet;

@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_APP_INTERFACE_H_
