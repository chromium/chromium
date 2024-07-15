// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/coreml/graph_impl_coreml.h"

#import <CoreML/CoreML.h>
#import <Foundation/Foundation.h>

#include <memory>

#include "base/apple/foundation_util.h"
#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/containers/span_reader.h"
#include "base/containers/span_writer.h"
#include "base/dcheck_is_on.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected_macros.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "services/webnn/coreml/context_impl_coreml.h"
#include "services/webnn/coreml/graph_builder_coreml.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_switches.h"

@interface WebNNMLFeatureProvider : NSObject <MLFeatureProvider>
- (MLFeatureValue*)featureValueForName:(NSString*)featureName;
@property(readonly, nonatomic) NSSet<NSString*>* featureNames;
@end

@implementation WebNNMLFeatureProvider
- (MLFeatureValue*)featureValueForName:(NSString*)featureName {
  return _featureValues[featureName];
}
- (instancetype)initWithFeatures:(NSSet<NSString*>*)feature_names
                   featureValues:(NSDictionary*)feature_values {
  self = [super init];
  if (self) {
    _featureNames = feature_names;
    _featureValues = feature_values;
  }
  return self;
}
@synthesize featureNames = _featureNames;
NSDictionary* _featureValues;
@end

namespace webnn::coreml {

namespace {

// Responsible for cleaning up disk artifacts created by the CoreML model
// compilation process.
struct ScopedModelPaths {
  ~ScopedModelPaths() {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kWebNNCoreMlDumpModel)) {
      const auto dump_directory =
          base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
              switches::kWebNNCoreMlDumpModel);
      LOG(INFO) << "[WebNN] Copying model files to " << dump_directory;
      if (dump_directory.empty()) {
        LOG(ERROR) << "[WebNN] Dump directory not specified.";
      } else {
        if (!model_file_dir.IsValid() ||
            !base::CopyDirectory(model_file_dir.GetPath(), dump_directory,
                                 /*recursive=*/true)) {
          LOG(ERROR) << "[WebNN] Failed to copy model file directory.";
        }
        if (!compiled_model_dir.IsValid() ||
            !base::CopyDirectory(compiled_model_dir.GetPath(), dump_directory,
                                 /*recursive=*/true)) {
          LOG(ERROR) << "[WebNN] Failed to copy compiled model directory.";
        }
      }
    }
    // Though the destructors of ScopedTempDir will delete these directories.
    // Explicitly delete them here to check for success.
    if (model_file_dir.IsValid()) {
      CHECK(model_file_dir.Delete());
    }
    if (compiled_model_dir.IsValid()) {
      CHECK(compiled_model_dir.Delete());
    }
  }

  base::ScopedTempDir model_file_dir;
  base::ScopedTempDir compiled_model_dir;
};

uint32_t GetDataTypeByteSize(MLMultiArrayDataType data_type) {
  switch (data_type) {
    case MLMultiArrayDataTypeDouble:
      return 8;
    case MLMultiArrayDataTypeFloat32:
    case MLMultiArrayDataTypeInt32:
      return 4;
    case MLMultiArrayDataTypeFloat16:
      return 2;
  }
}

std::optional<MLMultiArrayDataType> ToMLMultiArrayDataType(
    OperandDataType data_type) {
  switch (data_type) {
    case OperandDataType::kFloat32:
      return MLMultiArrayDataTypeFloat32;
    case OperandDataType::kFloat16:
      if (__builtin_available(macOS 14, *)) {
        return MLMultiArrayDataTypeFloat16;
      }
      NOTREACHED_NORETURN();
    case OperandDataType::kInt32:
      return MLMultiArrayDataTypeInt32;
    case OperandDataType::kUint32:
    case OperandDataType::kInt64:
    case OperandDataType::kUint64:
    case OperandDataType::kInt8:
    case OperandDataType::kUint8:
      // Unsupported data types in coreml.
      return std::nullopt;
  }
}

