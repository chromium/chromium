// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_RENDERER_EXTENSION_H_
#define MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_RENDERER_EXTENSION_H_

#include "base/callback.h"
#include "base/win/scoped_handle.h"
#include "media/base/media_export.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

// This C++ enum is the equivalent to mojom::RenderingMode
enum class RenderingMode : int32_t {
  DirectComposition = 0,
  FrameServer = 1,
  kMinValue = 0,
  kMaxValue = 1,
};

// C++ interface equivalent to mojom::MediaFoundationRendererExtension.
// This interface allows MediaFoundationRenderer to support video rendering
// using Direct Compositon.
class MEDIA_EXPORT MediaFoundationRendererExtension {
 public:
  virtual ~MediaFoundationRendererExtension() = default;

  // TODO(frankli): naming: Change DComp into DirectComposition for interface
  // method names in a separate CL.

  // Enables Direct Composition video rendering and returns the Direct
  // Composition Surface handle. On failure, `error` explains the error reason.
  using GetDCompSurfaceCB = base::OnceCallback<void(base::win::ScopedHandle,
                                                    const std::string& error)>;
  virtual void GetDCompSurface(GetDCompSurfaceCB callback) = 0;

  // Notifies renderer whether video is enabled.
  virtual void SetVideoStreamEnabled(bool enabled) = 0;

  // Notifies renderer of output composition parameters.
  using SetOutputRectCB = base::OnceCallback<void(bool)>;
  virtual void SetOutputRect(const ::gfx::Rect& rect,
                             SetOutputRectCB callback) = 0;

  // Notify that the frame has been displayed and can be reused.
  virtual void NotifyFrameReleased(
      const base::UnguessableToken& frame_token) = 0;

  // Request a new frame to be provided to the client.
  virtual void RequestNextFrameBetweenTimestamps(
      base::TimeTicks deadline_min,
      base::TimeTicks deadline_max) = 0;

  // Change which mode we are using for video frame rendering.
  virtual void SetRenderingMode(RenderingMode mode) = 0;
};

}  // namespace media

#endif  // MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_RENDERER_EXTENSION_H_
