// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_test_environment.h"

#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "services/viz/privileged/mojom/gl/gpu_host.mojom.h"

namespace webnn::test {

WebNNTestEnvironment::WebNNTestEnvironment(
    WebNNContextProviderImpl::WebNNStatus status,
    WebNNContextProviderImpl::LoseAllContextsCallback
        lose_all_contexts_callback) {
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

  // All tests use the same client ID since no other client exists.
  constexpr int32_t kFakeClientIdForTesting = 0;

  mojo::PendingRemote<viz::mojom::GpuHost> gpu_host_proxy;
  std::ignore = gpu_host_proxy.InitWithNewPipeAndPassReceiver();
  context_provider_ = WebNNContextProviderImpl::Create(
      /*shared_context_state=*/nullptr, std::move(gpu_feature_info),
      std::move(gpu_info), /*shared_image_manager=*/nullptr,
      std::move(lose_all_contexts_callback),
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      g_webnn_scheduler.get(), kFakeClientIdForTesting,
      mojo::SharedRemote(std::move(gpu_host_proxy)));
}

void WebNNTestEnvironment::BindWebNNContextProvider(
    mojo::PendingReceiver<mojom::WebNNContextProvider> pending_receiver) {
  context_provider_->BindWebNNContextProvider(std::move(pending_receiver));
}

WebNNTestEnvironment::~WebNNTestEnvironment() = default;

}  // namespace webnn::test
