// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/layout_transformer.h"

#include <numeric>

#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_batch_normalization_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_transpose_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_instance_normalization_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pool_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_resample_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_transpose_options.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

namespace blink_mojom = webnn::mojom::blink;
namespace blink {
namespace {
webnn::InputOperandLayout BlinkInputOperandLayoutToNative(
    blink::V8MLInputOperandLayout::Enum type) {
  switch (type) {
    case blink::V8MLInputOperandLayout::Enum::kNchw:
      return webnn::InputOperandLayout::kNchw;
    case blink::V8MLInputOperandLayout::Enum::kNhwc:
      return webnn::InputOperandLayout::kNhwc;
  }
}

constexpr std::array<uint32_t, 4> kNchwToNhwcPermutation = {0u, 2u, 3u, 1u};
constexpr std::array<uint32_t, 4> kNhwcToNchwPermutation = {0u, 3u, 1u, 2u};

std::optional<base::span<const uint32_t>> GetInputOperandPermutation(
    blink::V8MLInputOperandLayout::Enum input_layout,
    const webnn::ContextProperties& context_properties) {
  if (BlinkInputOperandLayoutToNative(input_layout) ==
      context_properties.input_operand_layout) {
    return std::nullopt;
  }
  switch (input_layout) {
    case blink::V8MLInputOperandLayout::Enum::kNchw:
      CHECK_EQ(context_properties.input_operand_layout,
               webnn::InputOperandLayout::kNhwc);
      return kNchwToNhwcPermutation;
    case blink::V8MLInputOperandLayout::Enum::kNhwc:
      CHECK_EQ(context_properties.input_operand_layout,
               webnn::InputOperandLayout::kNchw);
      return kNhwcToNchwPermutation;
  }
}

std::optional<base::span<const uint32_t>> GetOutputOperandPermutation(
    blink::V8MLInputOperandLayout::Enum input_layout,
    const webnn::ContextProperties& context_properties) {
  if (BlinkInputOperandLayoutToNative(input_layout) ==
      context_properties.input_operand_layout) {
    return std::nullopt;
  }
  // The output layout is the same as the input layout and so the output
  // needs to have the inverse of the permutation returned by
  // `GetInputOperandPermutation()` applied.
  switch (input_layout) {
    case blink::V8MLInputOperandLayout::Enum::kNchw:
      CHECK_EQ(context_properties.input_operand_layout,
               webnn::InputOperandLayout::kNhwc);
      return kNhwcToNchwPermutation;
    case blink::V8MLInputOperandLayout::Enum::kNhwc:
      CHECK_EQ(context_properties.input_operand_layout,
               webnn::InputOperandLayout::kNchw);
      return kNchwToNhwcPermutation;
  }
}

std::optional<std::array<uint32_t, 4>> GetConv2DFilterPermutation(
    webnn::InputOperandLayout input_layout,
    bool depthwise,
    blink::V8MLConv2dFilterOperandLayout filter_layout) {
  switch (input_layout) {
    case webnn::InputOperandLayout::kNchw:
      // Mojo expects the OIHW layout.
      switch (filter_layout.AsEnum()) {
        case blink::V8MLConv2dFilterOperandLayout::Enum::kOihw:
          return std::nullopt;
        case blink::V8MLConv2dFilterOperandLayout::Enum::kHwio:
          return std::to_array<uint32_t>({3u, 2u, 0u, 1u});
        case blink::V8MLConv2dFilterOperandLayout::Enum::kOhwi:
          return std::to_array<uint32_t>({0u, 3u, 1u, 2u});
        case blink::V8MLConv2dFilterOperandLayout::Enum::kIhwo:
          return std::to_array<uint32_t>({3u, 0u, 1u, 2u});
      }
      break;
    case webnn::InputOperandLayout::kNhwc:
      if (depthwise) {
        // Mojo expects the IHWO layout.
        switch (filter_layout.AsEnum()) {
          case blink::V8MLConv2dFilterOperandLayout::Enum::kOihw:
            return std::to_array<uint32_t>({1u, 2u, 3u, 0u});
          case blink::V8MLConv2dFilterOperandLayout::Enum::kHwio:
            return std::to_array<uint32_t>({2u, 0u, 1u, 3u});
          case blink::V8MLConv2dFilterOperandLayout::Enum::kOhwi:
            return std::to_array<uint32_t>({3u, 1u, 2u, 0u});
          case blink::V8MLConv2dFilterOperandLayout::Enum::kIhwo:
            return std::nullopt;
        }
      } else {
        switch (filter_layout.AsEnum()) {
          // Mojo expects the OHWI layout.
          case blink::V8MLConv2dFilterOperandLayout::Enum::kOihw:
            return std::to_array<uint32_t>({0u, 2u, 3u, 1u});
          case blink::V8MLConv2dFilterOperandLayout::Enum::kHwio:
            return std::to_array<uint32_t>({3u, 0u, 1u, 2u});
          case blink::V8MLConv2dFilterOperandLayout::Enum::kOhwi:
            return std::nullopt;
          case blink::V8MLConv2dFilterOperandLayout::Enum::kIhwo:
            return std::to_array<uint32_t>({3u, 1u, 2u, 0u});
        }
      }
      break;
  }
}

std::optional<std::array<uint32_t, 4>> GetConvTranspose2DFilterPermutation(
    webnn::InputOperandLayout input_layout,
    blink::V8MLConvTranspose2dFilterOperandLayout filter_layout) {
  switch (input_layout) {
    case webnn::InputOperandLayout::kNchw:
      // Mojo expects IOHW layout.
      switch (filter_layout.AsEnum()) {
        case blink::V8MLConvTranspose2dFilterOperandLayout::Enum::kIohw:
          return std::nullopt;
        case blink::V8MLConvTranspose2dFilterOperandLayout::Enum::kHwoi:
          return std::to_array<uint32_t>({3, 2, 0, 1});
        case blink::V8MLConvTranspose2dFilterOperandLayout::Enum::kOhwi:
          return std::to_array<uint32_t>({3u, 0u, 1u, 2u});
      }
      break;
    case webnn::InputOperandLayout::kNhwc:
      // Mojo expects OHWI layout.
      switch (filter_layout.AsEnum()) {
        case blink::V8MLConvTranspose2dFilterOperandLayout::Enum::kIohw:
          return std::to_array<uint32_t>({1u, 2u, 3u, 0u});
        case blink::V8MLConvTranspose2dFilterOperandLayout::Enum::kHwoi:
          return std::to_array<uint32_t>({2u, 0u, 1u, 3u});
        case blink::V8MLConvTranspose2dFilterOperandLayout::Enum::kOhwi:
          return std::nullopt;
      }
      break;
  }
}

template <typename MLConv2dOptionsType>
void UpdateConv2dInputLayout(webnn::ContextProperties context_properties,
                             MLConv2dOptionsType* options) {
  switch (context_properties.input_operand_layout) {
    case webnn::InputOperandLayout::kNchw:
      options->setInputLayout(blink::V8MLInputOperandLayout::Enum::kNchw);
      break;
    case webnn::InputOperandLayout::kNhwc:
      options->setInputLayout(blink::V8MLInputOperandLayout::Enum::kNhwc);
      break;
  }
}

void UpdateConv2dFilterLayout(webnn::ContextProperties context_properties,
                              MLConv2dOptions* options,
                              bool depthwise) {
  webnn::InputOperandLayout input_layout =
      context_properties.input_operand_layout;
  switch (input_layout) {
    case webnn::InputOperandLayout::kNchw:
      options->setFilterLayout(
          blink::V8MLConv2dFilterOperandLayout::Enum::kOihw);
      break;
    case webnn::InputOperandLayout::kNhwc:
      if (depthwise) {
        options->setFilterLayout(
            blink::V8MLConv2dFilterOperandLayout::Enum::kIhwo);
      } else {
        options->setFilterLayout(
            blink::V8MLConv2dFilterOperandLayout::Enum::kOhwi);
      }
      break;
  }
}

void UpdateInstanceNormalizationInputLayout(
    webnn::ContextProperties context_properties,
    MLInstanceNormalizationOptions* options) {
  webnn::InputOperandLayout input_layout =
      context_properties.input_operand_layout;
  switch (input_layout) {
    case webnn::InputOperandLayout::kNchw:
      options->setLayout(blink::V8MLInputOperandLayout::Enum::kNchw);
      break;
    case webnn::InputOperandLayout::kNhwc:
      options->setLayout(blink::V8MLInputOperandLayout::Enum::kNhwc);
      break;
  }
}

void UpdateConvTranspose2dFilterLayout(
    webnn::ContextProperties context_properties,
    MLConvTranspose2dOptions* options) {
  webnn::InputOperandLayout input_layout =
      context_properties.input_operand_layout;
  switch (input_layout) {
    case webnn::InputOperandLayout::kNchw:
      options->setFilterLayout(
          blink::V8MLConvTranspose2dFilterOperandLayout::Enum::kIohw);
      break;
    case webnn::InputOperandLayout::kNhwc:
      options->setFilterLayout(
          blink::V8MLConvTranspose2dFilterOperandLayout::Enum::kOhwi);
      break;
  }
}

void UpdatePool2dInputLayout(webnn::ContextProperties context_properties,
                             MLPool2dOptions* options) {
  switch (context_properties.input_operand_layout) {
    case webnn::InputOperandLayout::kNchw:
      options->setLayout(blink::V8MLInputOperandLayout::Enum::kNchw);
      break;
    case webnn::InputOperandLayout::kNhwc:
      options->setLayout(blink::V8MLInputOperandLayout::Enum::kNhwc);
      break;
  }
}

bool IsDepthwiseConv2d(const MLOperator* conv2d) {
  const auto* options = static_cast<const MLConv2dOptions*>(conv2d->Options());
  CHECK(options);
  const MLOperand* input = conv2d->PositionalInputs()[0];
  CHECK(input);
  const std::vector<uint32_t>& input_shape = input->Shape();
  CHECK_EQ(input_shape.size(), 4u);
  const MLOperand* output = conv2d->Outputs()[0].Get();
  CHECK(output);
  const std::vector<uint32_t>& output_shape = output->Shape();
  CHECK_EQ(output_shape.size(), 4u);
  uint32_t input_channels, output_channels;
  switch (options->inputLayout().AsEnum()) {
    case blink::V8MLInputOperandLayout::Enum::kNchw:
      input_channels = input_shape[1];
      output_channels = output_shape[1];
      break;
    case blink::V8MLInputOperandLayout::Enum::kNhwc:
      input_channels = input_shape[3];
      output_channels = output_shape[3];
      break;
  }
  const uint32_t groups = base::checked_cast<uint32_t>(options->groups());
  return webnn::IsDepthwiseConv2d(input_channels, output_channels, groups);
}

Vector<uint32_t> PermuteShape(base::span<const uint32_t> shape,
                              base::span<const uint32_t> permutation) {
  OperandIndex shape_size = base::checked_cast<OperandIndex>(shape.size());
  Vector<uint32_t> permuted_array(shape_size);
  CHECK_EQ(shape_size, permutation.size());
  for (OperandIndex i = 0; i < shape_size; ++i) {
    permuted_array[i] = shape[permutation[i]];
  }
  return permuted_array;
}
constexpr std::array<uint32_t, 2> kResample2dChannelFirstAxes{2u, 3u};
constexpr std::array<uint32_t, 2> kResample2dChannelLastAxes{1u, 2u};
std::optional<std::vector<uint32_t>> GetResample2DPermutation(
    const Vector<uint32_t>& from_axes,
    const webnn::ContextProperties& context_properties) {
  if (context_properties.resample_2d_axes == webnn::Resample2DAxes::kAny) {
    return std::nullopt;
  }
  base::span<const uint32_t> to_axes =
      context_properties.resample_2d_axes ==
              webnn::Resample2DAxes::kChannelsFirst
          ? kResample2dChannelFirstAxes
          : kResample2dChannelLastAxes;
  CHECK_EQ(from_axes.size(), 2u);
  CHECK(std::ranges::is_sorted(from_axes));
  if (from_axes == to_axes) {
    return std::nullopt;
  }
  std::vector<uint32_t> permutation{0u, 1u, 2u, 3u};
  // Move each axis from from_axes to to_axes.
  for (size_t i = 0; i < from_axes.size(); ++i) {
    uint32_t from_axis = from_axes[static_cast<OperandIndex>(i)];
    uint32_t to_axis = to_axes[i];
    // Find the current index of the from_axis as it could have been moved from
    // previous iteration.
    auto it = std::ranges::find(permutation, from_axis);
    CHECK(it != permutation.end());
    size_t from_axis_index = std::distance(permutation.begin(), it);
    std::swap(permutation[to_axis], permutation[from_axis_index]);
  }
  return permutation;
}

std::vector<uint32_t> GetInversePermutation(
    base::span<const uint32_t> permutation) {
  std::vector<uint32_t> inverse_perm(permutation.size());
  for (size_t i = 0; i < permutation.size(); ++i) {
    CHECK(permutation[i] < inverse_perm.size());
    inverse_perm[permutation[i]] = base::checked_cast<uint32_t>(i);
  }
  return inverse_perm;
}

constexpr uint32_t kBatchNormalizationChannelFirstAxis = 1u;

std::optional<std::vector<uint32_t>> GetBatchNormalizationPermutation(
    const uint32_t from_axis,
    const uint32_t input_rank,
    const webnn::ContextProperties& context_properties) {
  if (input_rank < 2) {
    CHECK_EQ(input_rank, 1u);
    // It's unnecessary to transform the layout for 1D input.
    return std::nullopt;
  }
  if (context_properties.batch_normalization_axis ==
      webnn::BatchNormalizationAxis::kAny) {
    return std::nullopt;
  }
  const uint32_t to_axis = kBatchNormalizationChannelFirstAxis;
  if (from_axis == to_axis) {
    return std::nullopt;
  }
  std::vector<uint32_t> permutation(input_rank);
  std::iota(permutation.begin(), permutation.end(), 0);
  CHECK_LT(to_axis, input_rank);
  std::swap(permutation[to_axis], permutation[from_axis]);
  return permutation;
}
}  // namespace

void LayoutTransformer::Transform(MLNamedOperands& named_outputs) {
  HeapVector<Member<MLOperator>> sorted_operators =
      GetOperatorsInTopologicalOrder(named_outputs);
  HeapHashSet<Member<const MLOperator>> graph_output_operators;
  for (auto& named_output : named_outputs) {
    auto* output_operand = named_output.second.Get();
    graph_output_operators.insert(output_operand->Operator());
  }
  for (auto& op : sorted_operators) {
    MLOperand* original_output_operand = op->Outputs()[0].Get();
    MLOperand* updated_output_operand = original_output_operand;
    switch (op->Kind()) {
      case webnn::mojom::internal::Operation_Data::Operation_Tag::kConv2d: {
        switch (op->SubKind<blink_mojom::Conv2d::Kind>()) {
          case blink_mojom::Conv2d::Kind::kDirect: {
            updated_output_operand = HandleConv2d<MLConv2dOptions>(op.Get());
            break;
          }
          case blink_mojom::Conv2d::Kind::kTransposed: {
            updated_output_operand =
                HandleConv2d<MLConvTranspose2dOptions>(op.Get());
            break;
          }
        }
        break;
      }
      case webnn::mojom::internal::Operation_Data::Operation_Tag::kResample2d: {
        updated_output_operand = HandleResample2d(op.Get());
        break;
      }
      case webnn::mojom::internal::Operation_Data::Operation_Tag::
          kBatchNormalization: {
        updated_output_operand = HandleBatchNormalization(op.Get());
        break;
      }
      case webnn::mojom::internal::Operation_Data::Operation_Tag::
          kInstanceNormalization: {
        updated_output_operand = HandleInstanceNormalization(op.Get());
        break;
      }
      case webnn::mojom::internal::Operation_Data::Operation_Tag::kPool2d: {
        updated_output_operand = HandlePool2d(op.Get());
        break;
      }
      default:
        break;
    }
    // The handled operator is a graph output, update named_outputs.
    if (updated_output_operand != original_output_operand &&
        graph_output_operators.Contains(op)) {
      for (auto& named_output : named_outputs) {
        if (named_output.second.Get() == original_output_operand) {
          named_output.second = updated_output_operand;
        }
      }
    }
  }
}

void LayoutTransformer::InsertInputTranspose(
    MLOperator* op,
    OperandIndex positional_input_index,
    base::span<const uint32_t> permutation,
    String label,
    ExceptionState& exception_state) {
  auto* input_operand = op->PositionalInputs()[positional_input_index].Get();
  MLTransposeOptions* transpose_options = MLTransposeOptions::Create();
  transpose_options->setPermutation(Vector<uint32_t>(permutation));
  transpose_options->setLabel(label);
  auto* transpose_operand = graph_builder_->transpose(
      input_operand, transpose_options, exception_state);
  SwapInput(op, positional_input_index, transpose_operand);
}

void LayoutTransformer::PermuteOperandShape(
    MLOperand* operand,
    base::span<const uint32_t> permutation) {
  // Update output operand shape, but cannot directly update inplace
  // because it's visible for JS. We should create a new MLOperand instead.
  auto new_output_shape = PermuteShape(operand->Shape(), permutation);
  ReplaceOperandWithNewShape(operand, new_output_shape);
}

MLOperand* LayoutTransformer::InsertOutputTranspose(
    MLOperator* op,
    OperandIndex output_index,
    base::span<const uint32_t> permutation,
    String label,
    ExceptionState& exception_state) {
  auto output_ops = op->Outputs()[output_index]->DependentOperators();

  HeapVector<Member<MLOperator>> output_ops_set(output_ops);

  MLTransposeOptions* transpose_options = MLTransposeOptions::Create();
  transpose_options->setPermutation(Vector<uint32_t>(permutation));
  transpose_options->setLabel(label);
  auto* transpose_output_operand = graph_builder_->transpose(
      op->Outputs()[output_index], transpose_options, exception_state);

  for (auto& output_op : output_ops_set) {
    SwapInput(output_op.Get(), op->Outputs()[output_index].Get(),
              transpose_output_operand);
  }

  return transpose_output_operand;
}

template <typename MLConv2dOptionsType>
MLOperand* LayoutTransformer::HandleConv2d(MLOperator* conv2d) {
  CHECK_EQ(conv2d->Kind(), webnn::mojom::blink::Operation::Tag::kConv2d);
  auto* options = static_cast<MLConv2dOptionsType*>(conv2d->Options());
  CHECK(options);
  webnn::ContextProperties context_properties =
      graph_builder_->GetContext()->GetProperties();
  ExceptionState exception_state = GetExceptionState();

  auto original_layout = options->inputLayout().AsEnum();

  const std::optional<base::span<const uint32_t>> input_permutation =
      GetInputOperandPermutation(original_layout, context_properties);
  std::optional<std::array<uint32_t, 4>> filter_permutation;
  bool depthwise;
  if constexpr (std::is_same<MLConv2dOptionsType, MLConv2dOptions>::value) {
    depthwise = IsDepthwiseConv2d(conv2d);
    filter_permutation =
        GetConv2DFilterPermutation(context_properties.input_operand_layout,
                                   depthwise, options->filterLayout());
  } else if constexpr (std::is_same<MLConv2dOptionsType,
                                    MLConvTranspose2dOptions>::value) {
    filter_permutation = GetConvTranspose2DFilterPermutation(
        context_properties.input_operand_layout, options->filterLayout());
  } else {
    NOTREACHED();
  }

  if (input_permutation) {
    InsertInputTranspose(conv2d, /*positional_input_index=*/0,
                         *input_permutation, options->label(), exception_state);
    PermuteOperandShape(conv2d->Outputs()[0], *input_permutation);
    UpdateConv2dInputLayout(context_properties, options);
  }

  if (filter_permutation) {
    InsertInputTranspose(conv2d, /*positional_input_index=*/1,
                         *filter_permutation, options->label(),
                         exception_state);
    if constexpr (std::is_same<MLConv2dOptionsType, MLConv2dOptions>::value) {
      UpdateConv2dFilterLayout(context_properties, options, depthwise);
    } else if constexpr (std::is_same<MLConv2dOptionsType,
                                      MLConvTranspose2dOptions>::value) {
      UpdateConvTranspose2dFilterLayout(context_properties, options);
    } else {
      NOTREACHED();
    }
  }
  const std::optional<base::span<const uint32_t>> output_permutation =
      GetOutputOperandPermutation(original_layout, context_properties);

  auto* conv2d_output_operand = conv2d->Outputs()[0].Get();
  if (output_permutation) {
    conv2d_output_operand =
        InsertOutputTranspose(conv2d, /*output_index=*/0, *output_permutation,
                              options->label(), exception_state);
  }
  return conv2d_output_operand;
}

template MLOperand* LayoutTransformer::HandleConv2d<MLConv2dOptions>(
    MLOperator* conv2d);

template MLOperand* LayoutTransformer::HandleConv2d<MLConvTranspose2dOptions>(
    MLOperator* conv2d);

MLOperand* LayoutTransformer::HandleResample2d(MLOperator* resample2d) {
  CHECK_EQ(resample2d->Kind(),
           webnn::mojom::blink::Operation::Tag::kResample2d);
  auto* options = static_cast<MLResample2dOptions*>(resample2d->Options());
  CHECK(options);
  webnn::ContextProperties context_properties =
      graph_builder_->GetContext()->GetProperties();
  ExceptionState exception_state = GetExceptionState();
  // If axes are not present, the values are assumed to be channels first [2,
  // 3].
  auto axes = options->getAxesOr(
      {kResample2dChannelFirstAxes[0], kResample2dChannelFirstAxes[1]});
  CHECK_EQ(axes.size(), 2u);
  // When the target sizes are specified, the scales argument is ignored.
  if (!options->hasSizes()) {
    // If scales are not present, the values are assumed to be [1.0, 1.0].
    auto scales = options->getScalesOr({1.0, 1.0});
    CHECK_EQ(scales.size(), 2u);
    // If axes are not sorted, and backends are expecting sorted axes, sort the
    // corresponding scales too.
    if (context_properties.resample_2d_axes != webnn::Resample2DAxes::kAny &&
        axes[0] > axes[1]) {
      std::swap(scales[0], scales[1]);
    }
    options->setScales(scales);
  }
  std::ranges::sort(axes);
  const std::optional<std::vector<uint32_t>> input_permutation =
      GetResample2DPermutation(axes, context_properties);
  auto output_operand = resample2d->Outputs()[0];
  // Insert input transpose if needed.
  if (input_permutation) {
    switch (context_properties.resample_2d_axes) {
      case webnn::Resample2DAxes::kChannelsFirst:
        axes = {kResample2dChannelFirstAxes[0], kResample2dChannelFirstAxes[1]};
        break;
      case webnn::Resample2DAxes::kChannelsLast:
        axes = {kResample2dChannelLastAxes[0], kResample2dChannelLastAxes[1]};
        break;
      case webnn::Resample2DAxes::kAny:
        NOTREACHED();
    }
    InsertInputTranspose(resample2d, /*positional_input_index=*/0,
                         *input_permutation, options->label(), exception_state);
    PermuteOperandShape(resample2d->Outputs()[0], *input_permutation);
    // Insert output transpose.
    std::vector<uint32_t> output_permutation =
        GetInversePermutation(*input_permutation);
    output_operand = InsertOutputTranspose(resample2d, /*output_index=*/0,
                                           output_permutation, options->label(),
                                           exception_state);
  }
  // Update option axes
  options->setAxes(Vector<uint32_t>(axes));
  return output_operand;
}

MLOperand* LayoutTransformer::HandleBatchNormalization(MLOperator* batch_norm) {
  CHECK_EQ(batch_norm->Kind(),
           webnn::mojom::blink::Operation::Tag::kBatchNormalization);
  auto* options =
      static_cast<MLBatchNormalizationOptions*>(batch_norm->Options());
  CHECK(options);
  webnn::ContextProperties context_properties =
      graph_builder_->GetContext()->GetProperties();
  ExceptionState exception_state = GetExceptionState();
  const MLOperand* input_operand = batch_norm->PositionalInputs()[0];
  uint32_t axis = options->axis();
  const std::optional<std::vector<uint32_t>> input_permutation =
      GetBatchNormalizationPermutation(axis, input_operand->shape().size(),
                                       context_properties);
  MLOperand* output_operand = batch_norm->Outputs()[0].Get();
  // Insert input transpose if needed.
  if (input_permutation) {
    switch (context_properties.batch_normalization_axis) {
      case webnn::BatchNormalizationAxis::kChannelsFirst: {
        axis = kBatchNormalizationChannelFirstAxis;
        options->setAxis(axis);
        break;
      }
      case webnn::BatchNormalizationAxis::kAny:
        NOTREACHED();
    }
    InsertInputTranspose(batch_norm, /*positional_input_index=*/0,
                         *input_permutation, options->label(), exception_state);
    PermuteOperandShape(batch_norm->Outputs()[0], *input_permutation);
    // Insert output transpose.
    std::vector<uint32_t> output_permutation =
        GetInversePermutation(*input_permutation);
    output_operand = InsertOutputTranspose(batch_norm, /*output_index=*/0,
                                           output_permutation, options->label(),
                                           exception_state);
  }
  return output_operand;
}

MLOperand* LayoutTransformer::HandleInstanceNormalization(
    MLOperator* instance_norm) {
  CHECK_EQ(instance_norm->Kind(),
           webnn::mojom::blink::Operation::Tag::kInstanceNormalization);
  auto* options =
      static_cast<MLInstanceNormalizationOptions*>(instance_norm->Options());
  CHECK(options);
  webnn::ContextProperties context_properties =
      graph_builder_->GetContext()->GetProperties();
  ExceptionState exception_state = GetExceptionState();

  auto original_layout = options->layout().AsEnum();

  const std::optional<base::span<const uint32_t>> input_permutation =
      GetInputOperandPermutation(original_layout, context_properties);
  MLOperand* output_operand = instance_norm->Outputs()[0].Get();
  // Insert input transpose if needed.
  if (input_permutation) {
    InsertInputTranspose(instance_norm, /*positional_input_index=*/0,
                         *input_permutation, options->label(), exception_state);
    PermuteOperandShape(instance_norm->Outputs()[0], *input_permutation);

    UpdateInstanceNormalizationInputLayout(context_properties, options);
  }
  // Insert output transpose if needed.
  const std::optional<base::span<const uint32_t>> output_permutation =
      GetOutputOperandPermutation(original_layout, context_properties);
  if (output_permutation) {
    output_operand = InsertOutputTranspose(instance_norm, /*output_index=*/0,
                                           *output_permutation,
                                           options->label(), exception_state);
  }
  return output_operand;
}

MLOperand* LayoutTransformer::HandlePool2d(MLOperator* pool2d) {
  CHECK_EQ(pool2d->Kind(), webnn::mojom::blink::Operation::Tag::kPool2d);
  auto* options = static_cast<MLPool2dOptions*>(pool2d->Options());
  CHECK(options);
  webnn::ContextProperties context_properties =
      graph_builder_->GetContext()->GetProperties();
  ExceptionState exception_state = GetExceptionState();

  auto original_layout = options->layout().AsEnum();

  const std::optional<base::span<const uint32_t>> input_permutation =
      GetInputOperandPermutation(original_layout, context_properties);
  MLOperand* output_operand = pool2d->Outputs()[0].Get();
  if (input_permutation) {
    InsertInputTranspose(pool2d, /*positional_input_index=*/0,
                         *input_permutation, options->label(), exception_state);
    PermuteOperandShape(pool2d->Outputs()[0], *input_permutation);

    UpdatePool2dInputLayout(context_properties, options);
  }

  const std::optional<base::span<const uint32_t>> output_permutation =
      GetOutputOperandPermutation(original_layout, context_properties);
  if (output_permutation) {
    output_operand =
        InsertOutputTranspose(pool2d, /*output_index=*/0, *output_permutation,
                              options->label(), exception_state);
  }
  return output_operand;
}
}  // namespace blink
