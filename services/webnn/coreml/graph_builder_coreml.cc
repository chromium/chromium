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
#include <memory>
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
constexpr char kOpGatherTypeName[] = "gather_along_axis";
constexpr char kOpHardSigmoidTypeName[] = "sigmoid_hard";
constexpr char kOpInstanceNormalizationTypeName[] = "instance_norm";
constexpr char kOpLayerNormalizationTypeName[] = "layer_norm";
constexpr char kOpLeakyReluTypeName[] = "leaky_relu";
constexpr char kOpMatmulTypeName[] = "matmul";
constexpr char kOpPadTypeName[] = "pad";
constexpr char kOpReluTypeName[] = "relu";
constexpr char kOpReshapeTypeName[] = "reshape";
constexpr char kOpSigmoidTypeName[] = "sigmoid";
constexpr char kOpSliceTypeName[] = "slice_by_size";
constexpr char kOpSoftmaxTypeName[] = "softmax";
constexpr char kOpSoftplusTypeName[] = "softplus";
constexpr char kOpSoftsignTypeName[] = "softsign";
constexpr char kOpSplitTypeName[] = "split";
constexpr char kOpTanhTypeName[] = "tanh";
constexpr char kOpTileTypeName[] = "tile";
constexpr char kOpTransposeTypeName[] = "transpose";
constexpr char kOpWhereTypeName[] = "select";
// Elementwise binary operators.
constexpr char kOpAddTypeName[] = "add";
constexpr char kOpMultiplyTypeName[] = "mul";
constexpr char kOpDivideTypeName[] = "real_div";
constexpr char kOpSubtractTypeName[] = "sub";
constexpr char kOpMaximumTypeName[] = "maximum";
constexpr char kOpMinimumTypeName[] = "minimum";
constexpr char kOpPowerTypeName[] = "pow";
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
constexpr char kOpParamDataTypeName[] = "dtype";
constexpr char kOpParamEpsilon[] = "epsilon";
constexpr char kOpParamGamma[] = "gamma";
constexpr char kOpParamKeepDims[] = "keep_dims";
constexpr char kOpParamPad[] = "pad";
constexpr char kOpParamReps[] = "reps";
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
      base::span({base::checked_cast<uint32_t>(value.size())}), value);
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
void SetInputWithName(
    google::protobuf::Map<std::string,
                          CoreML::Specification::MILSpec::Argument>& inputs,
    std::string_view key,
    std::string_view name) {
  inputs[key].add_arguments()->set_name(std::string(name));
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

}  // namespace

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

  base::FilePath data_dir = ml_package_dir.Append(kMlPackageDataDir);

  GraphBuilderCoreml graph_builder(graph_info, std::move(context_properties),
                                   constant_operands,
                                   std::move(ml_package_dir));

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
       /*constant=*/kFloatsAndInt32,
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
       // TODO(crbug.com/368208141): Implement logical binary ops.
       /*logical_and_input=*/{},
       /*logical_or_input=*/{},
       /*logical_xor_input=*/{},
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
       // Note that INT16, and UINT16 is also supported by CoreML, but WebNN
       // does not have corresponding types. See docs here:
       // https://apple.github.io/coremltools/source/coremltools.converters.mil.mil.ops.defs.html#coremltools.converters.mil.mil.ops.defs.iOS17.scatter_gather.gather
       /*gather_input=*/kFloat16To32Int8To32AndUint8,
       /*gather_indices=*/
       kGatherIndicesSupportedDataTypes,
       // GatherElements is not implemented.
       /*gather_elements_input=*/{},
       /*gather_elements_indices=*/{},
       // GatherND is not implemented.
       /*gather_nd_input=*/{},
       /*gather_nd_indices=*/{},
       /*gelu_input=*/DataTypeConstraint::kFloat16To32,
       /*gemm_input=*/DataTypeConstraint::kFloat16To32,
       // Gru is not implemented.
       /*gru_input=*/{},
       // GruCell is not implemented.
       /*gru_cell_input=*/{},
       /*hard_sigmoid_input=*/DataTypeConstraint::kFloat16To32,
       /*hard_swish_input=*/DataTypeConstraint::kFloat16To32,
       /*instance_normalization_input=*/DataTypeConstraint::kFloat16To32,
       /*layer_normalization_input=*/DataTypeConstraint::kFloat16To32,
       /*leaky_relu_input=*/DataTypeConstraint::kFloat16To32,
       // TODO: crbug.com/338667172 - Consider enhancing the data type support
       // to include int32.
       /*linear_input=*/DataTypeConstraint::kFloat16To32,
       // Lstm is not implemented.
       /*lstm_input=*/{},
       // LstmCell is not implemented.
       /*lstm_cell_input=*/{},
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
       // TODO(crbug.com/363544348): Implement ScatterND.
       /*scatter_nd_input=*/{},
       /*scatter_nd_indices=*/{},
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
       /*triangular_input=*/{},
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
    base::FilePath ml_package_dir)
    : graph_info_(graph_info),
      constant_operands_(constant_operands),
      context_properties_(std::move(context_properties)),
      internal_operand_id_(
          base::ranges::max_element(
              graph_info_->id_to_operand_map,
              {},
              [](const auto& id_operand) { return id_operand.first; })
              ->first),
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

  RETURN_IF_ERROR(SetupMlPackageDirStructure());

  base::ElapsedTimer ml_weights_write_timer;
  RETURN_IF_ERROR(WriteWeightsToFile(block));
  UMA_HISTOGRAM_MEDIUM_TIMES("WebNN.CoreML.TimingMs.MLWeightsWrite",
                             ml_weights_write_timer.Elapsed());

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
        AddOperationForClamp(*operation->get_clamp(), block);
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
        AddOperationForCumulativeSum(*operation->get_cumulative_sum(), block);
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
      case mojom::Operation::Tag::kGemm: {
        RETURN_IF_ERROR(AddOperationForGemm(*operation->get_gemm(), block));
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
      case mojom::Operation::Tag::kMatmul: {
        AddOperationForMatmul(*operation->get_matmul(), block);
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
      case mojom::Operation::Tag::kSigmoid: {
        CHECK(context_properties_.data_type_limits.sigmoid_input.Has(
            MILDataTypeToOperandType(
                GetOperandInfo(operation->get_sigmoid()->input_operand_id)
                    .mil_data_type)));
        AddUnaryOperation(kOpSigmoidTypeName, *operation->get_sigmoid(), block);
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
        AddUnaryOperation(kOpSoftplusTypeName, *operation->get_softplus(),
                          block);
        break;
      }
      case mojom::Operation::Tag::kSoftsign: {
        CHECK(context_properties_.data_type_limits.softsign_input.Has(
            MILDataTypeToOperandType(
                GetOperandInfo(operation->get_softsign()->input_operand_id)
                    .mil_data_type)));
        AddUnaryOperation(kOpSoftsignTypeName, *operation->get_softsign(),
                          block);
        break;
      }
      case mojom::Operation::Tag::kSplit: {
        AddOperationForSplit(*operation->get_split(), block);
        break;
      }
      case mojom::Operation::Tag::kTanh: {
        CHECK(context_properties_.data_type_limits.tanh_input.Has(
            MILDataTypeToOperandType(
                GetOperandInfo(operation->get_tanh()->input_operand_id)
                    .mil_data_type)));
        AddUnaryOperation(kOpTanhTypeName, *operation->get_tanh(), block);
        break;
      }
      case mojom::Operation::Tag::kTile: {
        AddOperationForTile(*operation->get_tile(), block);
        break;
      }
      case mojom::Operation::Tag::kTranspose: {
        AddOperationForTranspose(*operation->get_transpose(), block);
        break;
      }
      case mojom::Operation::Tag::kWhere: {
        RETURN_IF_ERROR(AddOperationForWhere(*operation->get_where(), block));
        break;
      }
      case mojom::Operation::Tag::kDequantizeLinear:
      case mojom::Operation::Tag::kGatherElements:
      case mojom::Operation::Tag::kGatherNd:
      case mojom::Operation::Tag::kGelu:
      case mojom::Operation::Tag::kGru:
      case mojom::Operation::Tag::kGruCell:
      case mojom::Operation::Tag::kLstm:
      case mojom::Operation::Tag::kLstmCell:
      case mojom::Operation::Tag::kPrelu:
      case mojom::Operation::Tag::kQuantizeLinear:
      case mojom::Operation::Tag::kScatterNd:
      case mojom::Operation::Tag::kTriangular:
        return NewNotSupportedError(NotSupportedOperatorError(*operation));
    }
  }

  // Add output.
  for (uint64_t output_id : graph_info_->output_operands) {
    block.add_outputs(GetOperandInfo(output_id).coreml_name);
    RETURN_IF_ERROR(AddOutput(output_id));
  }
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
  UMA_HISTOGRAM_MEDIUM_TIMES("WebNN.CoreML.TimingMs.MLModelWrite",
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

base::expected<void, mojom::ErrorPtr> GraphBuilderCoreml::WriteWeightsToFile(
    CoreML::Specification::MILSpec::Block& block) {
  base::FilePath weights_file_path = ml_package_dir()
                                         .Append(kMlPackageDataDir)
                                         .Append(kMlPackageWeightsDir)
                                         .Append(kMlPackageWeightsFileName);
  base::File weights_file(weights_file_path,
                          base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  if (!weights_file.IsValid()) {
    LOG(ERROR) << "[WebNN] Unable to open " << weights_file_path << ": "
               << base::File::ErrorToString(weights_file.error_details());
    return NewUnknownError(kWriteWeightsErrorMessage);
  }

  uint64_t current_offset = 0;
  WeightHeader header{static_cast<uint32_t>(constant_operands_->size())};
  if (!weights_file.WriteAtCurrentPosAndCheck(
          base::byte_span_from_ref(header))) {
    return NewUnknownError(kWriteWeightsErrorMessage);
  }
  current_offset += sizeof(header);

  for (auto& [id, constant_operand] : *constant_operands_) {
    // int32 is only supported as immediate value.
    if (constant_operand->descriptor().shape().empty() ||
        constant_operand->descriptor().data_type() == OperandDataType::kInt32) {
      RETURN_IF_ERROR(AddConstantImmediateValue(id, block));
      continue;
    }

    std::optional<BlobDataType> weight_type = OperandTypeToDataTypeInWeightFile(
        constant_operand->descriptor().data_type());
    if (!weight_type.has_value()) {
      return NewNotSupportedError("Unsupported constant type.");
    }

    WeightMetadata metadata(weight_type.value(),
                            constant_operand->ByteSpan().size(),
                            current_offset + sizeof(metadata));

    if (!weights_file.WriteAtCurrentPosAndCheck(
            base::byte_span_from_ref(metadata))) {
      return NewUnknownError(kWriteWeightsErrorMessage);
    }

    if (!weights_file.WriteAtCurrentPosAndCheck(constant_operand->ByteSpan())) {
      return NewUnknownError(kWriteWeightsErrorMessage);
    }

    RETURN_IF_ERROR(AddConstantFileValue(id, current_offset, block));
    current_offset += sizeof(metadata);
    current_offset += constant_operand->ByteSpan().size();
    current_offset = base::bits::AlignUp(current_offset, kWeightAlignment);
    if (!weights_file.Seek(base::File::Whence::FROM_BEGIN, current_offset)) {
      return NewUnknownError(kWriteWeightsErrorMessage);
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
  SetInputWithName(*placeholder_op->mutable_inputs(), kOpParamX,
                   kPlaceholderInputName);
  SetInputWithName(*placeholder_op->mutable_inputs(), kOpParamY,
                   kPlaceholderInputName);
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

  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);

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

void GraphBuilderCoreml::AddUnaryOperation(
    std::string_view op_name,
    uint64_t input_operand_id,
    uint64_t output_operand_id,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info = GetOperandInfo(input_operand_id);

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(std::string(op_name));

  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);

  PopulateNamedValueType(output_operand_id, *op->add_outputs());
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
void GraphBuilderCoreml::AddUnaryOperation(
    std::string_view op_name,
    const T& operation,
    CoreML::Specification::MILSpec::Block& block) {
  AddUnaryOperation(op_name, operation.input_operand_id,
                    operation.output_operand_id, block);
}

void GraphBuilderCoreml::AddUnaryFloatsOperationWithEpsilon(
    std::string_view op_name,
    std::string_view input_name,
    CoreML::Specification::MILSpec::DataType input_mil_data_type,
    uint64_t output_operand_id,
    float epsilon,
    CoreML::Specification::MILSpec::Block& block) {
  CHECK(kFloatDataTypes.contains(input_mil_data_type));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(std::string(op_name));

  SetInputWithName(*op->mutable_inputs(), kOpParamX, input_name);

  SetInputWithValue(*op->mutable_inputs(), kOpParamEpsilon,
                    CreateFloatValue(input_mil_data_type, epsilon));

  PopulateNamedValueType(output_operand_id, *op->add_outputs());
}

template <typename T>
void GraphBuilderCoreml::AddUnaryFloatsOperationWithEpsilon(
    std::string_view op_name,
    const T& operation,
    float epsilon,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  AddUnaryFloatsOperationWithEpsilon(
      op_name, input_operand_info.coreml_name, input_operand_info.mil_data_type,
      operation.output_operand_id, epsilon, block);
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
  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   GetOperandInfo(input_operand_id).coreml_name);

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
  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);

  static constexpr char kParamMean[] = "mean";
  static constexpr char kParamVariance[] = "variance";

  // TODO(crbug.com/338529226): These params must all be constant tensors.
  SetInputWithName(*op->mutable_inputs(), kParamMean,
                   GetOperandInfo(operation.mean_operand_id).coreml_name);
  SetInputWithName(*op->mutable_inputs(), kParamVariance,
                   GetOperandInfo(operation.variance_operand_id).coreml_name);
  if (operation.scale_operand_id.has_value()) {
    SetInputWithName(*op->mutable_inputs(), kOpParamGamma,
                     GetOperandInfo(*operation.scale_operand_id).coreml_name);
  }
  if (operation.bias_operand_id.has_value()) {
    SetInputWithName(*op->mutable_inputs(), kOpParamBeta,
                     GetOperandInfo(*operation.bias_operand_id).coreml_name);
  }

  SetInputWithValue(
      *op->mutable_inputs(), kOpParamEpsilon,
      CreateFloatValue(input_operand_info.mil_data_type, operation.epsilon));

  CoreML::Specification::MILSpec::NamedValueType& output = *op->add_outputs();
  PopulateNamedValueType(operation.output_operand_id, output);
  return base::ok();
}

void GraphBuilderCoreml::AddOperationForCast(
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

  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);
  op->set_type(kOpCastTypeName);
  SetInputWithValue(
      *op->mutable_inputs(), kOpParamDataTypeName,
      CreateStringImmediateValue(MilDataTypeToString(output_data_type)));
  PopulateNamedValueType(output_operand_id, *op->add_outputs());
}

void GraphBuilderCoreml::AddOperationForClamp(
    const mojom::Clamp& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  CHECK(context_properties_.data_type_limits.clamp_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpClipTypeName);

  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);

  SetInputsWithValues(
      *op->mutable_inputs(),
      {
          {kOpParamAlpha, CreateFloatValue(input_operand_info.mil_data_type,
                                           operation.min_value)},
          {kOpParamBeta, CreateFloatValue(input_operand_info.mil_data_type,
                                          operation.max_value)},
      });

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
}

