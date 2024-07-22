// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml_context.h"

#include "base/numerics/checked_math.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/cpp/webnn_errors.h"
#include "services/webnn/public/mojom/webnn_buffer.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_arg_min_max_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_concat_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_lost_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_device_preference.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_device_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gather_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_model_format.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_op_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_data_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_power_preference.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_where_support_limits.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/ml/ml_trace.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_buffer.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

MLSupportLimits* SupportedDataTypesToSupportLimits(
    const webnn::SupportedDataTypes& supported_data_types) {
  MLSupportLimits* support_limits = MLSupportLimits::Create();
  Vector<String> data_types;
  for (auto data_type : supported_data_types) {
    data_types.push_back(webnn::DataTypeToString(data_type));
  }

  support_limits->setDataTypes(data_types);
  return support_limits;
}

}  // namespace

MLContext::MLContext(
    ExecutionContext* execution_context,
    const V8MLDevicePreference device_preference,
    const V8MLDeviceType device_type,
    const V8MLPowerPreference power_preference,
    const V8MLModelFormat model_format,
    const unsigned int num_threads,
    webnn::mojom::blink::CreateContextSuccessPtr create_context_success)
    : device_preference_(device_preference),
      device_type_(device_type),
      power_preference_(power_preference),
      model_format_(model_format),
      num_threads_(num_threads),
      lost_property_(MakeGarbageCollected<LostProperty>(execution_context)),
      context_remote_(execution_context),
      context_client_receiver_(this, execution_context),
      properties_(std::move(create_context_success->context_properties)),
      webnn_handle_(std::move(create_context_success->context_handle)) {
  context_remote_.Bind(
      std::move(create_context_success->context_remote),
      execution_context->GetTaskRunner(TaskType::kMachineLearning));
  context_client_receiver_.Bind(
      std::move(create_context_success->context_client_receiver),
      execution_context->GetTaskRunner(TaskType::kMachineLearning));
  context_client_receiver_.set_disconnect_handler(
      WTF::BindOnce(&MLContext::OnDisconnected, WrapWeakPersistent(this)));
}

MLContext::~MLContext() = default;

V8MLDevicePreference MLContext::GetDevicePreference() const {
  return device_preference_;
}

V8MLDeviceType MLContext::GetDeviceType() const {
  return device_type_;
}

V8MLPowerPreference MLContext::GetPowerPreference() const {
  return power_preference_;
}

V8MLModelFormat MLContext::GetModelFormat() const {
  return model_format_;
}

unsigned int MLContext::GetNumThreads() const {
  return num_threads_;
}

void MLContext::Trace(Visitor* visitor) const {
  visitor->Trace(lost_property_);
  visitor->Trace(context_remote_);
  visitor->Trace(context_client_receiver_);

  ScriptWrappable::Trace(visitor);
}

ScriptPromise<MLContextLostInfo> MLContext::lost(ScriptState* script_state) {
  return lost_property_->Promise(script_state->World());
}

ScriptPromise<MLComputeResult> MLContext::compute(
    ScriptState* script_state,
    MLGraph* graph,
    const MLNamedArrayBufferViews& inputs,
    const MLNamedArrayBufferViews& outputs,
    ExceptionState& exception_state) {
  ScopedMLTrace scoped_trace("MLContext::compute");
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return EmptyPromise();
  }

  if (graph->Context() != this) {
    exception_state.ThrowTypeError(
        "The graph isn't built within this context.");
    return EmptyPromise();
  }

  return graph->Compute(std::move(scoped_trace), inputs, outputs, script_state,
                        exception_state);
}

void MLContext::CreateWebNNGraph(
    webnn::mojom::blink::GraphInfoPtr graph_info,
    webnn::mojom::blink::WebNNContext::CreateGraphCallback callback) {
  if (!context_remote_.is_bound()) {
    std::move(callback).Run(webnn::mojom::blink::CreateGraphResult::NewError(
        webnn::mojom::blink::Error::New(
            webnn::mojom::blink::Error::Code::kUnknownError,
            "Context is lost.")));
    return;
  }

  context_remote_->CreateGraph(std::move(graph_info),
                               WTF::BindOnce(std::move(callback)));
}

