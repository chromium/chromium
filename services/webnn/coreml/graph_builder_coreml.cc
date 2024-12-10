// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/349653202): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "services/webnn/coreml/graph_builder_coreml.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include "base/bits.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/overloaded.h"
#include "base/json/json_file_value_serializer.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/types/fixed_array.h"
#include "base/unguessable_token.h"
#include "base/uuid.h"
#include "base/values.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/cpp/webnn_errors.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_utils.h"
#include "third_party/coremltools/mlmodel/format/FeatureTypes.pb.h"
#include "third_party/coremltools/mlmodel/format/MIL.pb.h"
#include "third_party/fp16/src/include/fp16.h"

namespace webnn::coreml {

// Documentation for the CoreML MIL Ops:
// https://apple.github.io/coremltools/source/coremltools.converters.mil.mil.ops.defs.html
// For the supported OS versions for any OP, the translation between iOS version
// numbers and macOS version numbers is documented here:
// https://github.com/apple/coremltools/blob/bba83f43859e087d50c7d764cb132e7d4b427611/coremltools/converters/mil/_deployment_compatibility.py#L25
// With regards to parameters annotated as optional, when building the MIL ops
// graph directly in protobuf as is the case here, all parameters are required.
// The optional annotations is intended for the Python API.

namespace {

constexpr char kWriteModelErrorMessage[] = "Failed to serialize Core ML model.";
constexpr char kWriteWeightsErrorMessage[] =
    "Failed to write constant to file.";

const base::FilePath::CharType kMlPackageExtension[] =
    FILE_PATH_LITERAL(".mlpackage");
const base::FilePath::CharType kMlPackageDataDir[] = FILE_PATH_LITERAL("Data");
const base::FilePath::CharType kMlPackageWeightsDir[] =
    FILE_PATH_LITERAL("weights");
const base::FilePath::CharType kMlPackageWeightsFileName[] =
    FILE_PATH_LITERAL("weights.bin");
const base::FilePath::CharType kMlPackageModelFileName[] =
    FILE_PATH_LITERAL("model.mlmodel");
const base::FilePath::CharType kManifestFileName[] =
    FILE_PATH_LITERAL("Manifest.json");

// Information in model package Manifest.json file.
constexpr char kManifestItemAuthorKey[] = "author";
constexpr char kManifestItemAuthorValue[] = "Chromium";
constexpr char kManifestItemDescriptionKey[] = "description";
constexpr char kManifestModelDescriptionValue[] = "CoreML Model Specification";
constexpr char kManifestWeightsDescriptionValue[] = "CoreML Model Weights";
constexpr char kManifestItemNameKey[] = "name";
constexpr char kManifestItemPathKey[] = "path";
constexpr char kManifestModelValue[] = "model.mlmodel";
constexpr char kManifestWeightsValue[] = "weights";
constexpr char kManifestItemInfoEntriesKey[] = "itemInfoEntries";
constexpr char kManifestVersionKey[] = "fileFormatVersion";
constexpr char kManifestVersionValue[] = "1.0.0";
constexpr char kManifestModelIdentifierKey[] = "rootModelIdentifier";

// Prefixes to be added to CoreML entities name identifiers to avoid collision.
constexpr char kInputNamePrefix[] = "input";
constexpr char kOutputNamePrefix[] = "output";
constexpr char kIntermediateOperandPrefix[] = "var";
constexpr char kStringSeparator[] = "_";
// Used for names of internal operands when a WebNN op needs to be
// decomposed into multiple CoreML ops.
constexpr char kInternalNamePrefix[] = "internal";

// Model op related consts.
//
// Special cases.
constexpr char kPlaceholderOuputName[] = "placeholder_output";

// op names
constexpr char kOpConstTypeName[] = "const";
// Generic operators.
constexpr char kOpArgminTypeName[] = "reduce_argmin";
constexpr char kOpArgmaxTypeName[] = "reduce_argmax";
constexpr char kOpBatchNormalizationTypeName[] = "batch_norm";
constexpr char kOpCastTypeName[] = "cast";
constexpr char kOpClipTypeName[] = "clip";
constexpr char kOpConcatTypeName[] = "concat";
constexpr char kOpConv2dTypeName[] = "conv";
constexpr char kOpConvTranspose2dTypeName[] = "conv_transpose";
constexpr char kOpCumulativeSumTypeName[] = "cumsum";
constexpr char kOpEluTypeName[] = "elu";
constexpr char kOpExpandTypeName[] = "tile";
constexpr char kOpFillTypeName[] = "fill";
constexpr char kOpGatherElementsTypeName[] = "gather_along_axis";
constexpr char kOpGatherNdTypeName[] = "gather_nd";
constexpr char kOpGatherTypeName[] = "gather";
constexpr char kOpGeluTypeName[] = "gelu";
constexpr char kOpHardSigmoidTypeName[] = "sigmoid_hard";
constexpr char kOpInstanceNormalizationTypeName[] = "instance_norm";
constexpr char kOpLayerNormalizationTypeName[] = "layer_norm";
constexpr char kOpLeakyReluTypeName[] = "leaky_relu";
constexpr char kOpLstmTypeName[] = "lstm";
constexpr char kOpMatmulTypeName[] = "matmul";
constexpr char kOpPadTypeName[] = "pad";
constexpr char kOpReluTypeName[] = "relu";
constexpr char kOpReshapeTypeName[] = "reshape";
constexpr char kOpScatterElementsTypeName[] = "scatter_along_axis";
constexpr char kOpScatterNDTypeName[] = "scatter_nd";
constexpr char kOpSigmoidTypeName[] = "sigmoid";
constexpr char kOpSliceTypeName[] = "slice_by_index";
constexpr char kOpSoftmaxTypeName[] = "softmax";
constexpr char kOpSoftplusTypeName[] = "softplus";
constexpr char kOpSoftsignTypeName[] = "softsign";
constexpr char kOpSplitTypeName[] = "split";
constexpr char kOpTanhTypeName[] = "tanh";
constexpr char kOpTileTypeName[] = "tile";
constexpr char kOpTransposeTypeName[] = "transpose";
constexpr char kOpTriangularTypeName[] = "band_part";
constexpr char kOpWhereTypeName[] = "select";
// Elementwise binary operators.
constexpr char kOpAddTypeName[] = "add";
constexpr char kOpMultiplyTypeName[] = "mul";
constexpr char kOpDivideTypeName[] = "real_div";
constexpr char kOpSubtractTypeName[] = "sub";
constexpr char kOpMaximumTypeName[] = "maximum";
constexpr char kOpMinimumTypeName[] = "minimum";
constexpr char kOpPowerTypeName[] = "pow";
constexpr char kOpLogicalAnd[] = "logical_and";
constexpr char kOpLogicalOr[] = "logical_or";
constexpr char kOpLogicalXor[] = "logical_xor";
// Elementwise unary operators.
constexpr char kOpLogicalEqual[] = "equal";
constexpr char kOpLogicalGreater[] = "greater";
constexpr char kOpLogicalGreaterEqual[] = "greater_equal";
constexpr char kOpLogicalLess[] = "less";
constexpr char kOpLogicalLessEqual[] = "less_equal";
constexpr char kOpLogicalNot[] = "logical_not";
constexpr char kOpAbsTypeName[] = "abs";
constexpr char kOpCeilTypeName[] = "ceil";
constexpr char kOpCosTypeName[] = "cos";
constexpr char kOpExpTypeName[] = "exp";
constexpr char kOpFloorTypeName[] = "floor";
constexpr char kOpIdentityTypeName[] = "identity";
constexpr char kOpSinTypeName[] = "sin";
constexpr char kOpTanTypeName[] = "tan";
constexpr char kOpErfTypeName[] = "erf";
constexpr char kOpSqrtTypeName[] = "sqrt";
constexpr char kOpReciprocalTypeName[] = "inverse";
constexpr char kOpLogTypeName[] = "log";

// Pooling operators.
constexpr char kOpAvgPoolTypeName[] = "avg_pool";
constexpr char kOpL2PoolTypeName[] = "l2_pool";
constexpr char kOpMaxPoolTypeName[] = "max_pool";
// Reduction operators.
constexpr char kOpReduceL1[] = "reduce_l1_norm";
constexpr char kOpReduceL2[] = "reduce_l2_norm";
constexpr char kOpReduceLogSum[] = "reduce_log_sum";
constexpr char kOpReduceLogSumExp[] = "reduce_log_sum_exp";
constexpr char kOpReduceMax[] = "reduce_max";
constexpr char kOpReduceMean[] = "reduce_mean";
constexpr char kOpReduceMin[] = "reduce_min";
constexpr char kOpReduceProduct[] = "reduce_prod";
constexpr char kOpReduceSum[] = "reduce_sum";
constexpr char kOpReduceSumSquare[] = "reduce_sum_square";
// Resample2d operators.
constexpr char kOpUpsampleBilinearTypeName[] = "upsample_bilinear";
constexpr char kOpUpsampleNearestNeighborTypeName[] =
    "upsample_nearest_neighbor";
// General op params that are shared across multiple ops.
constexpr char kOpParamAlpha[] = "alpha";
constexpr char kOpParamAxes[] = "axes";
constexpr char kOpParamAxis[] = "axis";
constexpr char kOpParamBeta[] = "beta";
constexpr char kOpParamBias[] = "bias";
constexpr char kOpParamData[] = "data";
constexpr char kOpParamDataTypeName[] = "dtype";
constexpr char kOpParamEpsilon[] = "epsilon";
constexpr char kOpParamGamma[] = "gamma";
constexpr char kOpParamIndices[] = "indices";
constexpr char kOpParamKeepDims[] = "keep_dims";
constexpr char kOpParamMode[] = "mode";
constexpr char kOpParamPad[] = "pad";
constexpr char kOpParamReps[] = "reps";
constexpr char kOpParamScatterModeValue[] = "update";
constexpr char kOpParamShape[] = "shape";
constexpr char kOpParamUpdates[] = "updates";
constexpr char kOpParamValidateIndices[] = "validate_indices";
constexpr char kOpParamWeight[] = "weight";
constexpr char kOpParamX[] = "x";
constexpr char kOpParamY[] = "y";
// Hard coded path used in the model file to point at the weight path.
constexpr char kWeightsRelativeFilePath[] = "@model_path/weights/weights.bin";

static constexpr auto kFloatDataTypes =
    base::MakeFixedFlatSet<CoreML::Specification::MILSpec::DataType>(
        {CoreML::Specification::MILSpec::DataType::FLOAT16,
         CoreML::Specification::MILSpec::DataType::FLOAT32});

static constexpr auto kFloatsAndInt32DataTypes =
    base::MakeFixedFlatSet<CoreML::Specification::MILSpec::DataType>(
        {CoreML::Specification::MILSpec::DataType::FLOAT16,
         CoreML::Specification::MILSpec::DataType::FLOAT32,
         CoreML::Specification::MILSpec::DataType::INT32});

// Maps to types defined in
// https://github.com/apple/coremltools/blob/b416f36054af9ca9d10b2d74ba215d0454677ca0/mlmodel/src/MILBlob/Blob/BlobDataType.hpp#L14
enum class BlobDataType : uint32_t {
  Float16 = 1,
  Float32 = 2,
  UInt8 = 3,
  Int8 = 4,
  BFloat16 = 5,
  Int16 = 6,
  UInt16 = 7,
};

// The weights format follows the definition in
// https://github.com/apple/coremltools/blob/b416f36054af9ca9d10b2d74ba215d0454677ca0/mlmodel/src/MILBlob/Blob/StorageFormat.hpp#L14-L78
// which defines the sentinel, alignment, header, and metadata structures.

// Default sentinel for validation for metadata.
constexpr uint64_t BlobMetadataSentinel = 0xDEADBEEF;

// All entries in the weight file need to be 64 bytes aligned, including the
// header, metadata and the weights.
constexpr uint64_t kWeightAlignment = 64;

struct WeightHeader {
  uint32_t count = 0;    // Number of constant values stored in the weight file.
  uint32_t version = 2;  // The default version that this format supports.

  uint64_t padding = 0;  // Paddings added to be 64 bytes aligned.
  uint64_t padding1 = 0;
  uint64_t padding2 = 0;
  uint64_t padding3 = 0;
  uint64_t padding4 = 0;
  uint64_t padding5 = 0;
  uint64_t padding6 = 0;
};

static_assert(sizeof(WeightHeader) == 64, "WeightHeader must be 64 bytes");

struct WeightMetadata {
  WeightMetadata(BlobDataType mil_data_type, uint64_t size_in_bytes,
                 uint64_t offset)
      : mil_data_type(mil_data_type),
        size_in_bytes(size_in_bytes),
        offset(offset) {}

  uint32_t sentinel = BlobMetadataSentinel;
  BlobDataType mil_data_type;
  uint64_t size_in_bytes;
  uint64_t offset;  // offset of the actual weight blob, after the metadata.

