// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_IMAGE_BACKING_H_

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing_helper.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_image_memory.h"

namespace gpu {

// Interface through which a representation that has a GL texture calls into its
// GLImage backing.
class GLTextureImageRepresentationClient {
 public:
  virtual bool GLTextureImageRepresentationBeginAccess(bool readonly) = 0;
  virtual void GLTextureImageRepresentationEndAccess(bool readonly) = 0;
  virtual void GLTextureImageRepresentationRelease(bool have_context) = 0;
};

// Representation of a GLTextureImageBacking or GLImageBacking
// as a GL Texture.
class GLTextureGLCommonRepresentation : public GLTextureImageRepresentation {
 public:
  GLTextureGLCommonRepresentation(SharedImageManager* manager,
                                  SharedImageBacking* backing,
                                  GLTextureImageRepresentationClient* client,
                                  MemoryTypeTracker* tracker,
                                  gles2::Texture* texture);
  ~GLTextureGLCommonRepresentation() override;

 private:
  // GLTextureImageRepresentation:
  gles2::Texture* GetTexture() override;
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

  const raw_ptr<GLTextureImageRepresentationClient> client_ = nullptr;
  raw_ptr<gles2::Texture> texture_;
  GLenum mode_ = 0;
};

// Representation of a GLTextureImageBacking or
// GLTextureImageBackingPassthrough as a GL TexturePassthrough.
class GLTexturePassthroughGLCommonRepresentation
    : public GLTexturePassthroughImageRepresentation {
 public:
  class Client {
   public:
    virtual bool OnGLTexturePassthroughBeginAccess(GLenum mode) = 0;
  };
  GLTexturePassthroughGLCommonRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      GLTextureImageRepresentationClient* client,
      MemoryTypeTracker* tracker,
      scoped_refptr<gles2::TexturePassthrough> texture_passthrough);
  ~GLTexturePassthroughGLCommonRepresentation() override;

 private:
  // GLTexturePassthroughImageRepresentation:
  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough()
      override;
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

  const raw_ptr<GLTextureImageRepresentationClient> client_ = nullptr;
  scoped_refptr<gles2::TexturePassthrough> texture_passthrough_;
  GLenum mode_ = 0;
};

// Skia representation for both GLTextureImageBackingHelper.
class SkiaGLCommonRepresentation : public SkiaImageRepresentation {
 public:
  class Client {
   public:
    virtual bool OnSkiaBeginReadAccess() = 0;
    virtual bool OnSkiaBeginWriteAccess() = 0;
  };
  SkiaGLCommonRepresentation(SharedImageManager* manager,
                             SharedImageBacking* backing,
                             GLTextureImageRepresentationClient* client,
                             scoped_refptr<SharedContextState> context_state,
                             sk_sp<SkPromiseImageTexture> promise_texture,
                             MemoryTypeTracker* tracker);
  ~SkiaGLCommonRepresentation() override;

  void SetBeginReadAccessCallback(
      base::RepeatingClosure begin_read_access_callback);

 private:
  // SkiaImageRepresentation:
  sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override;
  sk_sp<SkPromiseImageTexture> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphore,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override;
  void EndWriteAccess(sk_sp<SkSurface> surface) override;
  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override;
  void EndReadAccess() override;
  bool SupportsMultipleConcurrentReadAccess() override;

  void CheckContext();

  const raw_ptr<GLTextureImageRepresentationClient> client_ = nullptr;
  scoped_refptr<SharedContextState> context_state_;
  sk_sp<SkPromiseImageTexture> promise_texture_;

  raw_ptr<SkSurface> write_surface_ = nullptr;
#if DCHECK_IS_ON()
  raw_ptr<gl::GLContext> context_ = nullptr;
#endif
};

// Overlay representation for a GLImageBacking.
class OverlayGLImageRepresentation : public OverlayImageRepresentation {
 public:
  OverlayGLImageRepresentation(SharedImageManager* manager,
                               SharedImageBacking* backing,
                               MemoryTypeTracker* tracker,
                               scoped_refptr<gl::GLImage> gl_image);
  ~OverlayGLImageRepresentation() override;

