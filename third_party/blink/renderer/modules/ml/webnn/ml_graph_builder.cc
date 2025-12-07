// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"

#include <algorithm>
#include <variant>

#include "base/containers/contains.h"
#include "base/containers/enum_set.h"
#include "base/containers/span.h"
#include "base/metrics/histogram_macros.h"
#include "base/notimplemented.h"
#include "base/numerics/checked_math.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "services/webnn/public/cpp/ml_number.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/supported_tensors.h"
#include "services/webnn/public/cpp/webnn_errors.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/features.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_error.mojom-blink-forward.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom-blink-forward.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_bigint_unrestricteddouble.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_arg_min_max_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_batch_normalization_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_clamp_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_conv_transpose_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_cumulative_sum_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_elu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gather_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gemm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gru_cell_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gru_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_hard_sigmoid_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_instance_normalization_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_layer_normalization_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_leaky_relu_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_linear_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_lstm_cell_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_lstm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operand_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_operator_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pad_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pool_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_recurrent_network_activation.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_reduce_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_resample_2d_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_reverse_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_scatter_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_slice_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_split_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_transpose_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_triangular_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/ml/ml_context.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_constant_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_error.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_transform/pipeline.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_type_converter.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_utils.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_tensor.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/fp16/src/include/fp16.h"