  uint64_t padding = 0;  // Paddings added to be 64 bytes aligned.
  uint64_t padding1 = 0;
  uint64_t padding2 = 0;
  uint64_t padding3 = 0;
  uint64_t padding4 = 0;
};

static_assert(sizeof(WeightMetadata) == 64, "WeightMetadata must be 64 bytes");

std::optional<BlobDataType> OperandTypeToDataTypeInWeightFile(
    OperandDataType data_type) {
  switch (data_type) {
    case OperandDataType::kFloat16:
      return BlobDataType::Float16;
    case OperandDataType::kFloat32:
      return BlobDataType::Float32;
    case OperandDataType::kUint8:
      return BlobDataType::UInt8;
    case OperandDataType::kInt8:
      return BlobDataType::Int8;
    case OperandDataType::kInt32:
    case OperandDataType::kUint32:
    case OperandDataType::kInt64:
    case OperandDataType::kUint64:
    case OperandDataType::kInt4:
    case OperandDataType::kUint4:
      return std::nullopt;
  }
}

CoreML::Specification::MILSpec::DataType OperandTypeToMILDataType(
    OperandDataType data_type) {
  switch (data_type) {
    case OperandDataType::kFloat32:
      return CoreML::Specification::MILSpec::DataType::FLOAT32;
    case OperandDataType::kFloat16:
      return CoreML::Specification::MILSpec::DataType::FLOAT16;
    case OperandDataType::kInt32:
      return CoreML::Specification::MILSpec::DataType::INT32;
    case OperandDataType::kUint32:
      return CoreML::Specification::MILSpec::DataType::UINT32;
    case OperandDataType::kInt64:
      return CoreML::Specification::MILSpec::DataType::INT64;
    case OperandDataType::kUint64:
      return CoreML::Specification::MILSpec::DataType::UINT64;
    case OperandDataType::kInt8:
      return CoreML::Specification::MILSpec::DataType::INT8;
    case OperandDataType::kUint8:
      return CoreML::Specification::MILSpec::DataType::UINT8;
    default:
      NOTREACHED() << "Unsupported data type.";
  }
}

// CoreML has more data types than WebNN. This should only be called with valid
// WebNN mapped types.
OperandDataType MILDataTypeToOperandType(
    CoreML::Specification::MILSpec::DataType mil_data_type) {
  switch (mil_data_type) {
    case CoreML::Specification::MILSpec::DataType::FLOAT32:
      return OperandDataType::kFloat32;
    case CoreML::Specification::MILSpec::DataType::FLOAT16:
      return OperandDataType::kFloat16;
    case CoreML::Specification::MILSpec::DataType::INT32:
      return OperandDataType::kInt32;
    case CoreML::Specification::MILSpec::DataType::UINT32:
      return OperandDataType::kUint32;
    case CoreML::Specification::MILSpec::DataType::INT64:
      return OperandDataType::kInt64;
    case CoreML::Specification::MILSpec::DataType::UINT64:
      return OperandDataType::kUint64;
    case CoreML::Specification::MILSpec::DataType::INT8:
      return OperandDataType::kInt8;
    case CoreML::Specification::MILSpec::DataType::UINT8:
      return OperandDataType::kUint8;
    default:
      NOTREACHED() << "Unsupported data type.";
  }
}

std::string_view MilDataTypeToString(
    CoreML::Specification::MILSpec::DataType mil_data_type) {
  // String values accepted by Core ML for the kOpParamDataTypeName parameter.
  // Expand as needed when adding new ops that support other types.
  switch (mil_data_type) {
    case CoreML::Specification::MILSpec::DataType::FLOAT32:
      return "fp32";
    case CoreML::Specification::MILSpec::DataType::FLOAT16:
      return "fp16";
    case CoreML::Specification::MILSpec::DataType::INT32:
      return "int32";
    case CoreML::Specification::MILSpec::DataType::INT8:
      return "int8";
    case CoreML::Specification::MILSpec::DataType::UINT8:
      return "uint8";
    case CoreML::Specification::MILSpec::DataType::BOOL:
      return "bool";
    default:
      NOTREACHED() << "Unsupported data type.";
  }
}

base::unexpected<mojom::ErrorPtr> NewNotSupportedError(std::string message) {
  return base::unexpected(mojom::Error::New(
      mojom::Error::Code::kNotSupportedError, std::move(message)));
}

base::unexpected<mojom::ErrorPtr> NewUnknownError(std::string message) {
  return base::unexpected(
      mojom::Error::New(mojom::Error::Code::kUnknownError, std::move(message)));
}

template <typename DataType>
  requires internal::IsSupportedTensorType<DataType>
struct MilDataTypeMap;

template <>
struct MilDataTypeMap<int32_t> {
  static constexpr CoreML::Specification::MILSpec::DataType value =
      CoreML::Specification::MILSpec::DataType::INT32;
};
template <>
struct MilDataTypeMap<int8_t> {
  static constexpr CoreML::Specification::MILSpec::DataType value =
      CoreML::Specification::MILSpec::DataType::INT8;
};
template <>
struct MilDataTypeMap<uint8_t> {
  static constexpr CoreML::Specification::MILSpec::DataType value =
      CoreML::Specification::MILSpec::DataType::UINT8;
};
template <>
struct MilDataTypeMap<Float16> {
  static constexpr CoreML::Specification::MILSpec::DataType value =
      CoreML::Specification::MILSpec::DataType::FLOAT16;
};
template <>
struct MilDataTypeMap<float> {
  static constexpr CoreML::Specification::MILSpec::DataType value =
      CoreML::Specification::MILSpec::DataType::FLOAT32;
};
template <>
struct MilDataTypeMap<char> {
  static constexpr CoreML::Specification::MILSpec::DataType value =
      CoreML::Specification::MILSpec::DataType::STRING;
};
template <>
struct MilDataTypeMap<bool> {
  static constexpr CoreML::Specification::MILSpec::DataType value =
      CoreML::Specification::MILSpec::DataType::BOOL;
};

template <typename DataType>
  requires internal::IsSupportedTensorType<DataType>
void SetTensorValueForImmediateValue(
    CoreML::Specification::MILSpec::TensorValue& tensor,
    base::span<const DataType> value);

// As per
// https://github.com/apple/coremltools/blob/bba83f43859e087d50c7d764cb132e7d4b427611/coremltools/converters/mil/backend/mil/helper.py#L23,
// float16, int8, uint8, uint32 are stored in bytes.
template <>
void SetTensorValueForImmediateValue<Float16>(
    CoreML::Specification::MILSpec::TensorValue& tensor,
    base::span<const Float16> value) {
  tensor.mutable_bytes()->mutable_values()->assign(
      base::as_string_view(base::as_bytes(value)));
}
template <>
void SetTensorValueForImmediateValue<int8_t>(
    CoreML::Specification::MILSpec::TensorValue& tensor,
    base::span<const int8_t> value) {
  tensor.mutable_bytes()->mutable_values()->assign(
      base::as_string_view(base::as_bytes(value)));
}
template <>
void SetTensorValueForImmediateValue<uint8_t>(
    CoreML::Specification::MILSpec::TensorValue& tensor,
    base::span<const uint8_t> value) {
  tensor.mutable_bytes()->mutable_values()->assign(base::as_string_view(value));
}

template <>
void SetTensorValueForImmediateValue<float>(
    CoreML::Specification::MILSpec::TensorValue& tensor,
    base::span<const float> value) {
  for (auto next : value) {
    tensor.mutable_floats()->add_values(next);
  }
}
template <>
void SetTensorValueForImmediateValue<int32_t>(
    CoreML::Specification::MILSpec::TensorValue& tensor,
    base::span<const int32_t> value) {
  for (auto next : value) {
    tensor.mutable_ints()->add_values(next);
  }
}
template <>
void SetTensorValueForImmediateValue<char>(
    CoreML::Specification::MILSpec::TensorValue& tensor,
    base::span<const char> value) {
  tensor.mutable_strings()->add_values(
      std::string(base::as_string_view(value)));
}
template <>
void SetTensorValueForImmediateValue<bool>(
    CoreML::Specification::MILSpec::TensorValue& tensor,
    base::span<const bool> value) {
  for (auto next : value) {
    tensor.mutable_bools()->add_values(next);
  }
}

void PopulateValueType(CoreML::Specification::MILSpec::DataType mil_data_type,
                       base::span<const uint32_t> dimensions,
                       CoreML::Specification::MILSpec::ValueType& value_type) {
  auto* tensor_type = value_type.mutable_tensortype();
  tensor_type->set_datatype(mil_data_type);
  // STRING type is considered scalar.
  if (mil_data_type == CoreML::Specification::MILSpec::DataType::STRING) {
    return;
  }

  // Scalar value doesn't need to set rank and dimensions.
  if (dimensions.empty()) {
    return;
  }

  tensor_type->set_rank(dimensions.size());
  for (auto dimension : dimensions) {
    tensor_type->add_dimensions()->mutable_constant()->set_size(dimension);
  }
}

void PopulateValueTypeFromOperandInfo(
    const GraphBuilderCoreml::OperandInfo& operand_info,
    CoreML::Specification::MILSpec::ValueType& value_type) {
  PopulateValueType(operand_info.mil_data_type, operand_info.dimensions,
                    value_type);
}

template <typename DataType>
  requires internal::IsSupportedTensorType<DataType>
CoreML::Specification::MILSpec::Value CreateTensorImmediateValue(
    base::span<const uint32_t> dimensions,
    base::span<const DataType> value) {
  CoreML::Specification::MILSpec::DataType mil_data_type =
      MilDataTypeMap<DataType>::value;

  CoreML::Specification::MILSpec::Value immediate_value{};
  PopulateValueType(mil_data_type, dimensions, *immediate_value.mutable_type());
  auto* tensor = immediate_value.mutable_immediatevalue()->mutable_tensor();
  SetTensorValueForImmediateValue(*tensor, value);
  return immediate_value;
}

template <typename DataType>
  requires internal::IsSupportedTensorType<DataType>
CoreML::Specification::MILSpec::Value Create1DTensorImmediateValue(
    base::span<const DataType> value) {
  return CreateTensorImmediateValue(
      base::span_from_ref(base::checked_cast<uint32_t>(value.size())), value);
}

// Special handling for string case, otherwise directly passing
// char[] to `Create1DTensorImmediateValue` will include the null character in
// the `Value` proto.
CoreML::Specification::MILSpec::Value CreateStringImmediateValue(
    std::string_view value) {
  return Create1DTensorImmediateValue<char>(value);
}

template <typename DataType>
  requires internal::IsSupportedTensorType<DataType>
CoreML::Specification::MILSpec::Value CreateScalarImmediateValue(
    const DataType& value) {
  return CreateTensorImmediateValue(/*dimensions=*/{}, base::span(&value, 1u));
}

// `Operation` input can bind to a `Value` or name, when binding to a name it
// refers to a previous operation's output.
void SetInputWithValue(
    google::protobuf::Map<std::string,
                          CoreML::Specification::MILSpec::Argument>& inputs,
    std::string_view key,
    CoreML::Specification::MILSpec::Value value) {
  *inputs[key].add_arguments()->mutable_value() = std::move(value);
}

void SetInputsWithValues(
    google::protobuf::Map<std::string,
                          CoreML::Specification::MILSpec::Argument>& inputs,
    std::initializer_list<
        std::pair<std::string_view, CoreML::Specification::MILSpec::Value>>
        params) {
  for (auto param : params) {
    SetInputWithValue(inputs, param.first, std::move(param.second));
  }
}

// CoreML requires names to match regular expression [A-Za-z\_][A-Za-z0-9\_@]*
// Note prefixes "input_", "output_" are added to names, so here only removing
// characters that don't match [A-Za-z0-9\_@]*
// https://github.com/apple/coremltools/blob/0e292a072452db19d1e64b687a372c0c54704a90/mlmodel/format/MIL.proto#L23
std::string SanitizeName(std::string_view name) {
  std::string sanitized_name(name);
  std::erase_if(sanitized_name, [](char c) {
    return !base::IsAsciiAlphaNumeric(c) && c != '_' && c != '@';
  });
  return sanitized_name;
}

CoreML::Specification::MILSpec::Value CreateFloatValue(
    CoreML::Specification::MILSpec::DataType mil_data_type,
    float value) {
  CHECK(kFloatDataTypes.contains(mil_data_type));
  return mil_data_type == CoreML::Specification::MILSpec::DataType::FLOAT32
             ? CreateScalarImmediateValue(value)
             : CreateScalarImmediateValue(
                   static_cast<Float16>(fp16_ieee_from_fp32_value(value)));
}

// Activation param name used in lstm.
std::string_view GetActivationParam(
    mojom::RecurrentNetworkActivation activation) {
  switch (activation) {
    case (mojom::RecurrentNetworkActivation::kRelu):
      return "relu";
    case (mojom::RecurrentNetworkActivation::kSigmoid):
      return "sigmoid";
    case (mojom::RecurrentNetworkActivation::kTanh):
      return "tanh";
  }
}

base::FixedArray<int32_t> Ui32ToI32(base::span<const uint32_t> data) {
  base::FixedArray<int32_t> output(data.size());
  base::ranges::transform(data, output.begin(), [](uint32_t val) {
    return base::checked_cast<int32_t>(val);
  });
  return output;
}

std::string_view GetActivationOpName(
    mojom::RecurrentNetworkActivation activation) {
  switch (activation) {
    case (mojom::RecurrentNetworkActivation::kRelu):
      return kOpReluTypeName;
    case (mojom::RecurrentNetworkActivation::kSigmoid):
      return kOpSigmoidTypeName;
    case (mojom::RecurrentNetworkActivation::kTanh):
      return kOpTanhTypeName;
  }
}

enum class GruGate {
  kReset,   // 'r'
  kUpdate,  // 'z'
  kNew      // 'n'
};

size_t GetGruGateIndex(GruGate gate, mojom::GruWeightLayout layout) {
  switch (layout) {
    case (mojom::GruWeightLayout::kRzn): {
      switch (gate) {
        case (GruGate::kReset):
          return 0;
        case (GruGate::kUpdate):
          return 1;
        case (GruGate::kNew):
          return 2;
      }
    }
    case (mojom::GruWeightLayout::kZrn): {
      switch (gate) {
        case (GruGate::kUpdate):
          return 0;
        case (GruGate::kReset):
          return 1;
        case (GruGate::kNew):
          return 2;
      }
    }
  }
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr> SetupMlPackageDirStructure(
    const base::FilePath& ml_package_dir) {
  if (!base::CreateDirectory(ml_package_dir)) {
    return NewUnknownError("Fail to create .mlpackage directory.");
  }
  base::FilePath data_dir = ml_package_dir.Append(kMlPackageDataDir);
  if (!base::CreateDirectory(data_dir)) {
    return NewUnknownError("Fail to create .mlpackage/Data directory.");
  }

  base::FilePath weights_dir = data_dir.Append(kMlPackageWeightsDir);
  if (!base::CreateDirectory(weights_dir)) {
    return NewUnknownError("Fail to create .mlpackage/Data/weights directory.");
  }

  // Creates a Manifest.json file that contains the package information. The
  // coremltools definition is here
  // https://github.com/apple/coremltools/blob/169d0ac7657c60e0d96e08612727ac51ab68c431/modelpackage/src/ModelPackage.hpp.
  base::Value::Dict metadata;
  base::Value::Dict item_info_entries;
  base::Value::Dict model_info;
  model_info.Set(kManifestItemAuthorKey, kManifestItemAuthorValue);
  model_info.Set(kManifestItemDescriptionKey, kManifestModelDescriptionValue);
  model_info.Set(kManifestItemNameKey, kManifestModelValue);
  model_info.Set(kManifestItemPathKey, kManifestModelValue);
  // Follows coremltools to use uuid for model identifier and weights
  // identifier.
  // https://github.com/apple/coremltools/blob/169d0ac7657c60e0d96e08612727ac51ab68c431/modelpackage/src/ModelPackage.cpp#L374
  std::string model_identifier(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  item_info_entries.Set(model_identifier, std::move(model_info));

  base::Value::Dict weights_info;
  weights_info.Set(kManifestItemAuthorKey, kManifestItemAuthorValue);
  weights_info.Set(kManifestItemDescriptionKey,
                   kManifestWeightsDescriptionValue);
  weights_info.Set(kManifestItemNameKey, kManifestModelValue);
  weights_info.Set(kManifestItemPathKey, kManifestWeightsValue);
  item_info_entries.Set(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                        std::move(weights_info));

  metadata.Set(kManifestItemInfoEntriesKey, std::move(item_info_entries));
  metadata.Set(kManifestVersionKey, kManifestVersionValue);
  metadata.Set(kManifestModelIdentifierKey, model_identifier);
  JSONFileValueSerializer serializer(ml_package_dir.Append(kManifestFileName));
  if (!serializer.Serialize(std::move(metadata))) {
    return NewUnknownError("Fail to create Manifest.json for mlpackage.");
  }

  return base::ok();
}

}  // namespace

GraphBuilderCoreml::ScopedWeightItem::ScopedWeightItem(
    WeightsFileHandle& weights_file_handle,
    size_t byte_size,
    uint64_t offset)
    : weights_file_handle_(weights_file_handle),
      byte_size_(byte_size),
      offset_(offset) {}
GraphBuilderCoreml::ScopedWeightItem::~ScopedWeightItem() {
  CHECK(finalized_ || has_error_);
}

base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::ScopedWeightItem::WriteBytes(
    base::span<const uint8_t> bytes) {
  CHECK(!finalized_ && !has_error_);
  size_written_ += bytes.size_bytes();
  auto result = weights_file_handle_->WriteBytes(bytes);
  if (!result.has_value()) {
    has_error_ = true;
  }
  return result;
}

base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::ScopedWeightItem::Finalize() {
  CHECK(!finalized_ && !has_error_);
  CHECK_EQ(size_written_, byte_size_);
  auto result = weights_file_handle_->WeightItemFinalize(byte_size_);
  if (!result.has_value()) {
    has_error_ = true;
  }

  finalized_ = true;
  return result;
}

// static
std::optional<std::unique_ptr<GraphBuilderCoreml::WeightsFileHandle>>
GraphBuilderCoreml::WeightsFileHandle::CreateWeightsHandle(
    const base::FilePath& weights_file_path) {
  base::File weights_file = base::File(
      weights_file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  if (!weights_file.IsValid()) {
    LOG(ERROR) << "[WebNN] Unable to open " << weights_file_path << ": "
               << base::File::ErrorToString(weights_file.error_details());
    return std::nullopt;
  }

  // The header will be overwritten by `Finalize()` with updated weight count.
  WeightHeader header{0};
  if (!weights_file.WriteAtCurrentPosAndCheck(
          base::byte_span_from_ref(header))) {
    return std::nullopt;
  }
  uint64_t current_offset = sizeof(header);
  return std::make_unique<WeightsFileHandle>(std::move(weights_file),
                                             current_offset);
}

GraphBuilderCoreml::WeightsFileHandle::WeightsFileHandle(
    base::File weights_file,
    uint64_t current_offset)
    : weights_file_(std::move(weights_file)), current_offset_(current_offset) {}
GraphBuilderCoreml::WeightsFileHandle::~WeightsFileHandle() = default;

base::expected<uint64_t, mojom::ErrorPtr>
GraphBuilderCoreml::WeightsFileHandle::Write(base::span<const uint8_t> bytes,
                                             OperandDataType data_type) {
  CHECK(!has_error_ && !finalized_);
  ASSIGN_OR_RETURN(
      std::unique_ptr<GraphBuilderCoreml::ScopedWeightItem> weight_item,
      CreateScopedWeightItem(data_type, bytes.size()));

  RETURN_IF_ERROR(weight_item->WriteBytes(bytes));

  RETURN_IF_ERROR(weight_item->Finalize());
  return weight_item->offset();
}

base::expected<std::unique_ptr<GraphBuilderCoreml::ScopedWeightItem>,
               mojom::ErrorPtr>
GraphBuilderCoreml::WeightsFileHandle::CreateScopedWeightItem(
    OperandDataType data_type,
    size_t byte_size) {
  CHECK(!has_error_ && !finalized_);
  std::optional<BlobDataType> weight_type =
      OperandTypeToDataTypeInWeightFile(data_type);
  if (!weight_type.has_value()) {
    has_error_ = true;
    return NewUnknownError(kWriteWeightsErrorMessage);
  }

  WeightMetadata metadata(weight_type.value(), byte_size,
                          current_offset_ + sizeof(WeightMetadata));

  base::ElapsedTimer timer;
  if (!weights_file_.WriteAtCurrentPosAndCheck(
          base::byte_span_from_ref(metadata))) {
    has_error_ = true;
    return NewUnknownError(kWriteWeightsErrorMessage);
  }
  weights_write_time_ += timer.Elapsed();
  return std::make_unique<GraphBuilderCoreml::ScopedWeightItem>(
      *this, byte_size, current_offset_);
}

base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::WeightsFileHandle::WriteBytes(
    base::span<const uint8_t> bytes) {
  CHECK(!has_error_ && !finalized_);
  base::ElapsedTimer timer;

  if (!weights_file_.WriteAtCurrentPosAndCheck(bytes)) {
    has_error_ = true;
    return NewUnknownError(kWriteWeightsErrorMessage);
  }
  weights_write_time_ += timer.Elapsed();
  return base::ok();
}

base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::WeightsFileHandle::WeightItemFinalize(size_t byte_size) {
  CHECK(!has_error_ && !finalized_);
  base::ElapsedTimer timer;
  current_offset_ += sizeof(WeightMetadata);
  current_offset_ += byte_size;
  current_offset_ = base::bits::AlignUp(current_offset_, kWeightAlignment);
  if (!weights_file_.Seek(base::File::Whence::FROM_BEGIN, current_offset_)) {
    has_error_ = true;
    return NewUnknownError(kWriteWeightsErrorMessage);
  }
  num_of_weights_++;
  weights_write_time_ += timer.Elapsed();
  return base::ok();
}

base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::WeightsFileHandle::Finalize() {
  CHECK(!has_error_ && !finalized_);
  WeightHeader header{num_of_weights_};
  base::ElapsedTimer timer;
  if (!weights_file_.WriteAndCheck(/*offset=*/0,
                                   base::byte_span_from_ref(header))) {
    has_error_ = true;
    return NewUnknownError(kWriteWeightsErrorMessage);
  }
  weights_write_time_ += timer.Elapsed();
  DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES("WebNN.CoreML.TimingMs.MLWeightsWrite",
                                        weights_write_time_);
  finalized_ = true;
  return base::ok();
}

size_t GraphBuilderCoreml::WeightsFileHandle::GetByteSize(
    OperandDataType data_type) {
  CHECK(!has_error_ && !finalized_);
  switch (data_type) {
    case OperandDataType::kFloat16:
      return 2;
    case OperandDataType::kFloat32:
      return 4;
    case OperandDataType::kUint8:
    case OperandDataType::kInt8:
      return 1;
    case OperandDataType::kInt32:
    case OperandDataType::kUint32:
    case OperandDataType::kInt64:
    case OperandDataType::kUint64:
    case OperandDataType::kInt4:
    case OperandDataType::kUint4:
      NOTREACHED() << "Unsupported weight type";
  }
}

std::string GetCoreMLNameFromInput(std::string_view input_name,
                                   uint64_t operand_id) {
  // Prefix is added to user provided names to avoid collision with intermediate
  // operands' names. `operand_id` is added to avoid collision with other
  // inputs' sanitized values.
  return base::JoinString({kInputNamePrefix, SanitizeName(input_name),
                           base::NumberToString(operand_id)},
                          kStringSeparator);
}

std::string GetCoreMLNameFromOutput(std::string_view output_name,
                                    uint64_t operand_id) {
  // Prefix is added to user provided names to avoid collision with intermediate
  // operands' names. `operand_id` is added to avoid collision with other
  // outputs' sanitized values.
  return base::JoinString({kOutputNamePrefix, SanitizeName(output_name),
                           base::NumberToString(operand_id)},
                          kStringSeparator);
}

// static
base::expected<std::unique_ptr<GraphBuilderCoreml::Result>, mojom::ErrorPtr>
GraphBuilderCoreml::CreateAndBuild(
    const mojom::GraphInfo& graph_info,
    ContextProperties context_properties,
    const base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>&
        constant_operands,
    const base::FilePath& working_directory) {
  // Use a random string for the model package directory, because MLModel
  // compileModelAtURL creates a folder directly in the NSTemporaryDirectory
  // with the name of the .mlmodel file. Using a random string will avoid any
  // potential name collision of that dir.
  base::FilePath ml_package_dir =
      working_directory.AppendASCII(base::UnguessableToken::Create().ToString())
          .AddExtension(kMlPackageExtension);

  RETURN_IF_ERROR(SetupMlPackageDirStructure(ml_package_dir));

  auto weights_handle = WeightsFileHandle::CreateWeightsHandle(
      ml_package_dir.Append(kMlPackageDataDir)
          .Append(kMlPackageWeightsDir)
          .Append(kMlPackageWeightsFileName));
  if (!weights_handle) {
    return NewUnknownError(kWriteWeightsErrorMessage);
  }

  GraphBuilderCoreml graph_builder(graph_info, std::move(context_properties),
                                   constant_operands, std::move(ml_package_dir),
                                   std::move(*weights_handle));

  RETURN_IF_ERROR(graph_builder.BuildCoreMLModel());
  RETURN_IF_ERROR(graph_builder.SerializeModel());
  return graph_builder.FinishAndTakeResult();
}

// static
ContextProperties GraphBuilderCoreml::GetContextProperties() {
  static constexpr SupportedDataTypes kFloatsAndInt32{OperandDataType::kFloat16,
                                                      OperandDataType::kFloat32,
                                                      OperandDataType::kInt32};

  static constexpr SupportedDataTypes kFloat16To32Int8To32AndUint8{
      OperandDataType::kFloat32, OperandDataType::kFloat16,
      OperandDataType::kInt32, OperandDataType::kInt8, OperandDataType::kUint8};

  static constexpr SupportedDataTypes kGatherIndicesSupportedDataTypes{
      OperandDataType::kInt32, OperandDataType::kInt8, OperandDataType::kUint8};

  static constexpr SupportedDataTypes kArgMinMaxOutputSupportedDataTypes{
      OperandDataType::kInt32};

  // TODO: crbug.com/345271830 - specify data types for all parameters.
  return ContextProperties(
      InputOperandLayout::kNchw, Resample2DAxes::kChannelsFirst,
      {/*input=*/kFloatsAndInt32,
       /*constant=*/kFloat16To32Int8To32AndUint8,
       /*arg_min_max_input=*/kFloatsAndInt32,
       /*arg_min_max_output=*/
       kArgMinMaxOutputSupportedDataTypes,
       /*batch_normalization_input=*/DataTypeConstraint::kFloat16To32,
       // Note that BOOL, INT16, and UINT16 is also supported by CoreML, but
       // WebNN does not have corresponding types.
       // https://apple.github.io/coremltools/source/coremltools.converters.mil.mil.ops.defs.html#coremltools.converters.mil.mil.ops.defs.iOS17.elementwise_unary.cast
       /*cast_input=*/kFloat16To32Int8To32AndUint8,
       // WebNN's "clamp" maps to the "clip" operator in CoreML:
       // https://apple.github.io/coremltools/source/coremltools.converters.mil.mil.ops.defs.html#coremltools.converters.mil.mil.ops.defs.iOS15.elementwise_unary.clip
       /*clamp_input=*/DataTypeConstraint::kFloat16To32,
       /*concat_inputs=*/kFloatsAndInt32,
       /*conv2d_input=*/DataTypeConstraint::kFloat16To32,
       /*conv_transpose2d_input=*/DataTypeConstraint::kFloat16To32,
       /*cumulative_sum_input=*/kFloatsAndInt32,
       // DequantizeLinear is not implemented.
       /*dequantize_linear_input=*/{},
       /*dequantize_linear_scale=*/{},
       /*add_input=*/kFloatsAndInt32,
       /*sub_input=*/kFloatsAndInt32,
       /*mul_input=*/kFloatsAndInt32,
       /*div_input=*/kFloatsAndInt32,
       /*max_input=*/kFloatsAndInt32,
       /*min_input=*/kFloatsAndInt32,
       /*pow_input=*/kFloatsAndInt32,
       /*equal_input=*/kFloatsAndInt32,
       /*greater_input=*/kFloatsAndInt32,
       /*greater_or_equal_input=*/kFloatsAndInt32,
       /*lesser_input=*/kFloatsAndInt32,
       /*lesser_or_equal_input=*/kFloatsAndInt32,
       /*logical_and_input=*/DataTypeConstraint::kUint8,
       /*logical_or_input=*/DataTypeConstraint::kUint8,
       /*logical_xor_input=*/DataTypeConstraint::kUint8,
       /*logical_not_input=*/DataTypeConstraint::kUint8,
       /*logical_output=*/DataTypeConstraint::kUint8,
       /*abs_input=*/kFloatsAndInt32,
       /*ceil_input=*/DataTypeConstraint::kFloat16To32,
       /*cos_input=*/DataTypeConstraint::kFloat16To32,
       /*erf_input=*/DataTypeConstraint::kFloat16To32,
       /*exp_input=*/DataTypeConstraint::kFloat16To32,
       /*floor_input=*/DataTypeConstraint::kFloat16To32,
       /*identity_input=*/kFloatsAndInt32,
       /*log_input=*/DataTypeConstraint::kFloat16To32,
       /*neg_input=*/kFloatsAndInt32,
       /*reciprocal_input=*/DataTypeConstraint::kFloat16To32,
       // Sign is not implemented.
       /*sign_input=*/{},
       /*sin_input=*/DataTypeConstraint::kFloat16To32,
       /*sqrt_input=*/DataTypeConstraint::kFloat16To32,
       /*tan_input=*/DataTypeConstraint::kFloat16To32,
       /*elu_input=*/DataTypeConstraint::kFloat16To32,
       /*expand_input=*/kFloatsAndInt32,
       // Note that INT16, and UINT16 is also supported by CoreML for all gather
       // operators, but WebNN does not have corresponding types. See docs here:
       // https://apple.github.io/coremltools/source/coremltools.converters.mil.mil.ops.defs.html#coremltools.converters.mil.mil.ops.defs.iOS17.scatter_gather.gather
       /*gather_input=*/kFloat16To32Int8To32AndUint8,
       /*gather_indices=*/kGatherIndicesSupportedDataTypes,
       // Note that INT16, and UINT16 is also supported by CoreML, but WebNN
       // does not have corresponding types. See docs here:
       // https://apple.github.io/coremltools/source/coremltools.converters.mil.mil.ops.defs.html#coremltools.converters.mil.mil.ops.defs.iOS17.scatter_gather.gather_along_axis
       /*gather_elements_input=*/kFloat16To32Int8To32AndUint8,
       /*gather_elements_indices=*/kGatherIndicesSupportedDataTypes,
       /*gather_nd_input=*/kFloat16To32Int8To32AndUint8,
       /*gather_nd_indices=*/kGatherIndicesSupportedDataTypes,
       /*gelu_input=*/DataTypeConstraint::kFloat16To32,
       /*gemm_input=*/DataTypeConstraint::kFloat16To32,
       /*gru_input=*/DataTypeConstraint::kFloat16To32,
       /*gru_cell_input=*/DataTypeConstraint::kFloat16To32,
       /*hard_sigmoid_input=*/DataTypeConstraint::kFloat16To32,
       /*hard_swish_input=*/DataTypeConstraint::kFloat16To32,
       /*instance_normalization_input=*/DataTypeConstraint::kFloat16To32,
       /*layer_normalization_input=*/DataTypeConstraint::kFloat16To32,
       /*leaky_relu_input=*/DataTypeConstraint::kFloat16To32,
       // TODO: crbug.com/338667172 - Consider enhancing the data type support
       // to include int32.
       /*linear_input=*/DataTypeConstraint::kFloat16To32,
       /*lstm_input=*/DataTypeConstraint::kFloat16To32,
       // LstmCell is implemented with lstm, they should have the same
       // constraints.
       /*lstm_cell_input=*/DataTypeConstraint::kFloat16To32,
       /*matmul_input=*/kFloatsAndInt32,
       /*pad_input=*/DataTypeConstraint::kFloat16To32,
       /*average_pool2d_input=*/DataTypeConstraint::kFloat16To32,
       /*l2_pool2d_input=*/DataTypeConstraint::kFloat16To32,
       /*max_pool2d_input=*/DataTypeConstraint::kFloat16To32,
       // Prelu is not implemented.
       /*prelu_input=*/{},
       // QuantizeLinear is not implemented.
       /*quantize_linear_input=*/{},
       /*quantize_linear_zero_point=*/{},
       /*reduce_l1_input=*/kFloatsAndInt32,
       /*reduce_l2_input=*/kFloatsAndInt32,
       /*reduce_log_sum_input=*/kFloatsAndInt32,
       /*reduce_log_sum_exp_input=*/kFloatsAndInt32,
       /*reduce_max_input=*/kFloatsAndInt32,
       /*reduce_mean_input=*/kFloatsAndInt32,
       /*reduce_min_input=*/kFloatsAndInt32,
       /*reduce_product_input=*/kFloatsAndInt32,
       /*reduce_sum_input=*/kFloatsAndInt32,
       /*reduce_sum_square_input=*/kFloatsAndInt32,
       /*relu_input=*/DataTypeConstraint::kFloat16To32,
       /*resample2d_input=*/DataTypeConstraint::kFloat16To32,
       // Note that BOOL is also supported by CoreML, but WebNN does not have a
       // corresponding BOOL type. See docs here:
       // https://apple.github.io/coremltools/source/coremltools.converters.mil.mil.ops.defs.html#coremltools.converters.mil.mil.ops.defs.iOS15.tensor_transformation.reshape
       /*reshape_input=*/kFloatsAndInt32,
       // TODO(crbug.com/377349382): Implement Reverse.
       /*reverse_input=*/{},
       /*scatter_elements_input=*/kFloatsAndInt32,
       /*scatter_elements_indices=*/{OperandDataType::kInt32},
       /*scatter_nd_input=*/kFloatsAndInt32,
       /*scatter_nd_indices=*/{OperandDataType::kInt32},
       /*sigmoid_input=*/DataTypeConstraint::kFloat16To32,
       // Note that BOOL, INT16, and UINT16 is also supported by CoreML, but
       // WebNN does not have corresponding types. See docs here:
       // https://apple.github.io/coremltools/source/coremltools.converters.mil.mil.ops.defs.html#coremltools.converters.mil.mil.ops.defs.iOS17.tensor_transformation.slice_by_size
       /*slice_input=*/kFloat16To32Int8To32AndUint8,
       /*softmax_input=*/DataTypeConstraint::kFloat16To32,
       /*softplus_input=*/DataTypeConstraint::kFloat16To32,
       /*softsign_input=*/DataTypeConstraint::kFloat16To32,
       // Note that BOOL is also supported by CoreML, but WebNN does not have a
       // corresponding BOOL type. See docs here:
       // https://apple.github.io/coremltools/source/coremltools.converters.mil.mil.ops.defs.html#coremltools.converters.mil.mil.ops.defs.iOS15.tensor_operation.split
       /*split_input=*/kFloatsAndInt32,
       /*tanh_input=*/DataTypeConstraint::kFloat16To32,
       // Note that BOOL is also supported by CoreML, but WebNN does not have a
       // corresponding BOOL type. See docs here:
       // https://apple.github.io/coremltools/source/coremltools.converters.mil.mil.ops.defs.html#coremltools.converters.mil.mil.ops.defs.iOS15.tensor_operation.tile
       /*tile_input=*/kFloatsAndInt32,
       // Note that BOOL is also supported by CoreML, but WebNN does not have a
       // corresponding BOOL type. See docs here:
       // https://apple.github.io/coremltools/source/coremltools.converters.mil.mil.ops.defs.html#coremltools.converters.mil.mil.ops.defs.iOS15.tensor_operation.transpose
       /*transpose_input=*/kFloatsAndInt32,
       // Triangular is not implemented.
       // Note that BOOL is also supported by CoreML, but WebNN does not have a
       // corresponding BOOL type. See docs here:
       // https://apple.github.io/coremltools/source/coremltools.converters.mil.mil.ops.defs.html#coremltools.converters.mil.mil.ops.defs.iOS15.tensor_operation.band_part
       /*triangular_input=*/kFloatsAndInt32,
       /*where_condition=*/DataTypeConstraint::kUint8,
       // Note that BOOL is also supported by CoreML, but WebNN does not have a
       // corresponding BOOL type. See docs here:
       // https://apple.github.io/coremltools/source/coremltools.converters.mil.mil.ops.defs.html#coremltools.converters.mil.mil.ops.defs.iOS15.tensor_operation.transpose
       /*where_value=*/kFloatsAndInt32});
}

GraphBuilderCoreml::GraphBuilderCoreml(
    const mojom::GraphInfo& graph_info,
    ContextProperties context_properties,
    const base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>&
        constant_operands,
    base::FilePath ml_package_dir,
    std::unique_ptr<WeightsFileHandle> weights_file_handle)
    : graph_info_(graph_info),
      constant_operands_(constant_operands),
      context_properties_(std::move(context_properties)),
      internal_operand_id_(
          base::ranges::max_element(
              graph_info_->id_to_operand_map,
              {},
              [](const auto& id_operand) { return id_operand.first; })
              ->first),
      weights_file_handle_(std::move(weights_file_handle)),
      result_(std::make_unique<Result>(std::move(ml_package_dir))) {}

GraphBuilderCoreml::~GraphBuilderCoreml() = default;

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::BuildCoreMLModel() {
  CHECK_EQ(ml_model_.specificationversion(), 0);
  // Based on comment in Model.proto
  //  * 8 : iOS 17, macOS 14, tvOS 17, watchOS 10 (Core ML 7)
  //  * - iOS 17 ops
  //  * - Scene print v2
  //  * - ClassConfidenceThresholding model
  // use the model specification version supported on macOS 14 which is
  // version 8. We need to use version 8 because Cast in version 7 does
  // not support casting to uint8, which is required for logical binary
  // operators. Logical binary operators return bool tensors in CoreML
  // they need to be cast to uint8 to match WebNN.
  ml_model_.set_specificationversion(8);
  ml_model_.set_isupdatable(false);

  program_ = ml_model_.mutable_mlprogram();
  program_->set_version(1);

  // Creates a Program with a single main function, and a single block within
  // the function. The block contains all the ops right now.
  auto& main_function = (*program_->mutable_functions())["main"];
  // CoreML7 means specification version 8.
  main_function.set_opset("CoreML7");
  auto& block = (*main_function.mutable_block_specializations())["CoreML7"];

  for (const auto& [operand_id, _] : graph_info_->id_to_operand_map) {
    UpdateCoreMLInputInfoMap(operand_id);
  }

  // Add inputs.
  for (uint64_t input_id : graph_info_->input_operands) {
    RETURN_IF_ERROR(AddInput(input_id, main_function, block));
  }

  if (graph_info_->input_operands.empty()) {
    AddPlaceholderInput(main_function, block);
  }

  RETURN_IF_ERROR(WriteImmediateWeights(block));

  // Add operations.
  for (const mojom::OperationPtr& operation : graph_info_->operations) {
    std::string operand_op_name = GetOpName(*operation);
    switch (operation->which()) {
      case mojom::Operation::Tag::kArgMinMax: {
        RETURN_IF_ERROR(
            AddOperationForArgMinMax(*operation->get_arg_min_max(), block));
        break;
      }
      case mojom::Operation::Tag::kBatchNormalization: {
        RETURN_IF_ERROR(AddOperationForBatchNormalization(
            *operation->get_batch_normalization(), block));
        break;
      }
      case mojom::Operation::Tag::kClamp: {
        RETURN_IF_ERROR(AddOperationForClamp(*operation->get_clamp(), block));
        break;
      }
      case mojom::Operation::Tag::kConcat: {
        RETURN_IF_ERROR(AddOperationForConcat(*operation->get_concat(), block));
        break;
      }
      case mojom::Operation::Tag::kConv2d: {
        RETURN_IF_ERROR(AddOperationForConv2d(*operation->get_conv2d(), block));
        break;
      }
      case mojom::Operation::Tag::kCumulativeSum: {
        RETURN_IF_ERROR(AddOperationForCumulativeSum(
            *operation->get_cumulative_sum(), block));
        break;
      }
      case mojom::Operation::Tag::kElementWiseBinary: {
        const mojom::ElementWiseBinaryPtr& op =
            operation->get_element_wise_binary();
        RETURN_IF_ERROR(AddOperationForElementwiseBinary(
            op->lhs_operand_id, op->rhs_operand_id, op->output_operand_id,
            op->kind, block));
        break;
      }
      case mojom::Operation::Tag::kElementWiseUnary: {
        const mojom::ElementWiseUnaryPtr& op =
            operation->get_element_wise_unary();
        RETURN_IF_ERROR(AddOperationForElementwiseUnary(
            op->kind, op->input_operand_id, op->output_operand_id, block));
        break;
      }
      case mojom::Operation::Tag::kElu: {
        RETURN_IF_ERROR(AddOperationForElu(*operation->get_elu(), block));
        break;
      }
      case mojom::Operation::Tag::kExpand: {
        RETURN_IF_ERROR(AddOperationForExpand(*operation->get_expand(), block));
        break;
      }
      case mojom::Operation::Tag::kGather: {
        RETURN_IF_ERROR(AddOperationForGather(*operation->get_gather(), block));
        break;
      }
      case mojom::Operation::Tag::kGatherElements: {
        RETURN_IF_ERROR(AddOperationForGatherElements(
            *operation->get_gather_elements(), block));
        break;
      }
      case mojom::Operation::Tag::kGatherNd: {
        RETURN_IF_ERROR(
            AddOperationForGatherND(*operation->get_gather_nd(), block));
        break;
      }
      case mojom::Operation::Tag::kGelu: {
        RETURN_IF_ERROR(AddOperationForGelu(*operation->get_gelu(), block));
        break;
      }
      case mojom::Operation::Tag::kGemm: {
        RETURN_IF_ERROR(AddOperationForGemm(*operation->get_gemm(), block));
        break;
      }
      case mojom::Operation::Tag::kGru: {
        RETURN_IF_ERROR(AddOperationForGru(*operation->get_gru(), block));
        break;
      }
      case mojom::Operation::Tag::kGruCell: {
        RETURN_IF_ERROR(
            AddOperationForGruCell(*operation->get_gru_cell(), block));
        break;
      }
      case mojom::Operation::Tag::kHardSigmoid: {
        RETURN_IF_ERROR(
            AddOperationForHardSigmoid(*operation->get_hard_sigmoid(), block));
        break;
      }
      case mojom::Operation::Tag::kHardSwish: {
        RETURN_IF_ERROR(
            AddOperationForHardSwish(*operation->get_hard_swish(), block));
        break;
      }
      case mojom::Operation::Tag::kInstanceNormalization: {
        RETURN_IF_ERROR(AddOperationForInstanceNormalization(
            *operation->get_instance_normalization(), block));
        break;
      }
      case mojom::Operation::Tag::kLayerNormalization: {
        RETURN_IF_ERROR(AddOperationForLayerNormalization(
            *operation->get_layer_normalization(), block));
        break;
      }
      case mojom::Operation::Tag::kLeakyRelu: {
        RETURN_IF_ERROR(
            AddOperationForLeakyRelu(*operation->get_leaky_relu(), block));
        break;
      }
      case mojom::Operation::Tag::kLinear: {
        RETURN_IF_ERROR(AddOperationForLinear(*operation->get_linear(), block));
        break;
      }
      case mojom::Operation::Tag::kLstm: {
        RETURN_IF_ERROR(AddOperationForLstm(*operation->get_lstm(), block));
        break;
      }
      case mojom::Operation::Tag::kLstmCell: {
        RETURN_IF_ERROR(
            AddOperationForLstmCell(*operation->get_lstm_cell(), block));
        break;
      }
      case mojom::Operation::Tag::kMatmul: {
        RETURN_IF_ERROR(AddOperationForMatmul(*operation->get_matmul(), block));
        break;
      }
      case mojom::Operation::Tag::kPad: {
        RETURN_IF_ERROR(AddOperationForPad(*operation->get_pad(), block));
        break;
      }
      case mojom::Operation::Tag::kPool2d: {
        RETURN_IF_ERROR(AddOperationForPool2d(*operation->get_pool2d(), block));
        break;
      }
      case mojom::Operation::Tag::kReduce: {
        RETURN_IF_ERROR(AddOperationForReduce(*operation->get_reduce(), block));
        break;
      }
      case mojom::Operation::Tag::kRelu: {
        CHECK(context_properties_.data_type_limits.relu_input.Has(
            MILDataTypeToOperandType(
                GetOperandInfo(operation->get_relu()->input_operand_id)
                    .mil_data_type)));
        RETURN_IF_ERROR(
            AddUnaryOperation(SupportedDataType::kFloats, kOpReluTypeName,
                              *operation->get_relu(), block, operand_op_name));
        break;
      }
      case mojom::Operation::Tag::kResample2d: {
        RETURN_IF_ERROR(
            AddOperationForResample2d(*operation->get_resample2d(), block));
        break;
      }
      case mojom::Operation::Tag::kReshape: {
        RETURN_IF_ERROR(
            AddOperationForReshape(*operation->get_reshape(), block));
        break;
      }
      case mojom::Operation::Tag::kScatterElements: {
        RETURN_IF_ERROR(AddOperationForScatterElements(
            *operation->get_scatter_elements(), block));
        break;
      }
      case mojom::Operation::Tag::kScatterNd: {
        RETURN_IF_ERROR(
            AddOperationForScatterND(*operation->get_scatter_nd(), block));
        break;
      }
      case mojom::Operation::Tag::kSigmoid: {
        CHECK(context_properties_.data_type_limits.sigmoid_input.Has(
            MILDataTypeToOperandType(
                GetOperandInfo(operation->get_sigmoid()->input_operand_id)
                    .mil_data_type)));
        RETURN_IF_ERROR(AddUnaryOperation(kOpSigmoidTypeName,
                                          *operation->get_sigmoid(), block));
        break;
      }
      case mojom::Operation::Tag::kSlice: {
        RETURN_IF_ERROR(AddOperationForSlice(*operation->get_slice(), block));
        break;
      }
      case mojom::Operation::Tag::kSoftmax: {
        RETURN_IF_ERROR(
            AddOperationForSoftmax(*operation->get_softmax(), block));
        break;
      }
      case mojom::Operation::Tag::kSoftplus: {
        CHECK(context_properties_.data_type_limits.softplus_input.Has(
            MILDataTypeToOperandType(
                GetOperandInfo(operation->get_softplus()->input_operand_id)
                    .mil_data_type)));
        RETURN_IF_ERROR(AddUnaryOperation(kOpSoftplusTypeName,
                                          *operation->get_softplus(), block));
        break;
      }
      case mojom::Operation::Tag::kSoftsign: {
        CHECK(context_properties_.data_type_limits.softsign_input.Has(
            MILDataTypeToOperandType(
                GetOperandInfo(operation->get_softsign()->input_operand_id)
                    .mil_data_type)));
        RETURN_IF_ERROR(AddUnaryOperation(kOpSoftsignTypeName,
                                          *operation->get_softsign(), block));
        break;
      }
      case mojom::Operation::Tag::kSplit: {
        RETURN_IF_ERROR(AddOperationForSplit(*operation->get_split(), block));
        break;
      }
      case mojom::Operation::Tag::kTanh: {
        CHECK(context_properties_.data_type_limits.tanh_input.Has(
            MILDataTypeToOperandType(
                GetOperandInfo(operation->get_tanh()->input_operand_id)
                    .mil_data_type)));
        RETURN_IF_ERROR(
            AddUnaryOperation(kOpTanhTypeName, *operation->get_tanh(), block));
        break;
      }
      case mojom::Operation::Tag::kTile: {
        RETURN_IF_ERROR(AddOperationForTile(*operation->get_tile(), block));
        break;
      }
      case mojom::Operation::Tag::kTranspose: {
        RETURN_IF_ERROR(
            AddOperationForTranspose(*operation->get_transpose(), block));
        break;
      }
      case mojom::Operation::Tag::kTriangular: {
        RETURN_IF_ERROR(
            AddOperationForTriangular(*operation->get_triangular(), block));
        break;
      }
      case mojom::Operation::Tag::kWhere: {
        RETURN_IF_ERROR(AddOperationForWhere(*operation->get_where(), block));
        break;
      }
      case mojom::Operation::Tag::kDequantizeLinear:
      case mojom::Operation::Tag::kPrelu:
      case mojom::Operation::Tag::kQuantizeLinear:
      case mojom::Operation::Tag::kReverse:
        return NewNotSupportedError(NotSupportedOperatorError(*operation));
    }
  }

  // Add output.
  for (uint64_t output_id : graph_info_->output_operands) {
    block.add_outputs(GetOperandInfo(output_id).coreml_name);
    RETURN_IF_ERROR(AddOutput(output_id));
  }
  RETURN_IF_ERROR(weights_file_handle_->Finalize());
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilderCoreml::SerializeModel() {
  base::ElapsedTimer ml_model_write_timer;
  base::FilePath model_file_path = ml_package_dir()
                                       .Append(kMlPackageDataDir)
                                       .Append(kMlPackageModelFileName);
  base::File model_file(model_file_path,
                        base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  if (!model_file.IsValid()) {
    LOG(ERROR) << "[WebNN] Unable to open " << model_file_path << ": "
               << base::File::ErrorToString(model_file.error_details());
    return NewUnknownError(kWriteModelErrorMessage);
  }
  bool result =
      ml_model_.SerializeToFileDescriptor(model_file.GetPlatformFile());
  DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES("WebNN.CoreML.TimingMs.MLModelWrite",
                                        ml_model_write_timer.Elapsed());
  if (!result) {
    return NewUnknownError(kWriteModelErrorMessage);
  }
  return base::ok();
}

std::unique_ptr<GraphBuilderCoreml::Result>
GraphBuilderCoreml::FinishAndTakeResult() {
  return std::move(result_);
}

base::expected<void, mojom::ErrorPtr> GraphBuilderCoreml::WriteImmediateWeights(
    CoreML::Specification::MILSpec::Block& block) {
  // Only adds immediate values, weights saved in weight file are added lazily
  // because operation might need to manipulate the weights before they are
  // serialized. We don't support manipulating immediate values right now.
  for (auto& [id, constant_operand] : *constant_operands_) {
    // int32 is only supported as immediate value.
    if (constant_operand->descriptor().shape().empty() ||
        constant_operand->descriptor().data_type() == OperandDataType::kInt32) {
      AddConstantImmediateValue(id, block);
    }
  }
  return base::ok();
}

void GraphBuilderCoreml::AddPlaceholderInput(
    CoreML::Specification::MILSpec::Function& main_function,
    CoreML::Specification::MILSpec::Block& block) {
  auto* mutable_description = ml_model_.mutable_description();
  auto* feature_description = mutable_description->add_input();

  auto* feature_type = feature_description->mutable_type();
  auto* array_feature_type = feature_type->mutable_multiarraytype();
  array_feature_type->set_datatype(
      CoreML::Specification::ArrayFeatureType_ArrayDataType::
          ArrayFeatureType_ArrayDataType_FLOAT16);

  array_feature_type->add_shape(1);
  feature_description->mutable_name()->assign(kPlaceholderInputName);

  const OperandInfo operand_info{
      kPlaceholderInputName, base::span<const uint32_t>({1}),
      CoreML::Specification::MILSpec::DataType::FLOAT16};

  CoreML::Specification::MILSpec::NamedValueType& input_for_main_function =
      *main_function.add_inputs();
  input_for_main_function.set_name(kPlaceholderInputName);
  auto& value_type = *input_for_main_function.mutable_type();
  PopulateValueTypeFromOperandInfo(operand_info, value_type);

  // The model compute only succeeds when the placeholder is used in one op.
  CoreML::Specification::MILSpec::Operation* placeholder_op =
      block.add_operations();
  (*placeholder_op->mutable_inputs())[kOpParamX].add_arguments()->set_name(
      std::string(kPlaceholderInputName));
  (*placeholder_op->mutable_inputs())[kOpParamY].add_arguments()->set_name(
      std::string(kPlaceholderInputName));

  placeholder_op->set_type(kOpAddTypeName);
  CoreML::Specification::MILSpec::NamedValueType& outputs =
      *placeholder_op->add_outputs();
  outputs.set_name(kPlaceholderOuputName);
  auto& output_value_type = *outputs.mutable_type();
  PopulateValueTypeFromOperandInfo(operand_info, output_value_type);
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddInput(
    uint64_t input_id,
    CoreML::Specification::MILSpec::Function& main_function,
    CoreML::Specification::MILSpec::Block& block) {
  auto* mutable_description = ml_model_.mutable_description();
  auto* feature_description = mutable_description->add_input();
  const mojom::Operand& operand = GetOperand(input_id);
  RETURN_IF_ERROR(PopulateFeatureDescription(input_id, *feature_description));

  CoreML::Specification::MILSpec::NamedValueType& input =
      *main_function.add_inputs();
  PopulateNamedValueTypeForInput(input_id, input);

  if (operand.descriptor.shape().empty()) {
    ASSIGN_OR_RETURN(
        uint64_t internal_operand_id,
        GenerateInternalOperandInfo(
            OperandTypeToMILDataType(operand.descriptor.data_type()), {}));
    RETURN_IF_ERROR(
        AddOperationForReshape(input_id, internal_operand_id, block));
    // Points the input_id to the reshaped node's coreml identifier, so that
    // subsequent operations find the correct inputs.
    id_to_operand_info_map()[input_id].coreml_name =
        GetOperandInfo(internal_operand_id).coreml_name;
  }
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOutput(uint64_t output_id) {
  CHECK(id_to_operand_info_map().contains(output_id));
  auto* mutable_description = ml_model_.mutable_description();
  auto* feature_description = mutable_description->add_output();
  RETURN_IF_ERROR(PopulateFeatureDescription(output_id, *feature_description));
  return base::ok();
}

base::expected<CoreML::Specification::MILSpec::Operation*, mojom::ErrorPtr>
GraphBuilderCoreml::CreateUnaryOperation(
    SupportedDataType supported_data_type,
    std::string_view op_name,
    uint64_t input_operand_id,
    uint64_t output_operand_id,
    CoreML::Specification::MILSpec::Block& block,
    std::string_view operand_op_name) {
  const OperandInfo& input_operand_info = GetOperandInfo(input_operand_id);
  switch (supported_data_type) {
    case SupportedDataType::kFloats: {
      if (!kFloatDataTypes.contains(input_operand_info.mil_data_type)) {
        return NewNotSupportedError(NotSupportedInputArgumentTypeError(
            operand_op_name,
            MILDataTypeToOperandType(input_operand_info.mil_data_type)));
      }
      break;
    }
    case SupportedDataType::kFloatsAndInt32: {
      if (!kFloatsAndInt32DataTypes.contains(
              input_operand_info.mil_data_type)) {
        return NewNotSupportedError(NotSupportedInputArgumentTypeError(
            operand_op_name,
            MILDataTypeToOperandType(input_operand_info.mil_data_type)));
      }
      break;
    }
  }

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(std::string(op_name));

  RETURN_IF_ERROR(
      SetInputFromOperand(*op->mutable_inputs(), kOpParamX, input_operand_id));

  PopulateNamedValueType(output_operand_id, *op->add_outputs());
  return op;
}

base::expected<void, mojom::ErrorPtr> GraphBuilderCoreml::AddUnaryOperation(
    SupportedDataType supported_data_type,
    std::string_view op_name,
    uint64_t input_operand_id,
    uint64_t output_operand_id,
    CoreML::Specification::MILSpec::Block& block,
    std::string_view operand_op_name) {
  RETURN_IF_ERROR(CreateUnaryOperation(supported_data_type, op_name,
                                       input_operand_id, output_operand_id,
                                       block, operand_op_name));
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddUnaryOperation(
    std::string_view op_name,
    uint64_t input_operand_id,
    uint64_t output_operand_id,
    CoreML::Specification::MILSpec::Block& block) {
  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(std::string(op_name));

  RETURN_IF_ERROR(
      SetInputFromOperand(*op->mutable_inputs(), kOpParamX, input_operand_id));

  PopulateNamedValueType(output_operand_id, *op->add_outputs());
  return base::ok();
}

template <typename T>
base::expected<void, mojom::ErrorPtr> GraphBuilderCoreml::AddUnaryOperation(
    SupportedDataType supported_data_type,
    std::string_view op_name,
    const T& operation,
    CoreML::Specification::MILSpec::Block& block,
    std::string_view operand_op_name) {
  return AddUnaryOperation(supported_data_type, op_name,
                           operation.input_operand_id,
                           operation.output_operand_id, block, operand_op_name);
}

template <typename T>
[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddUnaryOperation(
    std::string_view op_name,
    const T& operation,
    CoreML::Specification::MILSpec::Block& block) {
  return AddUnaryOperation(op_name, operation.input_operand_id,
                           operation.output_operand_id, block);
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddUnaryFloatsOperationWithEpsilon(
    std::string_view op_name,
    uint64_t input_operand_id,
    uint64_t output_operand_id,
    float epsilon,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info = GetOperandInfo(input_operand_id);
  CHECK(kFloatDataTypes.contains(input_operand_info.mil_data_type));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(std::string(op_name));

  RETURN_IF_ERROR(
      SetInputFromOperand(*op->mutable_inputs(), kOpParamX, input_operand_id));

  SetInputWithValue(
      *op->mutable_inputs(), kOpParamEpsilon,
      CreateFloatValue(input_operand_info.mil_data_type, epsilon));

  PopulateNamedValueType(output_operand_id, *op->add_outputs());
  return base::ok();
}

template <typename T>
[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddUnaryFloatsOperationWithEpsilon(
    std::string_view op_name,
    const T& operation,
    float epsilon,
    CoreML::Specification::MILSpec::Block& block) {
  return AddUnaryFloatsOperationWithEpsilon(op_name, operation.input_operand_id,
                                            operation.output_operand_id,
                                            epsilon, block);
}

base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForArgMinMax(
    const mojom::ArgMinMax& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  CHECK(context_properties_.data_type_limits.arg_min_max_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  const OperandInfo& output_operand_info =
      GetOperandInfo(operation.output_operand_id);
  CHECK(context_properties_.data_type_limits.arg_min_max_output.Has(
      MILDataTypeToOperandType(output_operand_info.mil_data_type)));

  uint64_t input_operand_id = operation.input_operand_id;
  // CoreML doesn't support scalar input, in this case reshape to 1D then
  // reshape back.
  if (input_operand_info.dimensions.empty()) {
    ASSIGN_OR_RETURN(input_operand_id, GenerateInternalOperandInfo(
                                           input_operand_info.mil_data_type,
                                           base::span<const uint32_t>({1})));
    RETURN_IF_ERROR(AddOperationForReshape(operation.input_operand_id,
                                           input_operand_id, block));
  }
  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  switch (operation.kind) {
    case mojom::ArgMinMax_Kind::kMin:
      op->set_type(kOpArgminTypeName);
      break;
    case mojom::ArgMinMax_Kind::kMax:
      op->set_type(kOpArgmaxTypeName);
      break;
  }
  RETURN_IF_ERROR(
      SetInputFromOperand(*op->mutable_inputs(), kOpParamX, input_operand_id));

  SetInputsWithValues(
      *op->mutable_inputs(),
      {
          {kOpParamAxis, CreateScalarImmediateValue(
                             base::checked_cast<int32_t>(operation.axis))},
          {kOpParamKeepDims,
           CreateScalarImmediateValue(operation.keep_dimensions)},
      });

  // No need to add a reshape when keep_dimensions=false as the output is
  // already scalar.
  if (input_operand_info.dimensions.empty() && operation.keep_dimensions) {
    ASSIGN_OR_RETURN(
        int64_t intermediate_output_operand_id,
        GenerateInternalOperandInfo(output_operand_info.mil_data_type,
                                    base::span<const uint32_t>({1})));
    PopulateNamedValueType(intermediate_output_operand_id, *op->add_outputs());
    RETURN_IF_ERROR(AddOperationForReshape(intermediate_output_operand_id,
                                           operation.output_operand_id, block));
  } else {
    PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  }
  return base::ok();
}

base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForBatchNormalization(
    const mojom::BatchNormalization& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  CHECK(context_properties_.data_type_limits.batch_normalization_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  // TODO(crbug.com/338529225): Support ND inputs.
  if (input_operand_info.dimensions.size() < 3 ||
      input_operand_info.dimensions.size() > 5) {
    return NewNotSupportedError(
        "Unsupported rank for batchNormalization. It must be between 3 and 5.");
  }

  // TODO(crbug.com/338398666): Consider supporting more values for
  // `operation.axis` by transposing the input. CoreML only supports
  // batchNormalization over the "channel" dimension, though we don't actually
  // have any way to know the layout here, so we'll just guess it's:
  //  - NCH for a 3D input,
  //  - NCHW for a 4D input, or
  //  - NCDHW for a 5D input
  // https://apple.github.io/coremltools/source/coremltools.converters.mil.mil.ops.defs.html#coremltools.converters.mil.mil.ops.defs.iOS17.normalization.batch_norm
  if (operation.axis != 1) {
    return NewNotSupportedError(
        "Unsupported axis for batchNormalization. It must be the channel "
        "dimension.");
  }

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpBatchNormalizationTypeName);
  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamX,
                                      operation.input_operand_id));

  static constexpr char kParamMean[] = "mean";
  static constexpr char kParamVariance[] = "variance";

  // TODO(crbug.com/338529226): These params must all be constant tensors.
  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kParamMean,
                                      operation.mean_operand_id));
  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kParamVariance,
                                      operation.variance_operand_id));
  if (operation.scale_operand_id.has_value()) {
    RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamGamma,
                                        *operation.scale_operand_id));
  }
  if (operation.bias_operand_id.has_value()) {
    RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamBeta,
                                        *operation.bias_operand_id));
  }

  SetInputWithValue(
      *op->mutable_inputs(), kOpParamEpsilon,
      CreateFloatValue(input_operand_info.mil_data_type, operation.epsilon));

  CoreML::Specification::MILSpec::NamedValueType& output = *op->add_outputs();
  PopulateNamedValueType(operation.output_operand_id, output);
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForCast(
    uint64_t input_operand_id,
    uint64_t output_operand_id,
    CoreML::Specification::MILSpec::Block& block) {
  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  const OperandInfo& input_operand_info = GetOperandInfo(input_operand_id);
  const OperandInfo& output_operand_info = GetOperandInfo(output_operand_id);

  const CoreML::Specification::MILSpec::DataType& input_data_type =
      input_operand_info.mil_data_type;
  const CoreML::Specification::MILSpec::DataType& output_data_type =
      output_operand_info.mil_data_type;

  // BOOL type is supported here even though it's not a WebNN supported type.
  // This is used internally by logical ops to cast the CoreML output of BOOL
  // type to WebNN expected uint8.
  if (input_data_type != CoreML::Specification::MILSpec::DataType::BOOL) {
    CHECK(context_properties_.data_type_limits.cast_input.Has(
        MILDataTypeToOperandType(input_data_type)));
  }
  if (output_data_type != CoreML::Specification::MILSpec::DataType::BOOL) {
    CHECK(context_properties_.data_type_limits.cast_input.Has(
        MILDataTypeToOperandType(output_data_type)));
  }

  RETURN_IF_ERROR(
      SetInputFromOperand(*op->mutable_inputs(), kOpParamX, input_operand_id));
  op->set_type(kOpCastTypeName);
  SetInputWithValue(
      *op->mutable_inputs(), kOpParamDataTypeName,
      CreateStringImmediateValue(MilDataTypeToString(output_data_type)));
  PopulateNamedValueType(output_operand_id, *op->add_outputs());
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForClamp(
    const mojom::Clamp& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  CHECK(context_properties_.data_type_limits.clamp_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpClipTypeName);

  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamX,
                                      operation.input_operand_id));

  SetInputsWithValues(
      *op->mutable_inputs(),
      {
          {kOpParamAlpha, CreateFloatValue(input_operand_info.mil_data_type,
                                           operation.min_value)},
          {kOpParamBeta, CreateFloatValue(input_operand_info.mil_data_type,
                                          operation.max_value)},
      });

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForConcat(
    base::span<const uint64_t> input_operand_ids,
    uint64_t output_operand_id,
    uint32_t axis,
    CoreML::Specification::MILSpec::Block& block) {
  CHECK(
      base::ranges::all_of(input_operand_ids, [&](uint64_t input_operand_id) {
        return context_properties_.data_type_limits.concat_inputs.Has(
            MILDataTypeToOperandType(
                GetOperandInfo(input_operand_id).mil_data_type));
      }));

  static constexpr char kParamValues[] = "values";
  static constexpr char kParamInterleave[] = "interleave";

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpConcatTypeName);

  for (uint64_t input_operand_id : input_operand_ids) {
    RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kParamValues,
                                        input_operand_id));
  }
  SetInputsWithValues(*op->mutable_inputs(),
                      {{kOpParamAxis, CreateScalarImmediateValue(
                                          base::checked_cast<int32_t>(axis))},
                       {kParamInterleave, CreateScalarImmediateValue(false)}});

