// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_TENSOR_IMPL_ORT_H_
#define SERVICES_WEBNN_ORT_TENSOR_IMPL_ORT_H_

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "services/webnn/ort/buffer_content_ort.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom-forward.h"
#include "services/webnn/queueable_resource_state.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_tensor_impl.h"
#include "third_party/onnxruntime_headers/src/include/onnxruntime/core/session/onnxruntime_c_api.h"

namespace webnn::ort {

class TensorImplOrt final : public WebNNTensorImpl {
 public:
  TensorImplOrt(
      mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
      WebNNContextImpl* context,
      mojom::TensorInfoPtr tensor_info,
      scoped_refptr<QueueableResourceState<BufferContentOrt>> buffer_state);

  TensorImplOrt(const TensorImplOrt&) = delete;
  TensorImplOrt& operator=(const TensorImplOrt&) = delete;
  ~TensorImplOrt() override;

  const scoped_refptr<QueueableResourceState<BufferContentOrt>>&
  GetBufferState() const;

 private:
  void ReadTensorImpl(ReadTensorCallback callback) override;
  void WriteTensorImpl(mojo_base::BigBuffer src_buffer) override;

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<QueueableResourceState<BufferContentOrt>> buffer_state_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_TENSOR_IMPL_ORT_H_
