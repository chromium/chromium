// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video_capture_types.h"

#include <ostream>

#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "media/base/limits.h"

namespace media {

// This list is ordered by precedence of use.
static VideoPixelFormat const kSupportedCapturePixelFormats[] = {
    PIXEL_FORMAT_I420,  PIXEL_FORMAT_YV12, PIXEL_FORMAT_NV12,
    PIXEL_FORMAT_NV21,  PIXEL_FORMAT_UYVY, PIXEL_FORMAT_YUY2,
    PIXEL_FORMAT_RGB24, PIXEL_FORMAT_ARGB, PIXEL_FORMAT_MJPEG,
};

VideoCaptureFormat::VideoCaptureFormat()
    : frame_rate(0.0f), pixel_format(PIXEL_FORMAT_UNKNOWN) {}

VideoCaptureFormat::VideoCaptureFormat(const gfx::Size& frame_size,
                                       float frame_rate,
                                       VideoPixelFormat pixel_format)
    : frame_size(frame_size),
      frame_rate(frame_rate),
      pixel_format(pixel_format) {}

bool VideoCaptureFormat::IsValid() const {
  return (frame_size.width() <= media::limits::kMaxDimension) &&
         (frame_size.height() <= media::limits::kMaxDimension) &&
         (frame_size.GetArea() >= 0) &&
         (frame_size.GetArea() <= media::limits::kMaxCanvas) &&
         (frame_rate >= 0.0f) &&
         (frame_rate <= media::limits::kMaxFramesPerSecond) &&
         (pixel_format >= PIXEL_FORMAT_UNKNOWN &&
          pixel_format <= PIXEL_FORMAT_MAX);
}

// static
std::string VideoCaptureFormat::ToString(const VideoCaptureFormat& format) {
  // Beware: This string is parsed by manager.js:parseVideoCaptureFormat_,
  // take care when changing the formatting.
  return base::StringPrintf(
      "(%s)@%.3ffps, pixel format: %s", format.frame_size.ToString().c_str(),
      format.frame_rate, VideoPixelFormatToString(format.pixel_format).c_str());
}

// static
bool VideoCaptureFormat::ComparePixelFormatPreference(
    const VideoPixelFormat& lhs,
    const VideoPixelFormat& rhs) {
  auto* format_lhs = base::ranges::find(kSupportedCapturePixelFormats, lhs);
  auto* format_rhs = base::ranges::find(kSupportedCapturePixelFormats, rhs);
  return format_lhs < format_rhs;
}

VideoCaptureParams::VideoCaptureParams()
    : buffer_type(VideoCaptureBufferType::kSharedMemory),
      resolution_change_policy(ResolutionChangePolicy::FIXED_RESOLUTION),
      power_line_frequency(PowerLineFrequency::kDefault) {}

bool VideoCaptureParams::IsValid() const {
  return requested_format.IsValid() &&
         resolution_change_policy >= ResolutionChangePolicy::FIXED_RESOLUTION &&
         resolution_change_policy <= ResolutionChangePolicy::LAST &&
         power_line_frequency >= PowerLineFrequency::kDefault &&
         power_line_frequency <= PowerLineFrequency::k60Hz;
}

std::string VideoCaptureParams::SuggestedConstraints::ToString() const {
  return base::StrCat(
      {"min = ", min_frame_size.ToString(),
       ", max = ", max_frame_size.ToString(),
       ", fixed_aspect_ratio = ", fixed_aspect_ratio ? "true" : "false"});
}

VideoCaptureParams::SuggestedConstraints
VideoCaptureParams::SuggestConstraints() const {
  // The requested frame size is always the maximum frame size. Ensure that it
  // rounds to even numbers (to match I420 chroma sample sizes).
  gfx::Size max_frame_size = requested_format.frame_size;
  if (max_frame_size.width() % 2 != 0)
    max_frame_size.set_width(max_frame_size.width() - 1);
  if (max_frame_size.height() % 2 != 0)
    max_frame_size.set_height(max_frame_size.height() - 1);

  // Compute the minimum frame size as a function of the maximum frame size and
  // policy.
  gfx::Size min_frame_size;
  switch (resolution_change_policy) {
    case ResolutionChangePolicy::FIXED_RESOLUTION:
      min_frame_size = max_frame_size;
      break;

    case ResolutionChangePolicy::FIXED_ASPECT_RATIO: {
      constexpr int kMinLines = 180;
      if (max_frame_size.height() <= kMinLines) {
        min_frame_size = max_frame_size;
      } else {
        const double ideal_width = static_cast<double>(kMinLines) *
                                   max_frame_size.width() /
                                   max_frame_size.height();
        // Round |ideal_width| to the nearest even whole number.
        const int even_width = static_cast<int>(ideal_width / 2.0 + 0.5) * 2;
        min_frame_size = gfx::Size(even_width, kMinLines);
        if (min_frame_size.width() <= 0 ||
            min_frame_size.width() > max_frame_size.width()) {
          min_frame_size = max_frame_size;
        }
      }
      break;
    }

    case ResolutionChangePolicy::ANY_WITHIN_LIMIT:
      if (!max_frame_size.IsEmpty())
        min_frame_size = gfx::Size(2, 2);
      break;
  }
  DCHECK(min_frame_size.width() % 2 == 0);
  DCHECK(min_frame_size.height() % 2 == 0);

  return SuggestedConstraints{
      min_frame_size, max_frame_size,
      resolution_change_policy == ResolutionChangePolicy::FIXED_ASPECT_RATIO};
}

std::ostream& operator<<(
    std::ostream& os,
    const VideoCaptureParams::SuggestedConstraints& constraints) {
  return os << constraints.ToString();
}

}  // namespace media
