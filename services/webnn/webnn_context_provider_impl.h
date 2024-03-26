// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_CONTEXT_PROVIDER_IMPL_H_
#define SERVICES_WEBNN_WEBNN_CONTEXT_PROVIDER_IMPL_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/config/gpu_feature_info.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"

namespace webnn {

class WebNNContextImpl;

// Maintain a set of WebNNContextImpl instances that are created by the context
// provider.
class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNContextProviderImpl
    : public mojom::WebNNContextProvider {
 public:
  explicit WebNNContextProviderImpl(
#if !BUILDFLAG(IS_CHROMEOS)
      scoped_refptr<gpu::SharedContextState> shared_context_state,
      gpu::GpuFeatureInfo gpu_feature_info
#endif
  );

  WebNNContextProviderImpl(const WebNNContextProviderImpl&) = delete;
  WebNNContextProviderImpl& operator=(const WebNNContextProviderImpl&) = delete;

  ~WebNNContextProviderImpl() override;

  static void Create(
      mojo::PendingReceiver<mojom::WebNNContextProvider> receiver
#if !BUILDFLAG(IS_CHROMEOS)
      ,
      scoped_refptr<gpu::SharedContextState> shared_context_state,
      gpu::GpuFeatureInfo gpu_feature_info
#endif
  );

  static void CreateForTesting(
      mojo::PendingReceiver<mojom::WebNNContextProvider> receiver,
      bool is_gpu_supported = true);

  // Called when a WebNNContextImpl has a connection error. After this call, it
  // is no longer safe to access |impl|.
  void OnConnectionError(WebNNContextImpl* impl);

  // The test cases can override the context creating behavior by implementing
  // this class and setting its instance by SetBackendForTesting().
  class BackendForTesting {
   public:
    virtual void CreateWebNNContext(
        std::vector<std::unique_ptr<WebNNContextImpl>>& context_impls,
        WebNNContextProviderImpl* context_provider_impl,
        mojom::CreateContextOptionsPtr options,
        CreateWebNNContextCallback callback) = 0;
  };

  static void SetBackendForTesting(BackendForTesting* backend_for_testing);

 private:
  // mojom::WebNNContextProvider
  void CreateWebNNContext(mojom::CreateContextOptionsPtr options,
                          CreateWebNNContextCallback callback) override;

  std::vector<std::unique_ptr<WebNNContextImpl>> impls_;
  scoped_refptr<gpu::SharedContextState> shared_context_state_;
  const gpu::GpuFeatureInfo gpu_feature_info_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_CONTEXT_PROVIDER_IMPL_H_
