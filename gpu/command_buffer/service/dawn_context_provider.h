// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_DAWN_CONTEXT_PROVIDER_H_
#define GPU_COMMAND_BUFFER_SERVICE_DAWN_CONTEXT_PROVIDER_H_

#include <memory>
#include <optional>

#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/service/dawn_caching_interface.h"
#include "gpu/command_buffer/service/gpu_persistent_cache.h"
#include "gpu/command_buffer/service/graphite_shared_context.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/dawn/include/dawn/native/DawnNative.h"
#include "third_party/skia/include/gpu/graphite/ContextOptions.h"
#include "third_party/skia/include/gpu/graphite/dawn/DawnTypes.h"

#if BUILDFLAG(IS_WIN)
#include <d3d11.h>
#include <wrl/client.h>
#endif

namespace gl {
class ProgressReporter;
}  // namespace gl

namespace gpu {

class DawnSharedContext;
class GpuProcessShmCount;

namespace webgpu {
class DawnPlatform;
}

class GPU_GLES2_EXPORT DawnContextProvider {
 public:
  using ValidateAdapterFn =
      base::FunctionRef<bool(wgpu::BackendType, wgpu::Adapter)>;

  // `validate_adapter_fn` will be called after the wgpu::Adapter is available
  // to check if it should be used. If the function returns false creation will
  // fail.
  static std::unique_ptr<DawnContextProvider> Create(
      const GpuPreferences& gpu_preferences,
      const GpuFeatureInfo& gpu_feature_info,
      gl::ProgressReporter* progress_reporter = nullptr,
      ValidateAdapterFn validate_adapter_fn = DefaultValidateAdapterFn);
  static std::unique_ptr<DawnContextProvider> CreateWithBackend(
      wgpu::BackendType backend_type,
      bool force_fallback_adapter,
      const GpuPreferences& gpu_preferences,
      const GpuFeatureInfo& gpu_feature_info,
      gl::ProgressReporter* progress_reporter = nullptr,
      ValidateAdapterFn validate_adapter_fn = DefaultValidateAdapterFn);

  // Creates a new context provider for use on a different thread that shares
  // the wgpu::Device/Adapter/Instance with `existing`.
  static std::unique_ptr<DawnContextProvider> CreateWithSharedDevice(
      const DawnContextProvider* existing);

  static wgpu::BackendType GetDefaultBackendType();
  static bool DefaultForceFallbackAdapter();

  // Default function that will say adapter is supported.
  static bool DefaultValidateAdapterFn(wgpu::BackendType, wgpu::Adapter);

  DawnContextProvider(const DawnContextProvider&) = delete;
  DawnContextProvider& operator=(const DawnContextProvider&) = delete;
  ~DawnContextProvider();

  wgpu::Device GetDevice() const;
  wgpu::BackendType backend_type() const;
  bool is_vulkan_swiftshader_adapter() const;
  wgpu::Adapter GetAdapter() const;
  wgpu::Instance GetInstance() const;
  webgpu::DawnPlatform* GetDawnPlatform();

  // Sets the caching interface. This must be called before graphite context
  // is created and before device is shared with any other threads.
  void SetCachingInterface(
      std::unique_ptr<webgpu::DawnCachingInterface> dawn_caching_interface);
  void SetCachingInterface(scoped_refptr<GpuPersistentCache> persistent_cache);

  bool use_thread_safe_shared_context() const;

  void InitializeThreadSafeGraphiteContext(
      const skgpu::graphite::ContextOptions& options,
      GpuProcessShmCount* use_shader_cache_shm_count);

  bool InitializeGraphiteContext(
      const skgpu::graphite::ContextOptions& options,
      GpuProcessShmCount* use_shader_cache_shm_count);

  GraphiteSharedContext* GetGraphiteSharedContext() const;

#if BUILDFLAG(IS_WIN)
  Microsoft::WRL::ComPtr<ID3D11Device> GetD3D11Device() const;
#endif

  bool SupportsFeature(wgpu::FeatureName feature);

  std::optional<error::ContextLostReason> GetResetStatus() const;

 private:
  explicit DawnContextProvider(
      scoped_refptr<DawnSharedContext> dawn_shared_context);

  scoped_refptr<DawnSharedContext> dawn_shared_context_;
  std::unique_ptr<gpu::GraphiteSharedContext> graphite_shared_context_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_DAWN_CONTEXT_PROVIDER_H_
