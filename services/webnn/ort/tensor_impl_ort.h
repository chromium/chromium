// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_TENSOR_IMPL_ORT_H_
#define SERVICES_WEBNN_ORT_TENSOR_IMPL_ORT_H_

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "services/webnn/ort/device_allocator.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom-forward.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_tensor_impl.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/winml/winml/onnxruntime_c_api.h"

namespace webnn::ort {

class TensorImplOrt final : public WebNNTensorImpl {
 public:
  TensorImplOrt(mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
                WebNNContextImpl& context,
                mojom::TensorInfoPtr tensor_info,
                size_t size,
                ScopedOrtValue tensor,
                scoped_refptr<DeviceAllocator> device_allocator);

  TensorImplOrt(mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
                WebNNContextImpl& context,
                mojom::TensorInfoPtr tensor_info,
                RepresentationPtr representation,
                size_t size,
                ScopedOrtExternalMemoryHandle d3d_heap_external_memory_handle,
                Microsoft::WRL::ComPtr<ID3D12Resource> mapped_d3d12_buffer,
                ScopedOrtValue tensor);

  TensorImplOrt(const TensorImplOrt&) = delete;
  TensorImplOrt& operator=(const TensorImplOrt&) = delete;

  OrtValue* tensor() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return tensor_.get();
  }

 private:
  ~TensorImplOrt() override;

  void ReadTensorImpl(ReadTensorCallback callback) override;
  void WriteTensorImpl(mojo_base::BigBuffer src_buffer) override;
  bool ImportTensorImpl(ScopedAccessPtr access) override;
  void ExportTensorImpl(ScopedAccessPtr access) override;

  base::span<uint8_t> AsSpan() const;

  // The device allocator used for device tensor creation. May be nullptr if
  // device tensor is not supported.
  // If the device allocator is present, the tensor is allocated by the device
  // allocator, and its destruction depends on the allocator remaining valid.
  // Therefore, the device allocator must be referenced by `TensorImplOrt`
  // and declared before `tensor_` to ensure correct destruction order to avoid
  // use-after-free errors.
  scoped_refptr<DeviceAllocator> device_allocator_;
  // ORT wrapper around the D3D12 heap handle used for external memory import.
  // Valid if the tensor was created with the ORT interop API. Must be kept
  // alive for the lifetime of the OrtValue.
  const ScopedOrtExternalMemoryHandle d3d_heap_external_memory_handle_;
  // Exists only on the mapped-fallback interop path; keeps the buffer backing
  // `tensor_`'s raw pointer alive independent of `representation_`. Must be
  // declared before `tensor_` so it is destroyed after, since `tensor_` holds
  // a raw pointer into this buffer's mapping.
  const Microsoft::WRL::ComPtr<ID3D12Resource> mapped_d3d12_buffer_;
  const ScopedOrtValue tensor_ GUARDED_BY_CONTEXT(sequence_checker_);
  const size_t size_;
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_TENSOR_IMPL_ORT_H_
