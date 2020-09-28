// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_IPC_SERVICE_PICTURE_BUFFER_MANAGER_H_
#define MEDIA_GPU_IPC_SERVICE_PICTURE_BUFFER_MANAGER_H_

#include <stdint.h>

#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/gpu/command_buffer_helper.h"
#include "media/video/picture.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class PictureBufferManager
    : public base::RefCountedThreadSafe<PictureBufferManager> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  using ReusePictureBufferCB = base::RepeatingCallback<void(int32_t)>;

  // Creates a PictureBufferManager.
  //
  // |reuse_picture_buffer_cb|: Called when a picture is returned to the pool
  //     after its VideoFrame has been destructed.
  static scoped_refptr<PictureBufferManager> Create(
      ReusePictureBufferCB reuse_picture_buffer_cb);

  // Provides access to a CommandBufferHelper. This must be done before calling
  // CreatePictureBuffers().
  //
  // TODO(sandersd): It would be convenient to set this up at creation time.
  // Consider changes to CommandBufferHelper that would enable that.
  virtual void Initialize(
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      scoped_refptr<CommandBufferHelper> command_buffer_helper) = 0;

  // Predicts whether the VDA can output a picture without reusing one first.
  //
  // Implementations should be pessimistic; it is better to incorrectly skip
  // preroll than to hang waiting for an output that can never come.
  virtual bool CanReadWithoutStalling() = 0;

  // Creates and returns a vector of picture buffers, or an empty vector on
  // failure.
  //
  // |count|: Number of picture buffers to create.
  // |pixel_format|: Describes the arrangement of image data in the picture's
  //     textures and is surfaced by VideoFrames.
  // |planes|: Number of image planes (textures) in the picture.
  // |texture_size|: Size of textures to create.
  // |texture_target|: Type of textures to create.
  //
  // Must be called on the GPU thread.
  //
  // TODO(sandersd): For many subsampled pixel formats, it doesn't make sense to
  // allocate all planes with the same size.
  // TODO(sandersd): Surface control over allocation for GL_TEXTURE_2D. Right
  // now such textures are allocated as RGBA textures. (Other texture targets
  // are not automatically allocated.)
  // TODO(sandersd): The current implementation makes the context current.
  // Consider requiring that the context is already current.
  virtual std::vector<PictureBuffer> CreatePictureBuffers(
      uint32_t count,
      VideoPixelFormat pixel_format,
      uint32_t planes,
      gfx::Size texture_size,
      uint32_t texture_target) = 0;

  // Dismisses a picture buffer from the pool.
  //
  // A picture buffer may be dismissed even if it is bound to a VideoFrame; its
  // backing textures will be maintained until the VideoFrame is destroyed.
  virtual bool DismissPictureBuffer(int32_t picture_buffer_id) = 0;

  // Dismisses all picture buffers from the pool.
  virtual void DismissAllPictureBuffers() = 0;

  // Creates and returns a VideoFrame bound to a picture buffer, or nullptr on
  // failure.
  //
  // |picture|: Identifies the picture buffer and provides some metadata about
  //     the desired binding. Not all Picture features are supported.
  // |timestamp|: Presentation timestamp of the VideoFrame.
  // |visible_rect|: Visible region of the VideoFrame.
  // |natural_size|: Natural size of the VideoFrame.
  //
  // TODO(sandersd): Specify which Picture features are supported.
  virtual scoped_refptr<VideoFrame> CreateVideoFrame(
      Picture picture,
      base::TimeDelta timestamp,
      gfx::Rect visible_rect,
      gfx::Size natural_size) = 0;

 protected:
  PictureBufferManager() = default;

  // Must be called on the GPU thread if Initialize() was called.
  virtual ~PictureBufferManager() = default;

 private:
  friend class base::RefCountedThreadSafe<PictureBufferManager>;

  DISALLOW_COPY_AND_ASSIGN(PictureBufferManager);
};

}  // namespace media

#endif  // MEDIA_GPU_IPC_SERVICE_PICTURE_BUFFER_MANAGER_H_
