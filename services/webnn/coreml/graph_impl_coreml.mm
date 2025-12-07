// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/coreml/graph_impl_coreml.h"

#import <CoreML/CoreML.h>
#import <Foundation/Foundation.h>

#include <algorithm>
#include <memory>

#include "base/apple/foundation_util.h"
#include "base/barrier_callback.h"
#include "base/command_line.h"
#include "base/dcheck_is_on.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/types/expected_macros.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "services/webnn/coreml/buffer_content_coreml.h"
#include "services/webnn/coreml/context_impl_coreml.h"
#include "services/webnn/coreml/graph_builder_coreml.h"
#include "services/webnn/coreml/tensor_impl_coreml.h"
#include "services/webnn/coreml/utils_coreml.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/features.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/queueable_resource_state_base.h"
#include "services/webnn/resource_task.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_switches.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

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

API_AVAILABLE(macos(12.3))
base::flat_map<std::string,
               scoped_refptr<QueueableResourceState<BufferContent>>>
ToNamedBufferStateMap(
    const base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>>&
        named_tensors) {
  base::flat_map<std::string,
                 scoped_refptr<QueueableResourceState<BufferContent>>>
      buffer_states;
  buffer_states.reserve(named_tensors.size());

  for (const auto& [name, tensor] : named_tensors) {
    auto* coreml_tensor = static_cast<TensorImplCoreml*>(tensor.get());
    buffer_states.emplace(name, coreml_tensor->GetBufferState());
  }

  return buffer_states;
}

}  // namespace