  PopulateNamedValueType(output_operand_id, *op->add_outputs());
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForConcat(
    const mojom::Concat& operation,
    CoreML::Specification::MILSpec::Block& block) {
  return AddOperationForConcat(operation.input_operand_ids,
                               operation.output_operand_id, operation.axis,
                               block);
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForConv2d(
    const mojom::Conv2d& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand = GetOperandInfo(operation.input_operand_id);

  static constexpr char kParamStrides[] = "strides";
  static constexpr char kParamPadType[] = "pad_type";
  static constexpr char kParamPadTypeValue[] = "custom";
  static constexpr char kParamDilations[] = "dilations";
  static constexpr char kParamGroups[] = "groups";
  static constexpr char kParamOutputShape[] = "output_shape";

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  switch (operation.kind) {
    case mojom::Conv2d::Kind::kDirect:
      CHECK(context_properties_.data_type_limits.conv2d_input.Has(
          MILDataTypeToOperandType(input_operand.mil_data_type)));
      op->set_type(kOpConv2dTypeName);
      break;
    case mojom::Conv2d::Kind::kTransposed:
      CHECK(context_properties_.data_type_limits.conv_transpose2d_input.Has(
          MILDataTypeToOperandType(input_operand.mil_data_type)));
      op->set_type(kOpConvTranspose2dTypeName);
      break;
  }

  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamX,
                                      operation.input_operand_id));
  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamWeight,
                                      operation.filter_operand_id));

  std::array<int32_t, 2> strides = {
      base::checked_cast<int32_t>(operation.strides->height),
      base::checked_cast<int32_t>(operation.strides->width)};
  std::array<int32_t, 4> pad = {
      base::checked_cast<int32_t>(operation.padding->beginning->height),
      base::checked_cast<int32_t>(operation.padding->ending->height),
      base::checked_cast<int32_t>(operation.padding->beginning->width),
      base::checked_cast<int32_t>(operation.padding->ending->width)};
  std::array<int32_t, 2> dilations = {
      base::checked_cast<int32_t>(operation.dilations->height),
      base::checked_cast<int32_t>(operation.dilations->width)};

  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kParamStrides, Create1DTensorImmediateValue<int32_t>(strides)},
       {kParamPadType, CreateStringImmediateValue(kParamPadTypeValue)},
       {kOpParamPad, Create1DTensorImmediateValue<int32_t>(pad)},
       {kParamDilations, Create1DTensorImmediateValue<int32_t>(dilations)},
       {kParamGroups, CreateScalarImmediateValue(
                          base::checked_cast<int32_t>(operation.groups))}});
  if (operation.bias_operand_id) {
    // TODO(crbug.com/338529226): This param must be a constant tensor.
    RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamBias,
                                        operation.bias_operand_id.value()));
  }

  if (operation.kind == mojom::Conv2d::Kind::kTransposed) {
    // Get the output shape from the output operand.
    const OperandInfo& output_operand =
        GetOperandInfo(operation.output_operand_id);
    SetInputWithValue(*op->mutable_inputs(), kParamOutputShape,
                      Create1DTensorImmediateValue<int32_t>(
                          Ui32ToI32(output_operand.dimensions)));
  }

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForCumulativeSum(
    const mojom::CumulativeSum& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  CHECK(context_properties_.data_type_limits.cumulative_sum_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpCumulativeSumTypeName);
  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamX,
                                      operation.input_operand_id));

  static constexpr char kParamExclusive[] = "exclusive";
  static constexpr char kParamReverse[] = "reverse";

  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kOpParamAxis, CreateScalarImmediateValue(
                          base::checked_cast<int32_t>(operation.axis))},
       {kParamExclusive, CreateScalarImmediateValue(operation.exclusive)},
       {kParamReverse, CreateScalarImmediateValue(operation.reversed)}});
  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForElementwiseBinary(
    std::variant<uint64_t, CoreML::Specification::MILSpec::Value> lhs_operand,
    std::variant<uint64_t, CoreML::Specification::MILSpec::Value> rhs_operand,
    uint64_t output_operand_id,
    const mojom::ElementWiseBinary::Kind kind,
    CoreML::Specification::MILSpec::Block& block) {
  CoreML::Specification::MILSpec::DataType mil_data_type;
  std::visit(base::Overloaded{
                 [&](uint64_t lhs_operand_id) {
                   const OperandInfo& lhs_operand_info =
                       GetOperandInfo(lhs_operand_id);
                   mil_data_type = lhs_operand_info.mil_data_type;
                 },
                 [&](CoreML::Specification::MILSpec::Value lhs_value) {
                   mil_data_type = lhs_value.type().tensortype().datatype();
                 }},
             lhs_operand);
  OperandDataType input_data_type = MILDataTypeToOperandType(mil_data_type);
  std::string op_type_name;

  switch (kind) {
    case mojom::ElementWiseBinary::Kind::kAdd: {
      CHECK(
          context_properties_.data_type_limits.add_input.Has(input_data_type));
      op_type_name = kOpAddTypeName;
      break;
    }
    case mojom::ElementWiseBinary::Kind::kDiv: {
      CHECK(
          context_properties_.data_type_limits.div_input.Has(input_data_type));
      op_type_name = kOpDivideTypeName;
      break;
    }
    case mojom::ElementWiseBinary::Kind::kMul: {
      CHECK(
          context_properties_.data_type_limits.mul_input.Has(input_data_type));
      op_type_name = kOpMultiplyTypeName;
      break;
    }
    case mojom::ElementWiseBinary::Kind::kSub: {
      CHECK(
          context_properties_.data_type_limits.sub_input.Has(input_data_type));
      op_type_name = kOpSubtractTypeName;
      break;
    }
    case mojom::ElementWiseBinary::Kind::kMax: {
      CHECK(
          context_properties_.data_type_limits.max_input.Has(input_data_type));
      op_type_name = kOpMaximumTypeName;
      break;
    }
    case mojom::ElementWiseBinary::Kind::kMin: {
      CHECK(
          context_properties_.data_type_limits.min_input.Has(input_data_type));
      op_type_name = kOpMinimumTypeName;
      break;
    }
    case mojom::ElementWiseBinary::Kind::kPow: {
      CHECK(
          context_properties_.data_type_limits.pow_input.Has(input_data_type));
      op_type_name = kOpPowerTypeName;
      break;
    }
    case mojom::ElementWiseBinary::Kind::kEqual: {
      CHECK(context_properties_.data_type_limits.equal_input.Has(
          input_data_type));
      op_type_name = kOpLogicalEqual;
      break;
    }
    case mojom::ElementWiseBinary::Kind::kGreater: {
      CHECK(context_properties_.data_type_limits.greater_input.Has(
          input_data_type));
      op_type_name = kOpLogicalGreater;
      break;
    }
    case mojom::ElementWiseBinary::Kind::kGreaterOrEqual: {
      CHECK(context_properties_.data_type_limits.greater_or_equal_input.Has(
          input_data_type));
      op_type_name = kOpLogicalGreaterEqual;
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLesser: {
      CHECK(context_properties_.data_type_limits.lesser_input.Has(
          input_data_type));
      op_type_name = kOpLogicalLess;
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLesserOrEqual: {
      CHECK(context_properties_.data_type_limits.lesser_or_equal_input.Has(
          input_data_type));
      op_type_name = kOpLogicalLessEqual;
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLogicalAnd: {
      CHECK(context_properties_.data_type_limits.logical_and_input.Has(
          input_data_type));
      op_type_name = kOpLogicalAnd;
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLogicalOr: {
      CHECK(context_properties_.data_type_limits.logical_or_input.Has(
          input_data_type));
      op_type_name = kOpLogicalOr;
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLogicalXor: {
      CHECK(context_properties_.data_type_limits.logical_xor_input.Has(
          input_data_type));
      op_type_name = kOpLogicalXor;
      break;
    }
  }

  if (kind == mojom::ElementWiseBinary::Kind::kLogicalAnd ||
      kind == mojom::ElementWiseBinary::Kind::kLogicalOr ||
      kind == mojom::ElementWiseBinary::Kind::kLogicalXor) {
    // Logical binary ops in CoreML require both operands to be boolean tensors.
    CHECK(std::holds_alternative<uint64_t>(lhs_operand));
    uint64_t lhs_operand_id = std::get<uint64_t>(lhs_operand);
    ASSIGN_OR_RETURN(uint64_t cast_to_lhs_operand_id,
                     GenerateInternalOperandInfo(
                         CoreML::Specification::MILSpec::DataType::BOOL,
                         GetOperandInfo(lhs_operand_id).dimensions));
    RETURN_IF_ERROR(
        AddOperationForCast(lhs_operand_id, cast_to_lhs_operand_id, block));
    lhs_operand = cast_to_lhs_operand_id;
    mil_data_type = CoreML::Specification::MILSpec::DataType::BOOL;

    CHECK(std::holds_alternative<uint64_t>(rhs_operand));
    uint64_t rhs_operand_id = std::get<uint64_t>(rhs_operand);
    ASSIGN_OR_RETURN(uint64_t cast_to_rhs_operand_id,
                     GenerateInternalOperandInfo(
                         CoreML::Specification::MILSpec::DataType::BOOL,
                         GetOperandInfo(rhs_operand_id).dimensions));
    RETURN_IF_ERROR(
        AddOperationForCast(rhs_operand_id, cast_to_rhs_operand_id, block));
    rhs_operand = cast_to_rhs_operand_id;
  }

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(op_type_name);
  std::optional<mojom::ErrorPtr> set_input_error;
  std::visit(
      base::Overloaded{[&](uint64_t lhs_operand_id) {
                         auto result = SetInputFromOperand(
                             *op->mutable_inputs(), kOpParamX, lhs_operand_id);
                         if (!result.has_value()) {
                           set_input_error = std::move(result.error());
                         }
                       },
                       [&](CoreML::Specification::MILSpec::Value lhs_value) {
                         SetInputWithValue(*op->mutable_inputs(), kOpParamX,
                                           lhs_value);
                       }},
      lhs_operand);
  std::visit(
      base::Overloaded{[&](uint64_t rhs_operand_id) {
                         const OperandInfo& rhs_operand_info =
                             GetOperandInfo(rhs_operand_id);
                         CHECK_EQ(mil_data_type,
                                  rhs_operand_info.mil_data_type);
                         auto result = SetInputFromOperand(
                             *op->mutable_inputs(), kOpParamY, rhs_operand_id);
                         if (!result.has_value()) {
                           set_input_error = std::move(result.error());
                         }
                       },
                       [&](CoreML::Specification::MILSpec::Value rhs_value) {
                         SetInputWithValue(*op->mutable_inputs(), kOpParamY,
                                           rhs_value);
                       }},
      rhs_operand);

  if (set_input_error) {
    return base::unexpected<mojom::ErrorPtr>(*std::move(set_input_error));
  }
  if (IsLogicalElementWiseBinary(kind)) {
    // The output of logical binary ops need to be cast from a boolean
    // tensor that CoreML provides to an UInt8 that WebNN expects.
    ASSIGN_OR_RETURN(uint64_t internal_output_id,
                     GenerateInternalOperandInfo(
                         CoreML::Specification::MILSpec::DataType::BOOL,
                         GetOperandInfo(output_operand_id).dimensions));
    PopulateNamedValueType(internal_output_id, *op->add_outputs());

    return AddOperationForCast(internal_output_id, output_operand_id, block);
  } else {
    PopulateNamedValueType(output_operand_id, *op->add_outputs());
  }
  return base::ok();
}

base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForElementwiseUnary(
    mojom::ElementWiseUnary::Kind kind,
    uint64_t input_operand_id,
    uint64_t output_operand_id,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info = GetOperandInfo(input_operand_id);
  const CoreML::Specification::MILSpec::DataType input_data_type =
      input_operand_info.mil_data_type;
  const OperandDataType input_operand_data_type =
      MILDataTypeToOperandType(input_data_type);

  switch (kind) {
    case mojom::ElementWiseUnary::Kind::kAbs: {
      CHECK(context_properties_.data_type_limits.abs_input.Has(
          input_operand_data_type));
      return AddUnaryOperation(kOpAbsTypeName, input_operand_id,
                               output_operand_id, block);
    }
    case mojom::ElementWiseUnary::Kind::kCast: {
      return AddOperationForCast(input_operand_id, output_operand_id, block);
    }
    case mojom::ElementWiseUnary::Kind::kCeil: {
      CHECK(context_properties_.data_type_limits.ceil_input.Has(
          input_operand_data_type));
      return AddUnaryOperation(kOpCeilTypeName, input_operand_id,
                               output_operand_id, block);
    }
    case mojom::ElementWiseUnary::Kind::kCos: {
      CHECK(context_properties_.data_type_limits.cos_input.Has(
          input_operand_data_type));
      return AddUnaryOperation(kOpCosTypeName, input_operand_id,
                               output_operand_id, block);
    }
    case mojom::ElementWiseUnary::Kind::kErf: {
      CHECK(context_properties_.data_type_limits.erf_input.Has(
          input_operand_data_type));
      return AddUnaryOperation(kOpErfTypeName, input_operand_id,
                               output_operand_id, block);
    }
    case mojom::ElementWiseUnary::Kind::kExp: {
      CHECK(context_properties_.data_type_limits.exp_input.Has(
          input_operand_data_type));
      return AddUnaryOperation(kOpExpTypeName, input_operand_id,
                               output_operand_id, block);
    }
    case mojom::ElementWiseUnary::Kind::kFloor: {
      CHECK(context_properties_.data_type_limits.floor_input.Has(
          input_operand_data_type));
      return AddUnaryOperation(kOpFloorTypeName, input_operand_id,
                               output_operand_id, block);
    }
    case mojom::ElementWiseUnary::Kind::kIdentity: {
      CHECK(context_properties_.data_type_limits.identity_input.Has(
          input_operand_data_type));
      return AddUnaryOperation(kOpIdentityTypeName, input_operand_id,
                               output_operand_id, block);
    }
    case mojom::ElementWiseUnary::Kind::kSign: {
      // Sign is not implemented.
      NOTREACHED();
    }
    case mojom::ElementWiseUnary::Kind::kSin: {
      CHECK(context_properties_.data_type_limits.sin_input.Has(
          input_operand_data_type));
      return AddUnaryOperation(kOpSinTypeName, input_operand_id,
                               output_operand_id, block);
    }
    case mojom::ElementWiseUnary::Kind::kSqrt: {
      CHECK(context_properties_.data_type_limits.sqrt_input.Has(
          input_operand_data_type));
      return AddUnaryOperation(kOpSqrtTypeName, input_operand_id,
                               output_operand_id, block);
    }
    case mojom::ElementWiseUnary::Kind::kTan: {
      CHECK(context_properties_.data_type_limits.tan_input.Has(
          input_operand_data_type));
      return AddUnaryOperation(kOpTanTypeName, input_operand_id,
                               output_operand_id, block);
    }
    case mojom::ElementWiseUnary::Kind::kReciprocal: {
      CHECK(context_properties_.data_type_limits.reciprocal_input.Has(
          input_operand_data_type));
      // CoreML's reciprocal operator requires an epsilon value, the default
      // value as per the documentation 1e-4 results in expressions like
      // reciprocal(4) returning  0.24999 rather than 0.25.
      // In order to return expected results similar to other platforms,
      // set epsilon to 0.
      return AddUnaryFloatsOperationWithEpsilon(
          kOpReciprocalTypeName, input_operand_id, output_operand_id,
          /*epsilon=*/0, block);
    }
    case mojom::ElementWiseUnary::Kind::kLog: {
      CHECK(context_properties_.data_type_limits.log_input.Has(
          input_operand_data_type));
      // CoreML's log operator requires an epsilon value, the default
      // value as per the documentation 1e-45 potentially could result
      // in different result compared to other platforms.
      // In order to return expected results compatible with other
      // platforms, set epsilon to 0.
      return AddUnaryFloatsOperationWithEpsilon(
          kOpLogTypeName, input_operand_id, output_operand_id,
          /*epsilon=*/0, block);
    }
    case mojom::ElementWiseUnary::Kind::kNeg: {
      CHECK(context_properties_.data_type_limits.neg_input.Has(
          input_operand_data_type));
      // Implement this as mul(a, -1)
      CoreML::Specification::MILSpec::Value negative_one_value;
      switch (input_data_type) {
        case CoreML::Specification::MILSpec::DataType::FLOAT32:
          negative_one_value = CreateScalarImmediateValue<float>(-1.0f);
          break;
        case CoreML::Specification::MILSpec::DataType::FLOAT16:
          negative_one_value = CreateScalarImmediateValue<Float16>(
              static_cast<Float16>(fp16_ieee_from_fp32_value(-1.0f)));
          break;
        case CoreML::Specification::MILSpec::DataType::INT32:
          negative_one_value = CreateScalarImmediateValue<int32_t>(-1);
          break;
        default:
          NOTREACHED();
      }
      return AddOperationForElementwiseBinary(
          /*lhs_operand_id=*/input_operand_id,
          /*rhs_operand=*/negative_one_value,
          /*output_operand_id=*/output_operand_id,
          mojom::ElementWiseBinary::Kind::kMul, block);
    }
    case mojom::ElementWiseUnary::Kind::kLogicalNot: {
      CHECK(context_properties_.data_type_limits.logical_not_input.Has(
          input_operand_data_type));
      ASSIGN_OR_RETURN(uint64_t cast_to_bool_operand_id,
                       GenerateInternalOperandInfo(
                           CoreML::Specification::MILSpec::DataType::BOOL,
                           input_operand_info.dimensions));
      RETURN_IF_ERROR(AddOperationForCast(input_operand_id,
                                          cast_to_bool_operand_id, block));
      ASSIGN_OR_RETURN(uint64_t logical_not_output_operand_id,
                       GenerateInternalOperandInfo(
                           CoreML::Specification::MILSpec::DataType::BOOL,
                           input_operand_info.dimensions));
      RETURN_IF_ERROR(AddUnaryOperation(kOpLogicalNot, cast_to_bool_operand_id,
                                        logical_not_output_operand_id, block));
      return AddOperationForCast(logical_not_output_operand_id,
                                 output_operand_id, block);
    }
  }
}

base::expected<void, mojom::ErrorPtr> GraphBuilderCoreml::AddOperationForElu(
    const mojom::Elu& operation,
    CoreML::Specification::MILSpec::Block& block) {
  CHECK(context_properties_.data_type_limits.elu_input.Has(
      MILDataTypeToOperandType(
          GetOperandInfo(operation.input_operand_id).mil_data_type)));

  ASSIGN_OR_RETURN(
      CoreML::Specification::MILSpec::Operation * op,
      CreateUnaryOperation(SupportedDataType::kFloats, kOpEluTypeName,
                           operation.input_operand_id,
                           operation.output_operand_id, block, ops::kElu));

  SetInputWithValue(*op->mutable_inputs(), kOpParamAlpha,
                    CreateScalarImmediateValue<float>(operation.alpha));
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilderCoreml::AddOperationForExpand(
    const mojom::Expand& operation,
    CoreML::Specification::MILSpec::Block& block) {
  // Emulated by reshaping to output shape, then tile.
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  const OperandInfo& output_operand_info =
      GetOperandInfo(operation.output_operand_id);

  CHECK(context_properties_.data_type_limits.expand_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  uint64_t reshaped_input = operation.input_operand_id;
  size_t input_rank = input_operand_info.dimensions.size();
  size_t output_rank = output_operand_info.dimensions.size();
  std::vector<uint32_t> reshaped_dimensions(output_rank, 1);
  if (input_rank < output_rank) {
    // According to broadcasting rules, right align the dimensions and fill
    // beginning dimensions with ones.
    for (size_t i = 0; i < input_rank; ++i) {
      reshaped_dimensions[output_rank - i - 1] =
          input_operand_info.dimensions[input_rank - i - 1];
    }

    ASSIGN_OR_RETURN(reshaped_input, GenerateInternalOperandInfo(
                                         input_operand_info.mil_data_type,
                                         reshaped_dimensions));
    RETURN_IF_ERROR(AddOperationForReshape(operation.input_operand_id,
                                           reshaped_input, block));
  } else {
    reshaped_dimensions = input_operand_info.dimensions;
  }

  // Dimension i of input will be replicated reps[i] times.
  base::FixedArray<int32_t> reps(output_rank);
  for (size_t i = 0; i < output_rank; ++i) {
    if (output_operand_info.dimensions[i] == reshaped_dimensions[i]) {
      reps[i] = 1u;
    } else {
      CHECK_EQ(reshaped_dimensions[i], 1u);
      reps[i] = base::checked_cast<int32_t>(output_operand_info.dimensions[i]);
    }
  }
  ASSIGN_OR_RETURN(
      CoreML::Specification::MILSpec::Operation * op,
      CreateUnaryOperation(SupportedDataType::kFloatsAndInt32,
                           kOpExpandTypeName, reshaped_input,
                           operation.output_operand_id, block, ops::kExpand));

  SetInputWithValue(*op->mutable_inputs(), kOpParamReps,
                    Create1DTensorImmediateValue<int32_t>(reps));
  return base::ok();
}

void GraphBuilderCoreml::AddOperationForFill(
    CoreML::Specification::MILSpec::Value value,
    uint64_t output_operand_id,
    CoreML::Specification::MILSpec::Block& block) {
  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpFillTypeName);
  static constexpr char kParamValue[] = "value";
  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kOpParamShape, Create1DTensorImmediateValue<int32_t>(Ui32ToI32(
                           GetOperandInfo(output_operand_id).dimensions))},
       {kParamValue, std::move(value)}});
  PopulateNamedValueType(output_operand_id, *op->add_outputs());
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForGather(
    const mojom::Gather& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  const OperandInfo& indices_operand_info =
      GetOperandInfo(operation.indices_operand_id);

  CHECK(context_properties_.data_type_limits.gather_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));
  CHECK(context_properties_.data_type_limits.gather_indices.Has(
      MILDataTypeToOperandType(indices_operand_info.mil_data_type)));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpGatherTypeName);
  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamX,
                                      operation.input_operand_id));

  // TODO(crbug.com/339087333): Handle negative and out-of-bounds indices.
  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamIndices,
                                      operation.indices_operand_id));

  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kOpParamAxis, CreateScalarImmediateValue(
                          base::checked_cast<int32_t>(operation.axis))},
       {kOpParamValidateIndices, CreateScalarImmediateValue(false)}});

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForGatherElements(
    const mojom::GatherElements& operation,
    CoreML::Specification::MILSpec::Block& block) {
  CHECK(context_properties_.data_type_limits.gather_elements_input.Has(
      MILDataTypeToOperandType(
          GetOperandInfo(operation.input_operand_id).mil_data_type)));
  CHECK(context_properties_.data_type_limits.gather_elements_indices.Has(
      MILDataTypeToOperandType(
          GetOperandInfo(operation.indices_operand_id).mil_data_type)));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpGatherElementsTypeName);
  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamX,
                                      operation.input_operand_id));

  // TODO(crbug.com/339087333): Handle negative and out-of-bounds indices.
  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamIndices,
                                      operation.indices_operand_id));

  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kOpParamAxis, CreateScalarImmediateValue(
                          base::checked_cast<int32_t>(operation.axis))},
       {kOpParamValidateIndices, CreateScalarImmediateValue(false)}});

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForGatherND(
    const mojom::GatherND& operation,
    CoreML::Specification::MILSpec::Block& block) {
  CHECK(context_properties_.data_type_limits.gather_nd_input.Has(
      MILDataTypeToOperandType(
          GetOperandInfo(operation.input_operand_id).mil_data_type)));
  CHECK(context_properties_.data_type_limits.gather_nd_indices.Has(
      MILDataTypeToOperandType(
          GetOperandInfo(operation.indices_operand_id).mil_data_type)));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpGatherNdTypeName);
  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamX,
                                      operation.input_operand_id));

  // TODO(crbug.com/339087333): Handle negative and out-of-bounds indices.
  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamIndices,
                                      operation.indices_operand_id));

  SetInputWithValue(*op->mutable_inputs(), kOpParamValidateIndices,
                    CreateScalarImmediateValue(false));

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForGelu(
    const mojom::Gelu& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  CHECK(context_properties_.data_type_limits.gelu_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpGeluTypeName);
  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamX,
                                      operation.input_operand_id));

  constexpr char kParamModeExact[] = "EXACT";

  SetInputWithValue(*op->mutable_inputs(), kOpParamMode,
                    CreateStringImmediateValue(kParamModeExact));

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilderCoreml::AddOperationForGemm(
    uint64_t a_operand_id,
    uint64_t b_operand_id,
    std::optional<uint64_t> c_operand_id,
    uint64_t output_operand_id,
    CoreML::Specification::MILSpec::Block& block,
    bool a_transpose,
    bool b_transpose,
    float alpha,
    float beta) {
  // Gemm is not supported in CoreML. This is emulated with:
  //   add(mul(alpha, matmul(A, B)), mul(beta, C))
  const OperandInfo& a_operand_info = GetOperandInfo(a_operand_id);
  const OperandInfo& b_operand_info = GetOperandInfo(b_operand_id);
  CHECK(a_operand_info.dimensions.size() == 2 &&
        b_operand_info.dimensions.size() == 2);
  CHECK(context_properties_.data_type_limits.gemm_input.Has(
      MILDataTypeToOperandType(a_operand_info.mil_data_type)));
  CHECK_EQ(a_operand_info.mil_data_type, b_operand_info.mil_data_type);

  uint32_t first_dimension =
      a_transpose ? a_operand_info.dimensions[1] : a_operand_info.dimensions[0];
  uint32_t second_dimension =
      b_transpose ? b_operand_info.dimensions[0] : b_operand_info.dimensions[1];

  std::array<uint32_t, 2> matmul_dimensions{first_dimension, second_dimension};
  if (alpha == 1.0f && !c_operand_id) {
    return AddOperationForMatmul(a_operand_id, b_operand_id, a_transpose,
                                 b_transpose, output_operand_id, block);
  }

  ASSIGN_OR_RETURN(uint64_t matmul_output,
                   GenerateInternalOperandInfo(a_operand_info.mil_data_type,
                                               matmul_dimensions));
  RETURN_IF_ERROR(AddOperationForMatmul(a_operand_id, b_operand_id, a_transpose,
                                        b_transpose, matmul_output, block));

  if (alpha != 1.0f) {
    uint64_t with_alpha_output = output_operand_id;
    if (c_operand_id) {
      ASSIGN_OR_RETURN(with_alpha_output,
                       GenerateInternalOperandInfo(a_operand_info.mil_data_type,
                                                   matmul_dimensions));
    }

    RETURN_IF_ERROR(AddOperationForElementwiseBinary(
        matmul_output, CreateFloatValue(a_operand_info.mil_data_type, alpha),
        with_alpha_output, mojom::ElementWiseBinary::Kind::kMul, block));
    matmul_output = with_alpha_output;
  }

  if (!c_operand_id) {
    return base::ok();
  }
  const OperandInfo& c_operand_info = GetOperandInfo(*c_operand_id);
  CHECK_EQ(a_operand_info.mil_data_type, c_operand_info.mil_data_type);

  if (beta != 1.0f) {
    ASSIGN_OR_RETURN(uint64_t with_beta_output,
                     GenerateInternalOperandInfo(a_operand_info.mil_data_type,
                                                 matmul_dimensions));
    RETURN_IF_ERROR(AddOperationForElementwiseBinary(
        *c_operand_id, CreateFloatValue(c_operand_info.mil_data_type, beta),
        with_beta_output, mojom::ElementWiseBinary::Kind::kMul, block));
    c_operand_id = with_beta_output;
  }
  return AddOperationForElementwiseBinary(
      matmul_output, *c_operand_id, output_operand_id,
      mojom::ElementWiseBinary::Kind::kAdd, block);
}

