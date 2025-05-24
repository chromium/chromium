// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ai/echo_ai_proofreader.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace content {

EchoAIProofreader::EchoAIProofreader() = default;

EchoAIProofreader::~EchoAIProofreader() = default;

void EchoAIProofreader::Proofread(
    const std::string& input,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
      std::move(pending_responder));
  responder->OnStreaming("Model not available in Chromium\n" + input);
  responder->OnCompletion(/*context_info=*/nullptr);
}

}  // namespace content