void ExtractOutputRecursively(base::span<const uint8_t> bytes,
                              base::span<const uint32_t> dimensions,
                              base::span<const uint32_t> strides,
                              uint32_t item_byte_size,
                              base::span<uint8_t> output) {
  // Data is packed, copy the whole thing.
  // On the last dimension, the bytes could be more than the output because of
  // strides from previous dimension, but as long as current stride is 1, we can
  // copy continously.
  if (bytes.size() == output.size() ||
      (dimensions.size() == 1 && strides[0] == 1)) {
    output.copy_from(bytes.first(output.size()));
    return;
  }

  CHECK_EQ(output.size() % dimensions[0], 0u);
  size_t subspan_size = output.size() / dimensions[0];

  base::SpanReader<const uint8_t> reader(bytes);
  base::SpanWriter<uint8_t> writer(output);
  for (uint32_t i = 0; i < dimensions[0]; i++) {
    auto output_subspan = writer.Skip(subspan_size);
    CHECK(output_subspan);
    auto subspan = reader.Read(strides[0] * item_byte_size);
    CHECK(subspan);
    if (dimensions.size() == 1) {
      output_subspan->copy_from(subspan->first(item_byte_size));
    } else {
      ExtractOutputRecursively(*subspan, dimensions.subspan(1u),
                               strides.subspan(1u), item_byte_size,
                               output_subspan->subspan(0, subspan_size));
    }
  }
}

mojo_base::BigBuffer ExtractMaybeNonContiguousOutput(
    base::span<const uint8_t> bytes,
    uint32_t expected_byte_size,
    uint32_t item_byte_size,
    base::span<const uint32_t> dimensions,
    base::span<const uint32_t> strides) {
  mojo_base::BigBuffer output(expected_byte_size);

  // Bytes size should match with the layout from the strides.
  CHECK_EQ(bytes.size(), strides[0] * dimensions[0] * item_byte_size);
  ExtractOutputRecursively(bytes, dimensions, strides, item_byte_size,
                           base::span(output));
  return output;
}

}  // namespace

// static
void GraphImplCoreml::CreateAndBuild(
    ContextImplCoreml* context,
    mojom::GraphInfoPtr graph_info,
    ComputeResourceInfo compute_resource_info,
    mojom::CreateContextOptionsPtr context_options,
    ContextProperties context_properties,
    WebNNContextImpl::CreateGraphImplCallback callback) {
  auto wrapped_callback = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&GraphImplCoreml::DidCreateAndBuild, context->AsWeakPtr(),
                     std::move(callback)));

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()},
      base::BindOnce(&GraphImplCoreml::CreateAndBuildOnBackgroundThread,
                     std::move(graph_info), std::move(compute_resource_info),
                     std::move(context_options), std::move(context_properties),
                     std::move(wrapped_callback)));
}

