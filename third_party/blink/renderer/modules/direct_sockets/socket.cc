// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/socket.h"

#include <utility>

#include "net/base/net_errors.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

std::pair<DOMExceptionCode, String>
CreateDOMExceptionCodeAndMessageFromNetErrorCode(int32_t net_error) {
  switch (net_error) {
    case net::ERR_NAME_NOT_RESOLVED:
      return {DOMExceptionCode::kNetworkError,
              "Hostname couldn't be resolved."};
    case net::ERR_INVALID_URL:
      return {DOMExceptionCode::kDataError, "Supplied url is not valid."};
    case net::ERR_UNEXPECTED:
      return {DOMExceptionCode::kUnknownError, "Unexpected error occured."};
    case net::ERR_ACCESS_DENIED:
      return {DOMExceptionCode::kInvalidAccessError,
              "Access to the requested host is blocked."};
    case net::ERR_NETWORK_ACCESS_DENIED:
      return {DOMExceptionCode::kInvalidAccessError, "Firewall error."};
    default:
      return {DOMExceptionCode::kNetworkError, "Network Error."};
  }
}

}  // namespace

ScriptPromise Socket::opened(ScriptState* script_state) const {
  return ScriptPromise(script_state, opened_.Get(script_state->GetIsolate()));
}

ScriptPromise Socket::closed(ScriptState* script_state) const {
  return ScriptPromise(script_state, closed_.Get(script_state->GetIsolate()));
}

Socket::Socket(ScriptState* script_state)
    : ExecutionContextLifecycleStateObserver(
          ExecutionContext::From(script_state)),
      script_state_(script_state),
      service_(GetExecutionContext()),
      feature_handle_for_scheduler_(
          GetExecutionContext()->GetScheduler()->RegisterFeature(
              SchedulingPolicy::Feature::kOutstandingNetworkRequestDirectSocket,
              {SchedulingPolicy::DisableBackForwardCache()})),
      opened_resolver_(
          MakeGarbageCollected<ScriptPromiseResolver>(script_state)),
      opened_(script_state->GetIsolate(),
              opened_resolver_->Promise().V8Promise()),
      closed_resolver_(
          MakeGarbageCollected<ScriptPromiseResolver>(script_state)),
      closed_(script_state->GetIsolate(),
              closed_resolver_->Promise().V8Promise()) {
  UpdateStateIfNeeded();

  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      service_.BindNewPipeAndPassReceiver(
          GetExecutionContext()->GetTaskRunner(TaskType::kNetworking)));
  service_.set_disconnect_handler(
      WTF::BindOnce(&Socket::OnServiceConnectionError, WrapPersistent(this)));

  // |closed| promise is just one of the ways to learn that the socket state has
  // changed. Therefore it's not necessary to force developers to handle
  // rejections.
  closed_resolver_->Promise().MarkAsHandled();
}

Socket::~Socket() = default;

// static
bool Socket::CheckContextAndPermissions(ScriptState* script_state,
                                        ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Current context is detached.");
    return false;
  }

  if (!ExecutionContext::From(script_state)
           ->IsFeatureEnabled(
               mojom::blink::PermissionsPolicyFeature::kDirectSockets)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Permissions-Policy: direct-sockets are disabled.");
    return false;
  }

  return true;
}

// static
DOMException* Socket::CreateDOMExceptionFromNetErrorCode(int32_t net_error) {
  auto [code, message] =
      CreateDOMExceptionCodeAndMessageFromNetErrorCode(net_error);
  return MakeGarbageCollected<DOMException>(code, std::move(message));
}

void Socket::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(service_);

  visitor->Trace(opened_resolver_);
  visitor->Trace(opened_);

  visitor->Trace(closed_resolver_);
  visitor->Trace(closed_);

  ExecutionContextLifecycleStateObserver::Trace(visitor);
}

void Socket::ResetServiceAndFeatureHandle() {
  feature_handle_for_scheduler_.reset();
  service_.reset();
}

}  // namespace blink