// Represents the collection of resources associated with a particular graph.
// These resources may outlive their associated `GraphImplCoreml` instance while
// executing the graph.
class GraphImplCoreml::ComputeResources
    : public base::RefCountedThreadSafe<ComputeResources> {
 public:
  ComputeResources(
      base::flat_map<std::string, std::string> coreml_name_to_operand_name,
      MLModel* __strong ml_model)
      : coreml_name_to_operand_name_(std::move(coreml_name_to_operand_name)),
        ml_model_(std::move(ml_model)) {
    CHECK(ml_model_);
  }

  void DoDispatch(
      base::flat_map<std::string,
                     scoped_refptr<QueueableResourceState<BufferContent>>>
          named_input_buffer_states,
      base::flat_map<std::string,
                     scoped_refptr<QueueableResourceState<BufferContent>>>
          named_output_buffer_states,
      base::OnceClosure completion_closure,
      ScopedTrace scoped_trace) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    scoped_trace.AddStep("Set up prediction");
    base::ElapsedTimer model_predict_timer;

    NSString* feature_name;
    NSMutableSet* feature_names = [[NSMutableSet alloc] init];
    NSMutableDictionary* feature_values = [[NSMutableDictionary alloc] init];

    if (named_input_buffer_states.empty()) {
      CHECK_EQ(ml_model_.modelDescription.inputDescriptionsByName.count, 1u);

      NSString* placeholder_name =
          base::SysUTF8ToNSString(kPlaceholderInputName);
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
      CHECK_EQ(feature_description.type,
               MLFeatureType::MLFeatureTypeMultiArray);

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

    scoped_trace.AddStep("Trigger prediction");

    // Run the MLModel asynchronously.
    [ml_model_
        predictionFromFeatures:feature_provider
                       options:options
             completionHandler:
                 base::CallbackToBlock(base::BindOnce(
                     &GraphImplCoreml::ComputeResources::DidDispatch, this,
                     std::move(model_predict_timer), std::move(output_backings),
                     std::move(wrapped_completion_closure),
                     std::move(scoped_trace)))];
  }

  void DidDispatch(base::ElapsedTimer model_predict_timer,
                   NSMutableDictionary* output_backing_buffers,
                   base::OnceClosure completion_closure,
                   ScopedTrace scoped_trace,
                   id<MLFeatureProvider> output_features,
                   NSError* error) const {
    scoped_trace.AddStep("Process prediction");
    DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES("WebNN.CoreML.TimingMs."
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
    // claims CoreML may not use the specified backing
    // buffers in a handful of scenarios, including the vague
    // case where "the model doesn't support the user
    // allocated buffers". We shouldn't ship WebNN to users
    // with this CHECK enabled, but in the meantime let's see
    // if this check is ever hit...
    NSString* output_feature_name;
    for (output_feature_name in output_features.featureNames) {
      CHECK_EQ([output_features featureValueForName:output_feature_name]
                   .multiArrayValue,
               output_backing_buffers[output_feature_name]);
    }
  }

 private:
  friend class base::RefCountedThreadSafe<ComputeResources>;

  ~ComputeResources() = default;

  SEQUENCE_CHECKER(sequence_checker_);

  const base::flat_map<std::string, std::string> coreml_name_to_operand_name_;
  const MLModel* __strong ml_model_;
};

// Parameters needed to construct a `GraphImplCoreml`. Used for shuttling
// these objects between the background thread where the model is compiled and
// the originating thread.
struct GraphImplCoreml::Params {
  Params(ComputeResourceInfo compute_resource_info,
         base::flat_map<std::string, std::string> coreml_name_to_operand_name);
  ~Params();

  ComputeResourceInfo compute_resource_info;
  base::flat_map<std::string, std::string> coreml_name_to_operand_name;

  // Represents the compiled and configured Core ML model. This member must be
  // set before these params are used to construct a new `GraphImplCoreml`.
  MLModel* __strong ml_model;

  std::vector<mojom::Device> devices;
};

// static
void GraphImplCoreml::CreateAndBuild(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    ContextImplCoreml* context,
    mojom::GraphInfoPtr graph_info,
    ComputeResourceInfo compute_resource_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    base::flat_map<OperandId, WebNNTensorImpl*> constant_tensor_operands,
    mojom::CreateContextOptionsPtr context_options,
    ContextProperties context_properties,
    WebNNContextImpl::CreateGraphImplCallback callback) {
  auto wrapped_callback = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&GraphImplCoreml::DidCreateAndBuild, std::move(receiver),
                     context->AsWeakPtr(), std::move(callback)));

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
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
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
          *graph_info.get(), std::move(context_properties),
          context_options->device, std::move(constant_operands),
          model_file_dir.GetPath()),
      [&](mojom::ErrorPtr error) {
        std::move(callback).Run(base::unexpected(std::move(error)));
        return;
      });
  DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES(
      "WebNN.CoreML.TimingMs.MLModelTranslate", ml_model_write_timer.Elapsed());

  // Create a map of the names used internally by CoreML to the names used
  // externally by WebNN for all inputs and outputs.
  std::vector<std::pair<std::string, std::string>> coreml_name_to_operand_name(
      graph_info->input_operands.size() + graph_info->output_operands.size());
  for (auto const& input_id : graph_info->input_operands) {
    auto& name = graph_info->operands.at(input_id.value())->name;
    CHECK(name.has_value());
    coreml_name_to_operand_name.emplace_back(
        GetCoreMLNameFromInput(name.value(), input_id), name.value());
  }
  for (auto const& output_id : graph_info->output_operands) {
    auto& name = graph_info->operands.at(output_id.value())->name;
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
  DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES("WebNN.CoreML.TimingMs.MLModelCompile",
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
  ScopedModelPath scoped_model_files{std::move(model_file_dir)};
  ScopedModelPath scoped_compiled_model_files{
      std::move(scoped_compiled_model_dir)};

  if (error) {
    LOG(ERROR) << "[WebNN] " << error;
    std::move(callback).Run(base::unexpected(mojom::Error::New(
        mojom::Error::Code::kUnknownError, "Model compilation error.")));
    return;
  }

  MLModelConfiguration* configuration = [[MLModelConfiguration alloc] init];
  switch (context_options->device) {
    case mojom::Device::kCpu:
      configuration.computeUnits = MLComputeUnitsCPUOnly;
      break;
    case mojom::Device::kGpu:
      configuration.computeUnits =
          base::FeatureList::IsEnabled(
              mojom::features::kWebNNCoreMLExplicitGPUOrNPU)
              ? MLComputeUnitsCPUAndGPU
              : MLComputeUnitsAll;
      break;
    case mojom::Device::kNpu:
      configuration.computeUnits =
          base::FeatureList::IsEnabled(
              mojom::features::kWebNNCoreMLExplicitGPUOrNPU)
              ? MLComputeUnitsCPUAndNeuralEngine
              : MLComputeUnitsAll;
      break;
  }

  base::ElapsedTimer model_load_timer;
  NSError* model_load_error = nil;

  params->ml_model = [MLModel modelWithContentsOfURL:compiled_model_url
                                       configuration:configuration
                                               error:&model_load_error];
  DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES(
      "WebNN.CoreML.TimingMs.CompiledModelLoad", model_load_timer.Elapsed());
  if (model_load_error) {
    LOG(ERROR) << "[WebNN] " << model_load_error;
    std::move(callback).Run(base::unexpected(mojom::Error::New(
        mojom::Error::Code::kUnknownError, "Model load error.")));
    return;
  }
  [MLComputePlan
      loadContentsOfURL:compiled_model_url
          configuration:configuration
      completionHandler:base::CallbackToBlock(base::BindOnce(
                            &ReadComputePlan, std::move(params),
                            std::move(callback),
                            std::move(scoped_compiled_model_files)))];
}

// static
void GraphImplCoreml::ReadComputePlan(
    std::unique_ptr<Params> params,
    base::OnceCallback<void(
        base::expected<std::unique_ptr<Params>, mojom::ErrorPtr>)> callback,
    ScopedModelPath scoped_model_files,
    MLComputePlan* compute_plan,
    NSError* compute_plan_error) {
  if (compute_plan_error) {
    LOG(ERROR) << "[WebNN] " << compute_plan_error;
    std::move(callback).Run(base::unexpected(
        mojom::Error::New(mojom::Error::Code::kUnknownError,
                          "Failed to get compiled graph devices.")));
    return;
  }
  CHECK(compute_plan);

  MLModelStructureProgram* program = compute_plan.modelStructure.program;
  CHECK(program);

  MLModelStructureProgramFunction* main_function = program.functions[@"main"];
  CHECK(main_function);

  double total_weight = 0;
  NSArray<MLModelStructureProgramOperation*>* operations =
      main_function.block.operations;
  base::EnumSet<mojom::Device, mojom::Device::kCpu, mojom::Device::kNpu>
      devices;
  DLOG(INFO) << "[WebNN] Getting CoreML compute plan.";
  for (MLModelStructureProgramOperation* operation in operations) {
    // Get the compute device usage for the operation.
    MLComputePlanDeviceUsage* compute_device_usage =
        [compute_plan computeDeviceUsageForMLProgramOperation:operation];
    id<MLComputeDeviceProtocol> preferred_device =
        compute_device_usage.preferredComputeDevice;
    if (!preferred_device) {
      // This can happen on a 0 weight operation.
      DLOG(INFO) << operation.operatorName << " no preferred device";
    } else if ([preferred_device isKindOfClass:[MLCPUComputeDevice class]]) {
      DLOG(INFO) << operation.operatorName << " prefers CPU";
      devices.Put(mojom::Device::kCpu);
    } else if ([preferred_device isKindOfClass:[MLGPUComputeDevice class]]) {
      DLOG(INFO) << operation.operatorName << " prefers GPU";
      devices.Put(mojom::Device::kGpu);
    } else if ([preferred_device
                   isKindOfClass:[MLNeuralEngineComputeDevice class]]) {
      DLOG(INFO) << operation.operatorName << " prefers ANE";
      devices.Put(mojom::Device::kNpu);
    } else {
      NOTREACHED();
    }

    if (DLOG_IS_ON(INFO)) {
      std::string supported_devices;
      for (id<MLComputeDeviceProtocol> device in compute_device_usage
               .supportedComputeDevices) {
        if (!device) {
          continue;
        }
        if ([device isKindOfClass:[MLCPUComputeDevice class]]) {
          supported_devices += " CPU";
        } else if ([device isKindOfClass:[MLGPUComputeDevice class]]) {
          supported_devices += " GPU";
        } else if ([device isKindOfClass:[MLNeuralEngineComputeDevice class]]) {
          supported_devices += " ANE";
        } else {
          NOTREACHED();
        }
      }
      DLOG(INFO) << operation.operatorName
                 << " supported devices:" << supported_devices;
    }
    // Get the estimated cost of executing the operation.
    MLComputePlanCost* estimated_cost =
        [compute_plan estimatedCostOfMLProgramOperation:operation];
    DLOG(INFO) << "Operation weight " << estimated_cost.weight;
    total_weight += estimated_cost.weight;
  }
  params->devices.assign(devices.begin(), devices.end());
  DLOG(INFO) << "Total weight " << total_weight;
  std::move(callback).Run(std::move(params));
}

// static
void GraphImplCoreml::DidCreateAndBuild(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
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

  std::move(callback).Run(base::MakeRefCounted<GraphImplCoreml>(
      std::move(receiver), std::move(context), *std::move(result)));
}

GraphImplCoreml::ScopedModelPath::ScopedModelPath(base::ScopedTempDir file_dir)
    : file_dir(std::move(file_dir)) {}

GraphImplCoreml::ScopedModelPath::~ScopedModelPath() {
  if (!file_dir.IsValid()) {
    return;
  }

#if BUILDFLAG(IS_MAC)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNCoreMlDumpModel)) {
    const auto dump_directory =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            switches::kWebNNCoreMlDumpModel);
    LOG(INFO) << "[WebNN] Copying model files to " << dump_directory;
    if (dump_directory.empty()) {
      LOG(ERROR) << "[WebNN] Dump directory not specified.";
    } else {
      if (!base::CopyDirectory(file_dir.GetPath(), dump_directory,
                               /*recursive=*/true)) {
        LOG(ERROR) << "[WebNN] Failed to copy model file directory.";
      }
    }
  }
