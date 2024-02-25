// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEVICE_REAUTH_IOS_DEVICE_AUTHENTICATOR_H_
#define IOS_CHROME_BROWSER_DEVICE_REAUTH_IOS_DEVICE_AUTHENTICATOR_H_

#import "base/functional/callback.h"
#import "base/memory/weak_ptr.h"
#import "components/device_reauth/device_authenticator_common.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

// The DeviceAuthenticator implementation on iOS. This wraps up a
// `ReauthenticationModule`. Use this instead of the `ReauthenticationModule`
// when callsite specifically requires a `device_reauth::DeviceAuthenticator`.
class IOSDeviceAuthenticator : public DeviceAuthenticatorCommon {
 public:
  IOSDeviceAuthenticator(id<ReauthenticationProtocol> reauth_module,
                         DeviceAuthenticatorProxy* proxy,
                         const device_reauth::DeviceAuthParams& params);
  ~IOSDeviceAuthenticator() override;

  // DeviceAuthenticatorCommon:
  bool CanAuthenticateWithBiometrics() override;
  bool CanAuthenticateWithBiometricOrScreenLock() override;
  void AuthenticateWithMessage(const std::u16string& message,
                               AuthenticateCallback callback) override;
  void Cancel() override;

 private:
  // Called when the authentication completes with the result `succeeded`.
  void OnAuthenticationCompleted(bool succeeded);

  // Callback set by the caller of this class, to be executed after the
  // authentication completes.
  AuthenticateCallback callback_;

  // Module used to interact with iOS native authentication.
  id<ReauthenticationProtocol> authentication_module_;

  base::WeakPtrFactory<IOSDeviceAuthenticator> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_DEVICE_REAUTH_IOS_DEVICE_AUTHENTICATOR_H_
