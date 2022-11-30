// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSION_INTERACTION_PROVIDER_H_
#define EXTENSIONS_RENDERER_EXTENSION_INTERACTION_PROVIDER_H_

#include "extensions/renderer/bindings/interaction_provider.h"

#include "v8/include/v8-forward.h"

namespace extensions {

// Provides user interaction related utilities specific to extensions system,
// works for both RenderFrame based and Service Worker based extensions.
class ExtensionInteractionProvider : public InteractionProvider {
 public:
  // Extension system specific implementation of token.
  // Can refer to a RenderFrame based extension token or Service Worker based
  // extension token.
  class Token : public InteractionProvider::Token {
   public:
    Token(const Token&) = delete;
    Token& operator=(const Token&) = delete;

    ~Token() override;

    bool is_for_service_worker() const { return is_for_service_worker_; }

   private:
    friend class ExtensionInteractionProvider;
    friend class TestInteractionProvider;

    Token(bool is_for_service_worker);

    bool is_for_service_worker_ = false;
  };

  // Extension system specific implementation of scope.
  class Scope : public InteractionProvider::Scope {
   public:
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

    ~Scope() override;

    // Creates a Scope for a Service Worker context, without token.
    static std::unique_ptr<Scope> ForWorker(v8::Local<v8::Context> v8_context);

    // Creates a scope from a |token|.
    static std::unique_ptr<Scope> ForToken(
        v8::Local<v8::Context> v8_context,
        std::unique_ptr<InteractionProvider::Token> token);

   private:
    Scope();

    // Increments and decrements Service Worker specific interaction counts.
    class ScopedWorkerInteraction {
     public:
      ScopedWorkerInteraction(v8::Local<v8::Context> v8_context,
                              bool created_from_token);
      ~ScopedWorkerInteraction();

     private:
      v8::Local<v8::Context> v8_context_;
      bool created_from_token_ = false;
    };

    // Used for Service Worker based extension Contexts.
    std::unique_ptr<ScopedWorkerInteraction> worker_thread_interaction_;
  };

  ExtensionInteractionProvider();

  ExtensionInteractionProvider(const ExtensionInteractionProvider&) = delete;
  ExtensionInteractionProvider& operator=(const ExtensionInteractionProvider&) =
      delete;

  ~ExtensionInteractionProvider() override;

  // Returns true if |v8_context| has an active interaction.
  static bool HasActiveExtensionInteraction(v8::Local<v8::Context> v8_context);

  // InteractionProvider:
  std::unique_ptr<InteractionProvider::Token> GetCurrentToken(
      v8::Local<v8::Context> v8_context) const override;
  std::unique_ptr<InteractionProvider::Scope> CreateScopedInteraction(
      v8::Local<v8::Context> v8_context,
      std::unique_ptr<InteractionProvider::Token> token) const override;
  bool HasActiveInteraction(v8::Local<v8::Context> v8_context) const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSION_INTERACTION_PROVIDER_H_
