// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_UTILS_H_
#define SERVICES_WEBNN_WEBNN_UTILS_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"

namespace webnn {

namespace ops {
inline constexpr char kArgMax[] = "argMax";
inline constexpr char kArgMin[] = "argMin";
inline constexpr char kBatchNormalization[] = "batchNormalization";
inline constexpr char kClamp[] = "clamp";
inline constexpr char kConcat[] = "concat";
inline constexpr char kElu[] = "elu";
inline constexpr char kExpand[] = "expand";
inline constexpr char kGather[] = "gather";
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
inline constexpr char kRelu[] = "relu";
inline constexpr char kResample2d[] = "resample2d";
inline constexpr char kReshape[] = "reshape";
inline constexpr char kSigmoid[] = "sigmoid";
inline constexpr char kSlice[] = "slice";
inline constexpr char kSoftmax[] = "softmax";
inline constexpr char kSoftplus[] = "softplus";
inline constexpr char kSoftsign[] = "softsign";
inline constexpr char kSplit[] = "split";
inline constexpr char kTanh[] = "tanh";
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

// elementwise unary ops.
inline constexpr char kAbs[] = "abs";
inline constexpr char kCeil[] = "ceil";
inline constexpr char kCos[] = "cos";
inline constexpr char kExp[] = "exp";
inline constexpr char kFloor[] = "floor";
inline constexpr char kLog[] = "log";
inline constexpr char kNeg[] = "neg";
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

std::string COMPONENT_EXPORT(WEBNN_UTILS)
    OpTagToString(mojom::Operation::Tag tag);
std::string COMPONENT_EXPORT(WEBNN_UTILS)
    OpKindToString(mojom::ArgMinMax::Kind kind);
std::string COMPONENT_EXPORT(WEBNN_UTILS)
    OpKindToString(mojom::ElementWiseBinary::Kind kind);
std::string COMPONENT_EXPORT(WEBNN_UTILS)
    OpKindToString(mojom::ElementWiseUnary::Kind kind);
std::string COMPONENT_EXPORT(WEBNN_UTILS)
    OpKindToString(mojom::Reduce::Kind kind);
std::string COMPONENT_EXPORT(WEBNN_UTILS)
    DataTypeToString(mojom::Operand::DataType type);
std::string COMPONENT_EXPORT(WEBNN_UTILS) GetOpName(const mojom::Operation& op);
std::string COMPONENT_EXPORT(WEBNN_UTILS)
    NotSupportedOperatorError(const mojom::Operation& op);
std::string COMPONENT_EXPORT(WEBNN_UTILS)
    NotSupportedOperatorError(const mojom::ElementWiseUnary& op);
std::string COMPONENT_EXPORT(WEBNN_UTILS)
    NotSupportedArgumentTypeError(std::string_view op_name,
                                  std::string_view argument_name,
                                  mojom::Operand::DataType type);
std::string COMPONENT_EXPORT(WEBNN_UTILS)
    NotSupportedConstantTypeError(mojom::Operand::DataType type);
std::string COMPONENT_EXPORT(WEBNN_UTILS)
    NotSupportedInputArgumentTypeError(std::string_view op_name,
                                       mojom::Operand::DataType type);
std::string COMPONENT_EXPORT(WEBNN_UTILS)
    NotSupportedInputTypeError(std::string_view input_name,
                               mojom::Operand::DataType type);
std::string COMPONENT_EXPORT(WEBNN_UTILS)
    NotSupportedOptionTypeError(std::string_view op_name,
                                std::string_view option_name,
                                mojom::Operand::DataType type);

// The length of `permutation` must be the same as `array`. The values in
// `permutation` must be within the range [0, N-1] where N is the length of
// `array`. There must be no two or more same values in `permutation`.
//
// e.g., Given an array of [10, 11, 12, 13] and a permutation of [0, 2, 3, 1],
// the permuted array would be [10, 12, 13, 11].
std::vector<uint32_t> COMPONENT_EXPORT(WEBNN_UTILS)
    PermuteArray(base::span<const uint32_t> array,
                 base::span<const uint32_t> permutation);

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_UTILS_H_
