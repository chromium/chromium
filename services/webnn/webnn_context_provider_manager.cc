// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_provider_manager.h"

#include "services/webnn/webnn_context_provider_impl.h"

namespace webnn {

WebNNContextProviderManager::WebNNContextProviderManager() = default;

WebNNContextProviderManager::~WebNNContextProviderManager() = default;

#if !BUILDFLAG(IS_CHROMEOS)
void WebNNContextProviderManager::CreateWebNNContextProviderImpl(
    int client_id,
    mojo::PendingReceiver<mojom::WebNNContextProvider> receiver,
    scoped_refptr<gpu::SharedContextState> shared_context_state,
    gpu::GpuFeatureInfo gpu_feature_info,
    gpu::GPUInfo gpu_info) {
  // Safe to use base::Unretained because `this` manager owns
  // the provider that won't be destroyed until the callback executes.
  provider_impls_.emplace(
      client_id,
      WebNNContextProviderImpl::Create(
          std::move(receiver),
          base::BindOnce(&WebNNContextProviderManager::
                             DisconnectAndDestroyWebNNContextProviderImpl,
                         base::Unretained(this), client_id),
          std::move(shared_context_state), std::move(gpu_feature_info),
          std::move(gpu_info)));
}
#endif

void WebNNContextProviderManager::DisconnectAndDestroyWebNNContextProviderImpl(
    int client_id) {
  const auto it = provider_impls_.find(client_id);
  CHECK(it != provider_impls_.end());
  // Upon calling erase, the client_id will no longer refer to a valid
  // `WebNNContextProviderImpl`.
  provider_impls_.erase(client_id);
}

}  // namespace webnn
