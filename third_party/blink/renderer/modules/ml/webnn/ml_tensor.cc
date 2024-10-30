// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_tensor.h"

#include "base/metrics/histogram_functions.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "services/webnn/public/cpp/ml_tensor_usage.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_tensor_descriptor.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_error.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"

namespace blink {

namespace {

void RecordReadTensorTime(base::ElapsedTimer read_tensor_timer) {
  base::UmaHistogramMediumTimes("WebNN.MLTensor.TimingMs.Read",
                                read_tensor_timer.Elapsed());
}

}  // namespace

MLTensor::MLTensor(
    ExecutionContext* execution_context,
    MLContext* context,
    webnn::OperandDescriptor descriptor,
    webnn::MLTensorUsage usage,
    webnn::mojom::blink::CreateTensorSuccessPtr create_tensor_success,
    base::PassKey<MLContext> /*pass_key*/)
    : ml_context_(context),
      descriptor_(std::move(descriptor)),
      usage_(usage),
      webnn_handle_(std::move(create_tensor_success->tensor_handle)),
      remote_tensor_(execution_context) {
  remote_tensor_.Bind(
      std::move(create_tensor_success->tensor_remote),
      execution_context->GetTaskRunner(TaskType::kMachineLearning));
  remote_tensor_.set_disconnect_handler(
      WTF::BindOnce(&MLTensor::OnConnectionError, WrapWeakPersistent(this)));
}

MLTensor::~MLTensor() = default;

void MLTensor::Trace(Visitor* visitor) const {
  visitor->Trace(ml_context_);
  visitor->Trace(remote_tensor_);
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

bool MLTensor::importableToWebGPU() const {
  return usage_.Has(webnn::MLTensorUsageFlags::kWebGpuInterop);
}

bool MLTensor::readable() const {
  return usage_.Has(webnn::MLTensorUsageFlags::kRead);
}

bool MLTensor::writable() const {
  return usage_.Has(webnn::MLTensorUsageFlags::kWrite);
}

uint32_t MLTensor::usage() const {
  return static_cast<uint32_t>(usage_.ToEnumBitmask());
}

void MLTensor::destroy() {
  // Calling OnConnectionError() will disconnect and destroy the tensor in
  // the service. The remote tensor must remain unbound after calling
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

ScriptPromise<DOMArrayBuffer> MLTensor::ReadTensorImpl(
    ScopedMLTrace scoped_trace,
    ScriptState* script_state,
    ExceptionState& exception_state) {
  // Remote context gets automatically unbound when the execution context
  // destructs.
  if (!remote_tensor_.is_bound()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Tensor has been destroyed or context is lost.");
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<DOMArrayBuffer>>(
      script_state, exception_state.GetContext());
  pending_resolvers_.insert(resolver);

  base::ElapsedTimer read_tensor_timer;
  remote_tensor_->ReadTensor(WTF::BindOnce(
      &MLTensor::OnDidReadTensor, WrapPersistent(this), std::move(scoped_trace),
      WrapPersistent(resolver), std::move(read_tensor_timer)));

  return resolver->Promise();
}

ScriptPromise<IDLUndefined> MLTensor::ReadTensorImpl(
    ScopedMLTrace scoped_trace,
    ScriptState* script_state,
    DOMArrayBufferBase* dst_data,
    ExceptionState& exception_state) {
  // Remote context gets automatically unbound when the execution context
  // destructs.
  if (!remote_tensor_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid tensor state");
    return EmptyPromise();
  }

  if (dst_data->ByteLength() < PackedByteLength()) {
    exception_state.ThrowTypeError("The destination tensor is too small.");
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  pending_byob_resolvers_.insert(resolver);

  base::ElapsedTimer read_tensor_timer;
  remote_tensor_->ReadTensor(
      WTF::BindOnce(&MLTensor::OnDidReadTensorByob, WrapPersistent(this),
                    std::move(scoped_trace), WrapPersistent(resolver),
                    WrapPersistent(dst_data), std::move(read_tensor_timer)));
  return resolver->Promise();
}

ScriptPromise<IDLUndefined> MLTensor::ReadTensorImpl(
    ScopedMLTrace scoped_trace,
    ScriptState* script_state,
    DOMArrayBufferView* dst_data,
    ExceptionState& exception_state) {
  // Remote context gets automatically unbound when the execution context
  // destructs.
  if (!remote_tensor_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid tensor state");
    return EmptyPromise();
  }

  if (dst_data->byteLength() < PackedByteLength()) {
    exception_state.ThrowTypeError("The destination tensor is too small.");
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  pending_byob_resolvers_.insert(resolver);

  base::ElapsedTimer read_tensor_timer;
  remote_tensor_->ReadTensor(
      WTF::BindOnce(&MLTensor::OnDidReadTensorByobView, WrapPersistent(this),
                    std::move(scoped_trace), WrapPersistent(resolver),
                    WrapPersistent(dst_data), std::move(read_tensor_timer)));
  return resolver->Promise();
}

void MLTensor::OnDidReadTensor(
    ScopedMLTrace scoped_trace,
    ScriptPromiseResolver<DOMArrayBuffer>* resolver,
    base::ElapsedTimer read_tensor_timer,
    webnn::mojom::blink::ReadTensorResultPtr result) {
  pending_resolvers_.erase(resolver);

  if (result->is_error()) {
    const webnn::mojom::blink::Error& read_tensor_error = *result->get_error();
    resolver->RejectWithDOMException(
        WebNNErrorCodeToDOMExceptionCode(read_tensor_error.code),
        read_tensor_error.message);
    return;
  }
  resolver->Resolve(DOMArrayBuffer::Create(result->get_buffer()));

  RecordReadTensorTime(std::move(read_tensor_timer));
}

void MLTensor::OnDidReadTensorByob(
    ScopedMLTrace scoped_trace,
    ScriptPromiseResolver<IDLUndefined>* resolver,
    DOMArrayBufferBase* dst_data,
    base::ElapsedTimer read_tensor_timer,
    webnn::mojom::blink::ReadTensorResultPtr result) {
  pending_byob_resolvers_.erase(resolver);

  if (result->is_error()) {
    const webnn::mojom::blink::Error& read_tensor_error = *result->get_error();
    resolver->RejectWithDOMException(
        WebNNErrorCodeToDOMExceptionCode(read_tensor_error.code),
        read_tensor_error.message);
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
  dst_data->ByteSpanMaybeShared().copy_prefix_from(result->get_buffer());
  resolver->Resolve();

  RecordReadTensorTime(std::move(read_tensor_timer));
}

void MLTensor::OnDidReadTensorByobView(
    ScopedMLTrace scoped_trace,
    ScriptPromiseResolver<IDLUndefined>* resolver,
    DOMArrayBufferView* dst_data,
    base::ElapsedTimer read_tensor_timer,
    webnn::mojom::blink::ReadTensorResultPtr result) {
  pending_byob_resolvers_.erase(resolver);

  if (result->is_error()) {
    const webnn::mojom::blink::Error& read_tensor_error = *result->get_error();
    resolver->RejectWithDOMException(
        WebNNErrorCodeToDOMExceptionCode(read_tensor_error.code),
        read_tensor_error.message);
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
  dst_data->ByteSpanMaybeShared().copy_prefix_from(result->get_buffer());
  resolver->Resolve();

  RecordReadTensorTime(std::move(read_tensor_timer));
}

void MLTensor::WriteTensorImpl(base::span<const uint8_t> src_data,
                               ExceptionState& exception_state) {
  // Remote context gets automatically unbound when the execution context
  // destructs.
  if (!remote_tensor_.is_bound()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Tensor has been destroyed or context is lost.");
    return;
  }

  // Return early since empty written data can be ignored with no observable
  // effect.
  if (src_data.size() == 0) {
    return;
  }

  // Copy src data.
  remote_tensor_->WriteTensor(src_data);
}

void MLTensor::OnConnectionError() {
  remote_tensor_.reset();

  for (const auto& resolver : pending_resolvers_) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Tensor has been destroyed or context is lost.");
  }
  pending_resolvers_.clear();

  for (const auto& resolver : pending_byob_resolvers_) {
    resolver->RejectWithDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Tensor has been destroyed or context is lost.");
  }
  pending_byob_resolvers_.clear();
}

}  // namespace blink
