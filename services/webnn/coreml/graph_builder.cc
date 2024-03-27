// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/coreml/graph_builder.h"

#include <fstream>
#include <optional>
#include <string_view>

#include "base/bits.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
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

using mojom::Operand;
using mojom::OperandPtr;
using mojom::Operation;

namespace {

const char kWriteFileErrorMessage[] = "Failed to write constant to file.";

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
const char kManifestItemAuthorKey[] = "author";
const char kManifestItemAuthorValue[] = "Chromium";
const char kManifestItemDescriptionKey[] = "description";
const char kManifestModelDescriptionValue[] = "CoreML Model Specification";
const char kManifestWeightsDescriptionValue[] = "CoreML Model Weights";
const char kManifestItemNameKey[] = "name";
const char kManifestItemPathKey[] = "path";
const char kManifestModelValue[] = "model.mlmodel";
const char kManifestWeightsValue[] = "weights";
const char kManifestItemInfoEntriesKey[] = "itemInfoEntries";
const char kManifestVersionKey[] = "fileFormatVersion";
const char kManifestVersionValue[] = "1.0.0";
const char kManifestModelIdentifierKey[] = "rootModelIdentifier";

// model op related consts.
const char kPlaceholderOuputName[] = "placeholder_output";
const char kOpConstTypeName[] = "const";
const char kOpAddTypeName[] = "add";
const char kOpMultiplyTypeName[] = "mul";
const char kOpDivideTypeName[] = "real_div";
const char kOpSubtractTypeName[] = "sub";
const char kOpMaximumTypeName[] = "maximum";
const char kOpMinimumTypeName[] = "minimum";
const char kOpPowerTypeName[] = "pow";

// Hard coded path used in the model file to point at the weight path.
const char kWeightsRelativeFilePath[] = "@model_path/weights/weights.bin";

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
  WeightMetadata(BlobDataType mil_data_type,
                 uint64_t size_in_bytes,
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

std::string GetCoreMLNameFromOperand(uint64_t operand_id,
                                     const Operand& operand) {
  // CoreML doesn't allow op output names to start with numbers, so "var_"
  // prefixes are added.
  switch (operand.kind) {
    case Operand::Kind::kInput:
      CHECK(operand.name.has_value());
      return GetCoreMLNameFromInput(operand.name.value());
    case Operand::Kind::kConstant:
      return "var_" + base::NumberToString(operand_id);
    case Operand::Kind::kOutput:
      if (operand.name.has_value()) {
        return GetCoreMLNameFromOutput(operand.name.value());
      } else {
        // Intermediate outputs don't have names so use operand_id instead.
        return "var_" + base::NumberToString(operand_id);
      }
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

}  // namespace

std::string GetCoreMLNameFromInput(const std::string& input_name) {
  // Prefix is added to user provided names to avoid collision with intermediate
  // operands' names
  return base::StrCat({"input_", input_name});
}

std::string GetCoreMLNameFromOutput(const std::string& output_name) {
  // Prefix is added to user provided names to avoid collision with intermediate
  // operands' names
  return base::StrCat({"output_", output_name});
}

// static
[[nodiscard]] base::expected<std::unique_ptr<GraphBuilder>, mojom::ErrorPtr>
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

  auto graph_builder = base::WrapUnique(new GraphBuilder(
      graph_info, ml_package_dir, data_dir.Append(kMlPackageModelFileName),
      data_dir.Append(kMlPackageWeightsDir).Append(kMlPackageWeightsFileName)));

  RETURN_IF_ERROR(
      graph_builder->BuildCoreMLModel(graph_info, working_directory));

  if (!graph_builder->SerializeModel()) {
    return NewUnknownError("Failed to serialize CoreML model.");
  }

  return graph_builder;
}

GraphBuilder::GraphBuilder(const mojom::GraphInfo& graph_info,
                           base::FilePath ml_package_dir,
                           base::FilePath model_file_path,
                           base::FilePath weights_file_path)
    : graph_info_(graph_info),
      ml_package_dir_(std::move(ml_package_dir)),
      model_file_path_(std::move(model_file_path)),
      weights_file_path_(std::move(weights_file_path)) {}

GraphBuilder::~GraphBuilder() = default;

[[nodiscard]] base::expected<void, mojom::ErrorPtr>
GraphBuilder::BuildCoreMLModel(const mojom::GraphInfo& graph_info,
                               const base::FilePath& working_directory) {
  CHECK_EQ(ml_model_.specificationversion(), 0);
  // Based on comment in Model.proto
  //  * 7 : iOS 16, macOS 13, tvOS 16, watchOS 9 (Core ML 6)
  //  * - FLOAT16 array data type
  //  * - GRAYSCALE_FLOAT16 image color space.
  // use the model specification version supported on macOS 13 which is
  // version 7.
  ml_model_.set_specificationversion(7);
  ml_model_.set_isupdatable(false);

  program_ = ml_model_.mutable_mlprogram();
  program_->set_version(1);

  // Creates a Program with a single main function, and a single block within
  // the function. The block contains all the ops right now.
  // TODO(https://crbug.com/327216253): figure out when to use CoreML7 for some
  // ops.
  auto& main_function = (*program_->mutable_functions())["main"];
  // CoreML6 means specification version 7.
  main_function.set_opset("CoreML6");
  auto& block = (*main_function.mutable_block_specializations())["CoreML6"];

  // Add inputs.
  for (auto& input_id : graph_info.input_operands) {
    RETURN_IF_ERROR(AddInput(input_id, main_function));
  }

  if (graph_info.input_operands.empty()) {
    AddPlaceholderInput(main_function, block);
  }

  RETURN_IF_ERROR(SetupMlPackageDirStructure(working_directory));

  base::ElapsedTimer ml_weights_write_timer;
  RETURN_IF_ERROR(WriteWeightsToFile(block));
  UMA_HISTOGRAM_MEDIUM_TIMES("WebNN.CoreML.TimingMs.MLWeightsWrite",
                             ml_weights_write_timer.Elapsed());

  // Add operations.
  for (auto& operation : graph_info.operations) {
    switch (operation->which()) {
      case mojom::Operation::Tag::kElementWiseBinary: {
        RETURN_IF_ERROR(AddOperationForBinary(
            *operation->get_element_wise_binary(), block));
        break;
      }
      default: {
        return NewNotSupportedError("This operator is not implemented.");
      }
    }
  }

  // Add output.
  for (auto& output_id : graph_info.output_operands) {
    block.add_outputs(GetCoreMLNameFromOperand(
        output_id, *graph_info_->id_to_operand_map.at(output_id)));
    RETURN_IF_ERROR(AddOutput(output_id));
  }
  return base::ok();
}

bool GraphBuilder::SerializeModel() {
  base::ElapsedTimer ml_model_write_timer;
  // This will always overwrite if there is an existing file.
  std::fstream model_file(model_file_path_.value(),
                          std::ios::out | std::ios::binary);
  bool result = ml_model_.SerializeToOstream(&model_file);
  UMA_HISTOGRAM_MEDIUM_TIMES("WebNN.CoreML.TimingMs.MLModelWrite",
                             ml_model_write_timer.Elapsed());
  return result;
}

base::expected<void, mojom::ErrorPtr> GraphBuilder::WriteWeightsToFile(
    CoreML::Specification::MILSpec::Block& block) {
  base::File weights_file(weights_file_path_,
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
    auto& operand = graph_info_->id_to_operand_map.at(key);
    if (operand->dimensions.empty()) {
      AddConstantImmediateValue(key, block);
      continue;
    }

    std::optional<BlobDataType> weight_type =
        OperandTypeToDataTypeInWeightFile(operand->data_type);
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

    AddConstantFileValue(key, current_offset, *operand, block);
    current_offset += sizeof(metadata);
    current_offset += buffer.size();
    current_offset = base::bits::AlignUp(current_offset, kWeightAlignment);
    if (!weights_file.Seek(base::File::Whence::FROM_BEGIN, current_offset)) {
      return NewUnknownError(kWriteFileErrorMessage);
    }
  }
  return base::ok();
}

const GraphBuilder::OperandInfo* GraphBuilder::FindInputOperandInfo(
    const std::string& input_name) const {
  auto id = input_name_to_id_map_.find(input_name);
  if (id == input_name_to_id_map_.end()) {
    return nullptr;
  }
  return GetOperandInfo(id->second);
}

const base::FilePath& GraphBuilder::GetModelFilePath() {
  return ml_package_dir_;
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
  (*placeholder_op->mutable_inputs())["x"].add_arguments()->set_name(
      kPlaceholderInputName);
  (*placeholder_op->mutable_inputs())["y"].add_arguments()->set_name(
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
  const OperandPtr& operand = graph_info_->id_to_operand_map.at(input_id);
  RETURN_IF_ERROR(
      PopulateFeatureDescription(input_id, *operand, *feature_description));

  PopulateNamedValueType(input_id, *graph_info_->id_to_operand_map.at(input_id),
                         *main_function.add_inputs());

  CHECK(input_name_to_id_map_.try_emplace(operand->name.value(), input_id)
            .second);
  return base::ok();
}

[[nodiscard]] base::expected<void, mojom::ErrorPtr> GraphBuilder::AddOutput(
    uint64_t output_id) {
  const auto output_iterator = id_to_op_input_info_map_.find(output_id);
  CHECK(output_iterator != id_to_op_input_info_map_.end());
  const OperandPtr& operand = graph_info_->id_to_operand_map.at(output_id);
  auto* mutable_description = ml_model_.mutable_description();
  auto* feature_description = mutable_description->add_output();
  RETURN_IF_ERROR(
      PopulateFeatureDescription(output_id, *operand, *feature_description));
  return base::ok();
}

base::expected<void, mojom::ErrorPtr> GraphBuilder::AddOperationForBinary(
    const mojom::ElementWiseBinary& operation,
    CoreML::Specification::MILSpec::Block& block) {
  CoreML::Specification::MILSpec::Operation* op = block.add_operations();

  auto input_lhs = id_to_op_input_info_map_.at(operation.lhs_operand_id);
  auto input_rhs = id_to_op_input_info_map_.at(operation.rhs_operand_id);
  // Input keys (x, y) and supported types are defined in coremltools.
  // https://github.com/apple/coremltools/blob/b416f36054af9ca9d10b2d74ba215d0454677ca0/coremltools/converters/mil/mil/ops/defs/iOS15/elementwise_binary.py#L33
  static constexpr auto kSupportedBinaryOpsTypes =
      base::MakeFixedFlatSet<CoreML::Specification::MILSpec::DataType>(
          {CoreML::Specification::MILSpec::DataType::FLOAT16,
           CoreML::Specification::MILSpec::DataType::FLOAT32,
           CoreML::Specification::MILSpec::DataType::INT32});

  if (!kSupportedBinaryOpsTypes.contains(input_lhs.mil_data_type) ||
      !kSupportedBinaryOpsTypes.contains(input_rhs.mil_data_type)) {
    return NewNotSupportedError("Unsupported input datatype.");
  }

  (*op->mutable_inputs())["x"].add_arguments()->set_name(input_lhs.coreml_name);
  (*op->mutable_inputs())["y"].add_arguments()->set_name(input_rhs.coreml_name);

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
    default:
      return NewNotSupportedError("Unimplemented Binary Operator.");
  }

  PopulateNamedValueType(
      operation.output_operand_id,
      *graph_info_->id_to_operand_map.at(operation.output_operand_id),
      *op->add_outputs());

  return base::ok();
}

void GraphBuilder::AddConstantImmediateValue(
    uint32_t constant_id,
    CoreML::Specification::MILSpec::Block& block) {
  auto& operand = *graph_info_->id_to_operand_map.at(constant_id);
  auto* op = block.add_operations();
  PopulateNamedValueType(constant_id, operand, *op->add_outputs());
  op->set_type(kOpConstTypeName);

  google::protobuf::Map<std::string, ::CoreML::Specification::MILSpec::Value>&
      attributes = *op->mutable_attributes();
  attributes["name"] =
      CreateStringValue(id_to_op_input_info_map_.at(constant_id).coreml_name);
  CoreML::Specification::MILSpec::Value immediate_value{};
  PopulateValueType(operand, *immediate_value.mutable_type());
  auto* data = immediate_value.mutable_immediatevalue()->mutable_tensor();
  const mojo_base::BigBuffer& buffer =
      graph_info_->constant_id_to_buffer_map.at(constant_id);

  switch (operand.data_type) {
    case mojom::Operand::DataType::kFloat32:
      data->mutable_floats()->add_values(
          *reinterpret_cast<const float*>(buffer.data()));
      break;
    // As per
    // https://github.com/apple/coremltools/blob/bba83f43859e087d50c7d764cb132e7d4b427611/coremltools/converters/mil/backend/mil/helper.py#L23,
    // these types are stored in bytes.
    case mojom::Operand::DataType::kFloat16:
    case mojom::Operand::DataType::kInt8:
    case mojom::Operand::DataType::kUint8:
    case mojom::Operand::DataType::kUint32:
      data->mutable_bytes()->mutable_values()->assign(
          buffer.data(), buffer.data() + buffer.size());
      break;
    case mojom::Operand::DataType::kInt32:
      data->mutable_ints()->add_values(
          *reinterpret_cast<const int*>(buffer.data()));
      break;
    case mojom::Operand::DataType::kInt64:
    case mojom::Operand::DataType::kUint64:
      data->mutable_longints()->add_values(
          *reinterpret_cast<const long*>(buffer.data()));
      break;
  }
  attributes["val"] = std::move(immediate_value);
}

void GraphBuilder::AddConstantFileValue(
    uint32_t constant_id,
    uint64_t offset,
    const mojom::Operand& operand,
    CoreML::Specification::MILSpec::Block& block) {
  auto* op = block.add_operations();
  PopulateNamedValueType(constant_id, operand, *op->add_outputs());
  op->set_type(kOpConstTypeName);
  // Blob path is defined in generic Operation.attributes.
  // This follows the actual data structure in
  // https://github.com/apple/coremltools/blob/bba83f43859e087d50c7d764cb132e7d4b427611/coremltools/converters/mil/backend/mil/load.py#L60.
  auto& attributes = *op->mutable_attributes();
  attributes["name"] =
      CreateStringValue(id_to_op_input_info_map_.at(constant_id).coreml_name);
  CoreML::Specification::MILSpec::Value blob_value{};
  PopulateValueType(operand, *blob_value.mutable_type());
  CoreML::Specification::MILSpec::Value::BlobFileValue* blob =
      blob_value.mutable_blobfilevalue();
  blob->set_filename(kWeightsRelativeFilePath);
  blob->set_offset(offset);
  attributes["val"] = std::move(blob_value);
}

[[nodiscard]] const GraphBuilder::OperandInfo* GraphBuilder::GetOperandInfo(
    uint64_t operand_id) const {
  const auto input_iterator = id_to_op_input_info_map_.find(operand_id);
  CHECK(input_iterator != id_to_op_input_info_map_.end());
  return &input_iterator->second;
}

base::expected<void, mojom::ErrorPtr> GraphBuilder::PopulateFeatureDescription(
    uint64_t operand_id,
    const mojom::Operand& operand,
    ::CoreML::Specification::FeatureDescription& feature_description) {
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
      return NewNotSupportedError("Unsupported input datatype.");
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
      GetCoreMLNameFromOperand(operand_id, operand));
  return base::ok();
}

void GraphBuilder::PopulateNamedValueType(
    uint64_t operand_id,
    const mojom::Operand& operand,
    CoreML::Specification::MILSpec::NamedValueType& named_value_type) {
  named_value_type.set_name(GetCoreMLNameFromOperand(operand_id, operand));
  auto& value_type = *named_value_type.mutable_type();
  PopulateValueType(operand, value_type);

  // WebNN allows 0D scalar operands to have empty dimensions.
  // At the input and output nodes, these can be treated as a 1D tensor to
  // satisfy CoreML's requirement of having at least 1 dimension.
  CHECK(id_to_op_input_info_map_
            .try_emplace(operand_id,
                         OperandInfo(named_value_type.name(),
                                     operand.dimensions.empty()
                                         ? std::vector<uint32_t>({1})
                                         : operand.dimensions,
                                     operand.data_type,
                                     value_type.tensortype().datatype()))
            .second);
}

void GraphBuilder::PopulateValueType(
    const mojom::Operand& operand,
    CoreML::Specification::MILSpec::ValueType& value_type) {
  auto* tensor_type = value_type.mutable_tensortype();
  auto mil_data_type = OperandTypeToMILDataType(operand.data_type);
  tensor_type->set_datatype(mil_data_type);
  tensor_type->set_rank(operand.dimensions.empty() ? 1
                                                   : operand.dimensions.size());
  if (operand.dimensions.empty()) {
    tensor_type->set_rank(1);
    tensor_type->add_dimensions()->mutable_constant()->set_size(1);
  } else {
    tensor_type->set_rank(operand.dimensions.size());
    for (int dimension : operand.dimensions) {
      tensor_type->add_dimensions()->mutable_constant()->set_size(dimension);
    }
  }
}

base::expected<void, mojom::ErrorPtr> GraphBuilder::SetupMlPackageDirStructure(
    const base::FilePath& working_directory) {
  if (!base::CreateDirectory(ml_package_dir_)) {
    return NewUnknownError("Fail to create .mlpackage directory.");
  }
  base::FilePath data_dir = ml_package_dir_.Append(kMlPackageDataDir);
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
  JSONFileValueSerializer serializer(ml_package_dir_.Append(kManifestFileName));
  if (!serializer.Serialize(std::move(metadata))) {
    return NewUnknownError("Fail to create Manifest.json for mlpackage.");
  }

  return base::ok();
}

GraphBuilder::OperandInfo::OperandInfo(
    std::string coreml_name,
    std::vector<uint32_t> dimensions,
    mojom::Operand::DataType data_type,
    CoreML::Specification::MILSpec::DataType mil_data_type)
    : coreml_name(std::move(coreml_name)),
      dimensions(std::move(dimensions)),
      data_type(data_type),
      mil_data_type(std::move(mil_data_type)) {}

GraphBuilder::OperandInfo::OperandInfo() = default;
GraphBuilder::OperandInfo::~OperandInfo() = default;
GraphBuilder::OperandInfo::OperandInfo(OperandInfo&) = default;
GraphBuilder::OperandInfo::OperandInfo(OperandInfo&&) = default;

}  // namespace webnn::coreml
