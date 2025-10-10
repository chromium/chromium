// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GPU_MEMORY_BUFFER_FACTORY_DXGI_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GPU_MEMORY_BUFFER_FACTORY_DXGI_H_

#include <d3d11_1.h>
#include <dxgi.h>
#include <wrl/client.h>

#include "base/threading/thread_checker.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {

class GPU_GLES2_EXPORT GpuMemoryBufferFactoryDXGI {
 public:
  GpuMemoryBufferFactoryDXGI();
  ~GpuMemoryBufferFactoryDXGI();

  GpuMemoryBufferFactoryDXGI(const GpuMemoryBufferFactoryDXGI&) = delete;
  GpuMemoryBufferFactoryDXGI& operator=(const GpuMemoryBufferFactoryDXGI&) =
      delete;

  Microsoft::WRL::ComPtr<ID3D11Device> GetOrCreateD3D11Device();
  Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture() {
    return staging_texture_;
  }

 private:
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_
      GUARDED_BY_CONTEXT(thread_checker_);

  Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GPU_MEMORY_BUFFER_FACTORY_DXGI_H_
