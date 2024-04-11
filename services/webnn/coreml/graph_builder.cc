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

constexpr char kWriteFileErrorMessage[] = "Failed to write constant to file.";

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
// Used when some op parameters are passed as values instead of operands.
constexpr char kConstValuePrefix[] = "value";
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

// General op params that are shared across multiple ops.
constexpr char kOpParamX[] = "x";
constexpr char kOpParamY[] = "y";
constexpr char kOpDataTypeName[] = "dtype";

// Hard coded path used in the model file to point at the weight path.
constexpr char kWeightsRelativeFilePath[] = "@model_path/weights/weights.bin";

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

CoreML::Specification::MILSpec::Value CreateStringValue(
    std::string_view value) {
  CoreML::Specification::MILSpec::Value scalar_value{};
  scalar_value.mutable_type()->mutable_tensortype()->set_datatype(
      CoreML::Specification::MILSpec::DataType::STRING);
  scalar_value.mutable_immediatevalue()
      ->mutable_tensor()
      ->mutable_strings()
      ->add_values(value.data());
  return scalar_value;
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
struct MilDataTypeMap<int32_t> {
  static constexpr CoreML::Specification::MILSpec::DataType value =
      CoreML::Specification::MILSpec::DataType::INT32;
};
template <>
struct MilDataTypeMap<uint32_t> {
  static constexpr CoreML::Specification::MILSpec::DataType value =
      CoreML::Specification::MILSpec::DataType::UINT32;
};
template <>
struct MilDataTypeMap<int64_t> {
  static constexpr CoreML::Specification::MILSpec::DataType value =
      CoreML::Specification::MILSpec::DataType::INT64;
};
template <>
struct MilDataTypeMap<uint64_t> {
  static constexpr CoreML::Specification::MILSpec::DataType value =
      CoreML::Specification::MILSpec::DataType::UINT64;
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
void SetTensorValueForImmediateValue<uint32_t>(
    CoreML::Specification::MILSpec::TensorValue& tensor,
    base::span<const uint32_t> value) {
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
void SetTensorValueForImmediateValue<int64_t>(
    CoreML::Specification::MILSpec::TensorValue& tensor,
    base::span<const int64_t> value) {
  for (auto next : value) {
    tensor.mutable_longints()->add_values(next);
  }
}
template <>
void SetTensorValueForImmediateValue<uint64_t>(
    CoreML::Specification::MILSpec::TensorValue& tensor,
    base::span<const uint64_t> value) {
  for (auto next : value) {
    tensor.mutable_longints()->add_values(next);
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

  if (!graph_builder.SerializeModel()) {
    return NewUnknownError("Failed to serialize CoreML model.");
  }

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
      case mojom::Operation::Tag::kRelu: {
        RETURN_IF_ERROR(AddOperationForRelu(*operation->get_relu(), block));
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
      case mojom::Operation::Tag::kPool2d:
      case mojom::Operation::Tag::kPrelu:
      case mojom::Operation::Tag::kReduce:
      case mojom::Operation::Tag::kResample2d:
      case mojom::Operation::Tag::kReshape:
      case mojom::Operation::Tag::kSigmoid:
      case mojom::Operation::Tag::kSlice:
      case mojom::Operation::Tag::kSoftmax:
      case mojom::Operation::Tag::kSoftplus:
      case mojom::Operation::Tag::kSoftsign:
      case mojom::Operation::Tag::kSplit:
      case mojom::Operation::Tag::kTanh:
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

bool GraphBuilder::SerializeModel() {
  base::ElapsedTimer ml_model_write_timer;
  // This will always overwrite if there is an existing file.
  std::fstream model_file(ml_package_dir()
                              .Append(kMlPackageDataDir)
                              .Append(kMlPackageModelFileName)
                              .value(),
                          std::ios::out | std::ios::binary);
  bool result = ml_model_.SerializeToOstream(&model_file);
  UMA_HISTOGRAM_MEDIUM_TIMES("WebNN.CoreML.TimingMs.MLModelWrite",
                             ml_model_write_timer.Elapsed());
  return result;
}

std::unique_ptr<GraphBuilder::Result> GraphBuilder::FinishAndTakeResult() {
  return std::move(result_);
}

base::expected<void, mojom::ErrorPtr> GraphBuilder::WriteWeightsToFile(
    CoreML::Specification::MILSpec::Block& block) {
  base::File weights_file(ml_package_dir()
                              .Append(kMlPackageDataDir)
                              .Append(kMlPackageWeightsDir)
                              .Append(kMlPackageWeightsFileName),
                          base::File::FLAG_CREATE | base::File::FLAG_WRITE);

  uint64_t current_offset = 0;
  WeightHeader header{
      static_cast<uint32_t>(graph_info_->constant_id_to_buffer_map.size())};
  if (!weights_file.WriteAtCurrentPosAndCheck(
          base::byte_span_from_ref(header))) {
    return NewUnknownError(kWriteFileErrorMessage);
  }
  current_offset += sizeof(header);

  for (auto& [key, buffer] : graph_info_->constant_id_to_buffer_map) {
    const mojom::Operand& operand = GetOperand(key);
    if (operand.dimensions.empty()) {
      AddConstantImmediateValue(key, block);
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
      return NewUnknownError(kWriteFileErrorMessage);
    }

    if (!weights_file.WriteAtCurrentPosAndCheck(base::make_span(buffer))) {
      return NewUnknownError(kWriteFileErrorMessage);
    }

    AddConstantFileValue(key, current_offset, block);
    current_offset += sizeof(metadata);
    current_offset += buffer.size();
    current_offset = base::bits::AlignUp(current_offset, kWeightAlignment);
    if (!weights_file.Seek(base::File::Whence::FROM_BEGIN, current_offset)) {
      return NewUnknownError(kWriteFileErrorMessage);
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
  PopulateValueType(operand, value_type);

  // The model compute only succeeds when the placeholder is used in one op.
  CoreML::Specification::MILSpec::Operation* placeholder_op =
      block.add_operations();
  (*placeholder_op->mutable_inputs())[kOpParamX].add_arguments()->set_name(
      kPlaceholderInputName);
  (*placeholder_op->mutable_inputs())[kOpParamY].add_arguments()->set_name(
      kPlaceholderInputName);
  placeholder_op->set_type(kOpAddTypeName);
  CoreML::Specification::MILSpec::NamedValueType& outputs =
      *placeholder_op->add_outputs();
  outputs.set_name(kPlaceholderOuputName);
  auto& output_value_type = *outputs.mutable_type();
  PopulateValueType(operand, output_value_type);
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

  // Clip's min and max values are passed as constant scalar tensors.
  const std::string alpha_op_output_name =
      GetCoreMLNameForParam(operation.output_operand_id, kParamAlpha);
  const std::string beta_op_output_name =
      GetCoreMLNameForParam(operation.output_operand_id, kParamBeta);

  AddScalarImmediateValue(block, alpha_op_output_name, operation.min_value);
  AddScalarImmediateValue(block, beta_op_output_name, operation.max_value);

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpClipTypeName);

  (*op->mutable_inputs())[kOpParamX].add_arguments()->set_name(
      input_operand_info.coreml_name);
  (*op->mutable_inputs())[kParamAlpha].add_arguments()->set_name(
      alpha_op_output_name);
  (*op->mutable_inputs())[kParamBeta].add_arguments()->set_name(
      beta_op_output_name);

  PopulateNamedValueType(operation.output_operand_id, *op->add_outputs());
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilder::AddOperationForConcat(
    const mojom::Concat& operation,
    CoreML::Specification::MILSpec::Block& block) {
  // Note that BOOL is also supported by CoreML, but WebNN does not have a
  // corresponding BOOL type. See docs here:
  // https://apple.github.io/coremltools/source/coremltools.converters.mil.mil.ops.defs.html#coremltools.converters.mil.mil.ops.defs.iOS15.tensor_operation.concat
  static constexpr auto kSupportedConcatOpsTypes =
      base::MakeFixedFlatSet<CoreML::Specification::MILSpec::DataType>(
          {CoreML::Specification::MILSpec::DataType::FLOAT16,
           CoreML::Specification::MILSpec::DataType::FLOAT32,
           CoreML::Specification::MILSpec::DataType::INT32});
  if (base::ranges::any_of(
          operation.input_operand_ids, [&](uint64_t input_operand_id) {
            return !kSupportedConcatOpsTypes.contains(
                GetOperandInfo(input_operand_id).mil_data_type);
          })) {
    return NewNotSupportedError("Unsupported input datatype.");
  }

  static const char kParamValues[] = "values";
  static const char kParamAxis[] = "axis";
  static const char kParamInterleave[] = "interleave";

  const std::string axis_op_output_name =
      GetCoreMLNameForParam(operation.output_operand_id, kParamAxis);
  AddScalarImmediateValue(block, axis_op_output_name,
                          base::checked_cast<int32_t>(operation.axis));

  const std::string interleave_op_output_name =
      GetCoreMLNameForParam(operation.output_operand_id, kParamInterleave);
  AddScalarImmediateValue(block, interleave_op_output_name, false);

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpConcatTypeName);

  google::protobuf::Map<std::string,
                        ::CoreML::Specification::MILSpec::Argument>& inputs =
      (*op->mutable_inputs());

  for (uint64_t input_operand_id : operation.input_operand_ids) {
    inputs[kParamValues].add_arguments()->set_name(
        GetOperandInfo(input_operand_id).coreml_name);
  }
  inputs[kParamAxis].add_arguments()->set_name(axis_op_output_name);
  inputs[kParamInterleave].add_arguments()->set_name(interleave_op_output_name);

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
  // Input keys (x, y) and supported types are defined in coremltools.
  // https://github.com/apple/coremltools/blob/b416f36054af9ca9d10b2d74ba215d0454677ca0/coremltools/converters/mil/mil/ops/defs/iOS15/elementwise_binary.py#L33
  static constexpr auto kSupportedBinaryOpsTypes =
      base::MakeFixedFlatSet<CoreML::Specification::MILSpec::DataType>(
          {CoreML::Specification::MILSpec::DataType::FLOAT16,
           CoreML::Specification::MILSpec::DataType::FLOAT32,
           CoreML::Specification::MILSpec::DataType::INT32});

  if (!kSupportedBinaryOpsTypes.contains(lhs_operand_info.mil_data_type) ||
      !kSupportedBinaryOpsTypes.contains(rhs_operand_info.mil_data_type)) {
    return NewNotSupportedError("Unsupported input datatype.");
  }

  (*op->mutable_inputs())[kOpParamX].add_arguments()->set_name(
      lhs_operand_info.coreml_name);
  (*op->mutable_inputs())[kOpParamY].add_arguments()->set_name(
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
    PopulateValueType(
        *graph_info_->id_to_operand_map.at(operation.output_operand_id),
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
  (*op->mutable_inputs())[kOpParamX].add_arguments()->set_name(input_name);
  op->set_type(kOpCastTypeName);
  (*(*op->mutable_inputs())[kOpDataTypeName].add_arguments()->mutable_value()) =
      CreateStringValue(DataTypeToString(output_data_type));
  PopulateNamedValueType(output_operand_id, *op->add_outputs());
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilder::AddOperationForRelu(
    const mojom::Relu& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand_info =
      GetOperandInfo(operation.input_operand_id);

  static constexpr auto kSupportedReluOpsTypes =
      base::MakeFixedFlatSet<CoreML::Specification::MILSpec::DataType>(
          {CoreML::Specification::MILSpec::DataType::FLOAT16,
           CoreML::Specification::MILSpec::DataType::FLOAT32});
  if (!kSupportedReluOpsTypes.contains(input_operand_info.mil_data_type)) {
    return NewNotSupportedError("Unsupported input datatype.");
  }

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpReluTypeName);

  (*op->mutable_inputs())[kOpParamX].add_arguments()->set_name(
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
  static constexpr auto kSupportedTransposeOpsTypes =
      base::MakeFixedFlatSet<CoreML::Specification::MILSpec::DataType>(
          {CoreML::Specification::MILSpec::DataType::FLOAT16,
           CoreML::Specification::MILSpec::DataType::FLOAT32,
           CoreML::Specification::MILSpec::DataType::INT32});
  if (!kSupportedTransposeOpsTypes.contains(input_operand_info.mil_data_type)) {
    return NewNotSupportedError("Unsupported input datatype.");
  }

  constexpr char kParamPerm[] = "perm";
  // Permutation is passed as a vector, adds a const op for this, then uses the
  // const's output as the transpose's input. This op needs to be added to
  // `block` before the transpose op.
  const std::string perm_op_output_name =
      GetCoreMLNameForParam(operation.output_operand_id, kParamPerm);
  // CoreML expects permutation to be vector of int32_t.
  std::vector<int32_t> permutation;
  base::ranges::transform(
      operation.permutation, std::back_inserter(permutation),
      [](uint32_t val) { return base::checked_cast<int32_t>(val); });
  AddTensorImmediateValue<int32_t>(
      block, perm_op_output_name,
      base::span<const uint32_t>(
          {base::checked_cast<uint32_t>(permutation.size())}),
      permutation);

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpTransposeTypeName);
  (*op->mutable_inputs())[kOpParamX].add_arguments()->set_name(
      input_operand_info.coreml_name);

  (*op->mutable_inputs())[kParamPerm].add_arguments()->set_name(
      perm_op_output_name);

  CoreML::Specification::MILSpec::NamedValueType& output = *op->add_outputs();
  PopulateNamedValueType(operation.output_operand_id, output);
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilder::AddOperationForConv2d(
    const mojom::Conv2d& operation,
    CoreML::Specification::MILSpec::Block& block) {
  const OperandInfo& input_operand = GetOperandInfo(operation.input_operand_id);
  // Input keys and supported types are defined in coremltools.
  // https://github.com/apple/coremltools/blob/b416f36054af9ca9d10b2d74ba215d0454677ca0/coremltools/converters/mil/mil/ops/defs/iOS15/conv.py#L24
  static constexpr auto kSupportedConv2dOpsTypes =
      base::MakeFixedFlatSet<CoreML::Specification::MILSpec::DataType>(
          {CoreML::Specification::MILSpec::DataType::FLOAT16,
           CoreML::Specification::MILSpec::DataType::FLOAT32});

  if (operation.kind != mojom::Conv2d::Kind::kDirect) {
    // TODO: support transposed conv2d.
    return NewNotSupportedError("Unsupported conv2d kind.");
  }

  if (!kSupportedConv2dOpsTypes.contains(input_operand.mil_data_type)) {
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

  const std::string strides_output_name =
      GetCoreMLNameForParam(operation.output_operand_id, kParamStrides);
  std::array<int32_t, 2> strides = {
      base::checked_cast<int32_t>(operation.strides->height),
      base::checked_cast<int32_t>(operation.strides->width)};
  AddTensorImmediateValue<int32_t>(
      block, strides_output_name,
      base::span<const uint32_t>({static_cast<uint32_t>(strides.size())}),
      strides);

  const std::string pad_type_output_name =
      GetCoreMLNameForParam(operation.output_operand_id, kParamPadType);
  AddTensorImmediateValue<char>(block, pad_type_output_name, /*dimensions=*/{},
                                kParamPadTypeValue);

  const std::string pad_output_name =
      GetCoreMLNameForParam(operation.output_operand_id, kParamPad);
  std::array<int32_t, 4> pad = {
      base::checked_cast<int32_t>(operation.padding->beginning->height),
      base::checked_cast<int32_t>(operation.padding->ending->height),
      base::checked_cast<int32_t>(operation.padding->beginning->width),
      base::checked_cast<int32_t>(operation.padding->ending->width)};
  AddTensorImmediateValue<int32_t>(
      block, pad_output_name,
      base::span<const uint32_t>({static_cast<uint32_t>(pad.size())}), pad);

  const std::string dilations_output_name =
      GetCoreMLNameForParam(operation.output_operand_id, kParamDilations);
  std::array<int32_t, 2> dilations = {
      base::checked_cast<int32_t>(operation.dilations->height),
      base::checked_cast<int32_t>(operation.dilations->width)};
  AddTensorImmediateValue<int32_t>(
      block, dilations_output_name,
      base::span<const uint32_t>({static_cast<uint32_t>(dilations.size())}),
      dilations);

  const std::string groups_output_name =
      GetCoreMLNameForParam(operation.output_operand_id, kParamGroups);
  AddScalarImmediateValue(block, groups_output_name,
                          base::checked_cast<int32_t>(operation.groups));

  CoreML::Specification::MILSpec::Operation* op = block.add_operations();
  op->set_type(kOpConv2dTypeName);
  google::protobuf::Map<std::string,
                        ::CoreML::Specification::MILSpec::Argument>& inputs =
      (*op->mutable_inputs());
  inputs[kOpParamX].add_arguments()->set_name(input_operand.coreml_name);
  inputs[kParamWeight].add_arguments()->set_name(
      GetOperandInfo(operation.filter_operand_id).coreml_name);

  inputs[kParamStrides].add_arguments()->set_name(strides_output_name);
  inputs[kParamPadType].add_arguments()->set_name(pad_type_output_name);
  inputs[kParamPad].add_arguments()->set_name(pad_output_name);
  inputs[kParamDilations].add_arguments()->set_name(dilations_output_name);
  inputs[kParamGroups].add_arguments()->set_name(groups_output_name);
  if (operation.bias_operand_id) {
    inputs[kParamBias].add_arguments()->set_name(
        GetOperandInfo(operation.bias_operand_id.value()).coreml_name);
  }
  CoreML::Specification::MILSpec::NamedValueType& output = *op->add_outputs();
  PopulateNamedValueType(operation.output_operand_id, output);
  return base::ok();
}

template <typename DataType>
  requires internal::IsSupportedTensorType<DataType>
void GraphBuilder::AddScalarImmediateValue(
    CoreML::Specification::MILSpec::Block& block,
    std::string_view name,
    const DataType& value) {
  AddTensorImmediateValue(block, name, /*dimensions=*/{},
                          base::make_span(&value, 1u));
}

template <typename DataType>
  requires internal::IsSupportedTensorType<DataType>
void GraphBuilder::AddTensorImmediateValue(
    CoreML::Specification::MILSpec::Block& block,
    std::string_view name,
    base::span<const uint32_t> dimensions,
    base::span<const DataType> value) {
  auto* op = block.add_operations();
  CoreML::Specification::MILSpec::DataType mil_data_type =
      MilDataTypeMap<DataType>::value;

  op->set_type(kOpConstTypeName);

  google::protobuf::Map<std::string, ::CoreML::Specification::MILSpec::Value>&
      attributes = *op->mutable_attributes();
  attributes["name"] = CreateStringValue(name);
  CoreML::Specification::MILSpec::Value immediate_value{};
  PopulateValueType(mil_data_type, dimensions, *immediate_value.mutable_type());
  auto* tensor = immediate_value.mutable_immediatevalue()->mutable_tensor();
  SetTensorValueForImmediateValue(*tensor, value);
  attributes["val"] = std::move(immediate_value);
  CoreML::Specification::MILSpec::NamedValueType& output = *op->add_outputs();
  PopulateNamedValueType(name, mil_data_type, dimensions, output);
}

void GraphBuilder::AddConstantImmediateValue(
    uint64_t constant_id,
    CoreML::Specification::MILSpec::Block& block) {
  const mojom::Operand& operand = GetOperand(constant_id);

  std::string name = GetCoreMLNameFromOperand(constant_id);
  base::span<const uint8_t> value(
      graph_info_->constant_id_to_buffer_map.at(constant_id));
  switch (operand.data_type) {
    case mojom::Operand::DataType::kFloat32: {
      std::vector<float> floats(value.size() / sizeof(float));
      for (size_t i = 0u; i < floats.size(); ++i) {
        floats[i] = base::FloatFromNativeEndian(
            value.subspan(i * sizeof(float)).first<4u>());
      }
      AddTensorImmediateValue<float>(block, name, operand.dimensions, floats);
      break;
    }
    case mojom::Operand::DataType::kFloat16: {
      std::vector<Float16> float16s(value.size() / sizeof(Float16));
      for (size_t i = 0u; i < float16s.size(); ++i) {
        float16s[i].data = base::U16FromNativeEndian(
            value.subspan(i * sizeof(Float16)).first<2u>());
      }
      AddTensorImmediateValue<Float16>(block, name, operand.dimensions,
                                       float16s);
      break;
    }
    case mojom::Operand::DataType::kInt32: {
      std::vector<int32_t> ints(value.size() / sizeof(int32_t));
      for (size_t i = 0u; i < ints.size(); ++i) {
        ints[i] = base::I32FromNativeEndian(
            value.subspan(i * sizeof(int32_t)).first<4u>());
      }
      AddTensorImmediateValue<int32_t>(block, name, operand.dimensions, ints);
      break;
    }
    case mojom::Operand::DataType::kUint32: {
      std::vector<uint32_t> uints(value.size() / sizeof(uint32_t));
      for (size_t i = 0u; i < uints.size(); ++i) {
        uints[i] = base::U32FromNativeEndian(
            value.subspan(i * sizeof(uint32_t)).first<4u>());
      }
      AddTensorImmediateValue<uint32_t>(block, name, operand.dimensions, uints);
      break;
    }
    case mojom::Operand::DataType::kInt64: {
      std::vector<int64_t> longints(value.size() / sizeof(int64_t));
      for (size_t i = 0u; i < longints.size(); ++i) {
        longints[i] = base::I64FromNativeEndian(
            value.subspan(i * sizeof(int64_t)).first<8u>());
      }
      AddTensorImmediateValue<int64_t>(block, name, operand.dimensions,
                                       longints);
      break;
    }
    case mojom::Operand::DataType::kUint64: {
      std::vector<uint64_t> ulongints(value.size() / sizeof(uint64_t));
      for (size_t i = 0u; i < ulongints.size(); ++i) {
        ulongints[i] = base::U64FromNativeEndian(
            value.subspan(i * sizeof(uint64_t)).first<8u>());
      }
      AddTensorImmediateValue<uint64_t>(block, name, operand.dimensions,
                                        ulongints);
      break;
    }
    case mojom::Operand::DataType::kInt8: {
      std::vector<int8_t> int8s(value.size() / sizeof(int8_t));
      for (size_t i = 0u; i < int8s.size(); ++i) {
        int8s[i] = base::I8FromNativeEndian(
            value.subspan(i * sizeof(int8_t)).first<1u>());
      }
      AddTensorImmediateValue<int8_t>(block, name, operand.dimensions, int8s);
      break;
    }
    case mojom::Operand::DataType::kUint8: {
      std::vector<uint8_t> uint8s(value.size() / sizeof(uint8_t));
      for (size_t i = 0u; i < uint8s.size(); ++i) {
        uint8s[i] = base::U8FromNativeEndian(
            value.subspan(i * sizeof(uint8_t)).first<1u>());
      }
      AddTensorImmediateValue<uint8_t>(block, name, operand.dimensions, uint8s);
      break;
    }
  }
}

void GraphBuilder::AddConstantFileValue(
    uint64_t constant_id,
    uint64_t offset,
    CoreML::Specification::MILSpec::Block& block) {
  auto* op = block.add_operations();
  CoreML::Specification::MILSpec::NamedValueType& output = *op->add_outputs();
  PopulateNamedValueType(constant_id, output);
  op->set_type(kOpConstTypeName);
  // Blob path is defined in generic Operation.attributes.
  // This follows the actual data structure in
  // https://github.com/apple/coremltools/blob/bba83f43859e087d50c7d764cb132e7d4b427611/coremltools/converters/mil/backend/mil/load.py#L60.
  auto& attributes = *op->mutable_attributes();
  attributes["name"] =
      CreateStringValue(GetOperandInfo(constant_id).coreml_name);
  CoreML::Specification::MILSpec::Value blob_value{};
  const mojom::Operand& operand = GetOperand(constant_id);
  PopulateValueType(operand, *blob_value.mutable_type());
  CoreML::Specification::MILSpec::Value::BlobFileValue* blob =
      blob_value.mutable_blobfilevalue();
  blob->set_filename(kWeightsRelativeFilePath);
  blob->set_offset(offset);
  attributes["val"] = std::move(blob_value);
}

const mojom::Operand& GraphBuilder::GetOperand(uint64_t operand_id) const {
  return *graph_info_->id_to_operand_map.at(operand_id);
}

[[nodiscard]] const GraphBuilder::OperandInfo& GraphBuilder::GetOperandInfo(
    uint64_t operand_id) const {
  return result_->GetOperandInfo(operand_id);
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
  PopulateValueType(GetOperand(operand_id), value_type);
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

void GraphBuilder::PopulateValueType(
    const mojom::Operand& operand,
    CoreML::Specification::MILSpec::ValueType& value_type) {
  // TODO: change this when any `Operand` needs to keep scalar type.
  PopulateValueType(OperandTypeToMILDataType(operand.data_type),
                    operand.dimensions, value_type, /*keep_scalar_type=*/false);
}

void GraphBuilder::PopulateValueType(
    CoreML::Specification::MILSpec::DataType mil_data_type,
    base::span<const uint32_t> dimensions,
    CoreML::Specification::MILSpec::ValueType& value_type,
    bool keep_scalar_type) {
  auto* tensor_type = value_type.mutable_tensortype();
  tensor_type->set_datatype(mil_data_type);
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

std::string GraphBuilder::GetCoreMLNameForParam(uint64_t operand_id,
                                                std::string_view param_name) {
  return base::JoinString(
      {kConstValuePrefix, base::NumberToString(operand_id), param_name},
      kStringSeparator);
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
