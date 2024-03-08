// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_WEBNN_BUFFER_IMPL_H_
#define SERVICES_WEBNN_WEBNN_BUFFER_IMPL_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/webnn/public/mojom/webnn_buffer.mojom.h"
#include "services/webnn/webnn_object_impl.h"

namespace webnn {

class WebNNContextImpl;

class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNBufferImpl
    : public mojom::WebNNBuffer,
      public WebNNObjectImpl {
 public:
  explicit WebNNBufferImpl(mojo::PendingReceiver<mojom::WebNNBuffer> receiver,
                           WebNNContextImpl* context,
                           uint64_t size,
                           const base::UnguessableToken& buffer_handle);
  ~WebNNBufferImpl() override;

  WebNNBufferImpl(const WebNNBufferImpl&) = delete;
  WebNNBufferImpl& operator=(const WebNNBufferImpl&) = delete;

  uint64_t size() const { return size_; }

 private:
  // `OnDisconnect` is called from two places.
  //  - When the buffer is explicitly destroyed by the WebNN
  //  developer via the WebNN API.
  //  - When the buffer is dropped by the WebNN developer where
  //  the buffer gets implicitly destroyed upon garbage collection.
  void OnDisconnect();

  const uint64_t size_;

  mojo::Receiver<mojom::WebNNBuffer> receiver_;

  // WebNNContextImpl owns this object.
  const raw_ptr<WebNNContextImpl> context_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_WEBNN_BUFFER_IMPL_H_
