// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_TENSOR_IMPL_ORT_H_
#define SERVICES_WEBNN_ORT_TENSOR_IMPL_ORT_H_

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom-forward.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_tensor_impl.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/winml/winml/onnxruntime_c_api.h"

namespace webnn::ort {

class TensorImplOrt final : public WebNNTensorImpl {
 public:
  TensorImplOrt(mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
                base::WeakPtr<WebNNContextImpl> context,
                mojom::TensorInfoPtr tensor_info,
                size_t size,
                ScopedOrtValue tensor,
                bool can_access_on_cpu);

  TensorImplOrt(const TensorImplOrt&) = delete;
  TensorImplOrt& operator=(const TensorImplOrt&) = delete;

  OrtValue* tensor() const { return tensor_.get(); }

 private:
  ~TensorImplOrt() override;

  void ReadTensorImpl(ReadTensorCallback callback) override;
  void WriteTensorImpl(mojo_base::BigBuffer src_buffer) override;
  bool ImportTensorImpl() override;
  void ExportTensorImpl(ScopedAccessPtr access,
                        ExportTensorCallback callback) override;

  base::span<uint8_t> AsSpan() const;

  const ScopedOrtValue tensor_ GUARDED_BY_CONTEXT(gpu_sequence_checker_);
  const size_t size_;
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_TENSOR_IMPL_ORT_H_
