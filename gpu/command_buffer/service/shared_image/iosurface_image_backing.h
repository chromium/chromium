// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_IOSURFACE_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_IOSURFACE_IMAGE_BACKING_H_

#include "base/apple/scoped_nsobject.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/shared_image/dawn_shared_texture_cache.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_surface.h"

@protocol MTLDevice;

namespace gl {
class GLFenceEGL;
class ScopedEGLSurfaceIOSurface;
}  // namespace gl

namespace gpu {
class WebNNTensorRepresentation;

// Representation of a IOSurfaceImageBacking as a tensor.
class WebNNIOSurfaceTensorRepresentation : public WebNNTensorRepresentation {
 public:
  WebNNIOSurfaceTensorRepresentation(SharedImageManager* manager,
                                     SharedImageBacking* backing,
                                     MemoryTypeTracker* tracker);
  ~WebNNIOSurfaceTensorRepresentation() override;

 private:
  IOSurfaceRef GetIOSurface() const override;
  bool BeginAccess() override;
  void EndAccess() override;
};

// The state associated with an EGL texture representation of an IOSurface.
// This is used by the representations GLTextureIRepresentation and
// SkiaGaneshRepresentation (when the underlying GrContext uses GL).
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
  void RemoveClient();
  bool BelongsToCurrentThread() const;
  base::SingleThreadTaskRunner* created_task_runner() {
    return created_task_runner_.get();
  }

 private:
  friend class base::RefCounted<IOSurfaceBackingEGLState>;

  // This class was cleaved off of state from IOSurfaceImageBacking, and so
  // IOSurfaceImageBacking still accesses its internals.
  friend class IOSurfaceImageBacking;

  // The interface through which to call into IOSurfaceImageBacking.
  raw_ptr<Client> client_;

  // The display for this GL representation.
  const EGLDisplay egl_display_;

  const scoped_refptr<gl::GLContext> context_;
  const scoped_refptr<gl::GLSurface> surface_;

  // The GL (not EGL) target to which this texture is to be bound.
  const GLuint gl_target_;

  // The EGL and GLES internals for this IOSurface.
  std::vector<std::unique_ptr<gl::ScopedEGLSurfaceIOSurface>> egl_surfaces_;
  std::vector<scoped_refptr<gles2::TexturePassthrough>> gl_textures_;

  // Set to true if the context is known to be lost.
  bool context_lost_ = false;

  bool is_bind_pending_ = false;

  int num_ongoing_accesses_ = 0;

  scoped_refptr<base::SingleThreadTaskRunner> created_task_runner_;

  ~IOSurfaceBackingEGLState();
};

