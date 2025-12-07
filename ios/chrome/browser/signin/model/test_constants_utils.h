// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_TEST_CONSTANTS_UTILS_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_TEST_CONSTANTS_UTILS_H_

#import <UIKit/UIKit.h>

namespace signin {

// Returns the list of buttons that must be tested by eg test to dismiss the
// fake authentication view while remaining signed-out.
NSArray<NSString*>* FakeSystemIdentityManagerStaySignedOutButtons();

}  // namespace signin

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_TEST_CONSTANTS_UTILS_H_
