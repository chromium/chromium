// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/dispatch_context_impl_ort.h"

#include "base/logging.h"
#include "services/webnn/error.h"
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
    mojo::PendingReceiver<mojom::WebNNModelLoader> model_loader_receiver,
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
    EpDeviceInfo target_device) {
  DCHECK(owning_task_runner->RunsTasksInCurrentSequence());

  // Create the session options on the target EP device.
  auto session_options =
      ort::SessionOptions::CreateForDispatch(target_device, env);

  OrtHardwareDeviceType device_type = WebnnToOrtDeviceType(options->device);
  const EpWorkarounds ep_workarounds = env->GetEpWorkarounds(device_type);

  auto* dispatch_context = new DispatchContextImplOrt(
      std::move(receiver), std::move(model_loader_receiver),
      std::move(context_provider), std::move(ep_workarounds),
      std::move(options), std::move(session_options),
      std::move(write_tensor_consumer), std::move(read_tensor_producer),
      std::move(env), std::move(gpu_task_scheduler), std::move(memory_tracker),
      owning_task_runner, shared_image_manager, std::move(main_task_runner),
      std::move(target_device));

  return std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter>(
      dispatch_context, OnTaskRunnerDeleter(std::move(owning_task_runner)));
}

DispatchContextImplOrt::DispatchContextImplOrt(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    mojo::PendingReceiver<mojom::WebNNModelLoader> model_loader_receiver,
    base::WeakPtr<WebNNContextProviderImpl> context_provider,
    const EpWorkarounds& ep_workarounds,
    mojom::CreateContextOptionsPtr options,
    scoped_refptr<SessionOptions> session_options,
    mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
    mojo::ScopedDataPipeProducerHandle read_tensor_producer,
    scoped_refptr<Environment> env,
    std::unique_ptr<GpuTaskScheduler> gpu_task_scheduler,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    EpDeviceInfo target_device)
    : ContextImplOrt(std::move(receiver),
                     std::move(context_provider),
                     ep_workarounds,
                     std::move(options),
                     std::move(session_options),
                     std::move(write_tensor_consumer),
                     std::move(read_tensor_producer),
                     std::move(env),
                     std::move(gpu_task_scheduler),
                     std::move(memory_tracker),
                     std::move(owning_task_runner),
                     shared_image_manager,
                     std::move(main_task_runner)),
      target_device_(std::move(target_device)) {
  model_loader_receiver_.Bind(std::move(model_loader_receiver));
}

DispatchContextImplOrt::~DispatchContextImplOrt() = default;

void DispatchContextImplOrt::CreateGraphBuilder(
    mojo::PendingReceiver<mojom::WebNNGraphBuilder> /*receiver*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // A dispatch context only exists when the Compiler process is enabled, so
  // graph building must go through the Compiler process. A compromised
  // renderer could try to bypass it by sending CreateGraphBuilder directly
  // to this context; reject it unconditionally.
  ReportBadMessageAndDisconnect(kBadMessageGraphBuilderBypassesCompiler);
}

void DispatchContextImplOrt::RequestCompilerContext(
    mojo::PendingReceiver<mojom::WebNNCompilerContext>
        compiler_context_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Create the ModelLoader pipe pair on this thread (the owning thread where
  // model_loader_receiver_ lives), avoiding cross-thread posting.
  model_loader_receiver_.reset();
  auto model_loader_remote = model_loader_receiver_.BindNewPipeAndPassRemote();

  // Post to the main thread where `context_provider_` (a WeakPtr bound to the
  // main sequence) can be safely dereferenced.
  main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<WebNNContextProviderImpl> context_provider,
             mojom::CreateContextOptionsPtr options,
             ContextProperties properties, EpDeviceInfo target_device,
             mojo::PendingReceiver<mojom::WebNNCompilerContext>
                 compiler_context_receiver,
             mojo::PendingRemote<mojom::WebNNModelLoader> model_loader_remote) {
            if (!context_provider) {
              // Drop the pipe endpoints — peer endpoints will observe a
              // disconnect.
              LOG(ERROR) << "[WebNN] RequestCompilerContext() failed: "
                            "WebNNContextProviderImpl is no longer available.";
              return;
            }
            context_provider->ReconnectCompilerContext(
                std::move(options), properties, std::move(target_device),
                std::move(compiler_context_receiver),
                std::move(model_loader_remote));
          },
          context_provider_, options_->Clone(), properties_, target_device_,
          std::move(compiler_context_receiver),
          std::move(model_loader_remote)));
}

void DispatchContextImplOrt::LoadCompiledGraph(
    mojom::CompiledGraphPtr compiled_graph,
    LoadCompiledGraphCallback callback) {
  auto result = GraphImplOrt::CreateSessionFromCompiledGraph(
      *this, session_options(), env(),
      std::move(compiled_graph->compiled_model_data),
      std::move(compiled_graph->input_binding_names),
      std::move(compiled_graph->output_binding_names));
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
