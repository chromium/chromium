// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_APP_INTERFACE_H_

#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"

// FormInputAccessoryAppInterface contains the app-side
// implementation for helpers. These helpers are compiled into
// the app binary and can be called from either app or test code.
@interface FormInputAccessoryAppInterface : NSObject

// Sets a re-authentication mock (i.e. what asks user for fingerprint to
// view password) and its options for next test.
+ (void)setUpMockReauthenticationModule;
+ (void)mockReauthenticationModuleExpectedResult:
    (ReauthenticationResult)expectedResult;
+ (void)removeMockReauthenticationModule;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_FORM_INPUT_ACCESSORY_FORM_INPUT_ACCESSORY_APP_INTERFACE_H_
