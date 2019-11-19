// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/scoped_touch_id_test_environment.h"

#import <Security/Security.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "device/fido/mac/authenticator_config.h"
#include "device/fido/mac/fake_keychain.h"
#include "device/fido/mac/fake_touch_id_context.h"

namespace device {
namespace fido {
namespace mac {

static API_AVAILABLE(macosx(10.12.2))
    ScopedTouchIdTestEnvironment* g_current_environment = nullptr;

ScopedTouchIdTestEnvironment::ScopedTouchIdTestEnvironment()
    : keychain_(std::make_unique<FakeKeychain>()) {
  DCHECK(!g_current_environment);
  g_current_environment = this;

  // Override TouchIdContext::Create and TouchIdContext::IsAvailable.
  touch_id_context_create_ptr_ = TouchIdContext::g_create_;
  TouchIdContext::g_create_ = &ForwardCreate;

  touch_id_context_touch_id_available_ptr_ =
      TouchIdContext::g_touch_id_available_;
  TouchIdContext::g_touch_id_available_ = &ForwardTouchIdAvailable;

  Keychain::SetInstanceOverride(static_cast<Keychain*>(keychain_.get()));
}

ScopedTouchIdTestEnvironment::~ScopedTouchIdTestEnvironment() {
  DCHECK(touch_id_context_create_ptr_);
  TouchIdContext::g_create_ = touch_id_context_create_ptr_;

  DCHECK(touch_id_context_touch_id_available_ptr_);
  TouchIdContext::g_touch_id_available_ =
      touch_id_context_touch_id_available_ptr_;

  Keychain::ClearInstanceOverride();
  g_current_environment = nullptr;
}

// static
std::unique_ptr<TouchIdContext> ScopedTouchIdTestEnvironment::ForwardCreate() {
  return g_current_environment->CreateTouchIdContext();
}

// static
bool ScopedTouchIdTestEnvironment::ForwardTouchIdAvailable(
    const AuthenticatorConfig& config) {
  return g_current_environment->TouchIdAvailable(config);
}

bool ScopedTouchIdTestEnvironment::SetTouchIdAvailable(bool available) {
  return touch_id_available_ = available;
}

bool ScopedTouchIdTestEnvironment::TouchIdAvailable(
    const AuthenticatorConfig& config) {
  return touch_id_available_;
}

void ScopedTouchIdTestEnvironment::ForgeNextTouchIdContext(
    bool simulate_prompt_success) {
  CHECK(!next_touch_id_context_);
  next_touch_id_context_ = base::WrapUnique(new FakeTouchIdContext);
  next_touch_id_context_->set_callback_result(simulate_prompt_success);
}

std::unique_ptr<TouchIdContext>
ScopedTouchIdTestEnvironment::CreateTouchIdContext() {
  CHECK(next_touch_id_context_) << "Call ForgeNextTouchIdContext() for every "
                                   "context created in the test environment.";
  return std::move(next_touch_id_context_);
}

}  // namespace mac
}  // namespace fido
}  // namespace device
