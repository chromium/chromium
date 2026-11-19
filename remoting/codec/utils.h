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

// Minimum pixel count (~25% of a 1080p frame) required for YUV<->RGB
// conversion helpers to parallelize across `base::ThreadPool` via
// `base::CreateJob()`.
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

// Converts an ARGB `frame` into NV12 (`y_data` luma plane and interleaved UV
// `uv_data` chroma plane). Empty frames (`frame.size().is_empty()`) are a
// no-op. For non-empty frames, `CHECK`s that `frame.pixel_format()` is
// `webrtc::FOURCC_ARGB`, `y_stride >= width`, `uv_stride >= 2 * ((width + 1) /
// 2)`, and each plane span covers at least `stride * (rows - 1) + row_bytes`
// bytes (so tight visible-plane spans from
// `media::VideoFrame::GetWritableVisiblePlaneData()` are sufficient).
//
// When `frame` covers at least `kParallelYuvConversionPixelThreshold` pixels
// and multiple CPU workers are available in `base::ThreadPool`, conversion is
// partitioned across dynamic work-stealing horizontal strips via
// `base::CreateJob(...).Join()` before returning synchronously; under
// `ThreadPool` contention, the calling thread drains remaining strips inline
// without stalling on queued workers.
void ConvertDesktopFrameToNv12(const webrtc::DesktopFrame& frame,
                               base::span<uint8_t> y_data,
                               int y_stride,
                               base::span<uint8_t> uv_data,
                               int uv_stride);

// Converts planar 8-bit I420 (`is_i444 == false`) or I444 (`is_i444 == true`)
// YUV planes into `rgb_frame` using `output_fourcc` (`webrtc::FOURCC_ARGB` for
// BGRA byte order in memory, or `webrtc::FOURCC_ABGR` for RGBA byte order in
// memory). Empty frames (`rgb_frame.size().is_empty()`) are a no-op. For
// non-empty frames, `CHECK`s that `rgb_frame.data()` is non-null,
// `output_fourcc` is `FOURCC_ARGB` or `FOURCC_ABGR`, `rgb_frame.stride() >=
// width * kBytesPerPixel`, `y_stride >= width`, `u_stride` and `v_stride >=
// uv_width`, and each Y/U/V span covers at least `stride * (rows - 1) +
// row_bytes` bytes.
//
// When `rgb_frame` covers at least `kParallelYuvConversionPixelThreshold`
// pixels and multiple CPU workers are available in `base::ThreadPool`,
// conversion is partitioned across dynamic work-stealing horizontal strips via
// `base::CreateJob(...).Join()` before returning synchronously; under
// `ThreadPool` contention, the calling thread drains remaining strips inline
// without stalling on queued workers.
void ConvertYuvToDesktopFrame(base::span<const uint8_t> y_data,
                              int y_stride,
                              base::span<const uint8_t> u_data,
                              int u_stride,
                              base::span<const uint8_t> v_data,
                              int v_stride,
                              bool is_i444,
                              webrtc::FourCC output_fourcc,
                              webrtc::DesktopFrame& rgb_frame);

// Overrides the `(NumberOfProcessors() + 1) / 2` worker cap in
// `GetMaxParallelWorkers()` so unit tests exercise the multi-strip
// `base::CreateJob()` path deterministically even on 1- or 2-vCPU bots.
class ScopedParallelConversionCpuCapForTesting {
 public:
  explicit ScopedParallelConversionCpuCapForTesting(int max_workers);
  ScopedParallelConversionCpuCapForTesting(
      const ScopedParallelConversionCpuCapForTesting&) = delete;
  ScopedParallelConversionCpuCapForTesting& operator=(
      const ScopedParallelConversionCpuCapForTesting&) = delete;
  ~ScopedParallelConversionCpuCapForTesting();

 private:
  const int previous_override_;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_UTILS_H_
