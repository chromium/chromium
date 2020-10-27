// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/capability_list_win.h"

#include <algorithm>
#include <functional>

#include "base/check.h"
#include "media/capture/video_capture_types.h"

namespace media {

namespace {

// Compares the priority of the capture formats. Returns true if |lhs| is the
// preferred capture format in comparison with |rhs|. Returns false otherwise.
bool CompareCapability(const VideoCaptureFormat& requested,
                       const VideoCaptureFormat& lhs,
                       const VideoCaptureFormat& rhs) {
  // When 16-bit format or NV12 is requested and available, avoid other formats.
  // If both lhs and rhs are 16-bit, we still need to compare them based on
  // height, width and frame rate.
  const bool use_requested =
      (requested.pixel_format == media::PIXEL_FORMAT_Y16) ||
      (requested.pixel_format == media::PIXEL_FORMAT_NV12);
  if (use_requested && lhs.pixel_format != rhs.pixel_format) {
    if (lhs.pixel_format == requested.pixel_format)
      return true;
    if (rhs.pixel_format == requested.pixel_format)
      return false;
  }
  const int diff_height_lhs =
      std::abs(lhs.frame_size.height() - requested.frame_size.height());
  const int diff_height_rhs =
      std::abs(rhs.frame_size.height() - requested.frame_size.height());
  if (diff_height_lhs != diff_height_rhs)
    return diff_height_lhs < diff_height_rhs;

  const int diff_width_lhs =
      std::abs(lhs.frame_size.width() - requested.frame_size.width());
  const int diff_width_rhs =
      std::abs(rhs.frame_size.width() - requested.frame_size.width());
  if (diff_width_lhs != diff_width_rhs)
    return diff_width_lhs < diff_width_rhs;

  const float diff_fps_lhs = std::fabs(lhs.frame_rate - requested.frame_rate);
  const float diff_fps_rhs = std::fabs(rhs.frame_rate - requested.frame_rate);
  if (diff_fps_lhs != diff_fps_rhs)
    return diff_fps_lhs < diff_fps_rhs;

  return VideoCaptureFormat::ComparePixelFormatPreference(lhs.pixel_format,
                                                          rhs.pixel_format);
}

}  // namespace

const CapabilityWin& GetBestMatchedCapability(
    const VideoCaptureFormat& requested,
    const CapabilityList& capabilities) {
  DCHECK(!capabilities.empty());
  const CapabilityWin* best_match = &(*capabilities.begin());
  for (const CapabilityWin& capability : capabilities) {
    if (CompareCapability(requested, capability.supported_format,
                          best_match->supported_format)) {
      best_match = &capability;
    }
  }
  return *best_match;
}

}  // namespace media
