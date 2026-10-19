// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/web_view_device_auth_availability_checker.h"

#import <LocalAuthentication/LocalAuthentication.h>

namespace autofill {

namespace {

// Returns whether the device is currently able to evaluate `policy`. A fresh
// `LAContext` is used each time so that enrollment changes (e.g. the user
// removing their passcode) are picked up immediately.
bool CanEvaluatePolicy(LAPolicy policy) {
  LAContext* context = [[LAContext alloc] init];
  return [context canEvaluatePolicy:policy error:nil];
}

}  // namespace

WebViewDeviceAuthAvailabilityChecker::WebViewDeviceAuthAvailabilityChecker() =
    default;

WebViewDeviceAuthAvailabilityChecker::~WebViewDeviceAuthAvailabilityChecker() =
    default;

bool WebViewDeviceAuthAvailabilityChecker::CanAuthenticateWithBiometrics() {
  return CanEvaluatePolicy(LAPolicyDeviceOwnerAuthenticationWithBiometrics);
}

bool WebViewDeviceAuthAvailabilityChecker::
    CanAuthenticateWithBiometricOrScreenLock() {
  return CanEvaluatePolicy(LAPolicyDeviceOwnerAuthentication);
}

}  // namespace autofill
