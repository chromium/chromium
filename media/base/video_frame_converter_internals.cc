// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame_converter_internals.h"

#include "third_party/libyuv/include/libyuv.h"

namespace {

bool IsSupportedRGBFormat(media::VideoPixelFormat format) {
  return format == media::PIXEL_FORMAT_XBGR ||
         format == media::PIXEL_FORMAT_XRGB ||
         format == media::PIXEL_FORMAT_ABGR ||
         format == media::PIXEL_FORMAT_ARGB;
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

}  // namespace

namespace media::internals {

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

bool ARGBToI420x(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK(IsSupportedRGBFormat(src_frame.format()));
  DCHECK(dest_frame.format() == PIXEL_FORMAT_I420 ||
         dest_frame.format() == PIXEL_FORMAT_I420A);
  auto convert_fn = (src_frame.format() == PIXEL_FORMAT_XBGR ||
                     src_frame.format() == PIXEL_FORMAT_ABGR)
                        ? libyuv::ABGRToI420
                        : libyuv::ARGBToI420;
  if (convert_fn(src_frame.visible_data(VideoFrame::Plane::kARGB),
                 src_frame.stride(VideoFrame::Plane::kARGB),
                 dest_frame.GetWritableVisibleData(VideoFrame::Plane::kY),
                 dest_frame.stride(VideoFrame::Plane::kY),
                 dest_frame.GetWritableVisibleData(VideoFrame::Plane::kU),
                 dest_frame.stride(VideoFrame::Plane::kU),
                 dest_frame.GetWritableVisibleData(VideoFrame::Plane::kV),
                 dest_frame.stride(VideoFrame::Plane::kV),
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

bool ARGBToI444x(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK(IsSupportedRGBFormat(src_frame.format()));
  DCHECK(src_frame.format() == PIXEL_FORMAT_ARGB ||
         src_frame.format() == PIXEL_FORMAT_XRGB);
  DCHECK(dest_frame.format() == PIXEL_FORMAT_I444 ||
         dest_frame.format() == PIXEL_FORMAT_I444A);
  if (libyuv::ARGBToI444(
          src_frame.visible_data(VideoFrame::Plane::kARGB),
          src_frame.stride(VideoFrame::Plane::kARGB),
          dest_frame.GetWritableVisibleData(VideoFrame::Plane::kY),
          dest_frame.stride(VideoFrame::Plane::kY),
          dest_frame.GetWritableVisibleData(VideoFrame::Plane::kU),
          dest_frame.stride(VideoFrame::Plane::kU),
          dest_frame.GetWritableVisibleData(VideoFrame::Plane::kV),
          dest_frame.stride(VideoFrame::Plane::kV),
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

bool ARGBToNV12x(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK(IsSupportedRGBFormat(src_frame.format()));
  DCHECK(dest_frame.format() == PIXEL_FORMAT_NV12 ||
         dest_frame.format() == PIXEL_FORMAT_NV12A);

  auto convert_fn = (src_frame.format() == PIXEL_FORMAT_XBGR ||
                     src_frame.format() == PIXEL_FORMAT_ABGR)
                        ? libyuv::ABGRToNV12
                        : libyuv::ARGBToNV12;
  if (convert_fn(src_frame.visible_data(VideoFrame::Plane::kARGB),
                 src_frame.stride(VideoFrame::Plane::kARGB),
                 dest_frame.GetWritableVisibleData(VideoFrame::Plane::kY),
                 dest_frame.stride(VideoFrame::Plane::kY),
                 dest_frame.GetWritableVisibleData(VideoFrame::Plane::kUV),
                 dest_frame.stride(VideoFrame::Plane::kUV),
                 dest_frame.visible_rect().width(),
                 dest_frame.visible_rect().height()) != 0) {
    return false;
  }
  if (dest_frame.format() == PIXEL_FORMAT_NV12) {
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

bool ABGRToARGB(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK(src_frame.format() == PIXEL_FORMAT_ABGR ||
         src_frame.format() == PIXEL_FORMAT_XBGR);
  DCHECK(dest_frame.format() == PIXEL_FORMAT_ARGB ||
         dest_frame.format() == PIXEL_FORMAT_XRGB);
  DCHECK_EQ(src_frame.visible_rect().size(), dest_frame.visible_rect().size());
  return libyuv::ABGRToARGB(
             src_frame.visible_data(VideoFrame::Plane::kARGB),
             src_frame.stride(VideoFrame::Plane::kARGB),
             dest_frame.GetWritableVisibleData(VideoFrame::Plane::kARGB),
             dest_frame.stride(VideoFrame::Plane::kARGB),
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
         src_frame.format() == PIXEL_FORMAT_I420A);
  DCHECK_EQ(dest_frame.format(), PIXEL_FORMAT_YUV420P10);
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

bool I420xToNV12x(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK(src_frame.format() == PIXEL_FORMAT_I420 ||
         src_frame.format() == PIXEL_FORMAT_I420A);
  DCHECK(dest_frame.format() == PIXEL_FORMAT_NV12 ||
         dest_frame.format() == PIXEL_FORMAT_NV12A);
  DCHECK_EQ(src_frame.visible_rect().size(), dest_frame.visible_rect().size());
  if (libyuv::I420ToNV12(
          src_frame.visible_data(VideoFrame::Plane::kY),
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
  if (dest_frame.format() == PIXEL_FORMAT_NV12) {
    return true;
  }
  libyuv::CopyPlane(
      src_frame.visible_data(VideoFrame::Plane::kA),
      src_frame.stride(VideoFrame::Plane::kA),
      dest_frame.GetWritableVisibleData(VideoFrame::Plane::kATriPlanar),
      dest_frame.stride(VideoFrame::Plane::kATriPlanar),
      dest_frame.visible_rect().width(), dest_frame.visible_rect().height());
  return true;
}

bool I444xToNV12x(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK(src_frame.format() == PIXEL_FORMAT_I444 ||
         src_frame.format() == PIXEL_FORMAT_I444A);
  DCHECK(dest_frame.format() == PIXEL_FORMAT_NV12 ||
         dest_frame.format() == PIXEL_FORMAT_NV12A);
  DCHECK_EQ(src_frame.visible_rect().size(), dest_frame.visible_rect().size());
  if (libyuv::I444ToNV12(
          src_frame.visible_data(VideoFrame::Plane::kY),
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
  if (dest_frame.format() == PIXEL_FORMAT_NV12) {
    return true;
  }
  libyuv::CopyPlane(
      src_frame.visible_data(VideoFrame::Plane::kA),
      src_frame.stride(VideoFrame::Plane::kA),
      dest_frame.GetWritableVisibleData(VideoFrame::Plane::kATriPlanar),
      dest_frame.stride(VideoFrame::Plane::kATriPlanar),
      dest_frame.visible_rect().width(), dest_frame.visible_rect().height());
  return true;
}

void MergeUV(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK(src_frame.format() == PIXEL_FORMAT_I420 ||
         src_frame.format() == PIXEL_FORMAT_I420A);
  DCHECK(dest_frame.format() == PIXEL_FORMAT_NV12 ||
         dest_frame.format() == PIXEL_FORMAT_NV12A);
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
         src_frame.format() == PIXEL_FORMAT_NV12A);
  DCHECK(dest_frame.format() == PIXEL_FORMAT_I420 ||
         dest_frame.format() == PIXEL_FORMAT_I420A);
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

bool NV12xScale(const VideoFrame& src_frame,
                VideoFrame& dest_frame,
                libyuv::FilterMode filter) {
  DCHECK(src_frame.format() == PIXEL_FORMAT_NV12 ||
         src_frame.format() == PIXEL_FORMAT_NV12A);
  DCHECK(dest_frame.format() == PIXEL_FORMAT_NV12 ||
         dest_frame.format() == PIXEL_FORMAT_NV12A);
  if (libyuv::NV12Scale(
          src_frame.visible_data(VideoFrame::Plane::kY),
          src_frame.stride(VideoFrame::Plane::kY),
          src_frame.visible_data(VideoFrame::Plane::kUV),
          src_frame.stride(VideoFrame::Plane::kUV),
          src_frame.visible_rect().width(), src_frame.visible_rect().height(),
          dest_frame.GetWritableVisibleData(VideoFrame::Plane::kY),
          dest_frame.stride(VideoFrame::Plane::kY),
          dest_frame.GetWritableVisibleData(VideoFrame::Plane::kUV),
          dest_frame.stride(VideoFrame::Plane::kUV),
          dest_frame.visible_rect().width(), dest_frame.visible_rect().height(),
          filter) != 0) {
    return false;
  }
  if (dest_frame.format() == PIXEL_FORMAT_NV12) {
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

bool NV12xToI420x(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK(src_frame.format() == PIXEL_FORMAT_NV12 ||
         src_frame.format() == PIXEL_FORMAT_NV12A);
  DCHECK(dest_frame.format() == PIXEL_FORMAT_I420 ||
         dest_frame.format() == PIXEL_FORMAT_I420A);
  DCHECK_EQ(src_frame.visible_rect().size(), dest_frame.visible_rect().size());
  if (libyuv::NV12ToI420(
          src_frame.visible_data(VideoFrame::Plane::kY),
          src_frame.stride(VideoFrame::Plane::kY),
          src_frame.visible_data(VideoFrame::Plane::kUV),
          src_frame.stride(VideoFrame::Plane::kUV),
          dest_frame.GetWritableVisibleData(VideoFrame::Plane::kY),
          dest_frame.stride(VideoFrame::Plane::kY),
          dest_frame.GetWritableVisibleData(VideoFrame::Plane::kU),
          dest_frame.stride(VideoFrame::Plane::kU),
          dest_frame.GetWritableVisibleData(VideoFrame::Plane::kV),
          dest_frame.stride(VideoFrame::Plane::kV),
          dest_frame.visible_rect().width(),
          dest_frame.visible_rect().height()) != 0) {
    return false;
  }
  if (dest_frame.format() == PIXEL_FORMAT_I420) {
    return true;
  }
  libyuv::CopyPlane(src_frame.visible_data(VideoFrame::Plane::kATriPlanar),
                    src_frame.stride(VideoFrame::Plane::kATriPlanar),
                    dest_frame.GetWritableVisibleData(VideoFrame::Plane::kA),
                    dest_frame.stride(VideoFrame::Plane::kA),
                    dest_frame.visible_rect().width(),
                    dest_frame.visible_rect().height());
  return true;
}

bool NV12xToP010(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK(src_frame.format() == PIXEL_FORMAT_NV12 ||
         src_frame.format() == PIXEL_FORMAT_NV12A);
  DCHECK_EQ(dest_frame.format(), PIXEL_FORMAT_P010LE);
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

bool I4xxxPxxToP010(const VideoFrame& src_frame, VideoFrame& dest_frame) {
  DCHECK(src_frame.format() == PIXEL_FORMAT_YUV420P10 ||
         src_frame.format() == PIXEL_FORMAT_YUV422P10 ||
         src_frame.format() == PIXEL_FORMAT_YUV444P10 ||
         src_frame.format() == PIXEL_FORMAT_YUV420P12 ||
         src_frame.format() == PIXEL_FORMAT_YUV422P12 ||
         src_frame.format() == PIXEL_FORMAT_YUV444P12 ||
         src_frame.format() == PIXEL_FORMAT_YUV420AP10 ||
         src_frame.format() == PIXEL_FORMAT_YUV422AP10 ||
         src_frame.format() == PIXEL_FORMAT_YUV444AP10);
  DCHECK_EQ(dest_frame.format(), PIXEL_FORMAT_P010LE);
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

}  // namespace media::internals
