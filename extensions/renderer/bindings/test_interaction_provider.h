// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_TEST_INTERACTION_PROVIDER_H_
#define EXTENSIONS_RENDERER_BINDINGS_TEST_INTERACTION_PROVIDER_H_

#include "extensions/renderer/bindings/interaction_provider.h"

#include "v8/include/v8.h"

namespace extensions {
class TestInteractionProvider : public InteractionProvider {
 public:
  TestInteractionProvider();

  TestInteractionProvider(const TestInteractionProvider&) = delete;
  TestInteractionProvider& operator=(const TestInteractionProvider&) = delete;

  ~TestInteractionProvider() override;

  // InteractionProvider:
  std::unique_ptr<InteractionProvider::Token> GetCurrentToken(
      v8::Local<v8::Context> v8_context) const override;
  std::unique_ptr<InteractionProvider::Scope> CreateScopedInteraction(
      v8::Local<v8::Context> v8_context,
      std::unique_ptr<InteractionProvider::Token> token) const override;
  bool HasActiveInteraction(v8::Local<v8::Context> v8_context) const override;
};

// User activation mock for test: sets transient activation state on
// construction, resets on destruction.
class ScopedTestUserActivation {
 public:
  ScopedTestUserActivation();
  ~ScopedTestUserActivation();
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_TEST_INTERACTION_PROVIDER_H_
