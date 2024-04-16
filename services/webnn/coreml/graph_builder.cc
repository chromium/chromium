// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/coreml/graph_builder.h"

#include <algorithm>
#include <fstream>
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
#include "base/json/json_file_value_serializer.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/unguessable_token.h"
#include "base/uuid.h"
#include "base/values.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "third_party/coremltools/mlmodel/format/FeatureTypes.pb.h"
#include "third_party/coremltools/mlmodel/format/MIL.pb.h"

namespace webnn::coreml {

// Documentation for the CoreML MIL Ops:
// https://apple.github.io/coremltools/source/coremltools.converters.mil.mil.ops.defs.html
// For the supported OS versions for any OP, the translation between iOS version
// numbers and macOS version numbers is documented here:
// https://github.com/apple/coremltools/blob/bba83f43859e087d50c7d764cb132e7d4b427611/coremltools/converters/mil/_deployment_compatibility.py#L25

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
constexpr char kOpCastTypeName[] = "cast";
constexpr char kOpClipTypeName[] = "clip";
constexpr char kOpConcatTypeName[] = "concat";
constexpr char kOpConv2dTypeName[] = "conv";
constexpr char kOpReluTypeName[] = "relu";
constexpr char kOpSigmoidTypeName[] = "sigmoid";
constexpr char kOpSoftsignTypeName[] = "softsign";
constexpr char kOpTanhTypeName[] = "tanh";
constexpr char kOpTransposeTypeName[] = "transpose";
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
// Pooling operators.
constexpr char kOpAvgPoolTypeName[] = "avg_pool";
constexpr char kOpL2PoolTypeName[] = "l2_pool";
constexpr char kOpMaxPoolTypeName[] = "max_pool";
// Resample2d operators.
constexpr char kOpUpsampleBilinearTypeName[] = "upsample_bilinear";
constexpr char kOpUpsampleNearestNeighborTypeName[] =
    "upsample_nearest_neighbor";

// General op params that are shared across multiple ops.
constexpr char kOpParamX[] = "x";
constexpr char kOpParamY[] = "y";
constexpr char kOpDataTypeName[] = "dtype";

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
    mojom::Operand::DataType data_type) {
  switch (data_type) {
    case mojom::Operand::DataType::kFloat16:
      return BlobDataType::Float16;
    case mojom::Operand::DataType::kFloat32:
      return BlobDataType::Float32;
    case mojom::Operand::DataType::kUint8:
      return BlobDataType::UInt8;
    case mojom::Operand::DataType::kInt8:
      return BlobDataType::Int8;
    case mojom::Operand::DataType::kInt32:
    case mojom::Operand::DataType::kUint32:
    case mojom::Operand::DataType::kInt64:
    case mojom::Operand::DataType::kUint64:
      return std::nullopt;
  }
}

CoreML::Specification::MILSpec::DataType OperandTypeToMILDataType(
    mojom::Operand::DataType data_type) {
  switch (data_type) {
    case mojom::Operand::DataType::kFloat32:
      return CoreML::Specification::MILSpec::DataType::FLOAT32;
    case mojom::Operand::DataType::kFloat16:
      return CoreML::Specification::MILSpec::DataType::FLOAT16;
    case mojom::Operand::DataType::kInt32:
      return CoreML::Specification::MILSpec::DataType::INT32;
    case mojom::Operand::DataType::kUint32:
      return CoreML::Specification::MILSpec::DataType::UINT32;
    case mojom::Operand::DataType::kInt64:
      return CoreML::Specification::MILSpec::DataType::INT64;
    case mojom::Operand::DataType::kUint64:
      return CoreML::Specification::MILSpec::DataType::UINT64;
    case mojom::Operand::DataType::kInt8:
      return CoreML::Specification::MILSpec::DataType::INT8;
    case mojom::Operand::DataType::kUint8:
      return CoreML::Specification::MILSpec::DataType::UINT8;
  }
}

