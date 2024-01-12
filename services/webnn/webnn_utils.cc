// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_utils.h"

namespace webnn {

std::string OpTagToString(mojom::Operation::Tag tag) {
  switch (tag) {
    case mojom::Operation::Tag::kArgMinMax:
      return "argMin/Max";
    case mojom::Operation::Tag::kBatchNormalization:
      return "batchNormalization";
    case mojom::Operation::Tag::kClamp:
      return "clamp";
    case mojom::Operation::Tag::kConcat:
      return "concat";
    case mojom::Operation::Tag::kConv2d:
      return "conv2d";
    case mojom::Operation::Tag::kElementWiseBinary:
      return "element-wise binary";
    case mojom::Operation::Tag::kElu:
      return "elu";
    case mojom::Operation::Tag::kElementWiseUnary:
      return "element-wise unary";
    case mojom::Operation::Tag::kExpand:
      return "expand";
    case mojom::Operation::Tag::kGather:
      return "gather";
    case mojom::Operation::Tag::kGemm:
      return "gemm";
    case mojom::Operation::Tag::kHardSigmoid:
      return "hardSigmoid";
    case mojom::Operation::Tag::kInstanceNormalization:
      return "instanceNormalization";
    case mojom::Operation::Tag::kLayerNormalization:
      return "layerNormalization";
    case mojom::Operation::Tag::kLeakyRelu:
      return "leakyRelu";
    case mojom::Operation::Tag::kLinear:
      return "linear";
    case mojom::Operation::Tag::kMatmul:
      return "matmul";
    case mojom::Operation::Tag::kPad:
      return "pad";
    case mojom::Operation::Tag::kPool2d:
      return "pool2d";
    case mojom::Operation::Tag::kPrelu:
      return "prelu";
    case mojom::Operation::Tag::kReduce:
      return "reduce";
    case mojom::Operation::Tag::kRelu:
      return "relu";
    case mojom::Operation::Tag::kResample2d:
      return "resample2d";
    case mojom::Operation::Tag::kReshape:
      return "reshape";
    case mojom::Operation::Tag::kSigmoid:
      return "sigmoid";
    case mojom::Operation::Tag::kSlice:
      return "slice";
    case mojom::Operation::Tag::kSoftmax:
      return "softmax";
    case mojom::Operation::Tag::kSoftplus:
      return "softplus";
    case mojom::Operation::Tag::kSoftsign:
      return "softsign";
    case mojom::Operation::Tag::kSplit:
      return "split";
    case mojom::Operation::Tag::kTanh:
      return "tanh";
    case mojom::Operation::Tag::kTranspose:
      return "transpose";
    case mojom::Operation::Tag::kWhere:
      return "where";
  }
  NOTREACHED_NORETURN();
}

std::string OpKindToString(mojom::ArgMinMax::Kind kind) {
  switch (kind) {
    case mojom::ArgMinMax::Kind::kMin:
      return "ArgMin";
    case mojom::ArgMinMax::Kind::kMax:
      return "ArgMax";
  }
  NOTREACHED_NORETURN();
}

std::string OpKindToString(mojom::ElementWiseBinary::Kind kind) {
  switch (kind) {
    case mojom::ElementWiseBinary::Kind::kAdd:
      return "add";
    case mojom::ElementWiseBinary::Kind::kSub:
      return "sub";
    case mojom::ElementWiseBinary::Kind::kMul:
      return "mul";
    case mojom::ElementWiseBinary::Kind::kDiv:
      return "div";
    case mojom::ElementWiseBinary::Kind::kMax:
      return "max";
    case mojom::ElementWiseBinary::Kind::kMin:
      return "min";
    case mojom::ElementWiseBinary::Kind::kPow:
      return "pow";
    case mojom::ElementWiseBinary::Kind::kEqual:
      return "equal";
    case mojom::ElementWiseBinary::Kind::kGreater:
      return "greater";
    case mojom::ElementWiseBinary::Kind::kGreaterOrEqual:
      return "greaterOrEqual";
    case mojom::ElementWiseBinary::Kind::kLesser:
      return "lesser";
    case mojom::ElementWiseBinary::Kind::kLesserOrEqual:
      return "lesserOrEqual";
  }
  NOTREACHED_NORETURN();
}

std::string OpKindToString(mojom::ElementWiseUnary::Kind kind) {
  switch (kind) {
    case mojom::ElementWiseUnary::Kind::kAbs:
      return "abs";
    case mojom::ElementWiseUnary::Kind::kCeil:
      return "ceil";
    case mojom::ElementWiseUnary::Kind::kCos:
      return "cos";
    case mojom::ElementWiseUnary::Kind::kExp:
      return "exp";
    case mojom::ElementWiseUnary::Kind::kFloor:
      return "floor";
    case mojom::ElementWiseUnary::Kind::kLog:
      return "log";
    case mojom::ElementWiseUnary::Kind::kNeg:
      return "neg";
    case mojom::ElementWiseUnary::Kind::kSin:
      return "sin";
    case mojom::ElementWiseUnary::Kind::kTan:
      return "tan";
    case mojom::ElementWiseUnary::Kind::kLogicalNot:
      return "logicalNot";
    case mojom::ElementWiseUnary::Kind::kIdentity:
      return "identity";
    case mojom::ElementWiseUnary::Kind::kSqrt:
      return "sqrt";
    case mojom::ElementWiseUnary::Kind::kErf:
      return "erf";
    case mojom::ElementWiseUnary::Kind::kReciprocal:
      return "reciprocal";
    case mojom::ElementWiseUnary::Kind::kCast:
      return "cast";
  }
  NOTREACHED_NORETURN();
}

std::string OpKindToString(mojom::Reduce::Kind kind) {
  switch (kind) {
    case mojom::Reduce::Kind::kL1:
      return "ReduceL1";
    case mojom::Reduce::Kind::kL2:
      return "ReduceL2";
    case mojom::Reduce::Kind::kLogSum:
      return "ReduceLogSum";
    case mojom::Reduce::Kind::kLogSumExp:
      return "ReduceLogSumExp";
    case mojom::Reduce::Kind::kMax:
      return "ReduceMax";
    case mojom::Reduce::Kind::kMean:
      return "ReduceMean";
    case mojom::Reduce::Kind::kMin:
      return "ReduceMin";
    case mojom::Reduce::Kind::kProduct:
      return "ReduceProduct";
    case mojom::Reduce::Kind::kSum:
      return "ReduceSum";
    case mojom::Reduce::Kind::kSumSquare:
      return "ReduceSumSquare";
  }
  NOTREACHED_NORETURN();
}

std::string DataTypeToString(mojom::Operand::DataType type) {
  switch (type) {
    case mojom::Operand::DataType::kFloat32:
      return "float32";
    case mojom::Operand::DataType::kFloat16:
      return "float16";
    case mojom::Operand::DataType::kInt32:
      return "int32";
    case mojom::Operand::DataType::kUint32:
      return "uint32";
    case mojom::Operand::DataType::kInt8:
      return "int8";
    case mojom::Operand::DataType::kUint8:
      return "uint8";
    case mojom::Operand::DataType::kInt64:
      return "int64";
    case mojom::Operand::DataType::kUint64:
      return "uint64";
  }
  NOTREACHED_NORETURN();
}

}  // namespace webnn
