// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_COREML_CONTEXT_IMPL_COREML_H_
#define SERVICES_WEBNN_COREML_CONTEXT_IMPL_COREML_H_

#include "base/memory/weak_ptr.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"
#include "services/webnn/webnn_tensor_impl.h"

namespace webnn {

class WebNNConstantOperand;

namespace coreml {

// `ContextImplCoreml` is created by `WebNNContextProviderImpl` and responsible
// for creating a `GraphImplCoreml` for the CoreML backend on macOS.
class API_AVAILABLE(macos(14.4)) ContextImplCoreml final
    : public WebNNContextImpl {
 public:
  ContextImplCoreml(mojo::PendingReceiver<mojom::WebNNContext> receiver,
                    WebNNContextProviderImpl* context_provider,
                    mojom::CreateContextOptionsPtr options);

  ContextImplCoreml(const WebNNContextImpl&) = delete;
  ContextImplCoreml& operator=(const ContextImplCoreml&) = delete;

  ~ContextImplCoreml() override;

  // WebNNContextImpl:
  base::WeakPtr<WebNNContextImpl> AsWeakPtr() override;

 private:
  void CreateGraphImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
      mojom::GraphInfoPtr graph_info,
      WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
      base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
          constant_operands,
      base::flat_map<OperandId, WebNNTensorImpl*> constant_tensor_operands,
      CreateGraphImplCallback callback) override;

  void CreateTensorImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
      mojom::TensorInfoPtr tensor_info,
      CreateTensorImplCallback callback) override;

  base::WeakPtrFactory<ContextImplCoreml> weak_factory_{this};
};

}  // namespace coreml
}  // namespace webnn

#endif  // SERVICES_WEBNN_COREML_CONTEXT_IMPL_COREML_H_
