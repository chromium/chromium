// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_tensor.h"

#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "services/webnn/public/cpp/ml_tensor_usage.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_buffer.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_tensor_descriptor.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_error.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"

namespace blink {

MLTensor::MLTensor(
    ExecutionContext* execution_context,
    MLContext* context,
    webnn::OperandDescriptor descriptor,
    webnn::MLTensorUsage usage,
    webnn::mojom::blink::CreateBufferSuccessPtr create_buffer_success,
    base::PassKey<MLContext> /*pass_key*/)
    : ml_context_(context),
      descriptor_(std::move(descriptor)),
      usage_(usage),
      webnn_handle_(std::move(create_buffer_success->buffer_handle)),
      remote_buffer_(execution_context) {
  remote_buffer_.Bind(
      std::move(create_buffer_success->buffer_remote),
      execution_context->GetTaskRunner(TaskType::kMachineLearning));
  remote_buffer_.set_disconnect_handler(
      WTF::BindOnce(&MLTensor::OnConnectionError, WrapWeakPersistent(this)));
}

MLTensor::~MLTensor() = default;

void MLTensor::Trace(Visitor* visitor) const {
  visitor->Trace(ml_context_);
  visitor->Trace(remote_buffer_);
  visitor->Trace(pending_resolvers_);
  visitor->Trace(pending_byob_resolvers_);
  ScriptWrappable::Trace(visitor);
}

V8MLOperandDataType MLTensor::dataType() const {
  return ToBlinkDataType(descriptor_.data_type());
}

Vector<uint32_t> MLTensor::shape() const {
  return Vector<uint32_t>(descriptor_.shape());
}

uint32_t MLTensor::usage() const {
  return static_cast<uint32_t>(usage_.ToEnumBitmask());
}

void MLTensor::destroy() {
  // Calling OnConnectionError() will disconnect and destroy the buffer in
  // the service. The remote buffer must remain unbound after calling
  // OnConnectionError() because it is valid to call destroy() multiple times.
  OnConnectionError();
}

const webnn::OperandDescriptor& MLTensor::Descriptor() const {
  return descriptor_;
}

webnn::OperandDataType MLTensor::DataType() const {
  return descriptor_.data_type();
}

const std::vector<uint32_t>& MLTensor::Shape() const {
  return descriptor_.shape();
}

const webnn::MLTensorUsage& MLTensor::Usage() const {
  return usage_;
}

uint64_t MLTensor::PackedByteLength() const {
  return descriptor_.PackedByteLength();
}

ScriptPromise<DOMArrayBuffer> MLTensor::ReadBufferImpl(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  // Remote context gets automatically unbound when the execution context
  // destructs.
  if (!remote_buffer_.is_bound()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Buffer has been destroyed or context is lost.");
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<DOMArrayBuffer>>(
      script_state, exception_state.GetContext());
  pending_resolvers_.insert(resolver);

  remote_buffer_->ReadBuffer(WTF::BindOnce(&MLTensor::OnDidReadBuffer,
                                           WrapPersistent(this),
                                           WrapPersistent(resolver)));

  return resolver->Promise();
}

ScriptPromise<IDLUndefined> MLTensor::ReadBufferImpl(
    ScriptState* script_state,
    DOMArrayBufferBase* dst_data,
    ExceptionState& exception_state) {
  // Remote context gets automatically unbound when the execution context
  // destructs.
  if (!remote_buffer_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid buffer state");
    return EmptyPromise();
  }

  if (dst_data->ByteLength() < PackedByteLength()) {
    exception_state.ThrowTypeError("The destination buffer is too small.");
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  pending_byob_resolvers_.insert(resolver);

  remote_buffer_->ReadBuffer(
      WTF::BindOnce(&MLTensor::OnDidReadBufferByob, WrapPersistent(this),
                    WrapPersistent(resolver), WrapPersistent(dst_data)));
  return resolver->Promise();
}

ScriptPromise<IDLUndefined> MLTensor::ReadBufferImpl(
    ScriptState* script_state,
    DOMArrayBufferView* dst_data,
    ExceptionState& exception_state) {
  // Remote context gets automatically unbound when the execution context
  // destructs.
  if (!remote_buffer_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid buffer state");
    return EmptyPromise();
  }

  if (dst_data->byteLength() < PackedByteLength()) {
    exception_state.ThrowTypeError("The destination buffer is too small.");
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  pending_byob_resolvers_.insert(resolver);

  remote_buffer_->ReadBuffer(
      WTF::BindOnce(&MLTensor::OnDidReadBufferByobView, WrapPersistent(this),
                    WrapPersistent(resolver), WrapPersistent(dst_data)));
  return resolver->Promise();
}

void MLTensor::OnDidReadBuffer(
    ScriptPromiseResolver<DOMArrayBuffer>* resolver,
    webnn::mojom::blink::ReadBufferResultPtr result) {
  pending_resolvers_.erase(resolver);

  if (result->is_error()) {
    const webnn::mojom::blink::Error& read_buffer_error = *result->get_error();
    resolver->RejectWithDOMException(
        WebNNErrorCodeToDOMExceptionCode(read_buffer_error.code),
        read_buffer_error.message);
    return;
  }
  resolver->Resolve(DOMArrayBuffer::Create(result->get_buffer()));
}

void MLTensor::OnDidReadBufferByob(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    DOMArrayBufferBase* dst_data,
    webnn::mojom::blink::ReadBufferResultPtr result) {
  pending_byob_resolvers_.erase(resolver);

  if (result->is_error()) {
    const webnn::mojom::blink::Error& read_buffer_error = *result->get_error();
    resolver->RejectWithDOMException(
        WebNNErrorCodeToDOMExceptionCode(read_buffer_error.code),
        read_buffer_error.message);
    return;
  }

  if (dst_data->IsDetached()) {
    resolver->RejectWithTypeError("Buffer was detached.");
    return;
  }

  // It is safe to write into `dst_data` even though it was not transferred
  // because this method is called in a task which runs on same thread where
  // script executes, so script can't observe a partially written state (unless
  // `dst_data` is a SharedArrayBuffer).
  dst_data->ByteSpan().copy_prefix_from(result->get_buffer());
  resolver->Resolve();
}

void MLTensor::OnDidReadBufferByobView(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    DOMArrayBufferView* dst_data,
    webnn::mojom::blink::ReadBufferResultPtr result) {
  pending_byob_resolvers_.erase(resolver);

  if (result->is_error()) {
    const webnn::mojom::blink::Error& read_buffer_error = *result->get_error();
    resolver->RejectWithDOMException(
        WebNNErrorCodeToDOMExceptionCode(read_buffer_error.code),
        read_buffer_error.message);
    return;
  }

  if (dst_data->IsDetached()) {
    resolver->RejectWithTypeError("Buffer was detached.");
    return;
  }

  // It is safe to write into `dst_data` even though it was not transferred
  // because this method is called in a task which runs on same thread where
  // script executes, so script can't observe a partially written state (unless
  // `dst_data` is a SharedArrayBuffer).
  dst_data->ByteSpan().copy_prefix_from(result->get_buffer());
  resolver->Resolve();
}

void MLTensor::WriteBufferImpl(base::span<const uint8_t> src_data,
                               ExceptionState& exception_state) {
  // Remote context gets automatically unbound when the execution context
  // destructs.
  if (!remote_buffer_.is_bound()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Buffer has been destroyed or context is lost.");
    return;
  }

  // Return early since empty written data can be ignored with no observable
  // effect.
  if (src_data.size() == 0) {
    return;
  }

  // Copy src data.
  remote_buffer_->WriteBuffer(src_data);
}

void MLTensor::OnConnectionError() {
  remote_buffer_.reset();

  for (const auto& resolver : pending_resolvers_) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Buffer has been destroyed or context is lost.");
  }
  pending_resolvers_.clear();

  for (const auto& resolver : pending_byob_resolvers_) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Buffer has been destroyed or context is lost.");
  }
  pending_byob_resolvers_.clear();
}

}  // namespace blink
