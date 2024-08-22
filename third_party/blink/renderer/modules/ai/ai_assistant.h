// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_ASSISTANT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_ASSISTANT_H_

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
  uint64_t maxTokens() const;
  uint64_t tokensSoFar() const;
  uint64_t tokensLeft() const;

  uint32_t topK() const;
  float temperature() const;

  ScriptPromise<AIAssistant> clone(ScriptState* script_state,
                                   ExceptionState& exception_state);
  void destroy(ScriptState* script_state, ExceptionState& exception_state);

 private:
  void OnResponseComplete(std::optional<uint64_t> current_tokens);

  uint64_t current_tokens_ = 0;

  Member<AITextSession> text_session_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_ASSISTANT_H_
