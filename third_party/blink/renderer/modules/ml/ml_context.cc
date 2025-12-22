// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/ml_context.h"

#include <utility>

#include "base/feature_list.h"
#include "base/numerics/checked_math.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/types/pass_key.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/cpp/webnn_errors.h"
#include "services/webnn/public/cpp/webnn_trace.h"
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
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_rank_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_scatter_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_single_input_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_split_support_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_tensor_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_tensor_limits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_where_support_limits.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_error.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_tensor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"

#if BUILDFLAG(IS_MAC)
#include "gpu/command_buffer/common/capabilities.h"
#endif  // BUILDFLAG(IS_MAC)

namespace blink {

namespace {

MLTensorLimits* SupportedDataTypesAndRanksToTensorLimits(
    const webnn::SupportedDataTypes& supported_data_types,
    const webnn::SupportedRanks& supported_ranks) {
  MLTensorLimits* tensor_limits = MLTensorLimits::Create();

  MLRankRange* rank_range = MLRankRange::Create();
  rank_range->setMin(supported_ranks.min);
  rank_range->setMax(supported_ranks.max);
  tensor_limits->setRankRange(rank_range);

  Vector<String> data_types;
  for (auto data_type : supported_data_types) {
    data_types.push_back(webnn::DataTypeToString(data_type));
  }
  tensor_limits->setDataTypes(data_types);

  return tensor_limits;
}

MLTensorLimits* SupportedTensorLimitsToTensorLimits(
    const webnn::SupportedTensors& supported_tensors) {
  MLTensorLimits* tensor_limits = MLTensorLimits::Create();

  MLRankRange* rank_range = MLRankRange::Create();
  rank_range->setMin(supported_tensors.ranks.min);
  rank_range->setMax(supported_tensors.ranks.max);
  tensor_limits->setRankRange(rank_range);

  Vector<String> data_types;
  for (auto data_type : supported_tensors.data_types) {
    data_types.push_back(webnn::DataTypeToString(data_type));
  }
  tensor_limits->setDataTypes(data_types);

  return tensor_limits;
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

// Flatten N-D shape into 2D size for shared image creation.
// For example:
//   shape = [2,3,4]  W=4 x H=6 = 24 elements
//   shape = [3]      W=3 x H=1 = 3 elements
//   shape = [2,0,4]  W=4 x H=0 = 0 elements
//   shape = [0]      W=0 x H=0 = 0 elements
base::expected<gfx::Size, String> ShapeToSharedImageSize(
    const std::vector<uint32_t>& shape) {
  if (shape.empty()) {
    return base::unexpected("The tensor shape must not be [].");
  }

  // Last dimension
  const uint32_t width = shape.back();

  // Product of all preceding dimensions
  base::CheckedNumeric<uint32_t> checked_height(1u);
  for (size_t i = 0; i < shape.size() - 1; ++i) {
    checked_height *= shape[i];
  }

  uint32_t height;
  if (!checked_height.AssignIfValid(&height)) {
    return base::unexpected(
        "The number of elements implied by the shape is too large.");
  }

  // TODO(crbug.com/329471677): Consider supporting size 0 dimensions.
  DCHECK_NE(width, 0u);

  return gfx::Size(width, height);
}

base::expected<viz::SharedImageFormat, String>
OperandDataTypeToSharedImageFormat(webnn::OperandDataType data_type) {
// TODO(crbug.com/427252761): Support other data types in CoreML backend.
#if BUILDFLAG(IS_MAC)
  if (data_type != webnn::OperandDataType::kFloat16) {
    return base::unexpected(String::Format(
        "Invalid operand data type: %s", ToBlinkDataType(data_type).AsCStr()));
  }
  // The only format supported by CoreML `MLMultiArray::initWithPixelBuffer`.
  return viz::SinglePlaneFormat::kR_F16;
#else
  // Maps data_type to equivalent element size.
  switch (data_type) {
    // 1 byte per element
    case webnn::OperandDataType::kUint8:
    case webnn::OperandDataType::kInt8:
      return viz::SinglePlaneFormat::kR_8;
    // 2 bytes per element
    case webnn::OperandDataType::kFloat16:
      return viz::SinglePlaneFormat::kR_F16;
    // 4 bytes per element
    case webnn::OperandDataType::kUint32:
    case webnn::OperandDataType::kInt32:
    case webnn::OperandDataType::kFloat32:
      // TODO(crbug.com/345352987): Use shared image formats with 32 bits per
      // channel for float32/int32/uint32 instead of RGBA_8888, which only
      // matches the size.
      return viz::SinglePlaneFormat::kRGBA_8888;
    // Default case is for new format types added to MLTensor.
    default:
      return base::unexpected(
          String::Format("Invalid operand data type: %s",
                         ToBlinkDataType(data_type).AsCStr()));
  }
#endif  // BUILDFLAG(IS_MAC)
}

gpu::SharedImageUsageSet OperandUsageToSharedImageUsageSet(
    const webnn::MLTensorUsage& usage) {
  gpu::SharedImageUsageSet shared_image_usage_set(
      gpu::SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR);
  if (usage.Has(webnn::MLTensorUsageFlags::kRead)) {
    shared_image_usage_set |= gpu::SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR_READ;
  }
  if (usage.Has(webnn::MLTensorUsageFlags::kWrite)) {
    shared_image_usage_set |= gpu::SHARED_IMAGE_USAGE_WEBNN_SHARED_TENSOR_WRITE;
  }
  if (usage.Has(webnn::MLTensorUsageFlags::kWebGpuInterop)) {
    shared_image_usage_set |= gpu::SHARED_IMAGE_USAGE_WEBGPU_SHARED_BUFFER;
  }
  return shared_image_usage_set;
}

#if BUILDFLAG(IS_MAC)
// CoreML uses OneComponent16Half pixel buffer for webgpu interop, with last
// dimension being the width and the product of the rest of the dimensions being
// the height, both of them need to be within webgpu texture size limit.
base::expected<void, std::string> IsValidTensorSize(
    const webnn::OperandDescriptor descriptor,
    int max_texture_size) {
  if (descriptor.Rank() > 0) {
    int width = descriptor.shape()[descriptor.Rank() - 1];
    if (width > max_texture_size) {
      return base::unexpected("Tensor size is too large.");
    }
    if (descriptor.Rank() > 1) {
      int height = descriptor.NumberOfElements() / width;
      if (height > max_texture_size) {
        return base::unexpected("Tensor size is too large.");
      }
    }
  }
  return base::ok();
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace

MLContext::MLContext(
    ExecutionContext* execution_context,
    const V8MLDeviceType device_type,
    const V8MLPowerPreference power_preference,
    webnn::mojom::blink::CreateContextSuccessPtr create_context_success)
    : device_type_(device_type),
      power_preference_(power_preference),
      lost_property_(MakeGarbageCollected<LostProperty>(execution_context)),
      context_remote_(execution_context),
      properties_(std::move(create_context_success->context_properties)),
      write_tensor_producer_(
          std::move(create_context_success->write_tensor_producer)),
      read_tensor_consumer_(
          std::move(create_context_success->read_tensor_consumer)),
      webnn_handle_(std::move(create_context_success->context_handle)) {
  context_remote_.Bind(
      std::move(create_context_success->context_remote),
      execution_context->GetTaskRunner(TaskType::kMachineLearning));
  context_remote_.set_disconnect_with_reason_handler(
      BindOnce(&MLContext::OnLost, WrapWeakPersistent(this)));
}

MLContext::~MLContext() = default;

V8MLDeviceType MLContext::GetDeviceType() const {
  return device_type_;
}

V8MLPowerPreference MLContext::GetPowerPreference() const {
  return power_preference_;
}

void MLContext::Trace(Visitor* visitor) const {
  visitor->Trace(lost_property_);
  visitor->Trace(context_remote_);
  visitor->Trace(pending_resolvers_);
  visitor->Trace(graphs_);
  visitor->Trace(graph_builders_);
  visitor->Trace(tensors_);
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

    for (const auto& tensor : tensors_) {
      tensor->destroy();
    }
  }
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
  op_support_limits->setMaxTensorByteLength(
      properties_.tensor_byte_length_limit);
  op_support_limits->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.input));
  op_support_limits->setConstant(
      SupportedTensorLimitsToTensorLimits(data_type_limits.constant));
  op_support_limits->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.output()));

  MLSingleInputSupportLimits* argmin = MLSingleInputSupportLimits::Create();
  argmin->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.arg_min_max_input));
  argmin->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.arg_min_max_output));
  op_support_limits->setArgMin(argmin);
  MLSingleInputSupportLimits* argmax = MLSingleInputSupportLimits::Create();
  argmax->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.arg_min_max_input));
  argmax->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.arg_min_max_output));
  op_support_limits->setArgMax(argmax);

  MLBatchNormalizationSupportLimits* batch_normalization =
      MLBatchNormalizationSupportLimits::Create();
  batch_normalization->setInput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.batch_normalization_input));
  batch_normalization->setMean(SupportedTensorLimitsToTensorLimits(
      data_type_limits.batch_normalization_mean));
  batch_normalization->setVariance(SupportedTensorLimitsToTensorLimits(
      data_type_limits.batch_normalization_mean));
  batch_normalization->setScale(SupportedTensorLimitsToTensorLimits(
      data_type_limits.batch_normalization_mean));
  batch_normalization->setBias(SupportedTensorLimitsToTensorLimits(
      data_type_limits.batch_normalization_mean));
  batch_normalization->setOutput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.batch_normalization_input));
  op_support_limits->setBatchNormalization(batch_normalization);

  MLSingleInputSupportLimits* cast = MLSingleInputSupportLimits::Create();
  cast->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.cast_input));
  cast->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.cast_input));
  op_support_limits->setCast(cast);

  MLSingleInputSupportLimits* clamp = MLSingleInputSupportLimits::Create();
  clamp->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.clamp_input));
  clamp->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.clamp_input));
  op_support_limits->setClamp(clamp);

  MLConcatSupportLimits* concat = MLConcatSupportLimits::Create();
  concat->setInputs(
      SupportedTensorLimitsToTensorLimits(data_type_limits.concat_inputs));
  concat->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.concat_inputs));
  op_support_limits->setConcat(concat);

  MLConv2dSupportLimits* conv2d = MLConv2dSupportLimits::Create();
  conv2d->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.conv2d_input));
  conv2d->setFilter(
      SupportedTensorLimitsToTensorLimits(data_type_limits.conv2d_input));
  conv2d->setBias(
      SupportedTensorLimitsToTensorLimits(data_type_limits.conv2d_bias));
  conv2d->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.conv2d_input));
  op_support_limits->setConv2d(conv2d);

  MLConv2dSupportLimits* conv_transpose2d = MLConv2dSupportLimits::Create();
  conv_transpose2d->setInput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.conv_transpose2d_input));
  conv_transpose2d->setFilter(SupportedTensorLimitsToTensorLimits(
      data_type_limits.conv_transpose2d_input));
  conv_transpose2d->setBias(SupportedTensorLimitsToTensorLimits(
      data_type_limits.conv_transpose2d_bias));
  conv_transpose2d->setOutput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.conv_transpose2d_input));
  op_support_limits->setConvTranspose2d(conv_transpose2d);

  MLSingleInputSupportLimits* cumulative_sum =
      MLSingleInputSupportLimits::Create();
  cumulative_sum->setInput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.cumulative_sum_input));
  cumulative_sum->setOutput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.cumulative_sum_input));
  op_support_limits->setCumulativeSum(cumulative_sum);

  MLQuantizeDequantizeLinearSupportLimits* dequantize_linear =
      MLQuantizeDequantizeLinearSupportLimits::Create();
  dequantize_linear->setInput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.dequantize_linear_input));
  dequantize_linear->setScale(SupportedTensorLimitsToTensorLimits(
      data_type_limits.dequantize_linear_scale));
  dequantize_linear->setZeroPoint(SupportedTensorLimitsToTensorLimits(
      data_type_limits.dequantize_linear_zero_point));
  dequantize_linear->setOutput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.dequantize_linear_scale));
  op_support_limits->setDequantizeLinear(dequantize_linear);

  // Element-wise binary ops.
  MLBinarySupportLimits* add = MLBinarySupportLimits::Create();
  add->setA(SupportedTensorLimitsToTensorLimits(data_type_limits.add_input));
  add->setB(SupportedTensorLimitsToTensorLimits(data_type_limits.add_input));
  add->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.add_input));
  op_support_limits->setAdd(add);
  MLBinarySupportLimits* sub = MLBinarySupportLimits::Create();
  sub->setA(SupportedTensorLimitsToTensorLimits(data_type_limits.sub_input));
  sub->setB(SupportedTensorLimitsToTensorLimits(data_type_limits.sub_input));
  sub->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.sub_input));
  op_support_limits->setSub(sub);
  MLBinarySupportLimits* mul = MLBinarySupportLimits::Create();
  mul->setA(SupportedTensorLimitsToTensorLimits(data_type_limits.mul_input));
  mul->setB(SupportedTensorLimitsToTensorLimits(data_type_limits.mul_input));
  mul->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.mul_input));
  op_support_limits->setMul(mul);
  MLBinarySupportLimits* div = MLBinarySupportLimits::Create();
  div->setA(SupportedTensorLimitsToTensorLimits(data_type_limits.div_input));
  div->setB(SupportedTensorLimitsToTensorLimits(data_type_limits.div_input));
  div->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.div_input));
  op_support_limits->setDiv(div);
  MLBinarySupportLimits* max = MLBinarySupportLimits::Create();
  max->setA(SupportedTensorLimitsToTensorLimits(data_type_limits.max_input));
  max->setB(SupportedTensorLimitsToTensorLimits(data_type_limits.max_input));
  max->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.max_input));
  op_support_limits->setMax(max);
  MLBinarySupportLimits* min = MLBinarySupportLimits::Create();
  min->setA(SupportedTensorLimitsToTensorLimits(data_type_limits.min_input));
  min->setB(SupportedTensorLimitsToTensorLimits(data_type_limits.min_input));
  min->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.min_input));
  op_support_limits->setMin(min);
  MLBinarySupportLimits* pow = MLBinarySupportLimits::Create();
  pow->setA(SupportedTensorLimitsToTensorLimits(data_type_limits.pow_input));
  pow->setB(SupportedTensorLimitsToTensorLimits(data_type_limits.pow_input));
  pow->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.pow_input));
  op_support_limits->setPow(pow);

  // Element-wise logical ops.
  MLBinarySupportLimits* equal = MLBinarySupportLimits::Create();
  equal->setA(
      SupportedTensorLimitsToTensorLimits(data_type_limits.equal_input));
  equal->setB(
      SupportedTensorLimitsToTensorLimits(data_type_limits.equal_input));
  equal->setOutput(SupportedDataTypesAndRanksToTensorLimits(
      data_type_limits.logical_output, data_type_limits.equal_input.ranks));
  op_support_limits->setEqual(equal);
  MLBinarySupportLimits* greater = MLBinarySupportLimits::Create();
  greater->setA(
      SupportedTensorLimitsToTensorLimits(data_type_limits.greater_input));
  greater->setB(
      SupportedTensorLimitsToTensorLimits(data_type_limits.greater_input));
  greater->setOutput(SupportedDataTypesAndRanksToTensorLimits(
      data_type_limits.logical_output, data_type_limits.greater_input.ranks));
  op_support_limits->setGreater(greater);
  MLBinarySupportLimits* greater_or_equal = MLBinarySupportLimits::Create();
  greater_or_equal->setA(SupportedTensorLimitsToTensorLimits(
      data_type_limits.greater_or_equal_input));
  greater_or_equal->setB(SupportedTensorLimitsToTensorLimits(
      data_type_limits.greater_or_equal_input));
  greater_or_equal->setOutput(SupportedDataTypesAndRanksToTensorLimits(
      data_type_limits.logical_output,
      data_type_limits.greater_or_equal_input.ranks));
  op_support_limits->setGreaterOrEqual(greater_or_equal);
  MLBinarySupportLimits* lesser = MLBinarySupportLimits::Create();
  lesser->setA(
      SupportedTensorLimitsToTensorLimits(data_type_limits.lesser_input));
  lesser->setB(
      SupportedTensorLimitsToTensorLimits(data_type_limits.lesser_input));
  lesser->setOutput(SupportedDataTypesAndRanksToTensorLimits(
      data_type_limits.logical_output, data_type_limits.lesser_input.ranks));
  op_support_limits->setLesser(lesser);
  MLBinarySupportLimits* lesser_or_equal = MLBinarySupportLimits::Create();
  lesser_or_equal->setA(SupportedTensorLimitsToTensorLimits(
      data_type_limits.lesser_or_equal_input));
  lesser_or_equal->setB(SupportedTensorLimitsToTensorLimits(
      data_type_limits.lesser_or_equal_input));
  lesser_or_equal->setOutput(SupportedDataTypesAndRanksToTensorLimits(
      data_type_limits.logical_output,
      data_type_limits.lesser_or_equal_input.ranks));
  op_support_limits->setLesserOrEqual(lesser_or_equal);
  MLBinarySupportLimits* not_equal = MLBinarySupportLimits::Create();
  not_equal->setA(
      SupportedTensorLimitsToTensorLimits(data_type_limits.not_equal_input));
  not_equal->setB(
      SupportedTensorLimitsToTensorLimits(data_type_limits.not_equal_input));
  not_equal->setOutput(SupportedDataTypesAndRanksToTensorLimits(
      data_type_limits.logical_output, data_type_limits.not_equal_input.ranks));
  op_support_limits->setNotEqual(not_equal);
  MLBinarySupportLimits* logical_and = MLBinarySupportLimits::Create();
  logical_and->setA(
      SupportedTensorLimitsToTensorLimits(data_type_limits.logical_and_input));
  logical_and->setB(
      SupportedTensorLimitsToTensorLimits(data_type_limits.logical_and_input));
  logical_and->setOutput(SupportedDataTypesAndRanksToTensorLimits(
      data_type_limits.logical_output,
      data_type_limits.logical_and_input.ranks));
  op_support_limits->setLogicalAnd(logical_and);
  MLBinarySupportLimits* logical_or = MLBinarySupportLimits::Create();
  logical_or->setA(
      SupportedTensorLimitsToTensorLimits(data_type_limits.logical_or_input));
  logical_or->setB(
      SupportedTensorLimitsToTensorLimits(data_type_limits.logical_or_input));
  logical_or->setOutput(SupportedDataTypesAndRanksToTensorLimits(
      data_type_limits.logical_output,
      data_type_limits.logical_or_input.ranks));
  op_support_limits->setLogicalOr(logical_or);
  MLBinarySupportLimits* logical_xor = MLBinarySupportLimits::Create();
  logical_xor->setA(
      SupportedTensorLimitsToTensorLimits(data_type_limits.logical_xor_input));
  logical_xor->setB(
      SupportedTensorLimitsToTensorLimits(data_type_limits.logical_xor_input));
  logical_xor->setOutput(SupportedDataTypesAndRanksToTensorLimits(
      data_type_limits.logical_output,
      data_type_limits.logical_xor_input.ranks));
  op_support_limits->setLogicalXor(logical_xor);
  MLLogicalNotSupportLimits* logical_not = MLLogicalNotSupportLimits::Create();
  logical_not->setA(
      SupportedTensorLimitsToTensorLimits(data_type_limits.logical_not_input));
  logical_not->setOutput(SupportedDataTypesAndRanksToTensorLimits(
      data_type_limits.logical_output,
      data_type_limits.logical_not_input.ranks));
  op_support_limits->setLogicalNot(logical_not);
  MLLogicalNotSupportLimits* is_nan = MLLogicalNotSupportLimits::Create();
  is_nan->setA(
      SupportedTensorLimitsToTensorLimits(data_type_limits.is_nan_input));
  is_nan->setOutput(SupportedDataTypesAndRanksToTensorLimits(
      data_type_limits.logical_output, data_type_limits.is_nan_input.ranks));
  op_support_limits->setIsNaN(is_nan);
  MLLogicalNotSupportLimits* is_infinite = MLLogicalNotSupportLimits::Create();
  is_infinite->setA(
      SupportedTensorLimitsToTensorLimits(data_type_limits.is_infinite_input));
  is_infinite->setOutput(SupportedDataTypesAndRanksToTensorLimits(
      data_type_limits.logical_output,
      data_type_limits.is_infinite_input.ranks));
  op_support_limits->setIsInfinite(is_infinite);

  // Element-wise unary ops.
  MLSingleInputSupportLimits* abs = MLSingleInputSupportLimits::Create();
  abs->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.abs_input));
  abs->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.abs_input));
  op_support_limits->setAbs(abs);
  MLSingleInputSupportLimits* ceil = MLSingleInputSupportLimits::Create();
  ceil->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.ceil_input));
  ceil->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.ceil_input));
  op_support_limits->setCeil(ceil);
  MLSingleInputSupportLimits* cos = MLSingleInputSupportLimits::Create();
  cos->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.cos_input));
  cos->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.cos_input));
  op_support_limits->setCos(cos);
  MLSingleInputSupportLimits* erf = MLSingleInputSupportLimits::Create();
  erf->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.erf_input));
  erf->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.erf_input));
  op_support_limits->setErf(erf);
  MLSingleInputSupportLimits* exp = MLSingleInputSupportLimits::Create();
  exp->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.exp_input));
  exp->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.exp_input));
  op_support_limits->setExp(exp);
  MLSingleInputSupportLimits* floor = MLSingleInputSupportLimits::Create();
  floor->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.floor_input));
  floor->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.floor_input));
  op_support_limits->setFloor(floor);
  MLSingleInputSupportLimits* identity = MLSingleInputSupportLimits::Create();
  identity->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.identity_input));
  identity->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.identity_input));
  op_support_limits->setIdentity(identity);
  MLSingleInputSupportLimits* log = MLSingleInputSupportLimits::Create();
  log->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.log_input));
  log->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.log_input));
  op_support_limits->setLog(log);
  MLSingleInputSupportLimits* neg = MLSingleInputSupportLimits::Create();
  neg->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.neg_input));
  neg->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.neg_input));
  op_support_limits->setNeg(neg);
  MLSingleInputSupportLimits* reciprocal = MLSingleInputSupportLimits::Create();
  reciprocal->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.reciprocal_input));
  reciprocal->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.reciprocal_input));
  op_support_limits->setReciprocal(reciprocal);
  MLSingleInputSupportLimits* round_even = MLSingleInputSupportLimits::Create();
  round_even->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.round_even_input));
  round_even->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.round_even_input));
  op_support_limits->setRoundEven(round_even);
  MLSingleInputSupportLimits* sign = MLSingleInputSupportLimits::Create();
  sign->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.sign_input));
  sign->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.sign_input));
  op_support_limits->setSign(sign);
  MLSingleInputSupportLimits* sin = MLSingleInputSupportLimits::Create();
  sin->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.sin_input));
  sin->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.sin_input));
  op_support_limits->setSin(sin);
  MLSingleInputSupportLimits* sqrt = MLSingleInputSupportLimits::Create();
  sqrt->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.sqrt_input));
  sqrt->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.sqrt_input));
  op_support_limits->setSqrt(sqrt);
  MLSingleInputSupportLimits* tan = MLSingleInputSupportLimits::Create();
  tan->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.tan_input));
  tan->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.tan_input));
  op_support_limits->setTan(tan);

  MLSingleInputSupportLimits* elu = MLSingleInputSupportLimits::Create();
  elu->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.elu_input));
  elu->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.elu_input));
  op_support_limits->setElu(elu);

  MLSingleInputSupportLimits* expand = MLSingleInputSupportLimits::Create();
  expand->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.expand_input));
  expand->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.expand_input));
  op_support_limits->setExpand(expand);

  MLGatherSupportLimits* gather = MLGatherSupportLimits::Create();
  gather->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.gather_input));
  gather->setIndices(
      SupportedTensorLimitsToTensorLimits(data_type_limits.gather_indices));
  gather->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.gather_input));
  op_support_limits->setGather(gather);

  MLGatherSupportLimits* gather_elements = MLGatherSupportLimits::Create();
  gather_elements->setInput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.gather_elements_input));
  gather_elements->setIndices(SupportedTensorLimitsToTensorLimits(
      data_type_limits.gather_elements_indices));
  gather_elements->setOutput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.gather_elements_input));
  op_support_limits->setGatherElements(gather_elements);

  MLGatherSupportLimits* gather_nd = MLGatherSupportLimits::Create();
  gather_nd->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.gather_nd_input));
  gather_nd->setIndices(
      SupportedTensorLimitsToTensorLimits(data_type_limits.gather_nd_indices));
  gather_nd->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.gather_nd_input));
  op_support_limits->setGatherND(gather_nd);

  MLSingleInputSupportLimits* gelu = MLSingleInputSupportLimits::Create();
  gelu->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.gelu_input));
  gelu->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.gelu_input));
  op_support_limits->setGelu(gelu);

  MLGemmSupportLimits* gemm = MLGemmSupportLimits::Create();
  gemm->setA(SupportedTensorLimitsToTensorLimits(data_type_limits.gemm_a));
  gemm->setB(SupportedTensorLimitsToTensorLimits(data_type_limits.gemm_a));
  gemm->setC(SupportedTensorLimitsToTensorLimits(data_type_limits.gemm_c));
  gemm->setOutput(SupportedTensorLimitsToTensorLimits(data_type_limits.gemm_a));
  op_support_limits->setGemm(gemm);

  MLGruSupportLimits* gru = MLGruSupportLimits::Create();
  gru->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.gru_input));
  gru->setWeight(
      SupportedTensorLimitsToTensorLimits(data_type_limits.gru_input));
  gru->setRecurrentWeight(
      SupportedTensorLimitsToTensorLimits(data_type_limits.gru_input));
  gru->setBias(SupportedTensorLimitsToTensorLimits(data_type_limits.gru_bias));
  gru->setRecurrentBias(
      SupportedTensorLimitsToTensorLimits(data_type_limits.gru_bias));
  gru->setInitialHiddenState(
      SupportedTensorLimitsToTensorLimits(data_type_limits.gru_input));
  gru->setOutput0(
      SupportedTensorLimitsToTensorLimits(data_type_limits.gru_input));
  gru->setOutput1(SupportedTensorLimitsToTensorLimits(
      data_type_limits.gru_output_sequence));
  op_support_limits->setGru(gru);

  MLGruCellSupportLimits* gru_cell = MLGruCellSupportLimits::Create();
  gru_cell->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.gru_cell_input));
  gru_cell->setWeight(
      SupportedTensorLimitsToTensorLimits(data_type_limits.gru_cell_input));
  gru_cell->setRecurrentWeight(
      SupportedTensorLimitsToTensorLimits(data_type_limits.gru_cell_input));
  gru_cell->setHiddenState(
      SupportedTensorLimitsToTensorLimits(data_type_limits.gru_cell_input));
  gru_cell->setBias(
      SupportedTensorLimitsToTensorLimits(data_type_limits.gru_cell_bias));
  gru_cell->setRecurrentBias(
      SupportedTensorLimitsToTensorLimits(data_type_limits.gru_cell_bias));
  gru_cell->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.gru_cell_input));
  op_support_limits->setGruCell(gru_cell);

  MLSingleInputSupportLimits* hard_sigmoid =
      MLSingleInputSupportLimits::Create();
  hard_sigmoid->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.hard_sigmoid_input));
  hard_sigmoid->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.hard_sigmoid_input));
  op_support_limits->setHardSigmoid(hard_sigmoid);

  MLSingleInputSupportLimits* hard_swish = MLSingleInputSupportLimits::Create();
  hard_swish->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.hard_swish_input));
  hard_swish->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.hard_swish_input));
  op_support_limits->setHardSwish(hard_swish);

  MLNormalizationSupportLimits* instance_normalization =
      MLNormalizationSupportLimits::Create();
  instance_normalization->setInput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.instance_normalization_input));
  instance_normalization->setScale(SupportedTensorLimitsToTensorLimits(
      data_type_limits.instance_normalization_scale));
  instance_normalization->setBias(SupportedTensorLimitsToTensorLimits(
      data_type_limits.instance_normalization_scale));
  instance_normalization->setOutput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.instance_normalization_input));
  op_support_limits->setInstanceNormalization(instance_normalization);

  MLNormalizationSupportLimits* layer_normalization =
      MLNormalizationSupportLimits::Create();
  layer_normalization->setInput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.layer_normalization_input));
  layer_normalization->setScale(SupportedTensorLimitsToTensorLimits(
      data_type_limits.layer_normalization_input));
  layer_normalization->setBias(SupportedTensorLimitsToTensorLimits(
      data_type_limits.layer_normalization_input));
  layer_normalization->setOutput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.layer_normalization_input));
  op_support_limits->setLayerNormalization(layer_normalization);

  MLSingleInputSupportLimits* leaky_relu = MLSingleInputSupportLimits::Create();
  leaky_relu->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.leaky_relu_input));
  leaky_relu->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.leaky_relu_input));
  op_support_limits->setLeakyRelu(leaky_relu);

  MLSingleInputSupportLimits* linear = MLSingleInputSupportLimits::Create();
  linear->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.linear_input));
  linear->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.linear_input));
  op_support_limits->setLinear(linear);

  MLLstmSupportLimits* lstm = MLLstmSupportLimits::Create();
  lstm->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.lstm_input));
  lstm->setWeight(
      SupportedTensorLimitsToTensorLimits(data_type_limits.lstm_input));
  lstm->setRecurrentWeight(
      SupportedTensorLimitsToTensorLimits(data_type_limits.lstm_input));
  lstm->setBias(
      SupportedTensorLimitsToTensorLimits(data_type_limits.lstm_bias));
  lstm->setRecurrentBias(
      SupportedTensorLimitsToTensorLimits(data_type_limits.lstm_bias));
  lstm->setPeepholeWeight(
      SupportedTensorLimitsToTensorLimits(data_type_limits.lstm_bias));
  lstm->setInitialHiddenState(
      SupportedTensorLimitsToTensorLimits(data_type_limits.lstm_input));
  lstm->setInitialCellState(
      SupportedTensorLimitsToTensorLimits(data_type_limits.lstm_input));
  lstm->setOutput0(
      SupportedTensorLimitsToTensorLimits(data_type_limits.lstm_input));
  lstm->setOutput1(
      SupportedTensorLimitsToTensorLimits(data_type_limits.lstm_input));
  lstm->setOutput2(SupportedTensorLimitsToTensorLimits(
      data_type_limits.lstm_output_sequence));
  op_support_limits->setLstm(lstm);

  MLLstmCellSupportLimits* lstm_cell = MLLstmCellSupportLimits::Create();
  lstm_cell->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.lstm_cell_input));
  lstm_cell->setWeight(
      SupportedTensorLimitsToTensorLimits(data_type_limits.lstm_cell_input));
  lstm_cell->setRecurrentWeight(
      SupportedTensorLimitsToTensorLimits(data_type_limits.lstm_cell_input));
  lstm_cell->setHiddenState(
      SupportedTensorLimitsToTensorLimits(data_type_limits.lstm_cell_input));
  lstm_cell->setCellState(
      SupportedTensorLimitsToTensorLimits(data_type_limits.lstm_cell_input));
  lstm_cell->setBias(
      SupportedTensorLimitsToTensorLimits(data_type_limits.lstm_cell_bias));
  lstm_cell->setRecurrentBias(
      SupportedTensorLimitsToTensorLimits(data_type_limits.lstm_cell_bias));
  lstm_cell->setPeepholeWeight(
      SupportedTensorLimitsToTensorLimits(data_type_limits.lstm_cell_bias));
  lstm_cell->setOutput0(
      SupportedTensorLimitsToTensorLimits(data_type_limits.lstm_cell_input));
  lstm_cell->setOutput1(
      SupportedTensorLimitsToTensorLimits(data_type_limits.lstm_cell_input));
  op_support_limits->setLstmCell(lstm_cell);

  MLBinarySupportLimits* matmul = MLBinarySupportLimits::Create();
  matmul->setA(
      SupportedTensorLimitsToTensorLimits(data_type_limits.matmul_input));
  matmul->setB(
      SupportedTensorLimitsToTensorLimits(data_type_limits.matmul_input));
  matmul->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.matmul_input));
  op_support_limits->setMatmul(matmul);

  MLSingleInputSupportLimits* pad = MLSingleInputSupportLimits::Create();
  pad->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.pad_input));
  pad->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.pad_input));
  op_support_limits->setPad(pad);

  // Pool2d.
  MLSingleInputSupportLimits* average_pool2d =
      MLSingleInputSupportLimits::Create();
  average_pool2d->setInput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.average_pool2d_input));
  average_pool2d->setOutput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.average_pool2d_input));
  op_support_limits->setAveragePool2d(average_pool2d);

  MLSingleInputSupportLimits* l2_pool2d = MLSingleInputSupportLimits::Create();
  l2_pool2d->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.l2_pool2d_input));
  l2_pool2d->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.l2_pool2d_input));
  op_support_limits->setL2Pool2d(l2_pool2d);

  MLSingleInputSupportLimits* max_pool2d = MLSingleInputSupportLimits::Create();
  max_pool2d->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.max_pool2d_input));
  max_pool2d->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.max_pool2d_input));
  op_support_limits->setMaxPool2d(max_pool2d);

  MLPreluSupportLimits* prelu = MLPreluSupportLimits::Create();
  prelu->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.prelu_input));
  prelu->setSlope(
      SupportedTensorLimitsToTensorLimits(data_type_limits.prelu_input));
  prelu->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.prelu_input));
  op_support_limits->setPrelu(prelu);

  MLQuantizeDequantizeLinearSupportLimits* quantize_linear =
      MLQuantizeDequantizeLinearSupportLimits::Create();
  quantize_linear->setInput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.quantize_linear_input));
  quantize_linear->setScale(SupportedTensorLimitsToTensorLimits(
      data_type_limits.quantize_linear_input));
  quantize_linear->setZeroPoint(SupportedTensorLimitsToTensorLimits(
      data_type_limits.quantize_linear_zero_point));
  quantize_linear->setOutput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.quantize_linear_zero_point));
  op_support_limits->setQuantizeLinear(quantize_linear);

  // Reduction ops.
  MLSingleInputSupportLimits* reduce_l1 = MLSingleInputSupportLimits::Create();
  reduce_l1->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.reduce_l1_input));
  reduce_l1->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.reduce_l1_input));
  op_support_limits->setReduceL1(reduce_l1);
  MLSingleInputSupportLimits* reduce_l2 = MLSingleInputSupportLimits::Create();
  reduce_l2->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.reduce_l2_input));
  reduce_l2->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.reduce_l2_input));
  op_support_limits->setReduceL2(reduce_l2);
  MLSingleInputSupportLimits* reduce_log_sum =
      MLSingleInputSupportLimits::Create();
  reduce_log_sum->setInput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.reduce_log_sum_input));
  reduce_log_sum->setOutput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.reduce_log_sum_input));
  op_support_limits->setReduceLogSum(reduce_log_sum);
  MLSingleInputSupportLimits* reduce_log_sum_exp =
      MLSingleInputSupportLimits::Create();
  reduce_log_sum_exp->setInput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.reduce_log_sum_exp_input));
  reduce_log_sum_exp->setOutput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.reduce_log_sum_exp_input));
  op_support_limits->setReduceLogSumExp(reduce_log_sum_exp);
  MLSingleInputSupportLimits* reduce_max = MLSingleInputSupportLimits::Create();
  reduce_max->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.reduce_max_input));
  reduce_max->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.reduce_max_input));
  op_support_limits->setReduceMax(reduce_max);
  MLSingleInputSupportLimits* reduce_mean =
      MLSingleInputSupportLimits::Create();
  reduce_mean->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.reduce_mean_input));
  reduce_mean->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.reduce_mean_input));
  op_support_limits->setReduceMean(reduce_mean);
  MLSingleInputSupportLimits* reduce_min = MLSingleInputSupportLimits::Create();
  reduce_min->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.reduce_min_input));
  reduce_min->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.reduce_min_input));
  op_support_limits->setReduceMin(reduce_min);
  MLSingleInputSupportLimits* reduce_product =
      MLSingleInputSupportLimits::Create();
  reduce_product->setInput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.reduce_product_input));
  reduce_product->setOutput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.reduce_product_input));
  op_support_limits->setReduceProduct(reduce_product);
  MLSingleInputSupportLimits* reduce_sum = MLSingleInputSupportLimits::Create();
  reduce_sum->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.reduce_sum_input));
  reduce_sum->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.reduce_sum_input));
  op_support_limits->setReduceSum(reduce_sum);
  MLSingleInputSupportLimits* reduce_sum_square =
      MLSingleInputSupportLimits::Create();
  reduce_sum_square->setInput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.reduce_sum_square_input));
  reduce_sum_square->setOutput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.reduce_sum_square_input));
  op_support_limits->setReduceSumSquare(reduce_sum_square);

  MLSingleInputSupportLimits* relu = MLSingleInputSupportLimits::Create();
  relu->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.relu_input));
  relu->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.relu_input));
  op_support_limits->setRelu(relu);

  MLSingleInputSupportLimits* resample2d = MLSingleInputSupportLimits::Create();
  resample2d->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.resample2d_input));
  resample2d->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.resample2d_input));
  op_support_limits->setResample2d(resample2d);

  MLSingleInputSupportLimits* reshape = MLSingleInputSupportLimits::Create();
  reshape->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.reshape_input));
  reshape->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.reshape_input));
  op_support_limits->setReshape(reshape);

  MLSingleInputSupportLimits* reverse = MLSingleInputSupportLimits::Create();
  reverse->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.reverse_input));
  reverse->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.reverse_input));
  op_support_limits->setReverse(reverse);

  MLScatterSupportLimits* scatter_elements = MLScatterSupportLimits::Create();
  scatter_elements->setInput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.scatter_elements_input));
  scatter_elements->setIndices(SupportedTensorLimitsToTensorLimits(
      data_type_limits.scatter_elements_indices));
  scatter_elements->setUpdates(SupportedTensorLimitsToTensorLimits(
      data_type_limits.scatter_elements_input));
  scatter_elements->setOutput(SupportedTensorLimitsToTensorLimits(
      data_type_limits.scatter_elements_input));
  op_support_limits->setScatterElements(scatter_elements);

  MLScatterSupportLimits* scatter_nd = MLScatterSupportLimits::Create();
  scatter_nd->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.scatter_nd_input));
  scatter_nd->setIndices(
      SupportedTensorLimitsToTensorLimits(data_type_limits.scatter_nd_indices));
  scatter_nd->setUpdates(
      SupportedTensorLimitsToTensorLimits(data_type_limits.scatter_nd_updates));
  scatter_nd->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.scatter_nd_input));
  op_support_limits->setScatterND(scatter_nd);

  MLSingleInputSupportLimits* sigmoid = MLSingleInputSupportLimits::Create();
  sigmoid->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.sigmoid_input));
  sigmoid->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.sigmoid_input));
  op_support_limits->setSigmoid(sigmoid);

  MLSingleInputSupportLimits* slice = MLSingleInputSupportLimits::Create();
  slice->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.slice_input));
  slice->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.slice_input));
  op_support_limits->setSlice(slice);

  MLSingleInputSupportLimits* softmax = MLSingleInputSupportLimits::Create();
  softmax->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.softmax_input));
  softmax->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.softmax_input));
  op_support_limits->setSoftmax(softmax);

  MLSingleInputSupportLimits* softplus = MLSingleInputSupportLimits::Create();
  softplus->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.softplus_input));
  softplus->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.softplus_input));
  op_support_limits->setSoftplus(softplus);

  MLSingleInputSupportLimits* softsign = MLSingleInputSupportLimits::Create();
  softsign->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.softsign_input));
  softsign->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.softsign_input));
  op_support_limits->setSoftsign(softsign);

  MLSplitSupportLimits* split = MLSplitSupportLimits::Create();
  split->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.split_input));
  split->setOutputs(
      SupportedTensorLimitsToTensorLimits(data_type_limits.split_input));
  op_support_limits->setSplit(split);

  MLSingleInputSupportLimits* tanh = MLSingleInputSupportLimits::Create();
  tanh->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.tanh_input));
  tanh->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.tanh_input));
  op_support_limits->setTanh(tanh);

  MLSingleInputSupportLimits* tile = MLSingleInputSupportLimits::Create();
  tile->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.tile_input));
  tile->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.tile_input));
  op_support_limits->setTile(tile);

  MLSingleInputSupportLimits* transpose = MLSingleInputSupportLimits::Create();
  transpose->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.transpose_input));
  transpose->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.transpose_input));
  op_support_limits->setTranspose(transpose);

  MLSingleInputSupportLimits* triangular = MLSingleInputSupportLimits::Create();
  triangular->setInput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.triangular_input));
  triangular->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.triangular_input));
  op_support_limits->setTriangular(triangular);

  MLWhereSupportLimits* where = MLWhereSupportLimits::Create();
  where->setCondition(
      SupportedTensorLimitsToTensorLimits(data_type_limits.where_condition));
  where->setTrueValue(
      SupportedTensorLimitsToTensorLimits(data_type_limits.where_value));
  where->setFalseValue(
      SupportedTensorLimitsToTensorLimits(data_type_limits.where_value));
  where->setOutput(
      SupportedTensorLimitsToTensorLimits(data_type_limits.where_value));
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
  webnn::ScopedTrace scoped_trace("MLContext::createTensor");
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

  // TODO(crbug.com/345352987): use label from MLTensor if provided, instead of
  // hardcoding it here.
  constexpr char kTensorLabel[] = "tensor";

  ASSIGN_OR_RETURN(
      webnn::OperandDescriptor validated_descriptor,
      webnn::OperandDescriptor::Create(
          properties_, FromBlinkDataType(descriptor->dataType().AsEnum()),
          descriptor->shape(), kTensorLabel),
      [&exception_state](std::string error) {
        exception_state.ThrowTypeError(String(error));
        return ScriptPromise<MLTensor>();
      });

  RETURN_IF_ERROR(webnn::ValidateTensor(properties_, validated_descriptor),
                  [&exception_state](std::string error) {
                    exception_state.ThrowTypeError(String(error));
                    return ScriptPromise<MLTensor>();
                  });

  // Map the IDL tensor usage flags to the `MLTensorUsage` enumset.
  //
  // This assertion protects against the usage flags changing without updating
  // this mapping.
  static_assert(std::to_underlying(webnn::MLTensorUsageFlags::kMaxValue) == 3);
  webnn::MLTensorUsage usage;
  if (descriptor->readable()) {
    usage.Put(webnn::MLTensorUsageFlags::kRead);
  }
  if (descriptor->writable()) {
    usage.Put(webnn::MLTensorUsageFlags::kWrite);
  }
  // MLTensorUsageFlags::kGraphConstant is only assigned for
  // createConstantTensor().

  // MLTensorUsageFlags::kWebGpuInterop is only assigned for
  // createExportableTensor().

  auto tensor_info =
      webnn::mojom::blink::TensorInfo::New(validated_descriptor, usage);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<MLTensor>>(
      script_state, exception_state.GetContext());
  pending_resolvers_.insert(resolver);

  // Use `WebNNContext` to create `WebNNTensor` message pipe.
  context_remote_->CreateTensor(
      std::move(tensor_info), mojo_base::BigBuffer(0),
      blink::BindOnce(&MLContext::DidCreateWebNNTensor, WrapPersistent(this),
                      std::move(scoped_trace), WrapPersistent(resolver),
                      std::move(validated_descriptor), usage,
                      /*shared_image=*/nullptr, /*gpu_device=*/nullptr));

  return resolver->Promise();
}

