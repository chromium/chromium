// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_CONTEXT_PROVIDER_DML_H_
#define SERVICES_WEBNN_DML_CONTEXT_PROVIDER_DML_H_

#include "base/types/expected.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/webnn_context_impl.h"

namespace gpu {
class SharedContextState;
struct GpuFeatureInfo;
struct GPUInfo;
class MemoryTracker;
class SharedImageManager;
}  // namespace gpu

namespace webnn {

class ScopedSequence;
class WebNNContextProviderImpl;

namespace dml {

bool ShouldCreateDmlContext(const mojom::CreateContextOptions& options);

// Create a WebNN context that satisfies the requested preferences in a
// CreateContextOptions. This corresponds to the
// ML.createContext(MLContextOptions) overload in the WebNN API.
base::expected<std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter>,
               mojom::ErrorPtr>
CreateContextFromOptions(
    mojom::CreateContextOptionsPtr options,
    mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
    mojo::ScopedDataPipeProducerHandle read_tensor_producer,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const gpu::GPUInfo& gpu_info,
    const gpu::SharedContextState* shared_context_state,
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    base::WeakPtr<WebNNContextProviderImpl> context_provider,
    gpu::CommandBufferId command_buffer_id,
    std::unique_ptr<ScopedSequence> sequence,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);

}  // namespace dml

}  // namespace webnn

#endif  // SERVICES_WEBNN_DML_CONTEXT_PROVIDER_DML_H_
