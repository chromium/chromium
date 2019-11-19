// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/test_interaction_provider.h"

namespace extensions {

namespace {
bool g_mock_user_activation_v2_state_ = false;
}

TestInteractionProvider::TestInteractionProvider() = default;

TestInteractionProvider::~TestInteractionProvider() = default;

std::unique_ptr<InteractionProvider::Token>
TestInteractionProvider::GetCurrentToken(
    v8::Local<v8::Context> v8_context) const {
  // Note: Not necessary for tests.
  return nullptr;
}

std::unique_ptr<InteractionProvider::Scope>
TestInteractionProvider::CreateScopedInteraction(
    v8::Local<v8::Context> v8_context,
    std::unique_ptr<InteractionProvider::Token> token) const {
  // Note: Not necessary for tests.
  return nullptr;
}

bool TestInteractionProvider::HasActiveInteraction(
    v8::Local<v8::Context> v8_context) const {
  return g_mock_user_activation_v2_state_;
}

ScopedTestUserActivation::ScopedTestUserActivation() {
  DCHECK(!g_mock_user_activation_v2_state_);  // Nested scopes are not allowed.
  g_mock_user_activation_v2_state_ = true;
}

ScopedTestUserActivation::~ScopedTestUserActivation() {
  g_mock_user_activation_v2_state_ = false;
}

}  // namespace extensions
