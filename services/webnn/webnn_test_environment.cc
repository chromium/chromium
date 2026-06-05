// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_test_environment.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/run_until.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "services/viz/privileged/mojom/gl/gpu_host.mojom.h"
#include "services/webnn/buildflags.h"
#include "services/webnn/host/weights_file_provider.h"
#include "services/webnn/webnn_context_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)
#include "services/webnn/host/weights_file_creator_impl.h"
#include "services/webnn/public/cpp/in_process_context_provider.h"  // nogncheck
#endif

namespace webnn::test {

namespace {

// Mimics the renderer-process behavior of falling back to an in-process
// backend (TFLite, LiteRT, or ORT) when the GPU-process
// `WebNNContextProvider` rejects a request.
class InProcessFallbackContextProvider : public mojom::WebNNContextProvider {
 public:
  InProcessFallbackContextProvider(
      mojo::PendingRemote<mojom::WebNNContextProvider> gpu_process_remote,
      bool is_incognito,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : gpu_process_remote_(std::move(gpu_process_remote)),
        is_incognito_(is_incognito),
        task_runner_(std::move(task_runner)) {}

  ~InProcessFallbackContextProvider() override = default;

  InProcessFallbackContextProvider(const InProcessFallbackContextProvider&) =
      delete;
  InProcessFallbackContextProvider& operator=(
      const InProcessFallbackContextProvider&) = delete;

  // mojom::WebNNContextProvider:
  void CreateWebNNContext(mojom::CreateContextOptionsPtr options,
                          CreateWebNNContextCallback callback) override {
    // Try the GPU-process provider first; on error,
    // `OnGpuProcessCreateContextResult` may fall back to the in-process
    // provider with the same options.
    auto options_clone = options.Clone();
    gpu_process_remote_->CreateWebNNContext(
        std::move(options),
        base::BindOnce(
            &InProcessFallbackContextProvider::OnGpuProcessCreateContextResult,
            base::Unretained(this), std::move(options_clone),
            std::move(callback)));
  }

 private:
  void OnGpuProcessCreateContextResult(mojom::CreateContextOptionsPtr options,
                                       CreateWebNNContextCallback callback,
                                       mojom::CreateContextResultPtr result) {
#if BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)
    // Fall back to the in-process provider on GPU-process failure, mirroring
    // `ML::CreateInProcessContext` in the renderer.
    if (result->is_error() &&
        !WebNNContextProviderImpl::HasBackendForTesting()) {
      EnsureInProcessConnection();
      in_process_remote_->CreateWebNNContext(std::move(options),
                                             std::move(callback));
      return;
    }
#endif  // BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)
    std::move(callback).Run(std::move(result));
  }

#if BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)
  // Lazily binds the in-process provider remote on first use, mirroring
  // `ML::EnsureInProcessConnection` in the renderer.
  void EnsureInProcessConnection() {
    if (in_process_remote_.is_bound()) {
      return;
    }
    mojo::PendingRemote<mojom::WebNNWeightsFileCreator> weights_file_creator;
    WeightsFileCreatorImpl::Create(
        weights_file_creator.InitWithNewPipeAndPassReceiver(), is_incognito_);
    mojo::ScopedMessagePipeHandle in_process_pipe =
        webnn::CreateInProcessContextProvider(weights_file_creator.PassPipe(),
                                              task_runner_);
    in_process_remote_.Bind(mojo::PendingRemote<mojom::WebNNContextProvider>(
        std::move(in_process_pipe), 0u));
  }
#endif  // BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)

  mojo::Remote<mojom::WebNNContextProvider> gpu_process_remote_;
  mojo::Remote<mojom::WebNNContextProvider> in_process_remote_;
  [[maybe_unused]] const bool is_incognito_;
  [[maybe_unused]] const scoped_refptr<base::SingleThreadTaskRunner>
      task_runner_;
};

}  // namespace

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

void FakeGpuHostForTesting::RequestWebNNCompilerContext(
    webnn::mojom::CreateContextOptionsPtr context_options,
    const webnn::ContextProperties& context_properties,
    base::flat_map<std::string, webnn::mojom::EpPackageInfoPtr> ep_package_info,
    RequestWebNNCompilerContextCallback callback) {
  std::move(callback).Run(mojo::NullRemote(), mojo::NullReceiver());
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
    : task_environment_(std::move(task_environment)),
      destruction_callback_(
          base::BindRepeating(&WebNNTestEnvironment::OnContextDestroyed,
                              base::Unretained(this))) {
  WebNNContextImpl::SetDestructionCallbackForTesting(&destruction_callback_);
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
      std::move(gpu_feature_info), std::move(gpu_info),
      /*shared_image_manager=*/nullptr, /*peak_memory_monitor=*/nullptr,
      std::move(lose_all_contexts_callback),
      task_environment_->GetMainThreadTaskRunner(), g_webnn_scheduler.get(),
      mojo::SharedRemote(std::move(gpu_host_proxy)));
  context_provider_->SetDisconnectHandlerForTesting(base::BindRepeating(
      &WebNNTestEnvironment::OnReceiverDisconnected, base::Unretained(this)));
}

void WebNNTestEnvironment::BindWebNNContextProvider(
    mojo::PendingReceiver<mojom::WebNNContextProvider> pending_receiver,
    bool is_incognito) {
  // All tests use the same client ID since no other client exists.
  constexpr int32_t kFakeClientIdForTesting = 0;
  // All tests use the same tracing ID since no other client exists.
  constexpr uint64_t kFakeClientTracingIdForTesting = 0;

  mojo::PendingRemote<mojom::WebNNContextProvider> gpu_process_remote;
  context_provider_->BindWebNNContextProvider(
      gpu_process_remote.InitWithNewPipeAndPassReceiver(),
      {is_incognito, kFakeClientIdForTesting, kFakeClientTracingIdForTesting});

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<InProcessFallbackContextProvider>(
          std::move(gpu_process_remote), is_incognito,
          task_environment_->GetMainThreadTaskRunner()),
      std::move(pending_receiver));
  pending_receiver_count_++;
}

void WebNNTestEnvironment::OnReceiverDisconnected() {
  CHECK_GT(pending_receiver_count_, 0u);
  pending_receiver_count_--;
}

void WebNNTestEnvironment::OnContextDestroyed() {
  destroyed_context_count_++;
}

void WebNNTestEnvironment::WaitForAllContextsToBeDestroyed() {
  // Mojo pipe closure notifications are delivered by the IO thread (managed
  // by ScopedMojoIoForTest inside TaskEnvironment). Flushing the thread pool
  // ensures those notifications have been forwarded to the GPU scheduler,
  // which then posts disconnect tasks to the main thread. Without this flush,
  // RunUntil would never see the disconnect tasks arrive.
  base::ThreadPoolInstance::Get()->FlushForTesting();

  // Wait for all contexts to be fully destroyed (not just removed from the
  // provider's map). After RemoveWebNNContextImpl erases a context,
  // OnTaskRunnerDeleter posts the actual deletion to the SchedulerTaskRunner.
  // The destructor explicitly resets gpu_task_scheduler_ (releasing
  // MultiplexRouter refs) then fires our callback, incrementing
  // destroyed_context_count_.
  const size_t target_destroyed =
      destroyed_context_count_ + context_provider_->GetContextCountForTesting();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return destroyed_context_count_ >= target_destroyed; }));
}

void WebNNTestEnvironment::TearDown() {
  // Flush the thread pool to drain MultiplexRouter bounced tasks that hold
  // scoped_refptr<MultiplexRouter> via WrapRefCounted. In production these
  // drain naturally, but in tests the thread pool may shut down first.
  base::ThreadPoolInstance::Get()->FlushForTesting();

  EXPECT_TRUE(
      base::test::RunUntil([&]() { return pending_receiver_count_ == 0; }));
}

WebNNTestEnvironment::~WebNNTestEnvironment() {
  WebNNContextImpl::SetDestructionCallbackForTesting(nullptr);
}

}  // namespace webnn::test
