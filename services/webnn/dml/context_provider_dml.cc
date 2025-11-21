// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/dml/context_provider_dml.h"

#include <wrl.h>

#include "base/check_is_test.h"
#include "base/notreached.h"
#include "base/types/expected_macros.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/command_recorder.h"
#include "services/webnn/dml/context_impl_dml.h"
#include "services/webnn/dml/utils.h"
#include "services/webnn/error.h"
#include "services/webnn/public/mojom/features.mojom.h"
#include "services/webnn/scoped_sequence.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_context_provider_impl.h"

namespace webnn::dml {

namespace {

using Microsoft::WRL::ComPtr;

base::expected<scoped_refptr<Adapter>, mojom::ErrorPtr> GetDmlGpuAdapter(
    IDXGIAdapter* dxgi_adapter,
    const gpu::GpuFeatureInfo& gpu_feature_info) {
  if (gpu_feature_info.IsWorkaroundEnabled(DISABLE_WEBNN_FOR_GPU)) {
    return base::unexpected(CreateError(mojom::Error::Code::kNotSupportedError,
                                        "WebNN is blocklisted for GPU."));
  }

  if (!dxgi_adapter) {
    // Unit tests do not pass in an IDXGIAdapter. `GetGpuInstanceForTesting`
    // will select the default adapter returned by `IDXGIFactory::EnumAdapters`.
    CHECK_IS_TEST();
    return Adapter::GetGpuInstanceForTesting();
  }

  return Adapter::GetGpuInstance(std::move(dxgi_adapter));
}

base::expected<std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter>,
               mojom::ErrorPtr>
CreateDmlContext(scoped_refptr<Adapter> adapter,
                 mojo::PendingReceiver<mojom::WebNNContext> receiver,
                 base::WeakPtr<WebNNContextProviderImpl> context_provider,
                 mojom::CreateContextOptionsPtr options,
                 mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
                 mojo::ScopedDataPipeProducerHandle read_tensor_producer,
                 const gpu::GpuFeatureInfo& gpu_feature_info,
                 gpu::CommandBufferId command_buffer_id,
                 std::unique_ptr<ScopedSequence> sequence,
                 scoped_refptr<gpu::MemoryTracker> memory_tracker,
                 scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
                 gpu::SharedImageManager* shared_image_manager,
                 scoped_refptr<base::SingleThreadTaskRunner> main_task_runner) {
  ASSIGN_OR_RETURN(
      auto command_recorder,
      CommandRecorder::Create(adapter->command_queue(), adapter->dml_device()),
      [](HRESULT hr) {
        return CreateError(mojom::Error::Code::kUnknownError,
                           "Failed to create a CommandRecorder.");
      });
  auto task_runner = owning_task_runner;
  std::unique_ptr<WebNNContextImpl, OnTaskRunnerDeleter> context_impl(
      new ContextImplDml(
          std::move(adapter), std::move(receiver), std::move(context_provider),
          std::move(options), std::move(write_tensor_consumer),
          std::move(read_tensor_producer), std::move(command_recorder),
          gpu_feature_info, command_buffer_id, std::move(sequence),
          std::move(memory_tracker), std::move(owning_task_runner),
          shared_image_manager, std::move(main_task_runner)),
      OnTaskRunnerDeleter(std::move(task_runner)));
  return context_impl;
}

}  // namespace

bool ShouldCreateDmlContext(const mojom::CreateContextOptions& options) {
  if (!base::FeatureList::IsEnabled(mojom::features::kWebNNDirectML)) {
    return false;
  }

  switch (options.device) {
    case mojom::Device::kCpu:
      return false;
    case mojom::Device::kGpu:
    case mojom::Device::kNpu:
      return true;
  }
}

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
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner) {
  DCHECK(gpu_feature_info.IsInitialized());
  if (gpu_feature_info.status_values[gpu::GPU_FEATURE_TYPE_WEBNN] !=
      gpu::kGpuFeatureStatusEnabled) {
    LOG(ERROR) << "[WebNN] is not compatible with device.";
    return base::unexpected(
        CreateError(mojom::Error::Code::kNotSupportedError,
                    "WebNN is not compatible with device."));
  }
  // Get the `Adapter` instance which is created for the adapter according to
  // the device type. At the current stage, all `ContextImpl` share one
  // instance for one device type.
  base::expected<scoped_refptr<Adapter>, mojom::ErrorPtr>
      adapter_creation_result;
  switch (options->device) {
    case mojom::Device::kCpu:
      NOTREACHED();
    case mojom::Device::kGpu: {
      ComPtr<IDXGIAdapter> dxgi_adapter;
      if (shared_context_state) {
        ComPtr<ID3D11Device> d3d11_device =
            shared_context_state->GetD3D11Device();
        if (!d3d11_device) {
          return base::unexpected(
              CreateError(mojom::Error::Code::kNotSupportedError,
                          "Failed to get D3D11 device."));
        }
        ComPtr<IDXGIDevice> dxgi_device;
        // A QueryInterface() via As() from an ID3D11Device to IDXGIDevice is
        // always expected to succeed.
        CHECK_EQ(d3d11_device.As(&dxgi_device), S_OK);

        // The D3D team has promised that asking for an adapter from a valid
        // IDXGIDevice will always succeed.
        CHECK_EQ(dxgi_device->GetAdapter(&dxgi_adapter), S_OK);
        CHECK(dxgi_adapter);
      }
      adapter_creation_result =
          GetDmlGpuAdapter(dxgi_adapter.Get(), gpu_feature_info);
      break;
    }
    case mojom::Device::kNpu:
      adapter_creation_result =
          Adapter::GetNpuInstance(gpu_feature_info, gpu_info);
      break;
  }
  if (!adapter_creation_result.has_value()) {
    return base::unexpected(std::move(adapter_creation_result.error()));
  }

  return CreateDmlContext(
      std::move(adapter_creation_result.value()), std::move(receiver),
      std::move(context_provider), std::move(options),
      std::move(write_tensor_consumer), std::move(read_tensor_producer),
      gpu_feature_info, command_buffer_id, std::move(sequence),
      std::move(memory_tracker), std::move(owning_task_runner),
      shared_image_manager, std::move(main_task_runner));
}
}  // namespace webnn::dml
