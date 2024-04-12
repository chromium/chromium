// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/model_execution/mock_model_execution_session.h"

#include <optional>

#include "base/functional/bind.h"
#include "third_party/blink/public/mojom/model_execution/model_session.mojom-shared.h"

MockModelExecutionSession::MockModelExecutionSession() = default;

MockModelExecutionSession::~MockModelExecutionSession() = default;

void MockModelExecutionSession::Execute(
    const std::string& input,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder> responder) {
  blink::mojom::ModelStreamingResponder* bound_responder =
      responder_set_.Get(responder_set_.Add(std::move(responder)));

  bound_responder->OnResponse(
      blink::mojom::ModelStreamingResponseStatus::kOngoing, input);
  bound_responder->OnResponse(
      blink::mojom::ModelStreamingResponseStatus::kComplete, std::nullopt);
}
