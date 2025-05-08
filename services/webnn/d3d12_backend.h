// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_D3D12_BACKEND_H_
#define SERVICES_WEBNN_D3D12_BACKEND_H_

#include <stdint.h>

#include <memory>

#include "base/component_export.h"
#include "third_party/microsoft_dxheaders/src/include/directx/d3d12.h"

// Windows SDK headers should be included after DirectX headers.
#include <wrl.h>

namespace webnn::native::d3d12 {

// WebNNSharedFence is a wrapper of an ID3D12Fence and fence value which is
// signaled when the execution on GPU is completed.
struct COMPONENT_EXPORT(WEBNN_SERVICE) WebNNSharedFence {
  virtual Microsoft::WRL::ComPtr<ID3D12Fence> GetD3D12Fence() const = 0;
  virtual uint64_t GetFenceValue() const = 0;
  virtual ~WebNNSharedFence() = default;
};

// WebNNTensor is a native interface which exposes a WebNN tensor in the
// GPU service using backend-specific APIs. Implemented by the WebNN
// service and called by shared image backings to access the tensor.
class COMPONENT_EXPORT(WEBNN_SERVICE) WebNNTensor {
 public:
  // Begin WebNN access to the underlying buffer held in the `WebNNTensor`
  // instance. Input is a fence which will be waited on by WebNN before
  // execution resumes. If successful, EndAccessWebNN() must be called to
  // BeginAccessWebNN() again.
  virtual bool BeginAccessWebNN(Microsoft::WRL::ComPtr<ID3D12Fence> wait_fence,
                                uint64_t wait_fence_value) = 0;

  // End WebNN access to the underlying buffer held in the `WebNNTensor`
  // instance. Outputs a fence to be signaled by WebNN after execution
  // completes. If successful, BeginAccessWebNN() must be called to restore
  // access to WebNN and to EndAccessWebNN() again.
  virtual std::unique_ptr<WebNNSharedFence> EndAccessWebNN() = 0;

  // Retrieves the underlying buffer held in the `WebNNTensor` instance.
  // The returned tensor buffer is a committed resource which cannot be used
  // externally until EndAccessWebNN() is called.
  virtual ID3D12Resource* GetD3D12Buffer() const = 0;

  virtual ~WebNNTensor() = default;
};

}  // namespace webnn::native::d3d12

#endif  // SERVICES_WEBNN_D3D12_BACKEND_H_
