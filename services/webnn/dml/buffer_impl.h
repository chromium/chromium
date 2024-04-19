// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_BUFFER_IMPL_H_
#define SERVICES_WEBNN_DML_BUFFER_IMPL_H_

#include <d3d12.h>
#include <wrl.h>

#include "services/webnn/webnn_buffer_impl.h"

namespace webnn::dml {

class ContextImpl;

class BufferImpl final : public WebNNBufferImpl {
 public:
  BufferImpl(mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
             Microsoft::WRL::ComPtr<ID3D12Resource> buffer,
             ContextImpl* context,
             uint64_t size,
             const base::UnguessableToken& buffer_handle);

  BufferImpl(const BufferImpl&) = delete;
  BufferImpl& operator=(const BufferImpl&) = delete;
  ~BufferImpl() override;

  ID3D12Resource* buffer() const { return buffer_.Get(); }

 private:
  void ReadBufferImpl(ReadBufferCallback callback) override;
  void WriteBufferImpl(mojo_base::BigBuffer src_buffer) override;

  Microsoft::WRL::ComPtr<ID3D12Resource> buffer_;
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_BUFFER_IMPL_H_
