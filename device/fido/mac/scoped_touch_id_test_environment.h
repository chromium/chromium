// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_SCOPED_TOUCH_ID_TEST_ENVIRONMENT_H_
#define DEVICE_FIDO_MAC_SCOPED_TOUCH_ID_TEST_ENVIRONMENT_H_

#include <memory>

#include "base/component_export.h"
#include "base/mac/availability.h"
#include "base/macros.h"

namespace device {
namespace fido {
namespace mac {

struct AuthenticatorConfig;
class FakeKeychain;
class FakeTouchIdContext;
class TouchIdContext;

// ScopedTouchIdTestEnvironment overrides behavior of the Touch ID
// authenticator in testing. While in scope, it
//  - installs a fake Keychain to avoid writing to the macOS keychain, which
//    requires a valid code signature and keychain-access-group entitlement;
//  - allows faking TouchIdContext instances returned by TouchIdContext to stub
//    out Touch ID fingerprint prompts.
//  Overrides are reset when the instance is destroyed.
class COMPONENT_EXPORT(DEVICE_FIDO)
    API_AVAILABLE(macosx(10.12.2)) ScopedTouchIdTestEnvironment {
 public:
  ScopedTouchIdTestEnvironment();
  ~ScopedTouchIdTestEnvironment();

  // ForgeNextTouchIdContext sets up the FakeTouchIdContext returned by the
  // next call to TouchIdContext::Create. The fake will invoke the callback
  // passed to TouchIdContext::PromptTouchId with the given result.
  //
  // It is a fatal error to call TouchIdContext::Create without invoking this
  // method first while the test environment is in scope.
  void ForgeNextTouchIdContext(bool simulate_prompt_success);

  // Sets the value returned by TouchIdContext::TouchIdAvailable. The default on
  // instantiation of the test environment is true.
  bool SetTouchIdAvailable(bool available);

 private:
  static std::unique_ptr<TouchIdContext> ForwardCreate();
  static bool ForwardTouchIdAvailable(const AuthenticatorConfig& config);

  std::unique_ptr<TouchIdContext> CreateTouchIdContext();
  bool TouchIdAvailable(const AuthenticatorConfig&);

  using CreateFuncPtr = decltype(&ForwardCreate);
  CreateFuncPtr touch_id_context_create_ptr_;
  using TouchIdAvailableFuncPtr = decltype(&ForwardTouchIdAvailable);
  TouchIdAvailableFuncPtr touch_id_context_touch_id_available_ptr_;

  std::unique_ptr<FakeTouchIdContext> next_touch_id_context_;
  std::unique_ptr<FakeKeychain> keychain_;
  bool touch_id_available_ = true;

  DISALLOW_COPY_AND_ASSIGN(ScopedTouchIdTestEnvironment);
};

}  // namespace mac
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_MAC_SCOPED_TOUCH_ID_TEST_ENVIRONMENT_H_
