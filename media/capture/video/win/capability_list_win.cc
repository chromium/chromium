// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/capability_list_win.h"

#include <algorithm>
#include <functional>

#include "base/check.h"
#include "media/capture/video_capture_types.h"

#include "base/logging.h"
namespace media {

namespace {

// Compares the priority of the capture formats. Returns true if |lhs| is the
// preferred capture format in comparison with |rhs|. Returns false otherwise.
bool CompareCapability(const VideoCaptureFormat& requested,
                       const CapabilityWin& lhs,
                       const CapabilityWin& rhs) {
  // When 16-bit format or NV12 is requested and available, avoid other formats.
  // If both lhs and rhs are 16-bit, we still need to compare them based on
  // height, width and frame rate.
  const bool use_requested =
      (requested.pixel_format == media::PIXEL_FORMAT_Y16) ||
      (requested.pixel_format == media::PIXEL_FORMAT_NV12);
  if (use_requested &&
      lhs.supported_format.pixel_format != rhs.supported_format.pixel_format) {
    if (lhs.supported_format.pixel_format == requested.pixel_format)
      return true;
    if (rhs.supported_format.pixel_format == requested.pixel_format)
      return false;
  }
  const int diff_height_lhs = std::abs(
      lhs.supported_format.frame_size.height() - requested.frame_size.height());
  const int diff_height_rhs = std::abs(
      rhs.supported_format.frame_size.height() - requested.frame_size.height());
  if (diff_height_lhs != diff_height_rhs)
    return diff_height_lhs < diff_height_rhs;

  const int diff_width_lhs = std::abs(lhs.supported_format.frame_size.width() -
                                      requested.frame_size.width());
  const int diff_width_rhs = std::abs(rhs.supported_format.frame_size.width() -
                                      requested.frame_size.width());
  if (diff_width_lhs != diff_width_rhs)
    return diff_width_lhs < diff_width_rhs;

  const float diff_fps_lhs =
      std::fabs(lhs.supported_format.frame_rate - requested.frame_rate);
  const float diff_fps_rhs =
      std::fabs(rhs.supported_format.frame_rate - requested.frame_rate);
  if (diff_fps_lhs != diff_fps_rhs)
    return diff_fps_lhs < diff_fps_rhs;

  // Compare by internal pixel format to avoid conversions when possible.
  if (lhs.source_pixel_format != rhs.source_pixel_format) {
    // Deprioritize fake formats in passthrough mode to avoid extra conversions.
    // But do so only if the requested format is I420, because
    // MFCaptureEngine doesn't support MJPG->I420 conversion, so fake NV12 is
    // at least working.
    if (requested.pixel_format == media::PIXEL_FORMAT_NV12 &&
        lhs.maybe_fake ^ rhs.maybe_fake) {
      return rhs.maybe_fake;
    }
    // Choose the format with no conversion if possible.
    if (lhs.source_pixel_format == requested.pixel_format)
      return true;
    if (rhs.source_pixel_format == requested.pixel_format)
      return false;
    // Prefer I420<->NV12 conversion over all.
    if ((lhs.source_pixel_format == PIXEL_FORMAT_NV12 &&
         requested.pixel_format == PIXEL_FORMAT_I420) ||
        (lhs.source_pixel_format == PIXEL_FORMAT_I420 &&
         requested.pixel_format == PIXEL_FORMAT_NV12)) {
      return true;
    }
    if ((rhs.source_pixel_format == PIXEL_FORMAT_NV12 &&
         requested.pixel_format == PIXEL_FORMAT_I420) ||
        (rhs.source_pixel_format == PIXEL_FORMAT_I420 &&
         requested.pixel_format == PIXEL_FORMAT_NV12)) {
      return false;
    }
    // YUY2<->NV12 is the next best.
    if ((lhs.source_pixel_format == PIXEL_FORMAT_NV12 &&
         requested.pixel_format == PIXEL_FORMAT_YUY2) ||
        (lhs.source_pixel_format == PIXEL_FORMAT_YUY2 &&
         requested.pixel_format == PIXEL_FORMAT_NV12)) {
      return true;
    }
    if ((rhs.source_pixel_format == PIXEL_FORMAT_NV12 &&
         requested.pixel_format == PIXEL_FORMAT_YUY2) ||
        (rhs.source_pixel_format == PIXEL_FORMAT_YUY2 &&
         requested.pixel_format == PIXEL_FORMAT_NV12)) {
      return false;
    }
  }

  // Always prefer non-fake format over the same mjpg-backed.
  if (lhs.source_pixel_format == rhs.source_pixel_format &&
      (lhs.maybe_fake ^ rhs.maybe_fake)) {
    return rhs.maybe_fake;
  }

  return VideoCaptureFormat::ComparePixelFormatPreference(
      lhs.supported_format.pixel_format, rhs.supported_format.pixel_format);
}

}  // namespace

const CapabilityWin& GetBestMatchedCapability(
    const VideoCaptureFormat& requested,
    const CapabilityList& capabilities) {
  DCHECK(!capabilities.empty());
  const CapabilityWin* best_match = &(*capabilities.begin());
  for (const CapabilityWin& capability : capabilities) {
    if (CompareCapability(requested, capability, *best_match)) {
      best_match = &capability;
    }
  }
  return *best_match;
}

}  // namespace media