// static
void GraphImplCoreml::CreateAndBuildOnBackgroundThread(
    mojom::GraphInfoPtr graph_info,
    ComputeResourceInfo compute_resource_info,
    mojom::CreateContextOptionsPtr context_options,
    ContextProperties context_properties,
    base::OnceCallback<void(
        base::expected<std::unique_ptr<Params>, mojom::ErrorPtr>)> callback) {
  CHECK(graph_info);
  base::ScopedTempDir model_file_dir;
  if (!model_file_dir.CreateUniqueTempDir()) {
    std::move(callback).Run(base::unexpected(mojom::Error::New(
        mojom::Error::Code::kUnknownError, "Model allocation error.")));
    return;
  }
  base::ElapsedTimer ml_model_write_timer;
  // Generate .mlpackage.
  ASSIGN_OR_RETURN(
      std::unique_ptr<GraphBuilderCoreml::Result> build_graph_result,
      GraphBuilderCoreml::CreateAndBuild(*graph_info.get(),
                                         std::move(context_properties),
                                         model_file_dir.GetPath()),
      [&](mojom::ErrorPtr error) {
        std::move(callback).Run(base::unexpected(std::move(error)));
        return;
      });
  UMA_HISTOGRAM_MEDIUM_TIMES("WebNN.CoreML.TimingMs.MLModelTranslate",
                             ml_model_write_timer.Elapsed());

  // Collect information about model inputs that are required
  // later for model evaluation.
  std::vector<std::pair<std::string, CoreMLFeatureInfo>>
      input_feature_info_vector;
  input_feature_info_vector.reserve(
      compute_resource_info.input_names_to_descriptors.size());
  for (auto const& [name, _] :
       compute_resource_info.input_names_to_descriptors) {
    std::optional<GraphImplCoreml::CoreMLFeatureInfo> coreml_feature_info =
        GetCoreMLFeatureInfo(
            build_graph_result->FindModelInputOperandInfo(name));
    if (!coreml_feature_info.has_value()) {
      std::move(callback).Run(base::unexpected(mojom::Error::New(
          mojom::Error::Code::kUnknownError, "Model inputs error.")));
      return;
    }
    input_feature_info_vector.emplace_back(
        name, std::move(coreml_feature_info.value()));
  }

  std::vector<std::pair<std::string, std::string>> coreml_name_to_operand_name;
  for (auto const& output_id : graph_info->output_operands) {
    auto& name = graph_info->id_to_operand_map.at(output_id)->name;
    CHECK(name.has_value());
    coreml_name_to_operand_name.emplace_back(
        GetCoreMLNameFromOutput(name.value(), output_id), name.value());
  }

  auto input_feature_info = std::make_unique<CoreMLFeatureInfoMap>(
      std::move(input_feature_info_vector));

  auto params = std::make_unique<Params>(
      std::move(compute_resource_info), std::move(input_feature_info),
      std::move(coreml_name_to_operand_name));

  [MLModel
      compileModelAtURL:base::apple::FilePathToNSURL(
                            build_graph_result->GetModelFilePath())
      completionHandler:base::CallbackToBlock(base::BindOnce(
                            &LoadCompiledModelOnBackgroundThread,
                            base::ElapsedTimer(), std::move(model_file_dir),
                            std::move(context_options), std::move(params),
                            std::move(callback)))];
}

// static
void GraphImplCoreml::LoadCompiledModelOnBackgroundThread(
    base::ElapsedTimer compilation_timer,
    base::ScopedTempDir model_file_dir,
    mojom::CreateContextOptionsPtr context_options,
    std::unique_ptr<Params> params,
    base::OnceCallback<void(
        base::expected<std::unique_ptr<Params>, mojom::ErrorPtr>)> callback,
    NSURL* compiled_model_url,
    NSError* error) {
  UMA_HISTOGRAM_MEDIUM_TIMES("WebNN.CoreML.TimingMs.MLModelCompile",
                             compilation_timer.Elapsed());

  // `compiled_model_url` refers to a directory placed directly inside
  // NSTemporaryDirectory(), it is not inside `model_file_dir`.
  // Wrap it in a `ScopedTempDir` to ensure it is always cleaned up after
  // loading the compiled model.
  base::ScopedTempDir scoped_compiled_model_dir;
  if (compiled_model_url) {
    CHECK(scoped_compiled_model_dir.Set(
        base::apple::NSURLToFilePath(compiled_model_url)));
  }
  ScopedModelPaths scoped_paths{
      .model_file_dir = std::move(model_file_dir),
      .compiled_model_dir = std::move(scoped_compiled_model_dir)};

  if (error) {
    LOG(ERROR) << "[WebNN] " << error;
    std::move(callback).Run(base::unexpected(mojom::Error::New(
        mojom::Error::Code::kUnknownError, "Model compilation error.")));
    return;
  }

  MLModelConfiguration* configuration = [[MLModelConfiguration alloc] init];
  switch (context_options->device) {
    case mojom::CreateContextOptions::Device::kCpu:
      configuration.computeUnits = MLComputeUnitsCPUOnly;
      break;
    case mojom::CreateContextOptions::Device::kGpu:
      // TODO: crbug.com/344935458 - Switch to MLComputeUnitsCPUAndGPU
      // when we figure out how to fix the crashes.
      configuration.computeUnits = MLComputeUnitsAll;
      break;
    case mojom::CreateContextOptions::Device::kNpu:
      configuration.computeUnits = MLComputeUnitsAll;
      break;
  }

  base::ElapsedTimer model_load_timer;
  NSError* model_load_error = nil;
  params->ml_model = [MLModel modelWithContentsOfURL:compiled_model_url
                                       configuration:configuration
                                               error:&model_load_error];
  UMA_HISTOGRAM_MEDIUM_TIMES("WebNN.CoreML.TimingMs.CompiledModelLoad",
                             model_load_timer.Elapsed());
  if (model_load_error) {
    LOG(ERROR) << "[WebNN] " << model_load_error;
    std::move(callback).Run(base::unexpected(mojom::Error::New(
        mojom::Error::Code::kUnknownError, "Model load error.")));
    return;
  }

  std::move(callback).Run(std::move(params));
}

