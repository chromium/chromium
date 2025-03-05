// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_LANGUAGE_MODEL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_LANGUAGE_MODEL_H_

#include "base/types/pass_key.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_language_model_clone_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_language_model_prompt_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_language_model_prompt_role.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/modules/ai/ai_language_model_factory.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

// The class that represents an `AILanguageModel` object.
class AILanguageModel final : public EventTarget,
                              public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Get the mojo enum value for the given V8 `role` enum value.
  static mojom::blink::AILanguageModelPromptRole ConvertRoleToMojo(
      V8AILanguageModelPromptRole role);

  AILanguageModel(
      ExecutionContext* execution_context,
      mojo::PendingRemote<mojom::blink::AILanguageModel> pending_remote,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      mojom::blink::AILanguageModelInstanceInfoPtr info);
  ~AILanguageModel() override = default;

  void Trace(Visitor* visitor) const override;

  // EventTarget implementation
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(contextoverflow, kContextoverflow)

  // ai_language_model.idl implementation.
  ScriptPromise<IDLString> prompt(ScriptState* script_state,
                                  const V8AILanguageModelPromptInput* input,
                                  const AILanguageModelPromptOptions* options,
                                  ExceptionState& exception_state);
  ReadableStream* promptStreaming(ScriptState* script_state,
                                  const V8AILanguageModelPromptInput* input,
                                  const AILanguageModelPromptOptions* options,
                                  ExceptionState& exception_state);
  ScriptPromise<IDLUnsignedLongLong> countPromptTokens(
      ScriptState* script_state,
      const WTF::String& input,
      const AILanguageModelPromptOptions* options,
      ExceptionState& exception_state);
  uint64_t maxTokens() const { return max_tokens_; }
  uint64_t tokensSoFar() const { return current_tokens_; }
  uint64_t tokensLeft() const { return max_tokens_ - current_tokens_; }

  uint32_t topK() const { return top_k_; }
  float temperature() const { return temperature_; }
  std::optional<WTF::Vector<WTF::String>> expectedInputLanguages() const {
    return expected_input_languages_;
  }

  ScriptPromise<AILanguageModel> clone(
      ScriptState* script_state,
      const AILanguageModelCloneOptions* options,
      ExceptionState& exception_state);
  void destroy(ScriptState* script_state, ExceptionState& exception_state);

  HeapMojoRemote<mojom::blink::AILanguageModel>& GetAILanguageModelRemote();
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner();
  uint64_t GetCurrentTokens();

 private:
  void OnResponseComplete(
      mojom::blink::ModelExecutionContextInfoPtr context_info);
  void OnContextOverflow();

  uint64_t current_tokens_;
  uint64_t max_tokens_ = 0;
  uint32_t top_k_ = 0;
  float temperature_ = 0.0;
  std::optional<WTF::Vector<WTF::String>> expected_input_languages_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  HeapMojoRemote<mojom::blink::AILanguageModel> language_model_remote_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_AI_LANGUAGE_MODEL_H_
