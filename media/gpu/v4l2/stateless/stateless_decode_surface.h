// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_STATELESS_STATELESS_DECODE_SURFACE_H_
#define MEDIA_GPU_V4L2_STATELESS_STATELESS_DECODE_SURFACE_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "media/base/video_color_space.h"
#include "media/base/video_frame.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

class StatelessDecodeSurface : public base::RefCounted<StatelessDecodeSurface> {
 public:
  StatelessDecodeSurface(uint64_t frame_id, base::OnceClosure enqueue_cb);

  StatelessDecodeSurface(const StatelessDecodeSurface&) = delete;
  StatelessDecodeSurface& operator=(const StatelessDecodeSurface&) = delete;

  void SetVisibleRect(const gfx::Rect& visible_rect);
  void SetColorSpace(const VideoColorSpace& color_space);
  void SetVideoFrameTimestamp(const base::TimeDelta timestamp);

  uint64_t FrameID() const { return frame_id_; }
  VideoColorSpace ColorSpace() const { return color_space_; }
  base::TimeDelta VideoFrameTimestamp() const { return video_frame_timestamp_; }
  uint64_t GetReferenceTimestamp() const;
  gfx::Rect GetVisibleRect() const { return visible_rect_; }

  void SetReferenceSurfaces(
      std::vector<scoped_refptr<StatelessDecodeSurface>> ref_surfaces);
  void ClearReferenceSurfaces();

 protected:
  virtual ~StatelessDecodeSurface();
  friend class base::RefCounted<StatelessDecodeSurface>;

 private:
  // Identify this surface so that it can be matched up the the uncompressed
  // buffer when it is done being decompressed.
  const uint64_t frame_id_;

  // The visible size of the buffer.
  gfx::Rect visible_rect_;

  // The color space of the buffer.
  VideoColorSpace color_space_;

  // Timestamp associated with when the frame should be displayed.
  base::TimeDelta video_frame_timestamp_;

  // Callback to enqueue buffers once they are done being referenced.
  base::OnceClosure enqueue_cb_;

  // Frames that this frames uses for references. These are held onto until the
  // decode is done so that they are not reused while they need to be
  // referenced.
  std::vector<scoped_refptr<StatelessDecodeSurface>> reference_surfaces_;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_STATELESS_STATELESS_DECODE_SURFACE_H_
