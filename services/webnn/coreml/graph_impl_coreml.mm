// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/coreml/graph_impl_coreml.h"

#import <CoreML/CoreML.h>
#import <Foundation/Foundation.h>

#include <memory>

#include "base/apple/foundation_util.h"
#include "base/barrier_callback.h"
#include "base/command_line.h"
#include "base/dcheck_is_on.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
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
#include "services/webnn/coreml/buffer_content.h"
#include "services/webnn/coreml/context_impl_coreml.h"
#include "services/webnn/coreml/graph_builder_coreml.h"
#include "services/webnn/coreml/tensor_impl_coreml.h"
#include "services/webnn/coreml/utils_coreml.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/queueable_resource_state_base.h"
#include "services/webnn/resource_task.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_switches.h"

@interface WebNNMLFeatureProvider : NSObject <MLFeatureProvider>
- (MLFeatureValue*)featureValueForName:(NSString*)featureName;
@property(readonly, nonatomic) NSSet<NSString*>* featureNames;
@property(readonly, nonatomic) NSDictionary* featureValues;
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
@synthesize featureValues = _featureValues;
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

// Compute strides which may be used to construct an `MLMultiArray` given
// `multi_array_constraint`.
// See https://developer.apple.com/documentation/coreml/mlmultiarray/strides.
//
// For example, given a 4D input `shape`, its strides would be as follows:
// [
//   shape[1] * shape[2] * shape[3],
//   shape[2] * shape[3],
//   shape[3],
//   1
// ];
NSMutableArray* CalculateStrides(
    MLMultiArrayConstraint* multi_array_constraint) {
  // Empty shapes are not supported for input or output operands.
  CHECK_GT(multi_array_constraint.shape.count, 0u);

  NSMutableArray* strides =
      [NSMutableArray arrayWithCapacity:multi_array_constraint.shape.count];

  // Fill `strides` in reverse order, then return the list in reverse.

  // The last stride is always 1.
  uint32_t current_stride = 1;
  [strides addObject:@(current_stride)];

  for (uint32_t i = multi_array_constraint.shape.count - 1; i > 0; --i) {
    // Overflow checks are not needed here because this calculation will always
    // result in a value less than the similar calculation performed (with
    // overflow checks) in `OperandDescriptor::Create()` - and
    // `multi_array_constraint` corresponds to an `OperandDescriptor`.
    current_stride *= multi_array_constraint.shape[i].unsignedIntegerValue;

    [strides addObject:@(current_stride)];
  }

  return [[[strides reverseObjectEnumerator] allObjects] mutableCopy];
}

API_AVAILABLE(macos(12.3))
base::flat_map<std::string,
               scoped_refptr<QueueableResourceState<BufferContent>>>
ToNamedBufferStateMap(
    const base::flat_map<std::string_view, WebNNTensorImpl*>& named_tensors) {
  base::flat_map<std::string,
                 scoped_refptr<QueueableResourceState<BufferContent>>>
      buffer_states;
  buffer_states.reserve(named_tensors.size());

  for (const auto& [name, tensor] : named_tensors) {
    buffer_states.emplace(
        name, static_cast<TensorImplCoreml*>(tensor)->GetBufferState());
  }

  return buffer_states;
}

}  // namespace

// static
void GraphImplCoreml::CreateAndBuild(
    ContextImplCoreml* context,
    mojom::GraphInfoPtr graph_info,
    ComputeResourceInfo compute_resource_info,
    base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
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
                     std::move(constant_operands), std::move(context_options),
                     std::move(context_properties),
                     std::move(wrapped_callback)));
}

