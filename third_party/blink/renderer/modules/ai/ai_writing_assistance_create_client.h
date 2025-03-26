// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_WRITING_ASSISTANCE_CREATE_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_WRITING_ASSISTANCE_CREATE_CLIENT_H_

#include "third_party/blink/renderer/modules/ai/ai.h"
#include "third_party/blink/renderer/modules/ai/ai_context_observer.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"

namespace blink {

template <typename AIMojoClient,
          typename AIMojoCreateClient,
          typename CreateOptions,
          typename V8SessionObjectType>
class AIWritingAssistanceCreateClient
    : public GarbageCollected<
          AIWritingAssistanceCreateClient<AIMojoClient,
                                          AIMojoCreateClient,
                                          CreateOptions,
                                          V8SessionObjectType>>,
      public AIMojoCreateClient,
      public AIContextObserver<V8SessionObjectType> {
 public:
  AIWritingAssistanceCreateClient(
      ScriptState* script_state,
      AI* ai,
      ScriptPromiseResolver<V8SessionObjectType>* resolver,
      CreateOptions* options)
      : AIContextObserver<V8SessionObjectType>(script_state,
                                               ai,
                                               resolver,
                                               options->getSignalOr(nullptr)),
        ai_(ai),
        receiver_(this, ai->GetExecutionContext()),
        options_(options) {}
  ~AIWritingAssistanceCreateClient() override = default;

  AIWritingAssistanceCreateClient(const AIWritingAssistanceCreateClient&) =
      delete;
  AIWritingAssistanceCreateClient& operator=(
      const AIWritingAssistanceCreateClient&) = delete;

  void Trace(Visitor* visitor) const override {
    AIContextObserver<V8SessionObjectType>::Trace(visitor);
    visitor->Trace(ai_);
    visitor->Trace(receiver_);
    visitor->Trace(options_);
  }

  void Create() {
    mojo::PendingRemote<AIMojoCreateClient> client_remote;
    receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver(),
                   ai_->GetTaskRunner());
    RemoteCreate(std::move(client_remote));
  }

  // AIMojoCreateClient:
  void OnResult(mojo::PendingRemote<AIMojoClient> pending_remote) override {
    if (!this->GetResolver()) {
      return;
    }
    if (ai_->GetExecutionContext() && pending_remote) {
      this->GetResolver()->Resolve(MakeGarbageCollected<V8SessionObjectType>(
          ai_->GetExecutionContext(), ai_->GetTaskRunner(),
          std::move(pending_remote), options_));
    } else {
      this->GetResolver()->RejectWithDOMException(
          DOMExceptionCode::kInvalidStateError,
          kExceptionMessageUnableToCreateSession);
    }
    this->Cleanup();
  }

  void OnError(mojom::blink::AIManagerCreateClientError error) override {
    if (!this->GetResolver()) {
      return;
    }

    using mojom::blink::AIManagerCreateClientError;

    switch (error) {
      case AIManagerCreateClientError::kUnableToCreateSession:
      case AIManagerCreateClientError::kUnableToCalculateTokenSize: {
        this->GetResolver()->RejectWithDOMException(
            DOMExceptionCode::kInvalidStateError,
            kExceptionMessageUnableToCreateSession);
        break;
      }
      case AIManagerCreateClientError::kInitialInputTooLarge: {
        this->GetResolver()->RejectWithDOMException(
            DOMExceptionCode::kQuotaExceededError,
            kExceptionMessageInputTooLarge);
        break;
      }
      case AIManagerCreateClientError::kUnsupportedLanguage: {
        this->GetResolver()->RejectWithDOMException(
            DOMExceptionCode::kNotSupportedError,
            kExceptionMessageUnsupportedLanguages);
        break;
      }
    }
    this->Cleanup();
  }

  // AIContextObserver:
  void ResetReceiver() override { receiver_.reset(); }

 protected:
  // Executes a mojo call to create `AIMojoClient`.
  virtual void RemoteCreate(
      mojo::PendingRemote<AIMojoCreateClient> client_remote) = 0;

  Member<AI> ai_;
  HeapMojoReceiver<AIMojoCreateClient, AIWritingAssistanceCreateClient>
      receiver_;
  Member<CreateOptions> options_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_WRITING_ASSISTANCE_CREATE_CLIENT_H_