base::expected<void, mojom::ErrorPtr> GraphBuilderCoreml::AddOperationForConcat(
    const mojom::Concat& operation,
    CoreML::Specification::MILSpec::Block& block) {
  CHECK(base::ranges::all_of(
      operation.input_operand_ids, [&](uint64_t input_operand_id) {
        return context_properties_.data_type_limits.concat_inputs.Has(
            MILDataTypeToOperandType(
                GetOperandInfo(input_operand_id).mil_data_type));
      }));

  static constexpr char kParamValues[] = "values";
  static constexpr char kParamInterleave[] = "interleave";

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpConcatTypeName);

  for (uint64_t input_operand_id : operation.input_operand_ids) {
    SetInputWithName(*op->mutable_inputs(), kParamValues,
                     GetOperandInfo(input_operand_id).coreml_name);
  }
  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kOpParamAxis, CreateScalarImmediateValue(
                          base::checked_cast<int32_t>(operation.axis))},
       {kParamInterleave, CreateScalarImmediateValue(false)}});

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilderCoreml::AddOperationForConv2d(
    const mojom::Conv2d& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand = GetOperandInfo(operation.input_operand_id);

  static constexpr char kParamWeight[] = "weight";
  static constexpr char kParamStrides[] = "strides";
  static constexpr char kParamPadType[] = "pad_type";
  static constexpr char kParamPadTypeValue[] = "custom";
  static constexpr char kParamDilations[] = "dilations";
  static constexpr char kParamGroups[] = "groups";
  static constexpr char kParamBias[] = "bias";
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

  SetInputWithName(*op->mutable_inputs(), kOpParamX, input_operand.coreml_name);
  SetInputWithName(*op->mutable_inputs(), kParamWeight,
                   GetOperandInfo(operation.filter_operand_id).coreml_name);

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
    SetInputWithName(
        *op->mutable_inputs(), kParamBias,
        GetOperandInfo(operation.bias_operand_id.value()).coreml_name);
  }

  if (operation.kind == mojom::Conv2d::Kind::kTransposed) {
    // Get the output shape from the output operand.
    const OperandInfo& output_operand =
        GetOperandInfo(operation.output_operand_id);
    base::FixedArray<int32_t> output_shape(output_operand.dimensions.size());
    base::ranges::transform(
        output_operand.dimensions, output_shape.begin(),
        [](uint32_t val) { return base::checked_cast<int32_t>(val); });
    SetInputWithValue(*op->mutable_inputs(), kParamOutputShape,
                      Create1DTensorImmediateValue<int32_t>(output_shape));
  }

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