// static
void GraphImplCoreml::DidCreateAndBuild(
    base::WeakPtr<WebNNContextImpl> context,
    WebNNContextImpl::CreateGraphImplCallback callback,
    base::expected<std::unique_ptr<Params>, mojom::ErrorPtr> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "[WebNN] " << result.error()->message;
    std::move(callback).Run(base::unexpected(std::move(result).error()));
    return;
  }

  if (!context) {
    std::move(callback).Run(base::unexpected(mojom::Error::New(
        mojom::Error::Code::kUnknownError, "Context was destroyed.")));
    return;
  }

#if DCHECK_IS_ON()
  context->AssertCalledOnValidSequence();
#endif

  std::move(callback).Run(base::WrapUnique(new GraphImplCoreml(
      static_cast<ContextImplCoreml*>(context.get()), *std::move(result))));
}

// static
MLFeatureValue* GraphImplCoreml::CreateFeatureValue(
    GraphImplCoreml::CoreMLFeatureInfo* feature_info,
    mojo_base::BigBuffer data) {
  NSError* error;
  __block mojo_base::BigBuffer captured_data = std::move(data);
  MLMultiArray* multi_array =
      [[MLMultiArray alloc] initWithDataPointer:captured_data.data()
                                          shape:feature_info->shape
                                       dataType:feature_info->data_type
                                        strides:feature_info->stride
                                    deallocator:^(void* bytes) {
                                      mojo_base::BigBuffer destroy_in_block =
                                          std::move(captured_data);
                                    }
                                          error:&error];
  CHECK(!error);
  return [MLFeatureValue featureValueWithMultiArray:multi_array];
}

// static
std::optional<GraphImplCoreml::CoreMLFeatureInfo>
GraphImplCoreml::GetCoreMLFeatureInfo(
    const GraphBuilderCoreml::InputOperandInfo& operand_info) {
  std::optional<MLMultiArrayDataType> data_type =
      ToMLMultiArrayDataType(operand_info.data_type);
  if (!data_type.has_value()) {
    return std::nullopt;
  }
  NSMutableArray* shape =
      [[NSMutableArray alloc] initWithCapacity:operand_info.dimensions.size()];
  NSMutableArray* stride =
      [[NSMutableArray alloc] initWithCapacity:operand_info.dimensions.size()];
  base::CheckedNumeric<uint32_t> expected_size = 1;
  for (uint32_t dimension : operand_info.dimensions) {
    expected_size *= dimension;
  }
  if (!expected_size.IsValid()) {
    LOG(ERROR) << "[WebNN] Error GetCoreMLFeatureInfo expected size overflow";
    return std::nullopt;
  }
  uint32_t current_stride = expected_size.ValueOrDie();
  for (uint32_t dimension : operand_info.dimensions) {
    [shape addObject:@(dimension)];
    // since expected_size was computed by multiplying all dimensions together
    // current_stride has to be perfectly divisible by dimension.
    current_stride = current_stride / dimension;
    [stride addObject:@(current_stride)];
  }
  return GraphImplCoreml::CoreMLFeatureInfo(*data_type, shape, stride,
                                            operand_info.coreml_name);
}

