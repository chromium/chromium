// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_TFLITE_CONTEXT_IMPL_TFLITE_H_
#define SERVICES_WEBNN_TFLITE_CONTEXT_IMPL_TFLITE_H_

#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn::tflite {

// `ContextImplTflite` is created by `WebNNContextProviderImpl` and responsible
// for creating a `GraphImplTflite` which uses TFLite for inference.
class ContextImplTflite final : public WebNNContextImpl {
 public:
  ContextImplTflite(
      mojo::PendingReceiver<mojom::WebNNContext> receiver,
      mojo::PendingRemote<mojom::WebNNContextClient> client_remote,
      WebNNContextProviderImpl* context_provider,
      mojom::CreateContextOptionsPtr options,
      base::UnguessableToken context_handle);

  ContextImplTflite(const WebNNContextImpl&) = delete;
  ContextImplTflite& operator=(const ContextImplTflite&) = delete;

  ~ContextImplTflite() override;

  // WebNNContextImpl:
  base::WeakPtr<WebNNContextImpl> AsWeakPtr() override;

 private:
  void CreateGraphImpl(
      mojom::GraphInfoPtr graph_info,
      WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
      CreateGraphImplCallback callback) override;

  std::unique_ptr<WebNNBufferImpl> CreateBufferImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
      mojom::BufferInfoPtr buffer_info,
      const base::UnguessableToken& buffer_handle) override;

  base::WeakPtrFactory<ContextImplTflite> weak_factory_{this};
};

}  // namespace webnn::tflite

#endif  // SERVICES_WEBNN_TFLITE_CONTEXT_IMPL_TFLITE_H_
