// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/smart_card/smart_card_connection.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_disposition.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_protocol.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/smart_card/smart_card_error.h"

namespace blink {
namespace {
constexpr char kOperationInProgress[] = "An operation is in progress.";
constexpr char kDisconnected[] = "Is disconnected.";

using device::mojom::blink::SmartCardDisposition;

SmartCardDisposition ToMojomDisposition(
    const V8SmartCardDisposition& disposition) {
  switch (disposition.AsEnum()) {
    case V8SmartCardDisposition::Enum::kLeave:
      return SmartCardDisposition::kLeave;
    case V8SmartCardDisposition::Enum::kReset:
      return SmartCardDisposition::kReset;
    case V8SmartCardDisposition::Enum::kUnpower:
      return SmartCardDisposition::kUnpower;
    case V8SmartCardDisposition::Enum::kEject:
      return SmartCardDisposition::kEject;
  }
}
}  // anonymous namespace

SmartCardConnection::SmartCardConnection(
    mojo::PendingRemote<device::mojom::blink::SmartCardConnection>
        pending_connection,
    device::mojom::blink::SmartCardProtocol active_protocol,
    ExecutionContext* execution_context)
    : connection_(execution_context), active_protocol_(active_protocol) {
  connection_.Bind(
      std::move(pending_connection),
      execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI));
}

ScriptPromise SmartCardConnection::disconnect(ScriptState* script_state,
                                              ExceptionState& exception_state) {
  return disconnect(
      script_state,
      V8SmartCardDisposition(V8SmartCardDisposition::Enum::kLeave),
      exception_state);
}

ScriptPromise SmartCardConnection::disconnect(
    ScriptState* script_state,
    const V8SmartCardDisposition& disposition,
    ExceptionState& exception_state) {
  if (!EnsureNoOperationInProgress(exception_state) ||
      !EnsureConnection(exception_state)) {
    return ScriptPromise();
  }

  ScriptPromiseResolver* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  operation_in_progress_ = true;
  connection_->Disconnect(
      ToMojomDisposition(disposition),
      WTF::BindOnce(&SmartCardConnection::OnDisconnectDone,
                    WrapPersistent(this), WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise SmartCardConnection::transmit(ScriptState* script_state,
                                            const DOMArrayPiece& send_buffer,
                                            ExceptionState& exception_state) {
  if (!EnsureNoOperationInProgress(exception_state) ||
      !EnsureConnection(exception_state)) {
    return ScriptPromise();
  }

  if (send_buffer.IsDetached() || send_buffer.IsNull()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid send buffer.");
    return ScriptPromise();
  }

  ScriptPromiseResolver* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  Vector<uint8_t> send_vector;
  send_vector.Append(send_buffer.Bytes(),
                     static_cast<wtf_size_t>(send_buffer.ByteLength()));

  operation_in_progress_ = true;
  connection_->Transmit(
      active_protocol_, send_vector,
      WTF::BindOnce(&SmartCardConnection::OnDataResult, WrapPersistent(this),
                    WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise SmartCardConnection::status() {
  NOTIMPLEMENTED();
  return ScriptPromise();
}

void SmartCardConnection::Trace(Visitor* visitor) const {
  visitor->Trace(connection_);
  ScriptWrappable::Trace(visitor);
}

bool SmartCardConnection::EnsureNoOperationInProgress(
    ExceptionState& exception_state) const {
  if (operation_in_progress_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kOperationInProgress);
    return false;
  }
  return true;
}

bool SmartCardConnection::EnsureConnection(
    ExceptionState& exception_state) const {
  if (!connection_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kDisconnected);
    return false;
  }
  return true;
}

void SmartCardConnection::OnDisconnectDone(
    ScriptPromiseResolver* resolver,
    device::mojom::blink::SmartCardResultPtr result) {
  CHECK(operation_in_progress_);
  operation_in_progress_ = false;

  if (result->is_error()) {
    auto* error = SmartCardError::Create(result->get_error());
    resolver->Reject(error);
    return;
  }

  CHECK(connection_.is_bound());
  connection_.reset();

  resolver->Resolve();
}

void SmartCardConnection::OnDataResult(
    ScriptPromiseResolver* resolver,
    device::mojom::blink::SmartCardDataResultPtr result) {
  CHECK(operation_in_progress_);
  operation_in_progress_ = false;

  if (result->is_error()) {
    auto* error = SmartCardError::Create(result->get_error());
    resolver->Reject(error);
    return;
  }

  const Vector<uint8_t>& data = result->get_data();

  resolver->Resolve(DOMArrayBuffer::Create(data.data(), data.size()));
}

}  // namespace blink
