// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_utils.h"

#include <algorithm>
#include <set>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/types/fixed_array.h"
#include "services/webnn/public/cpp/webnn_errors.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "third_party/tflite/buildflags.h"

#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
#include "third_party/xnnpack/src/include/xnnpack.h"  // nogncheck
#endif  // BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)

namespace webnn {

namespace {

#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
BASE_FEATURE(kWebNNUseXNNPackForConstantTransposeFolding,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)

size_t GetLinearOffset(base::span<const uint32_t> multi_dim_index,
                       base::span<const uint32_t> strides) {
  size_t offset = 0;
  for (size_t i = 0; i < multi_dim_index.size(); ++i) {
    offset += base::strict_cast<size_t>(multi_dim_index[i]) * strides[i];
  }
  return offset;
}

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

  if (std::ranges::any_of(axes, [rank](uint32_t axis) {
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
    case mojom::Operation::Tag::kReverse:
      return ops::kReverse;
    case mojom::Operation::Tag::kScatterElements:
      return ops::kScatterElements;
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
    case mojom::ElementWiseBinary::Kind::kNotEqual:
      return ops::kNotEqual;
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
    case mojom::ElementWiseUnary::Kind::kRoundEven:
      return ops::kRoundEven;
    case mojom::ElementWiseUnary::Kind::kSign:
      return ops::kSign;
    case mojom::ElementWiseUnary::Kind::kSin:
      return ops::kSin;
    case mojom::ElementWiseUnary::Kind::kTan:
      return ops::kTan;
    case mojom::ElementWiseUnary::Kind::kIsNaN:
      return ops::kIsNaN;
    case mojom::ElementWiseUnary::Kind::kIsInfinite:
      return ops::kIsInfinite;
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
    case mojom::ElementWiseBinary::Kind::kNotEqual:
    case mojom::ElementWiseBinary::Kind::kLogicalAnd:
    case mojom::ElementWiseBinary::Kind::kLogicalOr:
    case mojom::ElementWiseBinary::Kind::kLogicalXor:
      return true;
  }
}

bool IsLogicalElementWiseUnary(mojom::ElementWiseUnary::Kind kind) {
  switch (kind) {
    case mojom::ElementWiseUnary::Kind::kIsNaN:
    case mojom::ElementWiseUnary::Kind::kIsInfinite:
    case mojom::ElementWiseUnary::Kind::kLogicalNot:
      return true;
    default:
      return false;
  }
}

std::vector<uint32_t> CalculateStrides(base::span<const uint32_t> dimensions) {
  size_t rank = dimensions.size();
  std::vector<uint32_t> strides(rank);
  base::CheckedNumeric<uint32_t> stride = 1;
  for (size_t i = rank; i-- > 0;) {
    strides[i] = stride.ValueOrDie();
    stride *= dimensions[i];
  }
  CHECK(stride.IsValid());
  return strides;
}

webnn::Pool2dKind FromMojoPool2dType(mojom::Pool2d::Kind kind) {
  switch (kind) {
    case mojom::Pool2d::Kind::kAveragePool2d:
      return webnn::Pool2dKind::kAverage;
    case mojom::Pool2d::Kind::kL2Pool2d:
      return webnn::Pool2dKind::kL2;
    case mojom::Pool2d::Kind::kMaxPool2d:
      return webnn::Pool2dKind::kMax;
  }
}

webnn::ReduceKind FromMojoReduceType(mojom::Reduce::Kind kind) {
  switch (kind) {
    case mojom::Reduce::Kind::kL1:
      return webnn::ReduceKind::kL1;
    case mojom::Reduce::Kind::kL2:
      return webnn::ReduceKind::kL2;
    case mojom::Reduce::Kind::kLogSum:
      return webnn::ReduceKind::kLogSum;
    case mojom::Reduce::Kind::kLogSumExp:
      return webnn::ReduceKind::kLogSumExp;
    case mojom::Reduce::Kind::kMax:
      return webnn::ReduceKind::kMax;
    case mojom::Reduce::Kind::kMean:
      return webnn::ReduceKind::kMean;
    case mojom::Reduce::Kind::kMin:
      return webnn::ReduceKind::kMin;
    case mojom::Reduce::Kind::kProduct:
      return webnn::ReduceKind::kProduct;
    case mojom::Reduce::Kind::kSum:
      return webnn::ReduceKind::kSum;
    case mojom::Reduce::Kind::kSumSquare:
      return webnn::ReduceKind::kSumSquare;
  }
}

webnn::PaddingMode FromMojoPaddingMode(mojom::PaddingMode::Tag tag) {
  switch (tag) {
    case mojom::PaddingMode::Tag::kConstant:
      return webnn::PaddingMode::kConstant;
    case mojom::PaddingMode::Tag::kEdge:
      return webnn::PaddingMode::kEdge;
    case mojom::PaddingMode::Tag::kReflection:
      return webnn::PaddingMode::kReflection;
  }
}

base::HeapArray<uint8_t> TransposeConstantData(
    base::span<const uint8_t> data,
    base::span<const uint32_t> shape,
    base::span<const uint32_t> permutation,
    size_t element_size) {
  const size_t rank = shape.size();
  CHECK_EQ(rank, permutation.size());
  CHECK_GT(element_size, 0u);

  base::FixedArray<uint32_t> transposed_shape(rank);
  for (size_t i = 0; i < rank; ++i) {
    transposed_shape[i] = shape[permutation[i]];
  }

  auto transposed_data = base::HeapArray<uint8_t>::Uninit(data.size());

#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
  if (base::FeatureList::IsEnabled(
          kWebNNUseXNNPackForConstantTransposeFolding) &&
      rank <= XNN_MAX_TENSOR_DIMS) {
    base::FixedArray<size_t> xnn_shape(rank);
    base::FixedArray<size_t> xnn_perm(rank);
    for (size_t i = 0; i < rank; ++i) {
      xnn_shape[i] = shape[i];
      xnn_perm[i] = permutation[i];
    }

    switch (element_size) {
      case 1: {
        xnn_status status = xnn_run_transpose_nd_x8(
            data.data(), transposed_data.data(), rank, xnn_shape.data(),
            xnn_perm.data(), 0, nullptr);
        CHECK_EQ(status, xnn_status_success);
        break;
      }
      case 2: {
        xnn_status status = xnn_run_transpose_nd_x16(
            data.data(), transposed_data.data(), rank, xnn_shape.data(),
            xnn_perm.data(), 0, nullptr);
        CHECK_EQ(status, xnn_status_success);
        break;
      }
      case 4: {
        xnn_status status = xnn_run_transpose_nd_x32(
            data.data(), transposed_data.data(), rank, xnn_shape.data(),
            xnn_perm.data(), 0, nullptr);
        CHECK_EQ(status, xnn_status_success);
        break;
      }
      case 8: {
        xnn_status status = xnn_run_transpose_nd_x64(
            data.data(), transposed_data.data(), rank, xnn_shape.data(),
            xnn_perm.data(), 0, nullptr);
        CHECK_EQ(status, xnn_status_success);
        break;
      }
      default:
        NOTREACHED() << "Unsupported element size: " << element_size;
    }
    return transposed_data;
  }
#endif  // BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)

  // `permutation` maps a result axis to the source axis it comes from; the
  // inverse maps a source axis to where it ends up.
  base::FixedArray<uint32_t> inverse_permutation(rank);
  for (size_t i = 0; i < rank; ++i) {
    inverse_permutation[permutation[i]] = base::checked_cast<uint32_t>(i);
  }

  const std::vector<uint32_t> original_strides = CalculateStrides(shape);
  const std::vector<uint32_t> transposed_strides =
      CalculateStrides(transposed_shape);

  size_t number_of_elements = 1;
  for (size_t i = 0; i < rank; ++i) {
    number_of_elements *= shape[i];
  }

  base::span<uint8_t> transposed_span = transposed_data.as_span();
  base::FixedArray<uint32_t> transposed_idx(rank, 0);
  base::FixedArray<uint32_t> original_idx(rank);

  for (size_t i = 0; i < number_of_elements; ++i) {
    for (size_t d = 0; d < rank; ++d) {
      original_idx[d] = transposed_idx[inverse_permutation[d]];
    }

    size_t original_offset = GetLinearOffset(original_idx, original_strides);
    size_t transposed_offset =
        GetLinearOffset(transposed_idx, transposed_strides);

    transposed_span.subspan(transposed_offset * element_size, element_size)
        .copy_from(data.subspan(original_offset * element_size, element_size));

    for (size_t dimension = rank; dimension-- > 0;) {
      transposed_idx[dimension]++;
      if (transposed_idx[dimension] < transposed_shape[dimension]) {
        // Not overflowed, continue to next element.
        break;
      }
      // Reset and carry over.
      transposed_idx[dimension] = 0;
    }
  }

  return transposed_data;
}

}  // namespace webnn
