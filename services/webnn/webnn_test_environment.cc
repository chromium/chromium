// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_test_environment.h"

#include "base/run_loop.h"

namespace webnn::test {

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

  // All tests use the same client ID since no other client exists.
  constexpr int32_t kFakeClientIdForTesting = 0;

  context_provider_ = WebNNContextProviderImpl::Create(
      /*shared_context_state=*/nullptr, std::move(gpu_feature_info),
      std::move(gpu_info), /*shared_image_manager=*/nullptr,
      std::move(lose_all_contexts_callback),
      task_environment_->GetMainThreadTaskRunner(), &scheduler_,
      kFakeClientIdForTesting);
}

void WebNNTestEnvironment::BindWebNNContextProvider(
    mojo::PendingReceiver<mojom::WebNNContextProvider> pending_receiver) {
  context_provider_->BindWebNNContextProvider(std::move(pending_receiver));
}

WebNNTestEnvironment::~WebNNTestEnvironment() {
  // Destroy all WebNNContextImpls on their owning sequences before destroying
  // the gpu::Scheduler, since the contexts may post tasks to the same
  // sequences.
  auto pending_runners =
      context_provider_->GetAllContextTaskRunnersForTesting();

  // Drop all references to the contexts so their destructors run.
  context_provider_.reset();

  // Drain each task runner to ensure all tasks posted by contexts have
  // completed.
  for (auto& runner : pending_runners) {
    base::RunLoop loop;
    runner->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();  // Blocks until all previously posted tasks complete
  }
}

}  // namespace webnn::test
