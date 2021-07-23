// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_DCOMP_TEXTURE_WRAPPER_H_
#define MEDIA_BASE_WIN_DCOMP_TEXTURE_WRAPPER_H_

#include "base/callback.h"
#include "base/unguessable_token.h"

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace media {

class VideoFrame;

// DCOMPTextureWrapper encapsulates a DCOMPTexture's initialization and other
// operations related to composition output path.
class DCOMPTextureWrapper {
 public:
  virtual ~DCOMPTextureWrapper() = default;

  // Initializes the DCOMPTexture and returns success/failure in `init_cb`.
  // TODO(xhwang): Pass `DCOMPSurfaceHandleBoundCB` in `SetDCOMPSurface()`.
  using DCOMPSurfaceHandleBoundCB = base::OnceCallback<void(bool)>;
  using CompositionParamsReceivedCB = base::RepeatingCallback<void(gfx::Rect)>;
  using InitCB = base::OnceCallback<void(bool)>;
  virtual void Initialize(const gfx::Size& natural_size,
                          DCOMPSurfaceHandleBoundCB dcomp_handle_bound_cb,
                          CompositionParamsReceivedCB comp_params_received_cb,
                          InitCB init_cb) = 0;

  // Called whenever the video's natural size changes.
  virtual void UpdateTextureSize(const gfx::Size& natural_size) = 0;

  // Sets the DirectComposition surface identified by `surface_token`.
  virtual void SetDCOMPSurface(const base::UnguessableToken& surface_token) = 0;

  // Creates VideoFrame which will be returned in `create_video_frame_cb`.
  using CreateVideoFrameCB =
      base::OnceCallback<void(scoped_refptr<VideoFrame>)>;
  virtual void CreateVideoFrame(CreateVideoFrameCB create_video_frame_cb) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_WIN_DCOMP_TEXTURE_WRAPPER_H_