ScriptPromise<MLTensor> MLContext::createExportableTensor(
    ScriptState* script_state,
    const MLTensorDescriptor* descriptor,
    GPUDevice* device,
    ExceptionState& exception_state) {
  webnn::ScopedTrace scoped_trace("MLContext::createExportableTensor");
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

  if (!device || device->IsDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid GPUDevice");
    return EmptyPromise();
  }

  // TODO(crbug.com/345352987): use label from MLTensor if provided, instead of
  // hardcoding it here.
  constexpr char kTensorLabel[] = "tensor";

  ASSIGN_OR_RETURN(
      webnn::OperandDescriptor validated_descriptor,
      webnn::OperandDescriptor::Create(
          properties_, FromBlinkDataType(descriptor->dataType().AsEnum()),
          descriptor->shape(), kTensorLabel),
      [&exception_state](std::string error) {
        exception_state.ThrowTypeError(String(error));
        return ScriptPromise<MLTensor>();
      });

  RETURN_IF_ERROR(webnn::ValidateTensor(properties_, validated_descriptor),
                  [&exception_state](std::string error) {
                    exception_state.ThrowTypeError(String(error));
                    return ScriptPromise<MLTensor>();
                  });

  // Map the IDL tensor usage flags to the `MLTensorUsage` enumset.
  //
  // This assertion protects against the usage flags changing without updating
  // this mapping.
  static_assert(std::to_underlying(webnn::MLTensorUsageFlags::kMaxValue) == 3);
  webnn::MLTensorUsage usage;
  usage.Put(webnn::MLTensorUsageFlags::kWebGpuInterop);
  if (descriptor->readable()) {
    usage.Put(webnn::MLTensorUsageFlags::kRead);
  }
  if (descriptor->writable()) {
    usage.Put(webnn::MLTensorUsageFlags::kWrite);
  }

  // MLTensorUsageFlags::kGraphConstant is only assigned for
  // createConstantTensor().

  scoped_refptr<gpu::ClientSharedImage> shared_image;
  gpu::SyncToken shared_image_create_finished_token;

  // If the context is lost, the context provider would be invalid.
  auto context_provider_wrapper =
      blink::SharedGpuContext::ContextProviderWrapper();
  if (!context_provider_wrapper ||
      context_provider_wrapper->ContextProvider().IsContextLost()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Context is lost.");
    return EmptyPromise();
  }

#if BUILDFLAG(IS_MAC)
  RETURN_IF_ERROR(IsValidTensorSize(validated_descriptor,
                                    context_provider_wrapper->ContextProvider()
                                        .GetCapabilities()
                                        .max_texture_size),
                  [&exception_state](std::string error) {
                    exception_state.ThrowTypeError(String(error));
                    return ScriptPromise<MLTensor>();
                  });
#endif  // BUILDFLAG(IS_MAC)

  gpu::SharedImageInterface* sii =
      context_provider_wrapper->ContextProvider().SharedImageInterface();
  DCHECK(sii);

  // MLTensor represents data as an N-dimensional homogeneous buffer where
  // each element has the same size—similar to textures. To represent tensors
  // created from shared images, we convert the tensor shape into a 2D image:
  // the height is the product of all dimensions except the last, which
  // becomes the width. The total size of the tensor as a shared image becomes
  // the product of its width and height. This scheme is required for CoreML
  // which validates if a MLMultiArray based MLTensor can import a shared
  // image backed CVPixelBuffer which requires the size to match the shape.
  auto format_result =
      OperandDataTypeToSharedImageFormat(validated_descriptor.data_type());
  if (!format_result.has_value()) {
    exception_state.ThrowTypeError(format_result.error());
    return EmptyPromise();
  }

  auto size_result = ShapeToSharedImageSize(validated_descriptor.shape());
  if (!size_result.has_value()) {
    exception_state.ThrowTypeError(size_result.error());
    return EmptyPromise();
  }

  shared_image = sii->CreateSharedImageForMLTensor(
      kTensorLabel, format_result.value(), size_result.value(),
      OperandUsageToSharedImageUsageSet(usage));
  CHECK(shared_image);

  shared_image_create_finished_token = sii->GenVerifiedSyncToken();

  auto tensor_info =
      webnn::mojom::blink::TensorInfo::New(validated_descriptor, usage);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<MLTensor>>(
      script_state, exception_state.GetContext());
  pending_resolvers_.insert(resolver);

  // Use `WebNNContext` to create `WebNNTensor` message pipe.
  context_remote_->CreateTensorFromMailbox(
      std::move(tensor_info), shared_image->mailbox(),
      shared_image_create_finished_token,
      blink::BindOnce(&MLContext::DidCreateWebNNTensor, WrapPersistent(this),
                      std::move(scoped_trace), WrapPersistent(resolver),
                      std::move(validated_descriptor), usage, shared_image,
                      WrapPersistent(device)));

  return resolver->Promise();
}