void MLContext::OnLost(const String& message) {
  context_remote_.reset();
  context_client_receiver_.reset();

  CHECK_EQ(lost_property_->GetState(), LostProperty::kPending);
  auto* context_lost_info = MLContextLostInfo::Create();
  context_lost_info->setMessage(message);
  lost_property_->Resolve(context_lost_info);
}

void MLContext::OnDisconnected() {
  OnLost("WebNN context is lost due to connection error.");
}

void MLContext::CreateWebNNBuffer(
    mojo::PendingAssociatedReceiver<webnn::mojom::blink::WebNNBuffer> receiver,
    webnn::mojom::blink::BufferInfoPtr buffer_info,
    const base::UnguessableToken& buffer_handle) {
  CHECK(context_remote_.is_bound());
  // Use `WebNNContext` to create `WebNNBuffer` message pipe.
  context_remote_->CreateBuffer(std::move(receiver), std::move(buffer_info),
                                buffer_handle);
}

const MLOpSupportLimits* MLContext::opSupportLimits(ScriptState* script_state) {
  MLOpSupportLimits* op_support_limits = MLOpSupportLimits::Create();
  op_support_limits->setInput(
      SupportedDataTypesToSupportLimits(properties_.data_type_limits.input));
  op_support_limits->setConstant(
      SupportedDataTypesToSupportLimits(properties_.data_type_limits.constant));
  op_support_limits->setOutput(
      SupportedDataTypesToSupportLimits(properties_.data_type_limits.output()));

  MLArgMinMaxSupportLimits* argmin = MLArgMinMaxSupportLimits::Create();
  argmin->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.arg_min_max_input));
  argmin->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.arg_min_max_output));
  op_support_limits->setArgMin(argmin);
  MLArgMinMaxSupportLimits* argmax = MLArgMinMaxSupportLimits::Create();
  argmax->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.arg_min_max_input));
  argmax->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.arg_min_max_output));
  op_support_limits->setArgMax(argmax);

  MLConcatSupportLimits* concat = MLConcatSupportLimits::Create();
  concat->setInputs(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.concat_inputs));
  op_support_limits->setConcat(concat);

  MLGatherSupportLimits* gather = MLGatherSupportLimits::Create();
  gather->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.gather_input));
  gather->setIndices(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.gather_indices));
  op_support_limits->setGather(gather);

  MLWhereSupportLimits* where = MLWhereSupportLimits::Create();
  where->setCondition(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.where_condition));
  where->setTrueValue(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.where_true_value));
  where->setFalseValue(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.where_false_value));
  op_support_limits->setWhere(where);

  return op_support_limits;
}

MLBuffer* MLContext::createBuffer(ScriptState* script_state,
                                  const MLBufferDescriptor* descriptor,
                                  ExceptionState& exception_state) {
  ScopedMLTrace scoped_trace("MLContext::createBuffer");
  // Remote context gets automatically unbound when the execution context
  // destructs.
  if (!context_remote_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Context is lost.");
    return nullptr;
  }
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return nullptr;
  }

  return MLBuffer::Create(std::move(scoped_trace),
                          ExecutionContext::From(script_state), this,
                          descriptor, exception_state);
}

void MLContext::writeBuffer(
    ScriptState* script_state,
    MLBuffer* dst_buffer,
    const MaybeShared<DOMArrayBufferView>& src_data_view,
    uint64_t src_element_offset,
    ExceptionState& exception_state) {
  WriteWebNNBuffer(script_state, dst_buffer,
                   src_data_view->ByteSpanMaybeShared(), src_element_offset,
                   src_data_view->TypeSize(),
                   /*src_element_count=*/std::nullopt, exception_state);
}

void MLContext::writeBuffer(
    ScriptState* script_state,
    MLBuffer* dst_buffer,
    const MaybeShared<DOMArrayBufferView>& src_data_view,
    uint64_t src_element_offset,
    uint64_t src_element_count,
    ExceptionState& exception_state) {
  WriteWebNNBuffer(script_state, dst_buffer,
                   src_data_view->ByteSpanMaybeShared(), src_element_offset,
                   src_data_view->TypeSize(), src_element_count,
                   exception_state);
}

void MLContext::writeBuffer(ScriptState* script_state,
                            MLBuffer* dst_buffer,
                            const DOMArrayBufferBase* src_data_base,
                            uint64_t src_byte_offset,
                            ExceptionState& exception_state) {
  WriteWebNNBuffer(script_state, dst_buffer,
                   src_data_base->ByteSpanMaybeShared(), src_byte_offset,
                   /*src_data_type_size_bytes=*/1,
                   /*src_element_count=*/std::nullopt, exception_state);
}