class GPU_GLES2_EXPORT IOSurfaceImageBacking
    : public ClearTrackingSharedImageBacking,
      public IOSurfaceBackingEGLState::Client {
 public:
  IOSurfaceImageBacking(
      gfx::ScopedIOSurface io_surface,
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      gpu::SharedImageUsageSet usage,
      std::string debug_label,
      GLenum gl_target,
      bool framebuffer_attachment_angle,
      bool is_cleared,
      bool is_thread_safe,
      GrContextType gr_context_type,
      std::optional<gfx::BufferUsage> buffer_usage = std::nullopt);
  IOSurfaceImageBacking(const IOSurfaceImageBacking& other) = delete;
  IOSurfaceImageBacking& operator=(const IOSurfaceImageBacking& other) = delete;
  ~IOSurfaceImageBacking() override;

  bool UploadFromMemory(const std::vector<SkPixmap>& pixmaps) override;
  bool ReadbackToMemory(const std::vector<SkPixmap>& pixmaps) override;

  bool InitializePixels(base::span<const uint8_t> pixel_data);

  // Wait for commands to be scheduled on every WGPUDevice or EGLDisplay that's
  // pending a flush except those using the same MTLDevice as `waiting_device`.
  // This is needed in two cases: 1) handing off the IOSurface to CoreAnimation
  // since there's no other synchronization mechanism, and 2) accessing the
  // IOSurface on different GPUs/MTLDevices since there could be shadow copies
  // performed by the kernel.
  void WaitForCommandsToBeScheduled(id<MTLDevice> waiting_device = nil)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  IOSurfaceRef GetIOSurface();

  bool BeginAccessWebNN();
  void EndAccessWebNN();

 private:
  class GLTextureIRepresentation;
  class DawnRepresentation;
  class SkiaGraphiteDawnMetalRepresentation;
  class SkiaGaneshRepresentation;
  class SkiaGraphiteMetalRepresentation;
  class OverlayRepresentation;

  class DawnBufferCopyRepresentation final : public DawnBufferRepresentation {
   public:
    DawnBufferCopyRepresentation(
        SharedImageManager* manager,
        SharedImageBacking* backing,
        MemoryTypeTracker* tracker,
        const wgpu::Device& device,
        std::unique_ptr<DawnImageRepresentation> dawn_image_representation);

    ~DawnBufferCopyRepresentation() override;

    wgpu::Buffer BeginAccess(wgpu::BufferUsage usage) override;

    void EndAccess() override;

   private:
    wgpu::Device device_;
    std::unique_ptr<DawnImageRepresentation> dawn_image_representation_;
    wgpu::Buffer buffer_;
  };

  // SharedImageBacking:
  base::trace_event::MemoryAllocatorDump* OnMemoryDump(
      const std::string& dump_name,
      base::trace_event::MemoryAllocatorDumpGuid client_guid,
      base::trace_event::ProcessMemoryDump* pmd,
      uint64_t client_tracing_id) override;
  SharedImageBackingType GetType() const override;

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
      std::vector<wgpu::TextureFormat> view_formats,
      scoped_refptr<SharedContextState> context_state) final;
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
  std::unique_ptr<WebNNTensorRepresentation> ProduceWebNNTensor(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;
  std::unique_ptr<DawnBufferRepresentation> ProduceDawnBuffer(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      const wgpu::Device& device,
      wgpu::BackendType backend_type,
      scoped_refptr<SharedContextState> context_state) override;

  // IOSurfaceBackingEGLState::Client:
  bool IOSurfaceBackingEGLStateBeginAccess(IOSurfaceBackingEGLState* egl_state,
                                           bool readonly)
      EXCLUSIVE_LOCKS_REQUIRED(lock_) override;
  void IOSurfaceBackingEGLStateEndAccess(IOSurfaceBackingEGLState* egl_state,
                                         bool readonly)
      EXCLUSIVE_LOCKS_REQUIRED(lock_) override;
  void IOSurfaceBackingEGLStateBeingCreated(IOSurfaceBackingEGLState* egl_state)
      EXCLUSIVE_LOCKS_REQUIRED(lock_) override;
  void IOSurfaceBackingEGLStateBeingDestroyed(
      IOSurfaceBackingEGLState* egl_state,
      bool have_context) EXCLUSIVE_LOCKS_REQUIRED(lock_) override;

  // Updates the read and write accesses tracker variables on BeginAccess.
  bool BeginAccess(bool readonly) EXCLUSIVE_LOCKS_REQUIRED(lock_);
  // Updates the read and write accesses tracker variables on EndAccess.
  void EndAccess(bool readonly) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void AddSharedEventForEndAccess(id<MTLSharedEvent> shared_event,
                                  uint64_t signal_value,
                                  bool readonly)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void ProcessSharedEventsForBeginAccess(
      bool readonly,
      base::FunctionRef<void(id<MTLSharedEvent> shared_event,
                             uint64_t signaled_value)> process_fn)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Guarded by ScopedIOSurfaceLock instead of |lock_| for memory access.
  const gfx::ScopedIOSurface io_surface_;

  const gfx::Size io_surface_size_;
  const uint32_t io_surface_format_;

  // DawnSharedTextureCache that keeps an internal cache of per-device
  // SharedTextureData that vends WebGPU textures for the underlying IOSurface.
  scoped_refptr<DawnSharedTextureCache> dawn_texture_cache_ GUARDED_BY(lock_);

  const scoped_refptr<DawnSharedTextureCache>& GetDawnTextureCache()
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Tracks the number of currently-ongoing accesses to a given WGPU texture.
  base::flat_map<WGPUTexture, int> wgpu_texture_ongoing_accesses_
      GUARDED_BY(lock_);

  // Tracks devices with pending commands scheduled futures.
  // TODO(dawn:2453): The below comparator should be implemented in
  // wgpu::Device itself.
  struct WGPUDeviceCompare {
    bool operator()(const wgpu::Device& lhs, const wgpu::Device& rhs) const {
      return lhs.Get() < rhs.Get();
    }
  };
  base::flat_map<wgpu::Device, wgpu::Future, WGPUDeviceCompare>
      wgpu_commands_scheduled_futures_ GUARDED_BY(lock_);

  // Returns the number of ongoing accesses that were already present on this
  // texture prior to beginning this access.
  int TrackBeginAccessToWGPUTexture(wgpu::Texture texture)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Returns the number of ongoing accesses that will still be present on this
  // texture after ending this access.
  int TrackEndAccessToWGPUTexture(wgpu::Texture texture)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  const GLenum gl_target_;
  const bool framebuffer_attachment_angle_;

  // Used to determine whether to release the texture in EndAccess() in use
  // cases that need to ensure IOSurface synchronization.
  int num_ongoing_read_accesses_ GUARDED_BY(lock_) = 0;
  // Used with the above variable to catch cases where clients are performing
  // disallowed concurrent read/write accesses.
  bool ongoing_write_access_ GUARDED_BY(lock_) = false;

  scoped_refptr<IOSurfaceBackingEGLState> RetainGLTexture();
  void ReleaseGLTexture(IOSurfaceBackingEGLState* egl_state, bool have_context)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Whether or not the surface is currently purgeable.
  bool purgeable_ GUARDED_BY(lock_) = false;

  // This map tracks all IOSurfaceBackingEGLState instances that exist.
  base::flat_map<std::pair<EGLDisplay, base::SingleThreadTaskRunner*>,
                 IOSurfaceBackingEGLState*>
      egl_state_map_ GUARDED_BY(lock_);

  // If Skia is using GL, this object creates a GL texture at construction time
  // for the Skia GL context and reuses it (for that context) for its lifetime.
  // This egl_state is set in IOSurfaceImageBacking Ctor only.
  scoped_refptr<IOSurfaceBackingEGLState> egl_state_for_skia_gl_context_
      GUARDED_BY(lock_);

  // Tracks displays with pending commands scheduled fences.
  base::flat_map<EGLDisplay, std::unique_ptr<gl::GLFenceEGL>>
      egl_commands_scheduled_fences_ GUARDED_BY(lock_);

  using ScopedSharedEvent = base::apple::scoped_nsprotocol<id<MTLSharedEvent>>;
  struct SharedEventCompare {
    bool operator()(const ScopedSharedEvent& lhs,
                    const ScopedSharedEvent& rhs) const {
      return lhs.get() < rhs.get();
    }
  };
  using SharedEventMap =
      base::flat_map<ScopedSharedEvent, uint64_t, SharedEventCompare>;
  // Shared events and signals for exclusive accesses.
  SharedEventMap exclusive_shared_events_ GUARDED_BY(lock_);
  // Shared events and signals for non-exclusive accesses.
  SharedEventMap non_exclusive_shared_events_ GUARDED_BY(lock_);

  base::WeakPtrFactory<IOSurfaceImageBacking> weak_factory_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_IOSURFACE_IMAGE_BACKING_H_
