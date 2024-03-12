// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_provider_impl.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/error.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/webnn_context_impl.h"

#if BUILDFLAG(IS_WIN)
#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/command_queue.h"
#include "services/webnn/dml/command_recorder.h"
#include "services/webnn/dml/context_impl.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "services/webnn/coreml/context_impl.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "services/webnn/tflite/context_impl.h"
#endif

namespace webnn {

namespace {

WebNNContextProviderImpl::BackendForTesting* g_backend_for_testing = nullptr;

using webnn::mojom::CreateContextOptionsPtr;
using webnn::mojom::WebNNContextProvider;

}  // namespace

WebNNContextProviderImpl::WebNNContextProviderImpl(
    gpu::GpuFeatureInfo gpu_feature_info)
    : gpu_feature_info_(std::move(gpu_feature_info)) {}

WebNNContextProviderImpl::~WebNNContextProviderImpl() = default;

// static
void WebNNContextProviderImpl::Create(
    mojo::PendingReceiver<WebNNContextProvider> receiver,
    gpu::GpuFeatureInfo gpu_feature_info) {
  mojo::MakeSelfOwnedReceiver<WebNNContextProvider>(
      std::make_unique<WebNNContextProviderImpl>(std::move(gpu_feature_info)),
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
#if BUILDFLAG(IS_WIN)
  if (gpu_feature_info_.status_values[gpu::GPU_FEATURE_TYPE_WEBNN] !=
      gpu::kGpuFeatureStatusEnabled) {
    std::move(callback).Run(ToError<mojom::CreateContextResult>(
        mojom::Error::Code::kNotSupportedError,
        "WebNN is not compatible with GPU."));
    DLOG(ERROR) << "WebNN is not compatible with GPU.";
    return;
  }
  // Get the default `Adapter` instance which is created for the adapter queried
  // from ANGLE. At the current stage, all `ContextImpl` share this instance.
  //
  // TODO(crbug.com/1469755): Support getting `Adapter` instance based on
  // `options`.
  constexpr DML_FEATURE_LEVEL kMinDMLFeatureLevelForWebNN =
      DML_FEATURE_LEVEL_4_0;
  auto adapter_creation_result =
      dml::Adapter::GetInstance(kMinDMLFeatureLevelForWebNN);
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
    std::move(callback).Run(ToError<mojom::CreateContextResult>(
        mojom::Error::Code::kUnknownError,
        "Failed to create a WebNN context."));
    DLOG(ERROR) << "Failed to open the command recorder.";
    return;
  }

  // The remote sent to the renderer.
  mojo::PendingRemote<mojom::WebNNContext> blink_remote;
  // The receiver bound to WebNNContextImpl.
  impls_.push_back(base::WrapUnique<WebNNContextImpl>(new dml::ContextImpl(
      std::move(adapter), blink_remote.InitWithNewPipeAndPassReceiver(), this,
      std::move(command_recorder), gpu_feature_info_)));
  std::move(callback).Run(
      mojom::CreateContextResult::NewContextRemote(std::move(blink_remote)));
#elif BUILDFLAG(IS_MAC)
  if (__builtin_available(macOS 13, *)) {
    // The remote sent to the renderer.
    mojo::PendingRemote<mojom::WebNNContext> blink_remote;
    // The receiver bound to WebNNContextImpl.
    impls_.push_back(base::WrapUnique<WebNNContextImpl>(new coreml::ContextImpl(
        blink_remote.InitWithNewPipeAndPassReceiver(), this)));
    std::move(callback).Run(
        mojom::CreateContextResult::NewContextRemote(std::move(blink_remote)));
  } else {
    std::move(callback).Run(ToError<mojom::CreateContextResult>(
        mojom::Error::Code::kNotSupportedError,
        "WebNN Service is not supported on this platform."));
    DLOG(ERROR) << "WebNN Service is not supported on this platform.";
  }
#elif BUILDFLAG(IS_LINUX)
  // The remote sent to the renderer.
  mojo::PendingRemote<mojom::WebNNContext> blink_remote;
  impls_.push_back(base::WrapUnique<WebNNContextImpl>(new tflite::ContextImpl(
      blink_remote.InitWithNewPipeAndPassReceiver(), this)));
  std::move(callback).Run(
      mojom::CreateContextResult::NewContextRemote(std::move(blink_remote)));
#else
  // TODO(crbug.com/1273291): Supporting WebNN Service on the platform.
  std::move(callback).Run(ToError<mojom::CreateContextResult>(
      mojom::Error::Code::kNotSupportedError,
      "WebNN Service is not supported on this platform."));
  DLOG(ERROR) << "WebNN Service is not supported on this platform.";
#endif
}

}  // namespace webnn
