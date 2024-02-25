// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_DCOMP_TEXTURE_WRAPPER_H_
#define MEDIA_BASE_WIN_DCOMP_TEXTURE_WRAPPER_H_

#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace gpu {
struct Mailbox;
}  // namespace gpu

namespace media {

class VideoFrame;

// DCOMPTextureWrapper encapsulates a DCOMPTexture's initialization and other
// operations related to composition output path.
class DCOMPTextureWrapper {
 public:
  virtual ~DCOMPTextureWrapper() = default;

  // Initializes the DCOMPTexture and returns success/failure.
  using OutputRectChangeCB = base::RepeatingCallback<void(gfx::Rect)>;
  virtual bool Initialize(const gfx::Size& output_size,
                          OutputRectChangeCB output_rect_change_cb) = 0;

  // Called whenever the video's output size changes.
  virtual void UpdateTextureSize(const gfx::Size& output_size) = 0;

  // Sets the DirectComposition surface identified by `token`.
  using SetDCOMPSurfaceHandleCB = base::OnceCallback<void(bool)>;
  virtual void SetDCOMPSurfaceHandle(
      const base::UnguessableToken& token,
      SetDCOMPSurfaceHandleCB set_dcomp_surface_handle_cb) = 0;

  // Creates VideoFrame which will be returned in `create_video_frame_cb`.
  using CreateVideoFrameCB =
      base::OnceCallback<void(scoped_refptr<VideoFrame>, const gpu::Mailbox&)>;
  virtual void CreateVideoFrame(const gfx::Size& natural_size,
                                CreateVideoFrameCB create_video_frame_cb) = 0;

  using CreateDXVideoFrameCB =
      base::OnceCallback<void(scoped_refptr<VideoFrame>, const gpu::Mailbox&)>;
  virtual void CreateVideoFrame(const gfx::Size& natural_size,
                                gfx::GpuMemoryBufferHandle dx_handle,
                                CreateDXVideoFrameCB create_video_frame_cb) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_WIN_DCOMP_TEXTURE_WRAPPER_H_
