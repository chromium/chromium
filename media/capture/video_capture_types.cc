// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video_capture_types.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"

namespace media {

// This list is ordered by precedence of use.
static VideoPixelFormat const kSupportedCapturePixelFormats[] = {
    PIXEL_FORMAT_I420, PIXEL_FORMAT_YV12,  PIXEL_FORMAT_NV12,
    PIXEL_FORMAT_NV21, PIXEL_FORMAT_YUY2,  PIXEL_FORMAT_RGB24,
    PIXEL_FORMAT_ARGB, PIXEL_FORMAT_MJPEG,
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
  return (frame_size.width() < media::limits::kMaxDimension) &&
         (frame_size.height() < media::limits::kMaxDimension) &&
         (frame_size.GetArea() >= 0) &&
         (frame_size.GetArea() < media::limits::kMaxCanvas) &&
         (frame_rate >= 0.0f) &&
         (frame_rate < media::limits::kMaxFramesPerSecond) &&
         (pixel_format >= PIXEL_FORMAT_UNKNOWN &&
          pixel_format <= PIXEL_FORMAT_MAX);
}

size_t VideoCaptureFormat::ImageAllocationSize() const {
  return VideoFrame::AllocationSize(pixel_format, frame_size);
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
  auto* format_lhs = std::find(
      kSupportedCapturePixelFormats,
      kSupportedCapturePixelFormats + base::size(kSupportedCapturePixelFormats),
      lhs);
  auto* format_rhs = std::find(
      kSupportedCapturePixelFormats,
      kSupportedCapturePixelFormats + base::size(kSupportedCapturePixelFormats),
      rhs);
  return format_lhs < format_rhs;
}

VideoCaptureParams::VideoCaptureParams()
    : buffer_type(VideoCaptureBufferType::kSharedMemory),
      resolution_change_policy(ResolutionChangePolicy::FIXED_RESOLUTION),
      power_line_frequency(PowerLineFrequency::FREQUENCY_DEFAULT),
      enable_face_detection(false) {}

bool VideoCaptureParams::IsValid() const {
  return requested_format.IsValid() &&
         resolution_change_policy >= ResolutionChangePolicy::FIXED_RESOLUTION &&
         resolution_change_policy <= ResolutionChangePolicy::LAST &&
         power_line_frequency >= PowerLineFrequency::FREQUENCY_DEFAULT &&
         power_line_frequency <= PowerLineFrequency::FREQUENCY_MAX;
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
      // TODO(miu): This is a place-holder until "min constraints" are plumbed-
      // in from the MediaStream framework.  http://crbug.com/473336
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

}  // namespace media
