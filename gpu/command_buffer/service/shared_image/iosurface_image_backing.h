// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_IOSURFACE_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_IOSURFACE_IMAGE_BACKING_H_

#include "base/apple/scoped_nsobject.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/shared_image/dawn_shared_texture_holder.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_preferences.h"
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

class GPU_GLES2_EXPORT IOSurfaceImageBacking
    : public SharedImageBacking,
      public IOSurfaceBackingEGLState::Client {
 public:
  IOSurfaceImageBacking(
      gfx::ScopedIOSurface io_surface,
      gfx::GenericSharedMemoryId io_surface_id,
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
      GrContextType gr_context_type,
      std::optional<gfx::BufferUsage> buffer_usage = std::nullopt);
  IOSurfaceImageBacking(const IOSurfaceImageBacking& other) = delete;
  IOSurfaceImageBacking& operator=(const IOSurfaceImageBacking& other) = delete;
  ~IOSurfaceImageBacking() override;

  bool UploadFromMemory(const std::vector<SkPixmap>& pixmaps) override;
  bool ReadbackToMemory(const std::vector<SkPixmap>& pixmaps) override;

  bool InitializePixels(base::span<const uint8_t> pixel_data);

  void AddWGPUDeviceWithPendingCommands(wgpu::Device device);
  void WaitForDawnCommandsToBeScheduled(const wgpu::Device& device_to_exclude);

  void AddEGLDisplayWithPendingCommands(gl::GLDisplayEGL* display);
  void WaitForANGLECommandsToBeScheduled();
  void ClearEGLDisplaysWithPendingCommands(gl::GLDisplayEGL* display_to_keep);

  std::unique_ptr<gfx::GpuFence> GetLastWriteGpuFence();
  void SetReleaseFence(gfx::GpuFenceHandle release_fence);

 private:
  class GLTextureIRepresentation;
  class DawnRepresentation;
  class SkiaGaneshRepresentation;
  class SkiaGraphiteRepresentation;
  class OverlayRepresentation;

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

  // Updates the read and write accesses tracker variables on BeginAccess.
  bool BeginAccess(bool readonly);
  // Updates the read and write accesses tracker variables on EndAccess.
  void EndAccess(bool readonly);

  void AddSharedEventForEndAccess(id<MTLSharedEvent> shared_event,
                                  uint64_t signal_value,
                                  bool readonly);
  template <typename Fn>
  void ProcessSharedEventsForBeginAccess(bool readonly, const Fn& fn);

  const gfx::ScopedIOSurface io_surface_;
  const gfx::Size io_surface_size_;
  const uint32_t io_surface_format_;
  const gfx::GenericSharedMemoryId io_surface_id_;

  // DawnSharedTextureHolder that keeps an internal cache of per-device
  // SharedTextureData that vends WebGPU textures for the underlying IOSurface.
  std::unique_ptr<DawnSharedTextureHolder> dawn_texture_holder_;

  DawnSharedTextureHolder* GetDawnTextureHolder();

  // Tracks the number of currently-ongoing accesses to a given WGPU texture.
  base::flat_map<WGPUTexture, int> wgpu_texture_ongoing_accesses_;

  // Tracks the devices to invoke waitUntilScheduled.
  // TODO(dawn:2453): The below comparator should be implemented in
  // wgpu::Device itself.
  struct WGPUDeviceCompare {
    bool operator()(const wgpu::Device& lhs, const wgpu::Device& rhs) const {
      return lhs.Get() < rhs.Get();
    }
  };
  base::flat_set<wgpu::Device, WGPUDeviceCompare> wgpu_devices_pending_flush_;

  // Returns the number of ongoing accesses that were already present on this
  // texture prior to beginning this access.
  int TrackBeginAccessToWGPUTexture(wgpu::Texture texture);

  // Returns the number of ongoing accesses that will still be present on this
  // texture after ending this access.
  int TrackEndAccessToWGPUTexture(wgpu::Texture texture);

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
  base::flat_map<EGLDisplay, IOSurfaceBackingEGLState*> egl_state_map_;

  // GrContextType for SharedContextState used to distinguish between Ganesh
  // and Graphite.
  GrContextType gr_context_type_;

  // If Skia is using GL, this object creates a GL texture at construction time
  // for the Skia GL context and reuses it (for that context) for its lifetime.
  scoped_refptr<IOSurfaceBackingEGLState> egl_state_for_skia_gl_context_;

  // Tracks the displays to invoke eglWaitUntilWorkScheduledANGLE().
  base::flat_set<gl::GLDisplayEGL*> egl_displays_pending_flush_;

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
  SharedEventMap exclusive_shared_events_;
  // Shared events and signals for non-exclusive accesses.
  SharedEventMap non_exclusive_shared_events_;

  base::WeakPtrFactory<IOSurfaceImageBacking> weak_factory_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_IOSURFACE_IMAGE_BACKING_H_