// static
void GraphImplCoreml::CreateAndBuildOnBackgroundThread(
    mojom::GraphInfoPtr graph_info,
    ComputeResourceInfo compute_resource_info,
    base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
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
      GraphBuilderCoreml::CreateAndBuild(
          *graph_info.get(), std::move(context_properties), constant_operands,
          model_file_dir.GetPath()),
      [&](mojom::ErrorPtr error) {
        std::move(callback).Run(base::unexpected(std::move(error)));
        return;
      });
  UMA_HISTOGRAM_MEDIUM_TIMES("WebNN.CoreML.TimingMs.MLModelTranslate",
                             ml_model_write_timer.Elapsed());

  // Create a map of the names used internally by CoreML to the names used
  // externally by WebNN for all inputs and outputs.
  std::vector<std::pair<std::string, std::string>> coreml_name_to_operand_name(
      graph_info->input_operands.size() + graph_info->output_operands.size());
  for (auto const& input_id : graph_info->input_operands) {
    auto& name = graph_info->id_to_operand_map.at(input_id)->name;
    CHECK(name.has_value());
    coreml_name_to_operand_name.emplace_back(
        GetCoreMLNameFromInput(name.value(), input_id), name.value());
  }
  for (auto const& output_id : graph_info->output_operands) {
    auto& name = graph_info->id_to_operand_map.at(output_id)->name;
    CHECK(name.has_value());
    coreml_name_to_operand_name.emplace_back(
        GetCoreMLNameFromOutput(name.value(), output_id), name.value());
  }

  auto params = std::make_unique<Params>(
      std::move(compute_resource_info), std::move(coreml_name_to_operand_name));

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
MLFeatureValue* GraphImplCoreml::CreateMultiArrayFeatureValueFromBytes(
    MLMultiArrayConstraint* multi_array_constraint,
    mojo_base::BigBuffer data) {
  NSError* error;
  __block mojo_base::BigBuffer captured_data = std::move(data);
  MLMultiArray* multi_array = [[MLMultiArray alloc]
      initWithDataPointer:captured_data.data()
                    shape:multi_array_constraint.shape
                 dataType:multi_array_constraint.dataType
                  strides:CalculateStrides(multi_array_constraint)
              deallocator:^(void* bytes) {
                mojo_base::BigBuffer destroy_in_block =
                    std::move(captured_data);
              }
                    error:&error];
  CHECK(!error);
  return [MLFeatureValue featureValueWithMultiArray:multi_array];
}

GraphImplCoreml::GraphImplCoreml(ContextImplCoreml* context,
                                 std::unique_ptr<Params> params)
    : WebNNGraphImpl(context, std::move(params->compute_resource_info)),
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

  NSMutableSet* feature_names = [[NSMutableSet alloc] init];
  NSMutableDictionary* feature_values = [[NSMutableDictionary alloc] init];

  if (named_inputs.empty()) {
    CHECK_EQ(ml_model_.modelDescription.inputDescriptionsByName.count, 1u);

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
  } else {
    CHECK_EQ(named_inputs.size(),
             ml_model_.modelDescription.inputDescriptionsByName.count);

    // Create an `MLFeatureValue` for each of the `named_inputs`.
    NSString* feature_name;
    for (feature_name in ml_model_.modelDescription.inputDescriptionsByName) {
      [feature_names addObject:feature_name];

      MLFeatureDescription* feature_description =
          ml_model_.modelDescription.inputDescriptionsByName[feature_name];
      CHECK_EQ(feature_description.type,
               MLFeatureType::MLFeatureTypeMultiArray);

      auto operand_name_it = coreml_name_to_operand_name_.find(
          base::SysNSStringToUTF8(feature_name));
      CHECK(operand_name_it != coreml_name_to_operand_name_.end());

      auto buffer_it = named_inputs.find(operand_name_it->second);
      CHECK(buffer_it != named_inputs.end());

      mojo_base::BigBuffer buffer = std::move(buffer_it->second);

      MLFeatureValue* feature_value = CreateMultiArrayFeatureValueFromBytes(
          feature_description.multiArrayConstraint, std::move(buffer));
      if (!feature_value) {
        std::move(callback).Run(mojom::ComputeResult::NewError(
            mojom::Error::New(mojom::Error::Code::kUnknownError,
                              "Input initialization error")));
        return;
      }

      // Assert that `feature_value` is compatible with `feature_description`.
      CHECK([feature_description isAllowedValue:feature_value]);

      feature_values[feature_name] = feature_value;
    }
  }

  // Run the MLModel asynchronously.
  WebNNMLFeatureProvider* feature_provider =
      [[WebNNMLFeatureProvider alloc] initWithFeatures:feature_names
                                         featureValues:feature_values];
  auto done_callback = base::BindOnce(
      &GraphImplCoreml::DidPredictFromCompute, weak_factory_.GetWeakPtr(),
      std::move(model_predict_timer), std::move(callback));
  [ml_model_ predictionFromFeatures:feature_provider
                  completionHandler:base::CallbackToBlock(
                                        base::BindPostTaskToCurrentDefault(
                                            std::move(done_callback)))];
}

