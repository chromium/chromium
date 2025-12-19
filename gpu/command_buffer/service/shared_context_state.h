// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_CONTEXT_STATE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_CONTEXT_STATE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/lru_cache.h"
#include "base/gtest_prod_util.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gl2_types.h"
#include "gpu/command_buffer/common/skia_utils.h"
#include "gpu/command_buffer/service/gl_context_virtual_delegate.h"
#include "gpu/command_buffer/service/gpu_persistent_cache.h"
#include "gpu/command_buffer/service/gr_shader_cache.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_peak_memory.h"
#include "gpu/vulkan/buildflags.h"
#include "skia/buildflags.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "ui/gl/progress_reporter.h"

#if BUILDFLAG(IS_WIN)
#include <d3d11.h>
#include <wrl/client.h>
#endif

namespace gl {
class GLContext;
class GLDisplay;
class GLShareGroup;
class GLSurface;
}  // namespace gl

namespace viz {
class MetalContextProvider;
class VulkanContextProvider;
}  // namespace viz

namespace skgpu::graphite {
class PrecompileContext;
class Recorder;
}  // namespace skgpu::graphite

namespace gpu {
class DawnContextProvider;
class ExternalSemaphorePool;
class GpuDriverBugWorkarounds;
class GpuProcessShmCount;
class ServiceTransferCache;
class GraphiteSharedContext;

namespace gles2 {
class FeatureInfo;
struct ContextState;
}  // namespace gles2

namespace raster {
class GrCacheController;
class GrShaderCache;
class GraphiteCacheController;
class RasterDecoderTestBase;
}  // namespace raster

class GPU_GLES2_EXPORT SharedContextState
    : public base::trace_event::MemoryDumpProvider,
      public gpu::GLContextVirtualDelegate,
      public base::RefCounted<SharedContextState>,
      public GrContextOptions::ShaderErrorHandler {
 public:
  using ContextLostCallback =
      base::OnceCallback<void(bool, error::ContextLostReason)>;

  // This interface is used by the embedder to set custom GrContextOptions which
  // are passed to Skia.
  class GrContextOptionsProvider {
   public:
    ~GrContextOptionsProvider() = default;
    // The passed GrContextOptions will have the default fields set. The
    // embedder may modify the options as needed.
    virtual void SetCustomGrContextOptions(GrContextOptions& options) const = 0;
  };

  // TODO(vikassoni): Refactor code to have seperate constructor for GL and
  // Vulkan and not initialize/use GL related info for vulkan and vice-versa.
  SharedContextState(
      scoped_refptr<gl::GLShareGroup> share_group,
      scoped_refptr<gl::GLSurface> surface,
      scoped_refptr<gl::GLContext> context,
      bool use_virtualized_gl_contexts,
      ContextLostCallback context_lost_callback,
      GrContextType gr_context_type,
      viz::VulkanContextProvider* vulkan_context_provider = nullptr,
      viz::MetalContextProvider* metal_context_provider = nullptr,
      DawnContextProvider* dawn_context_provider = nullptr,
      scoped_refptr<gpu::MemoryTracker::Observer> peak_memory_monitor = nullptr,
      bool direct_rendering_display_compositor_enabled = false,
      bool created_on_compositor_gpu_thread = false,
      const gpu::SharedContextState::GrContextOptionsProvider*
          gr_context_options_provider = nullptr);

  SharedContextState(const SharedContextState&) = delete;
  SharedContextState& operator=(const SharedContextState&) = delete;

  bool InitializeSkia(
      const GpuPreferences& gpu_preferences,
      const GpuDriverBugWorkarounds& workarounds,
      gpu::raster::GrShaderCache* gr_cache = nullptr,
      scoped_refptr<GpuPersistentCache> persistent_cache = nullptr,
      GpuProcessShmCount* use_shader_cache_shm_count = nullptr,
      gl::ProgressReporter* progress_reporter = nullptr);
  bool GrContextIsGL() const {
    return gr_context_type_ == GrContextType::kGL;
  }
  bool GrContextIsVulkan() const {
    return gr_context_type_ == GrContextType::kVulkan;
  }
  bool IsGraphiteDawn() const;
  bool IsGraphiteMetal() const;
  bool IsGraphiteDawnMetal() const;
  bool IsGraphiteDawnD3D() const;
  bool IsGraphiteDawnD3D11() const;
  bool IsGraphiteDawnVulkan() const;
  bool IsGraphiteDawnVulkanSwiftShader() const;

  bool InitializeGL(const GpuPreferences& gpu_preferences,
                    scoped_refptr<gles2::FeatureInfo> feature_info);
  bool IsGLInitialized() const { return !!feature_info_; }

  void FlushAndSubmit(bool sync_to_cpu);
  void FlushWriteAccess(SkiaImageRepresentation::ScopedWriteAccess* access);
  void SubmitIfNecessary(std::vector<GrBackendSemaphore> signal_semaphores,
                         bool need_graphite_submit);

  // Returns true if context state is using GL, either for Skia to run on
  // or if there is no skia context and context state exists for WebGL fallback
  // only.
  bool IsUsingGL() const;
  bool MakeCurrent(gl::GLSurface* surface, bool needs_gl = false);
  void ReleaseCurrent(gl::GLSurface* surface);
  void MarkContextLost(error::ContextLostReason reason = error::kUnknown);
  bool IsCurrent(gl::GLSurface* surface, bool needs_gl = false);

  void PurgeMemory(base::MemoryPressureLevel memory_pressure_level);

  void UpdateSkiaOwnedMemorySize();
  uint64_t GetMemoryUsage();

  void PessimisticallyResetGrContext() const;

  void StoreVkPipelineCacheIfNeeded();

  void UseShaderCache(
      std::optional<gpu::raster::GrShaderCache::ScopedCacheUse>& cache_use,
      int32_t client_id) const;

  GLFormatCaps GetGLFormatCaps() { return GLFormatCaps(feature_info()); }

  gl::GLShareGroup* share_group() const { return share_group_.get(); }
  gl::GLContext* context() const { return context_.get(); }
  gl::GLContext* real_context() const { return real_context_.get(); }
  gl::GLSurface* surface() const;
  gl::GLDisplay* display();  // non const since it calls GLSurface::GetGLDisplay
  viz::VulkanContextProvider* vk_context_provider() const {
    return vk_context_provider_;
  }
  viz::MetalContextProvider* metal_context_provider() const {
    return metal_context_provider_;
  }
  DawnContextProvider* dawn_context_provider() const {
    return dawn_context_provider_;
  }
  gl::ProgressReporter* progress_reporter() const { return progress_reporter_; }
  // Ganesh/Graphite contexts may only be used on the GPU main thread.
  GrDirectContext* gr_context() const { return gr_context_; }
  gpu::GraphiteSharedContext* graphite_shared_context() const;
  // Graphite recorder for GPU main thread, used by RasterDecoder,
  // SkiaOutputSurfaceImplOnGpu, etc.
  skgpu::graphite::Recorder* gpu_main_graphite_recorder() const {
    return gpu_main_graphite_recorder_.get();
  }
  // Graphite recorder for Viz compositor thread, used by SkiaOutputSurfaceImpl.
  skgpu::graphite::Recorder* viz_compositor_graphite_recorder() const {
    return viz_compositor_graphite_recorder_.get();
  }
  GrContextType gr_context_type() const { return gr_context_type_; }
  // Handles Skia-reported shader compilation errors.
  void compileError(const char* shader,
                    const char* errors,
                    bool shaderWasCached) override;
  gles2::FeatureInfo* feature_info() { return feature_info_.get(); }
  gles2::ContextState* context_state() const { return context_state_.get(); }
  bool context_lost() const { return !!context_lost_reason_; }
  std::optional<error::ContextLostReason> context_lost_reason() {
    return context_lost_reason_;
  }
  bool need_context_state_reset() const { return need_context_state_reset_; }
  void set_need_context_state_reset(bool reset) {
    need_context_state_reset_ = reset;
  }
  ServiceTransferCache* transfer_cache() { return transfer_cache_.get(); }
  std::vector<uint8_t>* scratch_deserialization_buffer() {
    return &scratch_deserialization_buffer_;
  }
  bool use_virtualized_gl_contexts() const {
    return use_virtualized_gl_contexts_;
  }
  bool support_vulkan_external_object() const {
    return support_vulkan_external_object_;
  }
  bool support_gl_external_object_flags() const {
    return support_gl_external_object_flags_;
  }
  bool is_drdc_enabled() const { return is_drdc_enabled_; }

  gpu::MemoryTracker* memory_tracker() { return memory_tracker_.get(); }
  gpu::MemoryTypeTracker* memory_type_tracker() {
    return &memory_type_tracker_;
  }
#if BUILDFLAG(ENABLE_VULKAN) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_WIN))
  ExternalSemaphorePool* external_semaphore_pool() {
    return external_semaphore_pool_.get();
  }
#endif

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // Observer class which is notified when the context is lost.
  class ContextLostObserver {
   public:
    virtual void OnContextLost() = 0;

   protected:
    virtual ~ContextLostObserver() = default;
  };
  void AddContextLostObserver(ContextLostObserver* obs);
  void RemoveContextLostObserver(ContextLostObserver* obs);

  // Creating a SkSurface backed by FBO takes ~500 usec and holds ~50KB of heap
  // on Android circa 2020. Caching them is a memory/CPU tradeoff.
  void CacheSkSurface(void* key, sk_sp<SkSurface> surface) {
    sk_surface_cache_.Put(key, surface);
  }
  sk_sp<SkSurface> GetCachedSkSurface(void* key) {
    auto found = sk_surface_cache_.Get(key);
    if (found == sk_surface_cache_.end())
      return nullptr;
    return found->second;
  }
  void EraseCachedSkSurface(void* key) {
    auto found = sk_surface_cache_.Peek(key);
    if (found != sk_surface_cache_.end())
      sk_surface_cache_.Erase(found);
  }
  // Supports DCHECKs. OK to be approximate.
  bool CachedSkSurfaceIsUnique(void* key) {
    auto found = sk_surface_cache_.Peek(key);
    // It was purged. Assume it was unique.
    if (found == sk_surface_cache_.end())
      return true;
    return found->second->unique();
  }

  // Updates |context_lost_reason| and returns true if lost
  // (e.g. VK_ERROR_DEVICE_LOST or GL_UNKNOWN_CONTEXT_RESET_ARB).
  bool CheckResetStatus(bool needs_gl);
  bool device_needs_reset() { return device_needs_reset_; }

  void ScheduleSkiaCleanup();

  int32_t GetMaxTextureSize();

#if BUILDFLAG(IS_WIN)
  // Get the D3D11 device used for the compositing.
  Microsoft::WRL::ComPtr<ID3D11Device> GetD3D11Device() const;
#endif

 private:
  friend class base::RefCounted<SharedContextState>;
  friend class raster::RasterDecoderTestBase;
  FRIEND_TEST_ALL_PREFIXES(SharedContextStateTest,
                           GLOptionsProviderSetsCustomOptions);
  FRIEND_TEST_ALL_PREFIXES(SharedContextStateTest,
                           VulkanOptionsProviderSetsCustomOptions);

  ~SharedContextState() override;

  bool InitializeGanesh(
      const GpuPreferences& gpu_preferences,
      const GpuDriverBugWorkarounds& workarounds,
      gpu::raster::GrShaderCache* gr_cache = nullptr,
      scoped_refptr<GpuPersistentCache> persistent_cache = nullptr,
      GpuProcessShmCount* use_shader_cache_shm_count = nullptr,
      gl::ProgressReporter* progress_reporter = nullptr);

  bool InitializeGraphite(const GpuPreferences& gpu_preferences,
                          const GpuDriverBugWorkarounds& workarounds,
                          GpuProcessShmCount* use_shader_cache_shm_count);

  void FlushGraphiteRecorder();

  std::optional<error::ContextLostReason> GetResetStatus(bool needs_gl);

  // gpu::GLContextVirtualDelegate implementation.
  bool initialized() const override;
  const gles2::ContextState* GetContextState() override;
  void RestoreState(const gles2::ContextState* prev_state) override;
  void RestoreGlobalState() const override;
  void ClearAllAttributes() const override;
  void RestoreActiveTexture() const override;
  void RestoreAllTextureUnitAndSamplerBindings(
      const gles2::ContextState* prev_state) const override;
  void RestoreActiveTextureUnitBinding(unsigned int target) const override;
  void RestoreBufferBinding(unsigned int target) override;
  void RestoreBufferBindings() const override;
  void RestoreFramebufferBindings() const override;
  void RestoreRenderbufferBindings() override;
  void RestoreProgramBindings() const override;
  void RestoreTextureUnitBindings(unsigned unit) const override;
  void RestoreVertexAttribArray(unsigned index) override;
  void RestoreAllExternalTextureBindingsIfNeeded() override;
  QueryManager* GetQueryManager() override;

  bool use_virtualized_gl_contexts_ = false;
  bool support_vulkan_external_object_ = false;
  bool support_gl_external_object_flags_ = false;
  ContextLostCallback context_lost_callback_;
  const GrContextType gr_context_type_;
  scoped_refptr<MemoryTracker> memory_tracker_shared_context_state_;
  scoped_refptr<MemoryTracker> memory_tracker_;
  gpu::MemoryTypeTracker memory_type_tracker_;
  const raw_ptr<viz::VulkanContextProvider> vk_context_provider_ = nullptr;
  const raw_ptr<viz::MetalContextProvider> metal_context_provider_ = nullptr;
  const raw_ptr<DawnContextProvider> dawn_context_provider_ = nullptr;
  raw_ptr<const GrContextOptionsProvider> gr_context_options_provider_ =
      nullptr;
  bool created_on_compositor_gpu_thread_ = false;
  bool is_drdc_enabled_ = false;
  raw_ptr<GrDirectContext> gr_context_ = nullptr;
  std::unique_ptr<skgpu::graphite::Recorder> gpu_main_graphite_recorder_;
  std::unique_ptr<skgpu::graphite::Recorder> viz_compositor_graphite_recorder_;

  // These two are only used if Precompilation is enabled
  std::unique_ptr<skgpu::graphite::PrecompileContext> precompile_context_;
  base::RepeatingTimer pipeline_cache_stats_timer_;

  scoped_refptr<gl::GLShareGroup> share_group_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLContext> real_context_;

  // Most recent surface that this ShareContextState was made current with.
  // Avoids a call to MakeCurrent with a different surface, if we don't
  // care which surface is current.
  raw_ptr<gl::GLSurface, DanglingUntriaged> last_current_surface_ = nullptr;

  scoped_refptr<gles2::FeatureInfo> feature_info_;

  // raster decoders and display compositor share this context_state_.
  std::unique_ptr<gles2::ContextState> context_state_;

  raw_ptr<gl::ProgressReporter, DanglingUntriaged> progress_reporter_ = nullptr;
  sk_sp<GrDirectContext> owned_gr_context_;
  std::unique_ptr<ServiceTransferCache> transfer_cache_;
  std::vector<uint8_t> scratch_deserialization_buffer_;
  raw_ptr<gpu::raster::GrShaderCache, DanglingUntriaged> gr_shader_cache_ =
      nullptr;
  scoped_refptr<GpuPersistentCache> persistent_cache_ = nullptr;
  raw_ptr<GpuProcessShmCount, DanglingUntriaged> use_shader_cache_shm_count_ =
      nullptr;

  // |need_context_state_reset| is set whenever Skia may have altered the
  // driver's GL state.
  bool need_context_state_reset_ = false;

  std::optional<error::ContextLostReason> context_lost_reason_;
  base::ObserverList<ContextLostObserver>::Unchecked context_lost_observers_;

  base::LRUCache<void*, sk_sp<SkSurface>> sk_surface_cache_;

  bool device_needs_reset_ = false;
  base::Time last_gl_check_graphics_reset_status_;
  bool disable_check_reset_status_throttling_for_test_ = false;

#if BUILDFLAG(ENABLE_VULKAN) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_WIN))
  std::unique_ptr<ExternalSemaphorePool> external_semaphore_pool_;
#endif

  std::unique_ptr<raster::GrCacheController> gr_cache_controller_;

  // The graphite cache controller for |graphite_context_| and
  // |gpu_main_graphite_recorder_|.
  scoped_refptr<raster::GraphiteCacheController>
      gpu_main_graphite_cache_controller_;

  std::optional<int> max_texture_size_;

  base::WeakPtrFactory<SharedContextState> weak_ptr_factory_{this};
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_CONTEXT_STATE_H_
