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

#if BUILDFLAG(WEBNN_USE_TFLITE)
#include "services/webnn/tflite/context_impl_tflite.h"  // nogncheck
#endif  // BUILDFLAG(WEBNN_USE_TFLITE)

#if BUILDFLAG(WEBNN_USE_LITERT)
#include "base/feature_list.h"
#include "services/webnn/public/mojom/features.mojom-features.h"
#include "services/webnn/tflite/context_impl_litert.h"  // nogncheck
#endif

namespace webnn {

WebNNContextProviderInRenderer::WebNNContextProviderInRenderer(
    mojo::PendingRemote<mojom::WebNNWeightsFileCreator>
        weights_file_creator_remote,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : main_task_runner_(std::move(main_task_runner)) {
  // Bind the SharedRemote explicitly to `main_task_runner_` so that the
  // underlying Mojo Remote lives on the renderer's main thread.
  shared_weights_file_creator_.Bind(std::move(weights_file_creator_remote),
                                    main_task_runner_);
}
WebNNContextProviderInRenderer::~WebNNContextProviderInRenderer() = default;

void WebNNContextProviderInRenderer::CreateWebNNContext(
    mojom::CreateContextOptionsPtr options,
    CreateWebNNContextCallback callback) {
  // This provider is the renderer-process fallback that
  // `WebNNContextProviderImpl` routes to after the GPU process has either
  // declined the request or failed to create a context.
  //
  // Force the device to CPU since this provider runs in the renderer process
  // without access to GPU/NPU resources.
  options->device = mojom::Device::kCpu;

  mojo::PendingRemote<mojom::WebNNContext> remote;
  auto receiver = remote.InitWithNewPipeAndPassReceiver();

  scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner =
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  // Post context creation to the owning_task_runner so the Mojo receiver is
  // bound on the correct sequence (matching the GPU-process pattern).
#if BUILDFLAG(WEBNN_USE_LITERT)
  if (base::FeatureList::IsEnabled(mojom::features::kWebNNLiteRT)) {
    owning_task_runner->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&litert::ContextImplLiteRt::CreateForRenderer,
                       std::move(receiver), GetWeakPtr(), std::move(options),
                       owning_task_runner, main_task_runner_),
        base::BindOnce(
            &WebNNContextProviderInRenderer::OnCreateWebNNContextImpl,
            GetWeakPtr(), std::move(callback), std::move(remote)));
    return;
  }
#endif  // BUILDFLAG(WEBNN_USE_LITERT)

#if BUILDFLAG(WEBNN_USE_TFLITE)
  owning_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&tflite::ContextImplTflite::CreateForRenderer,
                     std::move(receiver), GetWeakPtr(), std::move(options),
                     owning_task_runner, main_task_runner_),
      base::BindOnce(&WebNNContextProviderInRenderer::OnCreateWebNNContextImpl,
                     GetWeakPtr(), std::move(callback), std::move(remote)));
#else
  std::move(callback).Run(ToError<mojom::CreateContextResult>(
      mojom::Error::Code::kNotSupportedError,
      "WebNN is not supported on this platform."));
#endif  // BUILDFLAG(WEBNN_USE_TFLITE)
}

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

void WebNNContextProviderInRenderer::CreateWeightsFile(
    base::OnceCallback<void(base::File)> callback) {
  if (!shared_weights_file_creator_.is_bound()) {
    std::move(callback).Run(base::File());
    return;
  }
  shared_weights_file_creator_->CreateWeightsFile(std::move(callback));
}

void WebNNContextProviderInRenderer::RemoveWebNNContextImpl(
    const blink::WebNNContextToken& handle) {
  auto it = context_impls_.find(handle);
  CHECK(it != context_impls_.end());
  context_impls_.erase(it);
}

}  // namespace webnn