void GraphBuilderCoreml::AddOperationForCumulativeSum(
    const mojom::CumulativeSum& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  CHECK(context_properties_.data_type_limits.cumulative_sum_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpCumulativeSumTypeName);
  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);

  static constexpr char kParamExclusive[] = "exclusive";
  static constexpr char kParamReverse[] = "reverse";

  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kOpParamAxis, CreateScalarImmediateValue(
                          base::checked_cast<int32_t>(operation.axis))},
       {kParamExclusive, CreateScalarImmediateValue(operation.exclusive)},
       {kParamReverse, CreateScalarImmediateValue(operation.reversed)}});
  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
}

base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForElementwiseBinary(
    uint64_t lhs_operand_id,
    std::variant<uint64_t, CoreML::Specification::MILSpec::Value> rhs_operand,
    uint64_t output_operand_id,
    const mojom::ElementWiseBinary::Kind kind,
    CoreML::Specification::MILSpec::Block& block) {
  CoreML::Specification::MILSpec::Operation* op = block.add_operations();

  const OperandInfo& lhs_operand_info = GetOperandInfo(lhs_operand_id);
  const OperandDataType input_data_type =
      MILDataTypeToOperandType(lhs_operand_info.mil_data_type);

  switch (kind) {
    case mojom::ElementWiseBinary::Kind::kAdd: {
      CHECK(
          context_properties_.data_type_limits.add_input.Has(input_data_type));
      op->set_type(kOpAddTypeName);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kDiv: {
      CHECK(
          context_properties_.data_type_limits.div_input.Has(input_data_type));
      op->set_type(kOpDivideTypeName);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kMul: {
      CHECK(
          context_properties_.data_type_limits.mul_input.Has(input_data_type));
      op->set_type(kOpMultiplyTypeName);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kSub: {
      CHECK(
          context_properties_.data_type_limits.sub_input.Has(input_data_type));
      op->set_type(kOpSubtractTypeName);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kMax: {
      CHECK(
          context_properties_.data_type_limits.max_input.Has(input_data_type));
      op->set_type(kOpMaximumTypeName);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kMin: {
      CHECK(
          context_properties_.data_type_limits.min_input.Has(input_data_type));
      op->set_type(kOpMinimumTypeName);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kPow: {
      CHECK(
          context_properties_.data_type_limits.pow_input.Has(input_data_type));
      op->set_type(kOpPowerTypeName);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kEqual: {
      CHECK(context_properties_.data_type_limits.equal_input.Has(
          input_data_type));
      op->set_type(kOpLogicalEqual);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kGreater: {
      CHECK(context_properties_.data_type_limits.greater_input.Has(
          input_data_type));
      op->set_type(kOpLogicalGreater);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kGreaterOrEqual: {
      CHECK(context_properties_.data_type_limits.greater_or_equal_input.Has(
          input_data_type));
      op->set_type(kOpLogicalGreaterEqual);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLesser: {
      CHECK(context_properties_.data_type_limits.lesser_input.Has(
          input_data_type));
      op->set_type(kOpLogicalLess);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLesserOrEqual: {
      CHECK(context_properties_.data_type_limits.lesser_or_equal_input.Has(
          input_data_type));
      op->set_type(kOpLogicalLessEqual);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLogicalAnd:
    case mojom::ElementWiseBinary::Kind::kLogicalOr:
    case mojom::ElementWiseBinary::Kind::kLogicalXor: {
      // TODO(crbug.com/368208141): Implement logical binary ops for CoreML.
      return NewNotSupportedError(
          "logicalAnd, logicalXor, and logicalXor are not yet supported on "
          "CoreML.");
    }
  }

  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   lhs_operand_info.coreml_name);

  std::visit(
      base::Overloaded{[&](uint64_t rhs_operand_id) {
                         const OperandInfo& rhs_operand_info =
                             GetOperandInfo(rhs_operand_id);
                         CHECK_EQ(lhs_operand_info.mil_data_type,
                                  rhs_operand_info.mil_data_type);
                         SetInputWithName(*op->mutable_inputs(), kOpParamY,
                                          rhs_operand_info.coreml_name);
                       },
                       [&](CoreML::Specification::MILSpec::Value rhs_value) {
                         SetInputWithValue(*op->mutable_inputs(), kOpParamY,
                                           rhs_value);
                       }},
      rhs_operand);

  if (IsLogicalElementWiseBinary(kind)) {
    // The output of logical binary ops need to be cast from a boolean
    // tensor that CoreML provides to an UInt8 that WebNN expects.
    ASSIGN_OR_RETURN(uint64_t internal_output_id,
                     GenerateInternalOperandInfo(
                         CoreML::Specification::MILSpec::DataType::BOOL,
                         GetOperandInfo(output_operand_id).dimensions));
    PopulateNamedValueType(internal_output_id, *op->add_outputs());

    AddOperationForCast(internal_output_id, output_operand_id, block);
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

  std::string_view input_name = GetOperandInfo(input_operand_id).coreml_name;

  switch (kind) {
    case mojom::ElementWiseUnary::Kind::kAbs: {
      CHECK(context_properties_.data_type_limits.abs_input.Has(
          input_operand_data_type));
      AddUnaryOperation(kOpAbsTypeName, input_operand_id, output_operand_id,
                        block);
      return base::ok();
    }
    case mojom::ElementWiseUnary::Kind::kCast: {
      AddOperationForCast(input_operand_id, output_operand_id, block);
      return base::ok();
    }
    case mojom::ElementWiseUnary::Kind::kCeil: {
      CHECK(context_properties_.data_type_limits.ceil_input.Has(
          input_operand_data_type));
      AddUnaryOperation(kOpCeilTypeName, input_operand_id, output_operand_id,
                        block);
      return base::ok();
    }
    case mojom::ElementWiseUnary::Kind::kCos: {
      CHECK(context_properties_.data_type_limits.cos_input.Has(
          input_operand_data_type));
      AddUnaryOperation(kOpCosTypeName, input_operand_id, output_operand_id,
                        block);
      return base::ok();
    }
    case mojom::ElementWiseUnary::Kind::kErf: {
      CHECK(context_properties_.data_type_limits.erf_input.Has(
          input_operand_data_type));
      AddUnaryOperation(kOpErfTypeName, input_operand_id, output_operand_id,
                        block);
      return base::ok();
    }
    case mojom::ElementWiseUnary::Kind::kExp: {
      CHECK(context_properties_.data_type_limits.exp_input.Has(
          input_operand_data_type));
      AddUnaryOperation(kOpExpTypeName, input_operand_id, output_operand_id,
                        block);
      return base::ok();
    }
    case mojom::ElementWiseUnary::Kind::kFloor: {
      CHECK(context_properties_.data_type_limits.floor_input.Has(
          input_operand_data_type));
      AddUnaryOperation(kOpFloorTypeName, input_operand_id, output_operand_id,
                        block);
      return base::ok();
    }
    case mojom::ElementWiseUnary::Kind::kIdentity: {
      CHECK(context_properties_.data_type_limits.identity_input.Has(
          input_operand_data_type));
      AddUnaryOperation(kOpIdentityTypeName, input_operand_id,
                        output_operand_id, block);
      return base::ok();
    }
    case mojom::ElementWiseUnary::Kind::kSign: {
      // Sign is not implemented.
      NOTREACHED_NORETURN();
    }
    case mojom::ElementWiseUnary::Kind::kSin: {
      CHECK(context_properties_.data_type_limits.sin_input.Has(
          input_operand_data_type));
      AddUnaryOperation(kOpSinTypeName, input_operand_id, output_operand_id,
                        block);
      return base::ok();
    }
    case mojom::ElementWiseUnary::Kind::kSqrt: {
      CHECK(context_properties_.data_type_limits.sqrt_input.Has(
          input_operand_data_type));
      AddUnaryOperation(kOpSqrtTypeName, input_operand_id, output_operand_id,
                        block);
      return base::ok();
    }
    case mojom::ElementWiseUnary::Kind::kTan: {
      CHECK(context_properties_.data_type_limits.tan_input.Has(
          input_operand_data_type));
      AddUnaryOperation(kOpTanTypeName, input_operand_id, output_operand_id,
                        block);
      return base::ok();
    }
    case mojom::ElementWiseUnary::Kind::kReciprocal: {
      CHECK(context_properties_.data_type_limits.reciprocal_input.Has(
          input_operand_data_type));
      // CoreML's reciprocal operator requires an epsilon value, the default
      // value as per the documentation 1e-4 results in expressions like
      // reciprocal(4) returning  0.24999 rather than 0.25.
      // In order to return expected results similar to other platforms,
      // set epsilon to 0.
      AddUnaryFloatsOperationWithEpsilon(kOpReciprocalTypeName, input_name,
                                         input_data_type, output_operand_id,
                                         /*epsilon=*/0, block);
      return base::ok();
    }
    case mojom::ElementWiseUnary::Kind::kLog: {
      CHECK(context_properties_.data_type_limits.log_input.Has(
          input_operand_data_type));
      // CoreML's log operator requires an epsilon value, the default
      // value as per the documentation 1e-45 potentially could result
      // in different result compared to other platforms.
      // In order to return expected results compatible with other
      // platforms, set epsilon to 0.
      AddUnaryFloatsOperationWithEpsilon(kOpLogTypeName, input_name,
                                         input_data_type, output_operand_id,
                                         /*epsilon=*/0, block);
      return base::ok();
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
      AddOperationForCast(input_operand_id, cast_to_bool_operand_id, block);
      ASSIGN_OR_RETURN(uint64_t logical_not_output_operand_id,
                       GenerateInternalOperandInfo(
                           CoreML::Specification::MILSpec::DataType::BOOL,
                           input_operand_info.dimensions));
      AddUnaryOperation(kOpLogicalNot, cast_to_bool_operand_id,
                        logical_not_output_operand_id, block);
      AddOperationForCast(logical_not_output_operand_id, output_operand_id,
                          block);
      return base::ok();
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

base::expected<void, mojom::ErrorPtr> GraphBuilderCoreml::AddOperationForGather(
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

  static constexpr char kParamIndices[] = "indices";
  static constexpr char kParamValidateIndices[] = "validate_indices";

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpGatherTypeName);
  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);

  SetInputWithName(*op->mutable_inputs(), kParamIndices,
                   indices_operand_info.coreml_name);

  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kOpParamAxis, CreateScalarImmediateValue(
                          base::checked_cast<int32_t>(operation.axis))},
       {kParamValidateIndices, CreateScalarImmediateValue(false)}});

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilderCoreml::AddOperationForGemm(
    const mojom::Gemm& operation,
    CoreML::Specification::MILSpec::Block& block) {
  // Gemm is not supported in CoreML. This is emulated with:
  //   add(mul(alpha, matmul(A, B)), mul(beta, C))
  const OperandInfo& a_operand_info = GetOperandInfo(operation.a_operand_id);
  const OperandInfo& b_operand_info = GetOperandInfo(operation.b_operand_id);
  CHECK(a_operand_info.dimensions.size() == 2 &&
        b_operand_info.dimensions.size() == 2);
  CHECK(context_properties_.data_type_limits.gemm_input.Has(
      MILDataTypeToOperandType(a_operand_info.mil_data_type)));
  CHECK_EQ(a_operand_info.mil_data_type, b_operand_info.mil_data_type);

  uint32_t first_dimension = operation.a_transpose
                                 ? a_operand_info.dimensions[1]
                                 : a_operand_info.dimensions[0];
  uint32_t second_dimension = operation.b_transpose
                                  ? b_operand_info.dimensions[0]
                                  : b_operand_info.dimensions[1];

  std::array<uint32_t, 2> matmul_dimensions{first_dimension, second_dimension};
  if (operation.alpha == 1.0f && !operation.c_operand_id) {
    AddOperationForMatmul(operation.a_operand_id, operation.b_operand_id,
                          operation.a_transpose, operation.b_transpose,
                          operation.output_operand_id, block);
    return base::ok();
  }

  ASSIGN_OR_RETURN(uint64_t matmul_output,
                   GenerateInternalOperandInfo(a_operand_info.mil_data_type,
                                               matmul_dimensions));
  AddOperationForMatmul(operation.a_operand_id, operation.b_operand_id,
                        operation.a_transpose, operation.b_transpose,
                        matmul_output, block);

  if (operation.alpha != 1.0f) {
    uint64_t with_alpha_output = operation.output_operand_id;
    if (operation.c_operand_id) {
      ASSIGN_OR_RETURN(with_alpha_output,
                       GenerateInternalOperandInfo(a_operand_info.mil_data_type,
                                                   matmul_dimensions));
    }

    RETURN_IF_ERROR(AddOperationForElementwiseBinary(
        matmul_output,
        CreateFloatValue(a_operand_info.mil_data_type, operation.alpha),
        with_alpha_output, mojom::ElementWiseBinary::Kind::kMul, block));
    matmul_output = with_alpha_output;
  }

  if (!operation.c_operand_id) {
    return base::ok();
  }
  uint64_t c_operand_id = operation.c_operand_id.value();
  const OperandInfo& c_operand_info = GetOperandInfo(c_operand_id);
  CHECK_EQ(a_operand_info.mil_data_type, c_operand_info.mil_data_type);

  if (operation.beta != 1.0f) {
    ASSIGN_OR_RETURN(uint64_t with_beta_output,
                     GenerateInternalOperandInfo(a_operand_info.mil_data_type,
                                                 matmul_dimensions));
    RETURN_IF_ERROR(AddOperationForElementwiseBinary(
        c_operand_id,
        CreateFloatValue(c_operand_info.mil_data_type, operation.beta),
        with_beta_output, mojom::ElementWiseBinary::Kind::kMul, block));
    c_operand_id = with_beta_output;
  }
  return AddOperationForElementwiseBinary(
      matmul_output, c_operand_id, operation.output_operand_id,
      mojom::ElementWiseBinary::Kind::kAdd, block);
}

base::expected<void, mojom::ErrorPtr>
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

  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);

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

base::expected<void, mojom::ErrorPtr>
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
  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);

  // TODO(crbug.com/338529226): These params must all be constant tensors.
  if (operation.scale_operand_id.has_value()) {
    SetInputWithName(*op->mutable_inputs(), kOpParamGamma,
                     GetOperandInfo(*operation.scale_operand_id).coreml_name);
  }
  if (operation.bias_operand_id.has_value()) {
    SetInputWithName(*op->mutable_inputs(), kOpParamBeta,
                     GetOperandInfo(*operation.bias_operand_id).coreml_name);
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

  // TODO: crbug.com/356905058: Figure out if unordered axes should be allowed.
  if (!base::ranges::is_sorted(operation.axes)) {
    return NewNotSupportedError("Axes must be ordered for layerNormalization.");
  }

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpLayerNormalizationTypeName);
  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);
  base::FixedArray<int32_t> axes(operation.axes.size());
  base::ranges::transform(operation.axes, axes.begin(), [](uint32_t val) {
    return base::checked_cast<int32_t>(val);
  });

  // TODO: crbug.com/338529226: These params must all be constant tensors.
  if (operation.scale_operand_id.has_value()) {
    SetInputWithName(
        *op->mutable_inputs(), kOpParamGamma,
        GetOperandInfo(operation.scale_operand_id.value()).coreml_name);
  }
  if (operation.bias_operand_id.has_value()) {
    SetInputWithName(
        *op->mutable_inputs(), kOpParamBeta,
        GetOperandInfo(operation.bias_operand_id.value()).coreml_name);
  }

  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kOpParamAxes, Create1DTensorImmediateValue<int32_t>(axes)},
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

void GraphBuilderCoreml::AddOperationForMatmul(
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
  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);

  SetInputWithName(*op->mutable_inputs(), kOpParamY,
                   GetOperandInfo(input_y_operand_id).coreml_name);

  static constexpr char kParamTransposeX[] = "transpose_x";
  static constexpr char kParamTransposeY[] = "transpose_y";
  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kParamTransposeX, CreateScalarImmediateValue(transpose_x)},
       {kParamTransposeY, CreateScalarImmediateValue(transpose_y)}});

  PopulateNamedValueType(output_operand_id, *op->add_outputs());
}

void GraphBuilderCoreml::AddOperationForMatmul(
    const mojom::Matmul& operation,
    CoreML::Specification::MILSpec::Block& block) {
  AddOperationForMatmul(
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

  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);

  base::FixedArray<int32_t> paddings(operation.beginning_padding.size() +
                                     operation.ending_padding.size());
  CHECK_EQ(operation.beginning_padding.size(), operation.ending_padding.size());
  for (size_t i = 0; i < operation.beginning_padding.size(); ++i) {
    paddings[i * 2] = operation.beginning_padding[i];
    paddings[i * 2 + 1] = operation.ending_padding[i];
  }

  constexpr char kParamMode[] = "mode";
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
       {kParamMode, CreateStringImmediateValue(mode)},
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

  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);

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

  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);

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

  base::FixedArray<int32_t> axes(operation.axes.size());
  base::ranges::transform(operation.axes, axes.begin(), [](uint32_t val) {
    return base::checked_cast<int32_t>(val);
  });
  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kOpParamAxes, Create1DTensorImmediateValue<int32_t>(axes)},
       {kOpParamKeepDims,
        CreateScalarImmediateValue(operation.keep_dimensions)}});

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

