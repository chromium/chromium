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

  // Called when a recorded command will modify the contents of this buffer.
  // The caller must compare last_submission_fence_value() with the command
  // queue's completed fence before mapping the buffer.
  void SetLastSubmissionFenceValue(uint64_t last_submission_fence_value);
  uint64_t last_submission_fence_value() const;

 private:
  void ReadBufferImpl(ReadBufferCallback callback) override;
  void WriteBufferImpl(mojo_base::BigBuffer src_buffer) override;

  Microsoft::WRL::ComPtr<ID3D12Resource> buffer_;

  // The fence value used to track progress of GPU execution of commands using
  // this buffer. Comparing it with the command queue's completed fence can
  // indicate whether commands have completed execution.
  uint64_t last_submission_fence_value_ = UINT64_MAX;
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_BUFFER_IMPL_DML_H_
