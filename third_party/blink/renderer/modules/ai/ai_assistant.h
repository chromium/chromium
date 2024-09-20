// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_ASSISTANT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_ASSISTANT_H_

#include "third_party/blink/public/mojom/ai/ai_assistant.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/ai/ai_text_session.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

// The class that represents an `AIAssistant` object.
class AIAssistant final : public ScriptWrappable,
                          public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  AIAssistant(ExecutionContext* context,
              AITextSession* text_session,
              scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~AIAssistant() override = default;

  void Trace(Visitor* visitor) const override;

  // ai_assistant.idl implementation.
  ScriptPromise<IDLString> prompt(ScriptState* script_state,
                                  const WTF::String& input,
                                  ExceptionState& exception_state);
  ReadableStream* promptStreaming(ScriptState* script_state,
                                  const WTF::String& input,
                                  ExceptionState& exception_state);
  uint64_t maxTokens() const { return max_tokens_; }
  uint64_t tokensSoFar() const { return current_tokens_; }
  uint64_t tokensLeft() const { return max_tokens_ - current_tokens_; }

  uint32_t topK() const { return top_k_; }
  float temperature() const { return temperature_; }

  ScriptPromise<AIAssistant> clone(ScriptState* script_state,
                                   ExceptionState& exception_state);
  void destroy(ScriptState* script_state, ExceptionState& exception_state);

 private:
  void OnResponseComplete(std::optional<uint64_t> current_tokens);
  // If the `AIAssistant` is created by the factory, the `text_session_` should
  // have the up-to-date info, and this will be called from the constructor.
  // If the `AIAssistant` is created by cloning another `AIAssistant`, this will
  // be set later by the `clone()` method.
  void SetInfo(const blink::mojom::blink::AIAssistantInfoPtr info);

  uint64_t current_tokens_ = 0;
  uint64_t max_tokens_ = 0;
  uint32_t top_k_ = 0;
  float temperature_ = 0.0;

  Member<AITextSession> text_session_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_ASSISTANT_H_
