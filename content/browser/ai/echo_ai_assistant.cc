// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ai/echo_ai_assistant.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/ai/ai_assistant.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace content {

EchoAIAssistant::EchoAIAssistant() = default;

EchoAIAssistant::~EchoAIAssistant() = default;

void EchoAIAssistant::DoMockExecution(const std::string& input,
                                      mojo::RemoteSetElementId responder_id) {
  blink::mojom::ModelStreamingResponder* responder =
      responder_set_.Get(responder_id);
  if (!responder) {
    return;
  }

  const std::string response = "Model not available in Chromium\n" + input;
  // To make EchoAIAssistant simple, we will use the string length as the size
  // in tokens.
  current_tokens_ += response.size();
  responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kOngoing,
                        response, /*current_tokens=*/std::nullopt);
  responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kComplete,
                        /*text=*/std::nullopt, current_tokens_);
}

void EchoAIAssistant::Prompt(
    const std::string& input,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  if (is_destroyed_) {
    mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
        std::move(pending_responder));
    responder->OnResponse(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed,
        /*text=*/std::nullopt, /*current_tokens=*/std::nullopt);
    return;
  }

  mojo::RemoteSetElementId responder_id =
      responder_set_.Add(std::move(pending_responder));
  // Simulate the time taken by model execution.
  content::GetUIThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&EchoAIAssistant::DoMockExecution,
                     weak_ptr_factory_.GetWeakPtr(), input, responder_id),
      base::Seconds(1));
}

void EchoAIAssistant::Fork(
    mojo::PendingRemote<blink::mojom::AIManagerCreateAssistantClient> client) {
  mojo::Remote<blink::mojom::AIManagerCreateAssistantClient> client_remote(
      std::move(client));
  mojo::PendingRemote<blink::mojom::AIAssistant> assistant;

  mojo::MakeSelfOwnedReceiver(std::make_unique<EchoAIAssistant>(),
                              assistant.InitWithNewPipeAndPassReceiver());
  client_remote->OnResult(
      std::move(assistant),
      blink::mojom::AIAssistantInfo::New(
          optimization_guide::features::GetOnDeviceModelMaxTokensForContext(),
          blink::mojom::AIAssistantSamplingParams::New(
              optimization_guide::features::GetOnDeviceModelDefaultTopK(),
              optimization_guide::features::
                  GetOnDeviceModelDefaultTemperature())));
}

void EchoAIAssistant::Destroy() {
  is_destroyed_ = true;

  for (auto& responder : responder_set_) {
    responder->OnResponse(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed,
        /*text=*/std::nullopt, /*current_tokens=*/std::nullopt);
  }
  responder_set_.Clear();
}

void EchoAIAssistant::CountPromptTokens(
    const std::string& input,
    mojo::PendingRemote<blink::mojom::AIAssistantCountPromptTokensClient>
        client) {
  mojo::Remote<blink::mojom::AIAssistantCountPromptTokensClient>(
      std::move(client))
      ->OnResult(input.size());
}

}  // namespace content
