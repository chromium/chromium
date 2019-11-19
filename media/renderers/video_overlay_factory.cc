// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/video_overlay_factory.h"

#include "base/time/time.h"
#include "media/base/video_frame.h"
#include "ui/gfx/geometry/size.h"

namespace media {

VideoOverlayFactory::VideoOverlayFactory()
    : overlay_plane_id_(base::UnguessableToken::Create()) {}

VideoOverlayFactory::~VideoOverlayFactory() = default;

scoped_refptr<VideoFrame> VideoOverlayFactory::CreateFrame(
    const gfx::Size& size) {
  // Frame size empty => video has one dimension = 0.
  // Dimension 0 case triggers a DCHECK later on if we push through the overlay
  // path.
  if (size.IsEmpty()) {
    DVLOG(1) << "Create black frame " << size.width() << "x" << size.height();
    return VideoFrame::CreateBlackFrame(gfx::Size(1, 1));
  }

  DVLOG(2) << "Create video overlay frame: " << size.ToString();
  scoped_refptr<VideoFrame> frame =
      VideoFrame::CreateVideoHoleFrame(overlay_plane_id_,
                                       size,                // natural size
                                       base::TimeDelta());  // timestamp
  DCHECK(frame);
  return frame;
}

}  // namespace media