ScriptPromise<MLTensor> MLContext::createConstantTensor(
    ScriptState* script_state,
    const MLOperandDescriptor* descriptor,
    AllowSharedBufferSource* src_data,
    ExceptionState& exception_state) {
  webnn::ScopedTrace scoped_trace("MLContext::createConstantTensor");
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
      webnn::OperandDescriptor validated_descriptor,
      webnn::OperandDescriptor::Create(
          properties_, FromBlinkDataType(descriptor->dataType().AsEnum()),
          descriptor->shape(), "constant_tensor"),
      [&exception_state](std::string error) {
        exception_state.ThrowTypeError(String(error));
        return ScriptPromise<MLTensor>();
      });

  RETURN_IF_ERROR(webnn::ValidateTensor(properties_, validated_descriptor),
                  [&exception_state](std::string error) {
                    exception_state.ThrowTypeError(String(error));
                    return ScriptPromise<MLTensor>();
                  });

  base::span<const uint8_t> bytes = AsByteSpan(*src_data);
  if (validated_descriptor.PackedByteLength() != bytes.size()) {
    exception_state.ThrowTypeError(
        String::Format("The source data byte length (%zu) doesn't match the "
                       "expected byte length (%zu).",
                       bytes.size(), validated_descriptor.PackedByteLength()));
    return ScriptPromise<MLTensor>();
  }

  if (!properties_.data_type_limits.constant.Supports(validated_descriptor)) {
    exception_state.ThrowTypeError(String(webnn::NotSupportedConstantError(
        validated_descriptor, properties_.data_type_limits.constant)));
    return ScriptPromise<MLTensor>();
  }

  webnn::MLTensorUsage usage =
      webnn::MLTensorUsage{webnn::MLTensorUsageFlags::kGraphConstant};
  auto tensor_info =
      webnn::mojom::blink::TensorInfo::New(validated_descriptor, usage);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<MLTensor>>(
      script_state, exception_state.GetContext());
  pending_resolvers_.insert(resolver);

  // Use `WebNNContext` to create `WebNNTensor` message pipe.
  context_remote_->CreateTensor(
      std::move(tensor_info), bytes,
      blink::BindOnce(&MLContext::DidCreateWebNNTensor, WrapPersistent(this),
                      std::move(scoped_trace), WrapPersistent(resolver),
                      std::move(validated_descriptor), usage,
                      /*shared_image=*/nullptr, /*gpu_device=*/nullptr));

  return resolver->Promise();
}