 private:
  bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) override;
  void EndReadAccess(gfx::GpuFenceHandle release_fence) override;
  gl::GLImage* GetGLImage() override;

  scoped_refptr<gl::GLImage> gl_image_;
};

class MemoryGLImageRepresentation : public MemoryImageRepresentation {
 public:
  MemoryGLImageRepresentation(SharedImageManager* manager,
                              SharedImageBacking* backing,
                              MemoryTypeTracker* tracker,
                              scoped_refptr<gl::GLImageMemory> image_memory);
  ~MemoryGLImageRepresentation() override;

 protected:
  SkPixmap BeginReadAccess() override;

 private:
  scoped_refptr<gl::GLImageMemory> image_memory_;
};

// Implementation of SharedImageBacking that creates a GL Texture that is backed
// by a GLImage and stores it as a gles2::Texture. Can be used with the legacy
// mailbox implementation.
class GPU_GLES2_EXPORT GLImageBacking
    : public SharedImageBacking,
      public GLTextureImageRepresentationClient {
 public:
  // Used when GLImageBacking is serving as a temporary SharedImage
  // wrapper to an already-allocated texture. The returned backing will not
  // create any new textures.
  static std::unique_ptr<GLImageBacking> CreateFromGLTexture(
      scoped_refptr<gl::GLImage> image,
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      GLenum texture_target,
      scoped_refptr<gles2::TexturePassthrough> wrapped_gl_texture);

  GLImageBacking(
      scoped_refptr<gl::GLImage> image,
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      const GLTextureImageBackingHelper::InitializeGLTextureParams& params,
      bool is_passthrough);
  GLImageBacking(const GLImageBacking& other) = delete;
  GLImageBacking& operator=(const GLImageBacking& other) = delete;
  ~GLImageBacking() override;

  void InitializePixels(GLenum format, GLenum type, const uint8_t* data);

  GLenum GetGLTarget() const;
  GLuint GetGLServiceId() const;
  std::unique_ptr<gfx::GpuFence> GetLastWriteGpuFence();
  void SetReleaseFence(gfx::GpuFenceHandle release_fence);

 private:
  // SharedImageBacking:
  scoped_refptr<gfx::NativePixmap> GetNativePixmap() override;
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

  // GLTextureImageRepresentationClient:
  bool GLTextureImageRepresentationBeginAccess(bool readonly) override;
  void GLTextureImageRepresentationEndAccess(bool readonly) override;
  void GLTextureImageRepresentationRelease(bool have_context) override;

  bool IsPassthrough() const { return is_passthrough_; }

  scoped_refptr<gl::GLImage> image_;

  // If |image_bind_or_copy_needed_| is true, then either bind or copy |image_|
  // to the GL texture, and un-set |image_bind_or_copy_needed_|.
  bool BindOrCopyImageIfNeeded();
  bool image_bind_or_copy_needed_ = true;

  void RetainGLTexture();
  void ReleaseGLTexture(bool have_context);
  size_t gl_texture_retain_count_ = 0;
  bool gl_texture_retained_for_legacy_mailbox_ = false;

  const GLTextureImageBackingHelper::InitializeGLTextureParams gl_params_;
  const bool is_passthrough_;

  // This is the cleared rect used by ClearedRect and SetClearedRect when
  // |texture_| is nullptr.
  gfx::Rect cleared_rect_;

  gles2::Texture* texture_ = nullptr;
  scoped_refptr<gles2::TexturePassthrough> passthrough_texture_;

  sk_sp<SkPromiseImageTexture> cached_promise_texture_;
  std::unique_ptr<gl::GLFence> last_write_gl_fence_;

  // If this backing was displayed as an overlay, this fence may be set.
  // Wait on this fence before allowing another access.
  gfx::GpuFenceHandle release_fence_;

  base::WeakPtrFactory<GLImageBacking> weak_factory_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_IMAGE_BACKING_H_
