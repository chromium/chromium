// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_TFLITE_CONTEXT_IMPL_CROS_H_
#define SERVICES_WEBNN_TFLITE_CONTEXT_IMPL_CROS_H_

#include "components/ml/mojom/ml_service.mojom.h"
#include "components/ml/mojom/web_platform_model.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/webnn_context_impl.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"

namespace webnn::tflite {

// `ContextImplCrOS` is created by `WebNNContextProviderImpl` and responsible
// for creating a `GraphImpl` which uses TFLite for inference.
class ContextImplCrOS final : public WebNNContextImpl {
 public:
  ContextImplCrOS(mojo::PendingReceiver<mojom::WebNNContext> receiver,
                  WebNNContextProviderImpl* context_provider);

  ContextImplCrOS(const ContextImplCrOS&) = delete;
  ContextImplCrOS& operator=(const ContextImplCrOS&) = delete;

  ~ContextImplCrOS() override;

  // Load the TFLite model with ML Service, the `ModelLoader` interface needs to
  // be created if it's not bound.
  void LoadModel(flatbuffers::DetachedBuffer model_content,
                 ml::model_loader::mojom::ModelLoader::LoadCallback callback);

 private:
  void CreateGraphImpl(mojom::GraphInfoPtr graph_info,
                       CreateGraphCallback callback) override;

  std::unique_ptr<WebNNBufferImpl> CreateBufferImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
      mojom::BufferInfoPtr buffer_info,
      const base::UnguessableToken& buffer_handle) override;

  void ReadBufferImpl(const WebNNBufferImpl& src_buffer,
                      mojom::WebNNBuffer::ReadBufferCallback callback) override;

  void WriteBufferImpl(const WebNNBufferImpl& dst_buffer,
                       mojo_base::BigBuffer src_buffer) override;

  // The TFLite model will be loaded in the callback when creating `ModelLoader`
  // interface successfully.
  void OnModelLoaderCreated(
      mojo::PendingRemote<ml::model_loader::mojom::ModelLoader>
          webnn_service_remote,
      flatbuffers::DetachedBuffer model_content,
      ml::model_loader::mojom::ModelLoader::LoadCallback callback,
      ml::model_loader::mojom::CreateModelLoaderResult result);

  mojo::Remote<ml::model_loader::mojom::ModelLoader> model_loader_remote_;

  base::WeakPtrFactory<ContextImplCrOS> weak_factory_{this};
};

}  // namespace webnn::tflite

#endif  // SERVICES_WEBNN_DML_CONTEXT_IMPL_H_
