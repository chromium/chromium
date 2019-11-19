// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_LAME_CAPTURE_OVERLAY_CHROMEOS_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_LAME_CAPTURE_OVERLAY_CHROMEOS_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect_f.h"

namespace media {
class VideoFrame;
}

namespace content {

// A minimal FrameSinkVideoCaptureOverlay implementation for aura::Window video
// capture on ChromeOS (i.e., not desktop capture, and not WebContents capture).
// See class comments for LameWindowCapturerChromeOS for further details on why
// this exists and why this placeholder is needed for now.
//
// The implementation here is a hodgepodge of code borrowed from
// viz::VideoCaptureOverlay and the legacy content::CursorRenderer. Like its
// full-featured VIZ cousin, it will cache the scaled version of the bitmap, and
// re-use it across multiple frames. However, the rest of the rendering impl is
// overly-simplified, sufferring from color accuracy and image sampling issues.
// However, its quality is sufficient for its main use as a mouse cursor
// renderer.
//
// TODO(crbug/806366): The goal is to remove this code by 2019.
class CONTENT_EXPORT LameCaptureOverlayChromeOS
    : public viz::mojom::FrameSinkVideoCaptureOverlay {
 public:
  // Implemented by LameWindowCapturerChromeOS.
  class CONTENT_EXPORT Owner {
   public:
    // Called to notify that the |overlay| has lost its mojo binding. The owner
    // will usually delete it during this method call.
    virtual void OnOverlayConnectionLost(
        LameCaptureOverlayChromeOS* overlay) = 0;

   protected:
    virtual ~Owner();
  };

  // A OnceCallback that, when run, renders the overlay on a VideoFrame.
  using OnceRenderer = base::OnceCallback<void(media::VideoFrame*)>;

  LameCaptureOverlayChromeOS(
      Owner* owner,
      mojo::PendingReceiver<viz::mojom::FrameSinkVideoCaptureOverlay> receiver);
  ~LameCaptureOverlayChromeOS() final;

  // viz::mojom::FrameSinkVideoCaptureOverlay implementation.
  void SetImageAndBounds(const SkBitmap& image, const gfx::RectF& bounds) final;
  void SetBounds(const gfx::RectF& bounds) final;

  // Returns a OnceCallback that, when run, renders this overlay on an
  // I420-format VideoFrame. The overlay's position and size are computed based
  // on the given content |region_in_frame|. Returns a null OnceCallback if
  // there is nothing to render at this time.
  OnceRenderer MakeRenderer(const gfx::Rect& region_in_frame);

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  mojo::Receiver<viz::mojom::FrameSinkVideoCaptureOverlay> receiver_;

  SkBitmap image_;
  gfx::RectF bounds_;

  // The scaled |image_| used in the last call to MakeRenderer(). This is reset
  // and re-generated whenever: a) the |image_| changes, or b) the required
  // bitmap size changes.
  SkBitmap cached_scaled_image_;

  DISALLOW_COPY_AND_ASSIGN(LameCaptureOverlayChromeOS);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_LAME_CAPTURE_OVERLAY_CHROMEOS_H_
