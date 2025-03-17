// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ai/echo_ai_language_model.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/location.h"
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
    blink::mojom::AILanguageModelSamplingParamsPtr sampling_params)
    : sampling_params_(std::move(sampling_params)) {}

EchoAILanguageModel::~EchoAILanguageModel() = default;

void EchoAILanguageModel::DoMockExecution(
    const std::string& input,
    mojo::RemoteSetElementId responder_id) {
  blink::mojom::ModelStreamingResponder* responder =
      responder_set_.Get(responder_id);
  if (!responder) {
    return;
  }

  if (input.size() > EchoAIManagerImpl::kMaxContextSizeInTokens) {
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorInputTooLarge);
    return;
  }
  if (current_tokens_ >
      EchoAIManagerImpl::kMaxContextSizeInTokens - input.size()) {
    current_tokens_ = input.size();
    responder->OnContextOverflow();
  }
  current_tokens_ += input.size();
  responder->OnStreaming(kResponsePrefix,
                         blink::mojom::ModelStreamingResponderAction::kAppend);
  responder->OnStreaming(input,
                         blink::mojom::ModelStreamingResponderAction::kAppend);
  responder->OnCompletion(
      blink::mojom::ModelExecutionContextInfo::New(current_tokens_));
}

void EchoAILanguageModel::Prompt(
    std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  if (is_destroyed_) {
    mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
        std::move(pending_responder));
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);
    return;
  }

  std::string response = "";
  for (const auto& prompt : prompts) {
    if (prompt->content->is_text()) {
      response += prompt->content->get_text();
    } else if (prompt->content->is_bitmap()) {
      response += "<image>";
    } else if (prompt->content->is_audio()) {
      response += "<audio>";
    } else {
      NOTIMPLEMENTED_LOG_ONCE();
    }
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

void EchoAILanguageModel::Fork(
    mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
        client) {
  mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient> client_remote(
      std::move(client));
  mojo::PendingRemote<blink::mojom::AILanguageModel> language_model;

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<EchoAILanguageModel>(sampling_params_.Clone()),
      language_model.InitWithNewPipeAndPassReceiver());
  client_remote->OnResult(
      std::move(language_model),
      blink::mojom::AILanguageModelInstanceInfo::New(
          EchoAIManagerImpl::kMaxContextSizeInTokens, current_tokens_,
          sampling_params_->Clone(), std::nullopt));
}

void EchoAILanguageModel::Destroy() {
  is_destroyed_ = true;

  for (auto& responder : responder_set_) {
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);
  }
  responder_set_.Clear();
}

void EchoAILanguageModel::CountPromptTokens(
    const std::string& input,
    mojo::PendingRemote<blink::mojom::AILanguageModelCountPromptTokensClient>
        client) {
  mojo::Remote<blink::mojom::AILanguageModelCountPromptTokensClient>(
      std::move(client))
      ->OnResult(input.size());
}

}  // namespace content
