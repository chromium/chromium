// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_TFLITE_BUFFER_IMPL_TFLITE_H_
#define SERVICES_WEBNN_TFLITE_BUFFER_IMPL_TFLITE_H_

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "services/webnn/public/mojom/webnn_buffer.mojom-forward.h"
#include "services/webnn/webnn_buffer_impl.h"

namespace webnn {

class WebNNContextImpl;

namespace tflite {

class BufferState;

// A simple implementation of WebNNBuffer which uses normal CPU buffers
// since TFLite is currently only configured to use CPU delegates.
class BufferImplTflite final : public WebNNBufferImpl {
 public:
  static std::unique_ptr<WebNNBufferImpl> Create(
      mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
      WebNNContextImpl* context,
      mojom::BufferInfoPtr buffer_info,
      const base::UnguessableToken& buffer_handle);

  BufferImplTflite(mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
                   WebNNContextImpl* context,
                   mojom::BufferInfoPtr buffer_info,
                   const base::UnguessableToken& buffer_handle,
                   scoped_refptr<BufferState> state,
                   base::PassKey<BufferImplTflite>);

  ~BufferImplTflite() override;

  BufferImplTflite(const BufferImplTflite&) = delete;
  BufferImplTflite& operator=(const BufferImplTflite&) = delete;

  const scoped_refptr<BufferState>& GetState() const;

 private:
  void ReadBufferImpl(ReadBufferCallback callback) override;
  void WriteBufferImpl(mojo_base::BigBuffer src_buffer) override;

  scoped_refptr<BufferState> state_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace tflite

}  // namespace webnn

#endif  // SERVICES_WEBNN_TFLITE_BUFFER_IMPL_TFLITE_H_
