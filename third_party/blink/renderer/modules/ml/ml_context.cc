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
#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_batch_normalization_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_binary_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_concat_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_lost_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_device_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gather_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gemm_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gru_cell_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gru_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_logical_not_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_lstm_cell_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_lstm_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_normalization_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_op_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_data_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_power_preference.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_prelu_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_quantize_dequantize_linear_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_scatter_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_single_input_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_tensor_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_where_support_limits.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/modules/ml/ml_trace.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_error.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_tensor.h"
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

blink::V8MLInputOperandLayout::Enum InputOperandLayoutToBlink(
    webnn::InputOperandLayout layout) {
  switch (layout) {
    case webnn::InputOperandLayout::kNchw:
      return blink::V8MLInputOperandLayout::Enum::kNchw;
    case webnn::InputOperandLayout::kNhwc:
      return blink::V8MLInputOperandLayout::Enum::kNhwc;
  }
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
      graph->destroy();
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

  LogConsoleWarning(script_state,
                    "WARNING: MLContext.compute() is deprecated. Use "
                    "MLContext.dispatch() instead.",
                    mojom::blink::ConsoleMessageSource::kDeprecation);

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
  const webnn::DataTypeLimits& data_type_limits = properties_.data_type_limits;

  MLOpSupportLimits* op_support_limits = MLOpSupportLimits::Create();
  op_support_limits->setPreferredInputLayout(
      InputOperandLayoutToBlink(properties_.input_operand_layout));
  op_support_limits->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.input));
  op_support_limits->setConstant(
      SupportedDataTypesToSupportLimits(data_type_limits.constant));
  op_support_limits->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.output()));

  MLSingleInputSupportLimits* argmin = MLSingleInputSupportLimits::Create();
  argmin->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.arg_min_max_input));
  argmin->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.arg_min_max_output));
  op_support_limits->setArgMin(argmin);
  MLSingleInputSupportLimits* argmax = MLSingleInputSupportLimits::Create();
  argmax->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.arg_min_max_input));
  argmax->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.arg_min_max_output));
  op_support_limits->setArgMax(argmax);

  MLBatchNormalizationSupportLimits* batch_normalization =
      MLBatchNormalizationSupportLimits::Create();
  batch_normalization->setInput(SupportedDataTypesToSupportLimits(
      data_type_limits.batch_normalization_input));
  batch_normalization->setMean(SupportedDataTypesToSupportLimits(
      data_type_limits.batch_normalization_input));
  batch_normalization->setVariance(SupportedDataTypesToSupportLimits(
      data_type_limits.batch_normalization_input));
  batch_normalization->setScale(SupportedDataTypesToSupportLimits(
      data_type_limits.batch_normalization_input));
  batch_normalization->setBias(SupportedDataTypesToSupportLimits(
      data_type_limits.batch_normalization_input));
  batch_normalization->setOutput(SupportedDataTypesToSupportLimits(
      data_type_limits.batch_normalization_input));
  op_support_limits->setBatchNormalization(batch_normalization);

  MLSingleInputSupportLimits* cast = MLSingleInputSupportLimits::Create();
  cast->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.cast_input));
  cast->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.cast_input));
  op_support_limits->setCast(cast);

  MLSingleInputSupportLimits* clamp = MLSingleInputSupportLimits::Create();
  clamp->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.clamp_input));
  clamp->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.clamp_input));
  op_support_limits->setClamp(clamp);

  MLConcatSupportLimits* concat = MLConcatSupportLimits::Create();
  concat->setInputs(
      SupportedDataTypesToSupportLimits(data_type_limits.concat_inputs));
  concat->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.concat_inputs));
  op_support_limits->setConcat(concat);

  MLConv2dSupportLimits* conv2d = MLConv2dSupportLimits::Create();
  conv2d->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.conv2d_input));
  conv2d->setFilter(
      SupportedDataTypesToSupportLimits(data_type_limits.conv2d_input));
  conv2d->setBias(
      SupportedDataTypesToSupportLimits(data_type_limits.conv2d_input));
  conv2d->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.conv2d_input));
  op_support_limits->setConv2d(conv2d);

  MLConv2dSupportLimits* conv_transpose2d = MLConv2dSupportLimits::Create();
  conv_transpose2d->setInput(SupportedDataTypesToSupportLimits(
      data_type_limits.conv_transpose2d_input));
  conv_transpose2d->setFilter(SupportedDataTypesToSupportLimits(
      data_type_limits.conv_transpose2d_input));
  conv_transpose2d->setBias(SupportedDataTypesToSupportLimits(
      data_type_limits.conv_transpose2d_input));
  conv_transpose2d->setOutput(SupportedDataTypesToSupportLimits(
      data_type_limits.conv_transpose2d_input));
  op_support_limits->setConvTranspose2d(conv_transpose2d);

  MLSingleInputSupportLimits* cumulative_sum =
      MLSingleInputSupportLimits::Create();
  cumulative_sum->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.cumulative_sum_input));
  cumulative_sum->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.cumulative_sum_input));
  op_support_limits->setCumulativeSum(cumulative_sum);

  MLQuantizeDequantizeLinearSupportLimits* dequantize_linear =
      MLQuantizeDequantizeLinearSupportLimits::Create();
  dequantize_linear->setInput(SupportedDataTypesToSupportLimits(
      data_type_limits.dequantize_linear_input));
  dequantize_linear->setScale(SupportedDataTypesToSupportLimits(
      data_type_limits.dequantize_linear_scale));
  dequantize_linear->setZeroPoint(SupportedDataTypesToSupportLimits(
      data_type_limits.dequantize_linear_input));
  dequantize_linear->setOutput(SupportedDataTypesToSupportLimits(
      data_type_limits.dequantize_linear_scale));
  op_support_limits->setDequantizeLinear(dequantize_linear);

  // Element-wise binary ops.
  MLBinarySupportLimits* add = MLBinarySupportLimits::Create();
  add->setA(SupportedDataTypesToSupportLimits(data_type_limits.add_input));
  add->setB(SupportedDataTypesToSupportLimits(data_type_limits.add_input));
  add->setOutput(SupportedDataTypesToSupportLimits(data_type_limits.add_input));
  op_support_limits->setAdd(add);
  MLBinarySupportLimits* sub = MLBinarySupportLimits::Create();
  sub->setA(SupportedDataTypesToSupportLimits(data_type_limits.sub_input));
  sub->setB(SupportedDataTypesToSupportLimits(data_type_limits.sub_input));
  sub->setOutput(SupportedDataTypesToSupportLimits(data_type_limits.sub_input));
  op_support_limits->setSub(sub);
  MLBinarySupportLimits* mul = MLBinarySupportLimits::Create();
  mul->setA(SupportedDataTypesToSupportLimits(data_type_limits.mul_input));
  mul->setB(SupportedDataTypesToSupportLimits(data_type_limits.mul_input));
  mul->setOutput(SupportedDataTypesToSupportLimits(data_type_limits.mul_input));
  op_support_limits->setMul(mul);
  MLBinarySupportLimits* div = MLBinarySupportLimits::Create();
  div->setA(SupportedDataTypesToSupportLimits(data_type_limits.div_input));
  div->setB(SupportedDataTypesToSupportLimits(data_type_limits.div_input));
  div->setOutput(SupportedDataTypesToSupportLimits(data_type_limits.div_input));
  op_support_limits->setDiv(div);
  MLBinarySupportLimits* max = MLBinarySupportLimits::Create();
  max->setA(SupportedDataTypesToSupportLimits(data_type_limits.max_input));
  max->setB(SupportedDataTypesToSupportLimits(data_type_limits.max_input));
  max->setOutput(SupportedDataTypesToSupportLimits(data_type_limits.max_input));
  op_support_limits->setMax(max);
  MLBinarySupportLimits* min = MLBinarySupportLimits::Create();
  min->setA(SupportedDataTypesToSupportLimits(data_type_limits.min_input));
  min->setB(SupportedDataTypesToSupportLimits(data_type_limits.min_input));
  min->setOutput(SupportedDataTypesToSupportLimits(data_type_limits.min_input));
  op_support_limits->setMin(min);
  MLBinarySupportLimits* pow = MLBinarySupportLimits::Create();
  pow->setA(SupportedDataTypesToSupportLimits(data_type_limits.pow_input));
  pow->setB(SupportedDataTypesToSupportLimits(data_type_limits.pow_input));
  pow->setOutput(SupportedDataTypesToSupportLimits(data_type_limits.pow_input));
  op_support_limits->setPow(pow);

  // Element-wise logical ops.
  MLBinarySupportLimits* equal = MLBinarySupportLimits::Create();
  equal->setA(SupportedDataTypesToSupportLimits(data_type_limits.equal_input));
  equal->setB(SupportedDataTypesToSupportLimits(data_type_limits.equal_input));
  equal->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.logical_output));
  op_support_limits->setEqual(equal);
  MLBinarySupportLimits* greater = MLBinarySupportLimits::Create();
  greater->setA(
      SupportedDataTypesToSupportLimits(data_type_limits.greater_input));
  greater->setB(
      SupportedDataTypesToSupportLimits(data_type_limits.greater_input));
  greater->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.logical_output));
  op_support_limits->setGreater(greater);
  MLBinarySupportLimits* greater_or_equal = MLBinarySupportLimits::Create();
  greater_or_equal->setA(SupportedDataTypesToSupportLimits(
      data_type_limits.greater_or_equal_input));
  greater_or_equal->setB(SupportedDataTypesToSupportLimits(
      data_type_limits.greater_or_equal_input));
  greater_or_equal->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.logical_output));
  op_support_limits->setGreaterOrEqual(greater_or_equal);
  MLBinarySupportLimits* lesser = MLBinarySupportLimits::Create();
  lesser->setA(
      SupportedDataTypesToSupportLimits(data_type_limits.lesser_input));
  lesser->setB(
      SupportedDataTypesToSupportLimits(data_type_limits.lesser_input));
  lesser->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.logical_output));
  op_support_limits->setLesser(lesser);
  MLBinarySupportLimits* lesser_or_equal = MLBinarySupportLimits::Create();
  lesser_or_equal->setA(SupportedDataTypesToSupportLimits(
      data_type_limits.lesser_or_equal_input));
  lesser_or_equal->setB(SupportedDataTypesToSupportLimits(
      data_type_limits.lesser_or_equal_input));
  lesser_or_equal->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.logical_output));
  op_support_limits->setLesserOrEqual(lesser_or_equal);
  MLBinarySupportLimits* logical_and = MLBinarySupportLimits::Create();
  logical_and->setA(
      SupportedDataTypesToSupportLimits(data_type_limits.logical_and_input));
  logical_and->setB(
      SupportedDataTypesToSupportLimits(data_type_limits.logical_and_input));
  logical_and->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.logical_output));
  op_support_limits->setLogicalAnd(logical_and);
  MLBinarySupportLimits* logical_or = MLBinarySupportLimits::Create();
  logical_or->setA(
      SupportedDataTypesToSupportLimits(data_type_limits.logical_or_input));
  logical_or->setB(
      SupportedDataTypesToSupportLimits(data_type_limits.logical_or_input));
  logical_or->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.logical_output));
  op_support_limits->setLogicalOr(logical_or);
  MLBinarySupportLimits* logical_xor = MLBinarySupportLimits::Create();
  logical_xor->setA(
      SupportedDataTypesToSupportLimits(data_type_limits.logical_xor_input));
  logical_xor->setB(
      SupportedDataTypesToSupportLimits(data_type_limits.logical_xor_input));
  logical_xor->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.logical_output));
  op_support_limits->setLogicalXor(logical_xor);
  MLLogicalNotSupportLimits* logical_not = MLLogicalNotSupportLimits::Create();
  logical_not->setA(
      SupportedDataTypesToSupportLimits(data_type_limits.logical_not_input));
  logical_not->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.logical_output));
  op_support_limits->setLogicalNot(logical_not);

  // Element-wise unary ops.
  MLSingleInputSupportLimits* abs = MLSingleInputSupportLimits::Create();
  abs->setInput(SupportedDataTypesToSupportLimits(data_type_limits.abs_input));
  abs->setOutput(SupportedDataTypesToSupportLimits(data_type_limits.abs_input));
  op_support_limits->setAbs(abs);
  MLSingleInputSupportLimits* ceil = MLSingleInputSupportLimits::Create();
  ceil->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.ceil_input));
  ceil->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.ceil_input));
  op_support_limits->setCeil(ceil);
  MLSingleInputSupportLimits* cos = MLSingleInputSupportLimits::Create();
  cos->setInput(SupportedDataTypesToSupportLimits(data_type_limits.cos_input));
  cos->setOutput(SupportedDataTypesToSupportLimits(data_type_limits.cos_input));
  op_support_limits->setCos(cos);
  MLSingleInputSupportLimits* erf = MLSingleInputSupportLimits::Create();
  erf->setInput(SupportedDataTypesToSupportLimits(data_type_limits.erf_input));
  erf->setOutput(SupportedDataTypesToSupportLimits(data_type_limits.erf_input));
  op_support_limits->setErf(erf);
  MLSingleInputSupportLimits* exp = MLSingleInputSupportLimits::Create();
  exp->setInput(SupportedDataTypesToSupportLimits(data_type_limits.exp_input));
  exp->setOutput(SupportedDataTypesToSupportLimits(data_type_limits.exp_input));
  op_support_limits->setExp(exp);
  MLSingleInputSupportLimits* floor = MLSingleInputSupportLimits::Create();
  floor->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.floor_input));
  floor->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.floor_input));
  op_support_limits->setFloor(floor);
  MLSingleInputSupportLimits* identity = MLSingleInputSupportLimits::Create();
  identity->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.identity_input));
  identity->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.identity_input));
  op_support_limits->setIdentity(identity);
  MLSingleInputSupportLimits* log = MLSingleInputSupportLimits::Create();
  log->setInput(SupportedDataTypesToSupportLimits(data_type_limits.log_input));
  log->setOutput(SupportedDataTypesToSupportLimits(data_type_limits.log_input));
  op_support_limits->setLog(log);
  MLSingleInputSupportLimits* neg = MLSingleInputSupportLimits::Create();
  neg->setInput(SupportedDataTypesToSupportLimits(data_type_limits.neg_input));
  neg->setOutput(SupportedDataTypesToSupportLimits(data_type_limits.neg_input));
  op_support_limits->setNeg(neg);
  MLSingleInputSupportLimits* reciprocal = MLSingleInputSupportLimits::Create();
  reciprocal->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.reciprocal_input));
  reciprocal->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.reciprocal_input));
  op_support_limits->setReciprocal(reciprocal);
  MLSingleInputSupportLimits* sign = MLSingleInputSupportLimits::Create();
  sign->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.sign_input));
  sign->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.sign_input));
  op_support_limits->setSign(sign);
  MLSingleInputSupportLimits* sin = MLSingleInputSupportLimits::Create();
  sin->setInput(SupportedDataTypesToSupportLimits(data_type_limits.sin_input));
  sin->setOutput(SupportedDataTypesToSupportLimits(data_type_limits.sin_input));
  op_support_limits->setSin(sin);
  MLSingleInputSupportLimits* sqrt = MLSingleInputSupportLimits::Create();
  sqrt->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.sqrt_input));
  sqrt->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.sqrt_input));
  op_support_limits->setSqrt(sqrt);
  MLSingleInputSupportLimits* tan = MLSingleInputSupportLimits::Create();
  tan->setInput(SupportedDataTypesToSupportLimits(data_type_limits.tan_input));
  tan->setOutput(SupportedDataTypesToSupportLimits(data_type_limits.tan_input));
  op_support_limits->setTan(tan);

  MLSingleInputSupportLimits* elu = MLSingleInputSupportLimits::Create();
  elu->setInput(SupportedDataTypesToSupportLimits(data_type_limits.elu_input));
  elu->setOutput(SupportedDataTypesToSupportLimits(data_type_limits.elu_input));
  op_support_limits->setElu(elu);

  MLSingleInputSupportLimits* expand = MLSingleInputSupportLimits::Create();
  expand->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.expand_input));
  expand->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.expand_input));
  op_support_limits->setExpand(expand);

  MLGatherSupportLimits* gather = MLGatherSupportLimits::Create();
  gather->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.gather_input));
  gather->setIndices(
      SupportedDataTypesToSupportLimits(data_type_limits.gather_indices));
  gather->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.gather_input));
  op_support_limits->setGather(gather);

  MLGatherSupportLimits* gather_elements = MLGatherSupportLimits::Create();
  gather_elements->setInput(SupportedDataTypesToSupportLimits(
      data_type_limits.gather_elements_input));
  gather_elements->setIndices(SupportedDataTypesToSupportLimits(
      data_type_limits.gather_elements_indices));
  gather_elements->setOutput(SupportedDataTypesToSupportLimits(
      data_type_limits.gather_elements_input));
  op_support_limits->setGatherElements(gather_elements);

  MLGatherSupportLimits* gather_nd = MLGatherSupportLimits::Create();
  gather_nd->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.gather_nd_input));
  gather_nd->setIndices(
      SupportedDataTypesToSupportLimits(data_type_limits.gather_nd_indices));
  gather_nd->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.gather_nd_input));
  op_support_limits->setGatherND(gather_nd);

  MLSingleInputSupportLimits* gelu = MLSingleInputSupportLimits::Create();
  gelu->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.gelu_input));
  gelu->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.gelu_input));
  op_support_limits->setGelu(gelu);

  MLGemmSupportLimits* gemm = MLGemmSupportLimits::Create();
  gemm->setA(SupportedDataTypesToSupportLimits(data_type_limits.gemm_input));
  gemm->setB(SupportedDataTypesToSupportLimits(data_type_limits.gemm_input));
  gemm->setC(SupportedDataTypesToSupportLimits(data_type_limits.gemm_input));
  gemm->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.gemm_input));
  op_support_limits->setGemm(gemm);

  MLGruSupportLimits* gru = MLGruSupportLimits::Create();
  gru->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.gru_input));
  gru->setWeight(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.gru_input));
  gru->setRecurrentWeight(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.gru_input));
  gru->setBias(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.gru_input));
  gru->setRecurrentBias(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.gru_input));
  gru->setInitialHiddenState(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.gru_input));
  gru->setOutputs(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.gru_input));
  op_support_limits->setGru(gru);

  MLGruCellSupportLimits* gru_cell = MLGruCellSupportLimits::Create();
  gru_cell->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.gru_cell_input));
  gru_cell->setWeight(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.gru_cell_input));
  gru_cell->setRecurrentWeight(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.gru_cell_input));
  gru_cell->setHiddenState(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.gru_cell_input));
  gru_cell->setBias(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.gru_cell_input));
  gru_cell->setRecurrentBias(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.gru_cell_input));
  gru_cell->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.gru_cell_input));
  op_support_limits->setGruCell(gru_cell);

  MLSingleInputSupportLimits* hard_sigmoid =
      MLSingleInputSupportLimits::Create();
  hard_sigmoid->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.hard_sigmoid_input));
  hard_sigmoid->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.hard_sigmoid_input));
  op_support_limits->setHardSigmoid(hard_sigmoid);

  MLSingleInputSupportLimits* hard_swish = MLSingleInputSupportLimits::Create();
  hard_swish->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.hard_swish_input));
  hard_swish->setOutput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.hard_swish_input));
  op_support_limits->setHardSwish(hard_swish);

  MLNormalizationSupportLimits* instance_normalization =
      MLNormalizationSupportLimits::Create();
  instance_normalization->setInput(SupportedDataTypesToSupportLimits(
      data_type_limits.instance_normalization_input));
  instance_normalization->setScale(SupportedDataTypesToSupportLimits(
      data_type_limits.instance_normalization_input));
  instance_normalization->setBias(SupportedDataTypesToSupportLimits(
      data_type_limits.instance_normalization_input));
  instance_normalization->setOutput(SupportedDataTypesToSupportLimits(
      data_type_limits.instance_normalization_input));
  op_support_limits->setInstanceNormalization(instance_normalization);

  MLNormalizationSupportLimits* layer_normalization =
      MLNormalizationSupportLimits::Create();
  layer_normalization->setInput(SupportedDataTypesToSupportLimits(
      data_type_limits.layer_normalization_input));
  layer_normalization->setScale(SupportedDataTypesToSupportLimits(
      data_type_limits.layer_normalization_input));
  layer_normalization->setBias(SupportedDataTypesToSupportLimits(
      data_type_limits.layer_normalization_input));
  layer_normalization->setOutput(SupportedDataTypesToSupportLimits(
      data_type_limits.layer_normalization_input));
  op_support_limits->setLayerNormalization(layer_normalization);

  MLSingleInputSupportLimits* leaky_relu = MLSingleInputSupportLimits::Create();
  leaky_relu->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.leaky_relu_input));
  leaky_relu->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.leaky_relu_input));
  op_support_limits->setLeakyRelu(leaky_relu);

  MLSingleInputSupportLimits* linear = MLSingleInputSupportLimits::Create();
  linear->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.linear_input));
  linear->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.linear_input));
  op_support_limits->setLinear(linear);

  MLLstmSupportLimits* lstm = MLLstmSupportLimits::Create();
  lstm->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.lstm_input));
  lstm->setWeight(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.lstm_input));
  lstm->setRecurrentWeight(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.lstm_input));
  lstm->setBias(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.lstm_input));
  lstm->setRecurrentBias(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.lstm_input));
  lstm->setPeepholeWeight(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.lstm_input));
  lstm->setInitialHiddenState(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.lstm_input));
  lstm->setInitialCellState(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.lstm_input));
  lstm->setOutputs(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.lstm_input));
  op_support_limits->setLstm(lstm);

  MLLstmCellSupportLimits* lstm_cell = MLLstmCellSupportLimits::Create();
  lstm_cell->setInput(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.lstm_cell_input));
  lstm_cell->setWeight(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.lstm_cell_input));
  lstm_cell->setRecurrentWeight(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.lstm_cell_input));
  lstm_cell->setHiddenState(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.lstm_cell_input));
  lstm_cell->setCellState(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.lstm_cell_input));
  lstm_cell->setBias(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.lstm_cell_input));
  lstm_cell->setRecurrentBias(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.lstm_cell_input));
  lstm_cell->setPeepholeWeight(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.lstm_cell_input));
  lstm_cell->setOutputs(SupportedDataTypesToSupportLimits(
      properties_.data_type_limits.lstm_cell_input));
  op_support_limits->setLstmCell(lstm_cell);

  MLBinarySupportLimits* matmul = MLBinarySupportLimits::Create();
  matmul->setA(
      SupportedDataTypesToSupportLimits(data_type_limits.matmul_input));
  matmul->setB(
      SupportedDataTypesToSupportLimits(data_type_limits.matmul_input));
  matmul->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.matmul_input));
  op_support_limits->setMatmul(matmul);

  MLSingleInputSupportLimits* pad = MLSingleInputSupportLimits::Create();
  pad->setInput(SupportedDataTypesToSupportLimits(data_type_limits.pad_input));
  pad->setOutput(SupportedDataTypesToSupportLimits(data_type_limits.pad_input));
  op_support_limits->setPad(pad);

  // Pool2d.
  MLSingleInputSupportLimits* average_pool2d =
      MLSingleInputSupportLimits::Create();
  average_pool2d->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.average_pool2d_input));
  average_pool2d->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.average_pool2d_input));
  op_support_limits->setAveragePool2d(average_pool2d);

  MLSingleInputSupportLimits* l2_pool2d = MLSingleInputSupportLimits::Create();
  l2_pool2d->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.l2_pool2d_input));
  l2_pool2d->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.l2_pool2d_input));
  op_support_limits->setL2Pool2d(l2_pool2d);

  MLSingleInputSupportLimits* max_pool2d = MLSingleInputSupportLimits::Create();
  max_pool2d->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.max_pool2d_input));
  max_pool2d->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.max_pool2d_input));
  op_support_limits->setMaxPool2d(max_pool2d);

  MLPreluSupportLimits* prelu = MLPreluSupportLimits::Create();
  prelu->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.prelu_input));
  prelu->setSlope(
      SupportedDataTypesToSupportLimits(data_type_limits.prelu_input));
  prelu->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.prelu_input));
  op_support_limits->setPrelu(prelu);

  MLQuantizeDequantizeLinearSupportLimits* quantize_linear =
      MLQuantizeDequantizeLinearSupportLimits::Create();
  quantize_linear->setInput(SupportedDataTypesToSupportLimits(
      data_type_limits.quantize_linear_input));
  quantize_linear->setScale(SupportedDataTypesToSupportLimits(
      data_type_limits.quantize_linear_input));
  quantize_linear->setZeroPoint(SupportedDataTypesToSupportLimits(
      data_type_limits.quantize_linear_zero_point));
  quantize_linear->setOutput(SupportedDataTypesToSupportLimits(
      data_type_limits.quantize_linear_zero_point));
  op_support_limits->setQuantizeLinear(quantize_linear);

  // Reduction ops.
  MLSingleInputSupportLimits* reduce_l1 = MLSingleInputSupportLimits::Create();
  reduce_l1->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.reduce_l1_input));
  reduce_l1->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.reduce_l1_input));
  op_support_limits->setReduceL1(reduce_l1);
  MLSingleInputSupportLimits* reduce_l2 = MLSingleInputSupportLimits::Create();
  reduce_l2->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.reduce_l2_input));
  reduce_l2->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.reduce_l2_input));
  op_support_limits->setReduceL2(reduce_l2);
  MLSingleInputSupportLimits* reduce_log_sum =
      MLSingleInputSupportLimits::Create();
  reduce_log_sum->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.reduce_log_sum_input));
  reduce_log_sum->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.reduce_log_sum_input));
  op_support_limits->setReduceLogSum(reduce_log_sum);
  MLSingleInputSupportLimits* reduce_log_sum_exp =
      MLSingleInputSupportLimits::Create();
  reduce_log_sum_exp->setInput(SupportedDataTypesToSupportLimits(
      data_type_limits.reduce_log_sum_exp_input));
  reduce_log_sum_exp->setOutput(SupportedDataTypesToSupportLimits(
      data_type_limits.reduce_log_sum_exp_input));
  op_support_limits->setReduceLogSumExp(reduce_log_sum_exp);
  MLSingleInputSupportLimits* reduce_max = MLSingleInputSupportLimits::Create();
  reduce_max->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.reduce_max_input));
  reduce_max->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.reduce_max_input));
  op_support_limits->setReduceMax(reduce_max);
  MLSingleInputSupportLimits* reduce_mean =
      MLSingleInputSupportLimits::Create();
  reduce_mean->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.reduce_mean_input));
  reduce_mean->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.reduce_mean_input));
  op_support_limits->setReduceMean(reduce_mean);
  MLSingleInputSupportLimits* reduce_min = MLSingleInputSupportLimits::Create();
  reduce_min->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.reduce_min_input));
  reduce_min->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.reduce_min_input));
  op_support_limits->setReduceMin(reduce_min);
  MLSingleInputSupportLimits* reduce_product =
      MLSingleInputSupportLimits::Create();
  reduce_product->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.reduce_product_input));
  reduce_product->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.reduce_product_input));
  op_support_limits->setReduceProduct(reduce_product);
  MLSingleInputSupportLimits* reduce_sum = MLSingleInputSupportLimits::Create();
  reduce_sum->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.reduce_sum_input));
  reduce_sum->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.reduce_sum_input));
  op_support_limits->setReduceSum(reduce_sum);
  MLSingleInputSupportLimits* reduce_sum_square =
      MLSingleInputSupportLimits::Create();
  reduce_sum_square->setInput(SupportedDataTypesToSupportLimits(
      data_type_limits.reduce_sum_square_input));
  reduce_sum_square->setOutput(SupportedDataTypesToSupportLimits(
      data_type_limits.reduce_sum_square_input));
  op_support_limits->setReduceSumSquare(reduce_sum_square);

  MLSingleInputSupportLimits* relu = MLSingleInputSupportLimits::Create();
  relu->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.relu_input));
  relu->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.relu_input));
  op_support_limits->setRelu(relu);

  MLSingleInputSupportLimits* resample2d = MLSingleInputSupportLimits::Create();
  resample2d->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.resample2d_input));
  resample2d->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.resample2d_input));
  op_support_limits->setResample2d(resample2d);

  MLSingleInputSupportLimits* reshape = MLSingleInputSupportLimits::Create();
  reshape->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.reshape_input));
  reshape->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.reshape_input));
  op_support_limits->setReshape(reshape);

  MLScatterSupportLimits* scatter_nd = MLScatterSupportLimits::Create();
  scatter_nd->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.scatter_nd_input));
  scatter_nd->setIndices(
      SupportedDataTypesToSupportLimits(data_type_limits.scatter_nd_indices));
  scatter_nd->setUpdates(
      SupportedDataTypesToSupportLimits(data_type_limits.scatter_nd_input));
  scatter_nd->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.scatter_nd_input));
  op_support_limits->setScatterND(scatter_nd);

  MLSingleInputSupportLimits* sigmoid = MLSingleInputSupportLimits::Create();
  sigmoid->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.sigmoid_input));
  sigmoid->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.sigmoid_input));
  op_support_limits->setSigmoid(sigmoid);

  MLSingleInputSupportLimits* slice = MLSingleInputSupportLimits::Create();
  slice->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.slice_input));
  slice->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.slice_input));
  op_support_limits->setSlice(slice);

  MLSingleInputSupportLimits* softmax = MLSingleInputSupportLimits::Create();
  softmax->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.softmax_input));
  softmax->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.softmax_input));
  op_support_limits->setSoftmax(softmax);

  MLSingleInputSupportLimits* softplus = MLSingleInputSupportLimits::Create();
  softplus->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.softplus_input));
  softplus->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.softplus_input));
  op_support_limits->setSoftplus(softplus);

  MLSingleInputSupportLimits* softsign = MLSingleInputSupportLimits::Create();
  softsign->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.softsign_input));
  softsign->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.softsign_input));
  op_support_limits->setSoftsign(softsign);

  MLSingleInputSupportLimits* split = MLSingleInputSupportLimits::Create();
  split->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.split_input));
  split->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.split_input));
  op_support_limits->setSplit(split);

  MLSingleInputSupportLimits* tanh = MLSingleInputSupportLimits::Create();
  tanh->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.tanh_input));
  tanh->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.tanh_input));
  op_support_limits->setTanh(tanh);

  MLSingleInputSupportLimits* tile = MLSingleInputSupportLimits::Create();
  tile->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.tile_input));
  tile->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.tile_input));
  op_support_limits->setTile(tile);

  MLSingleInputSupportLimits* transpose = MLSingleInputSupportLimits::Create();
  transpose->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.transpose_input));
  transpose->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.transpose_input));
  op_support_limits->setTranspose(transpose);

  MLSingleInputSupportLimits* triangular = MLSingleInputSupportLimits::Create();
  triangular->setInput(
      SupportedDataTypesToSupportLimits(data_type_limits.triangular_input));
  triangular->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.triangular_input));
  op_support_limits->setTriangular(triangular);

  MLWhereSupportLimits* where = MLWhereSupportLimits::Create();
  where->setCondition(
      SupportedDataTypesToSupportLimits(data_type_limits.where_condition));
  where->setTrueValue(
      SupportedDataTypesToSupportLimits(data_type_limits.where_value));
  where->setFalseValue(
      SupportedDataTypesToSupportLimits(data_type_limits.where_value));
  where->setOutput(
      SupportedDataTypesToSupportLimits(data_type_limits.where_value));
  op_support_limits->setWhere(where);

  return op_support_limits;
}

