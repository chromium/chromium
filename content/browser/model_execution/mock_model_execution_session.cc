// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/model_execution/mock_model_execution_session.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/model_execution/model_session.mojom-params-data.h"
#include "third_party/blink/public/mojom/model_execution/model_session.mojom-shared.h"
#include "third_party/blink/public/mojom/model_execution/model_session.mojom.h"

MockModelExecutionSession::MockModelExecutionSession() = default;

MockModelExecutionSession::~MockModelExecutionSession() = default;

void MockModelExecutionSession::DoMockExecution(
    const std::string& input,
    mojo::RemoteSetElementId responder_id) {
  blink::mojom::ModelStreamingResponder* responder =
      responder_set_.Get(responder_id);
  if (!responder) {
    return;
  }

  responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kOngoing,
                        input);
  responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kComplete,
                        std::nullopt);
}

void MockModelExecutionSession::Execute(
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
      base::BindOnce(&MockModelExecutionSession::DoMockExecution,
                     weak_ptr_factory_.GetWeakPtr(), input, responder_id),
      base::Seconds(1));
}

void MockModelExecutionSession::Destroy() {
  is_destroyed_ = true;

  for (auto& responder : responder_set_) {
    responder->OnResponse(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed,
        std::nullopt);
  }
  responder_set_.Clear();
}
