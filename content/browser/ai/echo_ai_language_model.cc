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
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace content {

EchoAILanguageModel::EchoAILanguageModel() = default;

EchoAILanguageModel::~EchoAILanguageModel() = default;

void EchoAILanguageModel::DoMockExecution(
    const std::string& input,
    mojo::RemoteSetElementId responder_id) {
  blink::mojom::ModelStreamingResponder* responder =
      responder_set_.Get(responder_id);
  if (!responder) {
    return;
  }

  const std::string response =
      "On-device model is not available in Chromium, this API is just echoing "
      "back the input:\n" +
      input;
  // To make EchoAILanguageModel simple, we will use the string length as the
  // size in tokens, and the `current_tokens_` will only keep track of the
  // response size. Once overflow, it will be cleared.
  current_tokens_ += response.size();
  bool did_overflow = false;
  if (current_tokens_ > EchoAIManagerImpl::kMaxContextSizeInTokens) {
    current_tokens_ = 0;
    did_overflow = true;
  }
  responder->OnStreaming(response);
  responder->OnCompletion(blink::mojom::ModelExecutionContextInfo::New(
      current_tokens_, did_overflow));
}

void EchoAILanguageModel::Prompt(
    const std::string& input,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  if (is_destroyed_) {
    mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
        std::move(pending_responder));
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed);
    return;
  }

  mojo::RemoteSetElementId responder_id =
      responder_set_.Add(std::move(pending_responder));
  // Simulate the time taken by model execution.
  content::GetUIThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&EchoAILanguageModel::DoMockExecution,
                     weak_ptr_factory_.GetWeakPtr(), input, responder_id),
      base::Seconds(1));
}

void EchoAILanguageModel::Fork(
    mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
        client) {
  mojo::Remote<blink::mojom::AIManagerCreateLanguageModelClient> client_remote(
      std::move(client));
  mojo::PendingRemote<blink::mojom::AILanguageModel> language_model;

  mojo::MakeSelfOwnedReceiver(std::make_unique<EchoAILanguageModel>(),
                              language_model.InitWithNewPipeAndPassReceiver());
  client_remote->OnResult(
      std::move(language_model),
      blink::mojom::AILanguageModelInfo::New(
          EchoAIManagerImpl::kMaxContextSizeInTokens,
          blink::mojom::AILanguageModelSamplingParams::New(
              optimization_guide::features::GetOnDeviceModelDefaultTopK(),
              optimization_guide::features::
                  GetOnDeviceModelDefaultTemperature())));
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