void MLContext::OnGraphCreated(MLGraph* graph) {
  graphs_.insert(graph);
}

ScriptPromise<MLTensor> MLContext::createTensor(
    ScriptState* script_state,
    const MLTensorDescriptor* descriptor,
    ExceptionState& exception_state) {
  ScopedMLTrace scoped_trace("MLContext::createTensor");
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

  ASSIGN_OR_RETURN(
      Vector<uint32_t> shape, GetShapeFromDescriptor(script_state, *descriptor),
      [&exception_state](std::string error) -> ScriptPromise<MLTensor> {
        exception_state.ThrowTypeError(String::FromUTF8(error));
        return EmptyPromise();
      });

  ASSIGN_OR_RETURN(
      webnn::OperandDescriptor validated_descriptor,
      webnn::OperandDescriptor::Create(
          FromBlinkDataType(descriptor->dataType().AsEnum()), shape),
      [&exception_state](std::string error) {
        exception_state.ThrowTypeError(String(error));
        return ScriptPromise<MLTensor>();
      });

  RETURN_IF_ERROR(webnn::ValidateTensor(properties_, validated_descriptor),
                  [&exception_state](std::string error) {
                    exception_state.ThrowTypeError(String(error));
                    return ScriptPromise<MLTensor>();
                  });

  // WebNN bitfield values have the same value as enums.
  webnn::MLTensorUsage usage;
  if (descriptor->hasUsage()) {
    usage = webnn::MLTensorUsage::FromEnumBitmask(descriptor->usage());
  }

  auto tensor_info =
      webnn::mojom::blink::TensorInfo::New(validated_descriptor, usage);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<MLTensor>>(
      script_state, exception_state.GetContext());
  pending_resolvers_.insert(resolver);

  // Use `WebNNContext` to create `WebNNTensor` message pipe.
  context_remote_->CreateTensor(
      std::move(tensor_info),
      WTF::BindOnce(&MLContext::DidCreateWebNNTensor, WrapPersistent(this),
                    std::move(scoped_trace), WrapPersistent(resolver),
                    std::move(validated_descriptor), usage));

  return resolver->Promise();
}

