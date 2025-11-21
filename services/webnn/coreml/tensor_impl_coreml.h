// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_COREML_TENSOR_IMPL_COREML_H_
#define SERVICES_WEBNN_COREML_TENSOR_IMPL_COREML_H_

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/pass_key.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
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
  static base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr> Create(
      mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
      base::WeakPtr<WebNNContextImpl> context,
      mojom::TensorInfoPtr tensor_info);

  static base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr> Create(
      mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
      base::WeakPtr<WebNNContextImpl> context,
      mojom::TensorInfoPtr tensor_info,
      RepresentationPtr representation);

  TensorImplCoreml(
      mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
      base::WeakPtr<WebNNContextImpl> context,
      mojom::TensorInfoPtr tensor_info,
      scoped_refptr<QueueableResourceState<BufferContent>> buffer_state,
      RepresentationPtr representation,
      base::PassKey<TensorImplCoreml> pass_key);

  TensorImplCoreml(const TensorImplCoreml&) = delete;
  TensorImplCoreml& operator=(const TensorImplCoreml&) = delete;

  // WebNNTensorImpl:
  void ReadTensorImpl(mojom::WebNNTensor::ReadTensorCallback callback) override;
  void WriteTensorImpl(mojo_base::BigBuffer src_buffer) override;
  bool ImportTensorImpl() override;
  void ExportTensorImpl(ScopedAccessPtr access,
                        ExportTensorCallback callback) override;

  const scoped_refptr<QueueableResourceState<BufferContent>>& GetBufferState()
      const;

 private:
  ~TensorImplCoreml() override;

  scoped_refptr<QueueableResourceState<BufferContent>> buffer_state_
      GUARDED_BY_CONTEXT(gpu_sequence_checker_);
};

}  // namespace coreml

}  // namespace webnn

#endif  // SERVICES_WEBNN_COREML_TENSOR_IMPL_COREML_H_
