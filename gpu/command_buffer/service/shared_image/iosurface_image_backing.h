// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_IOSURFACE_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_IOSURFACE_IMAGE_BACKING_H_

#include "base/apple/scoped_nsobject.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_surface.h"

namespace gl {
class ScopedEGLSurfaceIOSurface;
}  // namespace gl

namespace gpu {

// The state associated with an EGL texture representation of an IOSurface.
// This is used by the representations GLTextureIOSurfaceRepresentation and
// SkiaIOSurfaceRepresentation (when the underlying GrContext uses GL).
struct IOSurfaceBackingEGLState : base::RefCounted<IOSurfaceBackingEGLState> {
  // The interface through which IOSurfaceBackingEGLState calls into
  // IOSurfaceImageBacking.
  class Client {
   public:
    virtual bool IOSurfaceBackingEGLStateBeginAccess(
        IOSurfaceBackingEGLState* egl_state,
        bool readonly) = 0;
    virtual void IOSurfaceBackingEGLStateEndAccess(
        IOSurfaceBackingEGLState* egl_state,
        bool readonly) = 0;
    virtual void IOSurfaceBackingEGLStateBeingCreated(
        IOSurfaceBackingEGLState* egl_state) = 0;
    virtual void IOSurfaceBackingEGLStateBeingDestroyed(
        IOSurfaceBackingEGLState* egl_state,
        bool have_context) = 0;
  };

  IOSurfaceBackingEGLState(
      Client* client,
      EGLDisplay egl_display,
      gl::GLContext* gl_context,
      gl::GLSurface* gl_surface,
      GLuint gl_target,
      std::vector<scoped_refptr<gles2::TexturePassthrough>> gl_textures);
  GLenum GetGLTarget() const { return gl_target_; }
  GLuint GetGLServiceId(int plane_index) const;
  const scoped_refptr<gles2::TexturePassthrough>& GetGLTexture(
      int plane_index) const {
    return gl_textures_[plane_index];
  }
  bool BeginAccess(bool readonly);
  void EndAccess(bool readonly);
  void WillRelease(bool have_context);

  // Returns true if we need to (re)bind IOSurface to GLTexture before next
  // access.
  bool is_bind_pending() const { return is_bind_pending_; }
  void set_bind_pending() { is_bind_pending_ = true; }
  void clear_bind_pending() { is_bind_pending_ = false; }

 private:
  friend class base::RefCounted<IOSurfaceBackingEGLState>;

  // This class was cleaved off of state from IOSurfaceImageBacking, and so
  // IOSurfaceImageBacking still accesses its internals.
  friend class IOSurfaceImageBacking;

  // The interface through which to call into IOSurfaceImageBacking.
  const raw_ptr<Client> client_;

  // The display for this GL representation.
  const EGLDisplay egl_display_;

  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLSurface> surface_;

  // The GL (not EGL) target to which this texture is to be bound.
  const GLuint gl_target_;

  // The EGL and GLES internals for this IOSurface.
  std::vector<std::unique_ptr<gl::ScopedEGLSurfaceIOSurface>> egl_surfaces_;
  std::vector<scoped_refptr<gles2::TexturePassthrough>> gl_textures_;

  // Set to true if the context is known to be lost.
  bool context_lost_ = false;

  bool is_bind_pending_ = false;

  ~IOSurfaceBackingEGLState();
};

// Representation of an IOSurfaceImageBacking as a GL TexturePassthrough.
class GLTextureIOSurfaceRepresentation
    : public GLTexturePassthroughImageRepresentation {
 public:
  GLTextureIOSurfaceRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      scoped_refptr<IOSurfaceBackingEGLState> egl_state,
      MemoryTypeTracker* tracker);
  ~GLTextureIOSurfaceRepresentation() override;

 private:
  // GLTexturePassthroughImageRepresentation:
  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      int plane_index) override;
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

  scoped_refptr<IOSurfaceBackingEGLState> egl_state_;
  GLenum mode_ = 0;
};

