// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_TFLITE_CONTEXT_IMPL_CROS_H_
#define SERVICES_WEBNN_TFLITE_CONTEXT_IMPL_CROS_H_

#include "base/memory/weak_ptr.h"
#include "components/ml/mojom/web_platform_model.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-forward.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"

namespace webnn::tflite {

// `ContextImplCrOS` is created by `WebNNContextProviderImpl` and responsible
// for creating a `GraphImplTflite` which uses TFLite for inference.
class ContextImplCrOS final : public WebNNContextImpl {
 public:
  ContextImplCrOS(mojo::PendingReceiver<mojom::WebNNContext> receiver,
                  WebNNContextProviderImpl* context_provider,
                  mojom::CreateContextOptionsPtr options);

  ContextImplCrOS(const ContextImplCrOS&) = delete;
  ContextImplCrOS& operator=(const ContextImplCrOS&) = delete;

  ~ContextImplCrOS() override;

  // WebNNContextImpl:
  base::WeakPtr<WebNNContextImpl> AsWeakPtr() override;

  // Load the TFLite model with ML Service, the `ModelLoader` interface needs to
  // be created if it's not bound.
  void LoadModel(flatbuffers::DetachedBuffer model_content,
                 ml::model_loader::mojom::ModelLoader::LoadCallback callback);

 private:
  void CreateGraphImpl(
      mojom::GraphInfoPtr graph_info,
      WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
      CreateGraphImplCallback callback) override;

  void CreateTensorImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
      mojom::TensorInfoPtr tensor_info,
      CreateTensorImplCallback callback) override;

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

#endif  // SERVICES_WEBNN_TFLITE_CONTEXT_IMPL_CROS_H_