base::expected<void, mojom::ErrorPtr>
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

  SetInputWithName(*op.mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);

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
  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);

  static constexpr char kParamShape[] = "shape";
  base::FixedArray<int32_t> shape(output_operand_info.dimensions.size());
  base::ranges::transform(
      output_operand_info.dimensions, shape.begin(),
      [](uint32_t val) { return base::checked_cast<int32_t>(val); });
  SetInputWithValue(*op->mutable_inputs(), kParamShape,
                    Create1DTensorImmediateValue<int32_t>(shape));

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

base::expected<void, mojom::ErrorPtr> GraphBuilderCoreml::AddOperationForSlice(
    const mojom::Slice& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  CHECK(context_properties_.data_type_limits.slice_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpSliceTypeName);
  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);

  static constexpr char kParamBegin[] = "begin";
  static constexpr char kParamSize[] = "size";
  base::FixedArray<int32_t> beginnings(operation.starts_and_sizes.size());
  base::FixedArray<int32_t> sizes(operation.starts_and_sizes.size());
  for (size_t i = 0; i < operation.starts_and_sizes.size(); ++i) {
    if (operation.starts_and_sizes[i]->size == 0) {
      continue;
    }
    beginnings[i] =
        base::checked_cast<int32_t>(operation.starts_and_sizes[i]->start);
    sizes[i] = base::checked_cast<int32_t>(operation.starts_and_sizes[i]->size);
  }

  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kParamBegin, Create1DTensorImmediateValue<int32_t>(beginnings)},
       {kParamSize, Create1DTensorImmediateValue<int32_t>(sizes)}});

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddOperationForSoftmax(
    const mojom::Softmax& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  CHECK(context_properties_.data_type_limits.softmax_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpSoftmaxTypeName);

  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);

  SetInputWithValue(
      *op->mutable_inputs(), kOpParamAxis,
      CreateScalarImmediateValue(base::checked_cast<int32_t>(operation.axis)));
  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

