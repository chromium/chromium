// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/ml_operator.h"

#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_arg_min_max_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_cumulative_sum_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gru_cell_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_gru_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_lstm_cell_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_lstm_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_pad_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_split_options.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_operand.h"

namespace blink {

// static
String MLOperator::OperatorKindToString(
    webnn::mojom::blink::Operation::Tag kind,
    OperationSubKind sub_kind) {
  switch (kind) {
    case webnn::mojom::blink::Operation::Tag::kArgMinMax: {
      switch (absl::get<webnn::mojom::blink::ArgMinMax::Kind>(sub_kind)) {
        case webnn::mojom::blink::ArgMinMax::Kind::kMin:
          return "argMin";
        case webnn::mojom::blink::ArgMinMax::Kind::kMax:
          return "argMax";
      }
    }
    case webnn::mojom::blink::Operation::Tag::kBatchNormalization:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "batchNormalization";
    case webnn::mojom::blink::Operation::Tag::kElementWiseBinary: {
      switch (
          absl::get<webnn::mojom::blink::ElementWiseBinary::Kind>(sub_kind)) {
        case webnn::mojom::blink::ElementWiseBinary::Kind::kAdd:
          return "add";
        case webnn::mojom::blink::ElementWiseBinary::Kind::kSub:
          return "sub";
        case webnn::mojom::blink::ElementWiseBinary::Kind::kMul:
          return "mul";
        case webnn::mojom::blink::ElementWiseBinary::Kind::kDiv:
          return "div";
        case webnn::mojom::blink::ElementWiseBinary::Kind::kMin:
          return "min";
        case webnn::mojom::blink::ElementWiseBinary::Kind::kMax:
          return "max";
        case webnn::mojom::blink::ElementWiseBinary::Kind::kPow:
          return "pow";
        case webnn::mojom::blink::ElementWiseBinary::Kind::kEqual:
          return "equal";
        case webnn::mojom::blink::ElementWiseBinary::Kind::kGreater:
          return "greater";
        case webnn::mojom::blink::ElementWiseBinary::Kind::kGreaterOrEqual:
          return "greaterOrEqual";
        case webnn::mojom::blink::ElementWiseBinary::Kind::kLesser:
          return "lesser";
        case webnn::mojom::blink::ElementWiseBinary::Kind::kLesserOrEqual:
          return "lesserOrEqual";
        case webnn::mojom::blink::ElementWiseBinary::Kind::kLogicalAnd:
          return "logicalAnd";
        case webnn::mojom::blink::ElementWiseBinary::Kind::kLogicalOr:
          return "logicalOr";
        case webnn::mojom::blink::ElementWiseBinary::Kind::kLogicalXor:
          return "logicalXor";
      }
    }
    case webnn::mojom::blink::Operation::Tag::kClamp:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "clamp";
    case webnn::mojom::blink::Operation::Tag::kConcat:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "concat";
    case webnn::mojom::blink::Operation::Tag::kConv2d: {
      switch (absl::get<webnn::mojom::blink::Conv2d::Kind>(sub_kind)) {
        case webnn::mojom::blink::Conv2d::Kind::kDirect:
          return "conv2d";
        case webnn::mojom::blink::Conv2d::Kind::kTransposed:
          return "convTranspose2d";
      }
    }
    case webnn::mojom::blink::Operation::Tag::kCumulativeSum:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "cumulativeSum";
    case webnn::mojom::blink::Operation::Tag::kDequantizeLinear:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "dequantizeLinear";
    case webnn::mojom::blink::Operation::Tag::kElementWiseUnary: {
      switch (
          absl::get<webnn::mojom::blink::ElementWiseUnary::Kind>(sub_kind)) {
        case webnn::mojom::blink::ElementWiseUnary::Kind::kAbs:
          return "abs";
        case webnn::mojom::blink::ElementWiseUnary::Kind::kCast:
          return "cast";
        case webnn::mojom::blink::ElementWiseUnary::Kind::kCeil:
          return "ceil";
        case webnn::mojom::blink::ElementWiseUnary::Kind::kCos:
          return "cos";
        case webnn::mojom::blink::ElementWiseUnary::Kind::kExp:
          return "exp";
        case webnn::mojom::blink::ElementWiseUnary::Kind::kFloor:
          return "floor";
        case webnn::mojom::blink::ElementWiseUnary::Kind::kLog:
          return "log";
        case webnn::mojom::blink::ElementWiseUnary::Kind::kNeg:
          return "neg";
        case webnn::mojom::blink::ElementWiseUnary::Kind::kSign:
          return "sign";
        case webnn::mojom::blink::ElementWiseUnary::Kind::kSin:
          return "sin";
        case webnn::mojom::blink::ElementWiseUnary::Kind::kTan:
          return "tan";
        case webnn::mojom::blink::ElementWiseUnary::Kind::kErf:
          return "erf";
        case webnn::mojom::blink::ElementWiseUnary::Kind::kIdentity:
          return "identity";
        case webnn::mojom::blink::ElementWiseUnary::Kind::kLogicalNot:
          return "logicalNot";
        case webnn::mojom::blink::ElementWiseUnary::Kind::kReciprocal:
          return "reciprocal";
        case webnn::mojom::blink::ElementWiseUnary::Kind::kSqrt:
          return "sqrt";
      }
    }
    case webnn::mojom::blink::Operation::Tag::kInstanceNormalization:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "instanceNormalization";
    case webnn::mojom::blink::Operation::Tag::kLayerNormalization:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "layerNormalization";
    case webnn::mojom::blink::Operation::Tag::kLeakyRelu:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "leakyRelu";
    case webnn::mojom::blink::Operation::Tag::kLinear:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "linear";
    case webnn::mojom::blink::Operation::Tag::kLstm:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "lstm";
    case webnn::mojom::blink::Operation::Tag::kLstmCell:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "lstmCell";
    case webnn::mojom::blink::Operation::Tag::kElu:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "elu";
    case webnn::mojom::blink::Operation::Tag::kExpand:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "expand";
    case webnn::mojom::blink::Operation::Tag::kGather:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "gather";
    case webnn::mojom::blink::Operation::Tag::kGatherElements:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "gatherElements";
    case webnn::mojom::blink::Operation::Tag::kGatherNd:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "gatherND";
    case webnn::mojom::blink::Operation::Tag::kGelu:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "gelu";
    case webnn::mojom::blink::Operation::Tag::kGemm:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "gemm";
    case webnn::mojom::blink::Operation::Tag::kGru:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "gru";
    case webnn::mojom::blink::Operation::Tag::kGruCell:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "gruCell";
    case webnn::mojom::blink::Operation::Tag::kHardSigmoid:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "hardSigmoid";
    case webnn::mojom::blink::Operation::Tag::kHardSwish:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "hardSwish";
    case webnn::mojom::blink::Operation::Tag::kPool2d: {
      switch (absl::get<webnn::mojom::blink::Pool2d::Kind>(sub_kind)) {
        case webnn::mojom::blink::Pool2d::Kind::kAveragePool2d:
          return "averagePool2d";
        case webnn::mojom::blink::Pool2d::Kind::kL2Pool2d:
          return "l2Pool2d";
        case webnn::mojom::blink::Pool2d::Kind::kMaxPool2d:
          return "maxPool2d";
      }
    }
    case webnn::mojom::blink::Operation::Tag::kMatmul:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "matmul";
    case webnn::mojom::blink::Operation::Tag::kPad:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "pad";
    case webnn::mojom::blink::Operation::Tag::kPrelu:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "prelu";
    case webnn::mojom::blink::Operation::Tag::kQuantizeLinear:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "quantizeLinear";
    case webnn::mojom::blink::Operation::Tag::kReduce: {
      switch (absl::get<webnn::mojom::blink::Reduce::Kind>(sub_kind)) {
        case webnn::mojom::blink::Reduce::Kind::kL1:
          return "reduceL1";
        case webnn::mojom::blink::Reduce::Kind::kL2:
          return "reduceL2";
        case webnn::mojom::blink::Reduce::Kind::kLogSum:
          return "reduceLogSum";
        case webnn::mojom::blink::Reduce::Kind::kLogSumExp:
          return "reduceLogSumExp";
        case webnn::mojom::blink::Reduce::Kind::kMax:
          return "reduceMax";
        case webnn::mojom::blink::Reduce::Kind::kMean:
          return "reduceMean";
        case webnn::mojom::blink::Reduce::Kind::kMin:
          return "reduceMin";
        case webnn::mojom::blink::Reduce::Kind::kProduct:
          return "reduceProduct";
        case webnn::mojom::blink::Reduce::Kind::kSum:
          return "reduceSum";
        case webnn::mojom::blink::Reduce::Kind::kSumSquare:
          return "reduceSumSquare";
      }
    }
    case webnn::mojom::blink::Operation::Tag::kRelu:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "relu";
    case webnn::mojom::blink::Operation::Tag::kReshape:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "reshape";
    case webnn::mojom::blink::Operation::Tag::kResample2d:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "resample2d";
    case webnn::mojom::blink::Operation::Tag::kScatterNd:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "scatterND";
    case webnn::mojom::blink::Operation::Tag::kSigmoid:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "sigmoid";
    case webnn::mojom::blink::Operation::Tag::kSoftsign:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "softsign";
    case webnn::mojom::blink::Operation::Tag::kSlice:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "slice";
    case webnn::mojom::blink::Operation::Tag::kSoftmax:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "softmax";
    case webnn::mojom::blink::Operation::Tag::kSoftplus:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "softplus";
    case webnn::mojom::blink::Operation::Tag::kSplit:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "split";
    case webnn::mojom::blink::Operation::Tag::kTanh:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "tanh";
    case webnn::mojom::blink::Operation::Tag::kTile:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "tile";
    case webnn::mojom::blink::Operation::Tag::kTranspose:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "transpose";
    case webnn::mojom::blink::Operation::Tag::kTriangular:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "triangular";
    case webnn::mojom::blink::Operation::Tag::kWhere:
      CHECK(absl::holds_alternative<absl::monostate>(sub_kind));
      return "where";
  }
}

MLOperator::MLOperator(MLGraphBuilder* builder,
                       webnn::mojom::blink::Operation::Tag kind,
                       const MLOperatorOptions* options,
                       OperationSubKind sub_kind)
    : builder_(builder), kind_(kind), options_(options), sub_kind_(sub_kind) {}

MLOperator::~MLOperator() = default;

void MLOperator::Trace(Visitor* visitor) const {
  visitor->Trace(builder_);
  visitor->Trace(options_);
  visitor->Trace(inputs_);
  visitor->Trace(outputs_);
}

webnn::mojom::blink::Operation::Tag MLOperator::Kind() const {
  return kind_;
}

MLOperator::OperationSubKind MLOperator::SubKind() const {
  return sub_kind_;
}

const MLOperatorOptions* MLOperator::Options() const {
  return options_.Get();
}

const HeapVector<Member<const MLOperand>>& MLOperator::Inputs() const {
  return inputs_;
}

const HeapVector<Member<const MLOperand>>& MLOperator::Outputs() const {
  return outputs_;
}

void MLOperator::Connect(HeapVector<Member<const MLOperand>> inputs,
                         HeapVector<Member<const MLOperand>> outputs) {
  DCHECK(!inputs.empty());
  DCHECK(!outputs.empty());
  inputs_ = std::move(inputs);
  outputs_ = std::move(outputs);
}

MLArgMinMaxOperator::MLArgMinMaxOperator(MLGraphBuilder* builder,
                                         OperationSubKind sub_kind,
                                         const uint32_t axis,
                                         const MLArgMinMaxOptions* options)
    : MLOperator(builder,
                 webnn::mojom::blink::Operation::Tag::kArgMinMax,
                 options,
                 sub_kind),
      axis_(axis) {}

MLArgMinMaxOperator::~MLArgMinMaxOperator() = default;

MLConcatOperator::MLConcatOperator(MLGraphBuilder* builder,
                                   const uint32_t axis,
                                   const MLOperatorOptions* options)
    : MLOperator(builder,
                 webnn::mojom::blink::Operation::Tag::kConcat,
                 options),
      axis_(axis) {}

MLConcatOperator::~MLConcatOperator() = default;

uint32_t MLConcatOperator::Axis() const {
  return axis_;
}

MLCumulativeSumOperator::MLCumulativeSumOperator(
    MLGraphBuilder* builder,
    const uint32_t axis,
    const MLCumulativeSumOptions* options)
    : MLOperator(builder,
                 webnn::mojom::blink::Operation::Tag::kCumulativeSum,
                 options),
      axis_(axis) {}

MLCumulativeSumOperator::~MLCumulativeSumOperator() = default;

MLLstmOperator::MLLstmOperator(MLGraphBuilder* builder,
                               uint32_t steps,
                               uint32_t hidden_size,
                               const MLLstmOptions* options)
    : MLOperator(builder, webnn::mojom::blink::Operation::Tag::kLstm, options),
      steps_(steps),
      hidden_size_(hidden_size) {}

MLLstmOperator::~MLLstmOperator() = default;

uint32_t MLLstmOperator::steps() const {
  return steps_;
}

uint32_t MLLstmOperator::hidden_size() const {
  return hidden_size_;
}

MLLstmCellOperator::MLLstmCellOperator(MLGraphBuilder* builder,
                                       uint32_t hidden_size,
                                       const MLLstmCellOptions* options)
    : MLOperator(builder,
                 webnn::mojom::blink::Operation::Tag::kLstmCell,
                 options),
      hidden_size_(hidden_size) {}

MLLstmCellOperator::~MLLstmCellOperator() = default;

uint32_t MLLstmCellOperator::hidden_size() const {
  return hidden_size_;
}

MLGruOperator::MLGruOperator(MLGraphBuilder* builder,
                             uint32_t steps,
                             uint32_t hidden_size,
                             const MLOperatorOptions* options)
    : MLOperator(builder, webnn::mojom::blink::Operation::Tag::kGru, options),
      steps_(steps),
      hidden_size_(hidden_size) {}

MLGruOperator::~MLGruOperator() = default;

MLGruCellOperator::MLGruCellOperator(MLGraphBuilder* builder,
                                     uint32_t hidden_size,
                                     const MLGruCellOptions* options)
    : MLOperator(builder,
                 webnn::mojom::blink::Operation::Tag::kGruCell,
                 options),
      hidden_size_(hidden_size) {}

MLGruCellOperator::~MLGruCellOperator() = default;

MLPadOperator::MLPadOperator(MLGraphBuilder* builder,
                             const Vector<uint32_t>& beginning_padding,
                             const Vector<uint32_t>& ending_padding,
                             const MLPadOptions* options)
    : MLOperator(builder, webnn::mojom::blink::Operation::Tag::kPad, options),
      beginning_padding_(beginning_padding),
      ending_padding_(ending_padding) {}

MLPadOperator::~MLPadOperator() = default;

const Vector<uint32_t>& MLPadOperator::BeginningPadding() const {
  return beginning_padding_;
}

const Vector<uint32_t>& MLPadOperator::EndingPadding() const {
  return ending_padding_;
}

MLSliceOperator::MLSliceOperator(MLGraphBuilder* builder,
                                 const Vector<uint32_t>& starts,
                                 const Vector<uint32_t>& sizes,
                                 const MLOperatorOptions* options)
    : MLOperator(builder, webnn::mojom::blink::Operation::Tag::kSlice, options),
      starts_(starts),
      sizes_(sizes) {}

MLSliceOperator::~MLSliceOperator() = default;

const Vector<uint32_t>& MLSliceOperator::Starts() const {
  return starts_;
}

const Vector<uint32_t>& MLSliceOperator::Sizes() const {
  return sizes_;
}

MLSoftmaxOperator::MLSoftmaxOperator(MLGraphBuilder* builder,
                                     const uint32_t axis,
                                     const MLOperatorOptions* options)
    : MLOperator(builder,
                 webnn::mojom::blink::Operation::Tag::kSoftmax,
                 options),
      axis_(axis) {}

MLSoftmaxOperator::~MLSoftmaxOperator() = default;

MLSplitOperator::MLSplitOperator(MLGraphBuilder* builder,
                                 const uint32_t splits,
                                 const MLSplitOptions* options)
    : MLOperator(builder, webnn::mojom::blink::Operation::Tag::kSplit, options),
      is_even_split_(true),
      split_number_(splits) {}

MLSplitOperator::MLSplitOperator(MLGraphBuilder* builder,
                                 const Vector<uint32_t>& splits,
                                 const MLSplitOptions* options)
    : MLOperator(builder, webnn::mojom::blink::Operation::Tag::kSplit, options),
      is_even_split_(false),
      split_sizes_(splits) {}

MLSplitOperator::~MLSplitOperator() = default;

bool MLSplitOperator::IsEvenSplit() const {
  return is_even_split_;
}

uint32_t MLSplitOperator::SplitNumber() const {
  CHECK(is_even_split_);
  return split_number_;
}

const Vector<uint32_t>& MLSplitOperator::SplitSizes() const {
  CHECK(!is_even_split_);
  return split_sizes_;
}

MLTileOperator::MLTileOperator(MLGraphBuilder* builder,
                               const Vector<uint32_t>& repetitions,
                               const MLOperatorOptions* options)
    : MLOperator(builder, webnn::mojom::blink::Operation::Tag::kTile, options),
      repetitions_(repetitions) {}

MLTileOperator::~MLTileOperator() = default;

const Vector<uint32_t>& MLTileOperator::Repetitions() const {
  return repetitions_;
}

}  // namespace blink
