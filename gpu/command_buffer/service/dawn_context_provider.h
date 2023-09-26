// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_DAWN_CONTEXT_PROVIDER_H_
#define GPU_COMMAND_BUFFER_SERVICE_DAWN_CONTEXT_PROVIDER_H_

#include <dawn/platform/DawnPlatform.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/dawn_caching_interface.h"
#include "gpu/config/gpu_preferences.h"
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
namespace webgpu {
class DawnInstance;
}  // namespace webgpu

class GPU_GLES2_EXPORT DawnContextProvider {
 public:
  using CacheBlobCallback = webgpu::DawnCachingInterface::CacheBlobCallback;
  static std::unique_ptr<DawnContextProvider> Create(
      const GpuPreferences& gpu_preferences = GpuPreferences(),
      webgpu::DawnCachingInterfaceFactory* caching_interface_factory = nullptr,
      CacheBlobCallback callback = {});
  static std::unique_ptr<DawnContextProvider> CreateWithBackend(
      wgpu::BackendType backend_type,
      bool force_fallback_adapter = false,
      const GpuPreferences& gpu_preferences = GpuPreferences(),
      webgpu::DawnCachingInterfaceFactory* caching_interface_factory = nullptr,
      CacheBlobCallback callback = {});

  static wgpu::BackendType GetDefaultBackendType();
  static bool DefaultForceFallbackAdapter();

  DawnContextProvider(const DawnContextProvider&) = delete;
  DawnContextProvider& operator=(const DawnContextProvider&) = delete;

  ~DawnContextProvider();

  wgpu::Device GetDevice() const { return device_; }
  wgpu::BackendType backend_type() const { return backend_type_; }
  bool is_vulkan_swiftshader_adapter() const {
    return is_vulkan_swiftshader_adapter_;
  }
  wgpu::Instance GetInstance() const;

  bool InitializeGraphiteContext(
      const skgpu::graphite::ContextOptions& options);

  skgpu::graphite::Context* GetGraphiteContext() const {
    return graphite_context_.get();
  }

  webgpu::DawnCachingInterfaceFactory* caching_interface_factory() const {
    return caching_interface_factory_.get();
  }

#if BUILDFLAG(IS_WIN)
  Microsoft::WRL::ComPtr<ID3D11Device> GetD3D11Device() const;
#endif

 private:
  explicit DawnContextProvider(
      webgpu::DawnCachingInterfaceFactory* caching_interface_factory);
  bool Initialize(wgpu::BackendType backend_type,
                  bool force_fallback_adapter,
                  const GpuPreferences& gpu_preferences,
                  CacheBlobCallback callback);

  raw_ptr<webgpu::DawnCachingInterfaceFactory> caching_interface_factory_;
  std::unique_ptr<dawn::platform::Platform> platform_;
  std::unique_ptr<webgpu::DawnInstance> instance_;
  wgpu::Device device_;
  wgpu::BackendType backend_type_;
  bool is_vulkan_swiftshader_adapter_ = false;
  std::unique_ptr<skgpu::graphite::Context> graphite_context_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_DAWN_CONTEXT_PROVIDER_H_
