// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_GL_IMAGE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_GL_IMAGE_H_

#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_backing_gl_common.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gl/gl_fence.h"

namespace gpu {

// Interface through which a representation that has a GL texture calls into its
// GLImage backing.
class SharedImageRepresentationGLTextureClient {
 public:
  virtual bool SharedImageRepresentationGLTextureBeginAccess() = 0;
  virtual void SharedImageRepresentationGLTextureEndAccess(bool readonly) = 0;
  virtual void SharedImageRepresentationGLTextureRelease(bool have_context) = 0;
};

// Representation of a SharedImageBackingGLTexture or SharedImageBackingGLImage
// as a GL Texture.
class SharedImageRepresentationGLTextureImpl
    : public SharedImageRepresentationGLTexture {
 public:
  SharedImageRepresentationGLTextureImpl(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      SharedImageRepresentationGLTextureClient* client,
      MemoryTypeTracker* tracker,
      gles2::Texture* texture);
  ~SharedImageRepresentationGLTextureImpl() override;

 private:
  // SharedImageRepresentationGLTexture:
  gles2::Texture* GetTexture() override;
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

  SharedImageRepresentationGLTextureClient* const client_ = nullptr;
  gles2::Texture* texture_;
  GLenum mode_ = 0;
};

// Representation of a SharedImageBackingGLTexture or
// SharedImageBackingGLTexturePassthrough as a GL TexturePassthrough.
class SharedImageRepresentationGLTexturePassthroughImpl
    : public SharedImageRepresentationGLTexturePassthrough {
 public:
  class Client {
   public:
    virtual bool OnGLTexturePassthroughBeginAccess(GLenum mode) = 0;
  };
  SharedImageRepresentationGLTexturePassthroughImpl(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      SharedImageRepresentationGLTextureClient* client,
      MemoryTypeTracker* tracker,
      scoped_refptr<gles2::TexturePassthrough> texture_passthrough);
  ~SharedImageRepresentationGLTexturePassthroughImpl() override;

 private:
  // SharedImageRepresentationGLTexturePassthrough:
  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough()
      override;
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

  SharedImageRepresentationGLTextureClient* const client_ = nullptr;
  scoped_refptr<gles2::TexturePassthrough> texture_passthrough_;
  GLenum mode_ = 0;
};

// Skia representation for both SharedImageBackingGLCommon.
class SharedImageRepresentationSkiaImpl : public SharedImageRepresentationSkia {
 public:
  class Client {
   public:
    virtual bool OnSkiaBeginReadAccess() = 0;
    virtual bool OnSkiaBeginWriteAccess() = 0;
  };
  SharedImageRepresentationSkiaImpl(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      SharedImageRepresentationGLTextureClient* client,
      scoped_refptr<SharedContextState> context_state,
      sk_sp<SkPromiseImageTexture> promise_texture,
      MemoryTypeTracker* tracker);
  ~SharedImageRepresentationSkiaImpl() override;

  void SetBeginReadAccessCallback(
      base::RepeatingClosure begin_read_access_callback);

 private:
  // SharedImageRepresentationSkia:
  sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndWriteAccess(sk_sp<SkSurface> surface) override;
  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndReadAccess() override;
  bool SupportsMultipleConcurrentReadAccess() override;

  void CheckContext();

  SharedImageRepresentationGLTextureClient* const client_ = nullptr;
  scoped_refptr<SharedContextState> context_state_;
  sk_sp<SkPromiseImageTexture> promise_texture_;

  SkSurface* write_surface_ = nullptr;
#if DCHECK_IS_ON()
  gl::GLContext* context_ = nullptr;
#endif
};

// Overlay representation for a SharedImageBackingGLImage.
class SharedImageRepresentationOverlayImpl
    : public SharedImageRepresentationOverlay {
 public:
  SharedImageRepresentationOverlayImpl(SharedImageManager* manager,
                                       SharedImageBacking* backing,
                                       MemoryTypeTracker* tracker,
                                       scoped_refptr<gl::GLImage> gl_image);
  ~SharedImageRepresentationOverlayImpl() override;

 private:
  bool BeginReadAccess() override;
  void EndReadAccess() override;
  gl::GLImage* GetGLImage() override;
  std::unique_ptr<gfx::GpuFence> GetReadFence() override;

  scoped_refptr<gl::GLImage> gl_image_;
};

// Implementation of SharedImageBacking that creates a GL Texture that is backed
// by a GLImage and stores it as a gles2::Texture. Can be used with the legacy
// mailbox implementation.
class GPU_GLES2_EXPORT SharedImageBackingGLImage
    : public SharedImageBacking,
      public SharedImageRepresentationGLTextureClient {
 public:
  SharedImageBackingGLImage(
      scoped_refptr<gl::GLImage> image,
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      const SharedImageBackingGLCommon::InitializeGLTextureParams& params,
      const SharedImageBackingGLCommon::UnpackStateAttribs& attribs,
      bool is_passthrough);
  SharedImageBackingGLImage(const SharedImageBackingGLImage& other) = delete;
  SharedImageBackingGLImage& operator=(const SharedImageBackingGLImage& other) =
      delete;
  ~SharedImageBackingGLImage() override;

  void InitializePixels(GLenum format, GLenum type, const uint8_t* data);

  GLenum GetGLTarget() const;
  GLuint GetGLServiceId() const;
  std::unique_ptr<gfx::GpuFence> GetLastWriteGpuFence();

 private:
  // SharedImageBacking:
  scoped_refptr<gfx::NativePixmap> GetNativePixmap() override;
  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override;
  gfx::Rect ClearedRect() const final;
  void SetClearedRect(const gfx::Rect& cleared_rect) final;
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) final;
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) final;
  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) final;
  std::unique_ptr<SharedImageRepresentationOverlay> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) final;
  std::unique_ptr<SharedImageRepresentationDawn> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      WGPUDevice device) final;
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;
  std::unique_ptr<SharedImageRepresentationGLTexture>
  ProduceRGBEmulationGLTexture(SharedImageManager* manager,
                               MemoryTypeTracker* tracker) override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;

  // SharedImageRepresentationGLTextureClient:
  bool SharedImageRepresentationGLTextureBeginAccess() override;
  void SharedImageRepresentationGLTextureEndAccess(bool readonly) override;
  void SharedImageRepresentationGLTextureRelease(bool have_context) override;

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

  const SharedImageBackingGLCommon::InitializeGLTextureParams gl_params_;
  const SharedImageBackingGLCommon::UnpackStateAttribs gl_unpack_attribs_;
  const bool is_passthrough_;

  // This is the cleared rect used by ClearedRect and SetClearedRect when
  // |texture_| is nullptr.
  gfx::Rect cleared_rect_;

  gles2::Texture* rgb_emulation_texture_ = nullptr;
  gles2::Texture* texture_ = nullptr;
  scoped_refptr<gles2::TexturePassthrough> passthrough_texture_;

  sk_sp<SkPromiseImageTexture> cached_promise_texture_;
  std::unique_ptr<gl::GLFence> last_write_gl_fence_;

  base::WeakPtrFactory<SharedImageBackingGLImage> weak_factory_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_GL_IMAGE_H_
