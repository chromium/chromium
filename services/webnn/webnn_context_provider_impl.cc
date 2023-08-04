// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_provider_impl.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/webnn_context_impl.h"

#if BUILDFLAG(IS_WIN)
#include "services/webnn/dml/adapter.h"
#include "services/webnn/dml/context_impl.h"
#endif

namespace webnn {

namespace {

using webnn::mojom::CreateContextOptionsPtr;
using webnn::mojom::WebNNContextProvider;

}  // namespace

WebNNContextProviderImpl::WebNNContextProviderImpl() = default;

WebNNContextProviderImpl::~WebNNContextProviderImpl() = default;

// static
void WebNNContextProviderImpl::Create(
    mojo::PendingReceiver<WebNNContextProvider> receiver) {
  mojo::MakeSelfOwnedReceiver<WebNNContextProvider>(
      std::make_unique<WebNNContextProviderImpl>(), std::move(receiver));
}

void WebNNContextProviderImpl::OnConnectionError(WebNNContextImpl* impl) {
  auto it =
      base::ranges::find(impls_, impl, &std::unique_ptr<WebNNContextImpl>::get);
  CHECK(it != impls_.end());
  impls_.erase(it);
}

void WebNNContextProviderImpl::CreateWebNNContext(
    CreateContextOptionsPtr options,
    WebNNContextProvider::CreateWebNNContextCallback callback) {
#if BUILDFLAG(IS_WIN)
  // Get the default `Adapter` instance which is created for the adapter queried
  // from ANGLE. At the current stage, all `ContextImpl` share this instance.
  //
  // TODO(crbug.com/1469755): Support getting `Adapter` instance based on
  // `options`.
  scoped_refptr<dml::Adapter> adapter = dml::Adapter::GetInstance();
  if (!adapter) {
    std::move(callback).Run(mojom::CreateContextResult::kNotSupported,
                            mojo::NullRemote());
    return;
  }
  // The remote sent to the renderer.
  mojo::PendingRemote<mojom::WebNNContext> blink_remote;
  // The receiver bound to WebNNContextImpl.
  impls_.push_back(base::WrapUnique<WebNNContextImpl>(new dml::ContextImpl(
      std::move(adapter), blink_remote.InitWithNewPipeAndPassReceiver(),
      this)));
  std::move(callback).Run(mojom::CreateContextResult::kOk,
                          std::move(blink_remote));
#else
  // TODO(crbug.com/1273291): Supporting WebNN Service on the platform.
  std::move(callback).Run(mojom::CreateContextResult::kNotSupported,
                          mojo::NullRemote());
  DLOG(ERROR) << "Platform not supported for WebNN Service.";
#endif
}

}  // namespace webnn
