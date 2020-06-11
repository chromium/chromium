// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_GL_TEXTURE_INTERNAL_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_GL_TEXTURE_INTERNAL_H_

#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_backing_factory_gl_texture.h"
#include "gpu/command_buffer/service/shared_image_representation.h"

namespace gpu {

// Representation of a SharedImageBackingGLTexture or SharedImageBackingGLImage
// as a GL Texture.
class SharedImageRepresentationGLTextureImpl
    : public SharedImageRepresentationGLTexture {
 public:
  SharedImageRepresentationGLTextureImpl(SharedImageManager* manager,
                                         SharedImageBacking* backing,
                                         MemoryTypeTracker* tracker,
                                         gles2::Texture* texture);

 private:
  // SharedImageRepresentationGLTexturePassthrough:
  gles2::Texture* GetTexture() override;

  gles2::Texture* texture_;
};

// Representation of a SharedImageBackingGLTexture or
// SharedImageBackingGLTexturePassthrough as a GL TexturePassthrough.
class SharedImageRepresentationGLTexturePassthroughImpl
    : public SharedImageRepresentationGLTexturePassthrough {
 public:
  SharedImageRepresentationGLTexturePassthroughImpl(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<gles2::TexturePassthrough> texture_passthrough);
  ~SharedImageRepresentationGLTexturePassthroughImpl() override;

 private:
  // SharedImageRepresentationGLTexturePassthrough:
  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough()
      override;
  void EndAccess() override;

  scoped_refptr<gles2::TexturePassthrough> texture_passthrough_;
};

// Common superclass for SharedImageBackingGLTexture,
// SharedImageBackingPassthroughGLImage, and
// SharedImageRepresentationSkiaImpl.
class SharedImageBackingWithReadAccess : public SharedImageBacking {
 public:
  SharedImageBackingWithReadAccess(const Mailbox& mailbox,
                                   viz::ResourceFormat format,
                                   const gfx::Size& size,
                                   const gfx::ColorSpace& color_space,
                                   uint32_t usage,
                                   size_t estimated_size,
                                   bool is_thread_safe);
  ~SharedImageBackingWithReadAccess() override;

  virtual void BeginReadAccess() = 0;
};

// Skia representation for both SharedImageBackingGLTexture and
// SharedImageBackingGLTexturePassthrough.
class SharedImageRepresentationSkiaImpl : public SharedImageRepresentationSkia {
 public:
  SharedImageRepresentationSkiaImpl(
      SharedImageManager* manager,
      SharedImageBackingWithReadAccess* backing,
      scoped_refptr<SharedContextState> context_state,
      sk_sp<SkPromiseImageTexture> cached_promise_texture,
      MemoryTypeTracker* tracker,
      GLenum target,
      GLuint service_id);
  ~SharedImageRepresentationSkiaImpl() override;

  sk_sp<SkPromiseImageTexture> promise_texture();

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
  bool SupportsMultipleConcurrentReadAccess() override;

  // SharedImageBackingWithReadAccess:
  void EndReadAccess() override;

  void CheckContext();

  scoped_refptr<SharedContextState> context_state_;
  sk_sp<SkPromiseImageTexture> promise_texture_;

  SkSurface* write_surface_ = nullptr;
#if DCHECK_IS_ON()
  gl::GLContext* context_ = nullptr;
#endif
};

// Implementation of SharedImageBacking that creates a GL Texture that is not
// backed by a GLImage.
class SharedImageBackingGLTexture : public SharedImageBackingWithReadAccess {
 public:
  SharedImageBackingGLTexture(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage,
      gles2::Texture* texture,
      scoped_refptr<gles2::TexturePassthrough> passthrough_texture);
  ~SharedImageBackingGLTexture() override;

 private:
  // SharedImageBacking:
  gfx::Rect ClearedRect() const override;
  void SetClearedRect(const gfx::Rect& cleared_rect) override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override;
  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override;
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;
  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;
  std::unique_ptr<SharedImageRepresentationDawn> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      WGPUDevice device) override;

  // SharedImageBackingWithReadAccess:
  void BeginReadAccess() override;

  bool IsPassthrough() const;

  gles2::Texture* texture_ = nullptr;
  scoped_refptr<gles2::TexturePassthrough> passthrough_texture_;

  sk_sp<SkPromiseImageTexture> cached_promise_texture_;
};

// Implementation of SharedImageBacking that creates a GL Texture that is backed
// by a GLImage and stores it as a gles2::Texture. Can be used with the legacy
// mailbox implementation.
class SharedImageBackingGLImage : public SharedImageBackingWithReadAccess {
 public:
  SharedImageBackingGLImage(
      scoped_refptr<gl::GLImage> image,
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage,
      gles2::Texture* texture,
      const SharedImageBackingFactoryGLTexture::UnpackStateAttribs& attribs);
  ~SharedImageBackingGLImage() override;

 private:
  // SharedImageBacking:
  gfx::Rect ClearedRect() const override;
  void SetClearedRect(const gfx::Rect& cleared_rect) override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override;
  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override;
  scoped_refptr<gfx::NativePixmap> GetNativePixmap() override;
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;
  std::unique_ptr<SharedImageRepresentationGLTexture>
  ProduceRGBEmulationGLTexture(SharedImageManager* manager,
                               MemoryTypeTracker* tracker) override;
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;
  std::unique_ptr<SharedImageRepresentationDawn> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      WGPUDevice device) override;

  // SharedImageBackingWithReadAccess:
  void BeginReadAccess() override;

  scoped_refptr<gl::GLImage> image_;
  gles2::Texture* texture_ = nullptr;
  gles2::Texture* rgb_emulation_texture_ = nullptr;
  sk_sp<SkPromiseImageTexture> cached_promise_texture_;
  const SharedImageBackingFactoryGLTexture::UnpackStateAttribs attribs_;
  scoped_refptr<gfx::NativePixmap> native_pixmap_;
};

// Implementation of SharedImageBacking that creates a GL Texture and stores it
// as a gles2::TexturePassthrough. Can be used with the legacy mailbox
// implementation.
class SharedImageBackingPassthroughGLImage
    : public SharedImageBackingWithReadAccess {
 public:
  SharedImageBackingPassthroughGLImage(
      scoped_refptr<gl::GLImage> image,
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage,
      scoped_refptr<gles2::TexturePassthrough> passthrough_texture);
  ~SharedImageBackingPassthroughGLImage() override;

 private:
  // SharedImageBacking:
  gfx::Rect ClearedRect() const override;
  void SetClearedRect(const gfx::Rect& cleared_rect) override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override;
  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override;
  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;
  std::unique_ptr<SharedImageRepresentationDawn> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      WGPUDevice device) override;

  // SharedImageBackingWithReadAccess:
  void BeginReadAccess() override;

  scoped_refptr<gl::GLImage> image_;
  scoped_refptr<gles2::TexturePassthrough> texture_passthrough_;
  sk_sp<SkPromiseImageTexture> cached_promise_texture_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_GL_TEXTURE_INTERNAL_H_
