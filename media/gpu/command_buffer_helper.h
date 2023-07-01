// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_COMMAND_BUFFER_HELPER_H_
#define MEDIA_GPU_COMMAND_BUFFER_HELPER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {
class CommandBufferStub;
class DXGISharedHandleManager;
class MemoryTypeTracker;
class SharedImageBacking;
class SharedImageManager;
class SharedImageRepresentationFactoryRef;
class SharedImageStub;
class TextureBase;
}  // namespace gpu

namespace gl {
class GLContext;
}  // namespace gl

namespace gpu {
class GLImageNativePixmap;
}

namespace media {

class PictureBufferManagerImpl;

// TODO(sandersd): CommandBufferHelper does not inherently need to be ref
// counted, but some clients want that (VdaVideoDecoder and PictureBufferManager
// both hold a ref to the same CommandBufferHelper). Consider making an owned
// variant.
class MEDIA_GPU_EXPORT CommandBufferHelper
    : public base::RefCountedDeleteOnSequence<CommandBufferHelper> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  using WillDestroyStubCB = base::OnceCallback<void(bool have_context)>;

  // TODO(sandersd): Consider adding an Initialize(stub) method so that
  // CommandBufferHelpers can be created before a stub is available.
  static scoped_refptr<CommandBufferHelper> Create(
      gpu::CommandBufferStub* stub);

  CommandBufferHelper(const CommandBufferHelper&) = delete;
  CommandBufferHelper& operator=(const CommandBufferHelper&) = delete;

  // Waits for a SyncToken, then runs |done_cb|.
  //
  // |done_cb| may be destructed without running if the stub is destroyed.
  //
  // TODO(sandersd): Currently it is possible to lose the stub while
  // PictureBufferManager is waiting for all picture buffers, which results in a
  // decoding softlock. Notification of wait failure (or just context/stub lost)
  // is probably necessary.
  // TODO(blundell): Consider inlining this method in the one Android caller and
  // eliminating this class being built on Android altogether.
  virtual void WaitForSyncToken(gpu::SyncToken sync_token,
                                base::OnceClosure done_cb) = 0;

#if !BUILDFLAG(IS_ANDROID)
  // Gets the associated GLContext.
  //
  // Used by DXVAVDA to test for D3D11 support, and by V4L2VDA to create
  // EGLImages. New clients should use more specialized accessors instead.
  virtual gl::GLContext* GetGLContext() = 0;

  // Retrieve the interface through which to create shared images.
  virtual gpu::SharedImageStub* GetSharedImageStub() = 0;

  virtual gpu::MemoryTypeTracker* GetMemoryTypeTracker() = 0;

  virtual gpu::SharedImageManager* GetSharedImageManager() = 0;

#if BUILDFLAG(IS_WIN)
  virtual gpu::DXGISharedHandleManager* GetDXGISharedHandleManager() = 0;
#endif

  // Checks whether the stub has been destroyed.
  virtual bool HasStub() = 0;

  // Makes the GL context current.
  virtual bool MakeContextCurrent() = 0;

  // Register a shared image backing
  virtual std::unique_ptr<gpu::SharedImageRepresentationFactoryRef> Register(
      std::unique_ptr<gpu::SharedImageBacking> backing) = 0;

  virtual gpu::TextureBase* GetTexture(GLuint service_id) const = 0;

  // Creates a texture and returns its |service_id|.
  //
  // See glTexImage2D() for argument definitions.
  //
  // The texture will be configured as a video frame: linear filtering, clamp to
  // edge. If |target| is GL_TEXTURE_2D, storage will be allocated but not
  // initialized.
  //
  // It is up to the caller to initialize the texture before providing it to the
  // renderer, else the results are undefined.
  //
  // The context must be current.
  //
  // TODO(sandersd): Is really necessary to allocate storage? GpuVideoDecoder
  // does this, but it's not clear that any clients require it.
  virtual GLuint CreateTexture(GLenum target,
                               GLenum internal_format,
                               GLsizei width,
                               GLsizei height,
                               GLenum format,
                               GLenum type) = 0;

  // Destroys a texture.
  //
  // The context must be current.
  virtual void DestroyTexture(GLuint service_id) = 0;

  // Sets the cleared flag on level 0 of the texture.
  virtual void SetCleared(GLuint service_id) = 0;

#if BUILDFLAG(IS_OZONE)
  // Binds level 0 of the texture to an image for which the sampler binding
  // already exists.
  //
  // BindTexImage() will *not* be called when the texture is used.
  virtual bool BindClientManagedImage(GLuint service_id,
                                      gpu::GLImageNativePixmap* image) = 0;
#endif

  // Add a callback to be called when our stub is destroyed. This callback
  // may not change the current context.
  virtual void AddWillDestroyStubCB(WillDestroyStubCB callback) = 0;

  // Is the backing command buffer passthrough (versus validating).
  virtual bool IsPassthrough() const = 0;

  // Does this command buffer support ARB_texture_rectangle.
  virtual bool SupportsTextureRectangle() const = 0;
#endif

 protected:
  explicit CommandBufferHelper(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // TODO(sandersd): Deleting remaining textures upon destruction requires
  // making the context current, which may be undesireable. Consider adding an
  // explicit DestroyWithContext() API.
  virtual ~CommandBufferHelper() = default;

 private:
#if !BUILDFLAG(IS_ANDROID)
  // Creates a legacy mailbox for a texture.
  // NOTE: We are in the process of eliminating this method. DO NOT ADD ANY NEW
  // USAGES - instead, reach out to shared-image-team@ with your use case. See
  // crbug.com/1273084.
  //
  // TODO(sandersd): Specify the behavior when the stub has been destroyed. The
  // current implementation returns an empty (zero) mailbox. One solution would
  // be to add a HasStub() method, and not define behavior when it is false.
  virtual gpu::Mailbox CreateLegacyMailbox(GLuint service_id) = 0;

  friend class PictureBufferManagerImpl;
#endif

  friend class base::DeleteHelper<CommandBufferHelper>;
  friend class base::RefCountedDeleteOnSequence<CommandBufferHelper>;
};

}  // namespace media

#endif  // MEDIA_GPU_COMMAND_BUFFER_HELPER_H_
