// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_CONTEXT_IMPL_ORT_H_
#define SERVICES_WEBNN_ORT_CONTEXT_IMPL_ORT_H_

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"
#include "third_party/microsoft_dxheaders/include/onnxruntime_c_api.h"

namespace webnn::ort {

#define ORT_ABORT_ON_ERROR(g_ort, expr)                             \
  do {                                                       \
    OrtStatus* onnx_status = (expr);                         \
    if (onnx_status != NULL) {                               \
      const char* msg = g_ort->GetErrorMessage(onnx_status); \
      fprintf(stderr, "%s\n", msg);                          \
      g_ort->ReleaseStatus(onnx_status);                     \
      abort();                                               \
    }                                                        \
  } while (0);

// `ContextImplOrt` is created by `WebNNContextProviderImpl` and responsible
// for creating a `GraphImplOrt` which uses ORT for inference.
class ContextImplOrt final : public WebNNContextImpl {
 public:
  ContextImplOrt(mojo::PendingReceiver<mojom::WebNNContext> receiver,
                 WebNNContextProviderImpl* context_provider,
                 mojom::CreateContextOptionsPtr options);

  ContextImplOrt(const WebNNContextImpl&) = delete;
  ContextImplOrt& operator=(const ContextImplOrt&) = delete;

  ~ContextImplOrt() override;

  // WebNNContextImpl:
  base::WeakPtr<WebNNContextImpl> AsWeakPtr() override;

  static ContextProperties GetContextProperties();

  static const OrtApi* GetGlobalOrt();
  static OrtEnv* GetEnv(const OrtApi* g_ort);

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

  base::WeakPtrFactory<ContextImplOrt> weak_factory_{this};
  static OrtEnv* env_;
  static const OrtApi* g_ort_;
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_CONTEXT_IMPL_ORT_H_
