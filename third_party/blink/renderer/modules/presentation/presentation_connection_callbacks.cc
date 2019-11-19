// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_connection_callbacks.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/presentation/presentation_connection.h"
#include "third_party/blink/renderer/modules/presentation/presentation_error.h"
#include "third_party/blink/renderer/modules/presentation/presentation_request.h"

namespace blink {

PresentationConnectionCallbacks::PresentationConnectionCallbacks(
    ScriptPromiseResolver* resolver,
    PresentationRequest* request)
    : resolver_(resolver), request_(request), connection_(nullptr) {
  DCHECK(resolver_);
  DCHECK(request_);
}

PresentationConnectionCallbacks::PresentationConnectionCallbacks(
    ScriptPromiseResolver* resolver,
    ControllerPresentationConnection* connection)
    : resolver_(resolver), request_(nullptr), connection_(connection) {
  DCHECK(resolver_);
  DCHECK(connection_);
}

void PresentationConnectionCallbacks::HandlePresentationResponse(
    mojom::blink::PresentationConnectionResultPtr result,
    mojom::blink::PresentationErrorPtr error) {
  if (!resolver_->GetExecutionContext() ||
      resolver_->GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  if (result) {
    DCHECK(result->connection_remote);
    DCHECK(result->connection_receiver);
    OnSuccess(*result->presentation_info, std::move(result->connection_remote),
              std::move(result->connection_receiver));
  } else {
    OnError(*error);
  }
}

void PresentationConnectionCallbacks::OnSuccess(
    const mojom::blink::PresentationInfo& presentation_info,
    mojo::PendingRemote<mojom::blink::PresentationConnection> connection_remote,
    mojo::PendingReceiver<mojom::blink::PresentationConnection>
        connection_receiver) {
  // Reconnect to existing connection.
  if (connection_ && connection_->GetState() ==
                         mojom::blink::PresentationConnectionState::CLOSED) {
    connection_->DidChangeState(
        mojom::blink::PresentationConnectionState::CONNECTING);
  }

  // Create a new connection.
  if (!connection_ && request_) {
    connection_ = ControllerPresentationConnection::Take(
        resolver_.Get(), presentation_info, request_);
  }

  resolver_->Resolve(connection_);
  connection_->Init(std::move(connection_remote),
                    std::move(connection_receiver));
}

void PresentationConnectionCallbacks::OnError(
    const mojom::blink::PresentationError& error) {
  resolver_->Reject(CreatePresentationError(error));
  connection_ = nullptr;
}

}  // namespace blink
