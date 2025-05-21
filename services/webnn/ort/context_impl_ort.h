// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_CONTEXT_IMPL_ORT_H_
#define SERVICES_WEBNN_ORT_CONTEXT_IMPL_ORT_H_

#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/ort/ort_session_options.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/webnn_context_impl.h"

namespace webnn {

class WebNNConstantOperand;

namespace ort {

// `ContextImplOrt` is created by `WebNNContextProviderImpl` and responsible
// for creating a `GraphImplOrt` which uses ONNX Runtime for inference.
class ContextImplOrt final : public WebNNContextImpl {
 public:
  ContextImplOrt(mojo::PendingReceiver<mojom::WebNNContext> receiver,
                 WebNNContextProviderImpl* context_provider,
                 mojom::CreateContextOptionsPtr options,
                 ScopedOrtEnv env,
                 scoped_refptr<SessionOptions> session_options);

  ContextImplOrt(const WebNNContextImpl&) = delete;
  ContextImplOrt& operator=(const ContextImplOrt&) = delete;

  ~ContextImplOrt() override;

  // WebNNContextImpl:
  base::WeakPtr<WebNNContextImpl> AsWeakPtr() override;

  static ContextProperties GetContextProperties();

  scoped_refptr<SessionOptions> session_options() const {
    return session_options_;
  }

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

  // It is sequence bound.
  ScopedOrtEnv env_;

  // The session options are shared among all the sessions created by this
  // context.
  scoped_refptr<SessionOptions> session_options_;

  base::WeakPtrFactory<ContextImplOrt> weak_factory_{this};
};

}  // namespace ort
}  // namespace webnn

#endif  // SERVICES_WEBNN_ORT_CONTEXT_IMPL_ORT_H_
