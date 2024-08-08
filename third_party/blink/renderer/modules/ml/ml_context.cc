// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml_context.h"

#include "base/feature_list.h"
#include "base/numerics/checked_math.h"
#include "base/types/expected_macros.h"
#include "base/types/pass_key.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/cpp/webnn_errors.h"
#include "services/webnn/public/mojom/features.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_buffer.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_arg_min_max_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_buffer_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_concat_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_lost_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_device_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gather_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_op_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_data_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_power_preference.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_single_input_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_where_support_limits.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/modules/ml/ml_trace.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_buffer.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_error.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
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
    const V8MLDeviceType device_type,
    const V8MLPowerPreference power_preference,
    const unsigned int num_threads,
    webnn::mojom::blink::CreateContextSuccessPtr create_context_success)
    : device_type_(device_type),
      power_preference_(power_preference),
      num_threads_(num_threads),
      lost_property_(MakeGarbageCollected<LostProperty>(execution_context)),
      context_remote_(execution_context),
      properties_(std::move(create_context_success->context_properties)),
      webnn_handle_(std::move(create_context_success->context_handle)) {
  context_remote_.Bind(
      std::move(create_context_success->context_remote),
      execution_context->GetTaskRunner(TaskType::kMachineLearning));
  context_remote_.set_disconnect_with_reason_handler(
      WTF::BindOnce(&MLContext::OnLost, WrapWeakPersistent(this)));
}

MLContext::~MLContext() = default;

V8MLDeviceType MLContext::GetDeviceType() const {
  return device_type_;
}

V8MLPowerPreference MLContext::GetPowerPreference() const {
  return power_preference_;
}

unsigned int MLContext::GetNumThreads() const {
  return num_threads_;
}

void MLContext::Trace(Visitor* visitor) const {
  visitor->Trace(lost_property_);
  visitor->Trace(context_remote_);
  visitor->Trace(pending_resolvers_);
  visitor->Trace(graphs_);
  visitor->Trace(graph_builders_);
  visitor->Trace(buffers_);
  ScriptWrappable::Trace(visitor);
}

ScriptPromise<MLContextLostInfo> MLContext::lost(ScriptState* script_state) {
  return lost_property_->Promise(script_state->World());
}

void MLContext::destroy(ScriptState* script_state,
                        ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "destroy() called on an invalid context.");
    return;
  }

  if (context_remote_.is_bound()) {
    OnLost(0, "destroy() called on MLContext.");

    for (const auto& graph : graphs_) {
      graph->OnConnectionError();
    }

    for (const auto& graph_builder : graph_builders_) {
      graph_builder->OnConnectionError();
    }

    for (const auto& buffer : buffers_) {
      buffer->destroy();
    }
  }
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

MLGraphBuilder* MLContext::CreateWebNNGraphBuilder(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!context_remote_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Context is lost.");
    return nullptr;
  }

  mojo::PendingAssociatedRemote<webnn::mojom::blink::WebNNGraphBuilder>
      pending_remote;
  context_remote_->CreateGraphBuilder(
      pending_remote.InitWithNewEndpointAndPassReceiver());

  auto* graph_builder = MakeGarbageCollected<MLGraphBuilder>(
      ExecutionContext::From(script_state), this, std::move(pending_remote));
  graph_builders_.insert(graph_builder);

  return graph_builder;
}

