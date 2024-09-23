// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_SCOPED_TOUCH_ID_TEST_ENVIRONMENT_H_
#define DEVICE_FIDO_MAC_SCOPED_TOUCH_ID_TEST_ENVIRONMENT_H_

#include <os/availability.h>

#include <memory>

#include "base/component_export.h"
#include "device/fido/mac/authenticator_config.h"

namespace crypto {
class ScopedFakeAppleKeychainV2;
}  // namespace crypto

namespace device::fido::mac {

class FakeTouchIdContext;
class TouchIdContext;

// ScopedTouchIdTestEnvironment overrides behavior of the Touch ID
// authenticator in testing. While in scope, it
//  - installs a fake Keychain to avoid writing to the macOS keychain, which
//    requires a valid code signature and keychain-access-group entitlement;
//  - allows faking TouchIdContext instances returned by TouchIdContext to stub
//    out Touch ID fingerprint prompts.
//  Overrides are reset when the instance is destroyed.
class COMPONENT_EXPORT(DEVICE_FIDO) ScopedTouchIdTestEnvironment {
 public:
  explicit ScopedTouchIdTestEnvironment(AuthenticatorConfig config);

  ScopedTouchIdTestEnvironment(const ScopedTouchIdTestEnvironment&) = delete;
  ScopedTouchIdTestEnvironment& operator=(const ScopedTouchIdTestEnvironment&) =
      delete;

  ~ScopedTouchIdTestEnvironment();

  // Injects a fake to be returned by the
  // next call to `TouchIdContext::Create()`, which will automatically resolve
  // calls to `PromptTouchId()` successfully.
  void SimulateTouchIdPromptSuccess();

  // Like `SimulateTouchIdPromptSuccess()`, but `PromptTouchId()` resolves with
  // a failure.
  void SimulateTouchIdPromptFailure();

  // Sets the value returned by TouchIdContext::TouchIdAvailable. The default on
  // instantiation of the test environment is true.
  bool SetTouchIdAvailable(bool available);

  // Will prevent the next call to PromptTouchId from running the callback.
  void DoNotResolveNextPrompt();

  crypto::ScopedFakeAppleKeychainV2* keychain() { return keychain_.get(); }

 private:
  static std::unique_ptr<TouchIdContext> ForwardCreate();
  static bool ForwardTouchIdAvailable(AuthenticatorConfig);

  std::unique_ptr<TouchIdContext> CreateTouchIdContext();
  bool TouchIdAvailable(AuthenticatorConfig);

  using CreateFuncPtr = decltype(&ForwardCreate);
  CreateFuncPtr touch_id_context_create_ptr_;
  using TouchIdAvailableFuncPtr = decltype(&ForwardTouchIdAvailable);
  TouchIdAvailableFuncPtr touch_id_context_touch_id_available_ptr_;

  AuthenticatorConfig config_;
  std::unique_ptr<crypto::ScopedFakeAppleKeychainV2> keychain_;
  std::unique_ptr<FakeTouchIdContext> next_touch_id_context_;
  bool touch_id_available_ = true;
};

}  // namespace device::fido::mac

#endif  // DEVICE_FIDO_MAC_SCOPED_TOUCH_ID_TEST_ENVIRONMENT_H_