namespace blink {

namespace blink_mojom = webnn::mojom::blink;

namespace {

// These values are persisted to logs. Entries should not be renumbered or
// removed and numeric values should never be reused.
// Please keep in sync with MLGraphOperatorUma in
// //tools/metrics/histograms/metadata/webnn/enums.xml.
enum class MLGraphOperatorUma {
  kGraphBuilt = 0,
  kAbs = 1,
  kAdd = 2,
  kArgMax = 3,
  kArgMin = 4,
  kAveragePool2d = 5,
  kBatchNormalization = 6,
  kCast = 7,
  kCeil = 8,
  kClamp = 9,
  kConcat = 10,
  kConv2d = 11,
  kConvTranspose2d = 12,
  kCos = 13,
  kCumulativeSum = 14,
  kDequantizeLinear = 15,
  kDiv = 16,
  kElu = 17,
  kEqual = 18,
  kErf = 19,
  kExp = 20,
  kExpand = 21,
  kFloor = 22,
  kGather = 23,
  kGatherElements = 24,
  kGatherNd = 25,
  kGelu = 26,
  kGemm = 27,
  kGreater = 28,
  kGreaterOrEqual = 29,
  kGru = 30,
  kGruCell = 31,
  kHardSigmoid = 32,
  kHardSwish = 33,
  kIdentity = 34,
  kInstanceNormalization = 35,
  kL2Pool2d = 36,
  kLayerNormalization = 37,
  kLeakyRelu = 38,
  kLesser = 39,
  kLesserOrEqual = 40,
  kLinear = 41,
  kLog = 42,
  kLogicalAnd = 43,
  kLogicalNot = 44,
  kLogicalOr = 45,
  kLogicalXor = 46,
  kLstm = 47,
  kLstmCell = 48,
  kMatmul = 49,
  kMax = 50,
  kMaxPool2d = 51,
  kMin = 52,
  kMul = 53,
  kNeg = 54,
  kPad = 55,
  kPow = 56,
  kPrelu = 57,
  kQuantizeLinear = 58,
  kReciprocal = 59,
  kReduceL1 = 60,
  kReduceL2 = 61,
  kReduceLogSum = 62,
  kReduceLogSumExp = 63,
  kReduceMax = 64,
  kReduceMean = 65,
  kReduceMin = 66,
  kReduceProduct = 67,
  kReduceSum = 68,
  kReduceSumSquare = 69,
  kRelu = 70,
  kResample2d = 71,
  kReshape = 72,
  kScatterElements = 73,
  kScatterNd = 74,
  kSigmoid = 75,
  kSign = 76,
  kSin = 77,
  kSlice = 78,
  kSoftmax = 79,
  kSoftplus = 80,
  kSoftsign = 81,
  kSplit = 82,
  kSqrt = 83,
  kSub = 84,
  kTan = 85,
  kTanh = 86,
  kTile = 87,
  kTranspose = 88,
  kTriangular = 89,
  kWhere = 90,
  kReverse = 91,
  kNotEqual = 92,
  kRoundEven = 93,
  kIsNaN = 94,
  kIsInfinite = 95,
  kMinValue = kGraphBuilt,
  kMaxValue = kIsInfinite,
};

using MLGraphOperatorUmaSet = base::EnumSet<MLGraphOperatorUma,
                                            MLGraphOperatorUma::kMinValue,
                                            MLGraphOperatorUma::kMaxValue>;

MLGraphOperatorUma GetUmaValueForOperation(
    const blink_mojom::Operation& operation) {
  switch (operation.which()) {
    case blink_mojom::Operation::Tag::kArgMinMax: {
      switch (operation.get_arg_min_max()->kind) {
        case blink_mojom::ArgMinMax::Kind::kMax:
          return MLGraphOperatorUma::kArgMax;
        case blink_mojom::ArgMinMax::Kind::kMin:
          return MLGraphOperatorUma::kArgMin;
      }
      break;
    }
    case blink_mojom::Operation::Tag::kBatchNormalization:
      return MLGraphOperatorUma::kBatchNormalization;
    case blink_mojom::Operation::Tag::kClamp:
      return MLGraphOperatorUma::kClamp;
    case blink_mojom::Operation::Tag::kConv2d:
      return MLGraphOperatorUma::kConv2d;
    case blink_mojom::Operation::Tag::kConcat:
      return MLGraphOperatorUma::kConcat;
    case blink_mojom::Operation::Tag::kCumulativeSum:
      return MLGraphOperatorUma::kCumulativeSum;
    case blink_mojom::Operation::Tag::kDequantizeLinear:
      return MLGraphOperatorUma::kDequantizeLinear;
    case blink_mojom::Operation::Tag::kElementWiseBinary: {
      switch (operation.get_element_wise_binary()->kind) {
        case blink_mojom::ElementWiseBinary::Kind::kAdd:
          return MLGraphOperatorUma::kAdd;
        case blink_mojom::ElementWiseBinary::Kind::kSub:
          return MLGraphOperatorUma::kSub;
        case blink_mojom::ElementWiseBinary::Kind::kMul:
          return MLGraphOperatorUma::kMul;
        case blink_mojom::ElementWiseBinary::Kind::kDiv:
          return MLGraphOperatorUma::kDiv;
        case blink_mojom::ElementWiseBinary::Kind::kMax:
          return MLGraphOperatorUma::kMax;
        case blink_mojom::ElementWiseBinary::Kind::kMin:
          return MLGraphOperatorUma::kMin;
        case blink_mojom::ElementWiseBinary::Kind::kPow:
          return MLGraphOperatorUma::kPow;
        case blink_mojom::ElementWiseBinary::Kind::kEqual:
          return MLGraphOperatorUma::kEqual;
        case blink_mojom::ElementWiseBinary::Kind::kGreater:
          return MLGraphOperatorUma::kGreater;
        case blink_mojom::ElementWiseBinary::Kind::kGreaterOrEqual:
          return MLGraphOperatorUma::kGreaterOrEqual;
        case blink_mojom::ElementWiseBinary::Kind::kLesser:
          return MLGraphOperatorUma::kLesser;
        case blink_mojom::ElementWiseBinary::Kind::kLesserOrEqual:
          return MLGraphOperatorUma::kLesserOrEqual;
        case blink_mojom::ElementWiseBinary::Kind::kNotEqual:
          return MLGraphOperatorUma::kNotEqual;
        case blink_mojom::ElementWiseBinary::Kind::kLogicalAnd:
          return MLGraphOperatorUma::kLogicalAnd;
        case blink_mojom::ElementWiseBinary::Kind::kLogicalOr:
          return MLGraphOperatorUma::kLogicalOr;
        case blink_mojom::ElementWiseBinary::Kind::kLogicalXor:
          return MLGraphOperatorUma::kLogicalXor;
      }
      break;
    }
    case blink_mojom::Operation::Tag::kElementWiseUnary: {
      switch (operation.get_element_wise_unary()->kind) {
        case blink_mojom::ElementWiseUnary::Kind::kAbs:
          return MLGraphOperatorUma::kAbs;
        case blink_mojom::ElementWiseUnary::Kind::kCast:
          return MLGraphOperatorUma::kCast;
        case blink_mojom::ElementWiseUnary::Kind::kCeil:
          return MLGraphOperatorUma::kCeil;
        case blink_mojom::ElementWiseUnary::Kind::kCos:
          return MLGraphOperatorUma::kCos;
        case blink_mojom::ElementWiseUnary::Kind::kExp:
          return MLGraphOperatorUma::kExp;
        case blink_mojom::ElementWiseUnary::Kind::kFloor:
          return MLGraphOperatorUma::kFloor;
        case blink_mojom::ElementWiseUnary::Kind::kIdentity:
          return MLGraphOperatorUma::kIdentity;
        case blink_mojom::ElementWiseUnary::Kind::kLog:
          return MLGraphOperatorUma::kLog;
        case blink_mojom::ElementWiseUnary::Kind::kIsNaN:
          return MLGraphOperatorUma::kIsNaN;
        case blink_mojom::ElementWiseUnary::Kind::kIsInfinite:
          return MLGraphOperatorUma::kIsInfinite;
        case blink_mojom::ElementWiseUnary::Kind::kLogicalNot:
          return MLGraphOperatorUma::kLogicalNot;
        case blink_mojom::ElementWiseUnary::Kind::kNeg:
          return MLGraphOperatorUma::kNeg;
        case blink_mojom::ElementWiseUnary::Kind::kReciprocal:
          return MLGraphOperatorUma::kReciprocal;
        case blink_mojom::ElementWiseUnary::Kind::kRoundEven:
          return MLGraphOperatorUma::kRoundEven;
        case blink_mojom::ElementWiseUnary::Kind::kSign:
          return MLGraphOperatorUma::kSign;
        case blink_mojom::ElementWiseUnary::Kind::kSin:
          return MLGraphOperatorUma::kSin;
        case blink_mojom::ElementWiseUnary::Kind::kSqrt:
          return MLGraphOperatorUma::kSqrt;
        case blink_mojom::ElementWiseUnary::Kind::kTan:
          return MLGraphOperatorUma::kTan;
        case blink_mojom::ElementWiseUnary::Kind::kErf:
          return MLGraphOperatorUma::kErf;
      }
      break;
    }
    case blink_mojom::Operation::Tag::kElu:
      return MLGraphOperatorUma::kElu;
    case blink_mojom::Operation::Tag::kExpand:
      return MLGraphOperatorUma::kExpand;
    case blink_mojom::Operation::Tag::kGather:
      return MLGraphOperatorUma::kGather;
    case blink_mojom::Operation::Tag::kGatherElements:
      return MLGraphOperatorUma::kGatherElements;
    case blink_mojom::Operation::Tag::kGatherNd:
      return MLGraphOperatorUma::kGatherNd;
    case blink_mojom::Operation::Tag::kGelu:
      return MLGraphOperatorUma::kGelu;
    case blink_mojom::Operation::Tag::kGemm:
      return MLGraphOperatorUma::kGemm;
    case blink_mojom::Operation::Tag::kGru:
      return MLGraphOperatorUma::kGru;
    case blink_mojom::Operation::Tag::kGruCell:
      return MLGraphOperatorUma::kGruCell;
    case blink_mojom::Operation::Tag::kHardSigmoid:
      return MLGraphOperatorUma::kHardSigmoid;
    case blink_mojom::Operation::Tag::kHardSwish:
      return MLGraphOperatorUma::kHardSwish;
    case blink_mojom::Operation::Tag::kInstanceNormalization:
      return MLGraphOperatorUma::kInstanceNormalization;
    case blink_mojom::Operation::Tag::kLayerNormalization:
      return MLGraphOperatorUma::kLayerNormalization;
    case blink_mojom::Operation::Tag::kLeakyRelu:
      return MLGraphOperatorUma::kLeakyRelu;
    case blink_mojom::Operation::Tag::kLinear:
      return MLGraphOperatorUma::kLinear;
    case blink_mojom::Operation::Tag::kLstmCell:
      return MLGraphOperatorUma::kLstmCell;
    case blink_mojom::Operation::Tag::kLstm:
      return MLGraphOperatorUma::kLstm;
    case blink_mojom::Operation::Tag::kMatmul:
      return MLGraphOperatorUma::kMatmul;
    case blink_mojom::Operation::Tag::kPad:
      return MLGraphOperatorUma::kPad;
    case blink_mojom::Operation::Tag::kPool2d: {
      switch (operation.get_pool2d()->kind) {
        case blink_mojom::Pool2d::Kind::kAveragePool2d:
          return MLGraphOperatorUma::kAveragePool2d;
        case blink_mojom::Pool2d::Kind::kMaxPool2d:
          return MLGraphOperatorUma::kMaxPool2d;
        case blink_mojom::Pool2d::Kind::kL2Pool2d:
          return MLGraphOperatorUma::kL2Pool2d;
      }
      break;
    }
    case blink_mojom::Operation::Tag::kPrelu:
      return MLGraphOperatorUma::kPrelu;
    case blink_mojom::Operation::Tag::kQuantizeLinear:
      return MLGraphOperatorUma::kQuantizeLinear;
    case blink_mojom::Operation::Tag::kReduce: {
      switch (operation.get_reduce()->kind) {
        case blink_mojom::Reduce::Kind::kL1:
          return MLGraphOperatorUma::kReduceL1;
        case blink_mojom::Reduce::Kind::kL2:
          return MLGraphOperatorUma::kReduceL2;
        case blink_mojom::Reduce::Kind::kLogSum:
          return MLGraphOperatorUma::kReduceLogSum;
        case blink_mojom::Reduce::Kind::kLogSumExp:
          return MLGraphOperatorUma::kReduceLogSumExp;
        case blink_mojom::Reduce::Kind::kMax:
          return MLGraphOperatorUma::kReduceMax;
        case blink_mojom::Reduce::Kind::kMean:
          return MLGraphOperatorUma::kReduceMean;
        case blink_mojom::Reduce::Kind::kMin:
          return MLGraphOperatorUma::kReduceMin;
        case blink_mojom::Reduce::Kind::kProduct:
          return MLGraphOperatorUma::kReduceProduct;
        case blink_mojom::Reduce::Kind::kSum:
          return MLGraphOperatorUma::kReduceSum;
        case blink_mojom::Reduce::Kind::kSumSquare:
          return MLGraphOperatorUma::kReduceSumSquare;
      }
      break;
    }
    case blink_mojom::Operation::Tag::kRelu:
      return MLGraphOperatorUma::kRelu;
    case blink_mojom::Operation::Tag::kResample2d:
      return MLGraphOperatorUma::kResample2d;
    case blink_mojom::Operation::Tag::kReshape:
      return MLGraphOperatorUma::kReshape;
    case blink_mojom::Operation::Tag::kReverse:
      return MLGraphOperatorUma::kReverse;
    case blink_mojom::Operation::Tag::kScatterElements:
      return MLGraphOperatorUma::kScatterElements;
    case blink_mojom::Operation::Tag::kScatterNd:
      return MLGraphOperatorUma::kScatterNd;
    case blink_mojom::Operation::Tag::kSigmoid:
      return MLGraphOperatorUma::kSigmoid;
    case blink_mojom::Operation::Tag::kSlice:
      return MLGraphOperatorUma::kSlice;
    case blink_mojom::Operation::Tag::kSoftmax:
      return MLGraphOperatorUma::kSoftmax;
    case blink_mojom::Operation::Tag::kSoftplus:
      return MLGraphOperatorUma::kSoftplus;
    case blink_mojom::Operation::Tag::kSoftsign:
      return MLGraphOperatorUma::kSoftsign;
    case blink_mojom::Operation::Tag::kSplit:
      return MLGraphOperatorUma::kSplit;
    case blink_mojom::Operation::Tag::kTanh:
      return MLGraphOperatorUma::kTanh;
    case blink_mojom::Operation::Tag::kTile:
      return MLGraphOperatorUma::kTile;
    case blink_mojom::Operation::Tag::kTranspose:
      return MLGraphOperatorUma::kTranspose;
    case blink_mojom::Operation::Tag::kTriangular:
      return MLGraphOperatorUma::kTriangular;
    case blink_mojom::Operation::Tag::kWhere:
      return MLGraphOperatorUma::kWhere;
  }
}

void RecordOperatorsUsed(const blink_mojom::GraphInfo& graph_info) {
  static const std::string_view kOperatorHistogram = "WebNN.Operator";

  // Record once per graph that it has been built. This will give us a count
  // for the total number of built graphs, which will be used to
  // calculate what percentage of graphs use a given operator.
  UMA_HISTOGRAM_ENUMERATION(kOperatorHistogram,
                            MLGraphOperatorUma::kGraphBuilt);

  MLGraphOperatorUmaSet operators_used;
  for (const auto& operation : graph_info.operations) {
    MLGraphOperatorUma uma_value = GetUmaValueForOperation(*operation);
    // For a given operator, record that it has been used only once.
    if (!operators_used.Has(uma_value)) {
      UMA_HISTOGRAM_ENUMERATION(kOperatorHistogram, uma_value);
      operators_used.Put(uma_value);
    }
  }
}

#define THROW_AND_RETURN_TYPE_IF_ERROR(func, return_value) \
  RETURN_IF_ERROR(func, [&exception_state](String error) { \
    exception_state.ThrowTypeError(error);                 \
    return return_value;                                   \
  });

#define THROW_AND_RETURN_IF_ERROR(func, return_value)                       \
  RETURN_IF_ERROR(func, [&exception_state](String error) {                  \
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError, \
                                      error);                               \
    return return_value;                                                    \
  });

#define ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(lhs, rexpr)                \
  ASSIGN_OR_RETURN(lhs, rexpr, [&exception_state](std::string error) { \
    exception_state.ThrowTypeError(String::FromUTF8(error));           \
    return nullptr;                                                    \
  });

constexpr char kGraphAlreadyBuiltError[] =
    "This MLGraphBuilder has already built a graph.";

webnn::InputOperandLayout BlinkInputOperandLayoutToComponent(
    blink::V8MLInputOperandLayout::Enum type) {
  switch (type) {
    case blink::V8MLInputOperandLayout::Enum::kNchw:
      return webnn::InputOperandLayout::kNchw;
    case blink::V8MLInputOperandLayout::Enum::kNhwc:
      return webnn::InputOperandLayout::kNhwc;
  }
}

webnn::Conv2dFilterOperandLayout BlinkConv2dFilterLayoutToComponent(
    blink::V8MLConv2dFilterOperandLayout::Enum type) {
  switch (type) {
    case blink::V8MLConv2dFilterOperandLayout::Enum::kOihw:
      return webnn::Conv2dFilterOperandLayout::kOihw;
    case blink::V8MLConv2dFilterOperandLayout::Enum::kHwio:
      return webnn::Conv2dFilterOperandLayout::kHwio;
    case blink::V8MLConv2dFilterOperandLayout::Enum::kOhwi:
      return webnn::Conv2dFilterOperandLayout::kOhwi;
    case blink::V8MLConv2dFilterOperandLayout::Enum::kIhwo:
      return webnn::Conv2dFilterOperandLayout::kIhwo;
  }
}

webnn::ConvTranspose2dFilterOperandLayout
BlinkConvTranspose2dFilterLayoutToComponent(
    blink::V8MLConvTranspose2dFilterOperandLayout::Enum type) {
  switch (type) {
    case blink::V8MLConvTranspose2dFilterOperandLayout::Enum::kIohw:
      return webnn::ConvTranspose2dFilterOperandLayout::kIohw;
    case blink::V8MLConvTranspose2dFilterOperandLayout::Enum::kHwoi:
      return webnn::ConvTranspose2dFilterOperandLayout::kHwoi;
    case blink::V8MLConvTranspose2dFilterOperandLayout::Enum::kOhwi:
      return webnn::ConvTranspose2dFilterOperandLayout::kOhwi;
  }
}

webnn::RoundingType BlinkRoundingTypeToComponent(
    blink::V8MLRoundingType::Enum type) {
  switch (type) {
    case blink::V8MLRoundingType::Enum::kFloor:
      return webnn::RoundingType::kFloor;
    case blink::V8MLRoundingType::Enum::kCeil:
      return webnn::RoundingType::kCeil;
  }
}

webnn::Pool2dKind FromMojoPool2dKind(blink_mojom::Pool2d::Kind kind) {
  switch (kind) {
    case blink_mojom::Pool2d::Kind::kAveragePool2d:
      return webnn::Pool2dKind::kAverage;
    case blink_mojom::Pool2d::Kind::kL2Pool2d:
      return webnn::Pool2dKind::kL2;
    case blink_mojom::Pool2d::Kind::kMaxPool2d:
      return webnn::Pool2dKind::kMax;
  }
}

webnn::ReduceKind MojoReduceKindToComponent(blink_mojom::Reduce::Kind kind) {
  switch (kind) {
    case blink_mojom::Reduce::Kind::kL1:
      return webnn::ReduceKind::kL1;
    case blink_mojom::Reduce::Kind::kL2:
      return webnn::ReduceKind::kL2;
    case blink_mojom::Reduce::Kind::kLogSum:
      return webnn::ReduceKind::kLogSum;
    case blink_mojom::Reduce::Kind::kLogSumExp:
      return webnn::ReduceKind::kLogSumExp;
    case blink_mojom::Reduce::Kind::kMax:
      return webnn::ReduceKind::kMax;
    case blink_mojom::Reduce::Kind::kMean:
      return webnn::ReduceKind::kMean;
    case blink_mojom::Reduce::Kind::kMin:
      return webnn::ReduceKind::kMin;
    case blink_mojom::Reduce::Kind::kProduct:
      return webnn::ReduceKind::kProduct;
    case blink_mojom::Reduce::Kind::kSum:
      return webnn::ReduceKind::kSum;
    case blink_mojom::Reduce::Kind::kSumSquare:
      return webnn::ReduceKind::kSumSquare;
  }
}

webnn::RecurrentNetworkDirection BlinkRecurrentNetworkDirectionToComponent(
    blink::V8MLRecurrentNetworkDirection::Enum direction) {
  switch (direction) {
    case blink::V8MLRecurrentNetworkDirection::Enum::kForward:
      return webnn::RecurrentNetworkDirection::kForward;
    case blink::V8MLRecurrentNetworkDirection::Enum::kBackward:
      return webnn::RecurrentNetworkDirection::kBackward;
    case blink::V8MLRecurrentNetworkDirection::Enum::kBoth:
      return webnn::RecurrentNetworkDirection::kBoth;
  }
}

webnn::PaddingMode BlinkPaddingModeToComponent(
    blink::V8MLPaddingMode::Enum type) {
  switch (type) {
    case blink::V8MLPaddingMode::Enum::kConstant:
      return webnn::PaddingMode::kConstant;
    case blink::V8MLPaddingMode::Enum::kEdge:
      return webnn::PaddingMode::kEdge;
    case blink::V8MLPaddingMode::Enum::kReflection:
      return webnn::PaddingMode::kReflection;
  }
}

webnn::BatchNormalizationAttributes ConvertToBatchNormalizationAttributes(
    const blink::MLBatchNormalizationOptions* options) {
  CHECK(options);
  webnn::BatchNormalizationAttributes attributes;
  if (options->hasScale()) {
    attributes.scale = options->scale()->Descriptor();
  }
  if (options->hasBias()) {
    attributes.bias = options->bias()->Descriptor();
  }
  attributes.label = options->label().Utf8();
  attributes.axis = options->axis();
  return attributes;
}

String BuildErrorMessage(const std::string& label, StringView message) {
  return StrCat({String::FromUTF8(webnn::GetErrorLabelPrefix(label)), message});
}

template <typename MLConv2dOptionsType, typename Conv2dAttributesType>
base::expected<Conv2dAttributesType, String> ConvertToConv2dAttributesBase(
    const MLConv2dOptionsType* options) {
  Conv2dAttributesType attributes;
  CHECK(options);
  const std::string label = options->label().Utf8();
  // If padding is not present, the values are assumed to be [0,0,0,0].
  auto padding = options->getPaddingOr({0, 0, 0, 0});
  if (padding.size() != 4) {
    return base::unexpected(
        BuildErrorMessage(label, "The length of padding should be 4."));
  }
  // The order of padding array is [beginning_height, ending_height,
  // beginning_width, ending_width].
  attributes.padding = webnn::Padding2d{
      .beginning =
          webnn::Size2d<uint32_t>{.height = padding[0], .width = padding[2]},
      .ending =
          webnn::Size2d<uint32_t>{.height = padding[1], .width = padding[3]}};

  // If strides is not present, the values are assumed to be [1,1].
  auto strides = options->getStridesOr({1, 1});
  if (strides.size() != 2) {
    return base::unexpected(
        BuildErrorMessage(label, "The length of strides should be 2."));
  }
  attributes.strides =
      webnn::Size2d<uint32_t>{.height = strides[0], .width = strides[1]};

  // If dilations is not present, the values are assumed to be [1,1].
  auto dilations = options->getDilationsOr({1, 1});
  if (dilations.size() != 2) {
    return base::unexpected(
        BuildErrorMessage(label, "The length of dilations should be 2."));
  }
  attributes.dilations =
      webnn::Size2d<uint32_t>{.height = dilations[0], .width = dilations[1]};
  attributes.groups = options->groups();
  attributes.input_layout =
      BlinkInputOperandLayoutToComponent(options->inputLayout().AsEnum());
  if (options->hasBias()) {
    attributes.bias_operand = options->bias()->Descriptor();
  }
  attributes.label = label;

  return std::move(attributes);
}

base::expected<webnn::Conv2dAttributes, String> ConvertToConv2dAttributes(
    const blink::MLConv2dOptions* options) {
  auto attributes =
      ConvertToConv2dAttributesBase<blink::MLConv2dOptions,
                                    webnn::Conv2dAttributes>(options);
  if (!attributes.has_value()) {
    return base::unexpected(attributes.error());
  }
  attributes.value().filter_layout =
      BlinkConv2dFilterLayoutToComponent(options->filterLayout().AsEnum());

  return attributes;
}

base::expected<webnn::ConvTranspose2dAttributes, String>
ConvertToConvTranspose2dAttributes(
    const blink::MLConvTranspose2dOptions* options) {
  auto attributes =
      ConvertToConv2dAttributesBase<blink::MLConvTranspose2dOptions,
                                    webnn::ConvTranspose2dAttributes>(options);
  if (!attributes.has_value()) {
    return base::unexpected(attributes.error());
  }

  const std::string& label = attributes.value().label;
  // If output padding is not present, the values are assumed to be [0,0].
  const auto output_padding = options->getOutputPaddingOr({0, 0});
  if (output_padding.size() != 2) {
    return base::unexpected(
        BuildErrorMessage(label, "The length of output padding should be 2."));
  }
  attributes.value().output_padding = webnn::Size2d<uint32_t>{
      .height = output_padding[0], .width = output_padding[1]};

  if (options->hasOutputSizes()) {
    auto output_sizes = options->getOutputSizesOr({});
    if (output_sizes.size() != 2) {
      return base::unexpected(
          BuildErrorMessage(label, "The length of output sizes should be 2."));
    }
    attributes.value().output_sizes = webnn::Size2d<uint32_t>{
        .height = output_sizes[0], .width = output_sizes[1]};
  }

  attributes.value().filter_layout =
      BlinkConvTranspose2dFilterLayoutToComponent(
          options->filterLayout().AsEnum());

  return attributes;
}

base::expected<webnn::Pool2dAttributes, std::string> ConvertToPool2dAttributes(
    const blink::MLPool2dOptions* options) {
  CHECK(options);
  const std::string label = options->label().Utf8();
  webnn::Pool2dAttributes attributes;
  if (options->hasWindowDimensions()) {
    auto& window_dimensions = options->windowDimensions();
    if (window_dimensions.size() != 2) {
      return base::unexpected(webnn::GetErrorLabelPrefix(label) +
                              "The length of window dimensions should be 2.");
    }
    attributes.window_dimensions = webnn::Size2d<uint32_t>{
        .height = window_dimensions[0], .width = window_dimensions[1]};
  }

  // If padding is not present, the values are assumed to be [0,0,0,0].
  auto padding = options->getPaddingOr({0, 0, 0, 0});
  if (padding.size() != 4) {
    return base::unexpected(webnn::GetErrorLabelPrefix(label) +
                            "The length of padding should be 4.");
  }
  attributes.padding = webnn::Padding2d{
      .beginning =
          webnn::Size2d<uint32_t>{.height = padding[0], .width = padding[2]},
      .ending =
          webnn::Size2d<uint32_t>{.height = padding[1], .width = padding[3]}};

  // If strides is not present, the values are assumed to be [1,1].
  auto strides = options->getStridesOr({1, 1});
  if (strides.size() != 2) {
    return base::unexpected(webnn::GetErrorLabelPrefix(label) +
                            "The length of strides should be 2.");
  }
  attributes.strides =
      webnn::Size2d<uint32_t>{.height = strides[0], .width = strides[1]};

  // If dilations is not present, the values are assumed to be [1,1].
  auto dilations = options->getDilationsOr({1, 1});
  if (dilations.size() != 2) {
    return base::unexpected(webnn::GetErrorLabelPrefix(label) +
                            "The length of dilations should be 2.");
  }
  attributes.dilations =
      webnn::Size2d<uint32_t>{.height = dilations[0], .width = dilations[1]};
  attributes.layout =
      BlinkInputOperandLayoutToComponent(options->layout().AsEnum());
  attributes.rounding_type =
      BlinkRoundingTypeToComponent(options->roundingType().AsEnum());
  if (options->hasOutputSizes()) {
    // TODO(ningxin.hu@intel.com): report a DevTools warning message if rounding
    // type is provided but ignored.
    auto& output_size = options->outputSizes();
    if (output_size.size() != 2) {
      return base::unexpected(webnn::GetErrorLabelPrefix(label) +
                              "The length of output sizes should be 2.");
    }
    attributes.output_sizes = webnn::Size2d<uint32_t>{.height = output_size[0],
                                                      .width = output_size[1]};
  }
  attributes.label = label;
  return attributes;
}

webnn::GemmAttributes ConvertToGemmAttributes(
    const blink::MLGemmOptions* options) {
  CHECK(options);
  webnn::GemmAttributes attributes;
  if (options->hasC()) {
    attributes.c_operand = options->c()->Descriptor();
  }
  attributes.alpha = options->alpha();
  attributes.beta = options->beta();
  attributes.a_transpose = options->aTranspose();
  attributes.b_transpose = options->bTranspose();
  attributes.label = options->label().Utf8();
  return attributes;
}

webnn::GruAttributes ConvertToGruAttributes(MLGraphBuilder* builder,
                                            blink::MLGruOptions* options) {
  CHECK(options);
  webnn::GruAttributes attributes;

  if (options->hasBias()) {
    attributes.bias = options->bias()->Descriptor();
  }
  if (options->hasRecurrentBias()) {
    attributes.recurrent_bias = options->recurrentBias()->Descriptor();
  }
  if (options->hasInitialHiddenState()) {
    attributes.initial_hidden_state =
        options->initialHiddenState()->Descriptor();
  }
  attributes.return_sequence = options->returnSequence();
  attributes.direction =
      BlinkRecurrentNetworkDirectionToComponent(options->direction().AsEnum());
  if (!options->hasActivations()) {
    // Create a default activation sequence as defined in the spec.
    options->setActivations(
        {V8MLRecurrentNetworkActivation(
             V8MLRecurrentNetworkActivation::Enum::kSigmoid),
         V8MLRecurrentNetworkActivation(
             V8MLRecurrentNetworkActivation::Enum::kTanh)});
  }
  attributes.activation_count = options->activations().size();
  attributes.label = options->label().Utf8();
  return attributes;
}

webnn::GruCellAttributes ConvertToGruCellAttributes(
    MLGraphBuilder* builder,
    blink::MLGruCellOptions* options) {
  CHECK(options);
  webnn::GruCellAttributes attributes;

  if (options->hasBias()) {
    attributes.bias = options->bias()->Descriptor();
  }
  if (options->hasRecurrentBias()) {
    attributes.recurrent_bias = options->recurrentBias()->Descriptor();
  }
  if (!options->hasActivations()) {
    // Create a default activation sequence as defined in the spec.
    options->setActivations(
        {V8MLRecurrentNetworkActivation(
             V8MLRecurrentNetworkActivation::Enum::kSigmoid),
         V8MLRecurrentNetworkActivation(
             V8MLRecurrentNetworkActivation::Enum::kTanh)});
  }
  attributes.activation_count = options->activations().size();
  attributes.label = options->label().Utf8();
  return attributes;
}

webnn::InstanceNormalizationAttributes ConvertToInstanceNormalizationAttributes(
    const blink::MLInstanceNormalizationOptions* options) {
  CHECK(options);
  webnn::InstanceNormalizationAttributes attributes;
  if (options->hasScale()) {
    attributes.scale = options->scale()->Descriptor();
  }
  if (options->hasBias()) {
    attributes.bias = options->bias()->Descriptor();
  }
  attributes.layout =
      BlinkInputOperandLayoutToComponent(options->layout().AsEnum());
  attributes.label = options->label().Utf8();
  return attributes;
}

webnn::LayerNormalizationAttributes ConvertToLayerNormalizationAttributes(
    const blink::MLLayerNormalizationOptions* options) {
  CHECK(options);
  webnn::LayerNormalizationAttributes attributes;
  if (options->hasScale()) {
    attributes.scale = options->scale()->Descriptor();
  }
  if (options->hasBias()) {
    attributes.bias = options->bias()->Descriptor();
  }
  attributes.label = options->label().Utf8();
  return attributes;
}

webnn::LstmAttributes ConvertToLstmAttributes(
    const blink::MLLstmOptions* options) {
  CHECK(options);
  webnn::LstmAttributes attributes;

  if (options->hasBias()) {
    attributes.bias = options->bias()->Descriptor();
  }
  if (options->hasRecurrentBias()) {
    attributes.recurrent_bias = options->recurrentBias()->Descriptor();
  }
  if (options->hasPeepholeWeight()) {
    attributes.peephole_weight = options->peepholeWeight()->Descriptor();
  }
  if (options->hasInitialHiddenState()) {
    attributes.initial_hidden_state =
        options->initialHiddenState()->Descriptor();
  }
  if (options->hasInitialCellState()) {
    attributes.initial_cell_state = options->initialCellState()->Descriptor();
  }
  attributes.activation_count = options->activations().size();
  attributes.return_sequence = options->returnSequence();
  attributes.direction =
      BlinkRecurrentNetworkDirectionToComponent(options->direction().AsEnum());
  attributes.label = options->label().Utf8();
  return attributes;
}

webnn::LstmCellAttributes ConvertToLstmCellAttributes(
    const blink::MLLstmCellOptions* options) {
  CHECK(options);
  webnn::LstmCellAttributes attributes;

  if (options->hasBias()) {
    attributes.bias = options->bias()->Descriptor();
  }
  if (options->hasRecurrentBias()) {
    attributes.recurrent_bias = options->recurrentBias()->Descriptor();
  }
  if (options->hasPeepholeWeight()) {
    attributes.peephole_weight = options->peepholeWeight()->Descriptor();
  }
  attributes.activation_count = options->activations().size();
  attributes.label = options->label().Utf8();
  return attributes;
}

MLOperand* BuildArgMinMax(MLGraphBuilder* builder,
                          blink_mojom::ArgMinMax::Kind sub_kind,
                          MLOperand* input,
                          const uint32_t axis,
                          MLArgMinMaxOptions* options,
                          ExceptionState& exception_state) {
  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateArgMinMaxAndInferOutput(
          builder->GetContext()->GetProperties(), input->Descriptor(),
          options->label().Utf8(), axis,
          FromBlinkDataType(options->outputDataType().AsEnum()),
          options->keepDimensions()));

