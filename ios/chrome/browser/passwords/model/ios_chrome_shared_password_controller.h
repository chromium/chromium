// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_SHARED_PASSWORD_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_SHARED_PASSWORD_CONTROLLER_H_

#import "components/password_manager/ios/shared_password_controller.h"

// Wrapper around SharedPasswordController that provides the same functionality
// while enabling //ios/chrome-specific modifications.
@interface IOSChromeSharedPasswordController : SharedPasswordController
@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_SHARED_PASSWORD_CONTROLLER_H_
