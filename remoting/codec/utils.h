// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_UTILS_H_
#define REMOTING_CODEC_UTILS_H_

#include <cstdint>

#include "base/containers/span.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

// Gets the bounding rectangle of the provided desktop region.
webrtc::DesktopRect GetBoundingRect(const webrtc::DesktopRegion& region);

// Converts `rect` of an ARGB `frame` into I420 or I444 YUV planes. When
// `is_i444` is false (I420), `rect`'s top-left coordinates must be even.
void ConvertDesktopFrameRectToYuv(const webrtc::DesktopFrame& frame,
                                  const webrtc::DesktopRect& rect,
                                  bool is_i444,
                                  base::span<uint8_t> y_data,
                                  int y_stride,
                                  base::span<uint8_t> u_data,
                                  int u_stride,
                                  base::span<uint8_t> v_data,
                                  int v_stride);

// Widens each rectangle in `region` horizontally to SIMD row-alignment
// boundaries via `GetRowAlignedRect()` (clipped to `frame_width`) and coalesces
// any resulting overlaps into a non-overlapping `DesktopRegion`.
webrtc::DesktopRegion GetRowAlignedRegion(const webrtc::DesktopRegion& region,
                                          int frame_width);

// Minimum row-aligned dirty pixel count (~25% of a 1080p frame) required for
// `ConvertDesktopFrameRegionToYuv()` to parallelize conversion across
// `base::ThreadPool` via `base::CreateJob()`.
inline constexpr int64_t kParallelYuvConversionPixelThreshold = 512000;

// Converts each rectangle in `region` (aligned horizontally to SIMD row
// boundaries via `GetRowAlignedRect()`) from an ARGB `frame` into I420 or I444
// YUV planes. Because `GetRowAlignedRect()` only aligns `left` and `right`,
// when `is_i444` is false (I420), each rectangle in `region` must have an even
// `top()` coordinate (e.g., via `AlignRect()`).
//
// When the coalesced region covers at least
// `kParallelYuvConversionPixelThreshold` pixels and multiple CPU workers are
// available in `base::ThreadPool`, conversion is partitioned across dynamic
// work-stealing horizontal strips via `base::CreateJob(...).Join()` before
// returning synchronously; under `ThreadPool` contention, the calling thread
// drains remaining strips inline without stalling on queued workers.
void ConvertDesktopFrameRegionToYuv(const webrtc::DesktopFrame& frame,
                                    const webrtc::DesktopRegion& region,
                                    bool is_i444,
                                    base::span<uint8_t> y_data,
                                    int y_stride,
                                    base::span<uint8_t> u_data,
                                    int u_stride,
                                    base::span<uint8_t> v_data,
                                    int v_stride);

}  // namespace remoting

#endif  // REMOTING_CODEC_UTILS_H_
