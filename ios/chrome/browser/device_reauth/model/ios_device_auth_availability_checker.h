// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEVICE_REAUTH_MODEL_IOS_DEVICE_AUTH_AVAILABILITY_CHECKER_H_
#define IOS_CHROME_BROWSER_DEVICE_REAUTH_MODEL_IOS_DEVICE_AUTH_AVAILABILITY_CHECKER_H_

#import "components/device_reauth/device_auth_availability_checker.h"

@protocol ReauthenticationProtocol;

// iOS implementation of `DeviceAuthAvailabilityChecker` that checks biometric
// and screen lock availability using `ReauthenticationProtocol`.
class IOSDeviceAuthAvailabilityChecker
    : public device_reauth::DeviceAuthAvailabilityChecker {
 public:
  explicit IOSDeviceAuthAvailabilityChecker(
      id<ReauthenticationProtocol> reauth_module);
  ~IOSDeviceAuthAvailabilityChecker() override;

  // Returns whether biometrics are available for a given device.
  bool CanAuthenticateWithBiometrics() override;

  // Returns whether biometrics or screen lock are available for a given device.
  bool CanAuthenticateWithBiometricOrScreenLock() override;

 private:
  id<ReauthenticationProtocol> reauth_module_;
};

#endif  // IOS_CHROME_BROWSER_DEVICE_REAUTH_MODEL_IOS_DEVICE_AUTH_AVAILABILITY_CHECKER_H_
