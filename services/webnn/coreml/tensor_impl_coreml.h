// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_COREML_TENSOR_IMPL_COREML_H_
#define SERVICES_WEBNN_COREML_TENSOR_IMPL_COREML_H_

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/queueable_resource_state.h"
#include "services/webnn/webnn_tensor_impl.h"

namespace webnn {

class WebNNContextImpl;

namespace coreml {

class BufferContent;

class API_AVAILABLE(macos(12.3)) TensorImplCoreml final
    : public WebNNTensorImpl {
 public:
  static base::expected<std::unique_ptr<WebNNTensorImpl>, mojom::ErrorPtr>
  Create(mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
         WebNNContextImpl* context,
         mojom::TensorInfoPtr tensor_info);

  TensorImplCoreml(
      mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
      WebNNContextImpl* context,
      mojom::TensorInfoPtr tensor_info,
      scoped_refptr<QueueableResourceState<BufferContent>> buffer_state,
      base::PassKey<TensorImplCoreml> pass_key);

  TensorImplCoreml(const TensorImplCoreml&) = delete;
  TensorImplCoreml& operator=(const TensorImplCoreml&) = delete;
  ~TensorImplCoreml() override;

  // WebNNTensorImpl:
  void ReadTensorImpl(mojom::WebNNTensor::ReadTensorCallback callback) override;
  void WriteTensorImpl(mojo_base::BigBuffer src_buffer) override;

  const scoped_refptr<QueueableResourceState<BufferContent>>& GetBufferState()
      const;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<QueueableResourceState<BufferContent>> buffer_state_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace coreml

}  // namespace webnn

#endif  // SERVICES_WEBNN_COREML_TENSOR_IMPL_COREML_H_
