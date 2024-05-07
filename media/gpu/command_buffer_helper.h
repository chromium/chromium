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
}  // namespace gpu

namespace gl {
class GLContext;
}  // namespace gl

namespace media {

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

  // Add a callback to be called when our stub is destroyed. This callback
  // may not change the current context.
  virtual void AddWillDestroyStubCB(WillDestroyStubCB callback) = 0;

  // Does this command buffer support ARB_texture_rectangle.
  virtual bool SupportsTextureRectangle() const = 0;
#endif

 protected:
  explicit CommandBufferHelper(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // TODO(sandersd): Deleting remaining textures upon destruction requires
  // making the context current, which may be undesirable. Consider adding an
  // explicit DestroyWithContext() API.
  virtual ~CommandBufferHelper() = default;

 private:
  friend class base::DeleteHelper<CommandBufferHelper>;
  friend class base::RefCountedDeleteOnSequence<CommandBufferHelper>;
};

}  // namespace media

#endif  // MEDIA_GPU_COMMAND_BUFFER_HELPER_H_
