// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_CONTEXT_PROVIDER_MANAGER_H_
#define SERVICES_WEBNN_WEBNN_CONTEXT_PROVIDER_MANAGER_H_

#include <memory>

#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/config/gpu_feature_info.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"

namespace webnn {

class WebNNContextProviderImpl;

class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNContextProviderManager final {
 public:
  WebNNContextProviderManager();
  ~WebNNContextProviderManager();

  WebNNContextProviderManager(const WebNNContextProviderManager&) = delete;
  WebNNContextProviderManager& operator=(const WebNNContextProviderManager&) =
      delete;

#if !BUILDFLAG(IS_CHROMEOS)
  void CreateWebNNContextProviderImpl(
      int client_id,
      mojo::PendingReceiver<mojom::WebNNContextProvider> receiver,
      scoped_refptr<gpu::SharedContextState> shared_context_state,
      gpu::GpuFeatureInfo gpu_feature_info);
#endif

 private:
  // Called when a WebNNContextProviderImpl has a connection error. After this
  // call, it is no longer safe to access the provider associated with the
  // client.
  void DisconnectAndDestroyWebNNContextProviderImpl(int client_id);

  // Key is the client_id which is unique per frame/renderer/provider.
  base::flat_map<int, std::unique_ptr<WebNNContextProviderImpl>>
      provider_impls_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_CONTEXT_PROVIDER_MANAGER_H_
