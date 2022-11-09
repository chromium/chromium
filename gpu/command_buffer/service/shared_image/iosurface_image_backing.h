// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_IOSURFACE_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_IOSURFACE_IMAGE_BACKING_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing_helper.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_image_memory.h"

namespace gl {
class ScopedEGLSurfaceIOSurface;
}  // namespace gl

namespace gpu {

// Interface through which a representation that has a GL texture calls into its
// IOSurface backing.
class GLTextureIOSurfaceRepresentationClient {
 public:
  virtual bool GLTextureImageRepresentationBeginAccess(bool readonly) = 0;
  virtual void GLTextureImageRepresentationEndAccess(bool readonly) = 0;
  virtual void GLTextureImageRepresentationRelease(bool have_context) = 0;
};

// Representation of a GLTextureImageBacking or
// GLTextureImageBackingPassthrough as a GL TexturePassthrough.
class GLTextureIOSurfaceRepresentation
    : public GLTexturePassthroughImageRepresentation {
 public:
  GLTextureIOSurfaceRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      GLTextureIOSurfaceRepresentationClient* client,
      MemoryTypeTracker* tracker,
      scoped_refptr<gles2::TexturePassthrough> texture_passthrough);
  ~GLTextureIOSurfaceRepresentation() override;

 private:
  // GLTexturePassthroughImageRepresentation:
  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      int plane_index) override;
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

  const raw_ptr<GLTextureIOSurfaceRepresentationClient> client_ = nullptr;
  scoped_refptr<gles2::TexturePassthrough> texture_;
  GLenum mode_ = 0;
};

// Skia representation for both GLTextureImageBackingHelper.
class SkiaIOSurfaceRepresentation : public SkiaImageRepresentation {
 public:
  class Client {
   public:
    virtual bool OnSkiaBeginReadAccess() = 0;
    virtual bool OnSkiaBeginWriteAccess() = 0;
  };
  SkiaIOSurfaceRepresentation(SharedImageManager* manager,
                              SharedImageBacking* backing,
                              GLTextureIOSurfaceRepresentationClient* client,
                              scoped_refptr<SharedContextState> context_state,
                              sk_sp<SkPromiseImageTexture> promise_texture,
                              MemoryTypeTracker* tracker);
  ~SkiaIOSurfaceRepresentation() override;

  void SetBeginReadAccessCallback(
      base::RepeatingClosure begin_read_access_callback);

 private:
  // SkiaImageRepresentation:
  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override;
  std::vector<sk_sp<SkPromiseImageTexture>> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphore,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override;
  void EndWriteAccess() override;
  std::vector<sk_sp<SkPromiseImageTexture>> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override;
  void EndReadAccess() override;
  bool SupportsMultipleConcurrentReadAccess() override;

  void CheckContext();

  const raw_ptr<GLTextureIOSurfaceRepresentationClient> client_ = nullptr;
  scoped_refptr<SharedContextState> context_state_;
  sk_sp<SkPromiseImageTexture> promise_texture_;
  sk_sp<SkSurface> write_surface_;
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
                                 scoped_refptr<gl::GLImage> gl_image);
  ~OverlayIOSurfaceRepresentation() override;

 private:
  bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) override;
  void EndReadAccess(gfx::GpuFenceHandle release_fence) override;
  gfx::ScopedIOSurface GetIOSurface() const override;
  bool IsInUseByWindowServer() const override;

  scoped_refptr<gl::GLImage> gl_image_;
};

class MemoryIOSurfaceRepresentation : public MemoryImageRepresentation {
 public:
  MemoryIOSurfaceRepresentation(SharedImageManager* manager,
                                SharedImageBacking* backing,
                                MemoryTypeTracker* tracker,
                                scoped_refptr<gl::GLImageMemory> image_memory);
  ~MemoryIOSurfaceRepresentation() override;

 protected:
  SkPixmap BeginReadAccess() override;

