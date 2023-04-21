// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EGL_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EGL_IMAGE_BACKING_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/shared_image/gl_common_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing_helper.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/scoped_egl_image.h"

namespace gl {
class GLFenceEGL;
class SharedGLFenceEGL;
}  // namespace gl

namespace gpu {
class GpuDriverBugWorkarounds;
struct Mailbox;

// Implementation of SharedImageBacking that is used to create EGLImage targets
// from the same EGLImage object. Hence all the representations created from
// this backing uses EGL Image siblings. This backing is thread safe across
// different threads running different GL contexts not part of same shared
// group. This is achieved by using locks and fences for proper synchronization.
class EGLImageBacking : public ClearTrackingSharedImageBacking {
 public:
  EGLImageBacking(const Mailbox& mailbox,
                  viz::SharedImageFormat format,
                  const gfx::Size& size,
                  const gfx::ColorSpace& color_space,
                  GrSurfaceOrigin surface_origin,
                  SkAlphaType alpha_type,
                  uint32_t usage,
                  size_t estimated_size,
                  const GLCommonImageBackingFactory::FormatInfo& format_into,
                  const GpuDriverBugWorkarounds& workarounds,
                  bool use_passthrough,
                  base::span<const uint8_t> pixel_data);

  EGLImageBacking(const EGLImageBacking&) = delete;
  EGLImageBacking& operator=(const EGLImageBacking&) = delete;

  ~EGLImageBacking() override;

  // SharedImageBacking implementation.
  SharedImageBackingType GetType() const override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  void MarkForDestruction() override;

 protected:
  std::unique_ptr<GLTextureImageRepresentation> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;

  std::unique_ptr<SkiaGaneshImageRepresentation> ProduceSkiaGanesh(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

  std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      WGPUDevice device,
      WGPUBackendType backend_type,
      std::vector<WGPUTextureFormat> view_formats) final;

 private:
  class TextureHolder;
  class GLRepresentationShared;
  class GLTextureEGLImageRepresentation;
  class GLTexturePassthroughEGLImageRepresentation;

  template <class T>
  std::unique_ptr<T> ProduceGLTextureInternal(SharedImageManager* manager,
                                              MemoryTypeTracker* tracker);

  bool BeginWrite();
  void EndWrite();
  bool BeginRead(const GLRepresentationShared* reader);
  void EndRead(const GLRepresentationShared* reader);

  // Use to create EGLImage texture target from the same EGLImage object.
  // Optional |pixel_data| to initialize a texture with before EGLImage object
  // is created from it.
  scoped_refptr<TextureHolder> GenEGLImageSibling(
      base::span<const uint8_t> pixel_data);

  const GLCommonImageBackingFactory::FormatInfo format_info_;
  scoped_refptr<TextureHolder> source_texture_holder_;
  raw_ptr<gl::GLApi> created_on_context_;

  gl::ScopedEGLImage egl_image_ GUARDED_BY(lock_);

  // All reads and writes must wait for exiting writes to complete.
  // TODO(vikassoni): Use SharedGLFenceEGL here instead of GLFenceEGL here in
  // future for |write_fence_| once the SharedGLFenceEGL has the capability to
  // support multiple GLContexts.
  std::unique_ptr<gl::GLFenceEGL> write_fence_ GUARDED_BY(lock_);
  bool is_writing_ GUARDED_BY(lock_) = false;

  // All writes must wait for existing reads to complete. For a given GL
  // context, we only need to keep the most recent fence. Waiting on the most
  // recent read fence is enough to make sure all past read fences have been
  // signalled.
  base::flat_map<gl::GLApi*, scoped_refptr<gl::SharedGLFenceEGL>> read_fences_
      GUARDED_BY(lock_);
  base::flat_set<const GLRepresentationShared*> active_readers_
      GUARDED_BY(lock_);

  const bool use_passthrough_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EGL_IMAGE_BACKING_H_