void MLContext::writeTensor(
    ScriptState* script_state,
    MLTensor* dst_tensor,
    const MaybeShared<DOMArrayBufferView>& src_data_view,
    uint64_t src_element_offset,
    ExceptionState& exception_state) {
  WriteWebNNTensor(script_state, dst_tensor,
                   src_data_view->ByteSpanMaybeShared(), src_element_offset,
                   src_data_view->TypeSize(),
                   /*src_element_count=*/std::nullopt, exception_state);
}

void MLContext::writeTensor(
    ScriptState* script_state,
    MLTensor* dst_tensor,
    const MaybeShared<DOMArrayBufferView>& src_data_view,
    uint64_t src_element_offset,
    uint64_t src_element_count,
    ExceptionState& exception_state) {
  WriteWebNNTensor(script_state, dst_tensor,
                   src_data_view->ByteSpanMaybeShared(), src_element_offset,
                   src_data_view->TypeSize(), src_element_count,
                   exception_state);
}

void MLContext::writeTensor(ScriptState* script_state,
                            MLTensor* dst_tensor,
                            const DOMArrayBufferBase* src_data_base,
                            uint64_t src_byte_offset,
                            ExceptionState& exception_state) {
  WriteWebNNTensor(script_state, dst_tensor,
                   src_data_base->ByteSpanMaybeShared(), src_byte_offset,
                   /*src_data_type_size_bytes=*/1,
                   /*src_element_count=*/std::nullopt, exception_state);
}

