// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_test_environment.h"

#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "services/viz/privileged/mojom/gl/gpu_host.mojom.h"
#include "services/webnn/host/weights_file_provider.h"

namespace webnn::test {

FakeGpuHostForTesting::FakeGpuHostForTesting() : receiver_(this) {}

FakeGpuHostForTesting::~FakeGpuHostForTesting() = default;

void FakeGpuHostForTesting::Bind(
    mojo::PendingReceiver<viz::mojom::GpuHost> receiver) {
  receiver_.Bind(std::move(receiver));
}

void FakeGpuHostForTesting::DidInitialize(
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const std::optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu,
    const std::optional<gpu::GpuFeatureInfo>& gpu_feature_info_for_hardware_gpu,
    const gfx::GpuExtraInfo& gpu_extra_info) {}

void FakeGpuHostForTesting::DidFailInitialize() {}

void FakeGpuHostForTesting::DidCreateContextSuccessfully() {}

void FakeGpuHostForTesting::DidCreateOffscreenContext(const GURL& url) {}

void FakeGpuHostForTesting::DidDestroyOffscreenContext(const GURL& url) {}

void FakeGpuHostForTesting::DidDestroyChannel(int32_t client_id) {}

void FakeGpuHostForTesting::DidDestroyAllChannels() {}

void FakeGpuHostForTesting::DidLoseContext(gpu::error::ContextLostReason reason,
                                           const GURL& active_url) {}

void FakeGpuHostForTesting::DisableGpuCompositing() {}

void FakeGpuHostForTesting::DidUpdateGPUInfo(const gpu::GPUInfo& gpu_info) {}

void FakeGpuHostForTesting::GetIsolationKey(
    int32_t client_id,
    const blink::WebGPUExecutionContextToken& token,
    GetIsolationKeyCallback cb) {}

void FakeGpuHostForTesting::StoreBlobToDisk(
    const gpu::GpuDiskCacheHandle& handle,
    const std::string& key,
    const std::string& shader) {}

void FakeGpuHostForTesting::ClearGrShaderDiskCache() {}

#if BUILDFLAG(IS_WIN)
void FakeGpuHostForTesting::DidUpdateOverlayInfo(
    const gpu::OverlayInfo& overlay_info) {}

void FakeGpuHostForTesting::DidUpdateDXGIInfo(
    gfx::mojom::DXGIInfoPtr dxgi_info) {}

void FakeGpuHostForTesting::EnsureWebNNExecutionProvidersReady(
    EnsureWebNNExecutionProvidersReadyCallback callback) {
  // Initializes the execution providers used by the WebNN ORT backend.
  webnn::EnsureExecutionProvidersReady(std::move(callback));
}
#endif

void FakeGpuHostForTesting::CreateWebNNWeightsFile(
    CreateWebNNWeightsFileCallback callback) {
  webnn::CreateWeightsFile(std::move(callback));
}

WebNNTestEnvironment::WebNNTestEnvironment(
    WebNNContextProviderImpl::WebNNStatus status,
    WebNNContextProviderImpl::LoseAllContextsCallback
        lose_all_contexts_callback,
    std::unique_ptr<base::test::TaskEnvironment> task_environment)
    : task_environment_(std::move(task_environment)) {
  gpu::GpuFeatureInfo gpu_feature_info;
  gpu::GPUInfo gpu_info;

  for (auto& status_value : gpu_feature_info.status_values) {
    status_value = gpu::GpuFeatureStatus::kGpuFeatureStatusDisabled;
  }
  if (status !=
      WebNNContextProviderImpl::WebNNStatus::kWebNNGpuFeatureStatusDisabled) {
    gpu_feature_info.status_values[gpu::GPU_FEATURE_TYPE_WEBNN] =
        gpu::kGpuFeatureStatusEnabled;
  }
  if (status == WebNNContextProviderImpl::WebNNStatus::kWebNNGpuDisabled) {
    gpu_feature_info.enabled_gpu_driver_bug_workarounds.push_back(
        DISABLE_WEBNN_FOR_GPU);
  }
  if (status == WebNNContextProviderImpl::WebNNStatus::kWebNNNpuDisabled) {
    gpu_feature_info.enabled_gpu_driver_bug_workarounds.push_back(
        DISABLE_WEBNN_FOR_NPU);
  }

  // Initialize a Gpu Scheduler so tests can also use a scheduler
  // runner without the Gpu service. We only need to initialize once for the
  // whole GPU process and no teardown logic is needed, so use a global
  // singleton here. The sync point manager must come first since it is
  // passed to the scheduler as a naked pointer.
  static base::NoDestructor<gpu::SyncPointManager> g_webnn_sync_point_manager;
  static base::NoDestructor<gpu::Scheduler> g_webnn_scheduler{
      g_webnn_sync_point_manager.get()};

  mojo::PendingRemote<viz::mojom::GpuHost> gpu_host_proxy;
  fake_gpu_host_.Bind(gpu_host_proxy.InitWithNewPipeAndPassReceiver());
  context_provider_ = WebNNContextProviderImpl::Create(
      /*shared_context_state=*/nullptr, std::move(gpu_feature_info),
      std::move(gpu_info),
      /*shared_image_manager=*/nullptr, /*peak_memory_monitor=*/nullptr,
      std::move(lose_all_contexts_callback),
      task_environment_->GetMainThreadTaskRunner(), g_webnn_scheduler.get(),
      mojo::SharedRemote(std::move(gpu_host_proxy)));
}

void WebNNTestEnvironment::BindWebNNContextProvider(
    mojo::PendingReceiver<mojom::WebNNContextProvider> pending_receiver,
    bool is_incognito) {
  // All tests use the same client ID since no other client exists.
  constexpr int32_t kFakeClientIdForTesting = 0;
  // All tests use the same tracing ID since no other client exists.
  constexpr uint64_t kFakeClientTracingIdForTesting = 0;
  context_provider_->BindWebNNContextProvider(
      std::move(pending_receiver),
      {is_incognito, kFakeClientIdForTesting, kFakeClientTracingIdForTesting});
}

WebNNTestEnvironment::~WebNNTestEnvironment() = default;

}  // namespace webnn::test