void GraphImplCoreml::DidPredictFromCompute(
    base::ElapsedTimer model_predict_timer,
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

  // Read back the outputs.
  base::ElapsedTimer model_output_read_timer;

  auto barrier_callback =
      base::BarrierCallback<std::pair<std::string, mojo_base::BigBuffer>>(
          output_features.featureNames.count,
          base::BindOnce(
              [](mojom::WebNNGraph::ComputeCallback callback,
                 base::ElapsedTimer model_output_read_timer,
                 std::vector<std::pair<std::string, mojo_base::BigBuffer>>
                     named_outputs) {
                UMA_HISTOGRAM_MEDIUM_TIMES(
                    "WebNN.CoreML.TimingMs.ModelOutputRead",
                    model_output_read_timer.Elapsed());

                std::move(callback).Run(mojom::ComputeResult::NewNamedOutputs(
                    std::move(named_outputs)));
              },
              std::move(callback), std::move(model_output_read_timer)));

  for (NSString* feature_name in output_features.featureNames) {
    MLFeatureValue* feature_value =
        [output_features featureValueForName:feature_name];
    std::string name =
        coreml_name_to_operand_name_.at(base::SysNSStringToUTF8(feature_name));

    MLMultiArray* multi_array_value = feature_value.multiArrayValue;
    ReadFromMLMultiArray(
        multi_array_value,
        base::BindOnce(
            [](base::OnceCallback<void(
                   std::pair<std::string, mojo_base::BigBuffer>)> callback,
               std::string name, mojo_base::BigBuffer buffer) {
              std::move(callback).Run(
                  std::make_pair(std::move(name), std::move(buffer)));
            },
            barrier_callback, std::move(name)));
  }
}

void GraphImplCoreml::DispatchImpl(
    const base::flat_map<std::string_view, WebNNTensorImpl*>& named_inputs,
    const base::flat_map<std::string_view, WebNNTensorImpl*>& named_outputs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "webnn::coreml::GraphImpl::DispatchImpl");

  base::flat_map<std::string,
                 scoped_refptr<QueueableResourceState<BufferContent>>>
      named_input_buffer_states = ToNamedBufferStateMap(named_inputs);
  base::flat_map<std::string,
                 scoped_refptr<QueueableResourceState<BufferContent>>>
      named_output_buffer_states = ToNamedBufferStateMap(named_outputs);

  // Input tensors will be read from while the graph is executing, so lock them
  // them as shared/read-only.
  std::vector<scoped_refptr<QueueableResourceStateBase>> shared_resources;
  shared_resources.reserve(named_inputs.size());
  base::ranges::transform(
      named_input_buffer_states, std::back_inserter(shared_resources),
      [](const auto& name_and_state) { return name_and_state.second; });

  // Exclusively reserve all output tensors, which will be written to.
  std::vector<scoped_refptr<QueueableResourceStateBase>> exclusive_resources;
  exclusive_resources.reserve(named_outputs.size());
  base::ranges::transform(
      named_output_buffer_states, std::back_inserter(exclusive_resources),
      [](const auto& name_and_state) { return name_and_state.second; });

  auto task = base::MakeRefCounted<ResourceTask>(
      std::move(shared_resources), std::move(exclusive_resources),
      base::BindOnce(&GraphImplCoreml::DoDispatch, weak_factory_.GetWeakPtr(),
                     std::move(named_input_buffer_states),
                     std::move(named_output_buffer_states)));
  task->Enqueue();
}

