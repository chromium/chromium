// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_ASSISTANT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_ASSISTANT_H_

#include "base/types/pass_key.h"
#include "third_party/blink/public/mojom/ai/ai_assistant.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_assistant_clone_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_assistant_prompt_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/ai/ai_assistant_factory.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

// The class that represents an `AIAssistant` object.
class AIAssistant final : public ScriptWrappable,
                          public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  AIAssistant(ExecutionContext* execution_context,
              mojo::PendingRemote<mojom::blink::AIAssistant> pending_remote,
              scoped_refptr<base::SequencedTaskRunner> task_runner,
              mojom::blink::AIAssistantInfoPtr info,
              uint64_t current_tokens);
  ~AIAssistant() override = default;

  void Trace(Visitor* visitor) const override;

  // ai_assistant.idl implementation.
  ScriptPromise<IDLString> prompt(ScriptState* script_state,
                                  const WTF::String& input,
                                  const AIAssistantPromptOptions* options,
                                  ExceptionState& exception_state);
  ReadableStream* promptStreaming(ScriptState* script_state,
                                  const WTF::String& input,
                                  const AIAssistantPromptOptions* options,
                                  ExceptionState& exception_state);
  ScriptPromise<IDLUnsignedLongLong> countPromptTokens(
      ScriptState* script_state,
      const WTF::String& input,
      const AIAssistantPromptOptions* options,
      ExceptionState& exception_state);
  uint64_t maxTokens() const { return max_tokens_; }
  uint64_t tokensSoFar() const { return current_tokens_; }
  uint64_t tokensLeft() const { return max_tokens_ - current_tokens_; }

  uint32_t topK() const { return top_k_; }
  float temperature() const { return temperature_; }

  ScriptPromise<AIAssistant> clone(ScriptState* script_state,
                                   const AIAssistantCloneOptions* options,
                                   ExceptionState& exception_state);
  void destroy(ScriptState* script_state, ExceptionState& exception_state);

  // Allows `AIAssistantFactory` (for creating assistant) and `AIAssistant` (for
  // cloning assistant) to set the info after getting it from the remote.
  void SetInfo(std::variant<base::PassKey<AIAssistantFactory>,
                            base::PassKey<AIAssistant>> pass_key,
               const mojom::blink::AIAssistantInfoPtr info);

  HeapMojoRemote<mojom::blink::AIAssistant>& GetAIAssistantRemote();
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner();
  uint64_t GetCurrentTokens();

 private:
  void OnResponseComplete(std::optional<uint64_t> current_tokens);

  uint64_t current_tokens_;
  uint64_t max_tokens_ = 0;
  uint32_t top_k_ = 0;
  float temperature_ = 0.0;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  HeapMojoRemote<mojom::blink::AIAssistant> assistant_remote_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_ASSISTANT_H_
