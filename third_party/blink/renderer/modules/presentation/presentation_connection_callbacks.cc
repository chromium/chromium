// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_connection_callbacks.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/modules/presentation/presentation_connection.h"
#include "third_party/blink/renderer/modules/presentation/presentation_error.h"
#include "third_party/blink/renderer/modules/presentation/presentation_request.h"

#if BUILDFLAG(IS_ANDROID)
#include "third_party/blink/renderer/modules/presentation/presentation_metrics.h"
#endif

namespace blink {

PresentationConnectionCallbacks::PresentationConnectionCallbacks(
    ScriptPromiseResolver<PresentationConnection>* resolver,
    PresentationRequest* request)
    : resolver_(resolver), request_(request), connection_(nullptr) {
  DCHECK(resolver_);
  DCHECK(request_);
}

PresentationConnectionCallbacks::PresentationConnectionCallbacks(
    ScriptPromiseResolver<PresentationConnection>* resolver,
    ControllerPresentationConnection* connection)
    : resolver_(resolver), request_(nullptr), connection_(connection) {
  DCHECK(resolver_);
  DCHECK(connection_);
}

void PresentationConnectionCallbacks::HandlePresentationResponse(
    mojom::blink::PresentationConnectionResultPtr result,
    mojom::blink::PresentationErrorPtr error) {
  DCHECK(resolver_);

  ScriptState* const script_state = resolver_->GetScriptState();

  if (!IsInParallelAlgorithmRunnable(resolver_->GetExecutionContext(),
                                     script_state)) {
    return;
  }

  ScriptState::Scope script_state_scope(script_state);

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

  connection_->Init(std::move(connection_remote),
                    std::move(connection_receiver));
#if BUILDFLAG(IS_ANDROID)
  PresentationMetrics::RecordPresentationConnectionResult(request_, true);
#endif

  resolver_->Resolve(connection_);
}

void PresentationConnectionCallbacks::OnError(
    const mojom::blink::PresentationError& error) {
#if BUILDFLAG(IS_ANDROID)
  // These two error types are not recorded because it's likely that they don't
  // represent an actual error.
  if (error.error_type !=
          mojom::blink::PresentationErrorType::PRESENTATION_REQUEST_CANCELLED &&
      error.error_type !=
          mojom::blink::PresentationErrorType::NO_PRESENTATION_FOUND) {
    PresentationMetrics::RecordPresentationConnectionResult(request_, false);
  }
#endif

  resolver_->Reject(CreatePresentationError(
      resolver_->GetScriptState()->GetIsolate(), error));
  connection_ = nullptr;
}

}  // namespace blink
