// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_TFLITE_BUFFER_IMPL_H_
#define SERVICES_WEBNN_TFLITE_BUFFER_IMPL_H_

#include "base/containers/heap_array.h"
#include "services/webnn/webnn_buffer_impl.h"

namespace webnn {

class WebNNContextImpl;

namespace tflite {

// A simple implementation of WebNNBuffer which uses normal CPU buffers
// since TFLite is currently only configured to use CPU delegates.
class BufferImpl final : public WebNNBufferImpl {
 public:
  static std::unique_ptr<WebNNBufferImpl> Create(
      mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
      WebNNContextImpl* context,
      mojom::BufferInfoPtr buffer_info,
      const base::UnguessableToken& buffer_handle);

  ~BufferImpl() override;

  BufferImpl(const BufferImpl&) = delete;
  BufferImpl& operator=(const BufferImpl&) = delete;

 private:
  BufferImpl(mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
             WebNNContextImpl* context,
             size_t size,
             const base::UnguessableToken& buffer_handle);

  void ReadBufferImpl(ReadBufferCallback callback) override;
  void WriteBufferImpl(mojo_base::BigBuffer src_buffer) override;

  // TODO(https://crbug.com/40278771): Use a real hardware buffer on platforms
  // where that would be beneficial.
  base::HeapArray<uint8_t> buffer_;
};

}  // namespace tflite

}  // namespace webnn

#endif  // SERVICES_WEBNN_TFLITE_BUFFER_IMPL_H_
