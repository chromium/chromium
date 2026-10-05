// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_reauth/model/ios_device_auth_availability_checker.h"

#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"

IOSDeviceAuthAvailabilityChecker::IOSDeviceAuthAvailabilityChecker(
    id<ReauthenticationProtocol> reauth_module)
    : reauth_module_(reauth_module) {}

IOSDeviceAuthAvailabilityChecker::~IOSDeviceAuthAvailabilityChecker() = default;

bool IOSDeviceAuthAvailabilityChecker::CanAuthenticateWithBiometrics() {
  return [reauth_module_ canAttemptReauthWithBiometrics];
}

bool IOSDeviceAuthAvailabilityChecker::
    CanAuthenticateWithBiometricOrScreenLock() {
  return [reauth_module_ canAttemptReauth];
}