void MLContext::OnLost(uint32_t custom_reason, const std::string& description) {
  context_remote_.reset();

  auto* context_lost_info = MLContextLostInfo::Create();
  if (description.empty()) {
    context_lost_info->setMessage(
        "WebNN context is lost due to connection error.");
  } else {
    context_lost_info->setMessage(String::FromUTF8(description));
  }

  CHECK_EQ(lost_property_->GetState(), LostProperty::kPending);
  lost_property_->Resolve(context_lost_info);

  for (const auto& resolver : pending_resolvers_) {
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     "Context is lost.");
  }
  pending_resolvers_.clear();
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

  // Element-wise unary ops.
  MLSingleInputSupportLimits* abs = MLSingleInputSupportLimits::Create();
  abs->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.abs_input));
  abs->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.abs_input));
  op_support_limits->setAbs(abs);
  MLSingleInputSupportLimits* ceil = MLSingleInputSupportLimits::Create();
  ceil->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.ceil_input));
  ceil->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.ceil_input));
  op_support_limits->setCeil(ceil);
  MLSingleInputSupportLimits* cos = MLSingleInputSupportLimits::Create();
  cos->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.cos_input));
  cos->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.cos_input));
  op_support_limits->setCos(cos);
  MLSingleInputSupportLimits* erf = MLSingleInputSupportLimits::Create();
  erf->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.erf_input));
  erf->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.erf_input));
  op_support_limits->setErf(erf);
  MLSingleInputSupportLimits* exp = MLSingleInputSupportLimits::Create();
  exp->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.exp_input));
  exp->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.exp_input));
  op_support_limits->setExp(exp);
  MLSingleInputSupportLimits* floor = MLSingleInputSupportLimits::Create();
  floor->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.floor_input));
  floor->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.floor_input));
  op_support_limits->setFloor(floor);
  MLSingleInputSupportLimits* identity = MLSingleInputSupportLimits::Create();
  identity->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.identity_input));
  identity->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.identity_input));
  op_support_limits->setIdentity(identity);
  MLSingleInputSupportLimits* log = MLSingleInputSupportLimits::Create();
  log->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.log_input));
  log->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.log_input));
  op_support_limits->setLog(log);
  MLSingleInputSupportLimits* neg = MLSingleInputSupportLimits::Create();
  neg->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.neg_input));
  neg->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.neg_input));
  op_support_limits->setNeg(neg);
  MLSingleInputSupportLimits* reciprocal = MLSingleInputSupportLimits::Create();
  reciprocal->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.reciprocal_input));
  reciprocal->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.reciprocal_input));
  op_support_limits->setReciprocal(reciprocal);
  MLSingleInputSupportLimits* sin = MLSingleInputSupportLimits::Create();
  sin->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.sin_input));
  sin->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.sin_input));
  op_support_limits->setSin(sin);
  MLSingleInputSupportLimits* sqrt = MLSingleInputSupportLimits::Create();
  sqrt->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.sqrt_input));
  sqrt->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.sqrt_input));
  op_support_limits->setSqrt(sqrt);
  MLSingleInputSupportLimits* tan = MLSingleInputSupportLimits::Create();
  tan->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.tan_input));
  tan->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.tan_input));
  op_support_limits->setTan(tan);

  MLSingleInputSupportLimits* elu = MLSingleInputSupportLimits::Create();
  elu->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.elu_input));
  elu->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.elu_input));
  op_support_limits->setElu(elu);

  MLGatherSupportLimits* gather = MLGatherSupportLimits::Create();
  gather->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.gather_input));
  gather->setIndices(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.gather_indices));
  op_support_limits->setGather(gather);

  MLSingleInputSupportLimits* gelu = MLSingleInputSupportLimits::Create();
  gelu->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.gelu_input));
  gelu->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.gelu_input));
  op_support_limits->setGelu(gelu);

  MLSingleInputSupportLimits* leaky_relu = MLSingleInputSupportLimits::Create();
  leaky_relu->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.leaky_relu_input));
  leaky_relu->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.leaky_relu_input));
  op_support_limits->setLeakyRelu(leaky_relu);

  MLSingleInputSupportLimits* relu = MLSingleInputSupportLimits::Create();
  relu->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.relu_input));
  relu->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.relu_input));
  op_support_limits->setRelu(relu);

  MLSingleInputSupportLimits* sigmoid = MLSingleInputSupportLimits::Create();
  sigmoid->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.sigmoid_input));
  sigmoid->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.sigmoid_input));
  op_support_limits->setSigmoid(sigmoid);

  MLSingleInputSupportLimits* slice = MLSingleInputSupportLimits::Create();
  slice->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.slice_input));
  slice->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.slice_input));
  op_support_limits->setSlice(slice);

  MLSingleInputSupportLimits* softmax = MLSingleInputSupportLimits::Create();
  softmax->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.softmax_input));
  softmax->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.softmax_input));
  op_support_limits->setSoftmax(softmax);

  MLSingleInputSupportLimits* softplus = MLSingleInputSupportLimits::Create();
  softplus->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.softplus_input));
  softplus->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.softplus_input));
  op_support_limits->setSoftplus(softplus);

  MLSingleInputSupportLimits* softsign = MLSingleInputSupportLimits::Create();
  softsign->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.softsign_input));
  softsign->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.softsign_input));
  op_support_limits->setSoftsign(softsign);

  MLSingleInputSupportLimits* split = MLSingleInputSupportLimits::Create();
  split->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.split_input));
  split->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.split_input));
  op_support_limits->setSplit(split);

  MLWhereSupportLimits* where = MLWhereSupportLimits::Create();
  where->setCondition(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.where_condition));
  where->setTrueValue(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.where_value));
  where->setFalseValue(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.where_value));
  op_support_limits->setWhere(where);

  return op_support_limits;
}

