// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_utils.h"

#include <set>

#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "services/webnn/public/cpp/webnn_errors.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"

namespace webnn {

namespace {

std::string OpKindToString(mojom::Conv2d::Kind kind) {
  switch (kind) {
    case mojom::Conv2d::Kind::kDirect:
      return ops::kConv2d;
    case mojom::Conv2d::Kind::kTransposed:
      return ops::kConvTranspose2d;
  }
  NOTREACHED();
}

std::string OpKindToString(mojom::Pool2d::Kind kind) {
  switch (kind) {
    case mojom::Pool2d::Kind::kAveragePool2d:
      return ops::kAveragePool2d;
    case mojom::Pool2d::Kind::kL2Pool2d:
      return ops::kL2Pool2d;
    case mojom::Pool2d::Kind::kMaxPool2d:
      return ops::kMaxPool2d;
  }
}

// Check 1. no duplicate value in `axes`​, 2. values in `axes` ​​are all
// within [0, N - 1], where N is the length of `axes`.
bool ValidateAxes(base::span<const uint32_t> axes) {
  size_t rank = axes.size();

  if (base::ranges::any_of(axes, [rank](uint32_t axis) {
        return base::checked_cast<size_t>(axis) >= rank;
      })) {
    // All axes should be within range [0, N - 1].
    return false;
  }

  // TODO(crbug.com/40206287): Replace `std::set` with `std::bitset` for
  // duplication check after the maximum number of operand dimensions has been
  // settled and validated before using this function. Use `std::set` here at
  // present to avoid dimensions count check. Dimensions number issue tracked in
  // https://github.com/webmachinelearning/webnn/issues/456.
  if (rank != std::set<uint32_t>(axes.begin(), axes.end()).size()) {
    // Axes should not contain duplicate values.
    return false;
  }

  return true;
}

}  // namespace

std::string OpTagToString(mojom::Operation::Tag tag) {
  switch (tag) {
    case mojom::Operation::Tag::kArgMinMax:
      return "argMin/Max";
    case mojom::Operation::Tag::kBatchNormalization:
      return ops::kBatchNormalization;
    case mojom::Operation::Tag::kClamp:
      return ops::kClamp;
    case mojom::Operation::Tag::kConcat:
      return ops::kConcat;
    case mojom::Operation::Tag::kConv2d:
      return ops::kConv2d;
    case mojom::Operation::Tag::kCumulativeSum:
      return ops::kCumulativeSum;
    case mojom::Operation::Tag::kDequantizeLinear:
      return ops::kDequantizeLinear;
    case mojom::Operation::Tag::kElementWiseBinary:
      return "element-wise binary";
    case mojom::Operation::Tag::kElu:
      return ops::kElu;
    case mojom::Operation::Tag::kElementWiseUnary:
      return "element-wise unary";
    case mojom::Operation::Tag::kExpand:
      return ops::kExpand;
    case mojom::Operation::Tag::kGather:
      return ops::kGather;
    case mojom::Operation::Tag::kGatherElements:
      return ops::kGatherElements;
    case mojom::Operation::Tag::kGatherNd:
      return ops::kGatherNd;
    case mojom::Operation::Tag::kGelu:
      return ops::kGelu;
    case mojom::Operation::Tag::kGemm:
      return ops::kGemm;
    case mojom::Operation::Tag::kGru:
      return ops::kGru;
    case mojom::Operation::Tag::kGruCell:
      return ops::kGruCell;
    case mojom::Operation::Tag::kHardSigmoid:
      return ops::kHardSigmoid;
    case mojom::Operation::Tag::kHardSwish:
      return ops::kHardSwish;
    case mojom::Operation::Tag::kInstanceNormalization:
      return ops::kInstanceNormalization;
    case mojom::Operation::Tag::kLayerNormalization:
      return ops::kLayerNormalization;
    case mojom::Operation::Tag::kLeakyRelu:
      return ops::kLeakyRelu;
    case mojom::Operation::Tag::kLinear:
      return ops::kLinear;
    case mojom::Operation::Tag::kLstm:
      return ops::kLstm;
    case mojom::Operation::Tag::kLstmCell:
      return ops::kLstmCell;
    case mojom::Operation::Tag::kMatmul:
      return ops::kMatmul;
    case mojom::Operation::Tag::kPad:
      return ops::kPad;
    case mojom::Operation::Tag::kPool2d:
      return "pool2d";
    case mojom::Operation::Tag::kPrelu:
      return ops::kPrelu;
    case mojom::Operation::Tag::kQuantizeLinear:
      return ops::kQuantizeLinear;
    case mojom::Operation::Tag::kReduce:
      return "reduce";
    case mojom::Operation::Tag::kRelu:
      return ops::kRelu;
    case mojom::Operation::Tag::kResample2d:
      return ops::kResample2d;
    case mojom::Operation::Tag::kReshape:
      return ops::kReshape;
    case mojom::Operation::Tag::kScatterNd:
      return ops::kScatterND;
    case mojom::Operation::Tag::kSigmoid:
      return ops::kSigmoid;
    case mojom::Operation::Tag::kSlice:
      return ops::kSlice;
    case mojom::Operation::Tag::kSoftmax:
      return ops::kSoftmax;
    case mojom::Operation::Tag::kSoftplus:
      return ops::kSoftplus;
    case mojom::Operation::Tag::kSoftsign:
      return ops::kSoftsign;
    case mojom::Operation::Tag::kSplit:
      return ops::kSplit;
    case mojom::Operation::Tag::kTanh:
      return ops::kTanh;
    case mojom::Operation::Tag::kTile:
      return ops::kTile;
    case mojom::Operation::Tag::kTranspose:
      return ops::kTranspose;
    case mojom::Operation::Tag::kTriangular:
      return ops::kTriangular;
    case mojom::Operation::Tag::kWhere:
      return ops::kWhere;
  }
}

std::string OpKindToString(mojom::ArgMinMax::Kind kind) {
  switch (kind) {
    case mojom::ArgMinMax::Kind::kMin:
      return ops::kArgMin;
    case mojom::ArgMinMax::Kind::kMax:
      return ops::kArgMax;
  }
}

std::string OpKindToString(mojom::ElementWiseBinary::Kind kind) {
  switch (kind) {
    case mojom::ElementWiseBinary::Kind::kAdd:
      return ops::kAdd;
    case mojom::ElementWiseBinary::Kind::kSub:
      return ops::kSub;
    case mojom::ElementWiseBinary::Kind::kMul:
      return ops::kMul;
    case mojom::ElementWiseBinary::Kind::kDiv:
      return ops::kDiv;
    case mojom::ElementWiseBinary::Kind::kMax:
      return ops::kMax;
    case mojom::ElementWiseBinary::Kind::kMin:
      return ops::kMin;
    case mojom::ElementWiseBinary::Kind::kPow:
      return ops::kPow;
    case mojom::ElementWiseBinary::Kind::kEqual:
      return ops::kEqual;
    case mojom::ElementWiseBinary::Kind::kGreater:
      return ops::kGreater;
    case mojom::ElementWiseBinary::Kind::kGreaterOrEqual:
      return ops::kGreaterOrEqual;
    case mojom::ElementWiseBinary::Kind::kLesser:
      return ops::kLesser;
    case mojom::ElementWiseBinary::Kind::kLesserOrEqual:
      return ops::kLesserOrEqual;
    case mojom::ElementWiseBinary::Kind::kLogicalAnd:
      return ops::kLogicalAnd;
    case mojom::ElementWiseBinary::Kind::kLogicalOr:
      return ops::kLogicalOr;
    case mojom::ElementWiseBinary::Kind::kLogicalXor:
      return ops::kLogicalXor;
  }
}

std::string OpKindToString(mojom::ElementWiseUnary::Kind kind) {
  switch (kind) {
    case mojom::ElementWiseUnary::Kind::kAbs:
      return ops::kAbs;
    case mojom::ElementWiseUnary::Kind::kCeil:
      return ops::kCeil;
    case mojom::ElementWiseUnary::Kind::kCos:
      return ops::kCos;
    case mojom::ElementWiseUnary::Kind::kExp:
      return ops::kExp;
    case mojom::ElementWiseUnary::Kind::kFloor:
      return ops::kFloor;
    case mojom::ElementWiseUnary::Kind::kLog:
      return ops::kLog;
    case mojom::ElementWiseUnary::Kind::kNeg:
      return ops::kNeg;
    case mojom::ElementWiseUnary::Kind::kSign:
      return ops::kSign;
    case mojom::ElementWiseUnary::Kind::kSin:
      return ops::kSin;
    case mojom::ElementWiseUnary::Kind::kTan:
      return ops::kTan;
    case mojom::ElementWiseUnary::Kind::kLogicalNot:
      return ops::kLogicalNot;
    case mojom::ElementWiseUnary::Kind::kIdentity:
      return ops::kIdentity;
    case mojom::ElementWiseUnary::Kind::kSqrt:
      return ops::kSqrt;
    case mojom::ElementWiseUnary::Kind::kErf:
      return ops::kErf;
    case mojom::ElementWiseUnary::Kind::kReciprocal:
      return ops::kReciprocal;
    case mojom::ElementWiseUnary::Kind::kCast:
      return ops::kCast;
  }
}

std::string OpKindToString(mojom::Reduce::Kind kind) {
  switch (kind) {
    case mojom::Reduce::Kind::kL1:
      return ops::kReduceL1;
    case mojom::Reduce::Kind::kL2:
      return ops::kReduceL2;
    case mojom::Reduce::Kind::kLogSum:
      return ops::kReduceLogSum;
    case mojom::Reduce::Kind::kLogSumExp:
      return ops::kReduceLogSumExp;
    case mojom::Reduce::Kind::kMax:
      return ops::kReduceMax;
    case mojom::Reduce::Kind::kMean:
      return ops::kReduceMean;
    case mojom::Reduce::Kind::kMin:
      return ops::kReduceMin;
    case mojom::Reduce::Kind::kProduct:
      return ops::kReduceProduct;
    case mojom::Reduce::Kind::kSum:
      return ops::kReduceSum;
    case mojom::Reduce::Kind::kSumSquare:
      return ops::kReduceSumSquare;
  }
}

std::string GetOpName(const mojom::Operation& op) {
  const mojom::Operation::Tag& tag = op.which();
  switch (tag) {
    case mojom::Operation::Tag::kArgMinMax:
      return webnn::OpKindToString(op.get_arg_min_max()->kind);
    case mojom::Operation::Tag::kConv2d:
      return OpKindToString(op.get_conv2d()->kind);
    case mojom::Operation::Tag::kElementWiseBinary:
      return webnn::OpKindToString(op.get_element_wise_binary()->kind);
    case mojom::Operation::Tag::kElementWiseUnary:
      return webnn::OpKindToString(op.get_element_wise_unary()->kind);
    case mojom::Operation::Tag::kReduce:
      return webnn::OpKindToString(op.get_reduce()->kind);
    case mojom::Operation::Tag::kPool2d:
      return OpKindToString(op.get_pool2d()->kind);
    default:
      return OpTagToString(tag);
  }
}

std::string NotSupportedOperatorError(const mojom::Operation& op) {
  return base::StrCat({"Unsupported operator ", GetOpName(op), "."});
}

std::string NotSupportedOperatorError(const mojom::ElementWiseUnary& op) {
  return base::StrCat({"Unsupported operator ", OpKindToString(op.kind), "."});
}

std::string NotSupportedArgumentTypeError(std::string_view op_name,
                                          std::string_view argument_name,
                                          OperandDataType type) {
  return base::StrCat({"Unsupported data type ", DataTypeToString(type),
                       " for ", op_name, " argument ", argument_name, "."});
}

std::string NotSupportedInputArgumentTypeError(std::string_view op_name,
                                               OperandDataType type) {
  return base::StrCat({"Unsupported data type ", DataTypeToString(type),
                       " for ", op_name, " argument input."});
}

std::string NotSupportedOptionTypeError(std::string_view op_name,
                                        std::string_view option_name,
                                        OperandDataType type) {
  return base::StrCat({"Unsupported data type ", DataTypeToString(type),
                       " for ", op_name, " option ", option_name});
}

std::vector<uint32_t> PermuteArray(base::span<const uint32_t> array,
                                   base::span<const uint32_t> permutation) {
  CHECK_EQ(array.size(), permutation.size());
  CHECK(ValidateAxes(permutation));

  size_t arr_size = array.size();
  std::vector<uint32_t> permuted_array(arr_size);
  for (size_t i = 0; i < arr_size; ++i) {
    permuted_array[i] = array[permutation[i]];
  }

  return permuted_array;
}

bool IsLogicalElementWiseBinary(mojom::ElementWiseBinary::Kind kind) {
  switch (kind) {
    case mojom::ElementWiseBinary::Kind::kAdd:
    case mojom::ElementWiseBinary::Kind::kSub:
    case mojom::ElementWiseBinary::Kind::kMul:
    case mojom::ElementWiseBinary::Kind::kDiv:
    case mojom::ElementWiseBinary::Kind::kMax:
    case mojom::ElementWiseBinary::Kind::kMin:
    case mojom::ElementWiseBinary::Kind::kPow:
      return false;
    case mojom::ElementWiseBinary::Kind::kEqual:
    case mojom::ElementWiseBinary::Kind::kGreater:
    case mojom::ElementWiseBinary::Kind::kGreaterOrEqual:
    case mojom::ElementWiseBinary::Kind::kLesser:
    case mojom::ElementWiseBinary::Kind::kLesserOrEqual:
    case mojom::ElementWiseBinary::Kind::kLogicalAnd:
    case mojom::ElementWiseBinary::Kind::kLogicalOr:
    case mojom::ElementWiseBinary::Kind::kLogicalXor:
      return true;
  }
}

}  // namespace webnn
