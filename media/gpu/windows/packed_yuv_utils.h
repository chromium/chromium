// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_PACKED_YUV_UTILS_H_
#define MEDIA_GPU_WINDOWS_PACKED_YUV_UTILS_H_

#include <dxgi.h>
#include <stddef.h>
#include <stdint.h>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "media/base/video_types.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

class VideoFrame;

// Helpers for writing the packed DXGI YUV layouts. DXGI offers multi-plane
// formats only for 4:2:0, so 4:2:2 and 4:4:4 surfaces are packed, and
// VideoFrameConverter cannot produce them: it emits planar and bi-planar
// formats only, and libyuv has no writers for AYUV, Y210 or Y410 either.
// Callers convert to the bi-planar or planar format of matching geometry
// first, then use these helpers for the final interleave.

// Returns the VideoPixelFormat a source frame must be in to be packed into
// |format|, or PIXEL_FORMAT_UNKNOWN if |format| is not supported here. The
// D3D12 encoder's upload path uses this to pick its conversion target.
MEDIA_GPU_EXPORT VideoPixelFormat GetPackedDxgiSourceFormat(DXGI_FORMAT format);

// Returns the number of bytes that one row of |width| pixels occupies in
// |format|, or 0 if |format| is not supported here or |width| is not positive.
// The D3D12 encoder's upload path uses this to size its buffer.
MEDIA_GPU_EXPORT size_t GetPackedDxgiRowBytes(DXGI_FORMAT format, int width);

// Packs planar and bi-planar VideoFrames into the packed DXGI formats. Not
// thread-safe, matching the single encoder sequence it serves.
class MEDIA_GPU_EXPORT DXGIFramePacker {
 public:
  DXGIFramePacker();
  ~DXGIFramePacker();

  // Packs the visible region of |src_frame| into |dst| in the |dst_format|
  // layout, writing |dst_stride| bytes per row. |src_frame| must have direct
  // CPU access and be in GetPackedDxgiSourceFormat(dst_format). CHECKs on
  // destination geometry mismatches, which cannot happen for encoder-sized
  // frames; returns false only when a source precondition does not hold or
  // |dst| is too small.
  bool Pack(const VideoFrame& src_frame,
            DXGI_FORMAT dst_format,
            base::span<uint8_t> dst,
            size_t dst_stride);

 private:
  void PackI444ToAYUV(const VideoFrame& src,
                      base::span<uint8_t> dst,
                      size_t dst_stride);
  void PackP210ToY210(const VideoFrame& src,
                      base::span<uint8_t> dst,
                      size_t dst_stride);
  void PackP410ToY410(const VideoFrame& src,
                      base::span<uint8_t> dst,
                      size_t dst_stride);

  base::HeapArray<uint16_t> scratch_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_PACKED_YUV_UTILS_H_