GraphImplCoreml::GraphImplCoreml(ContextImplCoreml* context,
                                 std::unique_ptr<Params> params)
    : WebNNGraphImpl(context, std::move(params->compute_resource_info)),
      input_feature_info_(std::move(params->input_feature_info)),
      coreml_name_to_operand_name_(
          std::move(params->coreml_name_to_operand_name)),
      ml_model_(params->ml_model) {
  CHECK(ml_model_);
}

GraphImplCoreml::~GraphImplCoreml() = default;

void GraphImplCoreml::ComputeImpl(
    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs,
    mojom::WebNNGraph::ComputeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "webnn::coreml::GraphImplCoreml::ComputeImpl");
  CHECK(ml_model_);

  base::ElapsedTimer model_predict_timer;

  // Create MLFeatureValue for each of the `named_inputs`.
  NSMutableSet* feature_names = [[NSMutableSet alloc] init];
  NSMutableDictionary* feature_values = [[NSMutableDictionary alloc] init];
  for (auto& [key, buffer] : named_inputs) {
    auto feature_info = input_feature_info_->find(key);
    CHECK(feature_info != input_feature_info_->end());
    NSString* feature_name =
        base::SysUTF8ToNSString(feature_info->second.coreml_name);
    [feature_names addObject:feature_name];

    MLFeatureValue* feature_value =
        CreateFeatureValue(&feature_info->second, std::move(buffer));
    if (!feature_value) {
      std::move(callback).Run(mojom::ComputeResult::NewError(mojom::Error::New(
          mojom::Error::Code::kUnknownError, "Input initialization error")));
      return;
    }
    feature_values[feature_name] = feature_value;
  }

  if (named_inputs.empty()) {
    NSString* placeholder_name = base::SysUTF8ToNSString(kPlaceholderInputName);
    [feature_names addObject:placeholder_name];
    NSError* error;
    MLMultiArray* placeholder_input =
        [[MLMultiArray alloc] initWithShape:@[ @1 ]
                                   dataType:MLMultiArrayDataTypeFloat16
                                      error:&error];
    placeholder_input[0] = @0;
    CHECK(!error);
    feature_values[placeholder_name] =
        [MLFeatureValue featureValueWithMultiArray:placeholder_input];
  }

  // Run the MLModel asynchronously.
  WebNNMLFeatureProvider* feature_provider =
      [[WebNNMLFeatureProvider alloc] initWithFeatures:feature_names
                                         featureValues:feature_values];
  auto done_callback =
      base::BindOnce(&GraphImplCoreml::DidPredict, weak_factory_.GetWeakPtr(),
                     std::move(model_predict_timer), std::move(callback));
  [ml_model_ predictionFromFeatures:feature_provider
                  completionHandler:base::CallbackToBlock(
                                        base::BindPostTaskToCurrentDefault(
                                            std::move(done_callback)))];
}

