// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/scoped_touch_id_test_environment.h"

#import <Security/Security.h>

#include <ostream>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "crypto/scoped_fake_apple_keychain_v2.h"
#include "device/fido/mac/authenticator_config.h"
#include "device/fido/mac/fake_touch_id_context.h"

namespace device::fido::mac {

static ScopedTouchIdTestEnvironment* g_current_environment = nullptr;

ScopedTouchIdTestEnvironment::ScopedTouchIdTestEnvironment(
    AuthenticatorConfig config)
    : config_(std::move(config)),
      keychain_(std::make_unique<crypto::ScopedFakeAppleKeychainV2>(
          config_.keychain_access_group)) {
  DCHECK(!g_current_environment);
  g_current_environment = this;

  // Override TouchIdContext::Create and TouchIdContext::IsAvailable.
  touch_id_context_create_ptr_ = TouchIdContext::g_create_;
  TouchIdContext::g_create_ = &ForwardCreate;

  touch_id_context_touch_id_available_ptr_ =
      TouchIdContext::g_touch_id_available_;
  TouchIdContext::g_touch_id_available_ = &ForwardTouchIdAvailable;
}

ScopedTouchIdTestEnvironment::~ScopedTouchIdTestEnvironment() {
  DCHECK(!next_touch_id_context_) << "unclaimed SimulatePromptSuccess() call";

  DCHECK(touch_id_context_create_ptr_);
  TouchIdContext::g_create_ = touch_id_context_create_ptr_;

  DCHECK(touch_id_context_touch_id_available_ptr_);
  TouchIdContext::g_touch_id_available_ =
      touch_id_context_touch_id_available_ptr_;
  g_current_environment = nullptr;
}

// static
std::unique_ptr<TouchIdContext> ScopedTouchIdTestEnvironment::ForwardCreate() {
  return g_current_environment->CreateTouchIdContext();
}

// static
bool ScopedTouchIdTestEnvironment::ForwardTouchIdAvailable(
    AuthenticatorConfig config) {
  return g_current_environment->TouchIdAvailable(std::move(config));
}

bool ScopedTouchIdTestEnvironment::SetTouchIdAvailable(bool available) {
  return touch_id_available_ = available;
}

bool ScopedTouchIdTestEnvironment::TouchIdAvailable(
    AuthenticatorConfig config) {
  return touch_id_available_;
}

void ScopedTouchIdTestEnvironment::SimulateTouchIdPromptSuccess() {
  CHECK(!next_touch_id_context_);
  next_touch_id_context_.reset(new FakeTouchIdContext);
  next_touch_id_context_->set_callback_result(true);
}

void ScopedTouchIdTestEnvironment::SimulateTouchIdPromptFailure() {
  CHECK(!next_touch_id_context_);
  next_touch_id_context_.reset(new FakeTouchIdContext);
  next_touch_id_context_->set_callback_result(false);
}

void ScopedTouchIdTestEnvironment::DoNotResolveNextPrompt() {
  next_touch_id_context_.reset(new FakeTouchIdContext);
  next_touch_id_context_->DoNotResolveNextPrompt();
}

std::unique_ptr<TouchIdContext>
ScopedTouchIdTestEnvironment::CreateTouchIdContext() {
  CHECK(next_touch_id_context_)
      << "Call SimulateTouchIdPromptSuccess/Failure() for every "
         "context created in the test environment.";
  return std::move(next_touch_id_context_);
}

}  // namespace device::fido::mac
