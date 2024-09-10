// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_DML_TENSOR_IMPL_DML_H_
#define SERVICES_WEBNN_DML_TENSOR_IMPL_DML_H_

#include "services/webnn/public/mojom/webnn_tensor.mojom-forward.h"
#include "services/webnn/webnn_tensor_impl.h"
#include "third_party/microsoft_dxheaders/src/include/directx/d3d12.h"

// Windows SDK headers should be included after DirectX headers.
#include <wrl.h>

namespace webnn::dml {

class ContextImplDml;

class TensorImplDml final : public WebNNTensorImpl {
 public:
  TensorImplDml(mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
                Microsoft::WRL::ComPtr<ID3D12Resource> buffer,
                ContextImplDml* context,
                mojom::TensorInfoPtr tensor_info);

  TensorImplDml(const TensorImplDml&) = delete;
  TensorImplDml& operator=(const TensorImplDml&) = delete;
  ~TensorImplDml() override;

  ID3D12Resource* buffer() const { return buffer_.Get(); }

  // Called when a recorded command will modify the contents of this buffer.
  // The caller must compare last_submission_fence_value() with the command
  // queue's completed fence before mapping the buffer.
  void SetLastSubmissionFenceValue(uint64_t last_submission_fence_value);
  uint64_t last_submission_fence_value() const;

  base::WeakPtr<TensorImplDml> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  void ReadTensorImpl(ReadTensorCallback callback) override;
  void WriteTensorImpl(mojo_base::BigBuffer src_buffer) override;

  // The D3D12 resource that holds the tensor data.
  // The buffer must always remain valid after creation and could outlive
  // the scope of this `TensorImplDml` instance because it may be used
  // as the key to cache and synchronize buffers used in recording.
  const Microsoft::WRL::ComPtr<ID3D12Resource> buffer_;

  // The fence value used to track progress of GPU execution of commands using
  // this buffer. Comparing it with the command queue's completed fence can
  // indicate whether commands have completed execution.
  uint64_t last_submission_fence_value_ = 0;

  base::WeakPtrFactory<TensorImplDml> weak_factory_{this};
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_TENSOR_IMPL_DML_H_