void GraphImplCoreml::DidPredict(base::ElapsedTimer model_predict_timer,
                                 mojom::WebNNGraph::ComputeCallback callback,
                                 id<MLFeatureProvider> output_features,
                                 NSError* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UMA_HISTOGRAM_MEDIUM_TIMES("WebNN.CoreML.TimingMs.ModelPredict",
                             model_predict_timer.Elapsed());

  if (error) {
    LOG(ERROR) << "[WebNN] PredictionError : " << error;
    std::move(callback).Run(mojom::ComputeResult::NewError(mojom::Error::New(
        mojom::Error::Code::kUnknownError, "Error computing results")));
    return;
  }

  base::ElapsedTimer model_output_read_timer;
  // Read back the outputs
  // named_outputs is owned by the BarrierClosure. named_outputs
  // is allocated here with make unique and then moved into the
  // BarrierClosure's completion callback. Other completion
  // handlers that run before the barrier closure get a
  // raw_ptr to the named_outputs.
  auto named_outputs = std::make_unique<
      std::vector<std::pair<std::string, mojo_base::BigBuffer>>>();
  named_outputs->reserve(output_features.featureNames.count);
  auto* named_outputs_raw_ptr = named_outputs.get();
  base::RepeatingClosure done_barrier = base::BarrierClosure(
      output_features.featureNames.count,
      base::BindOnce(
          [](mojom::WebNNGraph::ComputeCallback callback,
             std::unique_ptr<std::vector<
                 std::pair<std::string, mojo_base::BigBuffer>>> named_outputs,
             base::ElapsedTimer model_output_read_timer) {
            UMA_HISTOGRAM_MEDIUM_TIMES("WebNN.CoreML.TimingMs.ModelOutputRead",
                                       model_output_read_timer.Elapsed());
            std::move(callback).Run(mojom::ComputeResult::NewNamedOutputs(
                std::move(*named_outputs)));
          },
          std::move(callback), std::move(named_outputs),
          std::move(model_output_read_timer)));
  for (NSString* feature_name in output_features.featureNames) {
    MLFeatureValue* feature_value =
        [output_features featureValueForName:feature_name];
    std::string name =
        coreml_name_to_operand_name_.at(base::SysNSStringToUTF8(feature_name));

    MLMultiArray* multiarray_value = feature_value.multiArrayValue;
    [multiarray_value getBytesWithHandler:^(const void* bytes, NSInteger size) {
      OperandDescriptor expected_descriptor =
          compute_resource_info().output_names_to_descriptors.at(name);

      size_t number_of_items = multiarray_value.count;
      CHECK_EQ(number_of_items, expected_descriptor.NumberOfElements());

      uint32_t item_byte_size = GetDataTypeByteSize(multiarray_value.dataType);
      CHECK_EQ(item_byte_size, OperandDescriptor::GetBytesPerElement(
                                   expected_descriptor.data_type()));

      std::vector<uint32_t> dimensions(multiarray_value.shape.count);
      for (uint32_t i = 0; i < multiarray_value.shape.count; ++i) {
        dimensions[i] = multiarray_value.shape[i].integerValue;
      }
      std::vector<uint32_t> strides(multiarray_value.strides.count);
      for (uint32_t i = 0; i < multiarray_value.strides.count; ++i) {
        strides[i] = multiarray_value.strides[i].integerValue;
      }
      CHECK_EQ(dimensions.size(), strides.size());

      // SAFETY: -[MLMultiArray getBytesWithHandler:] guarantees that
      // `bytes` points to at least `returned_size` valid bytes.
      auto data = UNSAFE_BUFFERS(base::span(static_cast<const uint8_t*>(bytes),
                                            base::checked_cast<size_t>(size)));
      named_outputs_raw_ptr->push_back(
          std::make_pair(name, ExtractMaybeNonContiguousOutput(
                                   data, expected_descriptor.PackedByteLength(),
                                   item_byte_size, dimensions, strides)));
      done_barrier.Run();
    }];
  }
}

void GraphImplCoreml::DispatchImpl(
    const base::flat_map<std::string_view, WebNNBufferImpl*>& named_inputs,
    const base::flat_map<std::string_view, WebNNBufferImpl*>& named_outputs) {
  // TODO(crbug.com/333392274): Implement MLBuffer for CoreML.
  NOTIMPLEMENTED();
}

GraphImplCoreml::Params::Params(
    ComputeResourceInfo compute_resource_info,
    std::unique_ptr<CoreMLFeatureInfoMap> input_feature_info,
    base::flat_map<std::string, std::string> coreml_name_to_operand_name)
    : compute_resource_info(std::move(compute_resource_info)),
      input_feature_info(std::move(input_feature_info)),
      coreml_name_to_operand_name(std::move(coreml_name_to_operand_name)) {}

GraphImplCoreml::Params::~Params() = default;

}  // namespace webnn::coreml
