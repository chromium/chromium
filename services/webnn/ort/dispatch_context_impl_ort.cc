// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/dispatch_context_impl_ort.h"

#include "base/containers/flat_map.h"
#include "services/webnn/gpu_task_scheduler.h"
#include "services/webnn/ort/graph_impl_ort.h"
#include "services/webnn/ort/ort_data_type.h"
#include "services/webnn/ort/ort_session_options.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn::ort {

// static
std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter>
DispatchContextImplOrt::Create(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    base::WeakPtr<WebNNContextProviderImpl> context_provider,
    mojom::CreateContextOptionsPtr options,
    mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
    mojo::ScopedDataPipeProducerHandle read_tensor_producer,
    scoped_refptr<Environment> env,
    std::unique_ptr<GpuTaskScheduler> gpu_task_scheduler,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    ScopedTrace scoped_trace) {
  DCHECK(owning_task_runner->RunsTasksInCurrentSequence());

  auto task_runner = owning_task_runner;
  OrtHardwareDeviceType device_type = WebnnToOrtDeviceType(options->device);
  const EpWorkarounds ep_workarounds = env->GetEpWorkarounds(device_type);
  scoped_refptr<SessionOptions> session_options =
      SessionOptions::Create(device_type, env);
  bool dequantize_linear_input_support_int32 = Environment::IsEpDevice(
      session_options->first_selected_device(), {kOpenVINOExecutionProvider});

  std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter> context_impl(
      new DispatchContextImplOrt(
          std::move(receiver), std::move(context_provider),
          std::move(ep_workarounds), dequantize_linear_input_support_int32,
          std::move(options), std::move(session_options),
          std::move(write_tensor_consumer), std::move(read_tensor_producer),
          std::move(env), std::move(gpu_task_scheduler),
          std::move(memory_tracker), std::move(owning_task_runner),
          shared_image_manager, std::move(main_task_runner)),
      OnTaskRunnerDeleter(std::move(task_runner)));
  return context_impl;
}

DispatchContextImplOrt::DispatchContextImplOrt(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    base::WeakPtr<WebNNContextProviderImpl> context_provider,
    const EpWorkarounds& ep_workarounds,
    bool dequantize_linear_input_support_int32,
    mojom::CreateContextOptionsPtr options,
    scoped_refptr<SessionOptions> session_options,
    mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
    mojo::ScopedDataPipeProducerHandle read_tensor_producer,
    scoped_refptr<Environment> env,
    std::unique_ptr<GpuTaskScheduler> gpu_task_scheduler,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : ContextImplOrt(std::move(receiver),
                     std::move(context_provider),
                     ep_workarounds,
                     dequantize_linear_input_support_int32,
                     std::move(options),
                     std::move(session_options),
                     std::move(write_tensor_consumer),
                     std::move(read_tensor_producer),
                     std::move(env),
                     std::move(gpu_task_scheduler),
                     std::move(memory_tracker),
                     std::move(owning_task_runner),
                     shared_image_manager,
                     std::move(main_task_runner)) {}

DispatchContextImplOrt::~DispatchContextImplOrt() = default;

base::WeakPtr<WebNNContextImpl> DispatchContextImplOrt::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

base::WeakPtr<DispatchContextImplOrt> DispatchContextImplOrt::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void DispatchContextImplOrt::BindModelLoader(
    mojo::PendingReceiver<mojom::WebNNModelLoader> receiver) {
  model_loader_receiver_.reset();
  model_loader_receiver_.Bind(std::move(receiver));
}

void DispatchContextImplOrt::LoadCompiledGraph(
    mojom::CompiledGraphPtr compiled_graph,
    mojo::PendingReceiver<mojom::WebNNGraph> graph_receiver,
    LoadCompiledGraphCallback callback) {
  // Split CompiledOperandDescriptor maps into separate binding name maps
  // and descriptor maps for ComputeResourceInfo and session creation.

  // Inputs
  std::vector<std::pair<std::string, OperandDescriptor>> input_desc_pairs;
  std::vector<std::pair<std::string, std::string>> input_name_pairs;
  input_desc_pairs.reserve(compiled_graph->inputs.size());
  input_name_pairs.reserve(compiled_graph->inputs.size());
  for (auto& [name, desc] : compiled_graph->inputs) {
    input_name_pairs.emplace_back(name, std::move(desc->binding_name));
    input_desc_pairs.emplace_back(name, std::move(desc->descriptor));
  }
  base::flat_map<std::string, OperandDescriptor> input_descriptors(
      std::move(input_desc_pairs));
  base::flat_map<std::string, std::string> input_binding_names(
      std::move(input_name_pairs));

  // Outputs
  std::vector<std::pair<std::string, OperandDescriptor>> output_desc_pairs;
  std::vector<std::pair<std::string, std::string>> output_name_pairs;
  output_desc_pairs.reserve(compiled_graph->outputs.size());
  output_name_pairs.reserve(compiled_graph->outputs.size());
  for (auto& [name, desc] : compiled_graph->outputs) {
    output_name_pairs.emplace_back(name, std::move(desc->binding_name));
    output_desc_pairs.emplace_back(name, std::move(desc->descriptor));
  }
  base::flat_map<std::string, OperandDescriptor> output_descriptors(
      std::move(output_desc_pairs));
  base::flat_map<std::string, std::string> output_binding_names(
      std::move(output_name_pairs));

  // Build ComputeResourceInfo from the I/O descriptors sent by Compiler.
  WebNNGraphImpl::ComputeResourceInfo compute_resource_info(
      std::move(input_descriptors), std::move(output_descriptors),
      base::PassKey<DispatchContextImplOrt>());

  auto result = GraphImplOrt::CreateSessionFromCompiledGraph(
      std::move(graph_receiver), *this, std::move(compute_resource_info),
      session_options(), env(), std::move(compiled_graph->compiled_model_data),
      std::move(input_binding_names), std::move(output_binding_names));

  if (!result.has_value()) {
    std::move(callback).Run(base::unexpected(std::move(result.error())));
    return;
  }

  auto& graph_impl = result.value();
  auto graph_token = graph_impl->handle();
  auto devices = graph_impl->devices();
  AddGraphImpl(std::move(graph_impl));

  auto success = mojom::LoadedGraphInfo::New(graph_token, std::move(devices));
  std::move(callback).Run(std::move(success));
}

}  // namespace webnn::ort
