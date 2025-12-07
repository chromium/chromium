// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_TENSOR_IMPL_H_
#define SERVICES_WEBNN_WEBNN_TENSOR_IMPL_H_

#include "base/component_export.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/sequence_deleter.h"
#include "services/webnn/webnn_object_impl.h"

namespace webnn {

class WebNNContextImpl;

// GPU process implementation of the MLTensor interface exposed to script.
class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNTensorImpl
    : public WebNNObjectImpl<mojom::WebNNTensor,
                             blink::WebNNTensorToken,
                             mojo::AssociatedReceiver<mojom::WebNNTensor>> {
 public:
  using RepresentationPtr =
      std::unique_ptr<gpu::WebNNTensorRepresentation, OnTaskRunnerDeleter>;

  WebNNTensorImpl(mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
                  base::WeakPtr<WebNNContextImpl> context,
                  mojom::TensorInfoPtr tensor_info);

  WebNNTensorImpl(mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
                  base::WeakPtr<WebNNContextImpl> context,
                  mojom::TensorInfoPtr tensor_info,
                  RepresentationPtr representation);

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

  // Returns true if the tensor has been exported (e.g., to WebGPU)
  // and is not currently being accessed by WebNN.
  // Used to prevent concurrent access between WebNN and other consumers.
  bool is_exported() const {
    return representation_ && !representation_access_;
  }

  // Called by `ImportTensor()` after WebNN begins access of the
  // platform-specific tensor as a shared image.
  // Backend subclasses implement this to perform any necessary
  // device synchronization and store the access. Returns true on success.
  // On success, the subclass should assign `representation_access_` to
  // gpu::WebNNTensorRepresentation::BeginScopedAccess().
  virtual bool ImportTensorImpl() = 0;

 protected:
  ~WebNNTensorImpl() override;

  // This method will be called by `ReadTensor()` after the read info is
  // validated. A backend subclass should implement this method to read data
  // from a platform specific buffer.
  virtual void ReadTensorImpl(
      mojom::WebNNTensor::ReadTensorCallback callback) = 0;

  using ScopedAccessPtr =
      std::unique_ptr<gpu::WebNNTensorRepresentation::ScopedAccess,
                      OnTaskRunnerDeleter>;

  // Called by `ExportTensor()` after WebNN finishes access of the
  // platform-specific tensor as a shared image.
  // Backend subclasses implement this to perform any necessary
  // device synchronization.
  virtual void ExportTensorImpl(ScopedAccessPtr access,
                                ExportTensorCallback callback) = 0;

  base::WeakPtr<WebNNContextImpl> context_;

  // The shared image representation used to access the contents from shared
  // image. Only valid when usage has WebGPUInterop.
  RepresentationPtr representation_{nullptr, OnTaskRunnerDeleter(nullptr)};

  // Non-null only while WebNN holds exclusive access. Null if exported.
  ScopedAccessPtr representation_access_{nullptr, OnTaskRunnerDeleter(nullptr)};

 private:
  // mojom::WebNNTensor
  void ReadTensor(ReadTensorCallback callback) override;
  void WriteTensor(mojo_base::BigBuffer src_buffer) override;
  void ImportTensor(const gpu::SyncToken& fence) override;
  void ExportTensor(ExportTensorCallback callback) override;

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
