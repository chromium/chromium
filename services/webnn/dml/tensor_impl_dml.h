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

namespace gfx {
class D3DSharedFence;
}  // namespace gfx

namespace webnn::dml {

class CommandQueue;

class COMPONENT_EXPORT(WEBNN_SERVICE) TensorImplDml final
    : public WebNNTensorImpl {
 public:
  TensorImplDml(mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
                Microsoft::WRL::ComPtr<ID3D12Resource> buffer,
                base::WeakPtr<WebNNContextImpl> context,
                mojom::TensorInfoPtr tensor_info);

  TensorImplDml(mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
                RepresentationPtr representation,
                base::WeakPtr<WebNNContextImpl> context,
                mojom::TensorInfoPtr tensor_info);

  TensorImplDml(const TensorImplDml&) = delete;
  TensorImplDml& operator=(const TensorImplDml&) = delete;

  ID3D12Resource* buffer() const { return buffer_.Get(); }

  // Called when a recorded command will modify the contents of this buffer.
  // The caller must compare last_submission_fence_value() with the command
  // queue's completed fence before mapping the buffer.
  void SetLastSubmissionFenceValue(uint64_t last_submission_fence_value);
  uint64_t last_submission_fence_value() const;

  base::WeakPtr<TensorImplDml> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  HRESULT WaitForExternalFenceAndReset(CommandQueue* command_queue);

  // Begin WebNN access to the underlying buffer held in the `WebNNTensor`
  // instance. Input is a fence which will be waited on by WebNN before
  // execution resumes. If successful, EndAccessWebNN() must be called to
  // BeginAccessWebNN() again.
  bool BeginAccessWebNN(Microsoft::WRL::ComPtr<ID3D12Fence> wait_fence,
                        uint64_t wait_fence_value);

  // End WebNN access to the underlying buffer held in the `WebNNTensor`
  // instance. Outputs a fence to be signaled by WebNN after execution
  // completes. If successful, BeginAccessWebNN() must be called to restore
  // access to WebNN and to EndAccessWebNN() again.
  scoped_refptr<gfx::D3DSharedFence> EndAccessWebNN();

 private:
  ~TensorImplDml() override;

  void ReadTensorImpl(ReadTensorCallback callback) override;
  void WriteTensorImpl(mojo_base::BigBuffer src_buffer) override;
  bool ImportTensorImpl() override;
  void ExportTensorImpl(ScopedAccessPtr access,
                        ExportTensorCallback callback) override;

  // The D3D12 resource that holds the tensor data.
  // The buffer must always remain valid after creation and could outlive
  // the scope of this `TensorImplDml` instance because it may be used
  // as the key to cache and synchronize buffers used in recording.
  const Microsoft::WRL::ComPtr<ID3D12Resource> buffer_;

  // The fence value used to track progress of GPU execution of commands using
  // this buffer. Comparing it with the command queue's completed fence can
  // indicate whether commands have completed execution.
  uint64_t last_submission_fence_value_ = 0;

  // Required input to `BeginAccessWebNN()` to resume WebNN execution after
  // this fence is signaled. If no value, there is no need to wait for access to
  // the tensor.
  scoped_refptr<gfx::D3DSharedFence> wait_fence_external_;

  base::WeakPtrFactory<TensorImplDml> weak_factory_{this};
};

}  // namespace webnn::dml

#endif  // SERVICES_WEBNN_DML_TENSOR_IMPL_DML_H_
