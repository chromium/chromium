// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_BUFFER_IMPL_DML_H_
#define SERVICES_WEBNN_DML_BUFFER_IMPL_DML_H_

#include "services/webnn/public/mojom/webnn_buffer.mojom-forward.h"
#include "services/webnn/webnn_buffer_impl.h"
#include "third_party/microsoft_dxheaders/src/include/directx/d3d12.h"

// Windows SDK headers should be included after DirectX headers.
#include <wrl.h>

namespace webnn::dml {

class ContextImplDml;

class BufferImplDml final : public WebNNBufferImpl {
 public:
  BufferImplDml(mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
                Microsoft::WRL::ComPtr<ID3D12Resource> buffer,
                ContextImplDml* context,
                mojom::BufferInfoPtr buffer_info,
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

  base::WeakPtr<BufferImplDml> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  void ReadBufferImpl(ReadBufferCallback callback) override;
  void WriteBufferImpl(mojo_base::BigBuffer src_buffer) override;

  // The D3D12 resource that holds the buffer data.
  // The buffer must always remain valid after creation and could outlive
  // the scope of this `BufferImplDml` instance because it may be used
  // as the key to cache and synchronize buffers used in recordering.
  const Microsoft::WRL::ComPtr<ID3D12Resource> buffer_;

  // The fence value used to track progress of GPU execution of commands using
  // this buffer. Comparing it with the command queue's completed fence can
  // indicate whether commands have completed execution.
  uint64_t last_submission_fence_value_ = UINT64_MAX;

  base::WeakPtrFactory<BufferImplDml> weak_factory_{this};
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_BUFFER_IMPL_DML_H_