std::string_view DataTypeToString(webnn::mojom::Operand::DataType data_type) {
  switch (data_type) {
    case webnn::mojom::Operand::DataType::kFloat32:
      return "fp32";
    case webnn::mojom::Operand::DataType::kFloat16:
      return "fp16";
    case webnn::mojom::Operand::DataType::kInt32:
      return "int32";
    case webnn::mojom::Operand::DataType::kInt8:
      return "int8";
    case webnn::mojom::Operand::DataType::kUint8:
      return "uint8";
    case webnn::mojom::Operand::DataType::kUint32:
    case webnn::mojom::Operand::DataType::kInt64:
    case webnn::mojom::Operand::DataType::kUint64:
      // The supported data types are an intersection of all the data types
      // in WebNN and the data types supported by the dtype parameter for
      // currently supported CoreML ops. Expand this list as needed for
      // new ops.
      NOTREACHED_NORETURN() << "Unsupported data type.";
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

// Some of the params set as immediate values need to be scalar, e.g. conv2d
// groups. For inputs coming from previous operands they are cast to {1}.
// TODO: handle case by case for casting scalar inputs.
void PopulateValueType(CoreML::Specification::MILSpec::DataType mil_data_type,
                       base::span<const uint32_t> dimensions,
                       CoreML::Specification::MILSpec::ValueType& value_type,
                       bool keep_scalar_type = true) {
  auto* tensor_type = value_type.mutable_tensortype();
  tensor_type->set_datatype(mil_data_type);
  // STRING type is considered scalar.
  if (mil_data_type == CoreML::Specification::MILSpec::DataType::STRING) {
    return;
  }
  if (dimensions.empty()) {
    // Scalar value doesn't need to set rank and dimensions.
    if (keep_scalar_type) {
      return;
    }
    tensor_type->set_rank(1);
    tensor_type->add_dimensions()->mutable_constant()->set_size(1);
    return;
  }

  tensor_type->set_rank(dimensions.size());
  for (auto dimension : dimensions) {
    tensor_type->add_dimensions()->mutable_constant()->set_size(dimension);
  }
}

void PopulateValueTypeFromOperand(
    const mojom::Operand& operand,
    CoreML::Specification::MILSpec::ValueType& value_type) {
  // TODO: change this when any `Operand` needs to keep scalar type.
  // For now it always cast scalar type to {1} dimension because op like `add`
  // don't accept 0D value, and we don't have an op that requires 0D value.
  PopulateValueType(OperandTypeToMILDataType(operand.data_type),
                    operand.dimensions, value_type, /*keep_scalar_type=*/false);
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

}  // namespace

std::string GetCoreMLNameFromInput(std::string_view input_name) {
  // Prefix is added to user provided names to avoid collision with intermediate
  // operands' names
  return base::JoinString({kInputNamePrefix, input_name}, kStringSeparator);
}

std::string GetCoreMLNameFromOutput(std::string_view output_name) {
  // Prefix is added to user provided names to avoid collision with intermediate
  // operands' names
  return base::JoinString({kOutputNamePrefix, output_name}, kStringSeparator);
}

// static
base::expected<std::unique_ptr<GraphBuilder::Result>, mojom::ErrorPtr>
GraphBuilder::CreateAndBuild(const mojom::GraphInfo& graph_info,
                             const base::FilePath& working_directory) {
  // Use a random string for the model package directory, because MLModel
  // compileModelAtURL creates a folder directly in the NSTemporaryDirectory
  // with the name of the .mlmodel file. Using a random string will avoid any
  // potential name collision of that dir.
  base::FilePath ml_package_dir =
      working_directory.AppendASCII(base::UnguessableToken::Create().ToString())
          .AddExtension(kMlPackageExtension);

  base::FilePath data_dir = ml_package_dir.Append(kMlPackageDataDir);

  GraphBuilder graph_builder(graph_info, std::move(ml_package_dir));

  RETURN_IF_ERROR(graph_builder.BuildCoreMLModel());
  RETURN_IF_ERROR(graph_builder.SerializeModel());
  return graph_builder.FinishAndTakeResult();
}

GraphBuilder::GraphBuilder(const mojom::GraphInfo& graph_info,
                           base::FilePath ml_package_dir)
    : graph_info_(graph_info),
      result_(std::make_unique<Result>(std::move(ml_package_dir))) {}

GraphBuilder::~GraphBuilder() = default;

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilder::BuildCoreMLModel() {
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
    RETURN_IF_ERROR(AddInput(input_id, main_function));
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
    switch (operation->which()) {
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
      case mojom::Operation::Tag::kElementWiseBinary: {
        RETURN_IF_ERROR(AddOperationForElementwiseBinary(
            *operation->get_element_wise_binary(), block));
        break;
      }
      case mojom::Operation::Tag::kElementWiseUnary: {
        RETURN_IF_ERROR(AddOperationForElementwiseUnary(
            *operation->get_element_wise_unary(), block));
        break;
      }
      case mojom::Operation::Tag::kPool2d: {
        RETURN_IF_ERROR(AddOperationForPool2d(*operation->get_pool2d(), block));
        break;
      }
      case mojom::Operation::Tag::kRelu: {
        RETURN_IF_ERROR(AddOperationForRelu(*operation->get_relu(), block));
        break;
      }
      case mojom::Operation::Tag::kResample2d: {
        RETURN_IF_ERROR(
            AddOperationForResample2d(*operation->get_resample2d(), block));
        break;
      }
      case mojom::Operation::Tag::kSigmoid: {
        RETURN_IF_ERROR(
            AddOperationForSigmoid(*operation->get_sigmoid(), block));
      break;
      }
      case mojom::Operation::Tag::kSoftsign: {
        RETURN_IF_ERROR(
            AddOperationForSoftsign(*operation->get_softsign(), block));
        break;
      }
      case mojom::Operation::Tag::kTanh: {
        RETURN_IF_ERROR(AddOperationForTanh(*operation->get_tanh(), block));
        break;
      }
      case mojom::Operation::Tag::kTranspose: {
        RETURN_IF_ERROR(
            AddOperationForTranspose(*operation->get_transpose(), block));
        break;
      }
      case mojom::Operation::Tag::kArgMinMax:
      case mojom::Operation::Tag::kBatchNormalization:
      case mojom::Operation::Tag::kElu:
      case mojom::Operation::Tag::kExpand:
      case mojom::Operation::Tag::kGather:
      case mojom::Operation::Tag::kGemm:
      case mojom::Operation::Tag::kGru:
      case mojom::Operation::Tag::kGruCell:
      case mojom::Operation::Tag::kHardSigmoid:
      case mojom::Operation::Tag::kHardSwish:
      case mojom::Operation::Tag::kLayerNormalization:
      case mojom::Operation::Tag::kInstanceNormalization:
      case mojom::Operation::Tag::kLeakyRelu:
      case mojom::Operation::Tag::kLinear:
      case mojom::Operation::Tag::kLstm:
      case mojom::Operation::Tag::kLstmCell:
      case mojom::Operation::Tag::kMatmul:
      case mojom::Operation::Tag::kPad:
      case mojom::Operation::Tag::kPrelu:
      case mojom::Operation::Tag::kReduce:
      case mojom::Operation::Tag::kReshape:
      case mojom::Operation::Tag::kSlice:
      case mojom::Operation::Tag::kSoftmax:
      case mojom::Operation::Tag::kSoftplus:
      case mojom::Operation::Tag::kSplit:
      case mojom::Operation::Tag::kTriangular:
      case mojom::Operation::Tag::kWhere:
        return NewNotSupportedError("This operator is not implemented.");
    }
  }

  // Add output.
  for (uint64_t output_id : graph_info_->output_operands) {
    block.add_outputs(GetCoreMLNameFromOperand(output_id));
    RETURN_IF_ERROR(AddOutput(output_id));
  }
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilder::SerializeModel() {
  base::ElapsedTimer ml_model_write_timer;
  base::FilePath model_file_path = ml_package_dir()
                                       .Append(kMlPackageDataDir)
                                       .Append(kMlPackageModelFileName);
  base::File model_file(model_file_path,
                        base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  if (!model_file.IsValid()) {
    LOG(ERROR) << "Unable to open " << model_file_path << ": "
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

std::unique_ptr<GraphBuilder::Result> GraphBuilder::FinishAndTakeResult() {
  return std::move(result_);
}

base::expected<void, mojom::ErrorPtr> GraphBuilder::WriteWeightsToFile(
    CoreML::Specification::MILSpec::Block& block) {
  base::FilePath weights_file_path = ml_package_dir()
                                         .Append(kMlPackageDataDir)
                                         .Append(kMlPackageWeightsDir)
                                         .Append(kMlPackageWeightsFileName);
  base::File weights_file(weights_file_path,
                          base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  if (!weights_file.IsValid()) {
    LOG(ERROR) << "Unable to open " << weights_file_path << ": "
               << base::File::ErrorToString(weights_file.error_details());
    return NewUnknownError(kWriteWeightsErrorMessage);
  }

  uint64_t current_offset = 0;
  WeightHeader header{
      static_cast<uint32_t>(graph_info_->constant_id_to_buffer_map.size())};
  if (!weights_file.WriteAtCurrentPosAndCheck(
          base::byte_span_from_ref(header))) {
    return NewUnknownError(kWriteWeightsErrorMessage);
  }
  current_offset += sizeof(header);

  for (auto& [key, buffer] : graph_info_->constant_id_to_buffer_map) {
    const mojom::Operand& operand = GetOperand(key);
    if (operand.dimensions.empty()) {
      RETURN_IF_ERROR(AddConstantImmediateValue(key, block));
      continue;
    }

    std::optional<BlobDataType> weight_type =
        OperandTypeToDataTypeInWeightFile(operand.data_type);
    if (!weight_type.has_value()) {
      return NewNotSupportedError("Unsupported constant type.");
    }

    WeightMetadata metadata(weight_type.value(), buffer.size(),
                            current_offset + sizeof(metadata));

    if (!weights_file.WriteAtCurrentPosAndCheck(
            base::byte_span_from_ref(metadata))) {
      return NewUnknownError(kWriteWeightsErrorMessage);
    }

    if (!weights_file.WriteAtCurrentPosAndCheck(base::make_span(buffer))) {
      return NewUnknownError(kWriteWeightsErrorMessage);
    }

    RETURN_IF_ERROR(AddConstantFileValue(key, current_offset, block));
    current_offset += sizeof(metadata);
    current_offset += buffer.size();
    current_offset = base::bits::AlignUp(current_offset, kWeightAlignment);
    if (!weights_file.Seek(base::File::Whence::FROM_BEGIN, current_offset)) {
      return NewUnknownError(kWriteWeightsErrorMessage);
    }
  }
  return base::ok();
}

void GraphBuilder::AddPlaceholderInput(
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

  const mojom::Operand operand{mojom::Operand::Kind::kInput,
                               mojom::Operand::DataType::kFloat16,
                               {1},
                               kPlaceholderInputName};

  CoreML::Specification::MILSpec::NamedValueType& input_for_main_function =
      *main_function.add_inputs();
  input_for_main_function.set_name(kPlaceholderInputName);
  auto& value_type = *input_for_main_function.mutable_type();
  PopulateValueTypeFromOperand(operand, value_type);

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
  PopulateValueTypeFromOperand(operand, output_value_type);
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr> GraphBuilder::AddInput(
    uint64_t input_id,
    CoreML::Specification::MILSpec::Function& main_function) {
  auto* mutable_description = ml_model_.mutable_description();
  auto* feature_description = mutable_description->add_input();
  const mojom::Operand& operand = GetOperand(input_id);
  RETURN_IF_ERROR(PopulateFeatureDescription(input_id, *feature_description));

  CoreML::Specification::MILSpec::NamedValueType& input =
      *main_function.add_inputs();
  PopulateNamedValueType(input_id, input);

  CHECK(input_name_to_id_map()
            .try_emplace(operand.name.value(), input_id)
            .second);
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr> GraphBuilder::AddOutput(
    uint64_t output_id) {
  CHECK(id_to_operand_info_map().contains(output_id));
  auto* mutable_description = ml_model_.mutable_description();
  auto* feature_description = mutable_description->add_output();
  RETURN_IF_ERROR(PopulateFeatureDescription(output_id, *feature_description));
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilder::AddOperationForClamp(
    const mojom::Clamp& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);

  // WebNN's "clamp" maps to the "clip" operator in CoreML:
  // https://apple.github.io/coremltools/source/coremltools.converters.mil.mil.ops.defs.html#coremltools.converters.mil.mil.ops.defs.iOS15.elementwise_unary.clip
  //
  // TODO: crbug.com/332731569 - Use CoreML's support for float16.
  if (input_operand_info.mil_data_type !=
      CoreML::Specification::MILSpec::DataType::FLOAT32) {
    return NewNotSupportedError("Unsupported input datatype.");
  }

  static constexpr char kParamAlpha[] = "alpha";
  static constexpr char kParamBeta[] = "beta";

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpClipTypeName);

  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);
  SetInputsWithValues(
      *op->mutable_inputs(),
      {
          {kParamAlpha, CreateScalarImmediateValue(operation.min_value)},
          {kParamBeta, CreateScalarImmediateValue(operation.max_value)},
      });

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilder::AddOperationForConcat(
    const mojom::Concat& operation,
    CoreML::Specification::MILSpec::Block& block) {
  // Note that BOOL is also supported by CoreML, but WebNN does not have a
  // corresponding BOOL type. See docs here:
  // https://apple.github.io/coremltools/source/coremltools.converters.mil.mil.ops.defs.html#coremltools.converters.mil.mil.ops.defs.iOS15.tensor_operation.concat
  if (base::ranges::any_of(
          operation.input_operand_ids, [&](uint64_t input_operand_id) {
            return !kFloatsAndInt32DataTypes.contains(
                GetOperandInfo(input_operand_id).mil_data_type);
          })) {
    return NewNotSupportedError("Unsupported input datatype.");
  }

  static const char kParamValues[] = "values";
  static const char kParamAxis[] = "axis";
  static const char kParamInterleave[] = "interleave";

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpConcatTypeName);

  for (uint64_t input_operand_id : operation.input_operand_ids) {
    SetInputWithName(*op->mutable_inputs(), kParamValues,
                     GetOperandInfo(input_operand_id).coreml_name);
  }
  SetInputsWithValues(
      *op->mutable_inputs(),
      {{kParamAxis, CreateScalarImmediateValue(
                        base::checked_cast<int32_t>(operation.axis))},
       {kParamInterleave, CreateScalarImmediateValue(false)}});

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

base::expected<void, mojom::ErrorPtr>
GraphBuilder::AddOperationForElementwiseBinary(
    const mojom::ElementWiseBinary& operation,
    CoreML::Specification::MILSpec::Block& block) {
  CoreML::Specification::MILSpec::Operation* op = block.add_operations();

  const OperandInfo& lhs_operand_info =
      GetOperandInfo(operation.lhs_operand_id);
  const OperandInfo& rhs_operand_info = GetOperandInfo(
      operation.rhs_operand_id);

  if (!kFloatsAndInt32DataTypes.contains(lhs_operand_info.mil_data_type) ||
      !kFloatsAndInt32DataTypes.contains(rhs_operand_info.mil_data_type)) {
    return NewNotSupportedError("Unsupported input datatype.");
  }

  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   lhs_operand_info.coreml_name);
  SetInputWithName(*op->mutable_inputs(), kOpParamY,
                   rhs_operand_info.coreml_name);

  bool is_logical_binary_operation = false;
  switch (operation.kind) {
    case mojom::ElementWiseBinary::Kind::kAdd: {
      op->set_type(kOpAddTypeName);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kDiv: {
      op->set_type(kOpDivideTypeName);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kMul: {
      op->set_type(kOpMultiplyTypeName);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kSub: {
      op->set_type(kOpSubtractTypeName);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kMax: {
      op->set_type(kOpMaximumTypeName);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kMin: {
      op->set_type(kOpMinimumTypeName);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kPow: {
      op->set_type(kOpPowerTypeName);
      break;
    }
    case mojom::ElementWiseBinary::Kind::kEqual: {
      op->set_type(kOpLogicalEqual);
      is_logical_binary_operation = true;
      break;
    }
    case mojom::ElementWiseBinary::Kind::kGreater: {
      op->set_type(kOpLogicalGreater);
      is_logical_binary_operation = true;
      break;
    }
    case mojom::ElementWiseBinary::Kind::kGreaterOrEqual: {
      op->set_type(kOpLogicalGreaterEqual);
      is_logical_binary_operation = true;
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLesser: {
      op->set_type(kOpLogicalLess);
      is_logical_binary_operation = true;
      break;
    }
    case mojom::ElementWiseBinary::Kind::kLesserOrEqual: {
      op->set_type(kOpLogicalLessEqual);
      is_logical_binary_operation = true;
      break;
    }
  }

  if (is_logical_binary_operation) {
    // The output of logical binary ops need to be cast from a boolean
    // tensor that CoreML provides to an UInt8 that WebNN expects.

    std::string internal_output_name = GenerateCoreMLNameForInternalOperand();
    auto* named_value_type = op->add_outputs();
    named_value_type->set_name(internal_output_name);
    auto& value_type = *named_value_type->mutable_type();
    PopulateValueTypeFromOperand(GetOperand(operation.output_operand_id),
                                 value_type);
    value_type.mutable_tensortype()->set_datatype(
        CoreML::Specification::MILSpec::DataType::BOOL);

    // Note: Input data type passed in here is kUint8, since the actual
    // datatype bool cannot be represented as an Operand::DataType.
    RETURN_IF_ERROR(
        AddOperationForCast(internal_output_name, operation.output_operand_id,
                            webnn::mojom::Operand::DataType::kUint8, block));
  } else {
    PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  }
  return base::ok();
}

base::expected<void, mojom::ErrorPtr>
GraphBuilder::AddOperationForElementwiseUnary(
    const mojom::ElementWiseUnary& operation,
    CoreML::Specification::MILSpec::Block& block) {
  switch (operation.kind) {
    case mojom::ElementWiseUnary::Kind::kCast: {
      const OperandInfo& input = GetOperandInfo(operation.input_operand_id);
      RETURN_IF_ERROR(AddOperationForCast(input.coreml_name,
                                          operation.output_operand_id,
                                          input.data_type, block));
      break;
    }
    default:
      return NewNotSupportedError("Unimplemented Unary Operator.");
  }
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilder::AddOperationForCast(
    const std::string& input_name,
    uint64_t output_operand_id,
    webnn::mojom::Operand::DataType input_data_type,
    CoreML::Specification::MILSpec::Block& block) {
  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  // https://apple.github.io/coremltools/source/coremltools.converters.mil.mil.ops.defs.html#coremltools.converters.mil.mil.ops.defs.iOS17.elementwise_unary.cast
  // Input can be one of the following types: int8, uint8, int16, uint16,
  // int32, fp16, fp32, or bool.
  static constexpr auto kSupportedCastOpsTypes =
      base::MakeFixedFlatSet<webnn::mojom::Operand::DataType>(
          {webnn::mojom::Operand::DataType::kFloat32,
           webnn::mojom::Operand::DataType::kFloat16,
           webnn::mojom::Operand::DataType::kInt32,
           webnn::mojom::Operand::DataType::kInt8,
           webnn::mojom::Operand::DataType::kUint8});
  if (!kSupportedCastOpsTypes.contains(input_data_type)) {
    return NewNotSupportedError("Unsupported input datatype.");
  }
  const mojom::Operand::DataType& output_data_type =
      GetOperand(output_operand_id).data_type;
  if (!kSupportedCastOpsTypes.contains(output_data_type)) {
    return NewNotSupportedError("Unsupported output datatype.");
  }
  SetInputWithName(*op->mutable_inputs(), kOpParamX, input_name);
  op->set_type(kOpCastTypeName);
  SetInputWithValue(
      *op->mutable_inputs(), kOpDataTypeName,
      CreateStringImmediateValue(DataTypeToString(output_data_type)));
  PopulateNamedValueType(output_operand_id, *op->add_outputs());
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilder::AddOperationForPool2d(
    const mojom::Pool2d& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);

  if (!kFloatDataTypes.contains(input_operand_info.mil_data_type)) {
    return NewNotSupportedError("Unsupported input datatype.");
  }

  if (operation.layout != mojom::InputOperandLayout::kChannelsFirst) {
    // TODO: crbug.com/334914466 - Support channels-last by adding transposes.
    return NewNotSupportedError("Unsupported input layout.");
  }

  if (operation.dilations->height != 1 || operation.dilations->width != 1) {
    // TODO: crbug.com/334914466 - Support dilations.
    return NewNotSupportedError("Unsupported dilations.");
  }

  static constexpr char kParamKernelSizes[] = "kernel_sizes";
  static constexpr char kParamStrides[] = "strides";
  static constexpr char kParamPadType[] = "pad_type";
  static constexpr char kParamPadTypeValue[] = "custom";
  static constexpr char kParamPad[] = "pad";
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
       {kParamPad, Create1DTensorImmediateValue<int32_t>(pad)},
       // TODO: crbug.com/334914466 - Support `ceil_mode` by calculating the
       // expected output shape and comparing it to the shape of the output
       // operand. Note that Core ML requires padding to be symmetric if
       // `ceil_mode` is true.
       {kParamCeilMode, CreateScalarImmediateValue(false)}});

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilder::AddOperationForRelu(
    const mojom::Relu& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);

  if (!kFloatDataTypes.contains(input_operand_info.mil_data_type)) {
    return NewNotSupportedError("Unsupported input datatype.");
  }

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpReluTypeName);

  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilder::AddOperationForResample2d(
    const mojom::Resample2d& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);

  // WebNN's "resample2d" maps to variants of the "upsample" operator in CoreML:
  // https://apple.github.io/coremltools/source/coremltools.converters.mil.mil.ops.defs.html#coremltools.converters.mil.mil.ops.defs.iOS15.image_resizing.upsample_bilinear
  // https://apple.github.io/coremltools/source/coremltools.converters.mil.mil.ops.defs.html#coremltools.converters.mil.mil.ops.defs.iOS15.image_resizing.upsample_nearest_neighbor
  if (!kFloatDataTypes.contains(input_operand_info.mil_data_type)) {
    return NewNotSupportedError("Unsupported input datatype.");
  }

  const std::array<size_t, 2> supported_axes = {2, 3};
  if (!base::ranges::equal(operation.axes, supported_axes)) {
    // TODO: crbug.com/334914468 - Support axes of {0, 1} and {1, 2}.
    return NewNotSupportedError("Unsupported axes.");
  }

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

base::expected<void, mojom::ErrorPtr> GraphBuilder::AddOperationForSigmoid(
    const mojom::Sigmoid& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);

  if (!kFloatDataTypes.contains(input_operand_info.mil_data_type)) {
    return NewNotSupportedError("Unsupported input datatype.");
  }

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpSigmoidTypeName);

  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilder::AddOperationForSoftsign(
    const mojom::Softsign& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);

  if (!kFloatDataTypes.contains(input_operand_info.mil_data_type)) {
    return NewNotSupportedError("Unsupported input datatype.");
  }

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpSoftsignTypeName);

  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilder::AddOperationForTanh(
    const mojom::Tanh& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);

  if (!kFloatDataTypes.contains(input_operand_info.mil_data_type)) {
    return NewNotSupportedError("Unsupported input datatype.");
  }

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpTanhTypeName);

  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilder::AddOperationForTranspose(
    const mojom::Transpose& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);
  // Note that BOOL is also supported by CoreML, but WebNN does not have a
  // corresponding BOOL type. See docs here:
  // https://apple.github.io/coremltools/source/coremltools.converters.mil.mil.ops.defs.html#coremltools.converters.mil.mil.ops.defs.iOS15.tensor_operation.transpose
  if (!kFloatsAndInt32DataTypes.contains(input_operand_info.mil_data_type)) {
    return NewNotSupportedError("Unsupported input datatype.");
  }

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpTransposeTypeName);
  SetInputWithName(*op->mutable_inputs(), kOpParamX,
                   input_operand_info.coreml_name);

  // CoreML expects permutation to be vector of int32_t.
  constexpr char kParamPerm[] = "perm";
  std::vector<int32_t> permutation;
  base::ranges::transform(
      operation.permutation, std::back_inserter(permutation),
      [](uint32_t val) { return base::checked_cast<int32_t>(val); });
  SetInputWithValue(*op->mutable_inputs(), kParamPerm,
                    Create1DTensorImmediateValue<int32_t>(permutation));

  CoreML::Specification::MILSpec::NamedValueType& output = *op->add_outputs();
  PopulateNamedValueType(operation.output_operand_id, output);
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilder::AddOperationForConv2d(
    const mojom::Conv2d& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand = GetOperandInfo(operation.input_operand_id);

  if (operation.kind != mojom::Conv2d::Kind::kDirect) {
    // TODO: support transposed conv2d.
    return NewNotSupportedError("Unsupported conv2d kind.");
  }

  if (!kFloatDataTypes.contains(input_operand.mil_data_type)) {
    return NewNotSupportedError("Unsupported input datatype.");
  }

  if (operation.input_layout != mojom::InputOperandLayout::kChannelsFirst) {
    // TODO: support channels last by adding transposes.
    return NewNotSupportedError("Unsupported input layout.");
  }

  if (!operation.activation.is_null()) {
    // TODO: support by adding additional activation layer.
    return NewNotSupportedError("activation is not supported.");
  }

  static constexpr char kParamWeight[] = "weight";
  static constexpr char kParamStrides[] = "strides";
  static constexpr char kParamPadType[] = "pad_type";
  static constexpr char kParamPadTypeValue[] = "custom";
  static constexpr char kParamPad[] = "pad";
  static constexpr char kParamDilations[] = "dilations";
  static constexpr char kParamGroups[] = "groups";
  static constexpr char kParamBias[] = "bias";

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpConv2dTypeName);
  google::protobuf::Map<std::string,
                        ::CoreML::Specification::MILSpec::Argument>& inputs =
      (*op->mutable_inputs());
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
       {kParamPad, Create1DTensorImmediateValue<int32_t>(pad)},
       {kParamDilations, Create1DTensorImmediateValue<int32_t>(dilations)},
       {kParamGroups, CreateScalarImmediateValue(
                          base::checked_cast<int32_t>(operation.groups))}});
  if (operation.bias_operand_id) {
    SetInputWithName(
        inputs, kParamBias,
        GetOperandInfo(operation.bias_operand_id.value()).coreml_name);
  }
  CoreML::Specification::MILSpec::NamedValueType& output = *op->add_outputs();
  PopulateNamedValueType(operation.output_operand_id, output);
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilder::AddConstantImmediateValue(
    uint64_t constant_id,
    CoreML::Specification::MILSpec::Block& block) {
  auto* op = block.add_operations();
  RETURN_IF_ERROR(PopulateConstantOpFromOperand(constant_id, *op));

  google::protobuf::Map<std::string, ::CoreML::Specification::MILSpec::Value>&
      attributes = *op->mutable_attributes();
  std::string name = GetCoreMLNameFromOperand(constant_id);
  attributes["name"] = CreateStringImmediateValue(name);

  base::span<const uint8_t> value(
      graph_info_->constant_id_to_buffer_map.at(constant_id));
  const mojom::Operand& operand = GetOperand(constant_id);
  // Convert to {1} for 0D constants to be consistent with the op output type.
  std::vector<uint32_t> dimensions = operand.dimensions.empty()
                                         ? std::vector<uint32_t>({1})
                                         : operand.dimensions;
  switch (operand.data_type) {
    case mojom::Operand::DataType::kFloat32: {
      std::vector<float> floats(value.size() / sizeof(float));
      for (size_t i = 0u; i < floats.size(); ++i) {
        floats[i] = base::FloatFromNativeEndian(
            value.subspan(i * sizeof(float)).first<4u>());
      }
      attributes["val"] = CreateTensorImmediateValue<float>(dimensions, floats);
      break;
    }
    case mojom::Operand::DataType::kFloat16: {
      std::vector<Float16> float16s(value.size() / sizeof(Float16));
      for (size_t i = 0u; i < float16s.size(); ++i) {
        float16s[i].data = base::U16FromNativeEndian(
            value.subspan(i * sizeof(Float16)).first<2u>());
      }
      attributes["val"] =
          CreateTensorImmediateValue<Float16>(dimensions, float16s);
      break;
    }
    case mojom::Operand::DataType::kInt32: {
      std::vector<int32_t> ints(value.size() / sizeof(int32_t));
      for (size_t i = 0u; i < ints.size(); ++i) {
        ints[i] = base::I32FromNativeEndian(
            value.subspan(i * sizeof(int32_t)).first<4u>());
      }
      attributes["val"] = CreateTensorImmediateValue<int32_t>(dimensions, ints);
      break;
    }
    case mojom::Operand::DataType::kUint32:
    case mojom::Operand::DataType::kInt64:
    case mojom::Operand::DataType::kUint64:
    case mojom::Operand::DataType::kInt8:
    case mojom::Operand::DataType::kUint8: {
      NOTREACHED_NORETURN() << "Unsupported data type.";
    }
  }
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilder::AddConstantFileValue(
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
  const mojom::Operand& operand = GetOperand(constant_id);
  PopulateValueTypeFromOperand(operand, *blob_value.mutable_type());
  CoreML::Specification::MILSpec::Value::BlobFileValue* blob =
      blob_value.mutable_blobfilevalue();
  blob->set_filename(kWeightsRelativeFilePath);
  blob->set_offset(offset);
  attributes["val"] = std::move(blob_value);
  return base::ok();
}

const mojom::Operand& GraphBuilder::GetOperand(uint64_t operand_id) const {
  return *graph_info_->id_to_operand_map.at(operand_id);
}

[[nodiscard]] const GraphBuilder::OperandInfo& GraphBuilder::GetOperandInfo(
    uint64_t operand_id) const {
  return result_->GetOperandInfo(operand_id);
}

base::expected<void, mojom::ErrorPtr>
GraphBuilder::PopulateConstantOpFromOperand(
    uint64_t constant_id,
    CoreML::Specification::MILSpec::Operation& op) {
  if (!kFloatsAndInt32DataTypes.contains(
          GetOperandInfo(constant_id).mil_data_type)) {
    return NewNotSupportedError("Unsupported input datatype.");
  }

  op.set_type(kOpConstTypeName);
  PopulateNamedValueType(constant_id, *op.add_outputs());
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilder::PopulateFeatureDescription(
    uint64_t operand_id,
    ::CoreML::Specification::FeatureDescription& feature_description) {
  const mojom::Operand& operand = GetOperand(operand_id);
  auto* feature_type = feature_description.mutable_type();
  auto* array_feature_type = feature_type->mutable_multiarraytype();
  switch (operand.data_type) {
    case mojom::Operand::DataType::kFloat32:
      array_feature_type->set_datatype(
          CoreML::Specification::ArrayFeatureType_ArrayDataType::
              ArrayFeatureType_ArrayDataType_FLOAT32);
      break;
    case mojom::Operand::DataType::kFloat16:
      array_feature_type->set_datatype(
          CoreML::Specification::ArrayFeatureType_ArrayDataType::
              ArrayFeatureType_ArrayDataType_FLOAT16);
      break;
    case mojom::Operand::DataType::kInt32:
      array_feature_type->set_datatype(
          CoreML::Specification::ArrayFeatureType_ArrayDataType::
              ArrayFeatureType_ArrayDataType_INT32);
      break;
    case mojom::Operand::DataType::kUint32:
    case mojom::Operand::DataType::kInt64:
    case mojom::Operand::DataType::kUint64:
    case mojom::Operand::DataType::kInt8:
    case mojom::Operand::DataType::kUint8:
      // CoreML only supports limited data types as input/output for a
      // model. Within the model wider set of data types are supported.
      return NewNotSupportedError("Unsupported datatype at model boundary.");
  }
  // FeatureDescriptions are about input and output features, WebNN allows
  // scalar operands to have empty dimensions. At the input and output layers
  // these can be treated as a 1D tensor to satisfy CoreML's requirement of
  // having atleast 1 dimension.
  if (operand.dimensions.empty()) {
    array_feature_type->add_shape(1);
  } else {
    for (int dimension : operand.dimensions) {
      array_feature_type->add_shape(dimension);
    }
  }
  feature_description.mutable_name()->assign(
      GetCoreMLNameFromOperand(operand_id));
  return base::ok();
}

std::string GraphBuilder::GenerateCoreMLNameForInternalOperand() {
  // Prefix is added to internal operands generated for WebNN operations that
  // need to be decomposed into multiple CoreML operations.
  return base::JoinString(
      {kInternalNamePrefix, base::NumberToString(internal_operand_id_++)},
      kStringSeparator);
}

void GraphBuilder::PopulateNamedValueType(
    uint64_t operand_id,
    CoreML::Specification::MILSpec::NamedValueType& named_value_type) {
  named_value_type.set_name(GetCoreMLNameFromOperand(operand_id));
  auto& value_type = *named_value_type.mutable_type();
  PopulateValueTypeFromOperand(GetOperand(operand_id), value_type);
}

void GraphBuilder::PopulateNamedValueType(
    std::string_view name,
    CoreML::Specification::MILSpec::DataType mil_data_type,
    base::span<const uint32_t> dimensions,
    CoreML::Specification::MILSpec::NamedValueType& named_value_type) {
  named_value_type.set_name(name.data());
  auto& value_type = *named_value_type.mutable_type();
  PopulateValueType(mil_data_type, dimensions, value_type);
}

void GraphBuilder::UpdateCoreMLInputInfoMap(uint64_t operand_id) {
  // WebNN allows 0D scalar operands to have empty dimensions.
  // At the input and output nodes, these can be treated as a 1D tensor to
  // satisfy CoreML's requirement of having at least 1 dimension.
  const mojom::Operand& operand = GetOperand(operand_id);
  const CoreML::Specification::MILSpec::DataType mil_data_type =
      OperandTypeToMILDataType(operand.data_type);
  CHECK(id_to_operand_info_map()
            .try_emplace(operand_id,
                         OperandInfo(GetCoreMLNameFromOperand(operand_id),
                                     operand.dimensions.empty()
                                         ? std::vector<uint32_t>({1})
                                         : operand.dimensions,
                                     operand.data_type, mil_data_type))
            .second);
}

base::expected<void, mojom::ErrorPtr>
GraphBuilder::SetupMlPackageDirStructure() {
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

std::string GraphBuilder::GetCoreMLNameFromOperand(uint64_t operand_id) {
  const mojom::Operand& operand = GetOperand(operand_id);
  // CoreML doesn't allow op output names to start with numbers, so "var_"
  // prefixes are added.
  switch (operand.kind) {
    case mojom::Operand::Kind::kInput:
      CHECK(operand.name.has_value());
      return GetCoreMLNameFromInput(operand.name.value());
    case mojom::Operand::Kind::kConstant:
      return base::JoinString(
          {kIntermediateOperandPrefix, base::NumberToString(operand_id)},
          kStringSeparator);
    case mojom::Operand::Kind::kOutput:
      if (operand.name.has_value()) {
        return GetCoreMLNameFromOutput(operand.name.value());
      } else {
        // Intermediate outputs don't have names so use operand_id instead.
        return base::JoinString(
            {kIntermediateOperandPrefix, base::NumberToString(operand_id)},
            kStringSeparator);
      }
  }
}

GraphBuilder::OperandInfo::OperandInfo(
    std::string coreml_name,
    std::vector<uint32_t> dimensions,
    mojom::Operand::DataType data_type,
    CoreML::Specification::MILSpec::DataType mil_data_type)
    : coreml_name(std::move(coreml_name)),
      dimensions(std::move(dimensions)),
      data_type(data_type),
      mil_data_type(mil_data_type) {}

GraphBuilder::OperandInfo::OperandInfo() = default;
GraphBuilder::OperandInfo::~OperandInfo() = default;
GraphBuilder::OperandInfo::OperandInfo(OperandInfo&) = default;
GraphBuilder::OperandInfo::OperandInfo(OperandInfo&&) = default;

GraphBuilder::Result::Result(base::FilePath ml_package_dir)
    : ml_package_dir(std::move(ml_package_dir)) {}
GraphBuilder::Result::~Result() = default;

const GraphBuilder::OperandInfo& GraphBuilder::Result::FindInputOperandInfo(
    const std::string& input_name) const {
  auto it = input_name_to_id_map.find(input_name);
  return GetOperandInfo(it->second);
}

const base::FilePath& GraphBuilder::Result::GetModelFilePath() {
  return ml_package_dir;
}

const GraphBuilder::OperandInfo& GraphBuilder::Result::GetOperandInfo(
    uint64_t operand_id) const {
  auto it = id_to_operand_info_map.find(operand_id);
  CHECK(it != id_to_operand_info_map.end());
  return it->second;
}

}  // namespace webnn::coreml
