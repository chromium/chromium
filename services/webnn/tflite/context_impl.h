// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_TFLITE_CONTEXT_IMPL_H_
#define SERVICES_WEBNN_TFLITE_CONTEXT_IMPL_H_

#include "services/webnn/webnn_context_impl.h"

namespace webnn::tflite {

// `ContextImpl` is created by `WebNNContextProviderImpl` and responsible for
// creating a `GraphImpl` which uses TFLite for inference.
class ContextImpl final : public WebNNContextImpl {
 public:
  ContextImpl(mojo::PendingReceiver<mojom::WebNNContext> receiver,
              WebNNContextProviderImpl* context_provider);

  ContextImpl(const WebNNContextImpl&) = delete;
  ContextImpl& operator=(const ContextImpl&) = delete;

  ~ContextImpl() override;

 private:
  void CreateGraphImpl(mojom::GraphInfoPtr graph_info,
                       CreateGraphCallback callback) override;

  std::unique_ptr<WebNNBufferImpl> CreateBufferImpl(
      mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
      mojom::BufferInfoPtr buffer_info,
      const base::UnguessableToken& buffer_handle) override;

  void ReadBufferImpl(const WebNNBufferImpl& src_buffer,
                      mojom::WebNNBuffer::ReadBufferCallback callback) override;

  void WriteBufferImpl(const WebNNBufferImpl& dst_buffer,
                       mojo_base::BigBuffer src_buffer) override;
};

}  // namespace webnn::tflite

#endif  // SERVICES_WEBNN_DML_CONTEXT_IMPL_H_
