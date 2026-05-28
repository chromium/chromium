// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_TEST_ENVIRONMENT_H_
#define SERVICES_WEBNN_WEBNN_TEST_ENVIRONMENT_H_

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "services/webnn/webnn_context_provider_impl.h"

#if BUILDFLAG(IS_WIN)
#include "services/webnn/host/execution_provider_initializer.h"
#include "ui/gfx/mojom/dxgi_info.mojom.h"
#endif

namespace webnn::test {

// A minimal fake GpuHost implementation for testing.
class FakeGpuHostForTesting : public viz::mojom::GpuHost {
 public:
  FakeGpuHostForTesting();
  ~FakeGpuHostForTesting() override;

  void Bind(mojo::PendingReceiver<viz::mojom::GpuHost> receiver);

  void DidInitialize(
      const gpu::GPUInfo& gpu_info,
      const gpu::GpuFeatureInfo& gpu_feature_info,
      const std::optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu,
      const std::optional<gpu::GpuFeatureInfo>&
          gpu_feature_info_for_hardware_gpu,
      const gfx::GpuExtraInfo& gpu_extra_info) override;
  void DidFailInitialize() override;
  void DidCreateContextSuccessfully() override;
  void DidCreateOffscreenContext(const GURL& url) override;
  void DidDestroyOffscreenContext(const GURL& url) override;
  void DidDestroyChannel(int32_t client_id) override;
  void DidDestroyAllChannels() override;
  void DidLoseContext(gpu::error::ContextLostReason reason,
                      const GURL& active_url) override;
  void DisableGpuCompositing() override;
  void DidUpdateGPUInfo(const gpu::GPUInfo& gpu_info) override;
  void GetIsolationKey(int32_t client_id,
                       const blink::WebGPUExecutionContextToken& token,
                       GetIsolationKeyCallback cb) override;
  void StoreBlobToDisk(const gpu::GpuDiskCacheHandle& handle,
                       const std::string& key,
                       const std::string& shader) override;
  void ClearGrShaderDiskCache() override;
#if BUILDFLAG(IS_WIN)
  void DidUpdateOverlayInfo(const gpu::OverlayInfo& overlay_info) override;
  void DidUpdateDXGIInfo(gfx::mojom::DXGIInfoPtr dxgi_info) override;
  void EnsureWebNNExecutionProvidersReady(
      EnsureWebNNExecutionProvidersReadyCallback callback) override;
  void RequestWebNNCompilerContext(
      webnn::mojom::CreateContextOptionsPtr context_options,
      const webnn::ContextProperties& context_properties,
      base::flat_map<std::string, webnn::mojom::EpPackageInfoPtr>
          ep_package_info,
      RequestWebNNCompilerContextCallback callback) override;
#endif
  void CreateWebNNWeightsFile(CreateWebNNWeightsFileCallback callback) override;

 private:
  mojo::Receiver<viz::mojom::GpuHost> receiver_;
};

class WebNNTestEnvironment {
 public:
  explicit WebNNTestEnvironment(
      WebNNContextProviderImpl::WebNNStatus status =
          WebNNContextProviderImpl::WebNNStatus::kWebNNEnabled,
      WebNNContextProviderImpl::LoseAllContextsCallback
          lose_all_contexts_callback = base::DoNothing(),
      std::unique_ptr<base::test::TaskEnvironment> task_environment =
          std::make_unique<base::test::TaskEnvironment>());

  ~WebNNTestEnvironment();

  void RunUntilIdle() { task_environment_->RunUntilIdle(); }

  // Waits until all WebNNContextImpl instances have been fully destroyed
  // (destructor has run). Call after resetting context remotes to ensure
  // service-side disconnect handlers, removal, and destruction all complete
  // before closing additional pipes.
  void WaitForAllContextsToBeDestroyed();

  void BindWebNNContextProvider(
      mojo::PendingReceiver<mojom::WebNNContextProvider> pending_receiver,
      bool is_incognito = false);

  // Flushes thread pool tasks and waits for all receivers bound via
  // BindWebNNContextProvider to disconnect. Call in test TearDown after
  // resetting remotes to ensure deterministic cleanup.
  void TearDown();

 private:
  void OnReceiverDisconnected();
  void OnContextDestroyed();

  FakeGpuHostForTesting fake_gpu_host_;
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  std::unique_ptr<WebNNContextProviderImpl> context_provider_;
  size_t pending_receiver_count_ = 0;
  size_t destroyed_context_count_ = 0;
  base::RepeatingClosure destruction_callback_;
};

}  // namespace webnn::test

#endif  // SERVICES_WEBNN_WEBNN_TEST_ENVIRONMENT_H_