void MLContext::writeTensor(ScriptState* script_state,
                            MLTensor* dst_tensor,
                            const DOMArrayBufferBase* src_data_base,
                            uint64_t src_byte_offset,
                            uint64_t src_byte_size,
                            ExceptionState& exception_state) {
  WriteWebNNTensor(script_state, dst_tensor,
                   src_data_base->ByteSpanMaybeShared(), src_byte_offset,
                   /*src_data_type_size_bytes=*/1,
                   /*src_element_count=*/src_byte_size, exception_state);
}

ScriptPromise<DOMArrayBuffer> MLContext::readTensor(
    ScriptState* script_state,
    MLTensor* src_tensor,
    ExceptionState& exception_state) {
  ScopedMLTrace scoped_trace("MLContext::readTensor");
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return EmptyPromise();
  }

  if (src_tensor->context() != this) {
    exception_state.ThrowTypeError(
        "The source buffer wasn't created with this context.");
    return EmptyPromise();
  }

  if (!src_tensor->Usage().Has(webnn::MLTensorUsageFlags::kRead)) {
    exception_state.ThrowTypeError(
        "The source buffer doesn't have read access.");
    return EmptyPromise();
  }

  return src_tensor->ReadTensorImpl(std::move(scoped_trace), script_state,
                                    exception_state);
}