base::expected<void, mojom::ErrorPtr> GraphBuilderCoreml::AddOperationForGemm(
    const mojom::Gemm& operation,
    CoreML::Specification::MILSpec::Block& block) {
  return AddOperationForGemm(
      operation.a_operand_id, operation.b_operand_id, operation.c_operand_id,
      operation.output_operand_id, block, operation.a_transpose,
      operation.b_transpose, operation.alpha, operation.beta);
}

base::expected<void, mojom::ErrorPtr> GraphBuilderCoreml::AddOperationForGru(
    const mojom::Gru& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  CoreML::Specification::MILSpec::DataType data_type =
      input_operand_info.mil_data_type;

  CHECK(context_properties_.data_type_limits.gru_input.Has(
      MILDataTypeToOperandType(data_type)));
  CHECK_EQ(GetOperandInfo(operation.weight_operand_id).mil_data_type,
           data_type);
  CHECK_EQ(GetOperandInfo(operation.recurrent_weight_operand_id).mil_data_type,
           data_type);
  if (operation.initial_hidden_state_operand_id) {
    CHECK_EQ(GetOperandInfo(*operation.initial_hidden_state_operand_id)
                 .mil_data_type,
             data_type);
  }
  if (operation.bias_operand_id) {
    CHECK_EQ(GetOperandInfo(*operation.bias_operand_id).mil_data_type,
             data_type);
  }
  if (operation.recurrent_bias_operand_id) {
    CHECK_EQ(GetOperandInfo(*operation.recurrent_bias_operand_id).mil_data_type,
             data_type);
  }

  // Input shape is [steps, batch_size, input_size].
  CHECK_EQ(input_operand_info.dimensions.size(), 3u);
  uint32_t batch_size = input_operand_info.dimensions[1];
  uint32_t input_size = input_operand_info.dimensions[2];
  uint32_t hidden_size = operation.hidden_size;
  uint32_t steps = operation.steps;
  size_t num_of_directions =
      operation.direction == mojom::RecurrentNetworkDirection::kBoth ? 2 : 1;
  base::FixedArray<uint64_t> initial_hidden_states(num_of_directions);
  base::FixedArray<uint64_t> weights(num_of_directions);
  base::FixedArray<uint64_t> recurrent_weights(num_of_directions);
  base::FixedArray<uint64_t> biases(num_of_directions);
  base::FixedArray<uint64_t> recurrent_biases(num_of_directions);

  if (operation.initial_hidden_state_operand_id) {
    RETURN_IF_ERROR(SplitAndSqueeze(*operation.initial_hidden_state_operand_id,
                                    initial_hidden_states, /*axis=*/0, block));
  } else {
    // When initial hidden state is not provided, use a tensor filled with
    // zeros.
    for (size_t i = 0; i < num_of_directions; i++) {
      ASSIGN_OR_RETURN(
          initial_hidden_states[i],
          GenerateInternalOperandInfo(
              data_type,
              base::span<const uint32_t>({batch_size, operation.hidden_size})));

      AddOperationForFill(
          CreateFloatValue(input_operand_info.mil_data_type, 0.0f),
          initial_hidden_states[i], block);
    }
  }

  // Split bidrectional weights and biases.
  RETURN_IF_ERROR(
      SplitAndSqueeze(operation.weight_operand_id, weights, 0, block));
  RETURN_IF_ERROR(SplitAndSqueeze(operation.recurrent_weight_operand_id,
                                  recurrent_weights, 0, block));
  if (operation.bias_operand_id) {
    RETURN_IF_ERROR(
        SplitAndSqueeze(*operation.bias_operand_id, biases, 0, block));
  }
  if (operation.recurrent_bias_operand_id) {
    RETURN_IF_ERROR(SplitAndSqueeze(*operation.recurrent_bias_operand_id,
                                    recurrent_biases, /*axis=*/0, block));
  }
  base::FixedArray<uint64_t> hidden_results(num_of_directions);
  base::FixedArray<uint64_t> last_step_results(num_of_directions);

  for (size_t direction = 0; direction < num_of_directions; direction++) {
    bool backward_direction =
        direction == 1 ||
        operation.direction == mojom::RecurrentNetworkDirection::kBackward;

    CHECK_EQ(input_operand_info.dimensions.size(), 3u);
    // weights and biases for individual gates.
    base::FixedArray<uint32_t> weight_shape({hidden_size, input_size});
    base::FixedArray<uint32_t> recurrent_weight_shape(
        {hidden_size, hidden_size});
    base::FixedArray<uint32_t> bias_shape({hidden_size});

    base::FixedArray<uint64_t> weights_per_gate(3);
    base::FixedArray<uint64_t> recurrent_weights_per_gate(3);
    base::FixedArray<uint64_t> biases_per_gate(3);
    base::FixedArray<uint64_t> recurrent_biases_per_gate(3);
    for (size_t i = 0; i < 3; i++) {
      ASSIGN_OR_RETURN(weights_per_gate[i],
                       GenerateInternalOperandInfo(data_type, weight_shape));
      ASSIGN_OR_RETURN(
          recurrent_weights_per_gate[i],
          GenerateInternalOperandInfo(data_type, recurrent_weight_shape));
      ASSIGN_OR_RETURN(biases_per_gate[i],
                       GenerateInternalOperandInfo(data_type, bias_shape));
      ASSIGN_OR_RETURN(recurrent_biases_per_gate[i],
                       GenerateInternalOperandInfo(data_type, bias_shape));
    }
    RETURN_IF_ERROR(AddOperationForSplit(weights[direction], weights_per_gate,
                                         /*axis=*/0, block));
    RETURN_IF_ERROR(AddOperationForSplit(recurrent_weights[direction],
                                         recurrent_weights_per_gate, /*axis=*/0,
                                         block));
    if (operation.bias_operand_id) {
      RETURN_IF_ERROR(AddOperationForSplit(biases[direction], biases_per_gate,
                                           /*axis=*/0, block));
    }
    if (operation.recurrent_bias_operand_id) {
      RETURN_IF_ERROR(AddOperationForSplit(recurrent_biases[direction],
                                           recurrent_biases_per_gate,
                                           /*axis=*/0, block));
    }

    // Setup hidden_list: [steps, batch_size, hidden_size]
    ASSIGN_OR_RETURN(uint64_t hidden_list,
                     GenerateInternalOperandInfo(
                         data_type, base::span<const uint32_t>(
                                        {steps, batch_size, hidden_size})));
    AddOperationForFill(CreateFloatValue(data_type, 0.0f), hidden_list, block);

    // Previous hidden state from previous step, starts with
    // initial_hidden_state.
    uint64_t hidden_prev = initial_hidden_states[direction];
    for (size_t step = 0; step < steps; step++) {
      size_t step_index = backward_direction ? steps - step - 1 : step;
      ASSIGN_OR_RETURN(
          uint64_t sliced_input,
          SliceFirstDimension(operation.input_operand_id, step_index, block));
      ASSIGN_OR_RETURN(uint64_t new_hidden_state,
                       GenerateInternalOperandInfo(
                           data_type, base::span<const uint32_t>(
                                          {batch_size, hidden_size})));
      RETURN_IF_ERROR(AddOperationForGruSingleStep(
          sliced_input, hidden_prev, new_hidden_state, weights_per_gate,
          recurrent_weights_per_gate,
          operation.bias_operand_id
              ? std::optional<base::span<const uint64_t>>(biases_per_gate)
              : std::nullopt,
          operation.recurrent_bias_operand_id
              ? std::optional<base::span<const uint64_t>>(
                    recurrent_biases_per_gate)
              : std::nullopt,
          operation.hidden_size, operation.layout, operation.activations[0],
          operation.activations[1], operation.reset_after, block));
      // Expand `new_hidden_state` to [1, batch_size, hidden_dim] so can be
      // added to hidden_list
      ASSIGN_OR_RETURN(uint64_t h,
                       GenerateInternalOperandInfo(
                           data_type, base::span<const uint32_t>(
                                          {1, batch_size, hidden_size})));
      RETURN_IF_ERROR(AddOperationForReshape(new_hidden_state, h, block));
      ASSIGN_OR_RETURN(uint64_t scatter_indices,
                       GenerateInternalOperandInfo(
                           CoreML::Specification::MILSpec::DataType::INT32,
                           base::span<const uint32_t>({1, 1})));
      AddOperationForFill(CreateScalarImmediateValue<int32_t>(step_index),
                          scatter_indices, block);
      uint64_t hidden_list_prev = hidden_list;
      ASSIGN_OR_RETURN(hidden_list,
                       GenerateInternalOperandInfo(
                           data_type, base::span<const uint32_t>(
                                          {steps, batch_size, hidden_size})));
      RETURN_IF_ERROR(AddOperationForScatterND(
          hidden_list_prev, scatter_indices, h, hidden_list, block));
      hidden_prev = new_hidden_state;
    }
    // Add the num_directions dimension so later all directions can be
    // concatenated.
    ASSIGN_OR_RETURN(hidden_results[direction],
                     GenerateInternalOperandInfo(
                         data_type, base::span<const uint32_t>(
                                        {operation.steps, /*num_directions=*/1,
                                         batch_size, operation.hidden_size})));
    RETURN_IF_ERROR(
        AddOperationForReshape(hidden_list, hidden_results[direction], block));

    ASSIGN_OR_RETURN(last_step_results[direction],
                     GenerateInternalOperandInfo(
                         data_type, base::span<const uint32_t>(
                                        {/*num_directions=*/1, batch_size,
                                         operation.hidden_size})));
    RETURN_IF_ERROR(AddOperationForReshape(
        hidden_prev, last_step_results[direction], block));
  }
  RETURN_IF_ERROR(AddOperationForConcat(last_step_results,
                                        operation.output_operand_ids[0],
                                        /*axis=*/0, block));
  //  [steps, num_directions, batch_size, hidden_size], concat in num_directions
  //  axis.
  if (operation.return_sequence) {
    RETURN_IF_ERROR(AddOperationForConcat(hidden_results,
                                          operation.output_operand_ids[1],
                                          /*axis=*/1, block));
  }

  return base::ok();
}

