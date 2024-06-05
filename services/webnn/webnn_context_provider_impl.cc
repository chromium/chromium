// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_provider_impl.h"

#include <memory>
#include <utility>

#include "base/check_is_test.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/buildflags.h"
#include "services/webnn/error.h"
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
#if BUILDFLAG(IS_CHROMEOS)
#include "services/webnn/tflite/context_impl_cros.h"
#else
#include "services/webnn/tflite/context_impl_tflite.h"
#endif
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
constexpr DML_FEATURE_LEVEL kMinDMLFeatureLevelForWebNN = DML_FEATURE_LEVEL_4_0;

base::expected<scoped_refptr<dml::Adapter>, mojom::ErrorPtr> GetDmlGpuAdapter(
    gpu::SharedContextState* shared_context_state) {
  if (!shared_context_state) {
    // Unit tests do not pass in a SharedContextState, since a reference to
    // a GpuServiceImpl must be initialized to obtain a SharedContextState.
    // Instead, we just enumerate the first DXGI adapter.
    CHECK_IS_TEST();
    return dml::Adapter::GetInstanceForTesting(kMinDMLFeatureLevelForWebNN);
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
  return dml::Adapter::GetGpuInstance(kMinDMLFeatureLevelForWebNN,
                                      std::move(dxgi_adapter));
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
#if !BUILDFLAG(IS_CHROMEOS)
    scoped_refptr<gpu::SharedContextState> shared_context_state,
    gpu::GpuFeatureInfo gpu_feature_info
#endif
    )
#if !BUILDFLAG(IS_CHROMEOS)
    : shared_context_state_(std::move(shared_context_state)),
      gpu_feature_info_(std::move(gpu_feature_info))
#endif
{
}

WebNNContextProviderImpl::~WebNNContextProviderImpl() = default;

// static
void WebNNContextProviderImpl::Create(
    mojo::PendingReceiver<WebNNContextProvider> receiver
#if !BUILDFLAG(IS_CHROMEOS)
    ,
    scoped_refptr<gpu::SharedContextState> shared_context_state,
    gpu::GpuFeatureInfo gpu_feature_info
#endif
) {
#if BUILDFLAG(IS_CHROMEOS)
  mojo::MakeSelfOwnedReceiver<WebNNContextProvider>(
      std::make_unique<WebNNContextProviderImpl>(), std::move(receiver));
#else
  CHECK_NE(shared_context_state, nullptr);
  mojo::MakeSelfOwnedReceiver<WebNNContextProvider>(
      std::make_unique<WebNNContextProviderImpl>(
          std::move(shared_context_state), std::move(gpu_feature_info)),
      std::move(receiver));
#endif
}

// static
void WebNNContextProviderImpl::CreateForTesting(
    mojo::PendingReceiver<mojom::WebNNContextProvider> receiver,
    bool is_gpu_supported) {
  CHECK_IS_TEST();

  gpu::GpuFeatureInfo gpu_feature_info;
  for (auto& status : gpu_feature_info.status_values) {
    status = gpu::GpuFeatureStatus::kGpuFeatureStatusDisabled;
  }
  gpu_feature_info.status_values[gpu::GPU_FEATURE_TYPE_WEBNN] =
      is_gpu_supported ? gpu::kGpuFeatureStatusEnabled
                       : gpu::kGpuFeatureStatusBlocklisted;

  mojo::MakeSelfOwnedReceiver<WebNNContextProvider>(
      std::make_unique<WebNNContextProviderImpl>(
#if !BUILDFLAG(IS_CHROMEOS)
          /*shared_context_state=*/nullptr, std::move(gpu_feature_info)
#endif
              ),
      std::move(receiver));
}

void WebNNContextProviderImpl::OnConnectionError(WebNNContextImpl* impl) {
  auto it =
      base::ranges::find(impls_, impl, &std::unique_ptr<WebNNContextImpl>::get);
  CHECK(it != impls_.end());
  impls_.erase(it);
}

// static
void WebNNContextProviderImpl::SetBackendForTesting(
    BackendForTesting* backend_for_testing) {
  g_backend_for_testing = backend_for_testing;
}

void WebNNContextProviderImpl::CreateWebNNContext(
    CreateContextOptionsPtr options,
    WebNNContextProvider::CreateWebNNContextCallback callback) {
  if (g_backend_for_testing) {
    g_backend_for_testing->CreateWebNNContext(impls_, this, std::move(options),
                                              std::move(callback));
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
          "WebNN is not compatible with GPU."));
      LOG(ERROR) << "[WebNN] is not compatible with GPU.";
      return;
    }

    // Get the `Adapter` instance which is created for the adapter according to
    // the device type. At the current stage, all `ContextImpl` share one
    // instance for one device type.
    base::expected<scoped_refptr<dml::Adapter>, mojom::ErrorPtr>
        adapter_creation_result;
    switch (options->device) {
      case mojom::CreateContextOptions::Device::kCpu:
        NOTREACHED_NORETURN();
      case mojom::CreateContextOptions::Device::kGpu:
        adapter_creation_result = GetDmlGpuAdapter(shared_context_state_.get());
        break;
      case mojom::CreateContextOptions::Device::kNpu:
        adapter_creation_result =
            dml::Adapter::GetNpuInstance(kMinDMLFeatureLevelForWebNN);
        break;
    }
    if (!adapter_creation_result.has_value()) {
      std::move(callback).Run(mojom::CreateContextResult::NewError(
          std::move(adapter_creation_result.error())));
      return;
    }

    scoped_refptr<dml::Adapter> adapter = adapter_creation_result.value();
    std::unique_ptr<dml::CommandRecorder> command_recorder =
        dml::CommandRecorder::Create(adapter->command_queue(),
                                     adapter->dml_device());
    if (!command_recorder) {
      std::move(callback).Run(mojom::CreateContextResult::NewError(
          dml::CreateError(mojom::Error::Code::kUnknownError,
                           "Failed to create a WebNN context.")));
      return;
    }

    context_impl =
        new dml::ContextImplDml(std::move(adapter), std::move(receiver), this,
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
#if BUILDFLAG(IS_CHROMEOS)
    // TODO: crbug.com/41486052 - Create the TFLite context using `options`.
    context_impl = new tflite::ContextImplCrOS(std::move(receiver), this);
#else
    context_impl = new tflite::ContextImplTflite(std::move(receiver), this,
                                                 std::move(options));
#endif  // BUILDFLAG(IS_CHROMEOS)
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

  mojom::ContextPropertiesPtr properties = context_impl->properties().Clone();
  impls_.push_back(base::WrapUnique<WebNNContextImpl>(context_impl));

  auto success = mojom::CreateContextSuccess::New(std::move(remote),
                                                  std::move(properties));
  std::move(callback).Run(
      mojom::CreateContextResult::NewSuccess(std::move(success)));
}

}  // namespace webnn
