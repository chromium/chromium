// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_DAWN_CONTEXT_PROVIDER_H_
#define GPU_COMMAND_BUFFER_SERVICE_DAWN_CONTEXT_PROVIDER_H_

#include <memory>

#include "build/build_config.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/dawn/include/dawn/native/DawnNative.h"
#include "third_party/skia/include/gpu/graphite/ContextOptions.h"
#include "third_party/skia/include/gpu/graphite/dawn/DawnTypes.h"

#if BUILDFLAG(IS_WIN)
#include <d3d11.h>
#include <wrl/client.h>
#endif

namespace skgpu::graphite {
class Context;
}  // namespace skgpu::graphite

namespace gpu {

class GPU_GLES2_EXPORT DawnContextProvider {
 public:
  static std::unique_ptr<DawnContextProvider> Create();

  DawnContextProvider(const DawnContextProvider&) = delete;
  DawnContextProvider& operator=(const DawnContextProvider&) = delete;

  ~DawnContextProvider();

  wgpu::Device GetDevice() const { return device_; }
  wgpu::Instance GetInstance() const { return instance_.Get(); }

#if BUILDFLAG(IS_WIN)
  Microsoft::WRL::ComPtr<ID3D11Device> GetD3D11Device() const;
#endif

  bool InitializeGraphiteContext(
      const skgpu::graphite::ContextOptions& options);

  skgpu::graphite::Context* GetGraphiteContext() const {
    return graphite_context_.get();
  }

 private:
  DawnContextProvider();

  wgpu::Device CreateDevice(wgpu::BackendType type);

  dawn::native::Instance instance_;
  wgpu::Device device_;
  std::unique_ptr<skgpu::graphite::Context> graphite_context_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_DAWN_CONTEXT_PROVIDER_H_
