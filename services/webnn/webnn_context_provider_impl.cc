// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_provider_impl.h"

#include <memory>
#include <utility>

#include "base/check_is_test.h"
#include "base/types/expected_macros.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/buildflags.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-forward.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/webnn_context_impl.h"

#if BUILDFLAG(IS_WIN)
#include <wrl.h>

#include "base/notreached.h"
#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/command_recorder.h"
#include "services/webnn/dml/context_impl_dml.h"
#include "services/webnn/dml/utils.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "services/webnn/coreml/context_impl_coreml.h"
#endif

#if BUILDFLAG(WEBNN_USE_TFLITE)
#include "services/webnn/tflite/context_impl_tflite.h"
#endif

namespace webnn {

#if BUILDFLAG(IS_WIN)
using Microsoft::WRL::ComPtr;
#endif

namespace {

WebNNContextProviderImpl::BackendForTesting* g_backend_for_testing = nullptr;

using webnn::mojom::CreateContextOptionsPtr;
using webnn::mojom::WebNNContextProvider;

#if BUILDFLAG(IS_WIN)
base::expected<scoped_refptr<dml::Adapter>, mojom::ErrorPtr> GetDmlGpuAdapter(
    gpu::SharedContextState* shared_context_state,
    const gpu::GpuFeatureInfo& gpu_feature_info) {
  if (gpu_feature_info.IsWorkaroundEnabled(DISABLE_WEBNN_FOR_GPU)) {
    return base::unexpected(
        dml::CreateError(mojom::Error::Code::kNotSupportedError,
                         "WebNN is blocklisted for GPU."));
  }

  if (!shared_context_state) {
    // Unit tests do not pass in a SharedContextState, since a reference to
    // a GpuServiceImpl must be initialized to obtain a SharedContextState.
    // Instead, we just enumerate the first DXGI adapter.
    CHECK_IS_TEST();
    return dml::Adapter::GetGpuInstanceForTesting();
  }

  // At the current stage, all `ContextImplDml` share this instance.
  //
  // TODO(crbug.com/40277628): Support getting `Adapter` instance based on
  // `options`.
  ComPtr<ID3D11Device> d3d11_device = shared_context_state->GetD3D11Device();
  if (!d3d11_device) {
    return base::unexpected(dml::CreateError(
        mojom::Error::Code::kNotSupportedError,
        "Failed to get D3D11 Device from SharedContextState."));
  }

  ComPtr<IDXGIDevice> dxgi_device;
  // A QueryInterface() via As() from a ID3D11Device to IDXGIDevice should
  // always succeed.
  CHECK_EQ(d3d11_device.As(&dxgi_device), S_OK);
  ComPtr<IDXGIAdapter> dxgi_adapter;
  // Asking for an adapter from IDXGIDevice is always expected to succeed.
  CHECK_EQ(dxgi_device->GetAdapter(&dxgi_adapter), S_OK);
  return dml::Adapter::GetGpuInstance(std::move(dxgi_adapter));
}
#endif

#if BUILDFLAG(IS_WIN)
bool ShouldCreateDmlContext(const mojom::CreateContextOptions& options) {
  switch (options.device) {
    case mojom::CreateContextOptions::Device::kCpu:
      return false;
    case mojom::CreateContextOptions::Device::kGpu:
    case mojom::CreateContextOptions::Device::kNpu:
      return true;
  }
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

WebNNContextProviderImpl::WebNNContextProviderImpl(
    scoped_refptr<gpu::SharedContextState> shared_context_state,
    gpu::GpuFeatureInfo gpu_feature_info,
    gpu::GPUInfo gpu_info,
    LoseAllContextsCallback lose_all_contexts_callback)
    : shared_context_state_(std::move(shared_context_state)),
      gpu_feature_info_(std::move(gpu_feature_info)),
      gpu_info_(std::move(gpu_info)),
      lose_all_contexts_callback_(std::move(lose_all_contexts_callback)) {}

WebNNContextProviderImpl::~WebNNContextProviderImpl() = default;

std::unique_ptr<WebNNContextProviderImpl> WebNNContextProviderImpl::Create(
    scoped_refptr<gpu::SharedContextState> shared_context_state,
    gpu::GpuFeatureInfo gpu_feature_info,
    gpu::GPUInfo gpu_info,
    LoseAllContextsCallback lose_all_contexts_callback) {
  CHECK_NE(shared_context_state, nullptr);
  return base::WrapUnique(new WebNNContextProviderImpl(
      std::move(shared_context_state), std::move(gpu_feature_info),
      std::move(gpu_info), std::move(lose_all_contexts_callback)));
}

void WebNNContextProviderImpl::BindWebNNContextProvider(
    mojo::PendingReceiver<mojom::WebNNContextProvider> receiver) {
  provider_receivers_.Add(this, std::move(receiver));
}

// static
void WebNNContextProviderImpl::CreateForTesting(
    mojo::PendingReceiver<mojom::WebNNContextProvider> receiver,
    WebNNStatus status) {
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

  LoseAllContextsCallback lose_all_contexts_callback = base::BindOnce([]() {});
  mojo::MakeSelfOwnedReceiver<WebNNContextProvider>(
      base::WrapUnique(new WebNNContextProviderImpl(
          /*shared_context_state=*/nullptr, std::move(gpu_feature_info),
          std::move(gpu_info), std::move(lose_all_contexts_callback))),
      std::move(receiver));
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

  WebNNContextImpl* context_impl = nullptr;
  mojo::PendingRemote<mojom::WebNNContext> remote;
  auto receiver = remote.InitWithNewPipeAndPassReceiver();

#if BUILDFLAG(IS_WIN)
  if (ShouldCreateDmlContext(*options)) {
    DCHECK(gpu_feature_info_.IsInitialized());
    if (gpu_feature_info_.status_values[gpu::GPU_FEATURE_TYPE_WEBNN] !=
        gpu::kGpuFeatureStatusEnabled) {
      std::move(callback).Run(ToError<mojom::CreateContextResult>(
          mojom::Error::Code::kNotSupportedError,
          "WebNN is not compatible with device."));
      LOG(ERROR) << "[WebNN] is not compatible with device.";
      return;
    }
    // Get the `Adapter` instance which is created for the adapter according to
    // the device type. At the current stage, all `ContextImpl` share one
    // instance for one device type.
    base::expected<scoped_refptr<dml::Adapter>, mojom::ErrorPtr>
        adapter_creation_result;
    switch (options->device) {
      case mojom::CreateContextOptions::Device::kCpu:
        NOTREACHED();
      case mojom::CreateContextOptions::Device::kGpu:
        adapter_creation_result =
            GetDmlGpuAdapter(shared_context_state_.get(), gpu_feature_info_);
        break;
      case mojom::CreateContextOptions::Device::kNpu:
        adapter_creation_result =
            dml::Adapter::GetNpuInstance(gpu_feature_info_, gpu_info_);
        break;
    }
    if (!adapter_creation_result.has_value()) {
      std::move(callback).Run(mojom::CreateContextResult::NewError(
          std::move(adapter_creation_result.error())));
      return;
    }

    scoped_refptr<dml::Adapter> adapter = adapter_creation_result.value();

    ASSIGN_OR_RETURN(
        auto command_recorder,
        dml::CommandRecorder::Create(adapter->command_queue(),
                                     adapter->dml_device()),
        [](WebNNContextProvider::CreateWebNNContextCallback callback,
           HRESULT hr) {
          std::move(callback).Run(mojom::CreateContextResult::NewError(
              dml::CreateError(mojom::Error::Code::kUnknownError,
                               "Failed to create a WebNN context.")));
        },
        std::move(callback));

    context_impl = new dml::ContextImplDml(
        std::move(adapter), std::move(receiver), this, std::move(options),
        std::move(command_recorder), gpu_feature_info_);
  }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
  // TODO: crbug.com/325612086 - Consider using supporting older Macs either
  // with TFLite or a more restrictive implementation on CoreML.
  if (__builtin_available(macOS 14, *)) {
    context_impl = new coreml::ContextImplCoreml(std::move(receiver), this,
                                                 std::move(options));
  }
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(WEBNN_USE_TFLITE)
  if (!context_impl) {
    context_impl = new tflite::ContextImplTflite(std::move(receiver), this,
                                                 std::move(options));
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
  impls_.emplace(base::WrapUnique<WebNNContextImpl>(context_impl));

  auto success = mojom::CreateContextSuccess::New(std::move(remote),
                                                  std::move(context_properties),
                                                  std::move(context_handle));
  std::move(callback).Run(
      mojom::CreateContextResult::NewSuccess(std::move(success)));
}

}  // namespace webnn
