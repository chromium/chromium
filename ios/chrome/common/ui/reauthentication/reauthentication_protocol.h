// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_REAUTHENTICATION_REAUTHENTICATION_PROTOCOL_H_
#define IOS_CHROME_COMMON_UI_REAUTHENTICATION_REAUTHENTICATION_PROTOCOL_H_

#import <Foundation/Foundation.h>

// Indicates the result of the Reauthentication attempt.
enum class ReauthenticationResult {
  kSuccess = 0,
  kFailure = 1,
  kSkipped = 2,
  kMaxValue = kSkipped,
};

// Protocol for implementor of hardware reauthentication check.
@protocol ReauthenticationProtocol <NSObject>

// Checks whether biometric authentication is enabled for the device.
- (BOOL)canAttemptReauthWithBiometrics;

// Checks whether Touch ID and/or passcode is enabled for the device.
- (BOOL)canAttemptReauth;

// Attempts to reauthenticate the user with Touch ID or Face ID, or passcode if
// such hardware is not available. If `canReusePreviousAuth` is YES, a previous
// successful reauthentication can be taken into consideration, otherwise a new
// reauth attempt must be made. `handler` will take action depending on the
// result of the reauth attempt.
- (void)attemptReauthWithLocalizedReason:(NSString*)localizedReason
                    canReusePreviousAuth:(BOOL)canReusePreviousAuth
                                 handler:
                                     (void (^)(ReauthenticationResult success))
                                         handler;

@end

#endif  // IOS_CHROME_COMMON_UI_REAUTHENTICATION_REAUTHENTICATION_PROTOCOL_H_
