// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_BUFFER_IMPL_H_
#define SERVICES_WEBNN_WEBNN_BUFFER_IMPL_H_

#include "base/component_export.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/public/mojom/webnn_buffer.mojom.h"
#include "services/webnn/webnn_object_impl.h"

namespace webnn {

class WebNNContextImpl;

class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNBufferImpl
    : public mojom::WebNNBuffer,
      public WebNNObjectImpl {
 public:
  explicit WebNNBufferImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
      WebNNContextImpl* context,
      uint64_t size,
      const base::UnguessableToken& buffer_handle);
  ~WebNNBufferImpl() override;

  WebNNBufferImpl(const WebNNBufferImpl&) = delete;
  WebNNBufferImpl& operator=(const WebNNBufferImpl&) = delete;

  // TODO(crbug.com/40278771): prefer using `size_t` over `uint64_t`.
  uint64_t size() const { return size_; }

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

  const uint64_t size_;

  mojo::AssociatedReceiver<mojom::WebNNBuffer> receiver_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_BUFFER_IMPL_H_