 private:
  scoped_refptr<gl::GLImageMemory> image_memory_;
};

// This class is only put into unique_ptrs and is never copied or assigned.
class SharedEventAndSignalValue {
 public:
  SharedEventAndSignalValue(id shared_event, uint64_t signaled_value);
  ~SharedEventAndSignalValue();
  SharedEventAndSignalValue(const SharedEventAndSignalValue& other) = delete;
  SharedEventAndSignalValue(SharedEventAndSignalValue&& other) = delete;
  SharedEventAndSignalValue& operator=(const SharedEventAndSignalValue& other) =
      delete;

  // Return value is actually id<MTLSharedEvent>.
  id shared_event() const { return shared_event_; }

  // This is the value which will be signaled on the associated MTLSharedEvent.
  uint64_t signaled_value() const { return signaled_value_; }

 private:
  id shared_event_;
  uint64_t signaled_value_;
};

// Implementation of SharedImageBacking that creates a GL Texture that is backed
// by a GLImage and stores it as a gles2::Texture. Can be used with the legacy
// mailbox implementation.
class GPU_GLES2_EXPORT IOSurfaceImageBacking
    : public SharedImageBacking,
      public GLTextureIOSurfaceRepresentationClient {
 public:
  IOSurfaceImageBacking(
      scoped_refptr<gl::GLImage> image,
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      const GLTextureImageBackingHelper::InitializeGLTextureParams& params);
  IOSurfaceImageBacking(const IOSurfaceImageBacking& other) = delete;
  IOSurfaceImageBacking& operator=(const IOSurfaceImageBacking& other) = delete;
  ~IOSurfaceImageBacking() override;

  void InitializePixels(GLenum format, GLenum type, const uint8_t* data);

  GLenum GetGLTarget() const;
  GLuint GetGLServiceId() const;
  std::unique_ptr<gfx::GpuFence> GetLastWriteGpuFence();
  void SetReleaseFence(gfx::GpuFenceHandle release_fence);

  void AddSharedEventAndSignalValue(id sharedEvent, uint64_t signalValue);
  std::vector<std::unique_ptr<SharedEventAndSignalValue>> TakeSharedEvents();

 private:
  // SharedImageBacking:
  void OnMemoryDump(const std::string& dump_name,
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
      WGPUDevice device,
      WGPUBackendType backend_type) final;
  std::unique_ptr<SkiaImageRepresentation> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;
  std::unique_ptr<MemoryImageRepresentation> ProduceMemory(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;

  // GLTextureIOSurfaceRepresentationClient:
  bool GLTextureImageRepresentationBeginAccess(bool readonly) override;
  void GLTextureImageRepresentationEndAccess(bool readonly) override;
  void GLTextureImageRepresentationRelease(bool have_context) override;

  bool IsPassthrough() const { return true; }

  scoped_refptr<gl::GLImage> image_;

  // Used to determine whether to release the texture in EndAccess() in use
  // cases that need to ensure IOSurface synchronization.
  uint num_ongoing_read_accesses_ = 0;
  // Used with the above variable to catch cases where clients are performing
  // disallowed concurrent read/write accesses.
  bool ongoing_write_access_ = false;

  void RetainGLTexture();
  void ReleaseGLTexture(bool have_context);
  size_t gl_texture_retain_count_ = 0;
  bool gl_texture_retained_for_legacy_mailbox_ = false;

  const GLTextureImageBackingHelper::InitializeGLTextureParams gl_params_;

  // This is the cleared rect used by ClearedRect and SetClearedRect when
  // |texture_| is nullptr.
  gfx::Rect cleared_rect_;

  std::unique_ptr<gl::ScopedEGLSurfaceIOSurface> egl_surface_;
  scoped_refptr<gles2::TexturePassthrough> gl_texture_;

  sk_sp<SkPromiseImageTexture> cached_promise_texture_;
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