void MLContext::writeTensor(ScriptState* script_state,
                            MLTensor* dst_tensor,
                            AllowSharedBufferSource* src_data,
                            ExceptionState& exception_state) {
  webnn::ScopedTrace scoped_trace("MLContext::writeTensor");
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return;
  }

  if (dst_tensor->context() != this) {
    exception_state.ThrowTypeError(
        "The destination tensor wasn't created with this context.");
    return;
  }

  if (!dst_tensor->Usage().Has(webnn::MLTensorUsageFlags::kWrite)) {
    exception_state.ThrowTypeError(
        "The destination tensor doesn't have write access.");
    return;
  }

  // TODO(crbug.com/378604909): When `src_data` is an ArrayBufferView, check its
  // element type being compatible with the MLTensor data type.

  base::span<const uint8_t> bytes = AsByteSpan(*src_data);
  if (bytes.size() != dst_tensor->PackedByteLength()) {
    exception_state.ThrowTypeError(
        "The sizes of the source buffer and destination tensor do not match.");
    return;
  }

  dst_tensor->WriteTensorImpl(bytes, exception_state);
}

ScriptPromise<DOMArrayBuffer> MLContext::readTensor(
    ScriptState* script_state,
    MLTensor* src_tensor,
    ExceptionState& exception_state) {
  webnn::ScopedTrace scoped_trace("MLContext::readTensor");
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return EmptyPromise();
  }

  if (src_tensor->context() != this) {
    exception_state.ThrowTypeError(
        "The source tensor wasn't created with this context.");
    return EmptyPromise();
  }

  if (!src_tensor->Usage().Has(webnn::MLTensorUsageFlags::kRead)) {
    exception_state.ThrowTypeError(
        "The source tensor doesn't have read access.");
    return EmptyPromise();
  }

  return src_tensor->ReadTensorImpl(std::move(scoped_trace), script_state,
                                    exception_state);
}

