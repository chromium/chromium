// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ai/echo_ai_language_model.h"

#include <optional>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notimplemented.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/browser/ai/echo_ai_manager_impl.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace content {

namespace {
constexpr char kResponsePrefix[] =
    "On-device model is not available in Chromium, this API is just echoing "
    "back the input:\n";
}

EchoAILanguageModel::EchoAILanguageModel(
    blink::mojom::AILanguageModelSamplingParamsPtr sampling_params,
    base::flat_set<blink::mojom::AILanguageModelPromptType> input_types,
    std::vector<blink::mojom::AILanguageModelPromptPtr> initial_prompts,
    uint32_t initial_tokens_size)
    : current_tokens_(initial_tokens_size),
      sampling_params_(std::move(sampling_params)),
      input_types_(input_types),
      initial_prompts_(std::move(initial_prompts)) {}

EchoAILanguageModel::~EchoAILanguageModel() = default;

void EchoAILanguageModel::DoMockExecution(
    const std::string& input,
    mojo::RemoteSetElementId responder_id) {
  blink::mojom::ModelStreamingResponder* responder =
      responder_set_.Get(responder_id);
  if (!responder) {
    return;
  }

  uint32_t quota = EchoAIManagerImpl::kMaxContextSizeInTokens;
  if (input.size() > quota) {
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorInputTooLarge,
        blink::mojom::QuotaErrorInfo::New(input.size(), quota));
    return;
  }
  if (current_tokens_ > quota - input.size()) {
    current_tokens_ = input.size();
    responder->OnQuotaOverflow();
  }
  current_tokens_ += input.size();
  responder->OnStreaming(kResponsePrefix);
  responder->OnStreaming(input);
  responder->OnCompletion(
      blink::mojom::ModelExecutionContextInfo::New(current_tokens_));
}

void EchoAILanguageModel::Prompt(
    std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
    on_device_model::mojom::ResponseConstraintPtr constraint,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  if (is_destroyed_) {
    mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
        std::move(pending_responder));
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed,
        /*quota_error_info=*/nullptr);
    return;
  }
  std::string response = "";

  if (!did_echo_initial_prompts_) {
    auto initial_string = PromptsToText(initial_prompts_);
    if (!initial_string.has_value()) {
      return;
    }
    response.append(*initial_string);
    did_echo_initial_prompts_ = true;
  }

  auto prompts_as_string = PromptsToText(prompts);
  if (!prompts_as_string.has_value()) {
    return;
  }
  response.append(*prompts_as_string);
  for (const auto& prompt : prompts) {
    prompt_history_.push_back(prompt.Clone());
  }

  mojo::RemoteSetElementId responder_id =
      responder_set_.Add(std::move(pending_responder));
  // Simulate the time taken by model execution.
  content::GetUIThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&EchoAILanguageModel::DoMockExecution,
                     weak_ptr_factory_.GetWeakPtr(), response, responder_id),
      base::Seconds(1));
}

void EchoAILanguageModel::Append(
    std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  for (const auto& prompt : prompts) {
    prompt_history_.push_back(prompt.Clone());
  }
  mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
      std::move(pending_responder));
  responder->OnCompletion(
      blink::mojom::ModelExecutionContextInfo::New(current_tokens_));
}

void EchoAILanguageModel::Fork(
    mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
        client) {
  mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient> client_remote(
      std::move(client));
  mojo::PendingRemote<blink::mojom::AILanguageModel> language_model;

  // This sessions initial prompts + history is copied into the
  // forked session as initial prompts.
  std::vector<blink::mojom::AILanguageModelPromptPtr> prompts_copy;
  for (const auto& prompt : initial_prompts_) {
    prompts_copy.push_back(prompt.Clone());
  }
  for (const auto& prompt : prompt_history_) {
    prompts_copy.push_back(prompt.Clone());
  }
  mojo::MakeSelfOwnedReceiver(std::make_unique<EchoAILanguageModel>(
                                  sampling_params_.Clone(), input_types_,
                                  std::move(prompts_copy), current_tokens_),
                              language_model.InitWithNewPipeAndPassReceiver());
  client_remote->OnResult(
      std::move(language_model),
      blink::mojom::AILanguageModelInstanceInfo::New(
          EchoAIManagerImpl::kMaxContextSizeInTokens, current_tokens_,
          sampling_params_->Clone(), base::ToVector(input_types_)));
}

void EchoAILanguageModel::Destroy() {
  is_destroyed_ = true;

  for (auto& responder : responder_set_) {
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed,
        /*quota_error_info=*/nullptr);
  }
  responder_set_.Clear();
}

void EchoAILanguageModel::MeasureInputUsage(
    std::vector<blink::mojom::AILanguageModelPromptPtr> input,
    MeasureInputUsageCallback callback) {
  size_t total = 0;
  for (const auto& prompt : input) {
    for (const auto& content : prompt->content) {
      if (content->is_text()) {
        total += content->get_text().size();
      } else {
        total += 100;  // TODO(crbug.com/415304330): Improve estimate.
      }
    }
  }
  std::move(callback).Run(total);
}

std::optional<std::string> EchoAILanguageModel::PromptsToText(
    const std::vector<blink::mojom::AILanguageModelPromptPtr>& prompts) {
  std::string response = "";
  for (const auto& prompt : prompts) {
    for (auto& content : prompt->content) {
      if (content->is_text()) {
        response += content->get_text();
      } else if (content->is_bitmap()) {
        if (!input_types_.contains(
                blink::mojom::AILanguageModelPromptType::kImage)) {
          mojo::ReportBadMessage("Image input is not supported.");
          return std::nullopt;
        }
        response += "<image>";
      } else if (content->is_audio()) {
        if (!input_types_.contains(
                blink::mojom::AILanguageModelPromptType::kAudio)) {
          mojo::ReportBadMessage("Audio input is not supported.");
          return std::nullopt;
        }

        response += "<audio>";
      } else {
        NOTIMPLEMENTED_LOG_ONCE();
      }
    }
  }
  return response;
}

}  // namespace content
