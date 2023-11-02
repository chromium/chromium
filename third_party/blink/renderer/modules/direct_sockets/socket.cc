// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/socket.h"

#include <utility>

#include "net/base/net_errors.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
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
    case net::ERR_BLOCKED_BY_RESPONSE:
      return {
          DOMExceptionCode::kInvalidAccessError,
          "Access to the requested host is blocked by cross-origin policy."};
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

ScriptPromise Socket::close(ScriptState*, ExceptionState& exception_state) {
  if (!Initialized()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Socket is not properly initialized.");
    return ScriptPromise();
  }

  if (Closed()) {
    return closed(script_state_);
  }

  if (readable_stream_wrapper_->Locked() ||
      writable_stream_wrapper_->Locked()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Close called on locked streams.");
    return ScriptPromise();
  }

  auto* reason = MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kAbortError, "Stream closed.");

  auto readable_cancel = readable_stream_wrapper_->Readable()->cancel(
      script_state_, ScriptValue::From(script_state_, reason), exception_state);
  DCHECK(!exception_state.HadException()) << exception_state.Message();
  readable_cancel.MarkAsHandled();

  auto writable_abort = writable_stream_wrapper_->Writable()->abort(
      script_state_, ScriptValue::From(script_state_, reason), exception_state);
  DCHECK(!exception_state.HadException()) << exception_state.Message();
  writable_abort.MarkAsHandled();

  return closed(script_state_);
}

Socket::Socket(ScriptState* script_state)
    : ExecutionContextLifecycleStateObserver(
          ExecutionContext::From(script_state)),
      script_state_(script_state),
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

void Socket::ContextDestroyed() {}

void Socket::ContextLifecycleStateChanged(
    mojom::blink::FrameLifecycleState state) {
  if (state == mojom::blink::FrameLifecycleState::kFrozen) {
    // Clear service_remote_ and fail pending connection.
    OnServiceConnectionError();
  }
}

void Socket::ConnectService() {
  service_ = DirectSocketsServiceMojoRemote::Create(
      GetExecutionContext(),
      WTF::BindOnce(&Socket::OnServiceConnectionError, WrapPersistent(this)));
}

bool Socket::Closed() const {
  return !closed_resolver_;
}

bool Socket::Initialized() const {
  return readable_stream_wrapper_ && writable_stream_wrapper_;
}

bool Socket::HasPendingActivity() const {
  return writable_stream_wrapper_ &&
         writable_stream_wrapper_->HasPendingWrite();
}

void Socket::ResolveClosed() {
  closed_resolver_->Resolve();
  closed_resolver_ = nullptr;
}

void Socket::RejectClosed(ScriptValue exception) {
  closed_resolver_->Reject(exception);
  closed_resolver_ = nullptr;
}

void Socket::CloseServiceAndResetFeatureHandle() {
  if (service_) {
    service_->Close();
  }
  feature_handle_for_scheduler_.reset();
}

void Socket::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(service_);

  visitor->Trace(opened_resolver_);
  visitor->Trace(opened_);

  visitor->Trace(closed_resolver_);
  visitor->Trace(closed_);

  visitor->Trace(readable_stream_wrapper_);
  visitor->Trace(writable_stream_wrapper_);

  ExecutionContextLifecycleStateObserver::Trace(visitor);
}

}  // namespace blink