#endif
  // Though the destructors of ScopedTempDir will delete these directories.
  // Explicitly delete them here to check for success.
  CHECK(file_dir.Delete());
}

GraphImplCoreml::GraphImplCoreml(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    base::WeakPtr<WebNNContextImpl> context,
    std::unique_ptr<Params> params)
    : WebNNGraphImpl(std::move(receiver),
                     std::move(context),
                     std::move(params->compute_resource_info),
                     std::move(params->devices)),
      compute_resources_(base::MakeRefCounted<ComputeResources>(
          std::move(params->coreml_name_to_operand_name),
          params->ml_model)) {}

GraphImplCoreml::~GraphImplCoreml() = default;

void GraphImplCoreml::DispatchImpl(
    base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>> named_inputs,
    base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>> named_outputs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ScopedTrace scoped_trace("GraphImplCoreml::DispatchImpl");

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
  std::ranges::transform(
      named_input_buffer_states, std::back_inserter(shared_resources),
      [](const auto& name_and_state) { return name_and_state.second; });

  // Exclusively reserve all output tensors, which will be written to.
  std::vector<scoped_refptr<QueueableResourceStateBase>> exclusive_resources;
  exclusive_resources.reserve(named_outputs.size());
  std::ranges::transform(
      named_output_buffer_states, std::back_inserter(exclusive_resources),
      [](const auto& name_and_state) { return name_and_state.second; });

  scoped_trace.AddStep("Acquire resources");
  auto task = base::MakeRefCounted<ResourceTask>(
      std::move(shared_resources), std::move(exclusive_resources),
      base::BindOnce(
          [](scoped_refptr<ComputeResources> compute_resources,
             base::flat_map<
                 std::string,
                 scoped_refptr<QueueableResourceState<BufferContent>>>
                 named_input_buffer_states,
             base::flat_map<
                 std::string,
                 scoped_refptr<QueueableResourceState<BufferContent>>>
                 named_output_buffer_states,
             ScopedTrace scoped_trace, base::OnceClosure completion_closure) {
            compute_resources->DoDispatch(std::move(named_input_buffer_states),
                                          std::move(named_output_buffer_states),
                                          std::move(completion_closure),
                                          std::move(scoped_trace));
          },
          compute_resources_, std::move(named_input_buffer_states),
          std::move(named_output_buffer_states), std::move(scoped_trace)));
  task->Enqueue();
}

GraphImplCoreml::Params::Params(
    ComputeResourceInfo compute_resource_info,
    base::flat_map<std::string, std::string> coreml_name_to_operand_name)
    : compute_resource_info(std::move(compute_resource_info)),
      coreml_name_to_operand_name(std::move(coreml_name_to_operand_name)) {}

GraphImplCoreml::Params::~Params() = default;

}  // namespace webnn::coreml