  auto* arg_min_max = MakeGarbageCollected<MLArgMinMaxOperator>(
      builder, sub_kind, axis, options);
  MLOperand* output = MLOperand::CreateOutput(
      builder, std::move(output_descriptor), arg_min_max);
  arg_min_max->Connect({input}, {output});

  return output;
}

MLOperand* BuildElementWiseBinary(
    const webnn::ContextProperties& context_properties,
    MLGraphBuilder* builder,
    blink_mojom::ElementWiseBinary::Kind kind,
    const webnn::SupportedTensors& tensor_constraint,
    MLOperand* a,
    MLOperand* b,
    MLOperatorOptions* options,
    ExceptionState& exception_state) {
  const std::string label = options->label().Utf8();
  if (!tensor_constraint.Supports(a->Descriptor())) {
    exception_state.ThrowTypeError(BuildErrorMessage(
        label, String(NotSupportedArgumentError("a", a->Descriptor(),
                                                tensor_constraint))));
    return nullptr;
  }
  if (!tensor_constraint.Supports(b->Descriptor())) {
    exception_state.ThrowTypeError(BuildErrorMessage(
        label, String(NotSupportedArgumentError("b", b->Descriptor(),
                                                tensor_constraint))));
    return nullptr;
  }

  if (a->DataType() != b->DataType()) {
    exception_state.ThrowTypeError(
        BuildErrorMessage(label, "The input operand data types don't match."));
    return nullptr;
  }
  auto output_shape = webnn::BroadcastShapes(a->Shape(), b->Shape());
  if (!output_shape) {
    exception_state.ThrowTypeError(
        BuildErrorMessage(label, "The input shapes are not broadcastable."));
    return nullptr;
  }

  // Logical operator outputs are bools, otherwise output operators are the same
  // type as input operators.
  webnn::OperandDataType data_type = IsLogicalBinaryOperator(kind)
                                         ? webnn::OperandDataType::kUint8
                                         : a->DataType();

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::OperandDescriptor::Create(context_properties, data_type,
                                       *output_shape, label));

  auto* binary = MakeGarbageCollected<MLOperator>(
      builder, /*kind=*/blink_mojom::Operation::Tag::kElementWiseBinary,
      options, /*sub_kind=*/kind);
  MLOperand* output =
      MLOperand::CreateOutput(builder, std::move(output_descriptor), binary);

  binary->Connect({a, b}, {output});
  return output;
}

MLOperand* BuildUnaryOperator(MLGraphBuilder* builder,
                              ExceptionState& exception_state,
                              blink_mojom::Operation::Tag kind,
                              const webnn::SupportedTensors& tensor_constraint,
                              MLOperand* input,
                              MLOperatorOptions* options) {
  // The output tensor of unary operator has the same data type and dimensions
  // as its input tensor.
  if (!tensor_constraint.Supports(input->Descriptor())) {
    exception_state.ThrowTypeError(BuildErrorMessage(
        options->label().Utf8(), String(NotSupportedInputArgumentError(
                                     input->Descriptor(), tensor_constraint))));
    return nullptr;
  }

  auto* unary = MakeGarbageCollected<MLOperator>(builder, kind, options);

  MLOperand* output =
      MLOperand::CreateOutput(builder, input->Descriptor(), unary);
  unary->Connect({input}, {output});
  return output;
}

MLOperand* BuildElementWiseUnaryOperator(
    const webnn::ContextProperties& context_properties,
    MLGraphBuilder* builder,
    ExceptionState& exception_state,
    blink_mojom::ElementWiseUnary::Kind kind,
    const webnn::SupportedTensors& tensor_constraint,
    MLOperand* input,
    MLOperatorOptions* options) {
  const std::string label = options->label().Utf8();
  if (!tensor_constraint.Supports(input->Descriptor())) {
    exception_state.ThrowTypeError(
        BuildErrorMessage(label, String(webnn::NotSupportedInputArgumentError(
                                     input->Descriptor(), tensor_constraint))));
    return nullptr;
  }

  // Logical operator outputs are bools, otherwise output operators are the same
  // type as input operators.
  webnn::OperandDataType data_type = IsLogicalUnaryOperator(kind)
                                         ? webnn::OperandDataType::kUint8
                                         : input->DataType();

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::OperandDescriptor::Create(context_properties, data_type,
                                       input->Shape(), label));

  auto* unary = MakeGarbageCollected<MLOperator>(
      builder, /*kind=*/blink_mojom::Operation::Tag::kElementWiseUnary, options,
      /*sub_kind=*/kind);
  MLOperand* output =
      MLOperand::CreateOutput(builder, std::move(output_descriptor), unary);
  unary->Connect({input}, {output});
  return output;
}

MLOperand* BuildReduce(MLGraphBuilder* builder,
                       blink_mojom::Reduce::Kind kind,
                       const webnn::ContextProperties& context_properties,
                       MLOperand* input,
                       MLReduceOptions* options,
                       ExceptionState& exception_state) {
  const auto axes = options->getAxesOr(CreateAllAxes(input->Rank()));
  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateReduceAndInferOutput(
          context_properties, MojoReduceKindToComponent(kind),
          input->Descriptor(), options->label().Utf8(), axes,
          options->keepDimensions()));

  auto* reduce = MakeGarbageCollected<MLOperator>(
      builder, /*kind=*/blink_mojom::Operation::Tag::kReduce, options,
      /*sub_kind=*/kind);
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-reduce, the output
  // tensor of reduce has the same data type as its input.
  MLOperand* output =
      MLOperand::CreateOutput(builder, std::move(output_descriptor), reduce);
  reduce->Connect({input}, {output});
  return output;
}

MLOperand* BuildPool2d(MLGraphBuilder* builder,
                       blink_mojom::Pool2d::Kind kind,
                       const webnn::ContextProperties& context_properties,
                       MLOperand* input,
                       MLPool2dOptions* options,
                       ExceptionState& exception_state) {
  auto pool2d_attributes = ConvertToPool2dAttributes(options);
  if (!pool2d_attributes.has_value()) {
    exception_state.ThrowTypeError(String::FromUTF8(pool2d_attributes.error()));
    return nullptr;
  }

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidatePool2dAndInferOutput(
          context_properties, input->Descriptor(), pool2d_attributes.value(),
          FromMojoPool2dKind(kind)));

  // Create pool2d operator and its output operand. Connect the pool2d operator
  // to its input and output operands.
  auto* pool2d = MakeGarbageCollected<MLOperator>(
      builder, /*kind=*/blink_mojom::Operation::Tag::kPool2d, options,
      /*sub_kind=*/kind);
  MLOperand* output =
      MLOperand::CreateOutput(builder, std::move(output_descriptor), pool2d);
  pool2d->Connect({input}, {output});
  return output;
}

// Determines the input and output resources required for this computational
// graph by traversing the graph from `named_outputs` to its inputs.
// This may fail if the graph is not valid.
base::expected<std::pair<MLGraph::NamedOperandDescriptors,
                         MLGraph::NamedOperandDescriptors>,
               String>
DetermineGraphConstraintsFromOutputs(const MLNamedOperands& named_outputs) {
  // The outputs should not be empty.
  if (named_outputs.empty()) {
    return base::unexpected("At least one output needs to be provided.");
  }

  // The queue and visited set of operators that help implement the
  // breadth-first graph traversal:
  // https://en.wikipedia.org/wiki/Breadth-first_search
  HeapDeque<Member<const MLOperator>> operators_queue;
  HeapHashSet<Member<const MLOperator>> visited_operators;

  MLGraph::NamedOperandDescriptors input_constraints;
  MLGraph::NamedOperandDescriptors output_constraints;

  // Validate the named outputs, setup corresponding output resource info and
  // initialize the queue and visited set with their dependent operators.
  for (const auto& output : named_outputs) {
    const auto& name = output.first;
    const auto& operand = output.second;
    if (operand->Kind() != blink_mojom::Operand::Kind::kOutput) {
      return base::unexpected(String::Format(
          "The operand with name \"%s\" is not an output operand.",
          name.Utf8().c_str()));
    }
    // Setup resource info for this output operand.
    output_constraints.insert(name, operand->Descriptor());
    visited_operators.insert(operand->Operator());
    operators_queue.push_back(operand->Operator());
  }

  // An input MLOperand may be used by more than one MLOperators. This set
  // ensures an input MLOperand won't be validated multiple times.
  HeapHashSet<Member<const MLOperand>> visited_input_operands;
  while (operators_queue.size() > 0) {
    const auto current_operator = operators_queue.TakeFirst();
    for (const auto& operand : current_operator->Inputs()) {
      switch (operand->Kind()) {
        case blink_mojom::Operand::Kind::kOutput:
          if (!visited_operators.Contains(operand->Operator())) {
            visited_operators.insert(operand->Operator());
            operators_queue.push_back(operand->Operator());
          }
          break;
        case blink_mojom::Operand::Kind::kInput:
          if (visited_input_operands.Contains(operand)) {
            continue;
          }
          visited_input_operands.insert(operand);
          if (input_constraints.Contains(operand->Name())) {
            return base::unexpected(
                String::Format("The input name \"%s\" is duplicated.",
                               operand->Name().Utf8().c_str()));
          }
          input_constraints.insert(operand->Name(), operand->Descriptor());
          break;
        case blink_mojom::Operand::Kind::kConstant:
          if (visited_input_operands.Contains(operand)) {
            continue;
          }
          visited_input_operands.insert(operand);
          break;
      }
    }
  }
  return std::make_pair(std::move(input_constraints),
                        std::move(output_constraints));
}

