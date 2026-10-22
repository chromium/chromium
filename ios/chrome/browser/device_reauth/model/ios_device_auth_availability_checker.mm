// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_reauth/model/ios_device_auth_availability_checker.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/task/thread_pool.h"
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

void IOSDeviceAuthAvailabilityChecker::CanAuthenticateWithBiometricOrScreenLock(
    base::OnceCallback<void(bool)> callback) {
  id<ReauthenticationProtocol> reauth_module = reauth_module_;
  // Strongly capturing `reauth_module` in the reply block ensures its lifetime
  // is prolonged until after the task has run, and it is destroyed on the
  // original sequence.
  base::OnceClosure destroy_on_original_sequence =
      base::DoNothingWithBoundArgs(reauth_module);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(^bool {
        return [reauth_module canAttemptReauth];
      }),
      std::move(callback).Then(std::move(destroy_on_original_sequence)));
}
