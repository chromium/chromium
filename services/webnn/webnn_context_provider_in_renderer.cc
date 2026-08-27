// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_provider_in_renderer.h"

#include <utility>

#include "base/check.h"
#include "base/task/thread_pool.h"
#include "services/webnn/buildflags.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"

#if BUILDFLAG(WEBNN_USE_LITERT)
#include "services/webnn/tflite/context_impl_litert.h"  // nogncheck
#endif

namespace webnn {

WebNNContextProviderInRenderer::WebNNContextProviderInRenderer(
    mojo::PendingRemote<mojom::WebNNWeightsFileCreator>
        weights_file_creator_remote,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    WebGPUContextHelperCallback webgpu_context_helper)
    : main_task_runner_(std::move(main_task_runner)),
      webgpu_context_helper_(std::move(webgpu_context_helper)) {
  // Bind the SharedRemote explicitly to `main_task_runner_` so that the
  // underlying Mojo Remote lives on the renderer's main thread.
  shared_weights_file_creator_.Bind(std::move(weights_file_creator_remote),
                                    main_task_runner_);
}
WebNNContextProviderInRenderer::~WebNNContextProviderInRenderer() = default;

void WebNNContextProviderInRenderer::CreateWebNNContext(
    mojom::CreateContextOptionsPtr options,
    CreateWebNNContextCallback callback) {
  // Force the device to CPU for non-GPU requests (e.g. NPU).
  if (options->device != mojom::Device::kGpu) {
    options->device = mojom::Device::kCpu;
  }
  mojo::PendingRemote<mojom::WebNNContext> remote;
  auto receiver = remote.InitWithNewPipeAndPassReceiver();

  scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner =
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

#if BUILDFLAG(WEBNN_USE_LITERT)
  if (options->device == mojom::Device::kGpu && webgpu_context_helper_) {
    // Forward the WebGPU initialization request directly to the worker thread.
    // When WebGPU context initialization completes on the worker thread, the
    // callback executes directly on the worker thread without hopping back to
    // the main thread, moving `webgpu_properties` directly into the context.
    webgpu_context_helper_.Run(
        owning_task_runner,
        base::BindOnce(&WebNNContextProviderInRenderer::CreateContextOnWorker,
                       GetWeakPtr(), std::move(options), std::move(callback),
                       std::move(remote), std::move(receiver),
                       owning_task_runner, main_task_runner_));
    return;
  }

  owning_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&WebNNContextProviderInRenderer::CreateContextOnWorker,
                     GetWeakPtr(), std::move(options), std::move(callback),
                     std::move(remote), std::move(receiver), owning_task_runner,
                     main_task_runner_, WebGpuContextProperties()));
#else
  std::move(callback).Run(ToError<mojom::CreateContextResult>(
      mojom::Error::Code::kNotSupportedError,
      "WebNN is not supported on this platform."));
#endif  // BUILDFLAG(WEBNN_USE_LITERT)
}

#if BUILDFLAG(WEBNN_USE_LITERT)
// static
void WebNNContextProviderInRenderer::CreateContextOnWorker(
    base::WeakPtr<WebNNContextProviderInRenderer> context_provider,
    mojom::CreateContextOptionsPtr options,
    CreateWebNNContextCallback callback,
    mojo::PendingRemote<mojom::WebNNContext> remote,
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    WebGpuContextProperties webgpu_properties) {
  DCHECK(owning_task_runner->RunsTasksInCurrentSequence());
  if (options->device == mojom::Device::kGpu && !webgpu_properties.IsValid()) {
    main_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            ToError<mojom::CreateContextResult>(
                mojom::Error::Code::kNotSupportedError,
                "Failed to obtain WebGPU context for LiteRT in renderer.")));
    return;
  }

  // Create the ContextImplLiteRt on the owning_task_runner and post the result
  // back to the main thread to complete the callback.
  auto context_impl = litert::ContextImplLiteRt::CreateForRenderer(
      std::move(receiver), context_provider, std::move(options),
      std::move(webgpu_properties), owning_task_runner, main_task_runner);

  main_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&WebNNContextProviderInRenderer::OnCreateWebNNContextImpl,
                     context_provider, std::move(callback), std::move(remote),
                     std::move(context_impl)));
}
#endif  // BUILDFLAG(WEBNN_USE_LITERT)

void WebNNContextProviderInRenderer::OnCreateWebNNContextImpl(
    CreateWebNNContextCallback callback,
    mojo::PendingRemote<mojom::WebNNContext> remote,
    WebNNContextImpl::WebNNContextImplPtr context_impl) {
  if (!context_impl) {
    std::move(callback).Run(ToError<mojom::CreateContextResult>(
        mojom::Error::Code::kNotSupportedError,
        "WebNN is not supported on this platform."));
    return;
  }

  ContextProperties context_properties = context_impl->properties();
  const blink::WebNNContextToken& context_handle = context_impl->handle();

  context_impls_.emplace(std::move(context_impl));

  // TODO(crbug.com/504319596): Support WebNN introspection for the in-process
  // TFLite backend.
  auto success = mojom::CreateContextSuccess::New(
      std::move(remote), /*compiler_context_remote=*/mojo::NullRemote(),
      std::move(context_properties), std::move(context_handle),
      // Data pipes are not needed when TFLite runs in-process with the
      // renderer, since tensor data does not cross a process boundary.
      /*write_tensor_producer=*/mojo::ScopedDataPipeProducerHandle(),
      /*read_tensor_consumer=*/mojo::ScopedDataPipeConsumerHandle(),
      /*command_buffer_id=*/0u);
  std::move(callback).Run(
      mojom::CreateContextResult::NewSuccess(std::move(success)));
}

void WebNNContextProviderInRenderer::OpenWeightsFile(
    base::OnceCallback<void(base::File,
                            mojo::PendingRemote<mojom::WeightsFileSession>)>
        callback) {
  if (!shared_weights_file_creator_.is_bound()) {
    std::move(callback).Run(base::File(),
                            mojo::PendingRemote<mojom::WeightsFileSession>());
    return;
  }
  shared_weights_file_creator_->OpenWeightsFile(std::move(callback));
}

void WebNNContextProviderInRenderer::RemoveWebNNContextImpl(
    const blink::WebNNContextToken& handle) {
  auto it = context_impls_.find(handle);
  CHECK(it != context_impls_.end());
  context_impls_.erase(it);
}

}  // namespace webnn
