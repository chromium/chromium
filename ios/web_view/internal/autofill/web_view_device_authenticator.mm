// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/web_view_device_authenticator.h"

#import <LocalAuthentication/LocalAuthentication.h>

#import "base/functional/bind.h"
#import "base/location.h"
#import "base/not_fatal_until.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/web_view/internal/autofill/cwv_autofill_client_ios_bridge.h"

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

WebViewDeviceAuthenticator::WebViewDeviceAuthenticator(
    id<CWVAutofillClientIOSBridge> bridge)
    : bridge_(bridge) {}

WebViewDeviceAuthenticator::~WebViewDeviceAuthenticator() = default;

bool WebViewDeviceAuthenticator::CanAuthenticateWithBiometrics() {
  return CanEvaluatePolicy(LAPolicyDeviceOwnerAuthenticationWithBiometrics);
}

bool WebViewDeviceAuthenticator::CanAuthenticateWithBiometricOrScreenLock() {
  return CanEvaluatePolicy(LAPolicyDeviceOwnerAuthentication);
}

void WebViewDeviceAuthenticator::AuthenticateWithMessage(
    const std::u16string& message,
    AuthenticateCallback callback) {
  if (callback_) {
    // Only one authentication may be pending. Fail the new request rather than
    // replace `callback_`, which may own `this`.
    NOTREACHED(base::NotFatalUntil::M160);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /*success=*/false));
    return;
  }
  callback_ = std::move(callback);

  // `MandatoryReauthManager` transfers ownership of `this` into `callback_`,
  // so `this` stays alive until `callback_` runs and the weak pointer is only
  // invalidated by `Cancel()`.
  base::OnceCallback<void(bool)> completion =
      base::BindOnce(&WebViewDeviceAuthenticator::OnAuthenticationCompleted,
                     weak_ptr_factory_.GetWeakPtr());

  id<CWVAutofillClientIOSBridge> bridge = bridge_;
  if (!bridge) {
    // Fail asynchronously to match the asynchronous contract of the bridge.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(completion), /*success=*/false));
    return;
  }

  [bridge authenticateUserWithReason:base::SysUTF16ToNSString(message)
                          completion:std::move(completion)];
}

void WebViewDeviceAuthenticator::Cancel() {
  // Drops any later response from the embedder.
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (callback_) {
    OnAuthenticationCompleted(/*success=*/false);
  }
}

void WebViewDeviceAuthenticator::OnAuthenticationCompleted(bool success) {
  // The bridge reports at most once per request, and `Cancel()` only calls this
  // while a request is pending.
  if (!callback_) {
    NOTREACHED(base::NotFatalUntil::M160);
    return;
  }
  // `callback_` may own `this`, so no members may be accessed after it runs.
  std::move(callback_).Run(success);
}

}  // namespace autofill
