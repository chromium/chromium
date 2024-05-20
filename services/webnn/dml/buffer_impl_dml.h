// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_BUFFER_IMPL_DML_H_
#define SERVICES_WEBNN_DML_BUFFER_IMPL_DML_H_

#include <d3d12.h>
#include <wrl.h>

#include "services/webnn/webnn_buffer_impl.h"

namespace webnn::dml {

class ContextImplDml;

class BufferImplDml final : public WebNNBufferImpl {
 public:
  BufferImplDml(mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
                Microsoft::WRL::ComPtr<ID3D12Resource> buffer,
                ContextImplDml* context,
                uint64_t size,
                const base::UnguessableToken& buffer_handle);

  BufferImplDml(const BufferImplDml&) = delete;
  BufferImplDml& operator=(const BufferImplDml&) = delete;
  ~BufferImplDml() override;

  ID3D12Resource* buffer() const { return buffer_.Get(); }

 private:
  void ReadBufferImpl(ReadBufferCallback callback) override;
  void WriteBufferImpl(mojo_base::BigBuffer src_buffer) override;

  Microsoft::WRL::ComPtr<ID3D12Resource> buffer_;
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_BUFFER_IMPL_DML_H_
