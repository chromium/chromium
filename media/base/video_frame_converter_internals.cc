// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame_converter_internals.h"

#include <algorithm>
#include <vector>

#include "media/base/video_types.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkM44.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace {

bool IsSupportedRGBFormat(media::VideoPixelFormat format) {
  return format == media::PIXEL_FORMAT_XBGR ||
         format == media::PIXEL_FORMAT_XRGB ||
         format == media::PIXEL_FORMAT_ABGR ||
         format == media::PIXEL_FORMAT_ARGB;
}

const uint16_t* VisibleData16(const media::VideoFrame& frame, size_t plane) {
  return base::subtle::reinterpret_span<const uint16_t>(
             frame.GetVisiblePlaneData(plane))
      .data();
}

uint16_t* WritableVisibleData16(media::VideoFrame& frame, size_t plane) {
  return base::subtle::reinterpret_span<uint16_t>(
             frame.GetWritableVisiblePlaneData(plane))
      .data();
}

int Stride16(const media::VideoFrame& frame, size_t plane) {
  return frame.stride(plane) / sizeof(uint16_t);
}

void Mask12BitMSBPlane(base::span<uint16_t> data,
                       int stride_elements,
                       int width,
                       int height) {
  for (int r = 0; r < height; ++r) {
    for (int c = 0; c < width; ++c) {
      data[r * stride_elements + c] &= 0xFFC0;
    }
  }
}

void Shift12To10Plane(base::span<uint16_t> data,
                      int stride_elements,
                      int width,
                      int height) {
  for (int r = 0; r < height; ++r) {
    for (int c = 0; c < width; ++c) {
      data[r * stride_elements + c] >>= 2;
    }
  }
}

}  // namespace

