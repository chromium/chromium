// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_reauth/model/ios_device_authenticator.h"

#import "base/functional/callback_helpers.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"

IOSDeviceAuthenticator::IOSDeviceAuthenticator(
    id<ReauthenticationProtocol> reauth_module,
    DeviceAuthenticatorProxy* proxy,
    const device_reauth::DeviceAuthParams& params)
    : DeviceAuthenticatorCommon(proxy,
                                params.GetAuthenticationValidityPeriod(),
                                params.GetAuthResultHistogram()),
      authentication_module_(reauth_module) {}

IOSDeviceAuthenticator::~IOSDeviceAuthenticator() = default;

bool IOSDeviceAuthenticator::CanAuthenticateWithBiometrics() {
  return [authentication_module_ canAttemptReauthWithBiometrics];
}

bool IOSDeviceAuthenticator::CanAuthenticateWithBiometricOrScreenLock() {
  return [authentication_module_ canAttemptReauth];
}

void IOSDeviceAuthenticator::CanAuthenticateWithBiometricOrScreenLock(
    base::OnceCallback<void(bool)> callback) {
  id<ReauthenticationProtocol> reauth_module = authentication_module_;
  // Strongly capturing `authentication_module_` in the reply block ensures its
  // lifetime is prolonged until after the task has run, and it is destroyed on
  // the original sequence.
  base::OnceClosure destroy_on_original_sequence =
      base::DoNothingWithBoundArgs(reauth_module);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(^bool {
        return [reauth_module canAttemptReauth];
      }),
      std::move(callback).Then(std::move(destroy_on_original_sequence)));
}

void IOSDeviceAuthenticator::AuthenticateWithMessage(
    const std::u16string& message,
    AuthenticateCallback callback) {
  callback_ = std::move(callback);
  bool can_reuse_previous_auth = !NeedsToAuthenticate();
  base::WeakPtr<IOSDeviceAuthenticator> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  auto completion_handler = ^(ReauthenticationResult result) {
    if (weak_this) {
      weak_this->OnAuthenticationCompleted(/*succeeded=*/result !=
                                           ReauthenticationResult::kFailure);
    }
  };

  [authentication_module_
      attemptReauthWithLocalizedReason:base::SysUTF16ToNSString(message)
                  canReusePreviousAuth:can_reuse_previous_auth
                               handler:completion_handler];
}

void IOSDeviceAuthenticator::Cancel() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (callback_) {
    OnAuthenticationCompleted(/*succeeded=*/false);
  }
}

void IOSDeviceAuthenticator::OnAuthenticationCompleted(bool succeeded) {
  CHECK(!callback_.is_null());
  RecordAuthenticationTimeIfSuccessful(succeeded);
  std::move(callback_).Run(succeeded);
}
