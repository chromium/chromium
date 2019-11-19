// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSION_INTERACTION_PROVIDER_H_
#define EXTENSIONS_RENDERER_EXTENSION_INTERACTION_PROVIDER_H_

#include "extensions/renderer/bindings/interaction_provider.h"

#include "base/optional.h"
#include "third_party/blink/public/web/web_user_gesture_token.h"
#include "v8/include/v8.h"

namespace blink {
class WebLocalFrame;
class WebScopedUserGesture;
}  // namespace blink

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
    ~Token() override;

    // Only valid for WebFrame based tokens.
    blink::WebUserGestureToken web_frame_token() const { return *frame_token_; }

    bool is_for_service_worker() const { return is_for_service_worker_; }

   private:
    friend class ExtensionInteractionProvider;
    friend class TestInteractionProvider;

    Token(bool is_for_service_worker);

    bool is_for_service_worker_ = false;

    // Used when this token is for main thread, i.e. when is_for_service_worker_
    // is false.
    base::Optional<blink::WebUserGestureToken> frame_token_;

    DISALLOW_COPY_AND_ASSIGN(Token);
  };

  // Extension system specific implementation of scope.
  class Scope : public InteractionProvider::Scope {
   public:
    ~Scope() override;

    // Creates a Scope for a Service Worker context, without token.
    static std::unique_ptr<Scope> ForWorker(v8::Local<v8::Context> v8_context);
    // Creates a scope for a RenderFrame, without token.
    static std::unique_ptr<Scope> ForFrame(blink::WebLocalFrame* web_frame);

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
    // Used for RenderFrame based extension Contexts.
    std::unique_ptr<blink::WebScopedUserGesture> main_thread_gesture_;

    DISALLOW_COPY_AND_ASSIGN(Scope);
  };

  ExtensionInteractionProvider();
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

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionInteractionProvider);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSION_INTERACTION_PROVIDER_H_