base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForGruCell(
    const mojom::GruCell& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  CoreML::Specification::MILSpec::DataType data_type =
      input_operand_info.mil_data_type;

  CHECK(context_properties_.data_type_limits.gru_cell_input.Has(
      MILDataTypeToOperandType(data_type)));
  CHECK_EQ(GetOperandInfo(operation.weight_operand_id).mil_data_type,
           data_type);
  CHECK_EQ(GetOperandInfo(operation.recurrent_weight_operand_id).mil_data_type,
           data_type);
  CHECK_EQ(GetOperandInfo(operation.hidden_state_operand_id).mil_data_type,
           data_type);
  if (operation.bias_operand_id) {
    CHECK_EQ(GetOperandInfo(*operation.bias_operand_id).mil_data_type,
             data_type);
  }
  if (operation.recurrent_bias_operand_id) {
    CHECK_EQ(GetOperandInfo(*operation.recurrent_bias_operand_id).mil_data_type,
             data_type);
  }

  CHECK_EQ(input_operand_info.dimensions.size(), 2u);
  uint32_t input_size = input_operand_info.dimensions[1];
  uint32_t hidden_size = operation.hidden_size;
  // weights and biases for individual gates.
  base::FixedArray<uint32_t> weight_shape({hidden_size, input_size});
  base::FixedArray<uint32_t> recurrent_weight_shape({hidden_size, hidden_size});
  base::FixedArray<uint32_t> bias_shape({hidden_size});

  base::FixedArray<uint64_t> weights_per_gate(3);
  base::FixedArray<uint64_t> recurrent_weights_per_gate(3);
  base::FixedArray<uint64_t> biases_per_gate(3);
  base::FixedArray<uint64_t> recurrent_biases_per_gate(3);

  for (size_t i = 0; i < 3; i++) {
    ASSIGN_OR_RETURN(weights_per_gate[i],
                     GenerateInternalOperandInfo(data_type, weight_shape));
    ASSIGN_OR_RETURN(
        recurrent_weights_per_gate[i],
        GenerateInternalOperandInfo(data_type, recurrent_weight_shape));
    ASSIGN_OR_RETURN(biases_per_gate[i],
                     GenerateInternalOperandInfo(data_type, bias_shape));
    ASSIGN_OR_RETURN(recurrent_biases_per_gate[i],
                     GenerateInternalOperandInfo(data_type, bias_shape));
  }
  RETURN_IF_ERROR(AddOperationForSplit(operation.weight_operand_id,
                                       weights_per_gate, 0, block));
  RETURN_IF_ERROR(AddOperationForSplit(operation.recurrent_weight_operand_id,
                                       recurrent_weights_per_gate, /*axis=*/0,
                                       block));
  if (operation.bias_operand_id) {
    RETURN_IF_ERROR(AddOperationForSplit(*operation.bias_operand_id,
                                         biases_per_gate, 0, block));
  }
  if (operation.recurrent_bias_operand_id) {
    RETURN_IF_ERROR(AddOperationForSplit(*operation.recurrent_bias_operand_id,
                                         recurrent_biases_per_gate, /*axis=*/0,
                                         block));
  }

  return AddOperationForGruSingleStep(
      operation.input_operand_id, operation.hidden_state_operand_id,
      operation.output_operand_id, weights_per_gate, recurrent_weights_per_gate,
      operation.bias_operand_id
          ? std::optional<base::span<const uint64_t>>(biases_per_gate)
          : std::nullopt,
      operation.recurrent_bias_operand_id
          ? std::optional<base::span<const uint64_t>>(recurrent_biases_per_gate)
          : std::nullopt,
      operation.hidden_size, operation.layout, operation.activations[0],
      operation.activations[1], operation.reset_after, block);
}

base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForGruSingleStep(
    uint64_t input_operand_id,
    uint64_t hidden_state_operand_id,
    uint64_t output_operand_id,
    base::span<const uint64_t> weights,
    base::span<const uint64_t> recurrent_weights,
    std::optional<base::span<const uint64_t>> biases,
    std::optional<base::span<const uint64_t>> recurrent_biases,
    uint32_t hidden_size,
    mojom::GruWeightLayout layout,
    mojom::RecurrentNetworkActivation activation,
    mojom::RecurrentNetworkActivation output_activation,
    bool reset_after,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info = GetOperandInfo(input_operand_id);

  CHECK_EQ(input_operand_info.dimensions.size(), 2u);
  // Input shape is [batch_size, input_size].
  uint32_t batch_size = input_operand_info.dimensions[0];
  CoreML::Specification::MILSpec::DataType data_type =
      input_operand_info.mil_data_type;

  // Results for reset and update gate.
  base::FixedArray<uint64_t> r_z_results(2);
  // The formula is the same for reset and update gate.
  for (size_t result_index = 0; result_index < r_z_results.size();
       result_index++) {
    size_t gate_index = GetGruGateIndex(
        (result_index == 0) ? GruGate::kReset : GruGate::kUpdate, layout);
    // Holds intermediate results for current gate calculation.
    base::FixedArray<uint64_t> gate_results(4);
    for (size_t i = 0; i < 4; i++) {
      ASSIGN_OR_RETURN(gate_results[i],
                       GenerateInternalOperandInfo(
                           data_type, base::span<const uint32_t>(
                                          {batch_size, hidden_size})));
    }

    RETURN_IF_ERROR(AddOperationForGemm(
        input_operand_id, weights[gate_index],
        biases ? std::optional<uint64_t>((*biases)[gate_index]) : std::nullopt,
        gate_results[0], block, /*a_transpose=*/false, /*b_transpose=*/true));

    RETURN_IF_ERROR(AddOperationForGemm(
        hidden_state_operand_id, recurrent_weights[gate_index],
        recurrent_biases
            ? std::optional<uint64_t>((*recurrent_biases)[gate_index])
            : std::nullopt,
        gate_results[1], block, /*a_transpose=*/false, /*b_transpose=*/true));

    RETURN_IF_ERROR(AddOperationForElementwiseBinary(
        gate_results[0], gate_results[1], gate_results[2],
        mojom::ElementWiseBinary::Kind::kAdd, block));
    RETURN_IF_ERROR(AddUnaryOperation(GetActivationOpName(activation),
                                      gate_results[2], gate_results[3], block));

    r_z_results[result_index] = gate_results[3];
  }

  size_t gate_index = GetGruGateIndex(GruGate::kNew, layout);

  // Holds intermediate results for new gate.
  base::FixedArray<uint64_t> new_results(5);
  for (size_t i = 0; i < new_results.size(); i++) {
    ASSIGN_OR_RETURN(
        new_results[i],
        GenerateInternalOperandInfo(
            data_type, base::span<const uint32_t>({batch_size, hidden_size})));
  }
  uint64_t reset = r_z_results[0];
  uint64_t update = r_z_results[1];
  RETURN_IF_ERROR(AddOperationForGemm(
      input_operand_id, weights[gate_index],
      biases ? std::optional<uint64_t>((*biases)[gate_index]) : std::nullopt,
      new_results[0], block, /*a_transpose=*/false, /*b_transpose=*/true));
  if (reset_after) {
    RETURN_IF_ERROR(AddOperationForGemm(
        hidden_state_operand_id, recurrent_weights[gate_index],
        recurrent_biases
            ? std::optional<uint64_t>((*recurrent_biases)[gate_index])
            : std::nullopt,
        new_results[1], block, /*a_transpose=*/false, /*b_transpose=*/true));
    RETURN_IF_ERROR(AddOperationForElementwiseBinary(
        reset, new_results[1], new_results[2],
        mojom::ElementWiseBinary::Kind::kMul, block));
  } else {
    RETURN_IF_ERROR(AddOperationForElementwiseBinary(
        reset, hidden_state_operand_id, new_results[1],
        mojom::ElementWiseBinary::Kind::kMul, block));
    RETURN_IF_ERROR(AddOperationForGemm(
        new_results[1], recurrent_weights[gate_index],
        recurrent_biases
            ? std::optional<uint64_t>((*recurrent_biases)[gate_index])
            : std::nullopt,
        new_results[2], block, /*a_transpose=*/false, /*b_transpose=*/true));
  }
  RETURN_IF_ERROR(AddOperationForElementwiseBinary(
      new_results[0], new_results[2], new_results[3],
      mojom::ElementWiseBinary::Kind::kAdd, block));

  RETURN_IF_ERROR(AddUnaryOperation(GetActivationOpName(output_activation),
                                    new_results[3], new_results[4], block));

  // h = (1-update_result) * new_result + update_result * h_prev
  // h : (batch_size, hidden_dim)
  base::FixedArray<uint64_t> hidden_results(3);
  for (size_t i = 0; i < hidden_results.size(); i++) {
    ASSIGN_OR_RETURN(
        hidden_results[i],
        GenerateInternalOperandInfo(
            data_type, base::span<const uint32_t>({batch_size, hidden_size})));
  }
  RETURN_IF_ERROR(AddOperationForElementwiseBinary(
      CreateFloatValue(data_type, 1.0f), update, hidden_results[0],
      mojom::ElementWiseBinary::Kind::kSub, block));
  RETURN_IF_ERROR(AddOperationForElementwiseBinary(
      hidden_results[0], new_results[4], hidden_results[1],
      mojom::ElementWiseBinary::Kind::kMul, block));
  RETURN_IF_ERROR(AddOperationForElementwiseBinary(
      update, hidden_state_operand_id, hidden_results[2],
      mojom::ElementWiseBinary::Kind::kMul, block));
  RETURN_IF_ERROR(AddOperationForElementwiseBinary(
      hidden_results[1], hidden_results[2], output_operand_id,
      mojom::ElementWiseBinary::Kind::kAdd, block));
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForHardSigmoid(
    uint64_t input_operand_id,
    float alpha,
    float beta,
    uint64_t output_operand_id,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info = GetOperandInfo(input_operand_id);
  CHECK(context_properties_.data_type_limits.hard_sigmoid_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpHardSigmoidTypeName);

  RETURN_IF_ERROR(
      SetInputFromOperand(*op->mutable_inputs(), kOpParamX, input_operand_id));

  SetInputsWithValues(
      *op->mutable_inputs(),
      {
          {kOpParamAlpha,
           CreateFloatValue(input_operand_info.mil_data_type, alpha)},
          {kOpParamBeta,
           CreateFloatValue(input_operand_info.mil_data_type, beta)},
      });

  PopulateNamedValueType(output_operand_id, *op->add_outputs());
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForHardSigmoid(
    const mojom::HardSigmoid& operation,
    CoreML::Specification::MILSpec::Block& block) {
  return AddOperationForHardSigmoid(operation.input_operand_id, operation.alpha,
                                    operation.beta, operation.output_operand_id,
                                    block);
}

base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForHardSwish(
    const mojom::HardSwish& operation,
    CoreML::Specification::MILSpec::Block& block) {
  // Hardswish is not supported in CoreML, the formula is:
  //  x * max(0, min(6, (x + 3))) / 6
  // This is mathematically equivalent to:
  //  x * max(min((x+3)/6, 1), 0)
  // Hardsigmoid is max(min(alpha * x + beta, 1), 0), so hardswish can be
  // emulated by: mul(x, hardsigmoid(x, alpha=1.0/6, beta=0.5))
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  CHECK(context_properties_.data_type_limits.hard_swish_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));
  ASSIGN_OR_RETURN(uint64_t hardsigmoid_output,
                   GenerateInternalOperandInfo(input_operand_info.mil_data_type,
                                               input_operand_info.dimensions));

  constexpr static float alpha = float(1.0 / 6);
  constexpr static float beta = float(0.5);

  RETURN_IF_ERROR(AddOperationForHardSigmoid(operation.input_operand_id, alpha,
                                             beta, hardsigmoid_output, block));
  RETURN_IF_ERROR(AddOperationForElementwiseBinary(
      operation.input_operand_id, hardsigmoid_output,
      operation.output_operand_id, mojom::ElementWiseBinary::Kind::kMul,
      block));
  return base::ok();
}

base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForInstanceNormalization(
    const mojom::InstanceNormalization& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  CHECK(context_properties_.data_type_limits.instance_normalization_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  if (operation.layout != mojom::InputOperandLayout::kChannelsFirst) {
    // TODO(crbug.com/338398666) Support channels-last by adding transposes.
    return NewNotSupportedError("Unsupported input layout.");
  }

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpInstanceNormalizationTypeName);
  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamX,
                                      operation.input_operand_id));

  // TODO(crbug.com/338529226): These params must all be constant tensors.
  if (operation.scale_operand_id.has_value()) {
    RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamGamma,
                                        *operation.scale_operand_id));
  }
  if (operation.bias_operand_id.has_value()) {
    RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamBeta,
                                        *operation.bias_operand_id));
  }

  SetInputWithValue(
      *op->mutable_inputs(), kOpParamEpsilon,
      CreateFloatValue(input_operand_info.mil_data_type, operation.epsilon));

  CoreML::Specification::MILSpec::NamedValueType& output = *op->add_outputs();
  PopulateNamedValueType(operation.output_operand_id, output);
  return base::ok();
}