class SkiaIOSurfaceRepresentation : public SkiaGaneshImageRepresentation {
 public:
  SkiaIOSurfaceRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      scoped_refptr<IOSurfaceBackingEGLState> egl_state,
      scoped_refptr<SharedContextState> context_state,
      std::vector<sk_sp<GrPromiseImageTexture>> promise_textures,
      MemoryTypeTracker* tracker);
  ~SkiaIOSurfaceRepresentation() override;

  void SetBeginReadAccessCallback(
      base::RepeatingClosure begin_read_access_callback);

 private:
  // SkiaGaneshImageRepresentation:
  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override;
  std::vector<sk_sp<GrPromiseImageTexture>> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphore,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override;
  void EndWriteAccess() override;
  std::vector<sk_sp<GrPromiseImageTexture>> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override;
  void EndReadAccess() override;
  bool SupportsMultipleConcurrentReadAccess() override;

  void CheckContext();

  scoped_refptr<IOSurfaceBackingEGLState> egl_state_;
  scoped_refptr<SharedContextState> context_state_;
  std::vector<sk_sp<GrPromiseImageTexture>> promise_textures_;
  std::vector<sk_sp<SkSurface>> write_surfaces_;
#if DCHECK_IS_ON()
  raw_ptr<gl::GLContext> context_ = nullptr;
#endif
};

// Overlay representation for a IOSurfaceImageBacking.
class OverlayIOSurfaceRepresentation : public OverlayImageRepresentation {
 public:
  OverlayIOSurfaceRepresentation(SharedImageManager* manager,
                                 SharedImageBacking* backing,
                                 MemoryTypeTracker* tracker,
                                 gfx::ScopedIOSurface io_surface);
  ~OverlayIOSurfaceRepresentation() override;

 private:
  bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) override;
  void EndReadAccess(gfx::GpuFenceHandle release_fence) override;
  gfx::ScopedIOSurface GetIOSurface() const override;
  bool IsInUseByWindowServer() const override;

  gfx::ScopedIOSurface io_surface_;
};

#if BUILDFLAG(USE_DAWN)
// Representation of a IOSurfaceImageBacking as a Dawn Texture.
class DawnIOSurfaceRepresentation : public DawnImageRepresentation {
 public:
  DawnIOSurfaceRepresentation(SharedImageManager* manager,
                              SharedImageBacking* backing,
                              MemoryTypeTracker* tracker,
                              wgpu::Device device,
                              gfx::ScopedIOSurface io_surface,
                              const gfx::Size& io_surface_size,
                              wgpu::TextureFormat wgpu_format,
                              std::vector<wgpu::TextureFormat> view_formats);
  ~DawnIOSurfaceRepresentation() override;

  wgpu::Texture BeginAccess(wgpu::TextureUsage usage) final;
  void EndAccess() final;

 private:
  const wgpu::Device device_;
  const gfx::ScopedIOSurface io_surface_;
  const gfx::Size io_surface_size_;
  const wgpu::TextureFormat wgpu_format_;
  const std::vector<wgpu::TextureFormat> view_formats_;
  wgpu::Texture texture_;
};
#endif  // BUILDFLAG(USE_DAWN)

// This class is only put into unique_ptrs and is never copied or assigned.
class SharedEventAndSignalValue : public BackpressureMetalSharedEvent {
 public:
  SharedEventAndSignalValue(id<MTLSharedEvent> shared_event,
                            uint64_t signaled_value);
  ~SharedEventAndSignalValue() override;
  SharedEventAndSignalValue(const SharedEventAndSignalValue& other) = delete;
  SharedEventAndSignalValue(SharedEventAndSignalValue&& other) = delete;
  SharedEventAndSignalValue& operator=(const SharedEventAndSignalValue& other) =
      delete;

  bool HasCompleted() const override;

  id<MTLSharedEvent> shared_event() const { return shared_event_; }

  // This is the value which will be signaled on the associated MTLSharedEvent.
  uint64_t signaled_value() const { return signaled_value_; }

 private:
  base::apple::scoped_nsprotocol<id<MTLSharedEvent>> shared_event_;
  uint64_t signaled_value_;
};

