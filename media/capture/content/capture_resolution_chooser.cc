// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/content/capture_resolution_chooser.h"

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "media/base/video_util.h"

namespace media {

namespace {

// Each snapped frame size is an integer multiple of this many lines apart.
// This is ideal for 16:9 content, but seems to also work well for many
// arbitrary aspect ratios.
const int kSnappedHeightStep = 90;

// The minimum amount of decrease in area between consecutive snapped frame
// sizes.  This matters externally, where the end-to-end system is hunting for a
// capture size that works within all resource bottlenecks.  If the snapped
// frame sizes are too-close together, the end-to-end system cannot stabilize.
// If they are too-far apart, quality is being sacrificed.
const int kMinAreaDecreasePercent = 15;

// Returns |size|, unless it exceeds |max_size| or is under |min_size|.  When
// the bounds are exceeded, computes and returns an alternate size of similar
// aspect ratio that is within the bounds.
gfx::Size ComputeBoundedCaptureSize(const gfx::Size& size,
                                    const gfx::Size& min_size,
                                    const gfx::Size& max_size) {
  if (size.width() > max_size.width() || size.height() > max_size.height()) {
    gfx::Size result = ScaleSizeToFitWithinTarget(size, max_size);
    result.SetToMax(min_size);
    return result;
  } else if (size.width() < min_size.width() ||
             size.height() < min_size.height()) {
    gfx::Size result = ScaleSizeToEncompassTarget(size, min_size);
    result.SetToMin(max_size);
    return result;
  } else {
    return size;
  }
}

// Returns true if the area of |a| is less than that of |b|.
bool CompareByArea(const gfx::Size& a, const gfx::Size& b) {
  return a.GetArea() < b.GetArea();
}

}  // namespace

// static
constexpr gfx::Size CaptureResolutionChooser::kDefaultCaptureSize;

CaptureResolutionChooser::CaptureResolutionChooser()
    : min_frame_size_(kDefaultCaptureSize),
      max_frame_size_(kDefaultCaptureSize),
      apply_aspect_ratio_adjustment_(false),
      target_area_(std::numeric_limits<decltype(target_area_)>::max()),
      capture_size_(kDefaultCaptureSize),
      snapped_sizes_({kDefaultCaptureSize}) {}

CaptureResolutionChooser::~CaptureResolutionChooser() = default;

void CaptureResolutionChooser::SetConstraints(const gfx::Size& min_frame_size,
                                              const gfx::Size& max_frame_size,
                                              bool use_fixed_aspect_ratio) {
  DCHECK_LT(0, min_frame_size.width());
  DCHECK_LT(0, min_frame_size.height());
  DCHECK_LE(min_frame_size.width(), max_frame_size.width());
  DCHECK_LE(min_frame_size.height(), max_frame_size.height());

  min_frame_size_ = min_frame_size;
  max_frame_size_ = max_frame_size;
  apply_aspect_ratio_adjustment_ =
      (min_frame_size != max_frame_size && use_fixed_aspect_ratio);

  UpdateSnappedFrameSizes();
  RecomputeCaptureSize();
}

void CaptureResolutionChooser::SetSourceSize(const gfx::Size& source_size) {
  source_size_ = source_size;
  UpdateSnappedFrameSizes();
  RecomputeCaptureSize();
}

void CaptureResolutionChooser::SetTargetFrameArea(int area) {
  DCHECK_GE(area, 0);
  target_area_ = area;
  RecomputeCaptureSize();
}

gfx::Size CaptureResolutionChooser::FindNearestFrameSize(int area) const {
  const auto begin = snapped_sizes_.begin();
  const auto end = snapped_sizes_.end();
  DCHECK(begin != end);
  const gfx::Size area_as_size(area, 1);  // A facade for CompareByArea().
  const auto p = std::lower_bound(begin, end, area_as_size, &CompareByArea);
  if (p == end) {
    // Boundary case: The target |area| is greater than or equal to the
    // largest, so the largest size is closest.
    return *(end - 1);
  } else if (p == begin) {
    // Boundary case: The target |area| is smaller than the smallest, so the
    // smallest size is closest.
    return *begin;
  } else {
    // |p| points to the smallest size whose area is greater than or equal to
    // the target |area|.  The next smaller size could be closer to the target
    // |area|, so it must also be considered.
    const auto q = p - 1;
    return ((p->GetArea() - area) < (area - q->GetArea())) ? *p : *q;
  }
}

gfx::Size CaptureResolutionChooser::FindLargerFrameSize(
    int area,
    int num_steps_up) const {
  DCHECK_GT(num_steps_up, 0);
  const auto begin = snapped_sizes_.begin();
  const auto end = snapped_sizes_.end();
  DCHECK(begin != end);
  const gfx::Size area_as_size(area, 1);  // A facade for CompareByArea().
  auto p = std::upper_bound(begin, end, area_as_size, &CompareByArea);
  // |p| is already pointing one step up.
  const int additional_steps_up = num_steps_up - 1;
  if ((end - p) > additional_steps_up)
    return *(p + additional_steps_up);
  else
    return *(end - 1);
}

gfx::Size CaptureResolutionChooser::FindSmallerFrameSize(
    int area,
    int num_steps_down) const {
  DCHECK_GT(num_steps_down, 0);
  const auto begin = snapped_sizes_.begin();
  const auto end = snapped_sizes_.end();
  DCHECK(begin != end);
  const gfx::Size area_as_size(area, 1);  // A facade for CompareByArea().
  const auto p = std::lower_bound(begin, end, area_as_size, &CompareByArea);
  if ((p - begin) >= num_steps_down)
    return *(p - num_steps_down);
  else
    return *begin;
}

void CaptureResolutionChooser::RecomputeCaptureSize() {
  const gfx::Size old_capture_size = capture_size_;
  capture_size_ = FindNearestFrameSize(target_area_);
  VLOG_IF(1, capture_size_ != old_capture_size)
      << "Recomputed capture size from " << old_capture_size.ToString()
      << " to " << capture_size_.ToString() << " ("
      << (100.0 * capture_size_.height() / snapped_sizes_.back().height())
      << "% of ideal size)";
}

void CaptureResolutionChooser::UpdateSnappedFrameSizes() {
  // Compute the largest snapped frame size, and the one that will determine the
  // aspect ratio of all the snapped frame sizes. If the |source_size_| has not
  // yet been set, use the |capture_size_| as a substitute.
  gfx::Size constrained_size =
      source_size_.IsEmpty() ? capture_size_ : source_size_;
  constrained_size = ComputeBoundedCaptureSize(
      apply_aspect_ratio_adjustment_
          ? PadToMatchAspectRatio(constrained_size, max_frame_size_)
          : constrained_size,
      min_frame_size_, max_frame_size_);

  // Start with the constrained size as the largest in the set.
  snapped_sizes_.clear();
  snapped_sizes_.push_back(constrained_size);

  // Repeatedly decrease the size in steps, adding each to |snapped_sizes_|.
  // However, skip the sizes that do not decrease in area by enough, relative to
  // the prior size.
  int last_area = constrained_size.GetArea();
  for (int height = constrained_size.height() - kSnappedHeightStep;
       height >= min_frame_size_.height(); height -= kSnappedHeightStep) {
    const int width =
        height * constrained_size.width() / constrained_size.height();
    if (width < min_frame_size_.width())
      break;
    const int smaller_area = width * height;
    const int percent_decrease = 100 * (last_area - smaller_area) / last_area;
    if (percent_decrease >= kMinAreaDecreasePercent) {
      snapped_sizes_.push_back(gfx::Size(width, height));
      last_area = smaller_area;
    }
  }

  // Reverse ordering, so that sizes are from smallest to largest.
  std::reverse(snapped_sizes_.begin(), snapped_sizes_.end());

  if (VLOG_IS_ON(1)) {
    std::vector<std::string> stringified_sizes;
    for (const gfx::Size& size : snapped_sizes_)
      stringified_sizes.push_back(size.ToString());
    VLOG_STREAM(1) << "Recomputed snapped frame sizes: "
                   << base::JoinString(stringified_sizes, " <--> ");
  }
}

}  // namespace media