ScriptPromise<IDLUndefined> MLContext::readTensor(
    ScriptState* script_state,
    MLTensor* src_tensor,
    AllowSharedBufferSource* dst_data,
    ExceptionState& exception_state) {
  webnn::ScopedTrace scoped_trace("MLContext::readTensor");
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return EmptyPromise();
  }

  if (src_tensor->context() != this) {
    exception_state.ThrowTypeError(
        "The source tensor wasn't created with this context.");
    return EmptyPromise();
  }

  // TODO(crbug.com/378604909): When `dst_data` is an ArrayBufferView, check its
  // element type being compatible with the MLTensor data type.

  return src_tensor->ReadTensorImpl(std::move(scoped_trace), script_state,
                                    dst_data, exception_state);
}

void MLContext::dispatch(ScriptState* script_state,
                         MLGraph* graph,
                         const MLNamedTensors& inputs,
                         const MLNamedTensors& outputs,
                         ExceptionState& exception_state) {
  webnn::ScopedTrace scoped_trace("MLContext::dispatch");
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
    webnn::ScopedTrace scoped_trace,
    ScriptPromiseResolver<blink::MLTensor>* resolver,
    webnn::OperandDescriptor validated_descriptor,
    webnn::MLTensorUsage usage,
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    GPUDevice* gpu_device,
    webnn::mojom::blink::CreateTensorResultPtr result) {
  pending_resolvers_.erase(resolver);

  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid()) {
    return;
  }

  if (result->is_error()) {
    const auto& create_tensor_error = result->get_error();
    resolver->RejectWithDOMException(
        WebNNErrorCodeToDOMExceptionCode(create_tensor_error->code),
        create_tensor_error->message);
    return;
  }

  auto* tensor = MakeGarbageCollected<MLTensor>(
      resolver->GetExecutionContext(), this, std::move(validated_descriptor),
      usage, std::move(shared_image), gpu_device,
      std::move(result->get_success()), base::PassKey<MLContext>());
  tensors_.insert(tensor);

  resolver->Resolve(tensor);
}

ScriptPromise<GPUBuffer> MLContext::exportToGPU(
    ScriptState* script_state,
    MLTensor* tensor,
    ExceptionState& exception_state) {
  webnn::ScopedTrace scoped_trace("MLContext::exportToGPU");
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return EmptyPromise();
  }
  if (!context_remote_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Context is lost.");
    return EmptyPromise();
  }
  if (tensor->context() != this) {
    exception_state.ThrowTypeError(
        "The source tensor was not created by this context.");
    return EmptyPromise();
  }

  return tensor->ExportToGPUImpl(std::move(scoped_trace), script_state,
                                 exception_state);
}

}  // namespace blink