void GraphBuilderCoreml::AddOperationForSplit(
    const mojom::Split& operation,
    CoreML::Specification::MILSpec::Block& block) {
  if (operation.output_operand_ids.size() == 1) {
    return AddUnaryOperation(kOpIdentityTypeName, operation.input_operand_id,
                             operation.output_operand_ids[0], block);
  }
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  CHECK(context_properties_.data_type_limits.split_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpSplitTypeName);
  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);

  base::FixedArray<int32_t> split_sizes(operation.output_operand_ids.size());
  for (size_t i = 0; i < operation.output_operand_ids.size(); ++i) {
    const uint64_t output_operand_id = operation.output_operand_ids[i];
    PopulateNamedValueType(output_operand_id, *op->add_outputs());
    const OperandInfo& output_operand_info = GetOperandInfo(output_operand_id);
    CHECK_LT(operation.axis, output_operand_info.dimensions.size());
    split_sizes[i] = output_operand_info.dimensions[operation.axis];
  }
  static constexpr char kParamSplitSizes[] = "split_sizes";
  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kParamSplitSizes, Create1DTensorImmediateValue<int32_t>(split_sizes)},
       {kOpParamAxis, CreateScalarImmediateValue(
                          base::checked_cast<int32_t>(operation.axis))}});
}