ScriptPromise<IDLUndefined> MLContext::readTensor(
    ScriptState* script_state,
    MLTensor* src_tensor,
    DOMArrayBufferBase* dst_data,
    ExceptionState& exception_state) {
  ScopedMLTrace scoped_trace("MLContext::readTensor");
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return EmptyPromise();
  }

  if (src_tensor->context() != this) {
    exception_state.ThrowTypeError(
        "The source buffer wasn't created with this context.");
    return EmptyPromise();
  }

  return src_tensor->ReadTensorImpl(std::move(scoped_trace), script_state,
                                    dst_data, exception_state);
}

ScriptPromise<IDLUndefined> MLContext::readTensor(
    ScriptState* script_state,
    MLTensor* src_tensor,
    MaybeShared<DOMArrayBufferView> dst_data,
    ExceptionState& exception_state) {
  ScopedMLTrace scoped_trace("MLContext::readTensor");
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return EmptyPromise();
  }

  if (src_tensor->context() != this) {
    exception_state.ThrowTypeError(
        "The source buffer wasn't created with this context.");
    return EmptyPromise();
  }

  return src_tensor->ReadTensorImpl(std::move(scoped_trace), script_state,
                                    dst_data.Get(), exception_state);
}

void MLContext::WriteWebNNTensor(ScriptState* script_state,
                                 MLTensor* dst_tensor,
                                 base::span<const uint8_t> src_data,
                                 uint64_t src_element_offset,
                                 unsigned src_data_type_size_bytes,
                                 std::optional<uint64_t> src_element_count,
                                 ExceptionState& exception_state) {
  ScopedMLTrace scoped_trace("MLContext::writeTensor");
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return;
  }

  if (dst_tensor->context() != this) {
    exception_state.ThrowTypeError(
        "The destination buffer wasn't created with this context.");
    return;
  }

  if (!dst_tensor->Usage().Has(webnn::MLTensorUsageFlags::kWrite)) {
    exception_state.ThrowTypeError(
        "The destination buffer doesn't have write access.");
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

  if (write_byte_size > dst_tensor->PackedByteLength()) {
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

  dst_tensor->WriteTensorImpl(
      src_data.subspan(checked_src_byte_offset.ValueOrDie(),
                       checked_write_byte_size.ValueOrDie()),
      exception_state);
}

void MLContext::dispatch(ScriptState* script_state,
                         MLGraph* graph,
                         const MLNamedTensors& inputs,
                         const MLNamedTensors& outputs,
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

void MLContext::DidCreateWebNNTensor(
    ScopedMLTrace scoped_trace,
    ScriptPromiseResolver<blink::MLTensor>* resolver,
    webnn::OperandDescriptor validated_descriptor,
    webnn::MLTensorUsage usage,
    webnn::mojom::blink::CreateTensorResultPtr result) {
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

  auto* buffer = MakeGarbageCollected<MLTensor>(
      resolver->GetExecutionContext(), this, std::move(validated_descriptor),
      usage, std::move(result->get_success()), base::PassKey<MLContext>());
  buffers_.insert(buffer);

  resolver->Resolve(buffer);
}

}  // namespace blink