Vector<webnn::OperandId> GetInputs(const blink_mojom::Operation& operation) {
  switch (operation.which()) {
    case blink_mojom::Operation::Tag::kArgMinMax:
      return {operation.get_arg_min_max()->input_operand_id};
    case blink_mojom::Operation::Tag::kBatchNormalization: {
      const auto& batch_normalization = *operation.get_batch_normalization();
      Vector<webnn::OperandId> inputs;
      inputs.reserve(3 + batch_normalization.bias_operand_id.has_value() +
                     batch_normalization.scale_operand_id.has_value());

      inputs.push_back(batch_normalization.input_operand_id);
      inputs.push_back(batch_normalization.mean_operand_id);
      inputs.push_back(batch_normalization.variance_operand_id);

      if (batch_normalization.bias_operand_id.has_value()) {
        inputs.push_back(*batch_normalization.bias_operand_id);
      }
      if (batch_normalization.scale_operand_id.has_value()) {
        inputs.push_back(*batch_normalization.scale_operand_id);
      }
      return inputs;
    }
    case blink_mojom::Operation::Tag::kClamp:
      return {operation.get_clamp()->input_operand_id};
    case blink_mojom::Operation::Tag::kConcat:
      return operation.get_concat()->input_operand_ids;
    case blink_mojom::Operation::Tag::kConv2d: {
      const auto& conv2d = *operation.get_conv2d();
      Vector<webnn::OperandId> inputs;
      inputs.reserve(2 + conv2d.bias_operand_id.has_value());

      inputs.push_back(conv2d.input_operand_id);
      inputs.push_back(conv2d.filter_operand_id);

      if (conv2d.bias_operand_id.has_value()) {
        inputs.push_back(*conv2d.bias_operand_id);
      }
      return inputs;
    }
    case blink_mojom::Operation::Tag::kCumulativeSum:
      return {operation.get_cumulative_sum()->input_operand_id};
    case blink_mojom::Operation::Tag::kDequantizeLinear:
      return {operation.get_dequantize_linear()->input_operand_id,
              operation.get_dequantize_linear()->scale_operand_id,
              operation.get_dequantize_linear()->zero_point_operand_id};
    case blink_mojom::Operation::Tag::kElementWiseBinary:
      return {operation.get_element_wise_binary()->lhs_operand_id,
              operation.get_element_wise_binary()->rhs_operand_id};
    case blink_mojom::Operation::Tag::kElu:
      return {operation.get_elu()->input_operand_id};
    case blink_mojom::Operation::Tag::kElementWiseUnary:
      return {operation.get_element_wise_unary()->input_operand_id};
    case blink_mojom::Operation::Tag::kExpand:
      return {operation.get_expand()->input_operand_id};
    case blink_mojom::Operation::Tag::kGather:
      return {operation.get_gather()->input_operand_id,
              operation.get_gather()->indices_operand_id};
    case blink_mojom::Operation::Tag::kGatherElements:
      return {operation.get_gather_elements()->input_operand_id,
              operation.get_gather_elements()->indices_operand_id};
    case blink_mojom::Operation::Tag::kGatherNd:
      return {operation.get_gather_nd()->input_operand_id,
              operation.get_gather_nd()->indices_operand_id};
    case blink_mojom::Operation::Tag::kGelu:
      return {operation.get_gelu()->input_operand_id};
    case blink_mojom::Operation::Tag::kGemm: {
      const auto& gemm = *operation.get_gemm();
      Vector<webnn::OperandId> inputs;
      inputs.reserve(2 + gemm.c_operand_id.has_value());

      inputs.push_back(gemm.a_operand_id);
      inputs.push_back(gemm.b_operand_id);

      if (gemm.c_operand_id.has_value()) {
        inputs.push_back(*gemm.c_operand_id);
      }
      return inputs;
    }
    case blink_mojom::Operation::Tag::kGru: {
      const auto& gru = *operation.get_gru();
      Vector<webnn::OperandId> inputs;
      inputs.reserve(3 + gru.bias_operand_id.has_value() +
                     gru.recurrent_bias_operand_id.has_value() +
                     gru.initial_hidden_state_operand_id.has_value());

      inputs.push_back(gru.input_operand_id);
      inputs.push_back(gru.weight_operand_id);
      inputs.push_back(gru.recurrent_weight_operand_id);

      if (gru.bias_operand_id.has_value()) {
        inputs.push_back(*gru.bias_operand_id);
      }
      if (gru.recurrent_bias_operand_id.has_value()) {
        inputs.push_back(*gru.recurrent_bias_operand_id);
      }
      if (gru.initial_hidden_state_operand_id.has_value()) {
        inputs.push_back(*gru.initial_hidden_state_operand_id);
      }
      return inputs;
    }
    case blink_mojom::Operation::Tag::kGruCell: {
      const auto& gru_cell = *operation.get_gru_cell();
      Vector<webnn::OperandId> inputs;
      inputs.reserve(4 + gru_cell.bias_operand_id.has_value() +
                     gru_cell.recurrent_bias_operand_id.has_value());

      inputs.push_back(gru_cell.input_operand_id);
      inputs.push_back(gru_cell.weight_operand_id);
      inputs.push_back(gru_cell.recurrent_weight_operand_id);
      inputs.push_back(gru_cell.hidden_state_operand_id);

      if (gru_cell.bias_operand_id.has_value()) {
        inputs.push_back(*gru_cell.bias_operand_id);
      }
      if (gru_cell.recurrent_bias_operand_id.has_value()) {
        inputs.push_back(*gru_cell.recurrent_bias_operand_id);
      }
      return inputs;
    }
    case blink_mojom::Operation::Tag::kHardSigmoid:
      return {operation.get_hard_sigmoid()->input_operand_id};
    case blink_mojom::Operation::Tag::kHardSwish:
      return {operation.get_hard_swish()->input_operand_id};
    case blink_mojom::Operation::Tag::kLayerNormalization: {
      const auto& layer_normalization = *operation.get_layer_normalization();
      Vector<webnn::OperandId> inputs;
      inputs.reserve(1 + layer_normalization.bias_operand_id.has_value() +
                     layer_normalization.scale_operand_id.has_value());

      inputs.push_back(layer_normalization.input_operand_id);

      if (layer_normalization.bias_operand_id.has_value()) {
        inputs.push_back(*layer_normalization.bias_operand_id);
      }
      if (layer_normalization.scale_operand_id.has_value()) {
        inputs.push_back(*layer_normalization.scale_operand_id);
      }
      return inputs;
    }
    case blink_mojom::Operation::Tag::kInstanceNormalization: {
      const auto& instance_normalization =
          *operation.get_instance_normalization();
      Vector<webnn::OperandId> inputs;
      inputs.reserve(1 + instance_normalization.bias_operand_id.has_value() +
                     instance_normalization.scale_operand_id.has_value());

      inputs.push_back(instance_normalization.input_operand_id);

      if (instance_normalization.bias_operand_id.has_value()) {
        inputs.push_back(*instance_normalization.bias_operand_id);
      }
      if (instance_normalization.scale_operand_id.has_value()) {
        inputs.push_back(*instance_normalization.scale_operand_id);
      }
      return inputs;
    }
    case blink_mojom::Operation::Tag::kLeakyRelu:
      return {operation.get_leaky_relu()->input_operand_id};
    case blink_mojom::Operation::Tag::kLinear:
      return {operation.get_linear()->input_operand_id};
    case blink_mojom::Operation::Tag::kLstm: {
      const auto& lstm = *operation.get_lstm();
      Vector<webnn::OperandId> inputs;
      inputs.reserve(3 + lstm.bias_operand_id.has_value() +
                     lstm.recurrent_bias_operand_id.has_value() +
                     lstm.peephole_weight_operand_id.has_value() +
                     lstm.initial_hidden_state_operand_id.has_value() +
                     lstm.initial_cell_state_operand_id.has_value());

      inputs.push_back(lstm.input_operand_id);
      inputs.push_back(lstm.weight_operand_id);
      inputs.push_back(lstm.recurrent_weight_operand_id);

      if (lstm.bias_operand_id.has_value()) {
        inputs.push_back(*lstm.bias_operand_id);
      }
      if (lstm.recurrent_bias_operand_id.has_value()) {
        inputs.push_back(*lstm.recurrent_bias_operand_id);
      }
      if (lstm.peephole_weight_operand_id.has_value()) {
        inputs.push_back(*lstm.peephole_weight_operand_id);
      }
      if (lstm.initial_hidden_state_operand_id.has_value()) {
        inputs.push_back(*lstm.initial_hidden_state_operand_id);
      }
      if (lstm.initial_cell_state_operand_id.has_value()) {
        inputs.push_back(*lstm.initial_cell_state_operand_id);
      }
      return inputs;
    }
    case blink_mojom::Operation::Tag::kLstmCell: {
      const auto& lstm_cell = *operation.get_lstm_cell();
      Vector<webnn::OperandId> inputs;
      inputs.reserve(5 + lstm_cell.bias_operand_id.has_value() +
                     lstm_cell.recurrent_bias_operand_id.has_value() +
                     lstm_cell.peephole_weight_operand_id.has_value());

      inputs.push_back(lstm_cell.input_operand_id);
      inputs.push_back(lstm_cell.weight_operand_id);
      inputs.push_back(lstm_cell.recurrent_weight_operand_id);
      inputs.push_back(lstm_cell.hidden_state_operand_id);
      inputs.push_back(lstm_cell.cell_state_operand_id);

      if (lstm_cell.bias_operand_id.has_value()) {
        inputs.push_back(*lstm_cell.bias_operand_id);
      }
      if (lstm_cell.recurrent_bias_operand_id.has_value()) {
        inputs.push_back(*lstm_cell.recurrent_bias_operand_id);
      }
      if (lstm_cell.peephole_weight_operand_id.has_value()) {
        inputs.push_back(*lstm_cell.peephole_weight_operand_id);
      }
      return inputs;
    }
    case blink_mojom::Operation::Tag::kMatmul:
      return {operation.get_matmul()->a_operand_id,
              operation.get_matmul()->b_operand_id};
    case blink_mojom::Operation::Tag::kPad:
      return {operation.get_pad()->input_operand_id};
    case blink_mojom::Operation::Tag::kPool2d:
      return {operation.get_pool2d()->input_operand_id};
    case blink_mojom::Operation::Tag::kPrelu:
      return {operation.get_prelu()->input_operand_id,
              operation.get_prelu()->slope_operand_id};
    case blink_mojom::Operation::Tag::kQuantizeLinear:
      return {operation.get_quantize_linear()->input_operand_id,
              operation.get_quantize_linear()->scale_operand_id,
              operation.get_quantize_linear()->zero_point_operand_id};
    case blink_mojom::Operation::Tag::kReduce:
      return {operation.get_reduce()->input_operand_id};
    case blink_mojom::Operation::Tag::kRelu:
      return {operation.get_relu()->input_operand_id};
    case blink_mojom::Operation::Tag::kResample2d:
      return {operation.get_resample2d()->input_operand_id};
    case blink_mojom::Operation::Tag::kReshape:
      return {operation.get_reshape()->input_operand_id};
    case blink_mojom::Operation::Tag::kReverse:
      return {operation.get_reverse()->input_operand_id};
    case blink_mojom::Operation::Tag::kScatterElements:
      return {operation.get_scatter_elements()->input_operand_id,
              operation.get_scatter_elements()->indices_operand_id,
              operation.get_scatter_elements()->updates_operand_id};
    case blink_mojom::Operation::Tag::kScatterNd:
      return {operation.get_scatter_nd()->input_operand_id,
              operation.get_scatter_nd()->indices_operand_id,
              operation.get_scatter_nd()->updates_operand_id};
    case blink_mojom::Operation::Tag::kSigmoid:
      return {operation.get_sigmoid()->input_operand_id};
    case blink_mojom::Operation::Tag::kSlice:
      return {operation.get_slice()->input_operand_id};
    case blink_mojom::Operation::Tag::kSoftmax:
      return {operation.get_softmax()->input_operand_id};
    case blink_mojom::Operation::Tag::kSoftplus:
      return {operation.get_softplus()->input_operand_id};
    case blink_mojom::Operation::Tag::kSoftsign:
      return {operation.get_softsign()->input_operand_id};
    case blink_mojom::Operation::Tag::kSplit:
      return {operation.get_split()->input_operand_id};
    case blink_mojom::Operation::Tag::kTanh:
      return {operation.get_tanh()->input_operand_id};
    case blink_mojom::Operation::Tag::kTile:
      return {operation.get_tile()->input_operand_id};
    case blink_mojom::Operation::Tag::kTranspose:
      return {operation.get_transpose()->input_operand_id};
    case blink_mojom::Operation::Tag::kTriangular:
      return {operation.get_triangular()->input_operand_id};
    case blink_mojom::Operation::Tag::kWhere:
      return {operation.get_where()->condition_operand_id,
              operation.get_where()->true_value_operand_id,
              operation.get_where()->false_value_operand_id};
  }
}

blink_mojom::GraphInfoPtr BuildWebNNGraphInfo(
    const MLNamedOperands& named_outputs,
    const webnn::ContextProperties& context_properties) {
  // The `GraphInfo` represents an entire information of WebNN graph.
  auto graph_info = blink_mojom::GraphInfo::New();

  HeapHashMap<Member<const MLOperand>, webnn::OperandId> operand_to_id_map;
  for (const auto& [name, operand] : named_outputs) {
    // Create `mojo::Operand` for output operands of graph with the name.
    auto output_operand =
        mojo::ConvertTo<blink_mojom::OperandPtr>(operand.Get());
    output_operand->name = name;
    webnn::OperandId operand_id =
        AddOperand(*graph_info, std::move(output_operand));
    graph_info->output_operands.push_back(operand_id);
    operand_to_id_map.insert(operand, operand_id);
  }

  HeapVector<Member<MLOperator>> topologically_sorted_operators =
      GetOperatorsInTopologicalOrder(named_outputs);

  // Visit the operators in topological order. For each operator,
  // 1, Create `mojo::Operand` for its input and output operands if needed.
  // 2, Create `mojo::Operator` with the id of input and output operands.
  for (const auto& current_operator : topologically_sorted_operators) {
    for (const auto& operand : current_operator->Inputs()) {
      if (operand_to_id_map.Contains(operand.Get())) {
        // The `mojo::Operand` is already converted with the MLOperand, skip it.
        continue;
      }
      switch (operand->Kind()) {
        case blink_mojom::Operand::Kind::kInput: {
          // Create `mojo::Operand` for the input MLOperand.
          //  Build the array of input operands for this graph with the id.
          webnn::OperandId operand_id = AddOperand(
              *graph_info,
              mojo::ConvertTo<blink_mojom::OperandPtr>(operand.Get()));
          graph_info->input_operands.push_back(operand_id);
          operand_to_id_map.insert(operand, operand_id);
          break;
        }
        case blink_mojom::Operand::Kind::kConstant: {
          // Convert `mojo::Operand` for constant operand.
          webnn::OperandId operand_id = AddOperand(
              *graph_info,
              mojo::ConvertTo<blink_mojom::OperandPtr>(operand.Get()));
          // Build the map of constant operands for this graph with the id.
          MLConstantOperand const* constant_operand =
              operand->AsConstantOperand();
          if (constant_operand->tensor()) {
            graph_info->id_to_constant_tensor_operand_map.insert(
                operand_id, constant_operand->tensor()->handle());
          } else {
            graph_info->constant_operand_ids_to_handles.insert(
                operand_id, operand->AsConstantOperand()->handle());
          }
          operand_to_id_map.insert(operand, operand_id);
          break;
        }
        case blink_mojom::Operand::Kind::kOutput:
          // Because the operators are visited in topological order, if this
          // operand is an intermediate operand, it should already be defined as
          // an output operand of the dependent operator.
          NOTREACHED();
      }
    }
    for (const auto& operand : current_operator->Outputs()) {
      if (operand_to_id_map.Contains(operand.Get())) {
        // The `mojo::Operand` is already converted with the MLOperand, skip it.
        continue;
      }

      // Because the graph's output operands are already converted before, this
      // operand should be an intermediate operand that connects with two
      // operators. Create `mojo::Operand` for this operand.
      webnn::OperandId operand_id = AddOperand(
          *graph_info, mojo::ConvertTo<blink_mojom::OperandPtr>(operand.Get()));
      operand_to_id_map.insert(operand, operand_id);
    }
    // Create `mojo::Operation` with the id of the input and output operands.
    SerializeMojoOperation(operand_to_id_map, context_properties,
                           current_operator.Get(), graph_info.get());
  }

  return graph_info;
}

// Manually reshape constant operands if the constant operand is only used in
// its reshaped form.
void FoldReshapableConstants(blink_mojom::GraphInfo& graph_info) {
  // Keep track of new IDs for constant operands.
  HashMap<webnn::OperandId, webnn::OperandId> constant_id_remappings;

  for (const auto& [initial_constant_id, handle] :
       graph_info.constant_operand_ids_to_handles) {
    webnn::OperandId constant_operand_id = initial_constant_id;

    // For each constant operand, keep walking down the dependencies until no
    // reshape is found.
    while (true) {
      // Do not fold if it reshapes to graph output.
      auto reshape_operation_it = std::ranges::find_if(
          graph_info.operations,
          [&constant_operand_id,
           &graph_info](const blink_mojom::OperationPtr& operation) {
            return operation->is_reshape() &&
                   operation->get_reshape()->input_operand_id ==
                       constant_operand_id &&
                   !graph_info.output_operands.Contains(
                       operation->get_reshape()->output_operand_id);
          });

      // No reshapes depend on this constant. Nothing to do here.
      if (reshape_operation_it == graph_info.operations.end()) {
        break;
      }

      // If the constant is depended on by other operators, we can't fold it.
      // Note the queried range includes `reshape_operation_it`.
      //
      // TODO(crbug.com/364348897): Consider handling the case where a constant
      // operand is reshaped by multiple identical reshape operators.
      if (std::ranges::count_if(
              graph_info.operations,
              [&constant_operand_id](
                  const blink_mojom::OperationPtr& operation) {
                return base::Contains(GetInputs(*operation),
                                      constant_operand_id);
              }) > 1) {
        break;
      }

      // The reshape is the only operator dependent on the constant. Do constant
      // folding and update the graph accordingly.

      // Remove the constant and reshape operators, respectively.
      auto& constant_operand =
          graph_info.operands.at(constant_operand_id.value());

      webnn::OperandId reshape_output_id =
          (*reshape_operation_it)->get_reshape()->output_operand_id;
      auto& reshape_operand = graph_info.operands.at(reshape_output_id.value());

      // Manually reshape the constant and let the list of operations reflect
      // this.
      CHECK_EQ(reshape_operand->descriptor.data_type(),
               constant_operand->descriptor.data_type());
      constant_operand->descriptor = reshape_operand->descriptor;

      graph_info.operations.erase(reshape_operation_it);
      // Update graph_info operands to make constant_operand have the reshaped
      // operand id. The reshape operand becomes an dangling operand.
      std::swap(graph_info.operands[constant_operand_id.value()],
                graph_info.operands[reshape_output_id.value()]);
      constant_id_remappings.Set(initial_constant_id, reshape_output_id);

      // Prepare for the next iteration of this loop.
      constant_operand_id = reshape_output_id;
    }
  }

  // Update `graph_info.constant_id_to_buffer_map` to reflect the new constant
  // IDs. This is done after the above loop to avoid mutating this map while
  // iterating over it.
  for (const auto& [former_id, new_id] : constant_id_remappings) {
    auto handle = graph_info.constant_operand_ids_to_handles.Take(former_id);
    graph_info.constant_operand_ids_to_handles.insert(new_id,
                                                      std::move(handle));
  }
}

}  // namespace

