// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ai/echo_ai_text_session.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/ai/ai_text_session.mojom-params-data.h"
#include "third_party/blink/public/mojom/ai/ai_text_session.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/ai_text_session.mojom.h"

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

  responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kOngoing,
                        "Model not available in Chromium\n" + input);
  responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kComplete,
                        std::nullopt);
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
        std::nullopt);
    return;
  }

  mojo::RemoteSetElementId responder_id =
      responder_set_.Add(std::move(pending_responder));
  content::GetUIThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&EchoAITextSession::DoMockExecution,
                     weak_ptr_factory_.GetWeakPtr(), input, responder_id),
      base::Seconds(1));
}

void EchoAITextSession::Destroy() {
  is_destroyed_ = true;

  for (auto& responder : responder_set_) {
    responder->OnResponse(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed,
        std::nullopt);
  }
  responder_set_.Clear();
}

}  // namespace content
