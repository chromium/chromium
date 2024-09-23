// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_util.h"

#import <LocalAuthentication/LocalAuthentication.h>
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

NSString* BiometricAuthenticationTypeString() {
  LAContext* ctx = [[LAContext alloc] init];
  // Call canEvaluatePolicy:error: once to populate biometrics type
  NSError* error;
  [ctx canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics
                   error:&error];
  if (error.code == LAErrorBiometryNotAvailable ||
      error.code == LAErrorBiometryNotEnrolled) {
    return l10n_util::GetNSString(IDS_IOS_INCOGNITO_REAUTH_PASSCODE);
  }

  switch (ctx.biometryType) {
    case LABiometryTypeFaceID:
      return @"Face ID";
    case LABiometryTypeTouchID:
      return @"Touch ID";
    default:
      return l10n_util::GetNSString(IDS_IOS_INCOGNITO_REAUTH_PASSCODE);
  }
}
