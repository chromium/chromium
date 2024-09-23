// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_TFLITE_CONTEXT_IMPL_TFLITE_H_
#define SERVICES_WEBNN_TFLITE_CONTEXT_IMPL_TFLITE_H_

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn::tflite {

// `ContextImplTflite` is created by `WebNNContextProviderImpl` and responsible
// for creating a `GraphImplTflite` which uses TFLite for inference.
class ContextImplTflite final : public WebNNContextImpl {
 public:
  ContextImplTflite(mojo::PendingReceiver<mojom::WebNNContext> receiver,
                    WebNNContextProviderImpl* context_provider,
                    mojom::CreateContextOptionsPtr options);

  ContextImplTflite(const WebNNContextImpl&) = delete;
  ContextImplTflite& operator=(const ContextImplTflite&) = delete;

  ~ContextImplTflite() override;

  // WebNNContextImpl:
  base::WeakPtr<WebNNContextImpl> AsWeakPtr() override;

 private:
  void CreateGraphImpl(
      mojom::GraphInfoPtr graph_info,
      WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
      base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      CreateGraphImplCallback callback) override;

  void CreateTensorImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
      mojom::TensorInfoPtr tensor_info,
      CreateTensorImplCallback callback) override;

  base::WeakPtrFactory<ContextImplTflite> weak_factory_{this};
};

}  // namespace webnn::tflite

#endif  // SERVICES_WEBNN_TFLITE_CONTEXT_IMPL_TFLITE_H_