// static
MLGraphBuilder* MLGraphBuilder::Create(ScriptState* script_state,
                                       MLContext* context,
                                       ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return nullptr;
  }

  return context->CreateWebNNGraphBuilder(script_state, exception_state);
}

MLGraphBuilder::MLGraphBuilder(
    ExecutionContext* execution_context,
    MLContext* context,
    mojo::PendingAssociatedRemote<blink_mojom::WebNNGraphBuilder>
        pending_remote)
    : execution_context_(execution_context),
      ml_context_(context),
      remote_(execution_context) {
  CHECK(base::FeatureList::IsEnabled(
      webnn::mojom::features::kWebMachineLearningNeuralNetwork));

  remote_.Bind(std::move(pending_remote),
               execution_context->GetTaskRunner(TaskType::kMachineLearning));
  remote_.set_disconnect_handler(
      BindOnce(&MLGraphBuilder::OnConnectionError, WrapWeakPersistent(this)));
}

MLGraphBuilder::~MLGraphBuilder() = default;

void MLGraphBuilder::Trace(Visitor* visitor) const {
  visitor->Trace(execution_context_);
  visitor->Trace(ml_context_);
  visitor->Trace(remote_);
  visitor->Trace(pending_resolver_);
  ScriptWrappable::Trace(visitor);
}

ExecutionContext* MLGraphBuilder::GetExecutionContext() const {
  return execution_context_.Get();
}

MLContext* MLGraphBuilder::GetContext() const {
  return ml_context_.Get();
}

MLOperand* MLGraphBuilder::input(ScriptState* script_state,
                                 String name,
                                 const MLOperandDescriptor* desc,
                                 ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  auto input_operand = MLOperand::ValidateAndCreateInput(
      ml_context_->GetProperties(), this, desc->dataType().AsEnum(),
      desc->shape(), std::move(name));
  if (!input_operand.has_value()) {
    exception_state.ThrowTypeError(input_operand.error());
    return nullptr;
  }

  return input_operand.value();
}

MLOperand* MLGraphBuilder::constant(ScriptState* script_state,
                                    const MLOperandDescriptor* desc,
                                    AllowSharedBufferSource* buffer,
                                    ExceptionState& exception_state) {
  webnn::ScopedTrace scoped_trace("MLGraphBuilder::constant");
  CHECK(buffer);

  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor descriptor,
      webnn::OperandDescriptor::Create(
          ml_context_->GetProperties(),
          FromBlinkDataType(desc->dataType().AsEnum()), desc->shape(),
          webnn::GetErrorLabelPrefix("constant")));

  webnn::OperandDataType data_type = descriptor.data_type();
  if (buffer->IsArrayBufferViewAllowShared()) {
    DOMArrayBufferView::ViewType buffer_view_type =
        buffer->GetAsArrayBufferViewAllowShared().Get()->GetType();
    if (buffer_view_type != DOMArrayBufferView::ViewType::kTypeUint8 &&
        buffer_view_type != GetArrayBufferViewType(data_type)) {
      if (data_type == webnn::OperandDataType::kFloat16 &&
          buffer_view_type == DOMArrayBufferView::ViewType::kTypeUint16) {
        // Passing a Uint16Array when the data type is float16 was supported
        // prior to Float16Array shipping. Maintain this special case to give
        // developers/frameworks time to migrate their code.
        // TODO(crbug.com/399459942): Remove this circa 2025Q3.
        LogConsoleWarning(
            script_state,
            "Passing a Uint16Array instance for a float16 "
            "operand is deprecated. Use Float16Array or Uint8Array instead.");

      } else {
        exception_state.ThrowTypeError(
            "The buffer view must either match the operand data type or be a "
            "Uint8Array.");
        return nullptr;
      }
    }
  }

  base::span<uint8_t> bytes = AsByteSpan(*buffer);
  if (descriptor.PackedByteLength() != bytes.size()) {
    exception_state.ThrowTypeError(
        String::Format("The buffer's byte length (%zu) doesn't match the "
                       "expected byte length (%zu).",
                       bytes.size(), descriptor.PackedByteLength()));
    return nullptr;
  }

  if (!ml_context_->GetProperties().data_type_limits.constant.Supports(
          descriptor)) {
    exception_state.ThrowTypeError(String(webnn::NotSupportedConstantError(
        descriptor, ml_context_->GetProperties().data_type_limits.constant)));
    return nullptr;
  }

  auto* constant =
      MakeGarbageCollected<MLConstantOperand>(this, std::move(descriptor));

  UMA_HISTOGRAM_MEMORY_KB("WebNN.ConstantDataSizeInKB", bytes.size() / 1024);
  TRACE_EVENT_BEGIN("webnn", "copy constant bytes into BigBuffer",
                    scoped_trace.track(), "size", bytes.size());
  mojo_base::BigBuffer constant_data = mojo_base::BigBuffer(bytes);
  scoped_trace.AddStep("post mojo message: CreatePendingConstant");
  remote_->CreatePendingConstant(constant->handle(), data_type,
                                 std::move(constant_data));
  TRACE_EVENT_END("webnn", scoped_trace.track());
  return constant;
}

MLOperand* MLGraphBuilder::constant(ScriptState* script_state,
                                    MLTensor* tensor,
                                    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  if (tensor->context() != ml_context_) {
    exception_state.ThrowTypeError(
        "The tensor wasn't created with this context.");
    return nullptr;
  }

  if (!tensor->IsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Tensor has been destroyed or context is lost.");
    return nullptr;
  }

  if (!tensor->Usage().Has(webnn::MLTensorUsageFlags::kGraphConstant)) {
    exception_state.ThrowTypeError(
        "Tensor was not created by createConstantTensor.");
    return nullptr;
  }

  return MakeGarbageCollected<MLConstantOperand>(this, tensor);
}

MLOperand* MLGraphBuilder::constant(
    ScriptState* script_state,
    V8MLOperandDataType type,
    const V8UnionBigintOrUnrestrictedDouble* value,
    ExceptionState& exception_state) {
  webnn::ScopedTrace scoped_trace("MLGraphBuilder::constant");

  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  webnn::OperandDataType data_type = FromBlinkDataType(type.AsEnum());
  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor descriptor,
      webnn::OperandDescriptor::Create(ml_context_->GetProperties(), data_type,
                                       /*shape=*/{},
                                       webnn::GetErrorLabelPrefix("constant")));

  if (!ml_context_->GetProperties().data_type_limits.constant.Supports(
          descriptor)) {
    exception_state.ThrowTypeError(String(webnn::NotSupportedConstantError(
        descriptor, ml_context_->GetProperties().data_type_limits.constant)));
    return nullptr;
  }

  base::expected<webnn::MLNumber, String> ml_number_value =
      ToMLNumberAsType(*value, data_type);
  if (!ml_number_value.has_value()) {
    exception_state.ThrowTypeError(ml_number_value.error());
    return nullptr;
  }

  // Write value to big buffer based on data type.
  mojo_base::BigBuffer constant_data;
  switch (data_type) {
    case webnn::OperandDataType::kFloat32: {
      constant_data = mojo_base::BigBuffer(base::byte_span_from_ref(
          base::allow_nonunique_obj, ml_number_value->AsFloat32()));
      break;
    }
    case webnn::OperandDataType::kFloat16: {
      constant_data = mojo_base::BigBuffer(base::byte_span_from_ref(
          fp16_ieee_from_fp32_value(ml_number_value->AsFloat32())));
      break;
    }
    case webnn::OperandDataType::kInt32: {
      constant_data = mojo_base::BigBuffer(
          base::byte_span_from_ref(ml_number_value->AsInt32()));
      break;
    }
    case webnn::OperandDataType::kUint32: {
      constant_data = mojo_base::BigBuffer(
          base::byte_span_from_ref(ml_number_value->AsUint32()));
      break;
    }
    case webnn::OperandDataType::kInt64: {
      constant_data = mojo_base::BigBuffer(
          base::byte_span_from_ref(ml_number_value->AsInt64()));
      break;
    }
    case webnn::OperandDataType::kUint64: {
      constant_data = mojo_base::BigBuffer(
          base::byte_span_from_ref(ml_number_value->AsUint64()));
      break;
    }
    case webnn::OperandDataType::kInt8: {
      constant_data = mojo_base::BigBuffer(
          base::byte_span_from_ref(ml_number_value->AsInt8()));
      break;
    }
    case webnn::OperandDataType::kUint8: {
      constant_data = mojo_base::BigBuffer(
          base::byte_span_from_ref(ml_number_value->AsUint8()));
      break;
    }
    case webnn::OperandDataType::kInt4:
    case webnn::OperandDataType::kUint4:
      exception_state.ThrowTypeError("Unsupported data type for constant");
      return nullptr;
  }

  size_t byte_length = descriptor.PackedByteLength();
  auto* constant =
      MakeGarbageCollected<MLConstantOperand>(this, std::move(descriptor));

  UMA_HISTOGRAM_MEMORY_KB("WebNN.ConstantDataSizeInKB", byte_length / 1024);
  TRACE_EVENT_BEGIN("webnn", "create constant scalar value BigBuffer",
                    scoped_trace.track(), "size", byte_length);
  scoped_trace.AddStep("post mojo message: CreatePendingConstant");
  remote_->CreatePendingConstant(constant->handle(), data_type,
                                 std::move(constant_data));
  TRACE_EVENT_END("webnn", scoped_trace.track());
  return constant;
}

MLOperand* MLGraphBuilder::argMin(MLOperand* input,
                                  const uint32_t axis,
                                  MLArgMinMaxOptions* options,
                                  ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);
  return BuildArgMinMax(this, blink_mojom::ArgMinMax::Kind::kMin, input, axis,
                        options, exception_state);
}

MLOperand* MLGraphBuilder::argMax(MLOperand* input,
                                  const uint32_t axis,
                                  MLArgMinMaxOptions* options,
                                  ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);
  return BuildArgMinMax(this, blink_mojom::ArgMinMax::Kind::kMax, input, axis,
                        options, exception_state);
}

MLOperand* MLGraphBuilder::batchNormalization(
    MLOperand* input,
    MLOperand* mean,
    MLOperand* variance,
    MLBatchNormalizationOptions* options,
    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<MLOperand>> inputs = {input, mean, variance};
  if (options->hasScale()) {
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(options->scale()), nullptr);
  }
  if (options->hasBias()) {
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(options->bias()), nullptr);
  }
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateBatchNormalizationAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(), mean->Descriptor(),
          variance->Descriptor(),
          ConvertToBatchNormalizationAttributes(options)));

  // Create batchNormalization operator and its output operand. Connect the
  // batchNormalization operator to its input and output operands.
  auto* batch_normalization = MakeGarbageCollected<MLOperator>(
      this, blink_mojom::Operation::Tag::kBatchNormalization, options);
  MLOperand* output = MLOperand::CreateOutput(
      this, std::move(output_descriptor), batch_normalization);
  batch_normalization->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::concat(const HeapVector<Member<MLOperand>>& inputs,
                                  const uint32_t axis,
                                  MLOperatorOptions* options,
                                  ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  std::vector<webnn::OperandDescriptor> input_component_operands;
  input_component_operands.reserve(inputs.size());
  std::ranges::transform(inputs, std::back_inserter(input_component_operands),
                         [](const auto& input) { return input->Descriptor(); });

  const std::string label = options->label().Utf8();
  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateConcatAndInferOutput(
          ml_context_->GetProperties(), input_component_operands, axis, label));

  auto* concat = MakeGarbageCollected<MLConcatOperator>(this, axis, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), concat);

  concat->Connect(inputs, {output});
  return output;
}

MLOperand* MLGraphBuilder::clamp(MLOperand* input,
                                 MLClampOptions* options,
                                 ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  const webnn::SupportedTensors& tensor_constraint =
      ml_context_->GetProperties().data_type_limits.clamp_input;
  if (!tensor_constraint.Supports(input->Descriptor())) {
    exception_state.ThrowTypeError(StrCat(
        {String::FromUTF8(webnn::GetErrorLabelPrefix(options->label().Utf8())),
         String(NotSupportedInputArgumentError(input->Descriptor(),
                                               tensor_constraint))}));
    return nullptr;
  }

  base::expected<webnn::MLNumber, String> min_value =
      options->hasMinValue()
          ? ToMLNumberAsType(*options->minValue(), input->DataType())
          : webnn::MLNumber::NegativeInfinity();
  if (!min_value.has_value()) {
    exception_state.ThrowTypeError(StrCat(
        {String::FromUTF8(webnn::GetErrorLabelPrefix(options->label().Utf8())),
         min_value.error()}));
    return nullptr;
  }
  base::expected<webnn::MLNumber, String> max_value =
      options->hasMaxValue()
          ? ToMLNumberAsType(*options->maxValue(), input->DataType())
          : webnn::MLNumber::Infinity();
  if (!max_value.has_value()) {
    exception_state.ThrowTypeError(
        BuildErrorMessage(options->label().Utf8(), max_value.error()));
    return nullptr;
  }

  // The behavior on different backends and hardware is inconsistent when NaN is
  // used for min_value or max_value. So we normalize it with minValue:NaN
  // coerced to -Infinity and maxValue:NaN coerced to Infinity.
  // This is being discussed in WG:
  // https://github.com/webmachinelearning/webnn/issues/874
  webnn::MLNumber coerced_min_value = min_value->IsNaN()
                                          ? webnn::MLNumber::NegativeInfinity()
                                          : std::move(*min_value);
  webnn::MLNumber coerced_max_value =
      max_value->IsNaN() ? webnn::MLNumber::Infinity() : std::move(*max_value);

  if (coerced_min_value.IsGreaterThan(coerced_max_value, input->DataType())) {
    exception_state.ThrowTypeError(BuildErrorMessage(
        options->label().Utf8(),
        "The min value should be less than or equal to the max value."));
    return nullptr;
  }

  auto* clamp = MakeGarbageCollected<MLClampOperator>(
      this, options->label(), std::move(coerced_min_value),
      std::move(coerced_max_value));
  MLOperand* output = MLOperand::CreateOutput(this, input->Descriptor(), clamp);

  clamp->Connect({input}, {output});
  return output;
}