void GraphBuilderCoreml::AddOperationForTile(
    const mojom::Tile& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  CHECK(context_properties_.data_type_limits.tile_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpTileTypeName);
  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);

  // CoreML expects repetitions to be vector of int32_t.
  base::FixedArray<int32_t> repetitions(operation.repetitions.size());
  base::ranges::transform(
      operation.repetitions, repetitions.begin(),
      [](uint32_t val) { return base::checked_cast<int32_t>(val); });
  SetInputWithValue(*op->mutable_inputs(), kOpParamReps,
                    Create1DTensorImmediateValue<int32_t>(repetitions));

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
}

void GraphBuilderCoreml::AddOperationForTranspose(
    const mojom::Transpose& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);

  CHECK(context_properties_.data_type_limits.transpose_input.Has(
      MILDataTypeToOperandType(input_operand_info.mil_data_type)));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpTransposeTypeName);
  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);

  // CoreML expects permutation to be vector of int32_t.
  static constexpr char kParamPerm[] = "perm";
  base::FixedArray<int32_t> permutation(operation.permutation.size());
  base::ranges::transform(
      operation.permutation, permutation.begin(),
      [](uint32_t val) { return base::checked_cast<int32_t>(val); });
  SetInputWithValue(*op->mutable_inputs(), kParamPerm,
                    Create1DTensorImmediateValue<int32_t>(permutation));

  CoreML::Specification::MILSpec::NamedValueType& output = *op->add_outputs();
  PopulateNamedValueType(operation.output_operand_id, output);
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

  AddOperationForCast(operation.condition_operand_id, bool_condition_operand_id,
                      block);

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpWhereTypeName);

  constexpr char kParamA[] = "a";
  constexpr char kParamB[] = "b";
  constexpr char kParamCond[] = "cond";
  SetInputWithName(*op->mutable_inputs(), kParamA,
                   true_operand_info.coreml_name);
  SetInputWithName(*op->mutable_inputs(), kParamB,
                   false_operand_info.coreml_name);
  SetInputWithName(*op->mutable_inputs(), kParamCond,
                   GetOperandInfo(bool_condition_operand_id).coreml_name);

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::AddConstantImmediateValue(
    uint64_t constant_id,
    CoreML::Specification::MILSpec::Block& block) {
  auto* op = block.add_operations();
  RETURN_IF_ERROR(PopulateConstantOpFromOperand(constant_id, *op));

  google::protobuf::Map<std::string, ::CoreML::Specification::MILSpec::Value>&
      attributes = *op->mutable_attributes();
  std::string name = GetOperandInfo(constant_id).coreml_name;
  attributes["name"] = CreateStringImmediateValue(name);

  auto& constant_operand = constant_operands_->at(constant_id);
  base::span<const uint8_t> value = constant_operand->ByteSpan();

  switch (constant_operand->descriptor().data_type()) {
    case OperandDataType::kFloat32: {
      base::FixedArray<float> floats(value.size() / sizeof(float));
      for (size_t i = 0u; i < floats.size(); ++i) {
        floats[i] = base::FloatFromNativeEndian(
            value.subspan(i * sizeof(float)).first<4u>());
      }
      attributes["val"] = CreateTensorImmediateValue<float>(
          constant_operand->descriptor().shape(), floats);
      break;
    }
    case OperandDataType::kFloat16: {
      base::FixedArray<Float16> float16s(value.size() / sizeof(Float16));
      for (size_t i = 0u; i < float16s.size(); ++i) {
        float16s[i].data = base::U16FromNativeEndian(
            value.subspan(i * sizeof(Float16)).first<2u>());
      }
      attributes["val"] = CreateTensorImmediateValue<Float16>(
          constant_operand->descriptor().shape(), float16s);
      break;
    }
    case OperandDataType::kInt32: {
      base::FixedArray<int32_t> ints(value.size() / sizeof(int32_t));
      for (size_t i = 0u; i < ints.size(); ++i) {
        ints[i] = base::I32FromNativeEndian(
            value.subspan(i * sizeof(int32_t)).first<4u>());
      }
      attributes["val"] = CreateTensorImmediateValue<int32_t>(
          constant_operand->descriptor().shape(), ints);
      break;
    }
    case OperandDataType::kUint32:
    case OperandDataType::kInt64:
    case OperandDataType::kUint64:
    case OperandDataType::kInt8:
    case OperandDataType::kUint8:
    case OperandDataType::kInt4:
    case OperandDataType::kUint4: {
      NOTREACHED() << "Unsupported data type.";
    }
  }
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilderCoreml::AddConstantFileValue(
    uint64_t constant_id,
    uint64_t offset,
    CoreML::Specification::MILSpec::Block& block) {
  auto* op = block.add_operations();
  RETURN_IF_ERROR(PopulateConstantOpFromOperand(constant_id, *op));
  // Blob path is defined in generic Operation.attributes.
  // This follows the actual data structure in
  // https://github.com/apple/coremltools/blob/bba83f43859e087d50c7d764cb132e7d4b427611/coremltools/converters/mil/backend/mil/load.py#L60.
  auto& attributes = *op->mutable_attributes();
  attributes["name"] =
      CreateStringImmediateValue(GetOperandInfo(constant_id).coreml_name);
  CoreML::Specification::MILSpec::Value blob_value{};
  const OperandInfo& operand_info = GetOperandInfo(constant_id);
  PopulateValueTypeFromOperandInfo(operand_info, *blob_value.mutable_type());
  CoreML::Specification::MILSpec::Value::BlobFileValue* blob =
      blob_value.mutable_blobfilevalue();
  blob->set_filename(kWeightsRelativeFilePath);
  blob->set_offset(offset);
  attributes["val"] = std::move(blob_value);
  return base::ok();
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
GraphBuilderCoreml::PopulateConstantOpFromOperand(
    uint64_t constant_id,
    CoreML::Specification::MILSpec::Operation& op) {
  CoreML::Specification::MILSpec::DataType mil_data_type =
      GetOperandInfo(constant_id).mil_data_type;
  CHECK(kFloatsAndInt32DataTypes.contains(mil_data_type));

  op.set_type(kOpConstTypeName);
  PopulateNamedValueType(constant_id, *op.add_outputs());
  return base::ok();
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

base::expected<void, mojom::ErrorPtr>
GraphBuilderCoreml::SetupMlPackageDirStructure() {
  if (!base::CreateDirectory(ml_package_dir())) {
    return NewUnknownError("Fail to create .mlpackage directory.");
  }
  base::FilePath data_dir = ml_package_dir().Append(kMlPackageDataDir);
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
  JSONFileValueSerializer serializer(
      ml_package_dir().Append(kManifestFileName));
  if (!serializer.Serialize(std::move(metadata))) {
    return NewUnknownError("Fail to create Manifest.json for mlpackage.");
  }

  return base::ok();
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