void MLContext::OnGraphCreated(MLGraph* graph) {
  graphs_.insert(graph);
}

ScriptPromise<MLBuffer> MLContext::createBuffer(
    ScriptState* script_state,
    const MLBufferDescriptor* descriptor,
    ExceptionState& exception_state) {
  ScopedMLTrace scoped_trace("MLContext::createBuffer");
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return EmptyPromise();
  }

  if (!base::FeatureList::IsEnabled(
          webnn::mojom::features::kWebMachineLearningNeuralNetwork)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Not implemented");
    return EmptyPromise();
  }

  if (!context_remote_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Context is lost.");
    return EmptyPromise();
  }

  ASSIGN_OR_RETURN(webnn::OperandDescriptor validated_descriptor,
                   webnn::OperandDescriptor::Create(
                       FromBlinkDataType(descriptor->dataType().AsEnum()),
                       descriptor->dimensions()),
                   [&exception_state](std::string error) {
                     exception_state.ThrowTypeError(String(error));
                     return ScriptPromise<MLBuffer>();
                   });

  RETURN_IF_ERROR(webnn::ValidateBuffer(properties_, validated_descriptor),
                  [&exception_state](std::string error) {
                    exception_state.ThrowTypeError(String(error));
                    return ScriptPromise<MLBuffer>();
                  });

  // TODO(crbug.com/343638938): Pass real buffer usages.
  auto buffer_info = webnn::mojom::blink::BufferInfo::New(
      validated_descriptor, webnn::MLBufferUsage());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<MLBuffer>>(
      script_state, exception_state.GetContext());
  pending_resolvers_.insert(resolver);

  // Use `WebNNContext` to create `WebNNBuffer` message pipe.
  context_remote_->CreateBuffer(
      std::move(buffer_info),
      WTF::BindOnce(&MLContext::DidCreateWebNNBuffer, WrapPersistent(this),
                    std::move(scoped_trace), WrapPersistent(resolver),
                    std::move(validated_descriptor)));

  return resolver->Promise();
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

  return src_buffer->ReadBufferImpl(script_state, exception_state);
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

void MLContext::DidCreateWebNNBuffer(
    ScopedMLTrace scoped_trace,
    ScriptPromiseResolver<blink::MLBuffer>* resolver,
    webnn::OperandDescriptor validated_descriptor,
    webnn::mojom::blink::CreateBufferResultPtr result) {
  pending_resolvers_.erase(resolver);

  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid()) {
    return;
  }

  if (result->is_error()) {
    const auto& create_buffer_error = result->get_error();
    resolver->RejectWithDOMException(
        WebNNErrorCodeToDOMExceptionCode(create_buffer_error->code),
        create_buffer_error->message);
    return;
  }

  auto* buffer = MakeGarbageCollected<MLBuffer>(
      resolver->GetExecutionContext(), this, std::move(validated_descriptor),
      std::move(result->get_success()), base::PassKey<MLContext>());
  buffers_.insert(buffer);

  resolver->Resolve(buffer);
}

}  // namespace blink