MLOperand* MLGraphBuilder::conv2d(MLOperand* input,
                                  MLOperand* filter,
                                  MLConv2dOptions* options,
                                  ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<MLOperand>> inputs = {input, filter};
  if (options->hasBias()) {
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(options->bias()), nullptr);
  }
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  auto conv2d_attributes = ConvertToConv2dAttributes(options);
  if (!conv2d_attributes.has_value()) {
    exception_state.ThrowTypeError(conv2d_attributes.error());
    return nullptr;
  }

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateConv2dAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(),
          filter->Descriptor(), conv2d_attributes.value()));

  // Create conv2d operator and its output operand. Connect the conv2d operator
  // to its input and output operands.
  auto* conv2d = MakeGarbageCollected<MLOperator>(
      this, blink_mojom::Operation::Tag::kConv2d, options,
      /*sub_type=*/blink_mojom::Conv2d::Kind::kDirect);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), conv2d);
  conv2d->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::convTranspose2d(MLOperand* input,
                                           MLOperand* filter,
                                           MLConvTranspose2dOptions* options,
                                           ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<MLOperand>> inputs = {input, filter};
  if (options->hasBias()) {
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(options->bias()), nullptr);
  }
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  auto convTranspose2d_attributes = ConvertToConvTranspose2dAttributes(options);
  if (!convTranspose2d_attributes.has_value()) {
    exception_state.ThrowTypeError(convTranspose2d_attributes.error());
    return nullptr;
  }

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateConvTranspose2dAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(),
          filter->Descriptor(), convTranspose2d_attributes.value()));

  // Create convTranspose2d operator and its output operand. Connect the
  // convTranspose2d operator to its input and output operands.
  auto* convTranspose2d = MakeGarbageCollected<MLOperator>(
      this, blink_mojom::Operation::Tag::kConv2d, options,
      /*sub_type=*/blink_mojom::Conv2d::Kind::kTransposed);
  MLOperand* output = MLOperand::CreateOutput(
      this, std::move(output_descriptor), convTranspose2d);
  convTranspose2d->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::cumulativeSum(MLOperand* input,
                                         const uint32_t axis,
                                         MLCumulativeSumOptions* options,
                                         ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateCumulativeSumAndInferOutput(ml_context_->GetProperties(),
                                                 input->Descriptor(), axis,
                                                 options->label().Utf8()));

  // Create cumulativeSum operator and its output operand. Connect the
  // cumulativeSum operator to its input and output operands.
  auto* cumulativeSum =
      MakeGarbageCollected<MLCumulativeSumOperator>(this, axis, options);
  MLOperand* output = MLOperand::CreateOutput(
      this, std::move(output_descriptor), cumulativeSum);
  cumulativeSum->Connect({input}, {output});

  return output;
}

// Macro to define the function for an elementwise binary op. `op_camel` is the
// name of the op in camel case. `op_snake` is the name of the op in snake case.
// `op_kind` is the corresponding `ElementWiseBinary::Kind` enum. We need to
// separately specify the camel case and the snake case name because the
// function name is in camel case, while the corresponding `DataTypeLimits`
// field is in snake case.
#define BUILD_ELEMENTWISE_BINARY_OP(op_camel, op_snake, op_kind)              \
  MLOperand* MLGraphBuilder::op_camel(MLOperand* a, MLOperand* b,             \
                                      MLOperatorOptions* options,             \
                                      ExceptionState& exception_state) {      \
    THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);          \
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs({a, b}), nullptr);          \
    return BuildElementWiseBinary(                                            \
        ml_context_->GetProperties(), this,                                   \
        blink_mojom::ElementWiseBinary::Kind::op_kind,                        \
        ml_context_->GetProperties().data_type_limits.op_snake##_input, a, b, \
        options, exception_state);                                            \
  }

BUILD_ELEMENTWISE_BINARY_OP(add, add, kAdd)
BUILD_ELEMENTWISE_BINARY_OP(sub, sub, kSub)
BUILD_ELEMENTWISE_BINARY_OP(mul, mul, kMul)
BUILD_ELEMENTWISE_BINARY_OP(div, div, kDiv)
BUILD_ELEMENTWISE_BINARY_OP(min, min, kMin)
BUILD_ELEMENTWISE_BINARY_OP(max, max, kMax)
BUILD_ELEMENTWISE_BINARY_OP(pow, pow, kPow)
BUILD_ELEMENTWISE_BINARY_OP(equal, equal, kEqual)
BUILD_ELEMENTWISE_BINARY_OP(greater, greater, kGreater)
BUILD_ELEMENTWISE_BINARY_OP(lesser, lesser, kLesser)
BUILD_ELEMENTWISE_BINARY_OP(greaterOrEqual, greater_or_equal, kGreaterOrEqual)
BUILD_ELEMENTWISE_BINARY_OP(lesserOrEqual, lesser_or_equal, kLesserOrEqual)
BUILD_ELEMENTWISE_BINARY_OP(notEqual, not_equal, kNotEqual)
BUILD_ELEMENTWISE_BINARY_OP(logicalAnd, logical_and, kLogicalAnd)
BUILD_ELEMENTWISE_BINARY_OP(logicalOr, logical_or, kLogicalOr)
BUILD_ELEMENTWISE_BINARY_OP(logicalXor, logical_xor, kLogicalXor)

#define BUILD_ELEMENTWISE_UNARY_OP(op_camel, op_snake, op_kind)                \
  MLOperand* MLGraphBuilder::op_camel(MLOperand* input,                        \
                                      MLOperatorOptions* options,              \
                                      ExceptionState& exception_state) {       \
    THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);           \
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);             \
    return BuildElementWiseUnaryOperator(                                      \
        ml_context_->GetProperties(), this, exception_state,                   \
        blink_mojom::ElementWiseUnary::Kind::op_kind,                          \
        ml_context_->GetProperties().data_type_limits.op_snake##_input, input, \
        options);                                                              \
  }

BUILD_ELEMENTWISE_UNARY_OP(abs, abs, kAbs)
BUILD_ELEMENTWISE_UNARY_OP(ceil, ceil, kCeil)
BUILD_ELEMENTWISE_UNARY_OP(cos, cos, kCos)
BUILD_ELEMENTWISE_UNARY_OP(exp, exp, kExp)
BUILD_ELEMENTWISE_UNARY_OP(floor, floor, kFloor)
BUILD_ELEMENTWISE_UNARY_OP(log, log, kLog)
BUILD_ELEMENTWISE_UNARY_OP(isNaN, is_nan, kIsNaN)
BUILD_ELEMENTWISE_UNARY_OP(isInfinite, is_infinite, kIsInfinite)
BUILD_ELEMENTWISE_UNARY_OP(logicalNot, logical_not, kLogicalNot)
BUILD_ELEMENTWISE_UNARY_OP(neg, neg, kNeg)
BUILD_ELEMENTWISE_UNARY_OP(roundEven, round_even, kRoundEven)
BUILD_ELEMENTWISE_UNARY_OP(sign, sign, kSign)
BUILD_ELEMENTWISE_UNARY_OP(sin, sin, kSin)
BUILD_ELEMENTWISE_UNARY_OP(tan, tan, kTan)
BUILD_ELEMENTWISE_UNARY_OP(erf, erf, kErf)
BUILD_ELEMENTWISE_UNARY_OP(identity, identity, kIdentity)
BUILD_ELEMENTWISE_UNARY_OP(reciprocal, reciprocal, kReciprocal)
BUILD_ELEMENTWISE_UNARY_OP(sqrt, sqrt, kSqrt)

MLOperand* MLGraphBuilder::cast(MLOperand* input,
                                const V8MLOperandDataType output_data_type,
                                MLOperatorOptions* options,
                                ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateCastAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(),
          FromBlinkDataType(output_data_type.AsEnum()),
          options->label().Utf8()));

  auto* cast = MakeGarbageCollected<MLOperator>(
      this, blink_mojom::Operation::Tag::kElementWiseUnary, options,
      /*sub_kind=*/blink_mojom::ElementWiseUnary::Kind::kCast);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), cast);

  cast->Connect({input}, {output});
  return output;
}

MLOperand* MLGraphBuilder::dequantizeLinear(MLOperand* input,
                                            MLOperand* scale,
                                            MLOperand* zeroPoint,
                                            MLOperatorOptions* options,
                                            ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  HeapVector<Member<MLOperand>> inputs = {input, scale, zeroPoint};
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateDequantizeLinearAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(),
          scale->Descriptor(), zeroPoint->Descriptor(),
          options->label().Utf8()));

  auto* dequantize_linear = MakeGarbageCollected<MLOperator>(
      this, blink_mojom::Operation::Tag::kDequantizeLinear, options);
  MLOperand* output = MLOperand::CreateOutput(
      this, std::move(output_descriptor), dequantize_linear);
  dequantize_linear->Connect(std::move(inputs), {output});
  return output;
}

#define BUILD_REDUCE_OP(op, op_kind)                                        \
  MLOperand* MLGraphBuilder::op(MLOperand* input, MLReduceOptions* options, \
                                ExceptionState& exception_state) {          \
    THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);        \
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);          \
    return BuildReduce(this, blink_mojom::Reduce::Kind::op_kind,            \
                       ml_context_->GetProperties(), input, options,        \
                       exception_state);                                    \
  }

BUILD_REDUCE_OP(reduceL1, kL1)
BUILD_REDUCE_OP(reduceL2, kL2)
BUILD_REDUCE_OP(reduceLogSum, kLogSum)
BUILD_REDUCE_OP(reduceLogSumExp, kLogSumExp)
BUILD_REDUCE_OP(reduceMax, kMax)
BUILD_REDUCE_OP(reduceMean, kMean)
BUILD_REDUCE_OP(reduceMin, kMin)
BUILD_REDUCE_OP(reduceProduct, kProduct)
BUILD_REDUCE_OP(reduceSum, kSum)
BUILD_REDUCE_OP(reduceSumSquare, kSumSquare)

MLOperand* MLGraphBuilder::elu(MLOperand* input,
                               MLEluOptions* options,
                               ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);
  const std::string label = options->label().Utf8();

  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-elu, the output tensor of
  // elu has the same data type and dimensions as its input.
  return BuildUnaryOperator(
      this, exception_state, blink_mojom::Operation::Tag::kElu,
      ml_context_->GetProperties().data_type_limits.elu_input, input, options);
}

MLOperand* MLGraphBuilder::expand(MLOperand* input,
                                  const Vector<uint32_t>& new_shape,
                                  MLOperatorOptions* options,
                                  ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateExpandAndInferOutput(ml_context_->GetProperties(),
                                          input->Descriptor(), new_shape,
                                          options->label().Utf8()));

  auto* expand = MakeGarbageCollected<MLOperator>(
      this, blink_mojom::Operation::Tag::kExpand, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), expand);

  expand->Connect({input}, {output});
  return output;
}

MLOperand* MLGraphBuilder::gather(MLOperand* input,
                                  MLOperand* indices,
                                  MLGatherOptions* options,
                                  ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<MLOperand>> inputs = {input, indices};
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateGatherAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(),
          indices->Descriptor(), options->axis(), options->label().Utf8()));

  auto* gather = MakeGarbageCollected<MLOperator>(
      this, blink_mojom::Operation::Tag::kGather, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), gather);

  gather->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::gatherElements(MLOperand* input,
                                          MLOperand* indices,
                                          MLGatherOptions* options,
                                          ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<MLOperand>> inputs = {input, indices};
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateGatherElementsAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(),
          indices->Descriptor(), options->axis(), options->label().Utf8()));

  auto* gather_elements = MakeGarbageCollected<MLOperator>(
      this, blink_mojom::Operation::Tag::kGatherElements, options);
  MLOperand* output = MLOperand::CreateOutput(
      this, std::move(output_descriptor), gather_elements);

  gather_elements->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::gatherND(MLOperand* input,
                                    MLOperand* indices,
                                    MLOperatorOptions* options,
                                    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<MLOperand>> inputs = {input, indices};
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateGatherNDAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(),
          indices->Descriptor(), options->label().Utf8()));

  auto* gather_nd = MakeGarbageCollected<MLOperator>(
      this, blink_mojom::Operation::Tag::kGatherNd, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), gather_nd);

  gather_nd->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::gelu(MLOperand* input,
                                MLOperatorOptions* options,
                                ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-gelu, the output tensor of
  // gelu has the same data type and dimensions as its input. And the input data
  // type must be one of the floating point types.
  return BuildUnaryOperator(
      this, exception_state, blink_mojom::Operation::Tag::kGelu,
      ml_context_->GetProperties().data_type_limits.gelu_input, input, options);
}

MLOperand* MLGraphBuilder::gemm(MLOperand* a,
                                MLOperand* b,
                                MLGemmOptions* options,
                                ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<MLOperand>> inputs = {a, b};
  if (options->hasC()) {
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(options->c()), nullptr);
  }
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateGemmAndInferOutput(ml_context_->GetProperties(),
                                        a->Descriptor(), b->Descriptor(),
                                        ConvertToGemmAttributes(options)));

  auto* gemm = MakeGarbageCollected<MLOperator>(
      this, blink_mojom::Operation::Tag::kGemm, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), gemm);

  gemm->Connect(std::move(inputs), {output});
  return output;
}

HeapVector<Member<MLOperand>> MLGraphBuilder::gru(
    MLOperand* input,
    MLOperand* weight,
    MLOperand* recurrent_weight,
    const uint32_t steps,
    const uint32_t hidden_size,
    MLGruOptions* options,
    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(),
                            HeapVector<Member<MLOperand>>());

  HeapVector<Member<MLOperand>> inputs = {input, weight, recurrent_weight};
  if (options->hasBias()) {
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(options->bias()),
                                   HeapVector<Member<MLOperand>>());
  }
  if (options->hasRecurrentBias()) {
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(options->recurrentBias()),
                                   HeapVector<Member<MLOperand>>());
  }
  if (options->hasInitialHiddenState()) {
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(options->initialHiddenState()),
                                   HeapVector<Member<MLOperand>>());
  }
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs),
                                 HeapVector<Member<MLOperand>>());

  auto validated_outputs = webnn::ValidateGruAndInferOutput(
      ml_context_->GetProperties(), input->Descriptor(), weight->Descriptor(),
      recurrent_weight->Descriptor(), steps, hidden_size,
      ConvertToGruAttributes(this, options));
  if (!validated_outputs.has_value()) {
    exception_state.ThrowTypeError(String::FromUTF8(validated_outputs.error()));
    return {};
  }
  auto* gru =
      MakeGarbageCollected<MLGruOperator>(this, steps, hidden_size, options);

  HeapVector<Member<MLOperand>> outputs;
  for (const auto& validated_output : validated_outputs.value()) {
    outputs.push_back(MLOperand::CreateOutput(this, validated_output, gru));
  }

  gru->Connect(std::move(inputs), outputs);
  return outputs;
}

MLOperand* MLGraphBuilder::gruCell(MLOperand* input,
                                   MLOperand* weight,
                                   MLOperand* recurrent_weight,
                                   MLOperand* hidden_state,
                                   const uint32_t hidden_size,
                                   MLGruCellOptions* options,
                                   ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<MLOperand>> inputs = {input, weight, recurrent_weight,
                                          hidden_state};
  if (options->hasBias()) {
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(options->bias()), nullptr);
  }
  if (options->hasRecurrentBias()) {
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(options->recurrentBias()),
                                   nullptr);
  }
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor validated_output,
      webnn::ValidateGruCellAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(),
          weight->Descriptor(), recurrent_weight->Descriptor(),
          hidden_state->Descriptor(), hidden_size,
          ConvertToGruCellAttributes(this, options)));

  auto* gru_cell =
      MakeGarbageCollected<MLGruCellOperator>(this, hidden_size, options);

  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(validated_output), gru_cell);

  gru_cell->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::hardSigmoid(MLOperand* input,
                                       MLHardSigmoidOptions* options,
                                       ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-hardsigmoid, the output
  // tensor of softplus has the same type and dimensions as its input.
  return BuildUnaryOperator(
      this, exception_state, blink_mojom::Operation::Tag::kHardSigmoid,
      ml_context_->GetProperties().data_type_limits.hard_sigmoid_input, input,
      options);
}

MLOperand* MLGraphBuilder::hardSwish(MLOperand* input,
                                     MLOperatorOptions* options,
                                     ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-hard-swish, the output
  // tensor of hard-swish has the same data type and dimensions as its input.
  return BuildUnaryOperator(
      this, exception_state, blink_mojom::Operation::Tag::kHardSwish,
      ml_context_->GetProperties().data_type_limits.hard_swish_input, input,
      options);
}

