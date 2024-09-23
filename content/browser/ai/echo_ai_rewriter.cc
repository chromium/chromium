// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ai/echo_ai_rewriter.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace content {

void EchoAIRewriter::Rewrite(
    const std::string& input,
    const std::optional<std::string>& context,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
      std::move(pending_responder));
  responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kOngoing,
                        "Model not available in Chromium\n" + input,
                        /*current_tokens=*/std::nullopt);
  responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kComplete,
                        std::nullopt, /*current_tokens=*/std::nullopt);
}

}  // namespace content
