// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_CONTEXT_STATE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_CONTEXT_STATE_H_

#include <memory>
#include <vector>

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/skia_utils.h"
#include "gpu/command_buffer/service/gl_context_virtual_delegate.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gl/progress_reporter.h"

namespace gl {
class GLContext;
class GLShareGroup;
class GLSurface;
}  // namespace gl

namespace viz {
class DawnContextProvider;
class MetalContextProvider;
class VulkanContextProvider;
}  // namespace viz

namespace gpu {
class GpuDriverBugWorkarounds;
class GpuProcessActivityFlags;
class ServiceTransferCache;

namespace gles2 {
class FeatureInfo;
struct ContextState;
}  // namespace gles2

class GPU_GLES2_EXPORT SharedContextState
    : public base::trace_event::MemoryDumpProvider,
      public gpu::GLContextVirtualDelegate,
      public base::RefCounted<SharedContextState>,
      public GrContextOptions::ShaderErrorHandler {
 public:
  // TODO: Refactor code to have seperate constructor for GL and Vulkan and not
  // initialize/use GL related info for vulkan and vice-versa.
  SharedContextState(
      scoped_refptr<gl::GLShareGroup> share_group,
      scoped_refptr<gl::GLSurface> surface,
      scoped_refptr<gl::GLContext> context,
      bool use_virtualized_gl_contexts,
      base::OnceClosure context_lost_callback,
      GrContextType gr_context_type = GrContextType::kGL,
      viz::VulkanContextProvider* vulkan_context_provider = nullptr,
      viz::MetalContextProvider* metal_context_provider = nullptr,
      viz::DawnContextProvider* dawn_context_provider = nullptr);

  void InitializeGrContext(const GpuDriverBugWorkarounds& workarounds,
                           GrContextOptions::PersistentCache* cache,
                           GpuProcessActivityFlags* activity_flags = nullptr,
                           gl::ProgressReporter* progress_reporter = nullptr);
  bool GrContextIsGL() const {
    return gr_context_type_ == GrContextType::kGL;
  }
  bool GrContextIsVulkan() const {
    return vk_context_provider_ && gr_context_type_ == GrContextType::kVulkan;
  }
  bool GrContextIsMetal() const {
    return metal_context_provider_ && gr_context_type_ == GrContextType::kMetal;
  }
  bool GrContextIsDawn() const {
    return dawn_context_provider_ && gr_context_type_ == GrContextType::kDawn;
  }

  bool InitializeGL(const GpuPreferences& gpu_preferences,
                    scoped_refptr<gles2::FeatureInfo> feature_info);
  bool IsGLInitialized() const { return !!feature_info_; }

  bool MakeCurrent(gl::GLSurface* surface, bool needs_gl = false);
  void MarkContextLost();
  bool IsCurrent(gl::GLSurface* surface);

  void PurgeMemory(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  void PessimisticallyResetGrContext() const;

  gl::GLShareGroup* share_group() { return share_group_.get(); }
  gl::GLContext* context() { return context_.get(); }
  gl::GLContext* real_context() { return real_context_.get(); }
  gl::GLSurface* surface() { return surface_.get(); }
  viz::VulkanContextProvider* vk_context_provider() {
    return vk_context_provider_;
  }
  viz::MetalContextProvider* metal_context_provider() {
    return metal_context_provider_;
  }
  viz::DawnContextProvider* dawn_context_provider() {
    return dawn_context_provider_;
  }
  gl::ProgressReporter* progress_reporter() const { return progress_reporter_; }
  GrContext* gr_context() { return gr_context_; }
  // Handles Skia-reported shader compilation errors.
  void compileError(const char* shader, const char* errors) override;
  gles2::FeatureInfo* feature_info() { return feature_info_.get(); }
  gles2::ContextState* context_state() const { return context_state_.get(); }
  bool context_lost() const { return context_lost_; }
  bool need_context_state_reset() const { return need_context_state_reset_; }
  void set_need_context_state_reset(bool reset) {
    need_context_state_reset_ = reset;
  }
  ServiceTransferCache* transfer_cache() { return transfer_cache_.get(); }
  std::vector<uint8_t>* scratch_deserialization_buffer() {
    return &scratch_deserialization_buffer_;
  }
  size_t max_resource_cache_bytes() const { return max_resource_cache_bytes_; }
  size_t glyph_cache_max_texture_bytes() const {
    return glyph_cache_max_texture_bytes_;
  }
  bool use_virtualized_gl_contexts() const {
    return use_virtualized_gl_contexts_;
  }
  bool support_vulkan_external_object() const {
    return support_vulkan_external_object_;
  }

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // Observer class which is notified when the context is lost.
  class ContextLostObserver {
   public:
    virtual void OnContextLost() = 0;

   protected:
    virtual ~ContextLostObserver() {}
  };
  void AddContextLostObserver(ContextLostObserver* obs);
  void RemoveContextLostObserver(ContextLostObserver* obs);

 private:
  friend class base::RefCounted<SharedContextState>;

  ~SharedContextState() override;

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
  base::OnceClosure context_lost_callback_;
  GrContextType gr_context_type_ = GrContextType::kGL;
  viz::VulkanContextProvider* const vk_context_provider_;
  viz::MetalContextProvider* const metal_context_provider_;
  viz::DawnContextProvider* const dawn_context_provider_;
  GrContext* gr_context_ = nullptr;

  scoped_refptr<gl::GLShareGroup> share_group_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLContext> real_context_;
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gles2::FeatureInfo> feature_info_;

  // raster decoders and display compositor share this context_state_.
  std::unique_ptr<gles2::ContextState> context_state_;

  gl::ProgressReporter* progress_reporter_ = nullptr;
  sk_sp<GrContext> owned_gr_context_;
  std::unique_ptr<ServiceTransferCache> transfer_cache_;
  size_t max_resource_cache_bytes_ = 0u;
  size_t glyph_cache_max_texture_bytes_ = 0u;
  std::vector<uint8_t> scratch_deserialization_buffer_;

  // |need_context_state_reset| is set whenever Skia may have altered the
  // driver's GL state.
  bool need_context_state_reset_ = false;

  bool context_lost_ = false;
  base::ObserverList<ContextLostObserver>::Unchecked context_lost_observers_;

  base::WeakPtrFactory<SharedContextState> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SharedContextState);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_CONTEXT_STATE_H_