MLOperand* MLGraphBuilder::instanceNormalization(
    MLOperand* input,
    MLInstanceNormalizationOptions* options,
    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<MLOperand>> inputs = {input};
  if (options->hasScale()) {
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(options->scale()), nullptr);
  }
  if (options->hasBias()) {
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(options->bias()), nullptr);
  }
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateInstanceNormalizationAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(),
          ConvertToInstanceNormalizationAttributes(options)));

  auto* instance_normalization = MakeGarbageCollected<MLOperator>(
      this, blink_mojom::Operation::Tag::kInstanceNormalization, options);

  MLOperand* output = MLOperand::CreateOutput(
      this, std::move(output_descriptor), instance_normalization);

  instance_normalization->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::layerNormalization(
    MLOperand* input,
    MLLayerNormalizationOptions* options,
    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<MLOperand>> inputs = {input};
  if (options->hasScale()) {
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(options->scale()), nullptr);
  }
  if (options->hasBias()) {
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(options->bias()), nullptr);
  }
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  // TODO(crbug.com/1273291): Figure out whether the `axes` should be required,
  // tracked by issue: https://github.com/webmachinelearning/webnn/issues/487
  const Vector<uint32_t> axes =
      options->getAxesOr(CreateLayerNormalizationDefaultAxes(input->Rank()));

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateLayerNormalizationAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(), axes,
          ConvertToLayerNormalizationAttributes(options)));

  auto* layer_normalization = MakeGarbageCollected<MLOperator>(
      this, blink_mojom::Operation::Tag::kLayerNormalization, options);

  MLOperand* output = MLOperand::CreateOutput(
      this, std::move(output_descriptor), layer_normalization);

  layer_normalization->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::leakyRelu(MLOperand* input,
                                     MLLeakyReluOptions* options,
                                     ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-leakyrelu, the output
  // tensor of leaky relu has the same type and dimensions as its input.
  return BuildUnaryOperator(
      this, exception_state, blink_mojom::Operation::Tag::kLeakyRelu,
      ml_context_->GetProperties().data_type_limits.leaky_relu_input, input,
      options);
}

MLOperand* MLGraphBuilder::linear(MLOperand* input,
                                  MLLinearOptions* options,
                                  ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  // The current spec doesn't specify the operand data type constraints of
  // linear. An issue has been filed to track it:
  // https://github.com/webmachinelearning/webnn/issues/283.
  //
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-linear, the output tensor
  // of linear has the same type and dimensions as its input.
  return BuildUnaryOperator(
      this, exception_state, blink_mojom::Operation::Tag::kLinear,
      ml_context_->GetProperties().data_type_limits.linear_input, input,
      options);
}

HeapVector<Member<MLOperand>> MLGraphBuilder::lstm(
    MLOperand* input,
    MLOperand* weight,
    MLOperand* recurrent_weight,
    const uint32_t steps,
    const uint32_t hidden_size,
    MLLstmOptions* options,
    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(),
                            HeapVector<Member<MLOperand>>());

  HeapVector<Member<MLOperand>> inputs = {input, weight, recurrent_weight};
  if (options->hasBias()) {
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(options->bias()),
                                   HeapVector<Member<MLOperand>>());
  }
  if (options->hasRecurrentBias()) {
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(options->recurrentBias()),
                                   HeapVector<Member<MLOperand>>());
  }
  if (options->hasPeepholeWeight()) {
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(options->peepholeWeight()),
                                   HeapVector<Member<MLOperand>>());
  }
  if (options->hasInitialHiddenState()) {
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(options->initialHiddenState()),
                                   HeapVector<Member<MLOperand>>());
  }
  if (options->hasInitialCellState()) {
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(options->initialCellState()),
                                   HeapVector<Member<MLOperand>>());
  }
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs),
                                 HeapVector<Member<MLOperand>>());

  if (!options->hasActivations()) {
    // Create a default activation sequence as defined in the spec.
    options->setActivations(
        {V8MLRecurrentNetworkActivation(
             V8MLRecurrentNetworkActivation::Enum::kSigmoid),
         V8MLRecurrentNetworkActivation(
             V8MLRecurrentNetworkActivation::Enum::kTanh),
         V8MLRecurrentNetworkActivation(
             V8MLRecurrentNetworkActivation::Enum::kTanh)});
  }

  auto validated_outputs = webnn::ValidateLstmAndInferOutput(
      ml_context_->GetProperties(), input->Descriptor(), weight->Descriptor(),
      recurrent_weight->Descriptor(), steps, hidden_size,
      ConvertToLstmAttributes(options));
  if (!validated_outputs.has_value()) {
    exception_state.ThrowTypeError(String::FromUTF8(validated_outputs.error()));
    return {};
  }

  auto* lstm =
      MakeGarbageCollected<MLLstmOperator>(this, steps, hidden_size, options);

  HeapVector<Member<MLOperand>> outputs;
  for (const auto& validated_output : validated_outputs.value()) {
    outputs.push_back(MLOperand::CreateOutput(this, validated_output, lstm));
  }

  lstm->Connect(std::move(inputs), outputs);
  return outputs;
}

HeapVector<Member<MLOperand>> MLGraphBuilder::lstmCell(
    MLOperand* input,
    MLOperand* weight,
    MLOperand* recurrent_weight,
    MLOperand* hidden_state,
    MLOperand* cell_state,
    const uint32_t hidden_size,
    MLLstmCellOptions* options,
    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(),
                            HeapVector<Member<MLOperand>>());

  HeapVector<Member<MLOperand>> inputs = {input, weight, recurrent_weight,
                                          hidden_state, cell_state};
  if (options->hasBias()) {
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(options->bias()),
                                   HeapVector<Member<MLOperand>>());
  }
  if (options->hasRecurrentBias()) {
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(options->recurrentBias()),
                                   HeapVector<Member<MLOperand>>());
  }
  if (options->hasPeepholeWeight()) {
    THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(options->peepholeWeight()),
                                   HeapVector<Member<MLOperand>>());
  }
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs),
                                 HeapVector<Member<MLOperand>>());

  if (!options->hasActivations()) {
    // Create a default activation sequence as defined in the spec.
    options->setActivations(
        {V8MLRecurrentNetworkActivation(
             V8MLRecurrentNetworkActivation::Enum::kSigmoid),
         V8MLRecurrentNetworkActivation(
             V8MLRecurrentNetworkActivation::Enum::kTanh),
         V8MLRecurrentNetworkActivation(
             V8MLRecurrentNetworkActivation::Enum::kTanh)});
  }

  auto validated_outputs = webnn::ValidateLstmCellAndInferOutput(
      ml_context_->GetProperties(), input->Descriptor(), weight->Descriptor(),
      recurrent_weight->Descriptor(), hidden_state->Descriptor(),
      cell_state->Descriptor(), hidden_size,
      ConvertToLstmCellAttributes(options));
  if (!validated_outputs.has_value()) {
    exception_state.ThrowTypeError(String::FromUTF8(validated_outputs.error()));
    return {};
  }

  auto* lstm_cell =
      MakeGarbageCollected<MLLstmCellOperator>(this, hidden_size, options);

  HeapVector<Member<MLOperand>> outputs;
  CHECK_EQ(validated_outputs->size(), 2u);
  outputs.reserve(2);
  for (const auto& validated_output : validated_outputs.value()) {
    outputs.push_back(
        MLOperand::CreateOutput(this, validated_output, lstm_cell));
  }

  lstm_cell->Connect(std::move(inputs), outputs);
  return outputs;
}

MLOperand* MLGraphBuilder::matmul(MLOperand* a,
                                  MLOperand* b,
                                  MLOperatorOptions* options,
                                  ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<MLOperand>> inputs = {a, b};
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateMatmulAndInferOutput(ml_context_->GetProperties(),
                                          a->Descriptor(), b->Descriptor(),
                                          options->label().Utf8()));

  // Create matmul operator and its output operand. Connect the matmul operator
  // to its input and output operands.
  auto* matmul = MakeGarbageCollected<MLOperator>(
      this, blink_mojom::Operation::Tag::kMatmul, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), matmul);

  matmul->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::pad(ScriptState* script_state,
                               MLOperand* input,
                               const Vector<uint32_t>& beginning_padding,
                               const Vector<uint32_t>& ending_padding,
                               MLPadOptions* options,
                               ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  const std::string label = options->label().Utf8();
  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidatePadAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(), beginning_padding,
          ending_padding, BlinkPaddingModeToComponent(options->mode().AsEnum()),
          label));

  // Pad becomes a no-op if input is a scalar and the paddings are all empty.
  if (input->Rank() == 0) {
    return BuildElementWiseUnaryOperator(
        ml_context_->GetProperties(), this, exception_state,
        blink_mojom::ElementWiseUnary::Kind::kIdentity,
        ml_context_->GetProperties().data_type_limits.identity_input, input,
        options);
  }

  base::expected<webnn::MLNumber, String> pad_value =
      ToMLNumberAsType(*options->value(), input->DataType());
  if (!pad_value.has_value()) {
    exception_state.ThrowTypeError(BuildErrorMessage(label, pad_value.error()));
    return nullptr;
  }

  if (options->mode().AsEnum() != V8MLPaddingMode::Enum::kConstant &&
      pad_value.value().AsFloat64() != 0.0) {
    LogConsoleWarning(script_state,
                      BuildErrorMessage(label,
                                        "The pad value is ignored unless the "
                                        "options.mode is set to constant."));
  }

  auto* pad = MakeGarbageCollected<MLPadOperator>(
      this, beginning_padding, ending_padding, std::move(*pad_value), options);
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-pad, the output
  // tensor of pad has the same data type as its input.
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), pad);

  pad->Connect({input}, {output});
  return output;
}

MLOperand* MLGraphBuilder::averagePool2d(MLOperand* input,
                                         MLPool2dOptions* options,
                                         ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  return BuildPool2d(this, blink_mojom::Pool2d::Kind::kAveragePool2d,
                     ml_context_->GetProperties(), input, options,
                     exception_state);
}

MLOperand* MLGraphBuilder::l2Pool2d(MLOperand* input,
                                    MLPool2dOptions* options,
                                    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  return BuildPool2d(this, blink_mojom::Pool2d::Kind::kL2Pool2d,
                     ml_context_->GetProperties(), input, options,
                     exception_state);
}

MLOperand* MLGraphBuilder::maxPool2d(MLOperand* input,
                                     MLPool2dOptions* options,
                                     ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  return BuildPool2d(this, blink_mojom::Pool2d::Kind::kMaxPool2d,
                     ml_context_->GetProperties(), input, options,
                     exception_state);
}

MLOperand* MLGraphBuilder::prelu(MLOperand* input,
                                 MLOperand* slope,
                                 MLOperatorOptions* options,
                                 ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<MLOperand>> inputs = {input, slope};
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidatePreluAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(),
          slope->Descriptor(), options->label().Utf8()));

  auto* prelu = MakeGarbageCollected<MLOperator>(
      this, blink_mojom::Operation::Tag::kPrelu, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), prelu);

  prelu->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::quantizeLinear(MLOperand* input,
                                          MLOperand* scale,
                                          MLOperand* zeroPoint,
                                          MLOperatorOptions* options,
                                          ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  HeapVector<Member<MLOperand>> inputs = {input, scale, zeroPoint};
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateQuantizeLinearAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(),
          scale->Descriptor(), zeroPoint->Descriptor(),
          options->label().Utf8()));

  auto* quantize_linear = MakeGarbageCollected<MLOperator>(
      this, blink_mojom::Operation::Tag::kQuantizeLinear, options);
  MLOperand* output = MLOperand::CreateOutput(
      this, std::move(output_descriptor), quantize_linear);
  quantize_linear->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::relu(MLOperand* input,
                                MLOperatorOptions* options,
                                ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-relu, the output tensor of
  // relu has the same data type and dimensions as its input.
  return BuildUnaryOperator(
      this, exception_state, blink_mojom::Operation::Tag::kRelu,
      ml_context_->GetProperties().data_type_limits.relu_input, input, options);
}

MLOperand* MLGraphBuilder::reshape(MLOperand* input,
                                   const Vector<uint32_t>& new_shape,
                                   MLOperatorOptions* options,
                                   ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  const std::string label = options->label().Utf8();

  if (!ml_context_->GetProperties().data_type_limits.reshape_input.Supports(
          input->Descriptor())) {
    exception_state.ThrowTypeError(BuildErrorMessage(
        label,
        String(NotSupportedInputArgumentError(
            input->Descriptor(),
            ml_context_->GetProperties().data_type_limits.reshape_input))));
    return nullptr;
  }

  if (!ml_context_->GetProperties()
           .data_type_limits.reshape_input.ranks.Supports(new_shape.size())) {
    exception_state.ThrowTypeError(BuildErrorMessage(
        label, String(NotSupportedOpOutputRankError(
                   static_cast<uint32_t>(new_shape.size()),
                   ml_context_->GetProperties()
                       .data_type_limits.reshape_input.ranks))));
    return nullptr;
  }

  // Setting the initial number of elements to 1 would cover the 0-D scalar with
  // empty dimensions.
  base::CheckedNumeric<size_t> checked_newshape_number_of_elements = 1;
  Vector<uint32_t> output_shape(new_shape.size());
  for (wtf_size_t i = 0; i < new_shape.size(); ++i) {
    auto dim = new_shape[i];
    if (dim == 0) {
      exception_state.ThrowTypeError(
          BuildErrorMessage(label, "The value of new shape should not be 0."));
      return nullptr;
    }
    checked_newshape_number_of_elements *= dim;
    output_shape[i] = dim;
  }
  size_t newshape_number_of_elements;
  if (!checked_newshape_number_of_elements.AssignIfValid(
          &newshape_number_of_elements)) {
    exception_state.ThrowTypeError(BuildErrorMessage(
        label, "The number of elements implied by new shape is too large."));
    return nullptr;
  }
  DCHECK_NE(newshape_number_of_elements, size_t(0));
  // The number of elements implied by new shape must be the same as the
  // number of elements in the input tensor.
  if (input->NumberOfElements() != newshape_number_of_elements) {
    exception_state.ThrowTypeError(BuildErrorMessage(
        label,
        String::Format(
            "The number of elements (%zu) implied by new shape doesn't match "
            "the number of elements (%zu) in the input tensor.",
            newshape_number_of_elements, input->NumberOfElements())));
    return nullptr;
  }

  // The output tensor byte length is valid because the data type and element
  // count are the same as input.
  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::OperandDescriptor::Create(ml_context_->GetProperties(),
                                       input->DataType(), output_shape, label));

  auto* reshape = MakeGarbageCollected<MLOperator>(
      this, blink_mojom::Operation::Tag::kReshape, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), reshape);

  reshape->Connect({input}, {output});
  return output;
}

MLOperand* MLGraphBuilder::resample2d(ScriptState* script_state,
                                      MLOperand* input,
                                      MLResample2dOptions* options,
                                      ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  const std::string label = options->label().Utf8();
  std::variant<base::span<const float>, base::span<const uint32_t>>
      scales_or_sizes;
  Vector<float> default_scales = {1.0, 1.0};
  if (options->hasSizes()) {
    if (options->hasScales()) {
      LogConsoleWarning(
          script_state,
          BuildErrorMessage(label,
                            "When sizes and scales are both specified, scales "
                            "argument is ignored."));
    }
    scales_or_sizes = options->sizes();
  } else {
    scales_or_sizes = options->hasScales() ? options->scales() : default_scales;
  }

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateResample2dAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(), scales_or_sizes,
          options->getAxesOr({2, 3}), label));

  // Create resample2d operator and its output operand. Connect the resample2d
  // operator to its input and output operands.
  auto* resample2d = MakeGarbageCollected<MLOperator>(
      this, blink_mojom::Operation::Tag::kResample2d, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), resample2d);

  resample2d->Connect({input}, {output});
  return output;
}

MLOperand* MLGraphBuilder::reverse(MLOperand* input,
                                   MLReverseOptions* options,
                                   ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  Vector<uint32_t> axes = options->getAxesOr(CreateAllAxes(input->Rank()));
  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateReverseAndInferOutput(ml_context_->GetProperties(),
                                           input->Descriptor(), axes,
                                           options->label().Utf8()));

  auto* reverse =
      MakeGarbageCollected<MLReverseOperator>(this, std::move(axes), options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), reverse);

  reverse->Connect({input}, {output});
  return output;
}