base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForLayerNormalization(
    const mojom::LayerNormalization& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  CHECK(context_properties_.data_type_limits.layer_normalization_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  // CoreML doesn't support empty axes. When axes is empty, the mean equals to
  // input, output = bias + (scale * 0)
  if (operation.axes.empty()) {
    uint64_t zeros = operation.output_operand_id;
    if (operation.bias_operand_id) {
      ASSIGN_OR_RETURN(
          zeros, GenerateInternalOperandInfo(input_operand_info.mil_data_type,
                                             input_operand_info.dimensions));
    }
    // input-input is zero, no need to multiply scale then divide by
    // sqrt(variance + epsilon).
    RETURN_IF_ERROR(AddOperationForElementwiseBinary(
        operation.input_operand_id, operation.input_operand_id, zeros,
        mojom::ElementWiseBinary::Kind::kSub, block));

    if (operation.bias_operand_id) {
      RETURN_IF_ERROR(AddOperationForElementwiseBinary(
          zeros, *operation.bias_operand_id, operation.output_operand_id,
          mojom::ElementWiseBinary::Kind::kAdd, block));
    }

    return base::ok();
  }

  // TODO: crbug.com/356905058: Figure out if unordered axes should be allowed.
  if (!base::ranges::is_sorted(operation.axes)) {
    return NewNotSupportedError("Axes must be ordered for layerNormalization.");
  }

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpLayerNormalizationTypeName);
  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamX,
                                      operation.input_operand_id));

  // TODO: crbug.com/338529226: These params must all be constant tensors.
  if (operation.scale_operand_id.has_value()) {
    RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamGamma,
                                        operation.scale_operand_id.value()));
  }
  if (operation.bias_operand_id.has_value()) {
    RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamBeta,
                                        operation.bias_operand_id.value()));
  }

  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kOpParamAxes,
        Create1DTensorImmediateValue<int32_t>(Ui32ToI32(operation.axes))},
       {kOpParamEpsilon, CreateFloatValue(input_operand_info.mil_data_type,
                                          operation.epsilon)}});

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForLeakyRelu(
    const mojom::LeakyRelu& operation,
    CoreML::Specification::MILSpec::Block& block) {
  CHECK(context_properties_.data_type_limits.leaky_relu_input.Has(
      MILDataTypeToOperandType(
          GetOperandInfo(operation.input_operand_id).mil_data_type)));

  ASSIGN_OR_RETURN(CoreML::Specification::MILSpec::Operation * op,
                   CreateUnaryOperation(
                       SupportedDataType::kFloats, kOpLeakyReluTypeName,
                       operation.input_operand_id, operation.output_operand_id,
                       block, ops::kLeakyRelu));

  SetInputWithValue(*op->mutable_inputs(), kOpParamAlpha,
                    CreateScalarImmediateValue<float>(operation.alpha));
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilderCoreml::AddOperationForLinear(
    const mojom::Linear& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  CHECK(context_properties_.data_type_limits.linear_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  // WebNN's linear operator (alpha * a + beta) is far simpler than CoreML's
  // "linear" operator (a fully connected layer), so just implement it as
  //   add(mul(alpha, a), beta)

  // Perform: mul(alpha, a)
  ASSIGN_OR_RETURN(uint64_t mul_output,
                   GenerateInternalOperandInfo(input_operand_info.mil_data_type,
                                               input_operand_info.dimensions));
  RETURN_IF_ERROR(AddOperationForElementwiseBinary(
      /*lhs_operand_id=*/operation.input_operand_id,
      /*rhs_operand=*/
      CreateFloatValue(input_operand_info.mil_data_type, operation.alpha),
      /*output_operand_id=*/mul_output, mojom::ElementWiseBinary::Kind::kMul,
      block));

  // Perform: add(mul_output, beta)
  RETURN_IF_ERROR(AddOperationForElementwiseBinary(
      /*lhs_operand_id=*/mul_output,
      /*rhs_operand=*/
      CreateFloatValue(input_operand_info.mil_data_type, operation.beta),
      /*output_operand_id=*/operation.output_operand_id,
      mojom::ElementWiseBinary::Kind::kAdd, block));
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForLstm(
    uint64_t input_operand_id,
    uint64_t weight_operand_id,
    uint64_t recurrent_weight_operand_id,
    uint32_t hidden_size,
    std::optional<uint64_t> bias_operand_id,
    std::optional<uint64_t> recurrent_bias_operand_id,
    std::optional<uint64_t> peephole_weight_operand_id,
    std::optional<uint64_t> initial_hidden_state_operand_id,
    std::optional<uint64_t> initial_cell_state_operand_id,
    bool return_sequence,
    mojom::RecurrentNetworkDirection direction,
    mojom::LstmWeightLayout layout,
    base::span<const mojom::RecurrentNetworkActivation> activations,
    base::span<const uint64_t> output_operand_ids,
    CoreML::Specification::MILSpec::Block& block) {
  if (!constant_operands_->contains(weight_operand_id)) {
    return NewNotSupportedError("lstm argument weight must be constant.");
  }
  if (!constant_operands_->contains(recurrent_weight_operand_id)) {
    return NewNotSupportedError(
        "lstm argument recurrentWeight must be constant.");
  }
  if (bias_operand_id && !constant_operands_->contains(*bias_operand_id)) {
    return NewNotSupportedError("lstm argument bias must be constant.");
  }
  if (recurrent_bias_operand_id &&
      !constant_operands_->contains(*recurrent_bias_operand_id)) {
    return NewNotSupportedError(
        "lstm argument recurrentBias must be constant.");
  }
  if (peephole_weight_operand_id &&
      !constant_operands_->contains(*peephole_weight_operand_id)) {
    return NewNotSupportedError(
        "lstm argument peepholeWeight must be constant.");
  }

  const OperandInfo& input_operand_info = GetOperandInfo(input_operand_id);
  CoreML::Specification::MILSpec::DataType data_type =
      input_operand_info.mil_data_type;
  OperandDataType operand_data_type = MILDataTypeToOperandType(data_type);
  CHECK(context_properties_.data_type_limits.lstm_input.Has(operand_data_type));

  CHECK_EQ(data_type, GetOperandInfo(weight_operand_id).mil_data_type);
  CHECK_EQ(data_type,
           GetOperandInfo(recurrent_weight_operand_id).mil_data_type);
  if (bias_operand_id) {
    CHECK_EQ(data_type, GetOperandInfo(*bias_operand_id).mil_data_type);
  }
  if (recurrent_bias_operand_id) {
    CHECK_EQ(data_type,
             GetOperandInfo(*recurrent_bias_operand_id).mil_data_type);
  }
  if (peephole_weight_operand_id) {
    CHECK_EQ(data_type,
             GetOperandInfo(*peephole_weight_operand_id).mil_data_type);
  }

  static constexpr char kParamActivation[] = "activation";
  static constexpr char kParamBiasBack[] = "bias_back";
  static constexpr char kParamCellActivation[] = "cell_activation";
  static constexpr char kParamDirection[] = "direction";
  static constexpr char kParamInitialHiddenState[] = "initial_h";
  static constexpr char kParamInitialCellState[] = "initial_c";
  static constexpr char kParamInputWeight[] = "weight_ih";
  static constexpr char kParamInputWeightBack[] = "weight_ih_back";
  static constexpr char kParamOutputSequence[] = "output_sequence";
  static constexpr char kParamPeephole[] = "peephole";
  static constexpr char kParamPeepholeBack[] = "peephole_back";
  static constexpr char kParamRecurrentActivation[] = "recurrent_activation";
  static constexpr char kParamRecurrentWeight[] = "weight_hh";
  static constexpr char kParamRecurrentWeightBack[] = "weight_hh_back";

  static constexpr char kForwardDirection[] = "forward";
  static constexpr char kBackwardDirection[] = "reverse";
  static constexpr char kBothDirection[] = "both";

  uint32_t num_of_directions =
      direction == mojom::RecurrentNetworkDirection::kBoth ? 2 : 1;

  CHECK_EQ(input_operand_info.dimensions.size(), 3u);
  uint32_t steps = input_operand_info.dimensions[0];
  uint32_t batch_size = input_operand_info.dimensions[1];
  uint32_t input_size = input_operand_info.dimensions[2];

  // If `initial_hidden_state` or `initial_cell_state` is provided, need to
  // change dimensions: [numDirections, batchSize, hiddenSize] -> [batchSize,
  // numDirections * hiddenSize]. Otherwise create tensors filled with zeros.
  ASSIGN_OR_RETURN(
      uint64_t initial_hidden_state,
      GenerateInternalOperandInfo(
          data_type, base::span<const uint32_t>(
                         {batch_size, hidden_size * num_of_directions})));
  if (initial_hidden_state_operand_id) {
    CHECK_EQ(GetOperandInfo(*initial_hidden_state_operand_id).mil_data_type,
             input_operand_info.mil_data_type);
    ASSIGN_OR_RETURN(
        uint64_t transposed_initial_hidden_state,
        GenerateInternalOperandInfo(
            data_type, base::span<const uint32_t>(
                           {batch_size, num_of_directions, hidden_size})));
    RETURN_IF_ERROR(AddOperationForTranspose(
        *initial_hidden_state_operand_id, transposed_initial_hidden_state,
        base::span<const uint32_t>({1, 0, 2}), block));
    RETURN_IF_ERROR(AddOperationForReshape(transposed_initial_hidden_state,
                                           initial_hidden_state, block));

  } else {
    AddOperationForFill(CreateFloatValue(data_type, 0.0f), initial_hidden_state,
                        block);
  }

  ASSIGN_OR_RETURN(
      uint64_t initial_cell_state,
      GenerateInternalOperandInfo(
          data_type, base::span<const uint32_t>(
                         {batch_size, hidden_size * num_of_directions})));
  if (initial_cell_state_operand_id) {
    CHECK_EQ(GetOperandInfo(*initial_cell_state_operand_id).mil_data_type,
             input_operand_info.mil_data_type);
    ASSIGN_OR_RETURN(
        uint64_t transposed_initial_cell_state,
        GenerateInternalOperandInfo(
            data_type, base::span<const uint32_t>(
                           {batch_size, num_of_directions, hidden_size})));
    RETURN_IF_ERROR(AddOperationForTranspose(
        *initial_cell_state_operand_id, transposed_initial_cell_state,
        base::span<const uint32_t>({1, 0, 2}), block));
    RETURN_IF_ERROR(AddOperationForReshape(transposed_initial_cell_state,
                                           initial_cell_state, block));

  } else {
    AddOperationForFill(CreateFloatValue(data_type, 0.0f), initial_cell_state,
                        block);
  }

  // Need to reorder layout to CoreML expected [input, forget, output, cell] -
  // ifog.
  std::array<size_t, 4> layout_reorder;
  switch (layout) {
    case (mojom::LstmWeightLayout::kIfgo): {
      layout_reorder = {0, 1, 3, 2};
      break;
    }
    case (mojom::LstmWeightLayout::kIofg): {
      layout_reorder = {0, 2, 1, 3};
      break;
    }
  }

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpLstmTypeName);
  RETURN_IF_ERROR(
      SetInputFromOperand(*op->mutable_inputs(), kOpParamX, input_operand_id));
  RETURN_IF_ERROR(SetInputFromOperand(
      *op->mutable_inputs(), kParamInitialHiddenState, initial_hidden_state));
  RETURN_IF_ERROR(SetInputFromOperand(
      *op->mutable_inputs(), kParamInitialCellState, initial_cell_state));
  std::string_view direction_param_value;
  switch (direction) {
    case mojom::RecurrentNetworkDirection::kForward:
      direction_param_value = kForwardDirection;
      break;
    case mojom::RecurrentNetworkDirection::kBackward:
      direction_param_value = kBackwardDirection;
      break;
    case mojom::RecurrentNetworkDirection::kBoth:
      direction_param_value = kBothDirection;
      break;
  }

  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kParamRecurrentActivation,
        CreateStringImmediateValue(GetActivationParam(activations[0]))},
       {kParamCellActivation,
        CreateStringImmediateValue(GetActivationParam(activations[1]))},
       {kParamActivation,
        CreateStringImmediateValue(GetActivationParam(activations[2]))},
       {kParamDirection, CreateStringImmediateValue(direction_param_value)},
       {kParamOutputSequence, CreateScalarImmediateValue(return_sequence)}});

  size_t item_byte_size = weights_file_handle_->GetByteSize(operand_data_type);

  base::FixedArray<uint32_t> weight_dimension{4 * hidden_size, input_size};
  base::span<const uint8_t> weight =
      constant_operands_->at(weight_operand_id)->ByteSpan();

  // Based on layout reorder, calculate [(offset, size), ..] to extract
  // subspans to write.
  base::FixedArray<std::pair<size_t, size_t>> weight_new_order(
      layout_reorder.size());
  uint32_t size_per_slice = hidden_size * input_size;
  for (size_t i = 0; i < layout_reorder.size(); i++) {
    weight_new_order[i] = {layout_reorder[i] * size_per_slice, size_per_slice};
  }

  // If it's bidirectional, need to write two weights. Same goes for
  // recurrent_weight, bias, peephole_weight.
  uint32_t weight_size_per_direction =
      4 * hidden_size * input_size * item_byte_size;
  RETURN_IF_ERROR(SetInputFromConstantReordered(
      *op->mutable_inputs(), kParamInputWeight,
      weight.first(weight_size_per_direction), operand_data_type,
      weight_dimension, weight_new_order));
  if (direction == mojom::RecurrentNetworkDirection::kBoth) {
    RETURN_IF_ERROR(SetInputFromConstantReordered(
        *op->mutable_inputs(), kParamInputWeightBack,
        weight.subspan(weight_size_per_direction, weight_size_per_direction),
        operand_data_type, weight_dimension, weight_new_order));
  }

  base::span<const uint8_t> recurrent_weight =
      constant_operands_->at(recurrent_weight_operand_id)->ByteSpan();
  base::FixedArray<uint32_t> recurrent_weight_dimension{4 * hidden_size,
                                                        hidden_size};

  base::FixedArray<std::pair<size_t, size_t>> recurrent_weight_new_order(
      layout_reorder.size());
  uint32_t recurrent_size_per_slice = hidden_size * hidden_size;
  for (size_t i = 0; i < layout_reorder.size(); i++) {
    recurrent_weight_new_order[i] = {
        layout_reorder[i] * recurrent_size_per_slice, recurrent_size_per_slice};
  }
  uint32_t recurrent_weight_size_per_direction =
      4 * hidden_size * hidden_size * item_byte_size;
  RETURN_IF_ERROR(SetInputFromConstantReordered(
      *op->mutable_inputs(), kParamRecurrentWeight,
      recurrent_weight.first(recurrent_weight_size_per_direction),
      operand_data_type, recurrent_weight_dimension,
      recurrent_weight_new_order));
  if (direction == mojom::RecurrentNetworkDirection::kBoth) {
    RETURN_IF_ERROR(SetInputFromConstantReordered(
        *op->mutable_inputs(), kParamRecurrentWeightBack,
        recurrent_weight.subspan(recurrent_weight_size_per_direction,
                                 recurrent_weight_size_per_direction),
        operand_data_type, recurrent_weight_dimension,
        recurrent_weight_new_order));
  }

  if (peephole_weight_operand_id) {
    base::span<const uint8_t> peephole_weight =
        constant_operands_->at(*peephole_weight_operand_id)->ByteSpan();
    base::FixedArray<uint32_t> peephole_weight_dimension{3 * hidden_size};
    // WebNN peephole weight layout is [input, output, forget], CoreML takes
    // [input, forget, output]
    base::FixedArray<size_t> peephole_layout_reorder{0, 2, 1};
    base::FixedArray<std::pair<size_t, size_t>> peephole_new_order(
        peephole_layout_reorder.size());
    for (size_t i = 0; i < peephole_new_order.size(); i++) {
      peephole_new_order[i] = {peephole_layout_reorder[i] * hidden_size,
                               hidden_size};
    }
    size_t peephole_weight_size_per_direction =
        3 * hidden_size * item_byte_size;
    RETURN_IF_ERROR(SetInputFromConstantReordered(
        *op->mutable_inputs(), kParamPeephole,
        peephole_weight.first(peephole_weight_size_per_direction),
        operand_data_type, peephole_weight_dimension, peephole_new_order));
    if (direction == mojom::RecurrentNetworkDirection::kBoth) {
      RETURN_IF_ERROR(SetInputFromConstantReordered(
          *op->mutable_inputs(), kParamPeepholeBack,
          peephole_weight.subspan(peephole_weight_size_per_direction,
                                  peephole_weight_size_per_direction),
          operand_data_type, peephole_weight_dimension, peephole_new_order));
    }
  }

  base::FixedArray<uint32_t> bias_dimensions{4 * hidden_size};
  base::FixedArray<std::pair<size_t, size_t>> bias_new_order(
      layout_reorder.size());
  for (size_t i = 0; i < layout_reorder.size(); i++) {
    bias_new_order[i] = {layout_reorder[i] * hidden_size, hidden_size};
  }
  size_t bias_size_per_direction = 4 * hidden_size * item_byte_size;
  // CoreML's 'bias' param is the combination of bias and recurrent_bias.
  if (bias_operand_id && recurrent_bias_operand_id) {
    base::span<const uint8_t> bias =
        constant_operands_->at(*bias_operand_id)->ByteSpan();
    base::span<const uint8_t> recurrent_bias =
        constant_operands_->at(*recurrent_bias_operand_id)->ByteSpan();

    RETURN_IF_ERROR(SetInputFromTwoConstantsReordered(
        *op->mutable_inputs(), kOpParamBias,
        bias.first(bias_size_per_direction),
        recurrent_bias.first(bias_size_per_direction), operand_data_type,
        bias_dimensions, bias_new_order));
    if (direction == mojom::RecurrentNetworkDirection::kBoth) {
      RETURN_IF_ERROR(SetInputFromTwoConstantsReordered(
          *op->mutable_inputs(), kParamBiasBack,
          bias.subspan(bias_size_per_direction, bias_size_per_direction),
          recurrent_bias.subspan(bias_size_per_direction,
                                 bias_size_per_direction),
          operand_data_type, bias_dimensions, bias_new_order));
    }
  } else if (bias_operand_id || recurrent_bias_operand_id) {
    uint64_t coreml_bias_param =
        bias_operand_id.value_or(*recurrent_bias_operand_id);
    base::span<const uint8_t> bias =
        constant_operands_->at(coreml_bias_param)->ByteSpan();
    RETURN_IF_ERROR(SetInputFromConstantReordered(
        *op->mutable_inputs(), kOpParamBias,
        bias.first(bias_size_per_direction), operand_data_type, bias_dimensions,
        bias_new_order));

    if (direction == mojom::RecurrentNetworkDirection::kBoth) {
      RETURN_IF_ERROR(SetInputFromConstantReordered(
          *op->mutable_inputs(), kParamBiasBack,
          bias.subspan(bias_size_per_direction, bias_size_per_direction),
          operand_data_type, bias_dimensions, bias_new_order));
    }
  }

  if (return_sequence) {
    // If return sequence, the first output of the CoreML lstm is the
    // outputs of every step [steps, batchSize, numDirections * hiddenSize] that
    // need to be reshaped to [steps, numDirections, batchSize, hiddenSize].
    CHECK_EQ(output_operand_ids.size(), 3u);
    ASSIGN_OR_RETURN(uint64_t coreml_first_output_id,
                     GenerateInternalOperandInfo(
                         data_type, base::span<const uint32_t>(
                                        {steps, batch_size,
                                         num_of_directions * hidden_size})));
    PopulateNamedValueType(coreml_first_output_id, *op->add_outputs());
    ASSIGN_OR_RETURN(uint64_t coreml_first_output_id_reshaped,
                     GenerateInternalOperandInfo(
                         data_type, base::span<const uint32_t>(
                                        {steps, batch_size, num_of_directions,
                                         hidden_size})));
    RETURN_IF_ERROR(AddOperationForReshape(
        coreml_first_output_id, coreml_first_output_id_reshaped, block));

    // [steps, batchSize, numDirections, hiddenSize] -> [steps, numDirections,
    // batchSize, hiddenSize]
    RETURN_IF_ERROR(AddOperationForTranspose(
        coreml_first_output_id_reshaped, output_operand_ids[2],
        base::span<const uint32_t>({0, 2, 1, 3}), block));
  } else {
    // Else, the first output of CoreML lstm is the output of the last step with
    // shape [1, batchSize, hiddenSize].
    ASSIGN_OR_RETURN(uint64_t unused_second_output,
                     GenerateInternalOperandInfo(
                         data_type, base::span<const uint32_t>(
                                        {1, batch_size, hidden_size})));
    PopulateNamedValueType(unused_second_output, *op->add_outputs());
  }

  // The second and third CoreML outputs are last step hidden state and cell
  // state. Both need to reshape & transpose from [batchSize, numDirection *
  // hiddenSize] -> [numDirections, batchSize, hiddenSize]
  CHECK_GE(output_operand_ids.size(), 2u);
  for (size_t i = 0; i < 2u; i++) {
    ASSIGN_OR_RETURN(
        uint64_t output_id,
        GenerateInternalOperandInfo(
            data_type, base::span<const uint32_t>(
                           {batch_size, num_of_directions * hidden_size})));
    PopulateNamedValueType(output_id, *op->add_outputs());
    ASSIGN_OR_RETURN(
        uint64_t output_id_reshaped,
        GenerateInternalOperandInfo(
            data_type, base::span<const uint32_t>(
                           {batch_size, num_of_directions, hidden_size})));
    RETURN_IF_ERROR(
        AddOperationForReshape(output_id, output_id_reshaped, block));

    // [batchSize, numDirections, hiddenSize] -> [numDirections, batchSize,
    // hiddenSize]
    RETURN_IF_ERROR(
        AddOperationForTranspose(output_id_reshaped, output_operand_ids[i],
                                 base::span<const uint32_t>({1, 0, 2}), block));
  }
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForLstm(
    const mojom::Lstm& operation,
    CoreML::Specification::MILSpec::Block& block) {
  return AddOperationForLstm(
      operation.input_operand_id, operation.weight_operand_id,
      operation.recurrent_weight_operand_id, operation.hidden_size,
      operation.bias_operand_id, operation.recurrent_bias_operand_id,
      operation.peephole_weight_operand_id,
      operation.initial_hidden_state_operand_id,
      operation.initial_cell_state_operand_id, operation.return_sequence,
      operation.direction, operation.layout, operation.activations,
      operation.output_operand_ids, block);
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForLstmCell(
    const mojom::LstmCell& operation,
    CoreML::Specification::MILSpec::Block& block) {
  // CoreML only has 'lstm' operation. So treat it as a single step
  // lstm.
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  CHECK_EQ(input_operand_info.dimensions.size(), 2u);
  uint32_t batch_size = input_operand_info.dimensions[0];
  ASSIGN_OR_RETURN(uint64_t reshaped_input,
                   GenerateInternalOperandInfo(
                       input_operand_info.mil_data_type,
                       base::span<const uint32_t>(
                           {/*steps=*/1, input_operand_info.dimensions[0],
                            input_operand_info.dimensions[1]})));
  RETURN_IF_ERROR(AddOperationForReshape(operation.input_operand_id,
                                         reshaped_input, block));

  // hidden_state, cell_state, output_hidden_state, output_cell_state all need
  // to add a numOfDirections dimension.
  std::array<uint64_t, 4> reshaped_operands;
  for (auto& reshaped_operand : reshaped_operands) {
    ASSIGN_OR_RETURN(
        reshaped_operand,
        GenerateInternalOperandInfo(
            input_operand_info.mil_data_type,
            base::span<const uint32_t>(
                {/*numOfDirections=*/1, batch_size, operation.hidden_size})));
  }
  uint64_t hidden_state_operand_id = reshaped_operands[0];
  uint64_t cell_state_operand_id = reshaped_operands[1];
  uint64_t output_hidden_state = reshaped_operands[2];
  uint64_t output_cell_state = reshaped_operands[3];
  RETURN_IF_ERROR(AddOperationForReshape(operation.hidden_state_operand_id,
                                         hidden_state_operand_id, block));
  RETURN_IF_ERROR(AddOperationForReshape(operation.cell_state_operand_id,
                                         cell_state_operand_id, block));

  RETURN_IF_ERROR(AddOperationForLstm(
      reshaped_input, operation.weight_operand_id,
      operation.recurrent_weight_operand_id, operation.hidden_size,
      operation.bias_operand_id, operation.recurrent_bias_operand_id,
      operation.peephole_weight_operand_id, hidden_state_operand_id,
      cell_state_operand_id,
      /*return_sequence=*/false, mojom::RecurrentNetworkDirection::kForward,
      operation.layout, operation.activations,
      base::span<const uint64_t>({output_hidden_state, output_cell_state}),
      block));
  CHECK_EQ(operation.output_operand_ids.size(), 2u);
  RETURN_IF_ERROR(AddOperationForReshape(
      output_hidden_state, operation.output_operand_ids[0], block));
  RETURN_IF_ERROR(AddOperationForReshape(
      output_cell_state, operation.output_operand_ids[1], block));
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForMatmul(
    uint64_t input_x_operand_id,
    uint64_t input_y_operand_id,
    bool transpose_x,
    bool transpose_y,
    uint64_t output_operand_id,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info = GetOperandInfo(input_x_operand_id);

  CHECK(context_properties_.data_type_limits.matmul_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpMatmulTypeName);
  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamX,
                                      input_x_operand_id));

  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamY,
                                      input_y_operand_id));

  static constexpr char kParamTransposeX[] = "transpose_x";
  static constexpr char kParamTransposeY[] = "transpose_y";
  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kParamTransposeX, CreateScalarImmediateValue(transpose_x)},
       {kParamTransposeY, CreateScalarImmediateValue(transpose_y)}});

  PopulateNamedValueType(output_operand_id, *op->add_outputs());
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForMatmul(
    const mojom::Matmul& operation,
    CoreML::Specification::MILSpec::Block& block) {
  return AddOperationForMatmul(
      operation.a_operand_id, operation.b_operand_id, /*transpose_x=*/false,
      /*transpose_y=*/false, operation.output_operand_id, block);
}

