// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/context_provider_tflite.h"

#include <utility>

#include "base/check.h"
#include "base/task/thread_pool.h"
#include "services/webnn/error.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/tflite/context_impl_tflite.h"
#include "services/webnn/tflite/graph_builder_tflite.h"

namespace webnn::tflite {

ContextProviderTflite::ContextProviderTflite(
    mojo::PendingRemote<mojom::WebNNWeightsFileCreator>
        weights_file_creator_remote,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : main_task_runner_(std::move(main_task_runner)) {
  // Bind the SharedRemote explicitly to `main_task_runner_` so that the
  // underlying Mojo Remote lives on the renderer's main thread.
  shared_weights_file_creator_.Bind(std::move(weights_file_creator_remote),
                                    main_task_runner_);
}
ContextProviderTflite::~ContextProviderTflite() = default;

void ContextProviderTflite::CreateWebNNContext(
    mojom::CreateContextOptionsPtr options,
    CreateWebNNContextCallback callback) {
  if (options->device != mojom::Device::kCpu) {
    std::move(callback).Run(ToError<mojom::CreateContextResult>(
        mojom::Error::Code::kNotSupportedError,
        "Only CPU device is supported for TFLite backend in this process."));
    return;
  }

  mojo::PendingRemote<mojom::WebNNContext> remote;
  auto receiver = remote.InitWithNewPipeAndPassReceiver();

  scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner =
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  // Post context creation to the owning_task_runner so the Mojo receiver is
  // bound on the correct sequence (matching the GPU-process pattern).
  owning_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ContextImplTflite::CreateForRenderer, std::move(receiver),
                     GetWeakPtr(), std::move(options), owning_task_runner,
                     main_task_runner_),
      base::BindOnce(&ContextProviderTflite::OnCreateWebNNContextImpl,
                     GetWeakPtr(), std::move(callback), std::move(remote)));
}

void ContextProviderTflite::OnCreateWebNNContextImpl(
    CreateWebNNContextCallback callback,
    mojo::PendingRemote<mojom::WebNNContext> remote,
    WebNNContextImpl::WebNNContextImplPtr context_impl) {
  if (!context_impl) {
    std::move(callback).Run(ToError<mojom::CreateContextResult>(
        mojom::Error::Code::kNotSupportedError,
        "Failed to create TFLite context."));
    return;
  }

  ContextProperties context_properties = context_impl->properties();
  const blink::WebNNContextToken& context_handle = context_impl->handle();

  context_impls_.emplace(std::move(context_impl));

  // TODO(crbug.com/504319596): Support WebNN introspection for the in-process
  // TFLite backend.
  auto success = mojom::CreateContextSuccess::New(
      std::move(remote), std::move(context_properties),
      std::move(context_handle),
      // Data pipes are not needed when TFLite runs in-process with the
      // renderer, since tensor data does not cross a process boundary.
      /*write_tensor_producer=*/mojo::ScopedDataPipeProducerHandle(),
      /*read_tensor_consumer=*/mojo::ScopedDataPipeConsumerHandle(),
      /*command_buffer_id=*/0);
  std::move(callback).Run(
      mojom::CreateContextResult::NewSuccess(std::move(success)));
}

void ContextProviderTflite::CreateWeightsFile(
    base::OnceCallback<void(base::File)> callback) {
  if (!shared_weights_file_creator_.is_bound()) {
    std::move(callback).Run(base::File());
    return;
  }
  shared_weights_file_creator_->CreateWeightsFile(std::move(callback));
}

void ContextProviderTflite::RemoveWebNNContextImpl(
    const blink::WebNNContextToken& handle) {
  auto it = context_impls_.find(handle);
  CHECK(it != context_impls_.end());
  context_impls_.erase(it);
}

}  // namespace webnn::tflite