void MLContext::writeBuffer(ScriptState* script_state,
                            MLBuffer* dst_buffer,
                            const DOMArrayBufferBase* src_data_base,
                            uint64_t src_byte_offset,
                            uint64_t src_byte_size,
                            ExceptionState& exception_state) {
  WriteWebNNBuffer(script_state, dst_buffer,
                   src_data_base->ByteSpanMaybeShared(), src_byte_offset,
                   /*src_data_type_size_bytes=*/1,
                   /*src_element_count=*/src_byte_size, exception_state);
}

ScriptPromise<DOMArrayBuffer> MLContext::readBuffer(
    ScriptState* script_state,
    MLBuffer* src_buffer,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return EmptyPromise();
  }

  if (src_buffer->context() != this) {
    exception_state.ThrowTypeError(
        "The source buffer wasn't created with this context.");
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<DOMArrayBuffer>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  src_buffer->ReadBufferImpl(resolver);
  return promise;
}

void MLContext::WriteWebNNBuffer(ScriptState* script_state,
                                 MLBuffer* dst_buffer,
                                 base::span<const uint8_t> src_data,
                                 uint64_t src_element_offset,
                                 unsigned src_data_type_size_bytes,
                                 std::optional<uint64_t> src_element_count,
                                 ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return;
  }

  if (dst_buffer->context() != this) {
    exception_state.ThrowTypeError(
        "The destination buffer wasn't created with this context.");
    return;
  }

  const size_t src_data_byte_length = src_data.size();
  if (src_element_offset > src_data_byte_length / src_data_type_size_bytes) {
    exception_state.ThrowTypeError(
        "Data offset is too large: srcOffset exceeded byte length of srcData.");
    return;
  }

  uint64_t src_byte_offset;
  if (!base::CheckMul(src_element_offset, src_data_type_size_bytes)
           .AssignIfValid(&src_byte_offset)) {
    exception_state.ThrowTypeError(
        "Data offset is too large: srcOffset will overflow.");
    return;
  }

  uint64_t max_write_size_bytes;
  if (!base::CheckSub(src_data_byte_length, src_byte_offset)
           .AssignIfValid(&max_write_size_bytes)) {
    exception_state.ThrowTypeError(
        "Number of bytes to write is too large: offset exceeds byte length.");
    return;
  }

  uint64_t write_byte_size = max_write_size_bytes;
  if (src_element_count.has_value()) {
    if (src_element_count.value() >
        max_write_size_bytes / src_data_type_size_bytes) {
      exception_state.ThrowTypeError(
          "Number of bytes to write is too large: number of elements will "
          "overflow.");
      return;
    }

    write_byte_size = src_element_count.value() * src_data_type_size_bytes;
  }

  if (write_byte_size > dst_buffer->PackedByteLength()) {
    exception_state.ThrowTypeError(
        "Number of bytes to write is too large: write size exceeded buffer "
        "size.");
    return;
  }

  // Write size and offset needs to be cast to size_t.
  base::CheckedNumeric<size_t> checked_write_byte_size(write_byte_size);
  if (!checked_write_byte_size.IsValid()) {
    exception_state.ThrowRangeError("Number of bytes to write is too large");
    return;
  }

  base::CheckedNumeric<size_t> checked_src_byte_offset(src_byte_offset);
  if (!checked_src_byte_offset.IsValid()) {
    exception_state.ThrowRangeError("Offset to write is too large");
    return;
  }

  dst_buffer->WriteBufferImpl(
      src_data.subspan(checked_src_byte_offset.ValueOrDie(),
                       checked_write_byte_size.ValueOrDie()),
      exception_state);
}

void MLContext::dispatch(ScriptState* script_state,
                         MLGraph* graph,
                         const MLNamedBuffers& inputs,
                         const MLNamedBuffers& outputs,
                         ExceptionState& exception_state) {
  ScopedMLTrace scoped_trace("MLContext::dispatch");
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return;
  }

  if (graph->Context() != this) {
    exception_state.ThrowTypeError(
        "The graph isn't built within this context.");
    return;
  }

  return graph->Dispatch(std::move(scoped_trace), inputs, outputs,
                         exception_state);
}

}  // namespace blink
