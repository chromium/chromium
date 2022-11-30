// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_feedback.h"

#include <cmath>

#include "base/containers/contains.h"
#include "base/logging.h"

namespace media {

namespace {

// Arbitrary limit above what is considered a reasonable request.
constexpr size_t kCombinedMappedSizesCountLimit = 6;

void SortSizesDescending(std::vector<gfx::Size>& sizes) {
  std::sort(sizes.begin(), sizes.end(),
            [](gfx::Size& a, gfx::Size& b) { return a.height() > b.height(); });
}

}  // namespace

VideoCaptureFeedback::VideoCaptureFeedback() = default;
VideoCaptureFeedback::VideoCaptureFeedback(const VideoCaptureFeedback& other) =
    default;

VideoCaptureFeedback::VideoCaptureFeedback(double resource_utilization,
                                           float max_framerate_fps,
                                           int max_pixels)
    : resource_utilization(resource_utilization),
      max_framerate_fps(max_framerate_fps),
      max_pixels(max_pixels) {}

VideoCaptureFeedback::~VideoCaptureFeedback() = default;

void VideoCaptureFeedback::Combine(const VideoCaptureFeedback& other) {
  // Take maximum of non-negative and finite |resource_utilization| values.
  if (other.resource_utilization >= 0 &&
      std::isfinite(other.resource_utilization)) {
    if (!std::isfinite(resource_utilization) ||
        resource_utilization < other.resource_utilization) {
      resource_utilization = other.resource_utilization;
    }
  }

  // Take minimum non-negative max_pixels value to satisfy both constraints.
  if (other.max_pixels > 0 &&
      (max_pixels <= 0 || max_pixels > other.max_pixels)) {
    max_pixels = other.max_pixels;
  }

  // Take minimum of non-negative max_framerate_fps.
  if (other.max_framerate_fps >= 0.0 &&
      (max_framerate_fps < 0.0 || max_framerate_fps > other.max_framerate_fps))
    max_framerate_fps = other.max_framerate_fps;

  // If any consumer wants mapped frames, all of them should get it.
  require_mapped_frame |= other.require_mapped_frame;

  // Merge mapped sizes for all consumers.
  for (const gfx::Size& mapped_size : other.mapped_sizes) {
    // Skip duplicates.
    if (base::Contains(mapped_sizes, mapped_size)) {
      continue;
    }
    // As a safety measure, limit the number of sizes that can be asked for.
    if (mapped_sizes.size() >= kCombinedMappedSizesCountLimit) {
      LOG(WARNING) << "Consumer mapped sizes count exceeds "
                   << kCombinedMappedSizesCountLimit;
      break;
    }
    mapped_sizes.push_back(mapped_size);
  }
  SortSizesDescending(mapped_sizes);
}

bool VideoCaptureFeedback::Empty() const {
  return !std::isfinite(max_framerate_fps) &&
         max_pixels == std::numeric_limits<int>::max() &&
         (resource_utilization < 0.0) && !require_mapped_frame &&
         mapped_sizes.empty();
}

VideoCaptureFeedback& VideoCaptureFeedback::WithUtilization(float utilization) {
  resource_utilization = utilization;
  return *this;
}

VideoCaptureFeedback& VideoCaptureFeedback::WithMaxFramerate(
    float framerate_fps) {
  max_framerate_fps = framerate_fps;
  return *this;
}

VideoCaptureFeedback& VideoCaptureFeedback::WithMaxPixels(int pixels) {
  max_pixels = pixels;
  return *this;
}

VideoCaptureFeedback& VideoCaptureFeedback::RequireMapped(bool require) {
  require_mapped_frame = require;
  return *this;
}

VideoCaptureFeedback& VideoCaptureFeedback::WithMappedSizes(
    std::vector<gfx::Size> sizes) {
  mapped_sizes = std::move(sizes);
  SortSizesDescending(mapped_sizes);
  return *this;
}

}  // namespace media