namespace media::internals {

const libyuv::ArgbConstants* GetArgbConstantsForColorSpace(
    const gfx::ColorSpace& cs,
    bool is_abgr) {
  const bool is_full_range = cs.GetRangeID() == gfx::ColorSpace::RangeID::FULL;

  if (cs.GetMatrixID() == gfx::ColorSpace::MatrixID::BT2020_NCL) {
    if (is_abgr) {
      return is_full_range ? &libyuv::kAbgrV2020Constants
                           : &libyuv::kAbgrU2020Constants;
    }
    return is_full_range ? &libyuv::kArgbV2020Constants
                         : &libyuv::kArgbU2020Constants;
  } else if (cs.GetMatrixID() == gfx::ColorSpace::MatrixID::SMPTE170M) {
    if (is_abgr) {
      return is_full_range ? &libyuv::kAbgrJPEGConstants
                           : &libyuv::kAbgrI601Constants;
    }
    return is_full_range ? &libyuv::kArgbJPEGConstants
                         : &libyuv::kArgbI601Constants;
  } else {
    // Default to BT.709
    if (is_abgr) {
      return is_full_range ? &libyuv::kAbgrF709Constants
                           : &libyuv::kAbgrH709Constants;
    }
    return is_full_range ? &libyuv::kArgbF709Constants
                         : &libyuv::kArgbH709Constants;
  }
}

void CopyVisiblePlanes(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK_EQ(src_frame.format(), dest_frame.format());
  DCHECK_EQ(src_frame.visible_rect().size(), dest_frame.visible_rect().size());
  for (size_t i = 0; i < VideoFrame::NumPlanes(dest_frame.format()); ++i) {
    libyuv::CopyPlane(src_frame.visible_data(i), src_frame.stride(i),
                      dest_frame.GetWritableVisibleData(i),
                      dest_frame.stride(i), dest_frame.GetVisibleRowBytes(i),
                      dest_frame.GetVisibleRows(i));
  }
}

bool ARGBScale(const VideoFrame& src_frame,
               VideoFrame& dest_frame,
               libyuv::FilterMode filter) {
  DCHECK(IsSupportedRGBFormat(src_frame.format()));
  return libyuv::ARGBScale(
             src_frame.visible_data(VideoFrame::Plane::kARGB),
             src_frame.stride(VideoFrame::Plane::kARGB),
             src_frame.visible_rect().width(),
             src_frame.visible_rect().height(),
             dest_frame.GetWritableVisibleData(VideoFrame::Plane::kARGB),
             dest_frame.stride(VideoFrame::Plane::kARGB),
             dest_frame.visible_rect().width(),
             dest_frame.visible_rect().height(), filter) == 0;
}

bool ARGBToI420x(const VideoFrame& src_frame,
                 VideoFrame& dest_frame,
                 const libyuv::ArgbConstants* matrix) {
  DCHECK(IsSupportedRGBFormat(src_frame.format()));
  DCHECK(dest_frame.format() == PIXEL_FORMAT_I420 ||
         dest_frame.format() == PIXEL_FORMAT_I420A);
  if (libyuv::ARGBToI420Matrix(
          src_frame.visible_data(VideoFrame::Plane::kARGB),
          src_frame.stride(VideoFrame::Plane::kARGB),
          dest_frame.GetWritableVisibleData(VideoFrame::Plane::kY),
          dest_frame.stride(VideoFrame::Plane::kY),
          dest_frame.GetWritableVisibleData(VideoFrame::Plane::kU),
          dest_frame.stride(VideoFrame::Plane::kU),
          dest_frame.GetWritableVisibleData(VideoFrame::Plane::kV),
          dest_frame.stride(VideoFrame::Plane::kV), matrix,
          dest_frame.visible_rect().width(),
          dest_frame.visible_rect().height()) != 0) {
    return false;
  }
  if (dest_frame.format() == PIXEL_FORMAT_I420) {
    return true;
  }
  return libyuv::ARGBExtractAlpha(
             src_frame.visible_data(VideoFrame::Plane::kARGB),
             src_frame.stride(VideoFrame::Plane::kARGB),
             dest_frame.GetWritableVisibleData(VideoFrame::Plane::kA),
             dest_frame.stride(VideoFrame::Plane::kA),
             dest_frame.visible_rect().width(),
             dest_frame.visible_rect().height()) == 0;
}

bool ARGBToI422x(const VideoFrame& src_frame,
                 VideoFrame& dest_frame,
                 const libyuv::ArgbConstants* matrix) {
  DCHECK(IsSupportedRGBFormat(src_frame.format()));
  DCHECK(dest_frame.format() == PIXEL_FORMAT_I422 ||
         dest_frame.format() == PIXEL_FORMAT_I422A);
  if (libyuv::ARGBToI422Matrix(
          src_frame.visible_data(VideoFrame::Plane::kARGB),
          src_frame.stride(VideoFrame::Plane::kARGB),
          dest_frame.GetWritableVisibleData(VideoFrame::Plane::kY),
          dest_frame.stride(VideoFrame::Plane::kY),
          dest_frame.GetWritableVisibleData(VideoFrame::Plane::kU),
          dest_frame.stride(VideoFrame::Plane::kU),
          dest_frame.GetWritableVisibleData(VideoFrame::Plane::kV),
          dest_frame.stride(VideoFrame::Plane::kV), matrix,
          dest_frame.visible_rect().width(),
          dest_frame.visible_rect().height()) != 0) {
    return false;
  }
  if (dest_frame.format() == PIXEL_FORMAT_I422) {
    return true;
  }
  return libyuv::ARGBExtractAlpha(
             src_frame.visible_data(VideoFrame::Plane::kARGB),
             src_frame.stride(VideoFrame::Plane::kARGB),
             dest_frame.GetWritableVisibleData(VideoFrame::Plane::kA),
             dest_frame.stride(VideoFrame::Plane::kA),
             dest_frame.visible_rect().width(),
             dest_frame.visible_rect().height()) == 0;
}

bool ARGBToI444x(const VideoFrame& src_frame,
                 VideoFrame& dest_frame,
                 const libyuv::ArgbConstants* matrix) {
  DCHECK(IsSupportedRGBFormat(src_frame.format()));
  DCHECK(dest_frame.format() == PIXEL_FORMAT_I444 ||
         dest_frame.format() == PIXEL_FORMAT_I444A);
  if (libyuv::ARGBToI444Matrix(
          src_frame.visible_data(VideoFrame::Plane::kARGB),
          src_frame.stride(VideoFrame::Plane::kARGB),
          dest_frame.GetWritableVisibleData(VideoFrame::Plane::kY),
          dest_frame.stride(VideoFrame::Plane::kY),
          dest_frame.GetWritableVisibleData(VideoFrame::Plane::kU),
          dest_frame.stride(VideoFrame::Plane::kU),
          dest_frame.GetWritableVisibleData(VideoFrame::Plane::kV),
          dest_frame.stride(VideoFrame::Plane::kV), matrix,
          dest_frame.visible_rect().width(),
          dest_frame.visible_rect().height()) != 0) {
    return false;
  }
  if (dest_frame.format() == PIXEL_FORMAT_I444) {
    return true;
  }
  return libyuv::ARGBExtractAlpha(
             src_frame.visible_data(VideoFrame::Plane::kARGB),
             src_frame.stride(VideoFrame::Plane::kARGB),
             dest_frame.GetWritableVisibleData(VideoFrame::Plane::kA),
             dest_frame.stride(VideoFrame::Plane::kA),
             dest_frame.visible_rect().width(),
             dest_frame.visible_rect().height()) == 0;
}

bool ARGBToNVxx(const VideoFrame& src_frame,
                VideoFrame& dest_frame,
                const libyuv::ArgbConstants* matrix) {
  DCHECK(IsSupportedRGBFormat(src_frame.format()));
  DCHECK(dest_frame.format() == PIXEL_FORMAT_NV12 ||
         dest_frame.format() == PIXEL_FORMAT_NV12A ||
         dest_frame.format() == PIXEL_FORMAT_NV16 ||
         dest_frame.format() == PIXEL_FORMAT_NV24);

  auto convert = libyuv::ARGBToNV12Matrix;
  switch (dest_frame.format()) {
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV12A:
      break;
    case PIXEL_FORMAT_NV16:
      convert = libyuv::ARGBToNV16Matrix;
      break;
    case PIXEL_FORMAT_NV24:
      convert = libyuv::ARGBToNV24Matrix;
      break;
    default:
      NOTREACHED();
  }

  if (convert(src_frame.visible_data(VideoFrame::Plane::kARGB),
              src_frame.stride(VideoFrame::Plane::kARGB),
              dest_frame.GetWritableVisibleData(VideoFrame::Plane::kY),
              dest_frame.stride(VideoFrame::Plane::kY),
              dest_frame.GetWritableVisibleData(VideoFrame::Plane::kUV),
              dest_frame.stride(VideoFrame::Plane::kUV), matrix,
              dest_frame.visible_rect().width(),
              dest_frame.visible_rect().height()) != 0) {
    return false;
  }
  if (dest_frame.format() != PIXEL_FORMAT_NV12A) {
    return true;
  }
  return libyuv::ARGBExtractAlpha(
             src_frame.visible_data(VideoFrame::Plane::kARGB),
             src_frame.stride(VideoFrame::Plane::kARGB),
             dest_frame.GetWritableVisibleData(VideoFrame::Plane::kATriPlanar),
             dest_frame.stride(VideoFrame::Plane::kATriPlanar),
             dest_frame.visible_rect().width(),
             dest_frame.visible_rect().height()) == 0;
}

bool ARGBToPx10(const VideoFrame& src_frame,
                VideoFrame& dest_frame,
                const libyuv::ArgbConstants* matrix) {
  DCHECK(IsSupportedRGBFormat(src_frame.format()));
  DCHECK(dest_frame.format() == PIXEL_FORMAT_P010LE ||
         dest_frame.format() == PIXEL_FORMAT_P210LE ||
         dest_frame.format() == PIXEL_FORMAT_P410LE);

  auto convert = libyuv::ARGBToP010Matrix;
  switch (dest_frame.format()) {
    case PIXEL_FORMAT_P010LE:
      break;
    case PIXEL_FORMAT_P210LE:
      convert = libyuv::ARGBToP210Matrix;
      break;
    case PIXEL_FORMAT_P410LE:
      convert = libyuv::ARGBToP410Matrix;
      break;
    default:
      NOTREACHED();
  }

  return convert(src_frame.visible_data(VideoFrame::Plane::kARGB),
                 src_frame.stride(VideoFrame::Plane::kARGB),
                 WritableVisibleData16(dest_frame, VideoFrame::Plane::kY),
                 Stride16(dest_frame, VideoFrame::Plane::kY),
                 WritableVisibleData16(dest_frame, VideoFrame::Plane::kUV),
                 Stride16(dest_frame, VideoFrame::Plane::kUV), matrix,
                 dest_frame.visible_rect().width(),
                 dest_frame.visible_rect().height()) == 0;
}

void I4xxxScale(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK(src_frame.format() == PIXEL_FORMAT_I420 ||
         src_frame.format() == PIXEL_FORMAT_I420A ||
         src_frame.format() == PIXEL_FORMAT_I422 ||
         src_frame.format() == PIXEL_FORMAT_I422A ||
         src_frame.format() == PIXEL_FORMAT_I444 ||
         src_frame.format() == PIXEL_FORMAT_I444A);
  DCHECK(dest_frame.format() == PIXEL_FORMAT_I420 ||
         dest_frame.format() == PIXEL_FORMAT_I420A ||
         dest_frame.format() == PIXEL_FORMAT_I422 ||
         dest_frame.format() == PIXEL_FORMAT_I422A ||
         dest_frame.format() == PIXEL_FORMAT_I444 ||
         dest_frame.format() == PIXEL_FORMAT_I444A);
  DCHECK_GE(VideoFrame::NumPlanes(src_frame.format()),
            VideoFrame::NumPlanes(dest_frame.format()));

  // When scaling just for format conversion don't use a filter since it will
  // actually end up harming quality.
  const auto kDefaultFiltering =
      src_frame.visible_rect() == dest_frame.visible_rect()
          ? libyuv::kFilterNone
          : libyuv::kFilterBox;

  for (size_t i = 0; i < VideoFrame::NumPlanes(dest_frame.format()); ++i) {
    libyuv::ScalePlane(src_frame.visible_data(i), src_frame.stride(i),
                       src_frame.GetVisibleColumns(i),
                       src_frame.GetVisibleRows(i),
                       dest_frame.GetWritableVisibleData(i),
                       dest_frame.stride(i), dest_frame.GetVisibleColumns(i),
                       dest_frame.GetVisibleRows(i), kDefaultFiltering);
  }
}

void I4xxxScale_16(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK(src_frame.format() == PIXEL_FORMAT_YUV420P10 ||
         src_frame.format() == PIXEL_FORMAT_YUV422P10 ||
         src_frame.format() == PIXEL_FORMAT_YUV444P10 ||
         src_frame.format() == PIXEL_FORMAT_YUV420P12 ||
         src_frame.format() == PIXEL_FORMAT_YUV422P12 ||
         src_frame.format() == PIXEL_FORMAT_YUV444P12 ||
         src_frame.format() == PIXEL_FORMAT_YUV420AP10 ||
         src_frame.format() == PIXEL_FORMAT_YUV422AP10 ||
         src_frame.format() == PIXEL_FORMAT_YUV444AP10);
  DCHECK(dest_frame.format() == PIXEL_FORMAT_YUV420P10 ||
         dest_frame.format() == PIXEL_FORMAT_YUV422P10 ||
         dest_frame.format() == PIXEL_FORMAT_YUV444P10 ||
         dest_frame.format() == PIXEL_FORMAT_YUV420P12 ||
         dest_frame.format() == PIXEL_FORMAT_YUV422P12 ||
         dest_frame.format() == PIXEL_FORMAT_YUV444P12 ||
         dest_frame.format() == PIXEL_FORMAT_YUV420AP10 ||
         dest_frame.format() == PIXEL_FORMAT_YUV422AP10 ||
         dest_frame.format() == PIXEL_FORMAT_YUV444AP10);

  const auto kDefaultFiltering =
      src_frame.visible_rect() == dest_frame.visible_rect()
          ? libyuv::kFilterNone
          : libyuv::kFilterBox;

  for (size_t i = 0; i < VideoFrame::NumPlanes(dest_frame.format()); ++i) {
    const uint16_t* src_ptr = base::subtle::reinterpret_span<const uint16_t>(
                                  src_frame.GetVisiblePlaneData(i))
                                  .data();
    int src_stride = src_frame.stride(i) / sizeof(uint16_t);
    uint16_t* dst_ptr = base::subtle::reinterpret_span<uint16_t>(
                            dest_frame.GetWritableVisiblePlaneData(i))
                            .data();
    int dst_stride = dest_frame.stride(i) / sizeof(uint16_t);

    libyuv::ScalePlane_16(src_ptr, src_stride, src_frame.GetVisibleColumns(i),
                          src_frame.GetVisibleRows(i), dst_ptr, dst_stride,
                          dest_frame.GetVisibleColumns(i),
                          dest_frame.GetVisibleRows(i), kDefaultFiltering);
  }
}

void Convert16To8Plane(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK(src_frame.format() == PIXEL_FORMAT_YUV420P10 ||
         src_frame.format() == PIXEL_FORMAT_YUV422P10 ||
         src_frame.format() == PIXEL_FORMAT_YUV444P10 ||
         src_frame.format() == PIXEL_FORMAT_YUV420P12 ||
         src_frame.format() == PIXEL_FORMAT_YUV422P12 ||
         src_frame.format() == PIXEL_FORMAT_YUV444P12 ||
         src_frame.format() == PIXEL_FORMAT_YUV420AP10 ||
         src_frame.format() == PIXEL_FORMAT_YUV422AP10 ||
         src_frame.format() == PIXEL_FORMAT_YUV444AP10);
  DCHECK(dest_frame.format() == PIXEL_FORMAT_I420 ||
         dest_frame.format() == PIXEL_FORMAT_I420A ||
         dest_frame.format() == PIXEL_FORMAT_I422 ||
         dest_frame.format() == PIXEL_FORMAT_I422A ||
         dest_frame.format() == PIXEL_FORMAT_I444 ||
         dest_frame.format() == PIXEL_FORMAT_I444A);
  DCHECK_EQ(src_frame.visible_rect().size(), dest_frame.visible_rect().size());

  int scale = (src_frame.format() == PIXEL_FORMAT_YUV420P12 ||
               src_frame.format() == PIXEL_FORMAT_YUV422P12 ||
               src_frame.format() == PIXEL_FORMAT_YUV444P12)
                  ? 4096    // 12 bits -> 8 bits
                  : 16384;  // 10 bits -> 8 bits

  for (size_t i = 0; i < VideoFrame::NumPlanes(dest_frame.format()); ++i) {
    const uint16_t* src_ptr = base::subtle::reinterpret_span<const uint16_t>(
                                  src_frame.GetVisiblePlaneData(i))
                                  .data();
    int src_stride = src_frame.stride(i) / sizeof(uint16_t);
    uint8_t* dst_ptr = dest_frame.GetWritableVisibleData(i);
    int dst_stride = dest_frame.stride(i);

    libyuv::Convert16To8Plane(src_ptr, src_stride, dst_ptr, dst_stride, scale,
                              src_frame.GetVisibleColumns(i),
                              src_frame.GetVisibleRows(i));
  }
}

void Convert8To16Plane(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK(src_frame.format() == PIXEL_FORMAT_I420 ||
         src_frame.format() == PIXEL_FORMAT_I420A ||
         src_frame.format() == PIXEL_FORMAT_I422 ||
         src_frame.format() == PIXEL_FORMAT_I422A ||
         src_frame.format() == PIXEL_FORMAT_I444 ||
         src_frame.format() == PIXEL_FORMAT_I444A);
  DCHECK(dest_frame.format() == PIXEL_FORMAT_YUV420P10 ||
         dest_frame.format() == PIXEL_FORMAT_YUV422P10 ||
         dest_frame.format() == PIXEL_FORMAT_YUV444P10);
  DCHECK_EQ(src_frame.visible_rect().size(), dest_frame.visible_rect().size());

  for (size_t i = 0; i < VideoFrame::NumPlanes(dest_frame.format()); ++i) {
    libyuv::Convert8To16Plane(src_frame.visible_data(i), src_frame.stride(i),
                              base::subtle::reinterpret_span<uint16_t>(
                                  dest_frame.GetWritableVisiblePlaneData(i))
                                  .data(),
                              dest_frame.stride(i) / sizeof(uint16_t), 1024,
                              dest_frame.GetVisibleColumns(i),
                              dest_frame.GetVisibleRows(i));
  }
}

void Shift12To10(VideoFrame& frame) {
  DCHECK(frame.format() == PIXEL_FORMAT_YUV420P10 ||
         frame.format() == PIXEL_FORMAT_YUV422P10 ||
         frame.format() == PIXEL_FORMAT_YUV444P10 ||
         frame.format() == PIXEL_FORMAT_YUV420AP10 ||
         frame.format() == PIXEL_FORMAT_YUV422AP10 ||
         frame.format() == PIXEL_FORMAT_YUV444AP10);

  for (size_t i = 0; i < VideoFrame::NumPlanes(frame.format()); ++i) {
    Shift12To10Plane(base::subtle::reinterpret_span<uint16_t>(
                         frame.GetWritableVisiblePlaneData(i)),
                     frame.stride(i) / sizeof(uint16_t),
                     frame.GetVisibleColumns(i), frame.GetVisibleRows(i));
  }
}

bool I4xxxToNVxx(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK(src_frame.format() == PIXEL_FORMAT_I420 ||
         src_frame.format() == PIXEL_FORMAT_I420A ||
         src_frame.format() == PIXEL_FORMAT_I422 ||
         src_frame.format() == PIXEL_FORMAT_I422A ||
         src_frame.format() == PIXEL_FORMAT_I444 ||
         src_frame.format() == PIXEL_FORMAT_I444A);
  DCHECK(dest_frame.format() == PIXEL_FORMAT_NV12 ||
         dest_frame.format() == PIXEL_FORMAT_NV12A ||
         dest_frame.format() == PIXEL_FORMAT_NV16 ||
         dest_frame.format() == PIXEL_FORMAT_NV24);
  DCHECK(dest_frame.format() != PIXEL_FORMAT_NV12A ||
         src_frame.format() == PIXEL_FORMAT_I420A ||
         src_frame.format() == PIXEL_FORMAT_I444A);
  DCHECK_EQ(src_frame.visible_rect().size(), dest_frame.visible_rect().size());

  const auto src_sampling =
      VideoPixelFormatToChromaSampling(src_frame.format());
  const auto dest_sampling =
      VideoPixelFormatToChromaSampling(dest_frame.format());
  if (src_sampling == dest_sampling) {
    libyuv::CopyPlane(src_frame.visible_data(VideoFrame::Plane::kY),
                      src_frame.stride(VideoFrame::Plane::kY),
                      dest_frame.GetWritableVisibleData(VideoFrame::Plane::kY),
                      dest_frame.stride(VideoFrame::Plane::kY),
                      dest_frame.visible_rect().width(),
                      dest_frame.visible_rect().height());
    MergeUV(src_frame, dest_frame);
  } else {
    DCHECK(dest_frame.format() == PIXEL_FORMAT_NV12 ||
           dest_frame.format() == PIXEL_FORMAT_NV12A);
    DCHECK(src_sampling == VideoChromaSampling::k420 ||
           src_sampling == VideoChromaSampling::k444);
    auto convert = src_sampling == VideoChromaSampling::k420
                       ? libyuv::I420ToNV12
                       : libyuv::I444ToNV12;
    if (convert(src_frame.visible_data(VideoFrame::Plane::kY),
                src_frame.stride(VideoFrame::Plane::kY),
                src_frame.visible_data(VideoFrame::Plane::kU),
                src_frame.stride(VideoFrame::Plane::kU),
                src_frame.visible_data(VideoFrame::Plane::kV),
                src_frame.stride(VideoFrame::Plane::kV),
                dest_frame.GetWritableVisibleData(VideoFrame::Plane::kY),
                dest_frame.stride(VideoFrame::Plane::kY),
                dest_frame.GetWritableVisibleData(VideoFrame::Plane::kUV),
                dest_frame.stride(VideoFrame::Plane::kUV),
                dest_frame.visible_rect().width(),
                dest_frame.visible_rect().height()) != 0) {
      return false;
    }
  }
  if (dest_frame.format() == PIXEL_FORMAT_NV12A) {
    libyuv::CopyPlane(
        src_frame.visible_data(VideoFrame::Plane::kA),
        src_frame.stride(VideoFrame::Plane::kA),
        dest_frame.GetWritableVisibleData(VideoFrame::Plane::kATriPlanar),
        dest_frame.stride(VideoFrame::Plane::kATriPlanar),
        dest_frame.visible_rect().width(), dest_frame.visible_rect().height());
  }
  return true;
}

void MergeUV(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK(src_frame.format() == PIXEL_FORMAT_I420 ||
         src_frame.format() == PIXEL_FORMAT_I420A ||
         src_frame.format() == PIXEL_FORMAT_I422 ||
         src_frame.format() == PIXEL_FORMAT_I422A ||
         src_frame.format() == PIXEL_FORMAT_I444 ||
         src_frame.format() == PIXEL_FORMAT_I444A);
  DCHECK(dest_frame.format() == PIXEL_FORMAT_NV12 ||
         dest_frame.format() == PIXEL_FORMAT_NV12A ||
         dest_frame.format() == PIXEL_FORMAT_NV16 ||
         dest_frame.format() == PIXEL_FORMAT_NV24);
  DCHECK_EQ(VideoPixelFormatToChromaSampling(src_frame.format()),
            VideoPixelFormatToChromaSampling(dest_frame.format()));
  DCHECK_EQ(src_frame.visible_rect().size(), dest_frame.visible_rect().size());
  libyuv::MergeUVPlane(
      src_frame.visible_data(VideoFrame::Plane::kU),
      src_frame.stride(VideoFrame::Plane::kU),
      src_frame.visible_data(VideoFrame::Plane::kV),
      src_frame.stride(VideoFrame::Plane::kV),
      dest_frame.GetWritableVisibleData(VideoFrame::Plane::kUV),
      dest_frame.stride(VideoFrame::Plane::kUV),
      dest_frame.GetVisibleColumns(VideoFrame::Plane::kUV),
      dest_frame.GetVisibleRows(VideoFrame::Plane::kUV));
}

void SplitUV(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK(src_frame.format() == PIXEL_FORMAT_NV12 ||
         src_frame.format() == PIXEL_FORMAT_NV12A ||
         src_frame.format() == PIXEL_FORMAT_NV16 ||
         src_frame.format() == PIXEL_FORMAT_NV24);
  DCHECK(dest_frame.format() == PIXEL_FORMAT_I420 ||
         dest_frame.format() == PIXEL_FORMAT_I420A ||
         dest_frame.format() == PIXEL_FORMAT_I422 ||
         dest_frame.format() == PIXEL_FORMAT_I444);
  DCHECK_EQ(VideoPixelFormatToChromaSampling(src_frame.format()),
            VideoPixelFormatToChromaSampling(dest_frame.format()));
  DCHECK_EQ(src_frame.visible_rect().size(), dest_frame.visible_rect().size());
  libyuv::SplitUVPlane(src_frame.visible_data(VideoFrame::Plane::kUV),
                       src_frame.stride(VideoFrame::Plane::kUV),
                       dest_frame.GetWritableVisibleData(VideoFrame::Plane::kU),
                       dest_frame.stride(VideoFrame::Plane::kU),
                       dest_frame.GetWritableVisibleData(VideoFrame::Plane::kV),
                       dest_frame.stride(VideoFrame::Plane::kV),
                       src_frame.GetVisibleColumns(VideoFrame::Plane::kUV),
                       src_frame.GetVisibleRows(VideoFrame::Plane::kUV));
}

bool NVxxScale(const VideoFrame& src_frame,
               VideoFrame& dest_frame,
               libyuv::FilterMode filter) {
  DCHECK(src_frame.format() == PIXEL_FORMAT_NV12 ||
         src_frame.format() == PIXEL_FORMAT_NV12A ||
         src_frame.format() == PIXEL_FORMAT_NV16 ||
         src_frame.format() == PIXEL_FORMAT_NV24);
  DCHECK(dest_frame.format() == PIXEL_FORMAT_NV12 ||
         dest_frame.format() == PIXEL_FORMAT_NV12A ||
         dest_frame.format() == PIXEL_FORMAT_NV16 ||
         dest_frame.format() == PIXEL_FORMAT_NV24);
  DCHECK(dest_frame.format() != PIXEL_FORMAT_NV12A ||
         src_frame.format() == PIXEL_FORMAT_NV12A);
  if (src_frame.visible_rect().size() == dest_frame.visible_rect().size()) {
    filter = libyuv::kFilterNone;
  }
  if (((src_frame.format() == PIXEL_FORMAT_NV12 ||
        src_frame.format() == PIXEL_FORMAT_NV12A) &&
       (dest_frame.format() == PIXEL_FORMAT_NV12 ||
        dest_frame.format() == PIXEL_FORMAT_NV12A)) ||
      (src_frame.format() == PIXEL_FORMAT_NV16 &&
       dest_frame.format() == PIXEL_FORMAT_NV16) ||
      (src_frame.format() == PIXEL_FORMAT_NV24 &&
       dest_frame.format() == PIXEL_FORMAT_NV24)) {
    auto scale = libyuv::NV12Scale;
    switch (src_frame.format()) {
      case PIXEL_FORMAT_NV12:
      case PIXEL_FORMAT_NV12A:
        break;
      case PIXEL_FORMAT_NV16:
        scale = libyuv::NV16Scale;
        break;
      case PIXEL_FORMAT_NV24:
        scale = libyuv::NV24Scale;
        break;
      default:
        NOTREACHED();
    }
    if (scale(src_frame.visible_data(VideoFrame::Plane::kY),
              src_frame.stride(VideoFrame::Plane::kY),
              src_frame.visible_data(VideoFrame::Plane::kUV),
              src_frame.stride(VideoFrame::Plane::kUV),
              src_frame.visible_rect().width(),
              src_frame.visible_rect().height(),
              dest_frame.GetWritableVisibleData(VideoFrame::Plane::kY),
              dest_frame.stride(VideoFrame::Plane::kY),
              dest_frame.GetWritableVisibleData(VideoFrame::Plane::kUV),
              dest_frame.stride(VideoFrame::Plane::kUV),
              dest_frame.visible_rect().width(),
              dest_frame.visible_rect().height(), filter) != 0) {
      return false;
    }
  } else {
    if (libyuv::ScalePlane(
            src_frame.visible_data(VideoFrame::Plane::kY),
            src_frame.stride(VideoFrame::Plane::kY),
            src_frame.GetVisibleColumns(VideoFrame::Plane::kY),
            src_frame.GetVisibleRows(VideoFrame::Plane::kY),
            dest_frame.GetWritableVisibleData(VideoFrame::Plane::kY),
            dest_frame.stride(VideoFrame::Plane::kY),
            dest_frame.GetVisibleColumns(VideoFrame::Plane::kY),
            dest_frame.GetVisibleRows(VideoFrame::Plane::kY), filter) != 0 ||
        libyuv::UVScale(
            src_frame.visible_data(VideoFrame::Plane::kUV),
            src_frame.stride(VideoFrame::Plane::kUV),
            src_frame.GetVisibleColumns(VideoFrame::Plane::kUV),
            src_frame.GetVisibleRows(VideoFrame::Plane::kUV),
            dest_frame.GetWritableVisibleData(VideoFrame::Plane::kUV),
            dest_frame.stride(VideoFrame::Plane::kUV),
            dest_frame.GetVisibleColumns(VideoFrame::Plane::kUV),
            dest_frame.GetVisibleRows(VideoFrame::Plane::kUV), filter) != 0) {
      return false;
    }
  }
  if (dest_frame.format() != PIXEL_FORMAT_NV12A) {
    return true;
  }
  libyuv::ScalePlane(
      src_frame.visible_data(VideoFrame::Plane::kATriPlanar),
      src_frame.stride(VideoFrame::Plane::kATriPlanar),
      src_frame.visible_rect().width(), src_frame.visible_rect().height(),
      dest_frame.GetWritableVisibleData(VideoFrame::Plane::kATriPlanar),
      dest_frame.stride(VideoFrame::Plane::kATriPlanar),
      dest_frame.visible_rect().width(), dest_frame.visible_rect().height(),
      filter);
  return true;
}

bool Px10Scale(const VideoFrame& src_frame,
               VideoFrame& dest_frame,
               libyuv::FilterMode filter) {
  DCHECK_EQ(src_frame.format(), dest_frame.format());
  DCHECK(src_frame.format() == PIXEL_FORMAT_P010LE ||
         src_frame.format() == PIXEL_FORMAT_P210LE ||
         src_frame.format() == PIXEL_FORMAT_P410LE);
  if (src_frame.visible_rect().size() == dest_frame.visible_rect().size()) {
    filter = libyuv::kFilterNone;
  }
  auto scale = libyuv::P010Scale;
  switch (src_frame.format()) {
    case PIXEL_FORMAT_P010LE:
      break;
    case PIXEL_FORMAT_P210LE:
      scale = libyuv::P210Scale;
      break;
    case PIXEL_FORMAT_P410LE:
      scale = libyuv::P410Scale;
      break;
    default:
      NOTREACHED();
  }
  return scale(VisibleData16(src_frame, VideoFrame::Plane::kY),
               Stride16(src_frame, VideoFrame::Plane::kY),
               VisibleData16(src_frame, VideoFrame::Plane::kUV),
               Stride16(src_frame, VideoFrame::Plane::kUV),
               src_frame.visible_rect().width(),
               src_frame.visible_rect().height(),
               WritableVisibleData16(dest_frame, VideoFrame::Plane::kY),
               Stride16(dest_frame, VideoFrame::Plane::kY),
               WritableVisibleData16(dest_frame, VideoFrame::Plane::kUV),
               Stride16(dest_frame, VideoFrame::Plane::kUV),
               dest_frame.visible_rect().width(),
               dest_frame.visible_rect().height(), filter) == 0;
}

void NVxxToI4xxx(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK(src_frame.format() == PIXEL_FORMAT_NV12 ||
         src_frame.format() == PIXEL_FORMAT_NV12A ||
         src_frame.format() == PIXEL_FORMAT_NV16 ||
         src_frame.format() == PIXEL_FORMAT_NV24);
  DCHECK(dest_frame.format() == PIXEL_FORMAT_I420 ||
         dest_frame.format() == PIXEL_FORMAT_I420A ||
         dest_frame.format() == PIXEL_FORMAT_I422 ||
         dest_frame.format() == PIXEL_FORMAT_I444);
  DCHECK(dest_frame.format() != PIXEL_FORMAT_I420A ||
         src_frame.format() == PIXEL_FORMAT_NV12A);
  DCHECK_EQ(VideoPixelFormatToChromaSampling(src_frame.format()),
            VideoPixelFormatToChromaSampling(dest_frame.format()));
  DCHECK_EQ(src_frame.visible_rect().size(), dest_frame.visible_rect().size());
  libyuv::CopyPlane(src_frame.visible_data(VideoFrame::Plane::kY),
                    src_frame.stride(VideoFrame::Plane::kY),
                    dest_frame.GetWritableVisibleData(VideoFrame::Plane::kY),
                    dest_frame.stride(VideoFrame::Plane::kY),
                    dest_frame.visible_rect().width(),
                    dest_frame.visible_rect().height());
  SplitUV(src_frame, dest_frame);
  if (dest_frame.format() == PIXEL_FORMAT_I420A) {
    libyuv::CopyPlane(src_frame.visible_data(VideoFrame::Plane::kATriPlanar),
                      src_frame.stride(VideoFrame::Plane::kATriPlanar),
                      dest_frame.GetWritableVisibleData(VideoFrame::Plane::kA),
                      dest_frame.stride(VideoFrame::Plane::kA),
                      dest_frame.visible_rect().width(),
                      dest_frame.visible_rect().height());
  }
}

bool NVxxToPx10(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK(src_frame.format() == PIXEL_FORMAT_NV12 ||
         src_frame.format() == PIXEL_FORMAT_NV12A ||
         src_frame.format() == PIXEL_FORMAT_NV16 ||
         src_frame.format() == PIXEL_FORMAT_NV24);
  DCHECK(dest_frame.format() == PIXEL_FORMAT_P010LE ||
         dest_frame.format() == PIXEL_FORMAT_P210LE ||
         dest_frame.format() == PIXEL_FORMAT_P410LE);
  DCHECK_EQ(VideoPixelFormatToChromaSampling(src_frame.format()),
            VideoPixelFormatToChromaSampling(dest_frame.format()));
  DCHECK_EQ(src_frame.visible_rect().size(), dest_frame.visible_rect().size());

  for (size_t i = 0; i < VideoFrame::NumPlanes(dest_frame.format()); ++i) {
    int cols = dest_frame.GetVisibleColumns(i);
    if (i == VideoFrame::Plane::kUV) {
      cols *= 2;  // UV plane has 2 elements per chroma column
    }
    const int rows = dest_frame.GetVisibleRows(i);
    uint16_t* dst_ptr = base::subtle::reinterpret_span<uint16_t>(
                            dest_frame.GetWritableVisiblePlaneData(i))
                            .data();
    const int dst_stride_elements = dest_frame.stride(i) / sizeof(uint16_t);

    libyuv::Convert8To16Plane(src_frame.visible_data(i), src_frame.stride(i),
                              dst_ptr, dst_stride_elements, 1024, cols, rows);
    libyuv::ConvertToMSBPlane_16(dst_ptr, dst_stride_elements, dst_ptr,
                                 dst_stride_elements, cols, rows, 10);
  }

  return true;
}

bool I4xxxToPx10(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK(src_frame.format() == PIXEL_FORMAT_I420 ||
         src_frame.format() == PIXEL_FORMAT_I420A ||
         src_frame.format() == PIXEL_FORMAT_I422 ||
         src_frame.format() == PIXEL_FORMAT_I422A ||
         src_frame.format() == PIXEL_FORMAT_I444 ||
         src_frame.format() == PIXEL_FORMAT_I444A);
  DCHECK(dest_frame.format() == PIXEL_FORMAT_P010LE ||
         dest_frame.format() == PIXEL_FORMAT_P210LE ||
         dest_frame.format() == PIXEL_FORMAT_P410LE);
  DCHECK_EQ(VideoPixelFormatToChromaSampling(src_frame.format()),
            VideoPixelFormatToChromaSampling(dest_frame.format()));
  DCHECK_EQ(src_frame.visible_rect().size(), dest_frame.visible_rect().size());

  auto convert = libyuv::I420ToP010;
  switch (dest_frame.format()) {
    case PIXEL_FORMAT_P010LE:
      break;
    case PIXEL_FORMAT_P210LE:
      convert = libyuv::I422ToP210;
      break;
    case PIXEL_FORMAT_P410LE:
      convert = libyuv::I444ToP410;
      break;
    default:
      NOTREACHED();
  }

  return convert(src_frame.visible_data(VideoFrame::Plane::kY),
                 src_frame.stride(VideoFrame::Plane::kY),
                 src_frame.visible_data(VideoFrame::Plane::kU),
                 src_frame.stride(VideoFrame::Plane::kU),
                 src_frame.visible_data(VideoFrame::Plane::kV),
                 src_frame.stride(VideoFrame::Plane::kV),
                 WritableVisibleData16(dest_frame, VideoFrame::Plane::kY),
                 Stride16(dest_frame, VideoFrame::Plane::kY),
                 WritableVisibleData16(dest_frame, VideoFrame::Plane::kUV),
                 Stride16(dest_frame, VideoFrame::Plane::kUV),
                 dest_frame.visible_rect().width(),
                 dest_frame.visible_rect().height()) == 0;
}

bool I4xxxPxxToPx10(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK(src_frame.format() == PIXEL_FORMAT_YUV420P10 ||
         src_frame.format() == PIXEL_FORMAT_YUV422P10 ||
         src_frame.format() == PIXEL_FORMAT_YUV444P10 ||
         src_frame.format() == PIXEL_FORMAT_YUV420P12 ||
         src_frame.format() == PIXEL_FORMAT_YUV422P12 ||
         src_frame.format() == PIXEL_FORMAT_YUV444P12 ||
         src_frame.format() == PIXEL_FORMAT_YUV420AP10 ||
         src_frame.format() == PIXEL_FORMAT_YUV422AP10 ||
         src_frame.format() == PIXEL_FORMAT_YUV444AP10);
  DCHECK(dest_frame.format() == PIXEL_FORMAT_P010LE ||
         dest_frame.format() == PIXEL_FORMAT_P210LE ||
         dest_frame.format() == PIXEL_FORMAT_P410LE);
  DCHECK_EQ(VideoPixelFormatToChromaSampling(src_frame.format()),
            VideoPixelFormatToChromaSampling(dest_frame.format()));
  DCHECK_EQ(src_frame.visible_rect().size(), dest_frame.visible_rect().size());

  const int depth = (src_frame.format() == PIXEL_FORMAT_YUV420P12 ||
                     src_frame.format() == PIXEL_FORMAT_YUV422P12 ||
                     src_frame.format() == PIXEL_FORMAT_YUV444P12)
                        ? 12
                        : 10;

  base::span<uint16_t> dst_y_span = base::subtle::reinterpret_span<uint16_t>(
      dest_frame.GetWritableVisiblePlaneData(VideoFrame::Plane::kY));
  const int dst_stride_y =
      dest_frame.stride(VideoFrame::Plane::kY) / sizeof(uint16_t);
  const int y_width = dest_frame.visible_rect().width();
  const int y_height = dest_frame.visible_rect().height();

  libyuv::ConvertToMSBPlane_16(
      base::subtle::reinterpret_span<const uint16_t>(
          src_frame.GetVisiblePlaneData(VideoFrame::Plane::kY))
          .data(),
      src_frame.stride(VideoFrame::Plane::kY) / sizeof(uint16_t),
      dst_y_span.data(), dst_stride_y, y_width, y_height, depth);

  if (depth == 12) {
    Mask12BitMSBPlane(dst_y_span, dst_stride_y, y_width, y_height);
  }

  base::span<uint16_t> dst_uv_span = base::subtle::reinterpret_span<uint16_t>(
      dest_frame.GetWritableVisiblePlaneData(VideoFrame::Plane::kUV));
  const int dst_stride_uv =
      dest_frame.stride(VideoFrame::Plane::kUV) / sizeof(uint16_t);
  const int uv_width = dest_frame.GetVisibleColumns(VideoFrame::Plane::kUV);
  const int uv_height = dest_frame.GetVisibleRows(VideoFrame::Plane::kUV);

  libyuv::MergeUVPlane_16(
      base::subtle::reinterpret_span<const uint16_t>(
          src_frame.GetVisiblePlaneData(VideoFrame::Plane::kU))
          .data(),
      src_frame.stride(VideoFrame::Plane::kU) / sizeof(uint16_t),
      base::subtle::reinterpret_span<const uint16_t>(
          src_frame.GetVisiblePlaneData(VideoFrame::Plane::kV))
          .data(),
      src_frame.stride(VideoFrame::Plane::kV) / sizeof(uint16_t),
      dst_uv_span.data(), dst_stride_uv, uv_width, uv_height, depth);

  if (depth == 12) {
    Mask12BitMSBPlane(dst_uv_span, dst_stride_uv, uv_width * 2, uv_height);
  }

  return true;
}

bool Px10ToIx10(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK(src_frame.format() == PIXEL_FORMAT_P010LE ||
         src_frame.format() == PIXEL_FORMAT_P210LE ||
         src_frame.format() == PIXEL_FORMAT_P410LE);
  DCHECK_EQ(src_frame.visible_rect().size(), dest_frame.visible_rect().size());

  auto convert = libyuv::P010ToI010;
  switch (src_frame.format()) {
    case PIXEL_FORMAT_P010LE:
      DCHECK_EQ(dest_frame.format(), PIXEL_FORMAT_YUV420P10);
      break;
    case PIXEL_FORMAT_P210LE:
      DCHECK_EQ(dest_frame.format(), PIXEL_FORMAT_YUV422P10);
      convert = libyuv::P210ToI210;
      break;
    case PIXEL_FORMAT_P410LE:
      DCHECK_EQ(dest_frame.format(), PIXEL_FORMAT_YUV444P10);
      convert = libyuv::P410ToI410;
      break;
    default:
      NOTREACHED();
  }

  return convert(VisibleData16(src_frame, VideoFrame::Plane::kY),
                 Stride16(src_frame, VideoFrame::Plane::kY),
                 VisibleData16(src_frame, VideoFrame::Plane::kUV),
                 Stride16(src_frame, VideoFrame::Plane::kUV),
                 WritableVisibleData16(dest_frame, VideoFrame::Plane::kY),
                 Stride16(dest_frame, VideoFrame::Plane::kY),
                 WritableVisibleData16(dest_frame, VideoFrame::Plane::kU),
                 Stride16(dest_frame, VideoFrame::Plane::kU),
                 WritableVisibleData16(dest_frame, VideoFrame::Plane::kV),
                 Stride16(dest_frame, VideoFrame::Plane::kV),
                 dest_frame.visible_rect().width(),
                 dest_frame.visible_rect().height()) == 0;
}

bool Px10ToI420(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK(src_frame.format() == PIXEL_FORMAT_P010LE ||
         src_frame.format() == PIXEL_FORMAT_P210LE ||
         src_frame.format() == PIXEL_FORMAT_P410LE);
  DCHECK_EQ(dest_frame.format(), PIXEL_FORMAT_I420);
  DCHECK_EQ(src_frame.visible_rect().size(), dest_frame.visible_rect().size());

  auto convert = libyuv::P010ToI420;
  switch (src_frame.format()) {
    case PIXEL_FORMAT_P010LE:
      break;
    case PIXEL_FORMAT_P210LE:
      convert = libyuv::P210ToI420;
      break;
    case PIXEL_FORMAT_P410LE:
      convert = libyuv::P410ToI420;
      break;
    default:
      NOTREACHED();
  }

  return convert(VisibleData16(src_frame, VideoFrame::Plane::kY),
                 Stride16(src_frame, VideoFrame::Plane::kY),
                 VisibleData16(src_frame, VideoFrame::Plane::kUV),
                 Stride16(src_frame, VideoFrame::Plane::kUV),
                 dest_frame.GetWritableVisibleData(VideoFrame::Plane::kY),
                 dest_frame.stride(VideoFrame::Plane::kY),
                 dest_frame.GetWritableVisibleData(VideoFrame::Plane::kU),
                 dest_frame.stride(VideoFrame::Plane::kU),
                 dest_frame.GetWritableVisibleData(VideoFrame::Plane::kV),
                 dest_frame.stride(VideoFrame::Plane::kV),
                 dest_frame.visible_rect().width(),
                 dest_frame.visible_rect().height()) == 0;
}

void Px10ToNVxx(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK(src_frame.format() == PIXEL_FORMAT_P010LE ||
         src_frame.format() == PIXEL_FORMAT_P210LE ||
         src_frame.format() == PIXEL_FORMAT_P410LE);
  DCHECK(dest_frame.format() == PIXEL_FORMAT_NV12 ||
         dest_frame.format() == PIXEL_FORMAT_NV16 ||
         dest_frame.format() == PIXEL_FORMAT_NV24);
  DCHECK_EQ(VideoPixelFormatToChromaSampling(src_frame.format()),
            VideoPixelFormatToChromaSampling(dest_frame.format()));
  DCHECK_EQ(src_frame.visible_rect().size(), dest_frame.visible_rect().size());

  for (size_t i = 0; i < VideoFrame::NumPlanes(dest_frame.format()); ++i) {
    int cols = dest_frame.GetVisibleColumns(i);
    if (i == VideoFrame::Plane::kUV) {
      cols *= 2;  // UV plane has 2 elements per chroma column.
    }
    libyuv::Convert16To8Plane(
        base::subtle::reinterpret_span<const uint16_t>(
            src_frame.GetVisiblePlaneData(i))
            .data(),
        src_frame.stride(i) / sizeof(uint16_t),
        dest_frame.GetWritableVisibleData(i), dest_frame.stride(i),
        /*scale=*/256, cols, dest_frame.GetVisibleRows(i));
  }
}

namespace {

constexpr int kRgbaF16BitDepth = 10;
constexpr float kRgbaF16MaxValue = (1 << kRgbaF16BitDepth) - 1;
// P0xx/P2xx/P4xx keep the 10 significant bits in the high bits of a word.
constexpr int kRgbaF16Shift = 16 - kRgbaF16BitDepth;
// The number of rows color managed by one Skia call. Skia sets up a conversion
// for every call, which is too much to pay per row. A multiple of the chroma
// subsampling height, so a chroma row never straddles two strips.
constexpr int kRgbaF16StripRows = 8;

// The rows of the RGB to YUV matrix, with the offset in w.
struct RgbToYuvRows {
  SkV4 y;
  SkV4 u;
  SkV4 v;
};

struct Yuv {
  float y;
  float u;
  float v;
};

// Clips the color managed RGB to the encodable range before the matrix, so
// out of range colors are clipped in RGB like they are on the GPU rather than
// per YUV component. NaN becomes 0.
float ClipRgb(float value) {
  return value > 0.0f ? std::min(value, 1.0f) : 0.0f;
}

// Rounds a value in [0, 1] to the nearest sample. Unlike std::lround(), this
// compiles to an inline conversion instead of a call per sample.
uint16_t QuantizeP10(float value) {
  const auto sample = static_cast<uint16_t>(
      std::clamp(value, 0.0f, 1.0f) * kRgbaF16MaxValue + 0.5f);
  return static_cast<uint16_t>(sample << kRgbaF16Shift);
}

Yuv ToYuv(const RgbToYuvRows& matrix, float r, float g, float b) {
  r = ClipRgb(r);
  g = ClipRgb(g);
  b = ClipRgb(b);
  return {
      matrix.y.x * r + matrix.y.y * g + matrix.y.z * b + matrix.y.w,
      matrix.u.x * r + matrix.u.y * g + matrix.u.z * b + matrix.u.w,
      matrix.v.x * r + matrix.v.y * g + matrix.v.z * b + matrix.v.w,
  };
}

// Converts a row of color managed RGBA floats to Y samples, and adds the U and
// V of its pixels to the sums of the chroma samples they belong to.
template <int kSubsampleWidth>
void ConvertRgbRow(const RgbToYuvRows& matrix,
                   base::span<const float> rgb,
                   int width,
                   base::span<uint16_t> y_row,
                   base::span<float> uv_sums) {
  static_assert(kSubsampleWidth == 1 || kSubsampleWidth == 2);
  int x = 0;
  if constexpr (kSubsampleWidth == 2) {
    // Both pixels of a pair belong to chroma sample x / 2.
    for (; x + 1 < width; x += 2) {
      const Yuv first =
          ToYuv(matrix, rgb[x * 4], rgb[x * 4 + 1], rgb[x * 4 + 2]);
      const Yuv second =
          ToYuv(matrix, rgb[x * 4 + 4], rgb[x * 4 + 5], rgb[x * 4 + 6]);
      y_row[x] = QuantizeP10(first.y);
      y_row[x + 1] = QuantizeP10(second.y);
      uv_sums[x] += first.u + second.u;
      uv_sums[x + 1] += first.v + second.v;
    }
  }
  // What is left is every pixel without horizontal subsampling, or the last
  // pixel of an odd width.
  for (; x < width; ++x) {
    const Yuv yuv = ToYuv(matrix, rgb[x * 4], rgb[x * 4 + 1], rgb[x * 4 + 2]);
    y_row[x] = QuantizeP10(yuv.y);
    uv_sums[(x / kSubsampleWidth) * 2] += yuv.u;
    uv_sums[(x / kSubsampleWidth) * 2 + 1] += yuv.v;
  }
}

}  // namespace

bool RGBAF16ToPx10(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK_EQ(src_frame.format(), PIXEL_FORMAT_RGBAF16);
  DCHECK(dest_frame.format() == PIXEL_FORMAT_P010LE ||
         dest_frame.format() == PIXEL_FORMAT_P210LE ||
         dest_frame.format() == PIXEL_FORMAT_P410LE);
  DCHECK_EQ(src_frame.visible_rect().size(), dest_frame.visible_rect().size());

  const gfx::ColorSpace& dest_color_space = dest_frame.ColorSpace();
  const gfx::ColorSpace dest_rgb = dest_color_space.GetAsFullRangeRGB();
  const gfx::ColorSpace src_rgb =
      src_frame.ColorSpace().IsValid()
          ? src_frame.ColorSpace().GetAsFullRangeRGB()
          : dest_rgb;
  // PQ and HLG need an SDR white level to map to Skia's linear space. Both
  // sides use the same one, so it cancels out when the transfers match.
  constexpr float kSdrWhiteLevel = gfx::ColorSpace::kDefaultSDRWhiteLevel;
  sk_sp<SkColorSpace> src_sk_color_space =
      src_rgb.ToSkColorSpace(kSdrWhiteLevel);
  sk_sp<SkColorSpace> dest_sk_color_space =
      dest_rgb.ToSkColorSpace(kSdrWhiteLevel);
  if (!src_sk_color_space || !dest_sk_color_space) {
    return false;
  }

  // GetTransferMatrix() maps RGB to full range YUV, and GetRangeAdjustMatrix()
  // expands the YUV range for decoding, so its inverse narrows it here.
  SkM44 range_narrowing;
  if (!dest_color_space.GetRangeAdjustMatrix(kRgbaF16BitDepth)
           .invert(&range_narrowing)) {
    return false;
  }
  const SkM44 rgb_to_yuv =
      range_narrowing * dest_color_space.GetTransferMatrix(kRgbaF16BitDepth);
  const RgbToYuvRows matrix = {rgb_to_yuv.row(0), rgb_to_yuv.row(1),
                               rgb_to_yuv.row(2)};

  const int width = dest_frame.visible_rect().width();
  const int height = dest_frame.visible_rect().height();
  const gfx::Size subsample =
      VideoFrame::SampleSize(dest_frame.format(), VideoFrame::Plane::kUV);
  const int chroma_width = (width + subsample.width() - 1) / subsample.width();

  const SkImageInfo src_info =
      SkImageInfo::Make(width, kRgbaF16StripRows, kRGBA_F16_SkColorType,
                        kUnpremul_SkAlphaType, std::move(src_sk_color_space));
  const SkImageInfo rgb_info =
      SkImageInfo::Make(width, kRgbaF16StripRows, kRGBA_F32_SkColorType,
                        kUnpremul_SkAlphaType, std::move(dest_sk_color_space));
  const size_t row_floats = static_cast<size_t>(width) * 4;
  std::vector<float> rgb_strip(row_floats * kRgbaF16StripRows);
  // Sums of the U and V of the pixels each chroma sample covers.
  std::vector<float> uv_sums(chroma_width * 2);

  const auto src = src_frame.GetVisiblePlaneData(VideoFrame::Plane::kARGB);
  const size_t src_stride = src_frame.stride(VideoFrame::Plane::kARGB);
  auto dest_y = base::subtle::reinterpret_span<uint16_t>(
      dest_frame.GetWritableVisiblePlaneData(VideoFrame::Plane::kY));
  const size_t dest_y_stride = Stride16(dest_frame, VideoFrame::Plane::kY);
  auto dest_uv = base::subtle::reinterpret_span<uint16_t>(
      dest_frame.GetWritableVisiblePlaneData(VideoFrame::Plane::kUV));
  const size_t dest_uv_stride = Stride16(dest_frame, VideoFrame::Plane::kUV);

  // Skia rejects a row stride that is not a whole number of pixels, which a
  // frame does not guarantee. Such a frame is read one row at a time, where
  // the row bytes only need to hold the row.
  const bool can_read_strips = src_stride % src_info.bytesPerPixel() == 0;
  auto read_strip = [&](int first_row, int rows) {
    if (can_read_strips) {
      const SkPixmap from(src_info.makeWH(width, rows),
                          src.subspan(first_row * src_stride).data(),
                          rows == 1 ? src_info.minRowBytes() : src_stride);
      const SkPixmap to(rgb_info.makeWH(width, rows), rgb_strip.data(),
                        rgb_info.minRowBytes());
      return from.readPixels(to);
    }
    for (int i = 0; i < rows; ++i) {
      const SkPixmap from(src_info.makeWH(width, 1),
                          src.subspan((first_row + i) * src_stride).data(),
                          src_info.minRowBytes());
      const SkPixmap to(rgb_info.makeWH(width, 1),
                        base::span(rgb_strip).subspan(i * row_floats).data(),
                        rgb_info.minRowBytes());
      if (!from.readPixels(to)) {
        return false;
      }
    }
    return true;
  };
  const auto convert_row =
      subsample.width() == 1 ? &ConvertRgbRow<1> : &ConvertRgbRow<2>;

  for (int strip_row = 0; strip_row < height; strip_row += kRgbaF16StripRows) {
    const int strip_height = std::min(kRgbaF16StripRows, height - strip_row);
    if (!read_strip(strip_row, strip_height)) {
      return false;
    }
    for (int first = 0; first < strip_height; first += subsample.height()) {
      const int rows = std::min(subsample.height(), strip_height - first);
      std::ranges::fill(uv_sums, 0.0f);
      for (int i = first; i < first + rows; ++i) {
        convert_row(matrix, base::span(rgb_strip).subspan(i * row_floats),
                    width, dest_y.subspan((strip_row + i) * dest_y_stride),
                    uv_sums);
      }
      const int chroma_row = (strip_row + first) / subsample.height();
      auto uv_row = dest_uv.subspan(chroma_row * dest_uv_stride);
      for (int chroma_x = 0; chroma_x < chroma_width; ++chroma_x) {
        const int columns =
            std::min(subsample.width(), width - chroma_x * subsample.width());
        const float pixels = static_cast<float>(columns * rows);
        uv_row[chroma_x * 2] = QuantizeP10(uv_sums[chroma_x * 2] / pixels);
        uv_row[chroma_x * 2 + 1] =
            QuantizeP10(uv_sums[chroma_x * 2 + 1] / pixels);
      }
    }
  }
  return true;
}

}  // namespace media::internals
