// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/packed_yuv_utils.h"

#include "base/check_op.h"
#include "base/containers/heap_array.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/video_frame.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"

namespace media {

namespace {

// AYUV, Y210 and Y410 are all 32bpp formats in storage.
constexpr size_t kBytesPerPixel = 4;

// P210/P410 and Y210 keep their 10 bits in the most significant bits of a
// 16-bit word. Passing depth=16 to the libyuv helpers therefore means "already
// msb aligned": the 16-bit merges shift by 0, and the XR30 merge shifts right
// by 6 to reach 10-bit fields.
constexpr int kMsbAlignedDepth = 16;

// Stride in 16-bit elements, which is what the libyuv _16 helpers expect. The
// plane strides of the formats handled here are always even.
int ElementStride16(const VideoFrame& frame, size_t plane) {
  const int byte_stride = frame.stride(plane);
  CHECK_EQ(byte_stride % 2, 0);
  return byte_stride / 2;
}

}  // namespace

DXGIFramePacker::DXGIFramePacker() = default;
DXGIFramePacker::~DXGIFramePacker() = default;

VideoPixelFormat GetPackedDxgiSourceFormat(DXGI_FORMAT format) {
  switch (format) {
    case DXGI_FORMAT_AYUV:
      return PIXEL_FORMAT_I444;
    case DXGI_FORMAT_Y210:
      return PIXEL_FORMAT_P210LE;
    case DXGI_FORMAT_Y410:
      return PIXEL_FORMAT_P410LE;
    default:
      return PIXEL_FORMAT_UNKNOWN;
  }
}

size_t GetPackedDxgiRowBytes(DXGI_FORMAT format, int width) {
  if (width <= 0 || GetPackedDxgiSourceFormat(format) == PIXEL_FORMAT_UNKNOWN) {
    return 0;
  }
  base::CheckedNumeric<size_t> row_bytes = base::checked_cast<size_t>(width);
  row_bytes *= kBytesPerPixel;
  return row_bytes.ValueOrDefault(0);
}

// I444 -> AYUV. libyuv's ARGB is B, G, R, A in ascending byte order and AYUV is
// V, U, Y, A, so V, U and Y take the B, G and R channels.
void DXGIFramePacker::PackI444ToAYUV(const VideoFrame& src,
                                     base::span<uint8_t> dst,
                                     size_t dst_stride) {
  const gfx::Size& size = src.visible_rect().size();
  libyuv::MergeARGBPlane(src.GetVisiblePlaneData(VideoFrame::Plane::kY).data(),
                         src.stride(VideoFrame::Plane::kY),
                         src.GetVisiblePlaneData(VideoFrame::Plane::kU).data(),
                         src.stride(VideoFrame::Plane::kU),
                         src.GetVisiblePlaneData(VideoFrame::Plane::kV).data(),
                         src.stride(VideoFrame::Plane::kV),
                         /*src_a=*/nullptr, /*src_stride_a=*/0, dst.data(),
                         base::checked_cast<int>(dst_stride), size.width(),
                         size.height());
}

// P210LE -> Y210. Reading the UV plane as a flat 16-bit stream yields
// U0, V0, U1, V1..., so interleaving it with the Y plane produces the
// Y0, U, Y1, V macro-pixels Y210 wants. Both formats define the 10 bits in the
// high bits of a 16-bit word with the low 6 bits zero, so the words copy across
// unchanged. MergeUVPlane_16() is repurposed as the two-plane interleaver that
// does this.
void DXGIFramePacker::PackP210ToY210(const VideoFrame& src,
                                     base::span<uint8_t> dst,
                                     size_t dst_stride) {
  const gfx::Size& size = src.visible_rect().size();
  auto y = base::subtle::reinterpret_span<const uint16_t>(
      src.GetVisiblePlaneData(VideoFrame::Plane::kY));
  auto uv = base::subtle::reinterpret_span<const uint16_t>(
      src.GetVisiblePlaneData(VideoFrame::Plane::kUV));
  const int y_stride = ElementStride16(src, VideoFrame::Plane::kY);
  const int uv_stride = ElementStride16(src, VideoFrame::Plane::kUV);
  // The destination is addressed in 16-bit elements, so every row must start
  // 16-bit aligned: reinterpret_span() CHECKs the buffer start and the stride
  // check covers the rows that follow it.
  CHECK_EQ(dst_stride % 2, 0u);
  auto dst_u16 = base::subtle::reinterpret_span<uint16_t>(dst);
  libyuv::MergeUVPlane_16(y.data(), y_stride, uv.data(), uv_stride,
                          dst_u16.data(),
                          base::checked_cast<int>(dst_stride / 2), size.width(),
                          size.height(), kMsbAlignedDepth);
}

// P410LE -> Y410. XR30 packs b, g and r into bits 0-9, 10-19 and 20-29 with the
// top two bits set, which is U, Y, V and opaque alpha for Y410. MergeXR30Plane
// needs separate U and V planes, so the interleaved chroma is split row by row.
// Splitting a row at a time is deliberate: the scratch stays cache resident
// between the split and the merge, where whole-plane scratch would not.
void DXGIFramePacker::PackP410ToY410(const VideoFrame& src,
                                     base::span<uint8_t> dst,
                                     size_t dst_stride) {
  const gfx::Size& size = src.visible_rect().size();
  const size_t row_samples = base::checked_cast<size_t>(size.width());
  if (scratch_.size() < row_samples * 2) {
    scratch_ = base::HeapArray<uint16_t>::Uninit(row_samples * 2);
  }
  uint16_t* u_row = scratch_.data();
  uint16_t* v_row = scratch_.subspan(row_samples).data();

  auto y = base::subtle::reinterpret_span<const uint16_t>(
      src.GetVisiblePlaneData(VideoFrame::Plane::kY));
  auto uv = base::subtle::reinterpret_span<const uint16_t>(
      src.GetVisiblePlaneData(VideoFrame::Plane::kUV));
  const int y_stride = ElementStride16(src, VideoFrame::Plane::kY);
  const int uv_stride = ElementStride16(src, VideoFrame::Plane::kUV);

  for (int row = 0; row < size.height(); ++row) {
    libyuv::SplitUVPlane_16(
        uv.subspan(base::checked_cast<size_t>(row) * uv_stride).data(),
        /*src_stride_uv=*/0, u_row, /*dst_stride_u=*/0, v_row,
        /*dst_stride_v=*/0, size.width(), /*height=*/1, kMsbAlignedDepth);
    libyuv::MergeXR30Plane(
        v_row, /*src_stride_r=*/0,
        y.subspan(base::checked_cast<size_t>(row) * y_stride).data(),
        /*src_stride_g=*/0, u_row, /*src_stride_b=*/0,
        dst.subspan(base::checked_cast<size_t>(row) * dst_stride).data(),
        /*dst_stride_ar30=*/0, size.width(), /*height=*/1, kMsbAlignedDepth);
  }
}

bool DXGIFramePacker::Pack(const VideoFrame& src_frame,
                           DXGI_FORMAT dst_format,
                           base::span<uint8_t> dst,
                           size_t dst_stride) {
  const VideoPixelFormat source_format = GetPackedDxgiSourceFormat(dst_format);
  if (source_format == PIXEL_FORMAT_UNKNOWN ||
      src_frame.format() != source_format || !src_frame.HasDirectCpuAccess()) {
    return false;
  }

  const gfx::Size& size = src_frame.visible_rect().size();
  // A Y210 macro-pixel spans two columns.
  if (dst_format == DXGI_FORMAT_Y210 && size.width() % 2 != 0) {
    return false;
  }

  const size_t row_bytes = GetPackedDxgiRowBytes(dst_format, size.width());
  // libyuv takes strides as int, and the row stride cannot be smaller than a
  // row. Neither can happen for encoder-sized frames.
  CHECK_GE(dst_stride, row_bytes);
  CHECK(base::IsValueInRangeForNumericType<int>(dst_stride));
  base::CheckedNumeric<size_t> required = dst_stride;
  required *= base::checked_cast<size_t>(size.height()) - 1u;
  required += row_bytes;
  if (!required.IsValid() || dst.size() < required.ValueOrDie()) {
    return false;
  }

  switch (dst_format) {
    case DXGI_FORMAT_AYUV:
      PackI444ToAYUV(src_frame, dst, dst_stride);
      return true;
    case DXGI_FORMAT_Y210:
      PackP210ToY210(src_frame, dst, dst_stride);
      return true;
    case DXGI_FORMAT_Y410:
      PackP410ToY410(src_frame, dst, dst_stride);
      return true;
    default:
      NOTREACHED();  // GetPackedDxgiSourceFormat() rejected |dst_format|.
  }
}

}  // namespace media
