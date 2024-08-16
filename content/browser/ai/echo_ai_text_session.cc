// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ai/echo_ai_text_session.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/ai/ai_text_session.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_text_session_info.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace content {

EchoAITextSession::EchoAITextSession() = default;

EchoAITextSession::~EchoAITextSession() = default;

void EchoAITextSession::DoMockExecution(const std::string& input,
                                        mojo::RemoteSetElementId responder_id) {
  blink::mojom::ModelStreamingResponder* responder =
      responder_set_.Get(responder_id);
  if (!responder) {
    return;
  }

  const std::string response = "Model not available in Chromium\n" + input;
  // To make EchoAITextSession simple, we will use the string length as the size
  // in tokens.
  current_tokens_ += response.size();
  responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kOngoing,
                        response, /*current_tokens=*/std::nullopt);
  responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kComplete,
                        /*text=*/std::nullopt, current_tokens_);
}

void EchoAITextSession::Prompt(
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
      base::BindOnce(&EchoAITextSession::DoMockExecution,
                     weak_ptr_factory_.GetWeakPtr(), input, responder_id),
      base::Seconds(1));
}

void EchoAITextSession::Fork(
    mojo::PendingReceiver<blink::mojom::AITextSession> session,
    ForkCallback callback) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<EchoAITextSession>(),
                              std::move(session));
  std::move(callback).Run(blink::mojom::AITextSessionInfo::New(
      optimization_guide::features::GetOnDeviceModelMaxTokensForContext(),
      blink::mojom::AITextSessionSamplingParams::New(
          optimization_guide::features::GetOnDeviceModelDefaultTopK(),
          optimization_guide::features::GetOnDeviceModelDefaultTemperature())));
}

void EchoAITextSession::Destroy() {
  is_destroyed_ = true;

  for (auto& responder : responder_set_) {
    responder->OnResponse(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed,
        /*text=*/std::nullopt, /*current_tokens=*/std::nullopt);
  }
  responder_set_.Clear();
}

}  // namespace content
