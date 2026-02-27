// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AI_ECHO_AI_LANGUAGE_MODEL_H_
#define CONTENT_BROWSER_AI_ECHO_AI_LANGUAGE_MODEL_H_

#include <optional>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"

namespace content {

// The implementation of `blink::mojom::AILanguageModel` which only echoes
// back the prompt text used for testing.
class EchoAILanguageModel : public blink::mojom::AILanguageModel {
 public:
  explicit EchoAILanguageModel(
      blink::mojom::AILanguageModelSamplingParamsPtr sampling_params,
      base::flat_set<blink::mojom::AILanguageModelPromptType> input_types,
      std::vector<blink::mojom::AILanguageModelPromptPtr> initial_prompts,
      uint32_t initial_tokens_size);
  EchoAILanguageModel(const EchoAILanguageModel&) = delete;
  EchoAILanguageModel& operator=(const EchoAILanguageModel&) = delete;

  ~EchoAILanguageModel() override;

  // `blink::mojom::AILanguageModel` implementation.
  void Prompt(std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
              on_device_model::mojom::ResponseConstraintPtr constraint,
              mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                  pending_responder) override;
  void Append(std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
              mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                  pending_responder) override;
  void Fork(
      mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
          client) override;
  void Destroy() override;
  void MeasureInputUsage(
      std::vector<blink::mojom::AILanguageModelPromptPtr> input,
      MeasureInputUsageCallback callback) override;

 private:
  void DoMockExecution(const std::string& input,
                       mojo::RemoteSetElementId responder_id);

  // Stringify a list of prompts. Returns nullopt on failure.
  std::optional<std::string> PromptsToText(
      const std::vector<blink::mojom::AILanguageModelPromptPtr>& prompts);

  bool is_destroyed_ = false;
  uint64_t current_tokens_ = 0;
  blink::mojom::AILanguageModelSamplingParamsPtr sampling_params_;
  // Prompt types supported by the language model in this session.
  base::flat_set<blink::mojom::AILanguageModelPromptType> input_types_;

  mojo::RemoteSet<blink::mojom::ModelStreamingResponder> responder_set_;

  // Initial prompts are echoed on the first Prompt call.
  std::vector<blink::mojom::AILanguageModelPromptPtr> initial_prompts_;
  bool did_echo_initial_prompts_ = false;

  std::vector<blink::mojom::AILanguageModelPromptPtr> prompt_history_;

  base::WeakPtrFactory<EchoAILanguageModel> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_AI_ECHO_AI_LANGUAGE_MODEL_H_