void GraphImplCoreml::DoDispatch(
    base::flat_map<std::string,
                   scoped_refptr<QueueableResourceState<BufferContent>>>
        named_input_buffer_states,
    base::flat_map<std::string,
                   scoped_refptr<QueueableResourceState<BufferContent>>>
        named_output_buffer_states,
    base::OnceClosure completion_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "webnn::coreml::GraphImpl::DoDispatch");

  base::ElapsedTimer model_predict_timer;

  NSString* feature_name;
  NSMutableSet* feature_names = [[NSMutableSet alloc] init];
  NSMutableDictionary* feature_values = [[NSMutableDictionary alloc] init];

  if (named_input_buffer_states.empty()) {
    CHECK_EQ(ml_model_.modelDescription.inputDescriptionsByName.count, 1u);

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
  } else {
    CHECK_EQ(named_input_buffer_states.size(),
             ml_model_.modelDescription.inputDescriptionsByName.count);

    // Create an `MLFeatureValue` for each of the inputs.
    for (feature_name in ml_model_.modelDescription.inputDescriptionsByName) {
      [feature_names addObject:feature_name];

      MLFeatureDescription* feature_description =
          ml_model_.modelDescription.inputDescriptionsByName[feature_name];
      CHECK_EQ(feature_description.type,
               MLFeatureType::MLFeatureTypeMultiArray);

      auto operand_name_it = coreml_name_to_operand_name_.find(
          base::SysNSStringToUTF8(feature_name));
      CHECK(operand_name_it != coreml_name_to_operand_name_.end());

      auto buffer_state_it =
          named_input_buffer_states.find(operand_name_it->second);
      CHECK(buffer_state_it != named_input_buffer_states.end());

      const BufferContent& buffer_content =
          buffer_state_it->second->GetSharedLockedResource();
      MLFeatureValue* feature_value = buffer_content.AsFeatureValue();
      if (!feature_value) {
        LOG(ERROR) << "Input initialization error";
        return;
      }

      // Assert that `feature_value` is compatible with
      // `feature_description`.
      CHECK([feature_description isAllowedValue:feature_value]);

      feature_values[feature_name] = feature_value;
    }
  }

  // Create an `MLFeatureValue` for each of the outputs.
  MLPredictionOptions* options = [[MLPredictionOptions alloc] init];
  NSMutableDictionary* output_backings = [[NSMutableDictionary alloc] init];
  CHECK_EQ(named_output_buffer_states.size(),
           ml_model_.modelDescription.outputDescriptionsByName.count);
  for (feature_name in ml_model_.modelDescription.outputDescriptionsByName) {
    MLFeatureDescription* feature_description =
        ml_model_.modelDescription.outputDescriptionsByName[feature_name];
    CHECK_EQ(feature_description.type, MLFeatureType::MLFeatureTypeMultiArray);

    auto operand_name_it = coreml_name_to_operand_name_.find(
        base::SysNSStringToUTF8(feature_name));
    CHECK(operand_name_it != coreml_name_to_operand_name_.end());

    auto buffer_state_it =
        named_output_buffer_states.find(operand_name_it->second);
    CHECK(buffer_state_it != named_output_buffer_states.end());

    BufferContent* const buffer_content =
        buffer_state_it->second->GetExclusivelyLockedResource();
    MLFeatureValue* feature_value = buffer_content->AsFeatureValue();
    if (!feature_value) {
      LOG(ERROR) << "Output initialization error";
      return;
    }

    // Assert that `feature_value` is compatible with
    // `feature_description`.
    CHECK([feature_description isAllowedValue:feature_value]);

    output_backings[feature_name] = feature_value.multiArrayValue;
  }

  [options setOutputBackings:output_backings];

  WebNNMLFeatureProvider* feature_provider =
      [[WebNNMLFeatureProvider alloc] initWithFeatures:feature_names
                                         featureValues:feature_values];

  // The completion handler may run on another thread, so post a task
  // back to this sequence to run the closure.
  auto wrapped_completion_closure =
      base::BindPostTaskToCurrentDefault(std::move(completion_closure));

  // Run the MLModel asynchronously.
  [ml_model_
      predictionFromFeatures:feature_provider
                     options:options
           completionHandler:
               base::CallbackToBlock(base::BindOnce(
                   [](base::ElapsedTimer model_predict_timer,
                      NSMutableDictionary* output_backing_buffers,
                      base::OnceClosure completion_closure,
                      id<MLFeatureProvider> output_features, NSError* error) {
                     UMA_HISTOGRAM_MEDIUM_TIMES("WebNN.CoreML.TimingMs."
                                                "ModelPredictWithDispatch",
                                                model_predict_timer.Elapsed());

                     // Unlock the resources bound to this `ResourceTask`.
                     std::move(completion_closure).Run();

                     if (error) {
                       // TODO(crbug.com/41492165): Report this error on the
                       // context.
                       LOG(ERROR) << "[WebNN] PredictionError: " << error;
                       return;
                     }

                     // Ensure that the provided backing buffers were in fact
                     // used.
                     //
                     // TODO(crbug.com/333392274): Remove this check,
                     // eventually. The header file for `MLPredictionOptions`
                     // claims CoreML may not use the specified backing buffers
                     // in a handful of scenarios, including the vague case
                     // where "the model doesn't support the user allocated
                     // buffers". We shouldn't ship WebNN to users with this
                     // CHECK enabled, but in the meantime let's see if this
                     // check is ever hit...
                     NSString* output_feature_name;
                     for (output_feature_name in output_features.featureNames) {
                       CHECK_EQ([output_features
                                    featureValueForName:output_feature_name]
                                    .multiArrayValue,
                                output_backing_buffers[output_feature_name]);
                     }
                   },
                   std::move(model_predict_timer), std::move(output_backings),
                   std::move(wrapped_completion_closure)))];
}

GraphImplCoreml::Params::Params(
    ComputeResourceInfo compute_resource_info,
    base::flat_map<std::string, std::string> coreml_name_to_operand_name)
    : compute_resource_info(std::move(compute_resource_info)),
      coreml_name_to_operand_name(std::move(coreml_name_to_operand_name)) {}

GraphImplCoreml::Params::~Params() = default;

}  // namespace webnn::coreml