base::expected<void, mojom::ErrorPtr> GraphBuilderCoreml::AddOperationForPad(
    const mojom::Pad& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);

  CHECK(context_properties_.data_type_limits.pad_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpPadTypeName);

  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamX,
                                      operation.input_operand_id));

  base::FixedArray<int32_t> paddings(operation.beginning_padding.size() +
                                     operation.ending_padding.size());
  CHECK_EQ(operation.beginning_padding.size(), operation.ending_padding.size());
  for (size_t i = 0; i < operation.beginning_padding.size(); ++i) {
    paddings[i * 2] = operation.beginning_padding[i];
    paddings[i * 2 + 1] = operation.ending_padding[i];
  }

  constexpr char kParamConstantVal[] = "constant_val";

  std::string_view mode;
  float constant = 0;
  switch (operation.mode->which()) {
    case mojom::PaddingMode::Tag::kConstant:
      mode = "constant";
      constant = operation.mode->get_constant()->value;
      break;
    case mojom::PaddingMode::Tag::kSymmetric:
      // TODO: crbug.com/354101904 - figure out out how to emulate this or
      // resolve the incompabitility at spec level.
      return NewNotSupportedError("Unsupported mode symmetric for pad.");
    case mojom::PaddingMode::Tag::kEdge:
      mode = "replicate";
      break;
    case mojom::PaddingMode::Tag::kReflection:
      mode = "reflect";
      break;
  }

  // TODO: crbug.com/354101905 - CoreML only supports padding the last two
  // dimensions. Figure out out how to emulate > 2D padding or resolve the
  // incompabitility at spec level.
  if (!operation.mode->is_constant() &&
      operation.beginning_padding.size() > 2) {
    bool beginning_paddings_zeros = true;
    for (size_t i = 0; i < operation.beginning_padding.size() - 2; i++) {
      if (operation.beginning_padding[i] != 0 ||
          operation.ending_padding[i] != 0) {
        beginning_paddings_zeros = false;
        break;
      }
    }
    if (!beginning_paddings_zeros) {
      return NewNotSupportedError(
          "Unsupported padding for pad, padding for more than two dimensions "
          "only supports 'constant' mode.");
    }
  }

  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kOpParamPad, Create1DTensorImmediateValue<int32_t>(paddings)},
       {kOpParamMode, CreateStringImmediateValue(mode)},
       {kParamConstantVal,
        CreateFloatValue(input_operand_info.mil_data_type, constant)}});
  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilderCoreml::AddOperationForPool2d(
    const mojom::Pool2d& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);

  switch (operation.kind) {
    case mojom::Pool2d::Kind::kAveragePool2d:
      CHECK(context_properties_.data_type_limits.average_pool2d_input.Has(
          MILDataTypeToOperandType(input_operand_info.mil_data_type)));
      break;
    case mojom::Pool2d::Kind::kL2Pool2d:
      CHECK(context_properties_.data_type_limits.l2_pool2d_input.Has(
          MILDataTypeToOperandType(input_operand_info.mil_data_type)));
      break;
    case mojom::Pool2d::Kind::kMaxPool2d:
      CHECK(context_properties_.data_type_limits.max_pool2d_input.Has(
          MILDataTypeToOperandType(input_operand_info.mil_data_type)));
      break;
  }

  if (operation.dilations->height != 1 || operation.dilations->width != 1) {
    // TODO: crbug.com/334914466 - Support dilations.
    return NewNotSupportedError("Unsupported dilations.");
  }

  static constexpr char kParamKernelSizes[] = "kernel_sizes";
  static constexpr char kParamStrides[] = "strides";
  static constexpr char kParamPadType[] = "pad_type";
  static constexpr char kParamPadTypeValue[] = "custom";
  static constexpr char kParamExcludePaddingFromAverage[] =
      "exclude_padding_from_average";
  static constexpr char kParamCeilMode[] = "ceil_mode";

  // CoreML supports 1D, 2D, and 3D pooling, but WebNN only supports 2D.
  static constexpr size_t kSpatialDimensions = 2u;

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  switch (operation.kind) {
    case mojom::Pool2d::Kind::kAveragePool2d:
      op->set_type(kOpAvgPoolTypeName);

      // The padding elements are not counted as part of the averaging
      // calculation.
      SetInputWithValue(*op->mutable_inputs(), kParamExcludePaddingFromAverage,
                        CreateScalarImmediateValue(true));
      break;
    case mojom::Pool2d::Kind::kL2Pool2d:
      op->set_type(kOpL2PoolTypeName);
      break;
    case mojom::Pool2d::Kind::kMaxPool2d:
      op->set_type(kOpMaxPoolTypeName);
      break;
  }

  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamX,
                                      operation.input_operand_id));

  const std::array<const int32_t, kSpatialDimensions> kernel_sizes = {
      base::checked_cast<int32_t>(operation.window_dimensions->height),
      base::checked_cast<int32_t>(operation.window_dimensions->width),
  };
  const std::array<const int32_t, kSpatialDimensions> strides = {
      base::checked_cast<int32_t>(operation.strides->height),
      base::checked_cast<int32_t>(operation.strides->width),
  };
  const std::array<const int32_t, 4> pad = {
      base::checked_cast<int32_t>(operation.padding->beginning->height),
      base::checked_cast<int32_t>(operation.padding->ending->height),
      base::checked_cast<int32_t>(operation.padding->beginning->width),
      base::checked_cast<int32_t>(operation.padding->ending->width)};

  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kParamKernelSizes, Create1DTensorImmediateValue<int32_t>(kernel_sizes)},
       {kParamStrides, Create1DTensorImmediateValue<int32_t>(strides)},
       {kParamPadType, CreateStringImmediateValue(kParamPadTypeValue)},
       {kOpParamPad, Create1DTensorImmediateValue<int32_t>(pad)},
       // TODO: crbug.com/334914466 - Support `ceil_mode` by calculating the
       // expected output shape and comparing it to the shape of the output
       // operand. Note that Core ML requires padding to be symmetric if
       // `ceil_mode` is true.
       {kParamCeilMode, CreateScalarImmediateValue(false)}});

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilderCoreml::AddOperationForReduce(
    const mojom::Reduce& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  // Special handling for 0D reduction or empty axes, neither is supported by
  // CoreML reduction. When input is 0D or when `axes` is empty, values are not
  // reduced, but reduction function is applied to individual input values.
  if (input_operand_info.dimensions.empty() || operation.axes.empty()) {
    switch (operation.kind) {
      case mojom::Reduce::Kind::kL1:
      case mojom::Reduce::Kind::kL2:
      case mojom::Reduce::Kind::kLogSumExp:
      case mojom::Reduce::Kind::kMax:
      case mojom::Reduce::Kind::kMean:
      case mojom::Reduce::Kind::kMin:
      case mojom::Reduce::Kind::kProduct:
      case mojom::Reduce::Kind::kSum:
        // Applying each of these reductions to a scalar value is a no-op.
        // TODO: crbug.com/356190937 - Further optimize away the identity node.
        return AddUnaryOperation(
            SupportedDataType::kFloatsAndInt32, kOpIdentityTypeName,
            operation.input_operand_id, operation.output_operand_id, block,
            ops::kIdentity);
      case mojom::Reduce::Kind::kLogSum:
        return AddOperationForElementwiseUnary(
            mojom::ElementWiseUnary::Kind::kLog, operation.input_operand_id,
            operation.output_operand_id, block);
      case mojom::Reduce::Kind::kSumSquare:
        return AddOperationForElementwiseBinary(
            operation.input_operand_id, operation.input_operand_id,
            operation.output_operand_id, mojom::ElementWiseBinary::Kind::kMul,
            block);
    }
  }
  CoreML::Specification::MILSpec::Operation* op = block.add_operations();

  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamX,
                                      operation.input_operand_id));

  const DataTypeLimits& data_type_limits = context_properties_.data_type_limits;
  const OperandDataType input_data_type =
      MILDataTypeToOperandType(input_operand_info.mil_data_type);

  switch (operation.kind) {
    case mojom::Reduce::Kind::kL1:
      CHECK(data_type_limits.reduce_l1_input.Has(input_data_type));
      op->set_type(kOpReduceL1);
      break;
    case mojom::Reduce::Kind::kL2:
      CHECK(data_type_limits.reduce_l2_input.Has(input_data_type));
      op->set_type(kOpReduceL2);
      break;
    case mojom::Reduce::Kind::kLogSum:
      CHECK(data_type_limits.reduce_log_sum_input.Has(input_data_type));
      op->set_type(kOpReduceLogSum);
      break;
    case mojom::Reduce::Kind::kLogSumExp:
      CHECK(data_type_limits.reduce_log_sum_exp_input.Has(input_data_type));
      op->set_type(kOpReduceLogSumExp);
      break;
    case mojom::Reduce::Kind::kMax:
      CHECK(data_type_limits.reduce_max_input.Has(input_data_type));
      op->set_type(kOpReduceMax);
      break;
    case mojom::Reduce::Kind::kMean:
      CHECK(data_type_limits.reduce_mean_input.Has(input_data_type));
      op->set_type(kOpReduceMean);
      break;
    case mojom::Reduce::Kind::kMin:
      CHECK(data_type_limits.reduce_min_input.Has(input_data_type));
      op->set_type(kOpReduceMin);
      break;
    case mojom::Reduce::Kind::kProduct:
      CHECK(data_type_limits.reduce_product_input.Has(input_data_type));
      op->set_type(kOpReduceProduct);
      break;
    case mojom::Reduce::Kind::kSum:
      CHECK(data_type_limits.reduce_sum_input.Has(input_data_type));
      op->set_type(kOpReduceSum);
      break;
    case mojom::Reduce::Kind::kSumSquare:
      CHECK(data_type_limits.reduce_sum_square_input.Has(input_data_type));
      op->set_type(kOpReduceSumSquare);
      break;
  }

  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kOpParamAxes,
        Create1DTensorImmediateValue<int32_t>(Ui32ToI32(operation.axes))},
       {kOpParamKeepDims,
        CreateScalarImmediateValue(operation.keep_dimensions)}});

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForResample2d(
    const mojom::Resample2d& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);

  // WebNN's "resample2d" maps to variants of the "upsample" operator in CoreML:
  // https://apple.github.io/coremltools/source/coremltools.converters.mil.mil.ops.defs.html#coremltools.converters.mil.mil.ops.defs.iOS15.image_resizing.upsample_bilinear
  // https://apple.github.io/coremltools/source/coremltools.converters.mil.mil.ops.defs.html#coremltools.converters.mil.mil.ops.defs.iOS15.image_resizing.upsample_nearest_neighbor
  CHECK(context_properties_.data_type_limits.resample2d_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  const std::array<size_t, 2> supported_axes = {2, 3};
  CHECK(base::ranges::equal(operation.axes, supported_axes));

  static constexpr char kParamScaleFactorHeight[] = "scale_factor_height";
  static constexpr char kParamScaleFactorWidth[] = "scale_factor_width";
  static constexpr char kParamAlignCorners[] = "align_corners";

  CoreML::Specification::MILSpec::Operation& op = *block.add_operations();
  switch (operation.mode) {
    case mojom::Resample2d::InterpolationMode::kLinear:
      op.set_type(kOpUpsampleBilinearTypeName);

      // TODO: crbug.com/334914468 - Follow along with
      // https://github.com/webmachinelearning/webnn/issues/270.
      SetInputWithValue(*op.mutable_inputs(), kParamAlignCorners,
                        CreateScalarImmediateValue(false));
      break;
    case mojom::Resample2d::InterpolationMode::kNearestNeighbor:
      op.set_type(kOpUpsampleNearestNeighborTypeName);
      break;
  }

  RETURN_IF_ERROR(SetInputFromOperand(*op.mutable_inputs(), kOpParamX,
                                      operation.input_operand_id));

  // Use explicit scales if given, otherwise, compute scales from output
  // dimensions / input dimensions.
  //
  // TODO: crbug.com/334914468 - Move this logic to the renderer such that
  // `operation.scales` cannot be optional.
  //
  // TODO: crbug.com/334914468 - Consider utilizing CoreML's support for int32
  // scales.
  std::array<float, 2> scales;
  if (operation.scales) {
    scales = {operation.scales->at(0), operation.scales->at(1)};
  } else {
    const OperandInfo& output_operand_info =
        GetOperandInfo(operation.output_operand_id);
    for (size_t i = 0; i < supported_axes.size(); ++i) {
      scales[i] = base::checked_cast<float>(
                      output_operand_info.dimensions[supported_axes[i]]) /
                  input_operand_info.dimensions[supported_axes[i]];
    }
  }

  SetInputsWithValues(
      *op.mutable_inputs(),
      {{kParamScaleFactorHeight, CreateScalarImmediateValue(scales[0])},
       {kParamScaleFactorWidth, CreateScalarImmediateValue(scales[1])}});

  PopulateNamedValueType(operation.output_operand_id, *op.add_outputs());
  return base::ok();
}

base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForReshape(
    uint64_t input_operand_id,
    uint64_t output_operand_id,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info = GetOperandInfo(input_operand_id);

  CHECK(context_properties_.data_type_limits.reshape_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  const OperandInfo& output_operand_info = GetOperandInfo(output_operand_id);
  if (output_operand_info.dimensions.size() > 5) {
    return NewNotSupportedError(
        "Unsupported rank for reshape. It should be between 0 to 5.");
  }

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpReshapeTypeName);
  RETURN_IF_ERROR(
      SetInputFromOperand(*op->mutable_inputs(), kOpParamX, input_operand_id));

  SetInputWithValue(*op->mutable_inputs(), kOpParamShape,
                    Create1DTensorImmediateValue<int32_t>(
                        Ui32ToI32(output_operand_info.dimensions)));

  CoreML::Specification::MILSpec::NamedValueType& output = *op->add_outputs();
  PopulateNamedValueType(output_operand_id, output);
  return base::ok();
}

base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForReshape(
    const mojom::Reshape& operation,
    CoreML::Specification::MILSpec::Block& block) {
  return AddOperationForReshape(operation.input_operand_id,
                                operation.output_operand_id, block);
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForScatterElements(
    const mojom::ScatterElements& operation,
    CoreML::Specification::MILSpec::Block& block) {
  CHECK(context_properties_.data_type_limits.scatter_elements_input.Has(
      MILDataTypeToOperandType(
          GetOperandInfo(operation.input_operand_id).mil_data_type)));
  CHECK(context_properties_.data_type_limits.scatter_elements_indices.Has(
      MILDataTypeToOperandType(
          GetOperandInfo(operation.indices_operand_id).mil_data_type)));
  CHECK(context_properties_.data_type_limits.scatter_elements_input.Has(
      MILDataTypeToOperandType(
          GetOperandInfo(operation.updates_operand_id).mil_data_type)));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpScatterElementsTypeName);
  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamData,
                                      operation.input_operand_id));

  // TODO(crbug.com/370535834): Handle negative and out-of-bounds indices.
  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamIndices,
                                      operation.indices_operand_id));

  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamUpdates,
                                      operation.updates_operand_id));

  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kOpParamAxis, CreateScalarImmediateValue(
                          base::checked_cast<int32_t>(operation.axis))},
       {kOpParamMode, CreateStringImmediateValue(kOpParamScatterModeValue)},
       {kOpParamValidateIndices, CreateScalarImmediateValue(false)}});

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForScatterND(
    uint64_t input_operand_id,
    uint64_t indices_operand_id,
    uint64_t updates_operand_id,
    uint64_t output_operand_id,
    CoreML::Specification::MILSpec::Block& block) {
  CHECK(context_properties_.data_type_limits.scatter_nd_input.Has(
      MILDataTypeToOperandType(
          GetOperandInfo(input_operand_id).mil_data_type)));
  CHECK(context_properties_.data_type_limits.scatter_nd_indices.Has(
      MILDataTypeToOperandType(
          GetOperandInfo(indices_operand_id).mil_data_type)));
  CHECK(context_properties_.data_type_limits.scatter_nd_input.Has(
      MILDataTypeToOperandType(
          GetOperandInfo(updates_operand_id).mil_data_type)));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpScatterNDTypeName);
  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamData,
                                      input_operand_id));

  // TODO(crbug.com/363544348): Handle negative and out-of-bounds indices.
  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamIndices,
                                      indices_operand_id));

  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamUpdates,
                                      updates_operand_id));

  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kOpParamMode, CreateStringImmediateValue(kOpParamScatterModeValue)},
       {kOpParamValidateIndices, CreateScalarImmediateValue(false)}});

  PopulateNamedValueType(output_operand_id, *op->add_outputs());
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForScatterND(
    const mojom::ScatterND& operation,
    CoreML::Specification::MILSpec::Block& block) {
  return AddOperationForScatterND(
      operation.input_operand_id, operation.indices_operand_id,
      operation.updates_operand_id, operation.output_operand_id, block);
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForSlice(
    uint64_t input_operand_id,
    uint64_t output_operand_id,
    base::span<const int32_t> beginnings,
    base::span<const int32_t> endings,
    base::span<const int32_t> strides,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info = GetOperandInfo(input_operand_id);
  CHECK(context_properties_.data_type_limits.slice_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpSliceTypeName);
  RETURN_IF_ERROR(
      SetInputFromOperand(*op->mutable_inputs(), kOpParamX, input_operand_id));

  static constexpr char kParamBegin[] = "begin";
  static constexpr char kParamEnd[] = "end";
  static constexpr char kParamStride[] = "stride";

  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kParamBegin, Create1DTensorImmediateValue<int32_t>(beginnings)},
       {kParamEnd, Create1DTensorImmediateValue<int32_t>(endings)},
       {kParamStride, Create1DTensorImmediateValue<int32_t>(strides)}});

  PopulateNamedValueType(output_operand_id, *op->add_outputs());
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForSlice(
    const mojom::Slice& operation,
    CoreML::Specification::MILSpec::Block& block) {
  base::FixedArray<int32_t> beginnings(operation.ranges.size());
  base::FixedArray<int32_t> endings(operation.ranges.size());
  base::FixedArray<int32_t> strides(operation.ranges.size());
  for (size_t i = 0; i < operation.ranges.size(); ++i) {
    beginnings[i] = base::checked_cast<int32_t>(operation.ranges[i].start);
    endings[i] = base::checked_cast<int32_t>(operation.ranges[i].start +
                                             operation.ranges[i].size);
    strides[i] = base::checked_cast<int32_t>(operation.ranges[i].stride);
  }

  return AddOperationForSlice(operation.input_operand_id,
                              operation.output_operand_id, beginnings, endings,
                              strides, block);
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForSoftmax(
    const mojom::Softmax& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  CHECK(context_properties_.data_type_limits.softmax_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpSoftmaxTypeName);

  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamX,
                                      operation.input_operand_id));

  SetInputWithValue(
      *op->mutable_inputs(), kOpParamAxis,
      CreateScalarImmediateValue(base::checked_cast<int32_t>(operation.axis)));
  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForSplit(
    uint64_t input_operand_id,
    base::span<const uint64_t> output_operand_ids,
    uint32_t axis,
    CoreML::Specification::MILSpec::Block& block) {
  if (output_operand_ids.size() == 1) {
    return AddUnaryOperation(kOpIdentityTypeName, input_operand_id,
                             output_operand_ids[0], block);
  }
  const OperandInfo& input_operand_info = GetOperandInfo(input_operand_id);
  CHECK(context_properties_.data_type_limits.split_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpSplitTypeName);
  RETURN_IF_ERROR(
      SetInputFromOperand(*op->mutable_inputs(), kOpParamX, input_operand_id));

  base::FixedArray<int32_t> split_sizes(output_operand_ids.size());
  for (size_t i = 0; i < output_operand_ids.size(); ++i) {
    const uint64_t output_operand_id = output_operand_ids[i];
    PopulateNamedValueType(output_operand_id, *op->add_outputs());
    const OperandInfo& output_operand_info = GetOperandInfo(output_operand_id);
    CHECK_LT(axis, output_operand_info.dimensions.size());
    split_sizes[i] = output_operand_info.dimensions[axis];
  }
  static constexpr char kParamSplitSizes[] = "split_sizes";
  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kParamSplitSizes, Create1DTensorImmediateValue<int32_t>(split_sizes)},
       {kOpParamAxis,
        CreateScalarImmediateValue(base::checked_cast<int32_t>(axis))}});
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForSplit(
    const mojom::Split& operation,
    CoreML::Specification::MILSpec::Block& block) {
  return AddOperationForSplit(operation.input_operand_id,
                              operation.output_operand_ids, operation.axis,
                              block);
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForTile(
    const mojom::Tile& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  CHECK(context_properties_.data_type_limits.tile_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpTileTypeName);
  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamX,
                                      operation.input_operand_id));

  // CoreML expects repetitions to be vector of int32_t.
  SetInputWithValue(
      *op->mutable_inputs(), kOpParamReps,
      Create1DTensorImmediateValue<int32_t>(Ui32ToI32(operation.repetitions)));

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForTranspose(
    uint64_t input_operand_id,
    uint64_t output_operand_id,
    base::span<const uint32_t> permutation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info = GetOperandInfo(input_operand_id);

  CHECK(context_properties_.data_type_limits.transpose_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  if (input_operand_info.dimensions.empty()) {
    return AddUnaryOperation(kOpIdentityTypeName, input_operand_id,
                             output_operand_id, block);
  }

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpTransposeTypeName);
  RETURN_IF_ERROR(
      SetInputFromOperand(*op->mutable_inputs(), kOpParamX, input_operand_id));

  // CoreML expects permutation to be vector of int32_t.
  static constexpr char kParamPerm[] = "perm";
  SetInputWithValue(
      *op->mutable_inputs(), kParamPerm,
      Create1DTensorImmediateValue<int32_t>(Ui32ToI32(permutation)));

  CoreML::Specification::MILSpec::NamedValueType& output = *op->add_outputs();
  PopulateNamedValueType(output_operand_id, output);
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForTranspose(
    const mojom::Transpose& operation,
    CoreML::Specification::MILSpec::Block& block) {
  return AddOperationForTranspose(operation.input_operand_id,
                                  operation.output_operand_id,
                                  operation.permutation, block);
}

base::expected<void, mojom::ErrorPtr> GraphBuilderCoreml::AddOperationForWhere(
    const mojom::Where& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& true_operand_info =
      GetOperandInfo(operation.true_value_operand_id);
  const OperandInfo& false_operand_info =
      GetOperandInfo(operation.false_value_operand_id);
  const OperandInfo& condition_operand_info =
      GetOperandInfo(operation.condition_operand_id);
  CHECK(context_properties_.data_type_limits.where_value.Has(
      MILDataTypeToOperandType(true_operand_info.mil_data_type)));
  CHECK(context_properties_.data_type_limits.where_value.Has(
      MILDataTypeToOperandType(false_operand_info.mil_data_type)));
  CHECK(context_properties_.data_type_limits.where_condition.Has(
      MILDataTypeToOperandType(condition_operand_info.mil_data_type)));

  ASSIGN_OR_RETURN(uint64_t bool_condition_operand_id,
                   GenerateInternalOperandInfo(
                       CoreML::Specification::MILSpec::DataType::BOOL,
                       condition_operand_info.dimensions));

  RETURN_IF_ERROR(AddOperationForCast(operation.condition_operand_id,
                                      bool_condition_operand_id, block));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpWhereTypeName);

  constexpr char kParamA[] = "a";
  constexpr char kParamB[] = "b";
  constexpr char kParamCond[] = "cond";
  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kParamA,
                                      operation.true_value_operand_id));
  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kParamB,
                                      operation.false_value_operand_id));
  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kParamCond,
                                      bool_condition_operand_id));

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForTriangular(
    const mojom::Triangular& operation,
    CoreML::Specification::MILSpec::Block& block) {
  CHECK(context_properties_.data_type_limits.triangular_input.Has(
      MILDataTypeToOperandType(
          GetOperandInfo(operation.input_operand_id).mil_data_type)));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpTriangularTypeName);
  RETURN_IF_ERROR(SetInputFromOperand(*op->mutable_inputs(), kOpParamX,
                                      operation.input_operand_id));

  static constexpr char kParamLower[] = "lower";
  static constexpr char kParamUpper[] = "upper";

  // CoreML's "band_part" operator is a poor approximator of WebNN's triangular
  // operator. WebNN's triangular operator may create a triangle:
  //   1. from the main diagonal outwards, (diagonal == 0)
  //   2. from the main diagonal outwards, plus additional diagonals of the
  //      other triangle, (e.g. upper == true && diagonal < 0)
  //   3. excluding the main diagonal (e.g. upper == true && diagonal > 0)
  //
  // Meanwhile, "band_part" starts from the main diagonal and offers to include
  // additional diagonals in either the upper or lower triangles, with -1
  // indicating to keep them all. It is not possible to exclude the main
  // diagonal, however, so case 3 is not possible to achieve with "band_part".
  //
  // TODO(crbug.com/374127244): Support case 3.

  if ((operation.upper && operation.diagonal > 0) ||
      (!operation.upper && operation.diagonal < 0)) {
    return NewNotSupportedError(
        "Unsupported diagonal for triangular. The main diagonal must be kept.");
  }

  // Keep the entire upper or lower triangle.
  int32_t kept_triangle = -1;
  // Keep diagonals of the other triangle if `operation.diagonal` is non-zero.
  int32_t other_triangle = std::abs(operation.diagonal);

  int32_t upper, lower = 0;
  if (operation.upper) {
    upper = kept_triangle;
    lower = other_triangle;
  } else {
    upper = other_triangle;
    lower = kept_triangle;
  }

  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kParamLower, CreateScalarImmediateValue<int32_t>(lower)},
       {kParamUpper, CreateScalarImmediateValue<int32_t>(upper)}});

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

