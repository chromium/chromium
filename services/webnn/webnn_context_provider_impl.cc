// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_provider_impl.h"

#include <memory>
#include <utility>

#include "base/check_is_test.h"
#include "base/metrics/histogram_functions.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/buildflags.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/mojom/features.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/webnn_context_impl.h"

#if BUILDFLAG(IS_WIN)
#include "base/types/expected_macros.h"
#include "services/webnn/dml/context_provider_dml.h"
#include "services/webnn/ort/context_provider_ort.h"
#include "services/webnn/ort/ort_session_options.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "services/webnn/coreml/context_impl_coreml.h"
#endif

#if BUILDFLAG(WEBNN_USE_TFLITE)
#include "services/webnn/tflite/context_impl_tflite.h"
#endif

namespace webnn {

namespace {

WebNNContextProviderImpl::BackendForTesting* g_backend_for_testing = nullptr;

using webnn::mojom::CreateContextOptionsPtr;
using webnn::mojom::WebNNContextProvider;

// These values are persisted to logs. Entries should not be renumbered or
// removed and numeric values should never be reused.
// Please keep in sync with DeviceTypeUma in
// //tools/metrics/histograms/metadata/webnn/enums.xml.
enum class DeviceTypeUma {
  kCpu = 0,
  kGpu = 1,
  kNpu = 2,
  kMaxValue = kNpu,
};

void RecordDeviceType(const mojom::Device device) {
  DeviceTypeUma uma_value;
  switch (device) {
    case mojom::Device::kCpu:
      uma_value = DeviceTypeUma::kCpu;
      break;
    case mojom::Device::kGpu:
      uma_value = DeviceTypeUma::kGpu;
      break;
    case mojom::Device::kNpu:
      uma_value = DeviceTypeUma::kNpu;
      break;
  }
  base::UmaHistogramEnumeration("WebNN.DeviceType", uma_value);
}

}  // namespace

WebNNContextProviderImpl::WebNNContextProviderImpl(
    scoped_refptr<gpu::SharedContextState> shared_context_state,
    gpu::GpuFeatureInfo gpu_feature_info,
    gpu::GPUInfo gpu_info,
    LoseAllContextsCallback lose_all_contexts_callback,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    gpu::Scheduler* scheduler,
    int32_t client_id)
    : shared_context_state_(std::move(shared_context_state)),
      gpu_feature_info_(std::move(gpu_feature_info)),
      gpu_info_(std::move(gpu_info)),
      lose_all_contexts_callback_(std::move(lose_all_contexts_callback)),
      scheduler_(scheduler),
      main_thread_task_runner_(std::move(main_thread_task_runner)),
      client_id_(client_id) {
  CHECK_NE(scheduler_, nullptr);
  CHECK_NE(main_thread_task_runner_, nullptr);
}

WebNNContextProviderImpl::~WebNNContextProviderImpl() = default;

std::unique_ptr<WebNNContextProviderImpl> WebNNContextProviderImpl::Create(
    scoped_refptr<gpu::SharedContextState> shared_context_state,
    gpu::GpuFeatureInfo gpu_feature_info,
    gpu::GPUInfo gpu_info,
    LoseAllContextsCallback lose_all_contexts_callback,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    gpu::Scheduler* scheduler,
    int32_t client_id) {
  CHECK_NE(shared_context_state, nullptr);
  return base::WrapUnique(new WebNNContextProviderImpl(
      std::move(shared_context_state), std::move(gpu_feature_info),
      std::move(gpu_info), std::move(lose_all_contexts_callback),
      std::move(main_thread_task_runner), scheduler, client_id));
}

void WebNNContextProviderImpl::BindWebNNContextProvider(
    mojo::PendingReceiver<mojom::WebNNContextProvider> receiver) {
  provider_receivers_.Add(this, std::move(receiver));
}

// static
base::optional_ref<WebNNContextProviderImpl>
WebNNContextProviderImpl::CreateForTesting(
    mojo::PendingReceiver<mojom::WebNNContextProvider> receiver,
    WebNNStatus status,
    LoseAllContextsCallback lose_all_contexts_callback) {
  CHECK_IS_TEST();

  gpu::GpuFeatureInfo gpu_feature_info;
  gpu::GPUInfo gpu_info;

  for (auto& status_value : gpu_feature_info.status_values) {
    status_value = gpu::GpuFeatureStatus::kGpuFeatureStatusDisabled;
  }
  if (status != WebNNStatus::kWebNNGpuFeatureStatusDisabled) {
    gpu_feature_info.status_values[gpu::GPU_FEATURE_TYPE_WEBNN] =
        gpu::kGpuFeatureStatusEnabled;
  }
  if (status == WebNNStatus::kWebNNGpuDisabled) {
    gpu_feature_info.enabled_gpu_driver_bug_workarounds.push_back(
        DISABLE_WEBNN_FOR_GPU);
  }
  if (status == WebNNStatus::kWebNNNpuDisabled) {
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

  // Cast is safe because only a WebNNContextProviderImpl can be created.
  return static_cast<WebNNContextProviderImpl*>(
      mojo::MakeSelfOwnedReceiver<mojom::WebNNContextProvider>(
          base::WrapUnique(new WebNNContextProviderImpl(
              /*shared_context_state=*/nullptr, std::move(gpu_feature_info),
              std::move(gpu_info), std::move(lose_all_contexts_callback),
              base::SingleThreadTaskRunner::GetCurrentDefault(),
              g_webnn_scheduler.get(), kFakeClientIdForTesting)),
          std::move(receiver))
          ->impl());
}

void WebNNContextProviderImpl::OnConnectionError(WebNNContextImpl* impl) {
  auto it = impls_.find(impl->handle());
  CHECK(it != impls_.end());
  impls_.erase(it);
}

#if BUILDFLAG(IS_WIN)
void WebNNContextProviderImpl::DestroyContextsAndKillGpuProcess(
    std::string_view reason) {
  // Send the contexts lost reason to the renderer process.
  for (const auto& impl : impls_) {
    impl->ResetReceiverWithReason(reason);
  }

  std::move(lose_all_contexts_callback_).Run();
}
#endif  // BUILDFLAG(IS_WIN)

// static
void WebNNContextProviderImpl::SetBackendForTesting(
    BackendForTesting* backend_for_testing) {
  g_backend_for_testing = backend_for_testing;
}

void WebNNContextProviderImpl::CreateWebNNContext(
    CreateContextOptionsPtr options,
    WebNNContextProvider::CreateWebNNContextCallback callback) {
  if (g_backend_for_testing) {
    impls_.emplace(g_backend_for_testing->CreateWebNNContext(
        this, std::move(options), std::move(callback)));
    return;
  }

  std::unique_ptr<WebNNContextImpl> context_impl;
  mojo::PendingRemote<mojom::WebNNContext> remote;
  auto receiver = remote.InitWithNewPipeAndPassReceiver();

  RecordDeviceType(options->device);

#if BUILDFLAG(IS_WIN)
  base::expected<std::unique_ptr<WebNNContextImpl>, mojom::ErrorPtr>
      context_creation_results;

  if (base::FeatureList::IsEnabled(mojom::features::kWebNNOnnxRuntime)) {
    context_creation_results = ort::CreateContextFromOptions(
        std::move(options), std::move(receiver), this);
    if (!context_creation_results.has_value()) {
      std::move(callback).Run(mojom::CreateContextResult::NewError(
          std::move(context_creation_results.error())));
      return;
    }
    context_impl = std::move(context_creation_results.value());
  }

  if (!context_impl && dml::ShouldCreateDmlContext(*options)) {
    context_creation_results = dml::CreateContextFromOptions(
        std::move(options), gpu_feature_info_, gpu_info_,
        shared_context_state_.get(), std::move(receiver), this);
    if (!context_creation_results.has_value()) {
      std::move(callback).Run(mojom::CreateContextResult::NewError(
          std::move(context_creation_results.error())));
      return;
    }
    context_impl = std::move(context_creation_results.value());
  }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_APPLE)
  if (__builtin_available(macOS 14.4, *)) {
    if (base::FeatureList::IsEnabled(mojom::features::kWebNNCoreML)
#if BUILDFLAG(IS_MAC)
        && base::mac::GetCPUType() == base::mac::CPUType::kArm
#endif  // BUILDFLAG(IS_MAC)
    ) {
      context_impl = std::make_unique<coreml::ContextImplCoreml>(
          std::move(receiver), this, std::move(options));
    }
  }
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(WEBNN_USE_TFLITE)
  if (!context_impl) {
    context_impl = std::make_unique<tflite::ContextImplTflite>(
        std::move(receiver), this, std::move(options));
  }
#endif  // BUILDFLAG(WEBNN_USE_TFLITE)

  if (!context_impl) {
    // TODO(crbug.com/40206287): Supporting WebNN Service on the platform.
    std::move(callback).Run(ToError<mojom::CreateContextResult>(
        mojom::Error::Code::kNotSupportedError,
        "WebNN Service is not supported on this platform."));
    LOG(ERROR) << "[WebNN] Service is not supported on this platform.";
    return;
  }

  ContextProperties context_properties = context_impl->properties();
  const blink::WebNNContextToken& context_handle = context_impl->handle();
  impls_.emplace(std::move(context_impl));

  auto success = mojom::CreateContextSuccess::New(std::move(remote),
                                                  std::move(context_properties),
                                                  std::move(context_handle));
  std::move(callback).Run(
      mojom::CreateContextResult::NewSuccess(std::move(success)));
}

base::optional_ref<WebNNContextImpl>
WebNNContextProviderImpl::GetWebNNContextImplForTesting(
    const blink::WebNNContextToken& handle) {
  CHECK_IS_TEST();
  const auto it = impls_.find(handle);
  if (it == impls_.end()) {
    mojo::ReportBadMessage(kBadMessageInvalidContext);
    return std::nullopt;
  }
  return it->get();
}

}  // namespace webnn
