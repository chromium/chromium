// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_TENSOR_IMPL_H_
#define SERVICES_WEBNN_WEBNN_TENSOR_IMPL_H_

#include "base/component_export.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/webnn_object_impl.h"

namespace gpu {
class WebNNTensorRepresentation;
}  // namespace gpu

namespace webnn {

class WebNNContextImpl;

// GPU process implementation of the MLTensor interface exposed to script.
class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNTensorImpl
    : public WebNNReceiverImpl<mojom::WebNNTensor>,
      public WebNNObjectImpl<blink::WebNNTensorToken> {
 public:
  explicit WebNNTensorImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
      base::WeakPtr<WebNNContextImpl> context,
      mojom::TensorInfoPtr tensor_info);

  WebNNTensorImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
      base::WeakPtr<WebNNContextImpl> context,
      mojom::TensorInfoPtr tensor_info,
      std::unique_ptr<gpu::WebNNTensorRepresentation> representation);

  WebNNTensorImpl(const WebNNTensorImpl&) = delete;
  WebNNTensorImpl& operator=(const WebNNTensorImpl&) = delete;

  OperandDataType data_type() const { return descriptor_.data_type(); }
  const std::vector<uint32_t>& shape() const { return descriptor_.shape(); }
  MLTensorUsage usage() const { return usage_; }

  size_t PackedByteLength() const { return descriptor_.PackedByteLength(); }
  size_t NumberOfElements() const { return descriptor_.NumberOfElements(); }

  base::WeakPtr<const WebNNTensorImpl> GetWeakPtr() const {
    return weak_factory_.GetWeakPtr();
  }

  bool IsValidWithDescriptor(const OperandDescriptor& descriptor) const;

  // This method will be called by `WriteTensor()` after the write info is
  // validated. A backend subclass should implement this method to write data
  // to a platform specific buffer.
  virtual void WriteTensorImpl(mojo_base::BigBuffer src_buffer) = 0;

 protected:
  ~WebNNTensorImpl() override;

  // This method will be called by `ReadTensor()` after the read info is
  // validated. A backend subclass should implement this method to read data
  // from a platform specific buffer.
  virtual void ReadTensorImpl(
      mojom::WebNNTensor::ReadTensorCallback callback) = 0;

  base::WeakPtr<WebNNContextImpl> context_;

  // The shared image representation used to access the contents from shared
  // image. Only valid when usage has WebGPUInterop.
  std::unique_ptr<gpu::WebNNTensorRepresentation> representation_;

 private:
  // mojom::WebNNTensor
  void ReadTensor(ReadTensorCallback callback) override;
  void WriteTensor(mojo_base::BigBuffer src_buffer) override;

  // `OnDisconnect` is called from two places.
  //  - When the tensor is explicitly destroyed by the WebNN
  //  developer via the WebNN API.
  //  - When the tensor is dropped by the WebNN developer where
  //  the tensor gets implicitly destroyed upon garbage collection.
  void OnDisconnect() override;

  const OperandDescriptor descriptor_;
  const MLTensorUsage usage_;

  base::WeakPtrFactory<WebNNTensorImpl> weak_factory_{this};
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_TENSOR_IMPL_H_