MLOperand* MLGraphBuilder::scatterElements(MLOperand* input,
                                           MLOperand* indices,
                                           MLOperand* updates,
                                           MLScatterOptions* options,
                                           ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<MLOperand>> inputs = {input, indices, updates};
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateScatterElementsAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(),
          indices->Descriptor(), updates->Descriptor(), options->axis(),
          options->label().Utf8()));

  auto* scatter_elements = MakeGarbageCollected<MLOperator>(
      this, blink_mojom::Operation::Tag::kScatterElements, options);
  MLOperand* output = MLOperand::CreateOutput(
      this, std::move(output_descriptor), scatter_elements);

  scatter_elements->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::scatterND(MLOperand* input,
                                     MLOperand* indices,
                                     MLOperand* updates,
                                     MLOperatorOptions* options,
                                     ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<MLOperand>> inputs = {input, indices, updates};
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateScatterNDAndInferOutput(
          ml_context_->GetProperties(), input->Descriptor(),
          indices->Descriptor(), updates->Descriptor(),
          options->label().Utf8()));

  auto* scatter_nd = MakeGarbageCollected<MLOperator>(
      this, blink_mojom::Operation::Tag::kScatterNd, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), scatter_nd);

  scatter_nd->Connect(std::move(inputs), {output});
  return output;
}

MLOperand* MLGraphBuilder::sigmoid(MLOperand* input,
                                   MLOperatorOptions* options,
                                   ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  // According to WebNN spec
  // https://webmachinelearning.github.io/webnn/#api-mlgraphbuilder-sigmoid, the
  // output tensor of sigmoid has the same data type and dimensions as its
  // input. And the input data type must be one of the floating point types.
  return BuildUnaryOperator(
      this, exception_state, blink_mojom::Operation::Tag::kSigmoid,
      ml_context_->GetProperties().data_type_limits.sigmoid_input, input,
      options);
}

MLOperand* MLGraphBuilder::slice(MLOperand* input,
                                 const Vector<uint32_t>& starts,
                                 const Vector<uint32_t>& sizes,
                                 MLSliceOptions* options,
                                 ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  webnn::SliceAttributes attributes;
  attributes.sizes.assign(sizes.begin(), sizes.end());
  attributes.starts.assign(starts.begin(), starts.end());
  Vector<uint32_t> strides =
      options->getStridesOr(CreateSliceDefaultStrides(input->Rank()));
  attributes.strides.assign(strides.begin(), strides.end());
  attributes.label = options->label().Utf8();

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateSliceAndInferOutput(ml_context_->GetProperties(),
                                         input->Descriptor(), attributes));

  // Slice becomes a no-op if the input is a scalar and starts, sizes, strides
  // are all empty.
  if (input->Rank() == 0) {
    return BuildElementWiseUnaryOperator(
        ml_context_->GetProperties(), this, exception_state,
        blink_mojom::ElementWiseUnary::Kind::kIdentity,
        ml_context_->GetProperties().data_type_limits.identity_input, input,
        options);
  }

  auto* slice = MakeGarbageCollected<MLSliceOperator>(this, starts, sizes,
                                                      strides, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), slice);

  slice->Connect({input}, {output});
  return output;
}

MLOperand* MLGraphBuilder::softmax(MLOperand* input,
                                   uint32_t axis,
                                   MLOperatorOptions* options,
                                   ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateSoftmaxAndInferOutput(ml_context_->GetProperties(),
                                           input->Descriptor(), axis,
                                           options->label().Utf8()));

  auto* softmax = MakeGarbageCollected<MLSoftmaxOperator>(this, axis, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), softmax);

  softmax->Connect({input}, {output});
  return output;
}

MLOperand* MLGraphBuilder::softplus(MLOperand* input,
                                    MLOperatorOptions* options,
                                    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-softplus, the output
  // tensor of softplus has the same type and dimensions as its input.
  return BuildUnaryOperator(
      this, exception_state, blink_mojom::Operation::Tag::kSoftplus,
      ml_context_->GetProperties().data_type_limits.softplus_input, input,
      options);
}

MLOperand* MLGraphBuilder::softsign(MLOperand* input,
                                    MLOperatorOptions* options,
                                    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-softsign, the output tensor
  // of softsign has the same data type and dimensions as its input.
  return BuildUnaryOperator(
      this, exception_state, blink_mojom::Operation::Tag::kSoftsign,
      ml_context_->GetProperties().data_type_limits.softsign_input, input,
      options);
}

HeapVector<Member<MLOperand>> MLGraphBuilder::split(
    MLOperand* input,
    const uint32_t splits,
    MLSplitOptions* options,
    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(),
                            HeapVector<Member<MLOperand>>());
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input),
                                 HeapVector<Member<MLOperand>>());

  auto validated_outputs = webnn::ValidateSplitAndInferOutput(
      ml_context_->GetProperties(), input->Descriptor(),
      {.splits = splits,
       .axis = options->axis(),
       .label = options->label().Utf8()});
  if (!validated_outputs.has_value()) {
    exception_state.ThrowTypeError(String::FromUTF8(validated_outputs.error()));
    return {};
  }

  auto* split = MakeGarbageCollected<MLSplitOperator>(this, splits, options);
  HeapVector<Member<MLOperand>> outputs;
  for (const auto& validated_output : validated_outputs.value()) {
    outputs.push_back(MLOperand::CreateOutput(this, validated_output, split));
  }
  split->Connect({input}, outputs);
  return outputs;
}

HeapVector<Member<MLOperand>> MLGraphBuilder::split(
    MLOperand* input,
    const Vector<uint32_t>& splits,
    MLSplitOptions* options,
    ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(),
                            HeapVector<Member<MLOperand>>());
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input),
                                 HeapVector<Member<MLOperand>>());

  auto validated_outputs = webnn::ValidateSplitAndInferOutput(
      ml_context_->GetProperties(), input->Descriptor(),
      {.splits = splits,
       .axis = options->axis(),
       .label = options->label().Utf8()});
  if (!validated_outputs.has_value()) {
    exception_state.ThrowTypeError(String::FromUTF8(validated_outputs.error()));
    return {};
  }

  auto* split = MakeGarbageCollected<MLSplitOperator>(this, splits, options);
  HeapVector<Member<MLOperand>> outputs;
  for (const auto& validated_output : validated_outputs.value()) {
    outputs.push_back(MLOperand::CreateOutput(this, validated_output, split));
  }
  split->Connect({input}, outputs);
  return outputs;
}

MLOperand* MLGraphBuilder::tanh(MLOperand* input,
                                MLOperatorOptions* options,
                                ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  // The input data type must be one of the floating point types.
  // The current spec doesn't specify the operand data type constraints of tanh,
  // an issue has been filed to track it-
  // https://github.com/webmachinelearning/webnn/issues/283.
  //
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-tanh, the output tensor of
  // tanh has the same data type and dimensions as its input.
  return BuildUnaryOperator(
      this, exception_state, blink_mojom::Operation::Tag::kTanh,
      ml_context_->GetProperties().data_type_limits.tanh_input, input, options);
}

MLOperand* MLGraphBuilder::tile(MLOperand* input,
                                const Vector<uint32_t>& repetitions,
                                MLOperatorOptions* options,
                                ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateTileAndInferOutput(ml_context_->GetProperties(),
                                        input->Descriptor(), repetitions,
                                        options->label().Utf8()));

  auto* tile = MakeGarbageCollected<MLTileOperator>(this, repetitions, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), tile);

  tile->Connect({input}, {output});
  return output;
}

MLOperand* MLGraphBuilder::transpose(MLOperand* input,
                                     MLTransposeOptions* options,
                                     ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  // According to WebNN spec:
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-transpose,
  // When permutation is not specified, it’s set to [N-1, ..., 0], where N is
  // the rank of the input tensor.
  const Vector<uint32_t> permutation =
      options->getPermutationOr(CreateDefaultPermutation(input->Rank()));
  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateTransposeAndInferOutput(ml_context_->GetProperties(),
                                             input->Descriptor(), permutation,
                                             options->label().Utf8()));

  auto* transpose = MakeGarbageCollected<MLOperator>(
      this, blink_mojom::Operation::Tag::kTranspose, options);
  // According to WebNN spec
  // https://www.w3.org/TR/webnn/#api-mlgraphbuilder-transpose, the output
  // tensor of transpose has the same data type as its input.
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), transpose);

  transpose->Connect({input}, {output});
  return output;
}

MLOperand* MLGraphBuilder::triangular(MLOperand* input,
                                      MLTriangularOptions* options,
                                      ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInput(input), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateTriangularAndInferOutput(ml_context_->GetProperties(),
                                              input->Descriptor(),
                                              options->label().Utf8()));

  auto* triangular = MakeGarbageCollected<MLOperator>(
      this, blink_mojom::Operation::Tag::kTriangular, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), triangular);

  triangular->Connect({input}, {output});
  return output;
}

MLOperand* MLGraphBuilder::where(MLOperand* condition,
                                 MLOperand* true_value,
                                 MLOperand* false_value,
                                 MLOperatorOptions* options,
                                 ExceptionState& exception_state) {
  THROW_AND_RETURN_IF_ERROR(ValidateGraphBuilderState(), nullptr);

  HeapVector<Member<MLOperand>> inputs = {condition, true_value, false_value};
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(inputs), nullptr);

  ASSIGN_OR_THROW_AND_RETURN_IF_ERROR(
      webnn::OperandDescriptor output_descriptor,
      webnn::ValidateWhereAndInferOutput(
          ml_context_->GetProperties(), condition->Descriptor(),
          true_value->Descriptor(), false_value->Descriptor(),
          options->label().Utf8()));

  auto* where = MakeGarbageCollected<MLOperator>(
      this, blink_mojom::Operation::Tag::kWhere, options);
  MLOperand* output =
      MLOperand::CreateOutput(this, std::move(output_descriptor), where);
  where->Connect(std::move(inputs), {output});
  return output;
}

ScriptPromise<MLGraph> MLGraphBuilder::build(ScriptState* script_state,
                                             MLNamedOperands& named_outputs,
                                             ExceptionState& exception_state) {
  webnn::ScopedTrace scoped_trace("MLGraphBuilder::build");
  base::expected<void, String> validation_result = ValidateGraphBuilderState();
  if (!validation_result.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      validation_result.error());
    return EmptyPromise();
  }

  HeapVector<Member<MLOperand>> outputs(named_outputs.size());
  std::ranges::transform(
      named_outputs, outputs.begin(),
      [](const auto& named_output) { return named_output.second; });
  THROW_AND_RETURN_TYPE_IF_ERROR(ValidateInputs(outputs),
                                 ScriptPromise<MLGraph>());

  for (const auto& named_output : named_outputs) {
    if (!ml_context_->GetProperties().data_type_limits.output().Supports(
            named_output.second->Descriptor())) {
      exception_state.ThrowTypeError(String(webnn::NotSupportedOutputError(
          named_output.first.Utf8(), named_output.second->Descriptor(),
          ml_context_->GetProperties().data_type_limits.output())));
      return EmptyPromise();
    }
  }

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid script state");
    return EmptyPromise();
  }

  scoped_trace.AddStep("DetermineGraphConstraintsFromOutputs");
  auto graph_constraints = DetermineGraphConstraintsFromOutputs(named_outputs);
  if (!graph_constraints.has_value()) {
    exception_state.ThrowTypeError(graph_constraints.error());
    return EmptyPromise();
  }

  // MLGraphTransformPipeline may change the operators topological order and
  // alter named outputs. BuildWebNNGraphInfo should sort the operators by
  // itself again before serialization.
  scoped_trace.AddStep("MLGraphTransformPipeline");
  auto* pipeline = MakeGarbageCollected<MLGraphTransformPipeline>(this);
  pipeline->Run(named_outputs);

  // Check the named_outputs descriptors are equal to the original's
  // descriptors.
  const auto& output_constraints = graph_constraints->second;
  CHECK_EQ(named_outputs.size(), output_constraints.size());
  for (const auto& [name, operand] : named_outputs) {
    const auto& constraint = output_constraints.at(name);
    // TODO(crbug.com/406666712): Change to `CHECK_EQ(operand->Descriptor(),
    // *constraint)` once we fix the `webnn::OperandDescriptor`.
    CHECK(operand->Descriptor() == *constraint);
  }

  scoped_trace.AddStep("BuildWebNNGraphInfo");
  blink_mojom::GraphInfoPtr graph_info =
      BuildWebNNGraphInfo(named_outputs, ml_context_->GetProperties());

  // Set `has_built_` after all inputs have been validated.
  has_built_ = true;

  scoped_trace.AddStep("FoldReshapableConstants");
  FoldReshapableConstants(*graph_info);

  scoped_trace.AddStep("RecordOperatorsUsed");
  RecordOperatorsUsed(*graph_info);

  pending_resolver_ = MakeGarbageCollected<ScriptPromiseResolver<MLGraph>>(
      script_state, exception_state.GetContext());

  scoped_trace.AddStep("post mojo message: CreateGraph");
  remote_->CreateGraph(std::move(graph_info),
                       blink::BindOnce(&MLGraphBuilder::DidCreateWebNNGraph,
                                       WrapPersistent(this),
                                       WrapPersistent(pending_resolver_.Get()),
                                       *std::move(graph_constraints)));
  return pending_resolver_->Promise();
}

void MLGraphBuilder::DidCreateWebNNGraph(
    ScriptPromiseResolver<blink::MLGraph>* resolver,
    std::pair<MLGraph::NamedOperandDescriptors,
              MLGraph::NamedOperandDescriptors> input_and_output_constraints,
    base::expected<blink_mojom::CreateGraphSuccessPtr, blink_mojom::ErrorPtr>
        result) {
  CHECK(has_built_);

  pending_resolver_.Clear();

  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid()) {
    return;
  }

  if (!result.has_value()) {
    const auto& create_graph_error = result.error();
    resolver->RejectWithDOMException(
        WebNNErrorCodeToDOMExceptionCode(create_graph_error->code),
        create_graph_error->message);
    return;
  }

  auto& success = result.value();
  Vector<V8MLDeviceType> devices;
  for (const auto& device : success->devices) {
    switch (device) {
      case blink_mojom::Device::kCpu:
        devices.push_back(V8MLDeviceType::Enum::kCpu);
        break;
      case blink_mojom::Device::kGpu:
        devices.push_back(V8MLDeviceType::Enum::kGpu);
        break;
      case blink_mojom::Device::kNpu:
        devices.push_back(V8MLDeviceType::Enum::kNpu);
        break;
    }
  }
  auto* graph = MakeGarbageCollected<MLGraph>(
      resolver->GetExecutionContext(), ml_context_,
      std::move(success->graph_remote),
      std::move(input_and_output_constraints.first),
      std::move(input_and_output_constraints.second), std::move(devices),
      base::PassKey<MLGraphBuilder>());
  ml_context_->OnGraphCreated(graph);

  resolver->Resolve(graph);
}

void MLGraphBuilder::OnConnectionError() {
  remote_.reset();

  if (pending_resolver_) {
    pending_resolver_->RejectWithDOMException(
        DOMExceptionCode::kInvalidStateError, "Context is lost.");
    pending_resolver_.Clear();
  }
}

base::expected<void, String> MLGraphBuilder::ValidateGraphBuilderState() const {
  if (has_built_) {
    return base::unexpected(kGraphAlreadyBuiltError);
  }
  if (!remote_.is_bound()) {
    return base::unexpected("Context is lost.");
  }
  return base::ok();
}

// As specified in https://www.w3.org/TR/webnn/#mlgraphbuilder-validate-operand.
base::expected<void, String> MLGraphBuilder::ValidateInput(
    const MLOperand* input) {
  CHECK(input);
  if (input->Builder() != this) {
    return base::unexpected("Invalid input: Created from another builder.");
  }
  return base::ok();
}

base::expected<void, String> MLGraphBuilder::ValidateInputs(
    const HeapVector<Member<MLOperand>>& inputs) {
  for (const MLOperand* input_to_validate : inputs) {
    RETURN_IF_ERROR(ValidateInput(input_to_validate));
  }
  return base::ok();
}

}  // namespace blink