CoreML::Specification::MILSpec::Value CreateConstantImmediateValue(
    base::span<const uint32_t> dimensions,
    OperandDataType data_type,
    base::span<const uint8_t> value) {
  switch (data_type) {
    case OperandDataType::kFloat32: {
      base::FixedArray<float> floats(value.size() / sizeof(float));
      for (size_t i = 0u; i < floats.size(); ++i) {
        floats[i] = base::FloatFromNativeEndian(
            value.subspan(i * sizeof(float)).first<4u>());
      }
      return CreateTensorImmediateValue<float>(dimensions, floats);
    }
    case OperandDataType::kFloat16: {
      base::FixedArray<Float16> float16s(value.size() / sizeof(Float16));
      for (size_t i = 0u; i < float16s.size(); ++i) {
        float16s[i].data = base::U16FromNativeEndian(
            value.subspan(i * sizeof(Float16)).first<2u>());
      }
      return CreateTensorImmediateValue<Float16>(dimensions, float16s);
    }
    case OperandDataType::kInt32: {
      base::FixedArray<int32_t> ints(value.size() / sizeof(int32_t));
      for (size_t i = 0u; i < ints.size(); ++i) {
        ints[i] = base::I32FromNativeEndian(
            value.subspan(i * sizeof(int32_t)).first<4u>());
      }
      return CreateTensorImmediateValue<int32_t>(dimensions, ints);
    }
    case OperandDataType::kInt8: {
      base::FixedArray<int8_t> int8s(value.size() / sizeof(int8_t));
      for (size_t i = 0u; i < int8s.size(); ++i) {
        int8s[i] = base::I8FromNativeEndian(
            value.subspan(i * sizeof(int8_t)).first<1u>());
      }
      return CreateTensorImmediateValue<int8_t>(dimensions, int8s);
    }
    case OperandDataType::kUint8: {
      base::FixedArray<uint8_t> uint8s(value.size() / sizeof(uint8_t));
      for (size_t i = 0u; i < uint8s.size(); ++i) {
        uint8s[i] = base::U8FromNativeEndian(
            value.subspan(i * sizeof(uint8_t)).first<1u>());
      }
      return CreateTensorImmediateValue<uint8_t>(dimensions, uint8s);
    }
    case OperandDataType::kUint32:
    case OperandDataType::kInt64:
    case OperandDataType::kUint64:
    case OperandDataType::kInt4:
    case OperandDataType::kUint4: {
      NOTREACHED() << "Unsupported data type.";
    }
  }
}
void GraphBuilderCoreml::AddConstantImmediateValue(
    uint64_t constant_id,
    CoreML::Specification::MILSpec::Block& block) {
  auto* op = block.add_operations();
  op->set_type(kOpConstTypeName);
  PopulateNamedValueType(constant_id, *op->add_outputs());

  google::protobuf::Map<std::string, ::CoreML::Specification::MILSpec::Value>&
      attributes = *op->mutable_attributes();
  std::string_view name = GetOperandInfo(constant_id).coreml_name;
  attributes["name"] = CreateStringImmediateValue(name);

  constant_pointers_[constant_id] = name;

  const auto& constant_operand = constant_operands_->at(constant_id);
  attributes["val"] = CreateConstantImmediateValue(
      constant_operand->descriptor().shape(),
      constant_operand->descriptor().data_type(), constant_operand->ByteSpan());
}

CoreML::Specification::MILSpec::Value
GraphBuilderCoreml::CreateConstantFileValue(uint64_t constant_id,
                                            uint64_t offset) {
  CoreML::Specification::MILSpec::Value blob_value{};
  const OperandInfo& operand_info = GetOperandInfo(constant_id);
  PopulateValueTypeFromOperandInfo(operand_info, *blob_value.mutable_type());
  CoreML::Specification::MILSpec::Value::BlobFileValue* blob =
      blob_value.mutable_blobfilevalue();
  blob->set_filename(kWeightsRelativeFilePath);
  blob->set_offset(offset);
  return blob_value;
}

const mojom::Operand& GraphBuilderCoreml::GetOperand(
    uint64_t operand_id) const {
  return *graph_info_->id_to_operand_map.at(operand_id);
}

[[nodiscard]] const GraphBuilderCoreml::OperandInfo&
GraphBuilderCoreml::GetOperandInfo(uint64_t operand_id) const {
  return result_->GetOperandInfo(operand_id);
}

base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::PopulateFeatureDescription(
    uint64_t operand_id,
    ::CoreML::Specification::FeatureDescription& feature_description) {
  const mojom::Operand& operand = GetOperand(operand_id);
  auto* feature_type = feature_description.mutable_type();
  auto* array_feature_type = feature_type->mutable_multiarraytype();
  switch (operand.descriptor.data_type()) {
    case OperandDataType::kFloat32:
      array_feature_type->set_datatype(
          CoreML::Specification::ArrayFeatureType_ArrayDataType::
              ArrayFeatureType_ArrayDataType_FLOAT32);
      break;
    case OperandDataType::kFloat16:
      array_feature_type->set_datatype(
          CoreML::Specification::ArrayFeatureType_ArrayDataType::
              ArrayFeatureType_ArrayDataType_FLOAT16);
      break;
    case OperandDataType::kInt32:
      array_feature_type->set_datatype(
          CoreML::Specification::ArrayFeatureType_ArrayDataType::
              ArrayFeatureType_ArrayDataType_INT32);
      break;
    case OperandDataType::kUint32:
    case OperandDataType::kInt64:
    case OperandDataType::kUint64:
    case OperandDataType::kInt8:
    case OperandDataType::kUint8:
    case OperandDataType::kInt4:
    case OperandDataType::kUint4:
      NOTREACHED() << "Unsupported input data type";
  }
  // FeatureDescriptions are about input and output features, WebNN allows
  // scalar operands to have empty dimensions. At the input and output layers
  // these can be treated as a 1D tensor to satisfy CoreML's requirement of
  // having at least 1 dimension.
  if (operand.descriptor.shape().empty()) {
    array_feature_type->add_shape(1);
  } else {
    for (int dimension : operand.descriptor.shape()) {
      array_feature_type->add_shape(dimension);
    }
  }

  if (operand.descriptor.shape().size() > 5) {
    return NewNotSupportedError(
        "Unsupported rank for input. It should be between 0 to 5.");
  }
  feature_description.mutable_name()->assign(
      GetOperandInfo(operand_id).external_coreml_name);
  return base::ok();
}

base::expected<uint64_t, mojom::ErrorPtr>
GraphBuilderCoreml::GenerateInternalOperandInfo(
    CoreML::Specification::MILSpec::DataType mil_data_type,
    base::span<const uint32_t> dimensions) {
  internal_operand_id_++;
  if (!internal_operand_id_.IsValid()) {
    return NewUnknownError("Number of operands in graph exceeds limit.");
  }
  uint64_t operand_id = internal_operand_id_.ValueOrDie();
  // Prefix is added to internal operands generated for WebNN operations that
  // need to be decomposed into multiple CoreML operations.
  CHECK(id_to_operand_info_map()
            .try_emplace(
                operand_id,
                OperandInfo(base::JoinString({kInternalNamePrefix,
                                              base::NumberToString(operand_id)},
                                             kStringSeparator),
                            dimensions, mil_data_type))
            .second);
  return operand_id;
}

void GraphBuilderCoreml::PopulateNamedValueType(
    uint64_t operand_id,
    CoreML::Specification::MILSpec::NamedValueType& named_value_type) {
  named_value_type.set_name(GetOperandInfo(operand_id).coreml_name);
  auto& value_type = *named_value_type.mutable_type();
  PopulateValueTypeFromOperandInfo(GetOperandInfo(operand_id), value_type);
}

void GraphBuilderCoreml::PopulateNamedValueType(
    std::string_view name,
    CoreML::Specification::MILSpec::DataType mil_data_type,
    base::span<const uint32_t> dimensions,
    CoreML::Specification::MILSpec::NamedValueType& named_value_type) {
  named_value_type.set_name(name.data());
  auto& value_type = *named_value_type.mutable_type();
  PopulateValueType(mil_data_type, dimensions, value_type);
}

void GraphBuilderCoreml::PopulateNamedValueTypeForInput(
    uint64_t operand_id,
    CoreML::Specification::MILSpec::NamedValueType& named_value_type) {
  PopulateNamedValueType(operand_id, named_value_type);

  // WebNN allows 0D scalar operands to have empty dimensions.
  // At the input nodes, these can be treated as a 1D tensor to
  // satisfy CoreML's requirement of having at least 1 dimension.
  if (GetOperand(operand_id).descriptor.Rank() == 0) {
    auto* tensor_type = named_value_type.mutable_type()->mutable_tensortype();
    tensor_type->set_rank(1);
    tensor_type->add_dimensions()->mutable_constant()->set_size(1);
  }
}

void GraphBuilderCoreml::UpdateCoreMLInputInfoMap(uint64_t operand_id) {
  const mojom::Operand& operand = GetOperand(operand_id);
  CHECK(id_to_operand_info_map()
            .try_emplace(operand_id,
                         OperandInfo(GetCoreMLNameFromOperand(operand_id),
                                     operand.descriptor.shape(),
                                     OperandTypeToMILDataType(
                                         operand.descriptor.data_type())))
            .second);
}

std::string GraphBuilderCoreml::GetCoreMLNameFromOperand(uint64_t operand_id) {
  const mojom::Operand& operand = GetOperand(operand_id);
  // CoreML doesn't allow op output names to start with numbers, so "var_"
  // prefixes are added.
  switch (operand.kind) {
    case mojom::Operand::Kind::kInput:
      CHECK(operand.name.has_value());
      return GetCoreMLNameFromInput(operand.name.value(), operand_id);
    case mojom::Operand::Kind::kConstant:
      return base::JoinString(
          {kIntermediateOperandPrefix, base::NumberToString(operand_id)},
          kStringSeparator);
    case mojom::Operand::Kind::kOutput:
      if (operand.name.has_value()) {
        return GetCoreMLNameFromOutput(operand.name.value(), operand_id);
      } else {
        // Intermediate outputs don't have names so use operand_id instead.
        return base::JoinString(
            {kIntermediateOperandPrefix, base::NumberToString(operand_id)},
            kStringSeparator);
      }
  }
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::SetInputFromOperand(
    google::protobuf::Map<std::string,
                          CoreML::Specification::MILSpec::Argument>& inputs,
    std::string_view key,
    uint64_t operand_id) {
  // Non-constant operands should already have an entity in the model.
  if (!constant_operands_->contains(operand_id)) {
    inputs[key].add_arguments()->set_name(
        std::string(GetOperandInfo(operand_id).coreml_name));
    return base::ok();
  }
  auto pointer = constant_pointers_.find(operand_id);
  // Lazily write weights to file if they are not already serialized.
  if (pointer == constant_pointers_.end()) {
    const std::unique_ptr<WebNNConstantOperand>& constant_operand =
        constant_operands_->at(operand_id);
    ASSIGN_OR_RETURN(uint64_t offset,
                     weights_file_handle_->Write(
                         constant_operand->ByteSpan(),
                         constant_operand->descriptor().data_type()));
    constant_pointers_[operand_id] = offset;
    SetInputWithValue(inputs, key, CreateConstantFileValue(operand_id, offset));
  } else {
    std::visit(
        base::Overloaded{
            [&](uint64_t offset) {
              SetInputWithValue(inputs, key,
                                CreateConstantFileValue(operand_id, offset));
            },
            [&](std::string_view coreml_name) {
              inputs[key].add_arguments()->set_name(std::string(coreml_name));
            }},
        pointer->second);
  }
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::SetInputFromConstantReordered(
    google::protobuf::Map<std::string,
                          CoreML::Specification::MILSpec::Argument>& inputs,
    std::string_view key,
    base::span<const uint8_t> bytes,
    OperandDataType data_type,
    base::span<const uint32_t> dimensions,
    base::span<const std::pair<size_t, size_t>> new_order) {
  CHECK(OperandTypeToDataTypeInWeightFile(data_type))
      << "Unsupported weight type for constant folding";

  ASSIGN_OR_RETURN(
      std::unique_ptr<GraphBuilderCoreml::ScopedWeightItem> weight_item,
      weights_file_handle_->CreateScopedWeightItem(data_type, bytes.size()));
  uint64_t offset = weight_item->offset();

  size_t byte_size = weights_file_handle_->GetByteSize(data_type);
  for (auto slice : new_order) {
    RETURN_IF_ERROR(weight_item->WriteBytes(
        bytes.subspan(slice.first * byte_size, slice.second * byte_size)));
  }

  RETURN_IF_ERROR(weight_item->Finalize());

  ASSIGN_OR_RETURN(uint64_t new_constant,
                   GenerateInternalOperandInfo(
                       OperandTypeToMILDataType(data_type), dimensions));

  SetInputWithValue(inputs, key, CreateConstantFileValue(new_constant, offset));

  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::SetInputFromTwoConstantsReordered(
    google::protobuf::Map<std::string,
                          CoreML::Specification::MILSpec::Argument>& inputs,
    std::string_view key,
    base::span<const uint8_t> a_bytes,
    base::span<const uint8_t> b_bytes,
    OperandDataType data_type,
    base::span<const uint32_t> dimensions,
    base::span<const std::pair<size_t, size_t>> new_order) {
  CHECK(OperandTypeToDataTypeInWeightFile(data_type))
      << "Unsupported weight type for constant folding";

  CHECK_EQ(a_bytes.size(), b_bytes.size());
  ASSIGN_OR_RETURN(
      std::unique_ptr<GraphBuilderCoreml::ScopedWeightItem> weight_item,
      weights_file_handle_->CreateScopedWeightItem(data_type, a_bytes.size()));
  uint64_t offset = weight_item->offset();

  size_t byte_size = weights_file_handle_->GetByteSize(data_type);

  for (auto& slice : new_order) {
    base::span<const uint8_t> a_subspan =
        a_bytes.subspan(slice.first * byte_size, slice.second * byte_size);
    base::span<const uint8_t> b_subspan =
        b_bytes.subspan(slice.first * byte_size, slice.second * byte_size);
    size_t subspan_size = slice.second;
    size_t subspan_byte_size = slice.second * byte_size;
    switch (data_type) {
      case OperandDataType::kFloat16: {
        base::FixedArray<Float16> float16s(subspan_size);
        for (size_t i = 0u; i < subspan_size; ++i) {
          // TODO(crbug.com/360052663): add tests for overflow
          base::CheckedNumeric<float> data =
              fp16_ieee_to_fp32_bits(base::U16FromNativeEndian(
                  a_subspan.subspan(i * sizeof(Float16)).first<2u>()));
          data += fp16_ieee_to_fp32_bits(base::U16FromNativeEndian(
              b_subspan.subspan(i * sizeof(Float16)).first<2u>()));
          float16s[i].data = fp16_ieee_from_fp32_value(
              data.ValueOrDefault(std::numeric_limits<float>::infinity()));
        }
        RETURN_IF_ERROR(weight_item->WriteBytes(base::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(float16s.data()),
            subspan_byte_size)));
        break;
      }
      case OperandDataType::kFloat32: {
        base::FixedArray<float> floats(subspan_size);
        for (size_t i = 0u; i < subspan_size; ++i) {
          base::CheckedNumeric<float> data = base::FloatFromNativeEndian(
              a_subspan.subspan(i * sizeof(float)).first<4u>());
          data += base::FloatFromNativeEndian(
              b_subspan.subspan(i * sizeof(float)).first<4u>());
          floats[i] =
              data.ValueOrDefault(std::numeric_limits<float>::infinity());
        }
        RETURN_IF_ERROR(weight_item->WriteBytes(base::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(floats.data()),
            subspan_byte_size)));
        break;
      }
      case OperandDataType::kUint8: {
        base::FixedArray<uint8_t> uints(subspan_size);
        for (size_t i = 0u; i < subspan_size; ++i) {
          base::CheckedNumeric<uint8_t> data = base::U8FromNativeEndian(
              a_subspan.subspan(i * sizeof(uint8_t)).first<1u>());
          data += base::U8FromNativeEndian(
              b_subspan.subspan(i * sizeof(uint8_t)).first<1u>());
          uints[i] =
              data.ValueOrDefault(std::numeric_limits<uint8_t>::infinity());
        }
        RETURN_IF_ERROR(weight_item->WriteBytes(base::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(uints.data()),
            subspan_byte_size)));
        break;
      }
      case OperandDataType::kInt8: {
        base::FixedArray<int8_t> ints(subspan_size);
        for (size_t i = 0u; i < subspan_size; ++i) {
          base::CheckedNumeric<int8_t> data = base::I8FromNativeEndian(
              a_subspan.subspan(i * sizeof(int8_t)).first<1u>());
          data += base::I8FromNativeEndian(
              b_subspan.subspan(i * sizeof(int8_t)).first<1u>());
          ints[i] =
              data.ValueOrDefault(std::numeric_limits<int8_t>::infinity());
        }
        RETURN_IF_ERROR(weight_item->WriteBytes(base::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(ints.data()), subspan_byte_size)));
        break;
      }
      case OperandDataType::kInt32:
      case OperandDataType::kUint32:
      case OperandDataType::kInt64:
      case OperandDataType::kUint64:
      case OperandDataType::kInt4:
      case OperandDataType::kUint4:
        NOTREACHED() << "Unsupported weight type";
    }
  }
  ASSIGN_OR_RETURN(uint64_t new_constant,
                   GenerateInternalOperandInfo(
                       OperandTypeToMILDataType(data_type), dimensions));
  RETURN_IF_ERROR(weight_item->Finalize());
  SetInputWithValue(inputs, key, CreateConstantFileValue(new_constant, offset));

  return base::ok();
}

base::expected<uint64_t, mojom::ErrorPtr>
GraphBuilderCoreml::SliceFirstDimension(
    uint64_t input_operand_id,
    int32_t index,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info = GetOperandInfo(input_operand_id);
  std::vector<uint32_t> sliced_dimensions(input_operand_info.dimensions);
  std::vector<uint32_t> endings(input_operand_info.dimensions);
  CHECK(!sliced_dimensions.empty());
  sliced_dimensions[0] = 1;

  base::FixedArray<int32_t> beginnings(input_operand_info.dimensions.size(), 0);
  base::FixedArray<int32_t> strides(input_operand_info.dimensions.size(), 1);
  beginnings[0] = index;
  endings[0] = index + 1;
  ASSIGN_OR_RETURN(uint64_t sliced,
                   GenerateInternalOperandInfo(input_operand_info.mil_data_type,
                                               sliced_dimensions));
  RETURN_IF_ERROR(AddOperationForSlice(input_operand_id, sliced, beginnings,
                                       Ui32ToI32(endings), strides, block));
  ASSIGN_OR_RETURN(uint64_t sliced_squeezed,
                   GenerateInternalOperandInfo(
                       input_operand_info.mil_data_type,
                       base::span<const uint32_t>(sliced_dimensions.begin() + 1,
                                                  sliced_dimensions.end())));
  RETURN_IF_ERROR(AddOperationForReshape(sliced, sliced_squeezed, block));
  return sliced_squeezed;
}

base::expected<void, mojom::ErrorPtr> GraphBuilderCoreml::SplitAndSqueeze(
    uint64_t input_operand_id,
    base::span<uint64_t> output_operand_ids,
    int32_t axis,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info = GetOperandInfo(input_operand_id);
  uint32_t num_of_split = output_operand_ids.size();
  CHECK_EQ(output_operand_ids.size(), input_operand_info.dimensions[axis]);
  base::FixedArray<uint64_t> outputs(num_of_split);

  std::vector<uint32_t> output_shape = input_operand_info.dimensions;
  output_shape[axis] = 1;

  std::vector<uint32_t> squeezed_output_shape = input_operand_info.dimensions;
  squeezed_output_shape.erase(squeezed_output_shape.begin() + axis);
  for (uint32_t i = 0; i < num_of_split; i++) {
    ASSIGN_OR_RETURN(outputs[i],
                     GenerateInternalOperandInfo(
                         input_operand_info.mil_data_type, output_shape));

    ASSIGN_OR_RETURN(
        output_operand_ids[i],
        GenerateInternalOperandInfo(input_operand_info.mil_data_type,
                                    squeezed_output_shape));
  }
  RETURN_IF_ERROR(AddOperationForSplit(input_operand_id, outputs, axis, block));
  for (uint32_t i = 0; i < num_of_split; i++) {
    RETURN_IF_ERROR(
        AddOperationForReshape(outputs[i], output_operand_ids[i], block));
  }
  return base::ok();
}

GraphBuilderCoreml::OperandInfo::OperandInfo(
    std::string name,
    base::span<const uint32_t> dimensions,
    CoreML::Specification::MILSpec::DataType mil_data_type)
    : coreml_name(std::move(name)),
      external_coreml_name(coreml_name),
      dimensions(dimensions.begin(), dimensions.end()),
      mil_data_type(mil_data_type) {}

GraphBuilderCoreml::OperandInfo::OperandInfo() = default;
GraphBuilderCoreml::OperandInfo::~OperandInfo() = default;
GraphBuilderCoreml::OperandInfo::OperandInfo(OperandInfo&) = default;
GraphBuilderCoreml::OperandInfo::OperandInfo(OperandInfo&&) = default;

GraphBuilderCoreml::Result::Result(base::FilePath ml_package_dir)
    : ml_package_dir(std::move(ml_package_dir)) {}
GraphBuilderCoreml::Result::~Result() = default;

const base::FilePath& GraphBuilderCoreml::Result::GetModelFilePath() {
  return ml_package_dir;
}

const GraphBuilderCoreml::OperandInfo&
GraphBuilderCoreml::Result::GetOperandInfo(uint64_t operand_id) const {
  auto it = id_to_operand_info_map.find(operand_id);
  CHECK(it != id_to_operand_info_map.end());
  return it->second;
}

}  // namespace webnn::coreml
