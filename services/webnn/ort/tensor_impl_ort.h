// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_TENSOR_IMPL_ORT_H_
#define SERVICES_WEBNN_ORT_TENSOR_IMPL_ORT_H_

#include "services/webnn/public/mojom/webnn_tensor.mojom-forward.h"
#include "services/webnn/webnn_tensor_impl.h"
#include "third_party/microsoft_dxheaders/include/onnxruntime_c_api.h"

namespace webnn::ort {

class ContextImplOrt;

class TensorImplOrt final : public WebNNTensorImpl {
 public:
  TensorImplOrt(mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
                ContextImplOrt* context,
                mojom::TensorInfoPtr tensor_info);

  TensorImplOrt(const TensorImplOrt&) = delete;
  TensorImplOrt& operator=(const TensorImplOrt&) = delete;
  ~TensorImplOrt() override;

  base::WeakPtr<TensorImplOrt> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  OrtValue* tensor() const { return tensor_.get(); }

 private:
  void ReadTensorImpl(ReadTensorCallback callback) override;
  void WriteTensorImpl(mojo_base::BigBuffer src_buffer) override;

  raw_ptr<OrtValue> tensor_;
  std::vector<int64_t> shape_;

  base::WeakPtrFactory<TensorImplOrt> weak_factory_{this};
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_TENSOR_IMPL_ORT_H_