class GPU_GLES2_EXPORT IOSurfaceImageBacking
    : public SharedImageBacking,
      public IOSurfaceBackingEGLState::Client {
 public:
  IOSurfaceImageBacking(
      gfx::ScopedIOSurface io_surface,
      uint32_t io_surface_plane,
      gfx::GenericSharedMemoryId io_surface_id,
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      GLenum gl_target,
      bool framebuffer_attachment_angle,
      bool is_cleared,
      bool retain_gl_texture,
      absl::optional<gfx::BufferUsage> buffer_usage = absl::nullopt);
  IOSurfaceImageBacking(const IOSurfaceImageBacking& other) = delete;
  IOSurfaceImageBacking& operator=(const IOSurfaceImageBacking& other) = delete;
  ~IOSurfaceImageBacking() override;

  bool UploadFromMemory(const std::vector<SkPixmap>& pixmaps) override;
  bool ReadbackToMemory(const std::vector<SkPixmap>& pixmaps) override;

  bool InitializePixels(base::span<const uint8_t> pixel_data);

  std::unique_ptr<gfx::GpuFence> GetLastWriteGpuFence();
  void SetReleaseFence(gfx::GpuFenceHandle release_fence);

  void AddSharedEventAndSignalValue(id<MTLSharedEvent> sharedEvent,
                                    uint64_t signalValue);
  std::vector<std::unique_ptr<SharedEventAndSignalValue>> TakeSharedEvents();

 private:
  class SkiaGraphiteIOSurfaceRepresentation;

  // SharedImageBacking:
  base::trace_event::MemoryAllocatorDump* OnMemoryDump(
      const std::string& dump_name,
      base::trace_event::MemoryAllocatorDumpGuid client_guid,
      base::trace_event::ProcessMemoryDump* pmd,
      uint64_t client_tracing_id) override;
  SharedImageBackingType GetType() const override;
  gfx::Rect ClearedRect() const final;
  void SetClearedRect(const gfx::Rect& cleared_rect) final;
  std::unique_ptr<GLTextureImageRepresentation> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) final;
  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) final;
  std::unique_ptr<OverlayImageRepresentation> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) final;
  std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      const wgpu::Device& device,
      wgpu::BackendType backend_type,
      std::vector<wgpu::TextureFormat> view_formats) final;
  std::unique_ptr<SkiaGaneshImageRepresentation> ProduceSkiaGanesh(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;
  std::unique_ptr<SkiaGraphiteImageRepresentation> ProduceSkiaGraphite(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;
  void SetPurgeable(bool purgeable) override;
  bool IsPurgeable() const override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle() override;

  // IOSurfaceBackingEGLState::Client:
  bool IOSurfaceBackingEGLStateBeginAccess(IOSurfaceBackingEGLState* egl_state,
                                           bool readonly) override;
  void IOSurfaceBackingEGLStateEndAccess(IOSurfaceBackingEGLState* egl_state,
                                         bool readonly) override;
  void IOSurfaceBackingEGLStateBeingCreated(
      IOSurfaceBackingEGLState* egl_state) override;
  void IOSurfaceBackingEGLStateBeingDestroyed(
      IOSurfaceBackingEGLState* egl_state,
      bool have_context) override;

  // Updates the read and write accesses tracker variables on BeginAccess and
  // waits on `release_fence_` if fence is not null.
  bool HandleBeginAccessSync(bool readonly);
  // Updates the read and write accesses tracker variables on EndAccess.
  void HandleEndAccessSync(bool readonly);

  bool IsPassthrough() const { return true; }

  const gfx::ScopedIOSurface io_surface_;
  const uint32_t io_surface_plane_;
  const gfx::Size io_surface_size_;
  const uint32_t io_surface_format_;
  const size_t io_surface_num_planes_;
  const gfx::GenericSharedMemoryId io_surface_id_;

  const GLenum gl_target_;
  const bool framebuffer_attachment_angle_;

  // Used to determine whether to release the texture in EndAccess() in use
  // cases that need to ensure IOSurface synchronization.
  uint num_ongoing_read_accesses_ = 0;
  // Used with the above variable to catch cases where clients are performing
  // disallowed concurrent read/write accesses.
  bool ongoing_write_access_ = false;

  scoped_refptr<IOSurfaceBackingEGLState> RetainGLTexture();
  void ReleaseGLTexture(IOSurfaceBackingEGLState* egl_state, bool have_context);

  // This is the cleared rect used by ClearedRect and SetClearedRect when
  // |texture_| is nullptr.
  gfx::Rect cleared_rect_;

  // Whether or not the surface is currently purgeable.
  bool purgeable_ = false;

  // This map tracks all IOSurfaceBackingEGLState instances that exist.
  std::map<EGLDisplay, IOSurfaceBackingEGLState*> egl_state_map_;
  scoped_refptr<IOSurfaceBackingEGLState> egl_state_for_legacy_mailbox_;

  std::unique_ptr<gl::GLFence> last_write_gl_fence_;

  // If this backing was displayed as an overlay, this fence may be set.
  // Wait on this fence before allowing another access.
  gfx::GpuFenceHandle release_fence_;

  std::vector<std::unique_ptr<SharedEventAndSignalValue>>
      shared_events_and_signal_values_;

  base::WeakPtrFactory<IOSurfaceImageBacking> weak_factory_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_IOSURFACE_IMAGE_BACKING_H_
