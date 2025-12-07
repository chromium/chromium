// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"

#include <numeric>

#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gemm_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"

namespace blink {

HeapVector<Member<MLOperator>> GetOperatorsInTopologicalOrder(
    const MLNamedOperands& named_outputs) {
  // A WebNN graph is represented by a directed acyclic graph (DAG) that has
  // operators as vertices and operand as edges. The topological sorting is
  // implemented by depth-first search (DFS) and visiting vertices in
  // post-order. It means a vertex (operator) is visited (pushed to the back of
  // the sorted list) after all its dependent vertices (operators) are visited.
  // With that, it ensures operator 'j' appears before operator 'i' in the
  // result, if 'i' depends on 'j'. The DFS algorithm is based on the
  // non-recursive implementation of:
  // https://en.wikipedia.org/wiki/Depth-first_search

  // The topologically sorted operators.
  HeapVector<Member<MLOperator>> toposorted_operators;

  // The to-visit stack and visited set for DFS graph traversal.
  HeapDeque<Member<MLOperator>> operators_to_visit;
  HeapHashSet<Member<MLOperator>> visited_operators;
  // Enumerate output operands and initialize the to-visit stack with their
  // dependent operators.
  for (const auto& output : named_outputs) {
    const auto* operand = output.second.Get();
    operators_to_visit.push_back(operand->Operator());
  }
  while (operators_to_visit.size() > 0) {
    // Get the current operator from the top of the to-visit stack.
    auto& current_operator = operators_to_visit.back();
    if (!visited_operators.Contains(current_operator.Get())) {
      // The current operator is not visited, check whether its dependent
      // operators are visited or not.
      bool skip_visit = false;
      for (const auto& operand : current_operator->Inputs()) {
        if (operand->Kind() == webnn::mojom::blink::Operand::Kind::kOutput) {
          auto* dependent_operator = operand->Operator();
          CHECK(dependent_operator);
          if (!visited_operators.Contains(dependent_operator)) {
            // As there is an dependent operator is not visited, skip visiting
            // this operator and push the dependent operator into the to-visit
            // stack.
            skip_visit = true;
            operators_to_visit.push_back(dependent_operator);
          }
        }
      }
      if (!skip_visit) {
        // When all dependent operators have been visited, visit the current
        // operator and add it into the visited set.
        toposorted_operators.push_back(current_operator);
        visited_operators.insert(current_operator);
        // Pop the current operator from the to-visit stack.
        operators_to_visit.pop_back();
      }
    } else {
      // The current operator is already visited, pop it and check the next
      // one.
      operators_to_visit.pop_back();
    }
  }
  return toposorted_operators;
}

DOMArrayBufferView::ViewType GetArrayBufferViewType(
    webnn::OperandDataType data_type) {
  switch (data_type) {
    case webnn::OperandDataType::kFloat32:
      return DOMArrayBufferView::ViewType::kTypeFloat32;
    case webnn::OperandDataType::kFloat16:
      return DOMArrayBufferView::ViewType::kTypeFloat16;
    case webnn::OperandDataType::kInt32:
      return DOMArrayBufferView::ViewType::kTypeInt32;
    case webnn::OperandDataType::kUint32:
      return DOMArrayBufferView::ViewType::kTypeUint32;
    case webnn::OperandDataType::kInt64:
      return DOMArrayBufferView::ViewType::kTypeBigInt64;
    case webnn::OperandDataType::kUint64:
      return DOMArrayBufferView::ViewType::kTypeBigUint64;
    case webnn::OperandDataType::kInt8:
      return DOMArrayBufferView::ViewType::kTypeInt8;
    case webnn::OperandDataType::kUint8:
      return DOMArrayBufferView::ViewType::kTypeUint8;
    case webnn::OperandDataType::kInt4:
    case webnn::OperandDataType::kUint4:
      return DOMArrayBufferView::ViewType::kTypeUint8;
  }
}

Vector<uint32_t> CreateDefaultPermutation(const wtf_size_t rank) {
  Vector<uint32_t> default_permutation(rank);
  for (wtf_size_t i = 0; i < rank; ++i) {
    default_permutation[i] = rank - 1 - i;
  }
  return default_permutation;
}

Vector<uint32_t> CreateAllAxes(const wtf_size_t rank) {
  Vector<uint32_t> default_axes(rank);
  std::iota(default_axes.begin(), default_axes.end(), 0);
  return default_axes;
}

Vector<uint32_t> CreateLayerNormalizationDefaultAxes(const wtf_size_t rank) {
  Vector<uint32_t> default_axes;
  if (rank > 1) {
    default_axes.resize(rank - 1);
    std::iota(default_axes.begin(), default_axes.end(), 1);
  }
  return default_axes;
}

Vector<uint32_t> CreateSliceDefaultStrides(wtf_size_t rank) {
  return Vector<uint32_t>(rank, 1);
}

base::expected<void, String> ValidateFilterLayout(
    bool depthwise,
    V8MLInputOperandLayout input_layout,
    V8MLConv2dFilterOperandLayout filter_layout) {
  CHECK(input_layout.AsEnum() == V8MLInputOperandLayout::Enum::kNhwc);

  if (!depthwise) {
    // For regular conv2d, NHWC input layout expects weights layout in ohwi that
    // is [groups * group_output_channels, kernel_height, kernel_width,
    // group_input_channels].
    //
    // TODO(crbug.com/1273291): support other layouts by transposing the
    // filter operand.
    if (filter_layout.AsEnum() != V8MLConv2dFilterOperandLayout::Enum::kOhwi) {
      return base::unexpected(String::Format(
          "The filter layout %s is not supported.", filter_layout.AsCStr()));
    }
  } else {
    // For depthwise conv2d, NHWC input layout expects weights layout in ihwo
    // that is [1, kernel_height, kernel_width, input_channels *
    // depth_multiplier].
    //
    // TODO(crbug.com/1273291): support other layouts by transposing the
    // filter operand.
    if (filter_layout.AsEnum() != V8MLConv2dFilterOperandLayout::Enum::kIhwo) {
      return base::unexpected(String::Format(
          "The filter layout %s is not supported.", filter_layout.AsCStr()));
    }
  }

  return base::ok();
}

webnn::Size2d<uint32_t> CalculateConvTransposeOutputSize2D(
    const blink::MLConvTranspose2dOptions* options,
    uint32_t input_height,
    uint32_t input_width,
    uint32_t filter_height,
    uint32_t filter_width,
    uint32_t stride_height,
    uint32_t stride_width,
    uint32_t dilation_height,
    uint32_t dilation_width,
    uint32_t output_padding_height,
    uint32_t output_padding_width) {
  // Set the padding from WebNN explicit padding that is in
  // [beginning_height, ending_height, beginning_width, ending_width],
  // default to 0.
  auto ml_padding = options->getPaddingOr({0, 0, 0, 0});
  CHECK_EQ(ml_padding.size(), 4u);
  const webnn::Padding2d padding{
      .beginning = webnn::Size2d<uint32_t>{.height = ml_padding[0],
                                           .width = ml_padding[2]},
      .ending = webnn::Size2d<uint32_t>{.height = ml_padding[1],
                                        .width = ml_padding[3]}};
  const auto output_height = webnn::CalculateConvTranspose2dOutputSize(
      input_height, filter_height, padding.beginning.height,
      padding.ending.height, stride_height, dilation_height,
      output_padding_height);
  CHECK(output_height.has_value());

  const auto output_width = webnn::CalculateConvTranspose2dOutputSize(
      input_width, filter_width, padding.beginning.width, padding.ending.width,
      stride_width, dilation_width, output_padding_width);
  CHECK(output_width.has_value());

  return webnn::Size2d<uint32_t>{.height = output_height.value(),
                                 .width = output_width.value()};
}

V8MLOperandDataType ToBlinkDataType(webnn::OperandDataType data_type) {
  switch (data_type) {
    case webnn::OperandDataType::kFloat32:
      return V8MLOperandDataType(V8MLOperandDataType::Enum::kFloat32);
    case webnn::OperandDataType::kFloat16:
      return V8MLOperandDataType(V8MLOperandDataType::Enum::kFloat16);
    case webnn::OperandDataType::kInt32:
      return V8MLOperandDataType(V8MLOperandDataType::Enum::kInt32);
    case webnn::OperandDataType::kUint32:
      return V8MLOperandDataType(V8MLOperandDataType::Enum::kUint32);
    case webnn::OperandDataType::kInt64:
      return V8MLOperandDataType(V8MLOperandDataType::Enum::kInt64);
    case webnn::OperandDataType::kUint64:
      return V8MLOperandDataType(V8MLOperandDataType::Enum::kUint64);
    case webnn::OperandDataType::kInt8:
      return V8MLOperandDataType(V8MLOperandDataType::Enum::kInt8);
    case webnn::OperandDataType::kUint8:
      return V8MLOperandDataType(V8MLOperandDataType::Enum::kUint8);
    case webnn::OperandDataType::kInt4:
      return V8MLOperandDataType(V8MLOperandDataType::Enum::kInt4);
    case webnn::OperandDataType::kUint4:
      return V8MLOperandDataType(V8MLOperandDataType::Enum::kUint4);
  }
}

webnn::OperandDataType FromBlinkDataType(V8MLOperandDataType::Enum data_type) {
  switch (data_type) {
    case V8MLOperandDataType::Enum::kFloat32:
      return webnn::OperandDataType::kFloat32;
    case V8MLOperandDataType::Enum::kFloat16:
      return webnn::OperandDataType::kFloat16;
    case V8MLOperandDataType::Enum::kInt32:
      return webnn::OperandDataType::kInt32;
    case V8MLOperandDataType::Enum::kUint32:
      return webnn::OperandDataType::kUint32;
    case V8MLOperandDataType::Enum::kInt64:
      return webnn::OperandDataType::kInt64;
    case V8MLOperandDataType::Enum::kUint64:
      return webnn::OperandDataType::kUint64;
    case V8MLOperandDataType::Enum::kInt8:
      return webnn::OperandDataType::kInt8;
    case V8MLOperandDataType::Enum::kUint8:
      return webnn::OperandDataType::kUint8;
    case V8MLOperandDataType::Enum::kInt4:
      return webnn::OperandDataType::kInt4;
    case V8MLOperandDataType::Enum::kUint4:
      return webnn::OperandDataType::kUint4;
  }
}

bool IsLogicalBinaryOperator(
    webnn::mojom::blink::ElementWiseBinary::Kind kind) {
  switch (kind) {
    case webnn::mojom::blink::ElementWiseBinary::Kind::kAdd:
    case webnn::mojom::blink::ElementWiseBinary::Kind::kSub:
    case webnn::mojom::blink::ElementWiseBinary::Kind::kMul:
    case webnn::mojom::blink::ElementWiseBinary::Kind::kDiv:
    case webnn::mojom::blink::ElementWiseBinary::Kind::kMax:
    case webnn::mojom::blink::ElementWiseBinary::Kind::kMin:
    case webnn::mojom::blink::ElementWiseBinary::Kind::kPow:
      return false;
    case webnn::mojom::blink::ElementWiseBinary::Kind::kEqual:
    case webnn::mojom::blink::ElementWiseBinary::Kind::kGreater:
    case webnn::mojom::blink::ElementWiseBinary::Kind::kGreaterOrEqual:
    case webnn::mojom::blink::ElementWiseBinary::Kind::kLesser:
    case webnn::mojom::blink::ElementWiseBinary::Kind::kLesserOrEqual:
    case webnn::mojom::blink::ElementWiseBinary::Kind::kNotEqual:
    case webnn::mojom::blink::ElementWiseBinary::Kind::kLogicalAnd:
    case webnn::mojom::blink::ElementWiseBinary::Kind::kLogicalOr:
    case webnn::mojom::blink::ElementWiseBinary::Kind::kLogicalXor:
      return true;
  }
}

bool IsLogicalUnaryOperator(webnn::mojom::blink::ElementWiseUnary::Kind kind) {
  switch (kind) {
    case webnn::mojom::blink::ElementWiseUnary::Kind::kIsNaN:
    case webnn::mojom::blink::ElementWiseUnary::Kind::kIsInfinite:
    case webnn::mojom::blink::ElementWiseUnary::Kind::kLogicalNot:
      return true;
    default:
      return false;
  }
}

void LogConsoleWarning(ScriptState* script_state,
                       const String& message,
                       mojom::blink::ConsoleMessageSource message_source) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (!execution_context) {
    return;
  }
  execution_context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      message_source, mojom::blink::ConsoleMessageLevel::kWarning, message));
}

}  // namespace blink
