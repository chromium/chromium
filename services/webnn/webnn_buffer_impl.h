// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_BUFFER_IMPL_H_
#define SERVICES_WEBNN_WEBNN_BUFFER_IMPL_H_

#include "base/component_export.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/mojom/webnn_buffer.mojom.h"
#include "services/webnn/webnn_object_impl.h"

namespace webnn {

class WebNNContextImpl;

// GPU process implementation of the MLBuffer interface exposed to script.
// Owned by the WebNNContextImpl which created it.
class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNBufferImpl
    : public mojom::WebNNBuffer,
      public WebNNObjectImpl<blink::WebNNBufferToken> {
 public:
  explicit WebNNBufferImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
      WebNNContextImpl* context,
      mojom::BufferInfoPtr buffer_info);
  ~WebNNBufferImpl() override;

  WebNNBufferImpl(const WebNNBufferImpl&) = delete;
  WebNNBufferImpl& operator=(const WebNNBufferImpl&) = delete;

  OperandDataType data_type() const { return descriptor_.data_type(); }
  const std::vector<uint32_t>& shape() const { return descriptor_.shape(); }

  size_t PackedByteLength() const { return descriptor_.PackedByteLength(); }
  size_t NumberOfElements() const { return descriptor_.NumberOfElements(); }

  base::WeakPtr<const WebNNBufferImpl> GetWeakPtr() const {
    return weak_factory_.GetWeakPtr();
  }

 protected:
  // This method will be called by `ReadBuffer()` after the read info is
  // validated. A backend subclass should implement this method to read data
  // from a platform specific buffer.
  virtual void ReadBufferImpl(
      mojom::WebNNBuffer::ReadBufferCallback callback) = 0;

  // This method will be called by `WriteBuffer()` after the write info is
  // validated. A backend subclass should implement this method to write data
  // to a platform specific buffer.
  virtual void WriteBufferImpl(mojo_base::BigBuffer src_buffer) = 0;

  // WebNNContextImpl owns this object.
  const raw_ptr<WebNNContextImpl> context_;

 private:
  // mojom::WebNNBuffer
  void ReadBuffer(ReadBufferCallback callback) override;
  void WriteBuffer(mojo_base::BigBuffer src_buffer) override;

  // `OnDisconnect` is called from two places.
  //  - When the buffer is explicitly destroyed by the WebNN
  //  developer via the WebNN API.
  //  - When the buffer is dropped by the WebNN developer where
  //  the buffer gets implicitly destroyed upon garbage collection.
  void OnDisconnect();

  const OperandDescriptor descriptor_;

  mojo::AssociatedReceiver<mojom::WebNNBuffer> receiver_;

  base::WeakPtrFactory<WebNNBufferImpl> weak_factory_{this};
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_BUFFER_IMPL_H_
