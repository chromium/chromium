// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_WEBNN_ERRORS_H_
#define SERVICES_WEBNN_PUBLIC_CPP_WEBNN_ERRORS_H_

#include <string>

#include "base/component_export.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/supported_data_types.h"

namespace webnn {

namespace ops {
inline constexpr char kArgMax[] = "argMax";
inline constexpr char kArgMin[] = "argMin";
inline constexpr char kBatchNormalization[] = "batchNormalization";
inline constexpr char kClamp[] = "clamp";
inline constexpr char kConcat[] = "concat";
inline constexpr char kCumulativeSum[] = "cumulativeSum";
inline constexpr char kDequantizeLinear[] = "dequantizeLinear";
inline constexpr char kElu[] = "elu";
inline constexpr char kExpand[] = "expand";
inline constexpr char kGather[] = "gather";
inline constexpr char kGatherElements[] = "gatherElements";
inline constexpr char kGatherNd[] = "gatherND";
inline constexpr char kGelu[] = "gelu";
inline constexpr char kGemm[] = "gemm";
inline constexpr char kGru[] = "gru";
inline constexpr char kGruCell[] = "gruCell";
inline constexpr char kHardSigmoid[] = "hardSigmoid";
inline constexpr char kHardSwish[] = "hardSwish";
inline constexpr char kInstanceNormalization[] = "instanceNormalization";
inline constexpr char kLayerNormalization[] = "layerNormalization";
inline constexpr char kLeakyRelu[] = "leakyRelu";
inline constexpr char kLinear[] = "linear";
inline constexpr char kLstm[] = "lstm";
inline constexpr char kLstmCell[] = "lstmCell";
inline constexpr char kMatmul[] = "matmul";
inline constexpr char kPad[] = "pad";
inline constexpr char kPrelu[] = "prelu";
inline constexpr char kQuantizeLinear[] = "quantizeLinear";
inline constexpr char kRelu[] = "relu";
inline constexpr char kResample2d[] = "resample2d";
inline constexpr char kReshape[] = "reshape";
inline constexpr char kScatterND[] = "scatterND";
inline constexpr char kSigmoid[] = "sigmoid";
inline constexpr char kSlice[] = "slice";
inline constexpr char kSoftmax[] = "softmax";
inline constexpr char kSoftplus[] = "softplus";
inline constexpr char kSoftsign[] = "softsign";
inline constexpr char kSplit[] = "split";
inline constexpr char kTanh[] = "tanh";
inline constexpr char kTile[] = "tile";
inline constexpr char kTranspose[] = "transpose";
inline constexpr char kTriangular[] = "triangular";
inline constexpr char kWhere[] = "where";

// conv2d ops.
inline constexpr char kConv2d[] = "conv2d";
inline constexpr char kConvTranspose2d[] = "convTranspose2d";

// elementwise binary ops.
inline constexpr char kAdd[] = "add";
inline constexpr char kSub[] = "sub";
inline constexpr char kMul[] = "mul";
inline constexpr char kDiv[] = "div";
inline constexpr char kMax[] = "max";
inline constexpr char kMin[] = "min";
inline constexpr char kPow[] = "pow";
inline constexpr char kEqual[] = "equal";
inline constexpr char kGreater[] = "greater";
inline constexpr char kGreaterOrEqual[] = "greaterOrEqual";
inline constexpr char kLesser[] = "lesser";
inline constexpr char kLesserOrEqual[] = "lesserOrEqual";
inline constexpr char kLogicalAnd[] = "logicalAnd";
inline constexpr char kLogicalOr[] = "logicalOr";
inline constexpr char kLogicalXor[] = "logicalXor";

// elementwise unary ops.
inline constexpr char kAbs[] = "abs";
inline constexpr char kCeil[] = "ceil";
inline constexpr char kCos[] = "cos";
inline constexpr char kExp[] = "exp";
inline constexpr char kFloor[] = "floor";
inline constexpr char kLog[] = "log";
inline constexpr char kNeg[] = "neg";
inline constexpr char kSign[] = "sign";
inline constexpr char kSin[] = "sin";
inline constexpr char kTan[] = "tan";
inline constexpr char kLogicalNot[] = "logicalNot";
inline constexpr char kIdentity[] = "identity";
inline constexpr char kSqrt[] = "sqrt";
inline constexpr char kErf[] = "erf";
inline constexpr char kReciprocal[] = "reciprocal";
inline constexpr char kCast[] = "cast";

// pooling
inline constexpr char kAveragePool2d[] = "averagePool2d";
inline constexpr char kL2Pool2d[] = "l2Pool2d";
inline constexpr char kMaxPool2d[] = "maxPool2d";

// reduce ops.
inline constexpr char kReduceL1[] = "reduceL1";
inline constexpr char kReduceL2[] = "reduceL2";
inline constexpr char kReduceLogSum[] = "reduceLogSum";
inline constexpr char kReduceLogSumExp[] = "reduceLogSumExp";
inline constexpr char kReduceMax[] = "reduceMax";
inline constexpr char kReduceMean[] = "reduceMean";
inline constexpr char kReduceMin[] = "reduceMin";
inline constexpr char kReduceProduct[] = "reduceProduct";
inline constexpr char kReduceSum[] = "reduceSum";
inline constexpr char kReduceSumSquare[] = "reduceSumSquare";

}  // namespace ops

std::string COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
    DataTypeToString(OperandDataType type);
std::string COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
    NotSupportedArgumentTypeError(std::string_view argument_name,
                                  OperandDataType type,
                                  SupportedDataTypes supported_types);
std::string COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
    NotSupportedConstantTypeError(OperandDataType type,
                                  SupportedDataTypes supported_types);
std::string COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
    NotSupportedInputArgumentTypeError(OperandDataType type,
                                       SupportedDataTypes supported_types);
std::string COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
    NotSupportedInputTypeError(std::string_view input_name,
                               OperandDataType type,
                               SupportedDataTypes supported_types);
std::string COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
    NotSupportedOpOutputTypeError(OperandDataType type,
                                  SupportedDataTypes supported_types);
std::string COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
    NotSupportedOutputTypeError(std::string_view output_name,
                                OperandDataType type,
                                SupportedDataTypes supported_types);
std::string COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
    NotSupportedMLTensorTypeError(OperandDataType type,
                                  SupportedDataTypes supported_types);

std::string COMPONENT_EXPORT(WEBNN_PUBLIC_CPP)
    GetErrorLabelPrefix(std::string_view label);

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_WEBNN_ERRORS_H_
