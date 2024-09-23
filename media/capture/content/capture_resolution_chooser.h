// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_CONTENT_CAPTURE_RESOLUTION_CHOOSER_H_
#define MEDIA_CAPTURE_CONTENT_CAPTURE_RESOLUTION_CHOOSER_H_

#include <vector>

#include "media/capture/capture_export.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// Encapsulates the logic that determines the capture frame resolution based on:
//   1. The configured minimum/maximum frame resolution.
//   2. Whether the capture frame resolution must be of a fixed aspect ratio.
//   3. Changes to resolution of the source content.
//   4. Changes to the (externally-computed) target data volume, provided in
//      terms of the number of pixels in the frame.
//
// In variable-resolution use cases, the capture sizes are "snapped" to a small
// (i.e., usually less than a dozen) set of possibilities.  This is to prevent
// the end-to-end system from having to deal with rapidly-changing video frame
// resolutions that results from providing a fine-grained range of values.  The
// possible snapped frame sizes are computed relative to the resolution of the
// source content: They are the same or smaller in size, and are of the same
// aspect ratio.
class CAPTURE_EXPORT CaptureResolutionChooser {
 public:
  // Default constructor. Capture size is fixed at kDefaultCaptureSize until
  // SetConstraints() is called.
  CaptureResolutionChooser();

  ~CaptureResolutionChooser();

  // Returns the current capture frame resolution to use.
  const gfx::Size& capture_size() const { return capture_size_; }

  // Specifies a new range of acceptable capture resolutions and whether a fixed
  // aspect ratio is required. When |min_frame_size| is equal to
  // |max_frame_size|, capture resolution will be held constant. If a fixed
  // aspect ratio is required, the aspect ratio of |max_frame_size| is used.
  void SetConstraints(const gfx::Size& min_frame_size,
                      const gfx::Size& max_frame_size,
                      bool use_fixed_aspect_ratio);

  // Returns the currently-set source size.
  const gfx::Size& source_size() const { return source_size_; }

  // Updates the capture size based on a change in the resolution of the source
  // content.
  void SetSourceSize(const gfx::Size& source_size);

  // Updates the capture size to target the given frame area, in terms of
  // gfx::Size::GetArea().  The initial target frame area is the maximum int
  // (i.e., always target the source size).
  void SetTargetFrameArea(int area);

  // Search functions to, given a frame |area|, return the nearest snapped frame
  // size, or N size steps up/down.  Snapped frame sizes are based on the
  // current source size.
  gfx::Size FindNearestFrameSize(int area) const;
  gfx::Size FindLargerFrameSize(int area, int num_steps_up) const;
  gfx::Size FindSmallerFrameSize(int area, int num_steps_down) const;

  // The default capture size, if SetConstraints() is never called.
  static constexpr gfx::Size kDefaultCaptureSize = gfx::Size(640, 360);

 private:
  // Called after any update that requires |capture_size_| be re-computed.
  void RecomputeCaptureSize();

  // Recomputes the |snapped_sizes_| cache.
  void UpdateSnappedFrameSizes();

  // Hard constraints.
  gfx::Size min_frame_size_;
  gfx::Size max_frame_size_;

  // If true, adjust the |source_size_| to match the aspect ratio of
  // |max_frame_size_| before computing the snapped frame sizes.
  bool apply_aspect_ratio_adjustment_;

  // Current source size.
  gfx::Size source_size_;

  // |capture_size_| will be computed such that its area is as close to this
  // value as possible.
  int target_area_;

  // The current computed capture frame resolution.
  gfx::Size capture_size_;

  // Cache of the set of possible values |capture_size_| can have, in order from
  // smallest to largest.  This is recomputed whenever UpdateSnappedFrameSizes()
  // is called.
  std::vector<gfx::Size> snapped_sizes_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_CONTENT_CAPTURE_RESOLUTION_CHOOSER_H_
