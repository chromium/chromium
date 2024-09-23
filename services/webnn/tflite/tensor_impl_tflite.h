// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_TFLITE_TENSOR_IMPL_TFLITE_H_
#define SERVICES_WEBNN_TFLITE_TENSOR_IMPL_TFLITE_H_

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/pass_key.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom-forward.h"
#include "services/webnn/queueable_resource_state.h"
#include "services/webnn/webnn_tensor_impl.h"

namespace webnn {

class WebNNContextImpl;

namespace tflite {

class BufferContent;

// A simple implementation of WebNNTensor which uses normal CPU buffers
// since TFLite is currently only configured to use CPU delegates.
class TensorImplTflite final : public WebNNTensorImpl {
 public:
  static base::expected<std::unique_ptr<WebNNTensorImpl>, mojom::ErrorPtr>
  Create(mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
         WebNNContextImpl* context,
         mojom::TensorInfoPtr tensor_info);

  TensorImplTflite(
      mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
      WebNNContextImpl* context,
      mojom::TensorInfoPtr tensor_info,
      scoped_refptr<QueueableResourceState<BufferContent>> buffer_state,
      base::PassKey<TensorImplTflite>);

  ~TensorImplTflite() override;

  TensorImplTflite(const TensorImplTflite&) = delete;
  TensorImplTflite& operator=(const TensorImplTflite&) = delete;

  const scoped_refptr<QueueableResourceState<BufferContent>>& GetBufferState()
      const;

 private:
  void ReadTensorImpl(ReadTensorCallback callback) override;
  void WriteTensorImpl(mojo_base::BigBuffer src_buffer) override;

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<QueueableResourceState<BufferContent>> buffer_state_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace tflite

}  // namespace webnn

#endif  // SERVICES_WEBNN_TFLITE_TENSOR_IMPL_TFLITE_H_
