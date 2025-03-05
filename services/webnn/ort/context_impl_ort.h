// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_CONTEXT_IMPL_ORT_H_
#define SERVICES_WEBNN_ORT_CONTEXT_IMPL_ORT_H_

#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/ort/tensor_impl_ort.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn::ort {

// `ContextImplOrt` is created by `WebNNContextProviderImpl` and responsible
// for creating a `GraphImplOrt` which uses ORT for inference.
class ContextImplOrt final : public WebNNContextImpl {
 public:
  ContextImplOrt(mojo::PendingReceiver<mojom::WebNNContext> receiver,
                 WebNNContextProviderImpl* context_provider,
                 mojom::CreateContextOptionsPtr options,
                 ScopedOrtEnv env);

  ContextImplOrt(const WebNNContextImpl&) = delete;
  ContextImplOrt& operator=(const ContextImplOrt&) = delete;

  ~ContextImplOrt() override;

  // WebNNContextImpl:
  base::WeakPtr<WebNNContextImpl> AsWeakPtr() override;

  static ContextProperties GetContextProperties();

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

  ScopedOrtEnv env_;

  base::WeakPtrFactory<ContextImplOrt> weak_factory_{this};
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_CONTEXT_IMPL_ORT_H_
