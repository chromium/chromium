// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_EGL_IMAGE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_EGL_IMAGE_H_

#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gl/gl_bindings.h"

namespace gl {
class GLFenceEGL;
class SharedGLFenceEGL;
}  // namespace gl

namespace gpu {
class GpuDriverBugWorkarounds;
class SharedImageRepresentationGLTexture;
class SharedImageRepresentationSkia;
class SharedImageBatchAccessManager;
struct Mailbox;

namespace gles2 {
class NativeImageBuffer;
}  // namespace gles2

// Implementation of SharedImageBacking that is used to create EGLImage targets
// from the same EGLImage object. Hence all the representations created from
// this backing uses EGL Image siblings. This backing is thread safe across
// different threads running different GL contexts not part of same shared
// group. This is achieved by using locks and fences for proper synchronization.
class SharedImageBackingEglImage : public ClearTrackingSharedImageBacking {
 public:
  SharedImageBackingEglImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      size_t estimated_size,
      GLuint gl_format,
      GLuint gl_type,
      SharedImageBatchAccessManager* batch_access_manager,
      const GpuDriverBugWorkarounds& workarounds,
      bool use_passthrough);

  ~SharedImageBackingEglImage() override;

  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override;
  void MarkForDestruction() override;

 protected:
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

 private:
  friend class SharedImageBatchAccessManager;
  class TextureHolder;
  class RepresentationGLShared;
  class RepresentationGLTexture;
  class RepresentationGLTexturePassthrough;

  template <class T>
  std::unique_ptr<T> ProduceGLTextureInternal(SharedImageManager* manager,
                                              MemoryTypeTracker* tracker);

  bool BeginWrite();
  void EndWrite();
  bool BeginRead(const RepresentationGLShared* reader);
  void EndRead(const RepresentationGLShared* reader);

  // Use to create EGLImage texture target from the same EGLImage object.
  scoped_refptr<TextureHolder> GenEGLImageSibling();

  void SetEndReadFence(scoped_refptr<gl::SharedGLFenceEGL> shared_egl_fence);

  const GLuint gl_format_;
  const GLuint gl_type_;
  scoped_refptr<TextureHolder> source_texture_holder_;
  gl::GLApi* created_on_context_;

  // This class encapsulates the EGLImage object for android.
  scoped_refptr<gles2::NativeImageBuffer> egl_image_buffer_ GUARDED_BY(lock_);

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
  base::flat_set<const RepresentationGLShared*> active_readers_
      GUARDED_BY(lock_);
  SharedImageBatchAccessManager* batch_access_manager_ = nullptr;

  const bool use_passthrough_;

  DISALLOW_COPY_AND_ASSIGN(SharedImageBackingEglImage);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_EGL_IMAGE_H_
