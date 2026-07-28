// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "media/base/video_frame_converter.h"

#include "base/trace_event/trace_event.h"
#include "media/base/video_frame_converter_internals.h"
#include "third_party/libyuv/include/libyuv.h"

namespace media {

namespace {

constexpr auto kDefaultFiltering = libyuv::kFilterBox;

std::optional<VideoPixelFormat> GetSourceFormatOverrideForABGRToARGB(
    VideoPixelFormat src_format,
    VideoPixelFormat dest_format) {
  if ((src_format == PIXEL_FORMAT_XBGR || src_format == PIXEL_FORMAT_ABGR) &&
      (dest_format == PIXEL_FORMAT_I444 || dest_format == PIXEL_FORMAT_I444A)) {
    return src_format == PIXEL_FORMAT_XBGR ? PIXEL_FORMAT_XRGB
                                           : PIXEL_FORMAT_ARGB;
  }
  return std::nullopt;
}

// Wraps `tmp_frame` in a new VideoFrame with pixel format `override_format`. No
// ref is taken on `tmp_frame`, so callers must guarantee it outlives the
// return frame.
scoped_refptr<VideoFrame> WrapTempFrameForABGRToARGB(
    VideoPixelFormat override_format,
    scoped_refptr<VideoFrame> tmp_frame) {
  return VideoFrame::WrapExternalData(
      override_format, tmp_frame->coded_size(), tmp_frame->visible_rect(),
      tmp_frame->natural_size(), tmp_frame->data_span(VideoFrame::Plane::kARGB),
      tmp_frame->timestamp());
}

}  // namespace

VideoFrameConverter::VideoFrameConverter()
    : frame_pool_(base::MakeRefCounted<FrameBufferPool>()) {}

VideoFrameConverter::~VideoFrameConverter() {
  frame_pool_->Shutdown();
}

EncoderStatus VideoFrameConverter::ConvertAndScale(const VideoFrame& src_frame,
                                                   VideoFrame& dest_frame) {
  TRACE_EVENT2("media", "ConvertAndScale", "src_frame",
               src_frame.AsHumanReadableString(), "dest_frame",
               dest_frame.AsHumanReadableString());

  if (!IsOpaque(dest_frame.format()) && IsOpaque(src_frame.format())) {
    // We can drop an alpha channel, but we can't make it from nothing.
    return EncoderStatus(EncoderStatus::Codes::kUnsupportedFrameFormat)
        .WithData("src", src_frame.AsHumanReadableString())
        .WithData("dst", dest_frame.AsHumanReadableString());
  }

  switch (src_frame.format()) {
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_ARGB:
      return ConvertAndScaleRGB(&src_frame, dest_frame);

    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_I444A:
      return ConvertAndScaleI4xxx(&src_frame, dest_frame);

    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV12A:
      return ConvertAndScaleNV12x(&src_frame, dest_frame);

    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12:
    case PIXEL_FORMAT_YUV420AP10:
    case PIXEL_FORMAT_YUV422AP10:
    case PIXEL_FORMAT_YUV444AP10:
      return ConvertAndScaleHBD(&src_frame, dest_frame);

    default:
      return EncoderStatus(EncoderStatus::Codes::kUnsupportedFrameFormat)
          .WithData("src", src_frame.AsHumanReadableString())
          .WithData("dst", dest_frame.AsHumanReadableString());
  }
}

scoped_refptr<VideoFrame> VideoFrameConverter::CreateTempFrame(
    VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size) {
  const auto tmp_size = VideoFrame::AllocationSize(format, coded_size);

  void* fb_id = nullptr;
  auto scratch_space = frame_pool_->GetFrameBuffer(tmp_size, &fb_id);
  if (scratch_space.empty()) {
    return nullptr;
  }

  auto tmp_frame = VideoFrame::WrapExternalData(
      format, coded_size, visible_rect, natural_size, scratch_space,
      base::TimeDelta());
  if (tmp_frame) {
    tmp_frame->AddDestructionObserver(frame_pool_->CreateFrameCallback(fb_id));
  }
  frame_pool_->ReleaseFrameBuffer(fb_id);
  return tmp_frame;
}

scoped_refptr<VideoFrame>
VideoFrameConverter::WrapBiplanarFrameInTriplanarFrame(
    const VideoFrame& frame) {
  VideoPixelFormat target_format = PIXEL_FORMAT_UNKNOWN;
  switch (frame.format()) {
    case PIXEL_FORMAT_NV12:
      target_format = PIXEL_FORMAT_I420;
      break;
    case PIXEL_FORMAT_NV12A:
      target_format = PIXEL_FORMAT_I420A;
      break;
    case PIXEL_FORMAT_P010LE:
      target_format = PIXEL_FORMAT_YUV420P10;
      break;
    default:
      NOTREACHED() << "Unsupported biplanar format: " << frame.format();
  }

  // What happens below is a bit complicated. We create a tri-planar frame with
  // freshly allocated U, V planes from `frame_pool_`, while the Y (and A)
  // planes come directly from `frame`. This avoids unnecessary allocations and
  // copies of the Y, A planes when converting to and from bi-planar formats.

  // 1. Allocate scratch space for U, V planes.
  const auto u_plane_size = VideoFrame::PlaneSize(
      target_format, VideoFrame::Plane::kU, frame.coded_size());
  const auto v_plane_size = VideoFrame::PlaneSize(
      target_format, VideoFrame::Plane::kV, frame.coded_size());

  void* fb_id;
  size_t u_size_bytes = u_plane_size.GetArea();
  size_t v_size_bytes = v_plane_size.GetArea();
  auto scratch_space =
      frame_pool_->GetFrameBuffer(u_size_bytes + v_size_bytes, &fb_id);
  if (scratch_space.empty()) {
    return nullptr;
  }

  // 2. Link Y (and A if applicable) planes of `frame` plus `scratch_space` in a
  // new frame.
  scoped_refptr<media::VideoFrame> wrapped_frame;
  if (IsOpaque(target_format)) {
    wrapped_frame = VideoFrame::WrapExternalYuvData(
        target_format, frame.coded_size(), frame.visible_rect(),
        frame.natural_size(), frame.stride(VideoFrame::Plane::kY),
        u_plane_size.width(), v_plane_size.width(),
        frame.data_span(VideoFrame::Plane::kY),
        scratch_space.first(u_size_bytes),
        scratch_space.subspan(u_size_bytes, v_size_bytes), frame.timestamp());
  } else {
    wrapped_frame = VideoFrame::WrapExternalYuvaData(
        target_format, frame.coded_size(), frame.visible_rect(),
        frame.natural_size(), frame.stride(VideoFrame::Plane::kY),
        u_plane_size.width(), v_plane_size.width(),
        frame.stride(VideoFrame::Plane::kATriPlanar),
        frame.data_span(VideoFrame::Plane::kY),
        scratch_space.first(u_size_bytes),
        scratch_space.subspan(u_size_bytes, v_size_bytes),
        frame.data_span(VideoFrame::Plane::kATriPlanar), frame.timestamp());
  }

  if (wrapped_frame) {
    wrapped_frame->AddDestructionObserver(
        frame_pool_->CreateFrameCallback(fb_id));
  }
  frame_pool_->ReleaseFrameBuffer(fb_id);
  return wrapped_frame;
}

EncoderStatus VideoFrameConverter::ConvertAndScaleRGB(
    const VideoFrame* src_frame,
    VideoFrame& dest_frame) {
  scoped_refptr<VideoFrame> tmp_frame;
  if (src_frame->visible_rect().size() != dest_frame.visible_rect().size()) {
    tmp_frame =
        CreateTempFrame(src_frame->format(), dest_frame.coded_size(),
                        dest_frame.visible_rect(), dest_frame.natural_size());
    if (!tmp_frame ||
        !internals::ARGBScale(*src_frame, *tmp_frame, kDefaultFiltering)) {
      return EncoderStatus::Codes::kScalingError;
    }
    src_frame = tmp_frame.get();
  }

  // libyuv's RGB to YUV methods always output BT.601.
  dest_frame.set_color_space(gfx::ColorSpace::CreateREC601());

  switch (dest_frame.format()) {
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_I420A:
      return internals::ARGBToI420x(*src_frame, dest_frame)
                 ? OkStatus()
                 : EncoderStatus(EncoderStatus::Codes::kFormatConversionError);

    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_I444A: {
      // libyuv lacks ABGRToI444 methods, so we convert ABGR to ARGB first.
      scoped_refptr<VideoFrame> argb_tmp_frame;
      if (auto src_format_override = GetSourceFormatOverrideForABGRToARGB(
              src_frame->format(), dest_frame.format())) {
        if (tmp_frame) {
          // If we have an existing `tmp_frame`, we must wrap it to change its
          // pixel format from xBGR to xRGB to avoid unnecessary copies.
          argb_tmp_frame =
              WrapTempFrameForABGRToARGB(*src_format_override, tmp_frame);
        } else {
          // Otherwise, if we don't already have a `tmp_frame` we must create a
          // new one with the correct xRGB pixel format.
          argb_tmp_frame = CreateTempFrame(
              *src_format_override, dest_frame.coded_size(),
              dest_frame.visible_rect(), dest_frame.natural_size());
        }
        if (!argb_tmp_frame ||
            !internals::ABGRToARGB(*src_frame, *argb_tmp_frame)) {
          return EncoderStatus::Codes::kScalingError;
        }
        src_frame = argb_tmp_frame.get();
      }
      return internals::ARGBToI444x(*src_frame, dest_frame)
                 ? OkStatus()
                 : EncoderStatus(EncoderStatus::Codes::kFormatConversionError);
    }

    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV12A:
      return internals::ARGBToNV12x(*src_frame, dest_frame)
                 ? OkStatus()
                 : EncoderStatus(EncoderStatus::Codes::kFormatConversionError);

    case PIXEL_FORMAT_P010LE: {
      auto tmp_nv12_frame =
          CreateTempFrame(PIXEL_FORMAT_NV12, dest_frame.coded_size(),
                          dest_frame.visible_rect(), dest_frame.natural_size());
      if (!tmp_nv12_frame ||
          !internals::ARGBToNV12x(*src_frame, *tmp_nv12_frame)) {
        return EncoderStatus(EncoderStatus::Codes::kFormatConversionError);
      }
      return internals::NV12xToP010(*tmp_nv12_frame, dest_frame)
                 ? OkStatus()
                 : EncoderStatus(EncoderStatus::Codes::kFormatConversionError);
    }

    default:
      return EncoderStatus(EncoderStatus::Codes::kUnsupportedFrameFormat)
          .WithData("src", src_frame->AsHumanReadableString())
          .WithData("dst", dest_frame.AsHumanReadableString());
  }
}

EncoderStatus VideoFrameConverter::ConvertAndScaleI4xxx(
    const VideoFrame* src_frame,
    VideoFrame& dest_frame) {
  // Converting between YUV formats doesn't change the color space.
  dest_frame.set_color_space(src_frame->ColorSpace());

  switch (dest_frame.format()) {
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_I444A:
      internals::I4xxxScale(*src_frame, dest_frame);
      return OkStatus();

    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV12A: {
      if (src_frame->visible_rect().size() ==
          dest_frame.visible_rect().size()) {
        auto convert_fn = src_frame->format() == PIXEL_FORMAT_I420 ||
                                  src_frame->format() == PIXEL_FORMAT_I420A
                              ? internals::I420xToNV12x
                              : internals::I444xToNV12x;
        return convert_fn(*src_frame, dest_frame)
                   ? OkStatus()
                   : EncoderStatus(
                         EncoderStatus::Codes::kFormatConversionError);
      }

      // Create a temporary frame wrapping the destination frame's Y, A planes
      // to avoid unnecessary copies and allocations during the NV12 conversion.
      auto tmp_frame = WrapBiplanarFrameInTriplanarFrame(dest_frame);
      if (!tmp_frame) {
        return EncoderStatus::Codes::kScalingError;
      }

      // Scale in I4xxx for simplicity. This will also take care of scaling the
      // Y, A planes directly into `dest_frame` due to the wrapper setup above.
      internals::I4xxxScale(*src_frame, *tmp_frame);
      internals::MergeUV(*tmp_frame, dest_frame);
      return OkStatus();
    }

    case PIXEL_FORMAT_P010LE: {
      // Due to limited libyuv primitives the following is a bit complicated.
      // 1. Perform scaling from I4xxx to I420
      scoped_refptr<VideoFrame> tmp_i420_frame;
      if (src_frame->visible_rect().size() !=
              dest_frame.visible_rect().size() ||
          (src_frame->format() != PIXEL_FORMAT_I420 &&
           src_frame->format() != PIXEL_FORMAT_I420A)) {
        tmp_i420_frame = CreateTempFrame(
            PIXEL_FORMAT_I420, dest_frame.coded_size(),
            dest_frame.visible_rect(), dest_frame.natural_size());
        if (!tmp_i420_frame) {
          return EncoderStatus::Codes::kScalingError;
        }
        internals::I4xxxScale(*src_frame, *tmp_i420_frame);
        src_frame = tmp_i420_frame.get();
      }

      // 2. Wrap Y plane from the P010 destination plus UV scratch space into a
      // new temporary YUV420P10 frame to avoid Y plane copy.
      auto tmp_frame = WrapBiplanarFrameInTriplanarFrame(dest_frame);
      if (!tmp_frame) {
        return EncoderStatus::Codes::kScalingError;
      }

      // 3. Convert I420x to YUV420P10.
      internals::Convert8To16Plane(*src_frame, *tmp_frame);

      // 4. Convert YUV420P10 to P010.
      return internals::I4xxxPxxToP010(*tmp_frame, dest_frame)
                 ? OkStatus()
                 : EncoderStatus(EncoderStatus::Codes::kFormatConversionError);
    }

    default:
      return EncoderStatus(EncoderStatus::Codes::kUnsupportedFrameFormat)
          .WithData("src", src_frame->AsHumanReadableString())
          .WithData("dst", dest_frame.AsHumanReadableString());
  }
}

EncoderStatus VideoFrameConverter::ConvertAndScaleNV12x(
    const VideoFrame* src_frame,
    VideoFrame& dest_frame) {
  // Converting between YUV formats doesn't change the color space.
  dest_frame.set_color_space(src_frame->ColorSpace());

  switch (dest_frame.format()) {
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_I444A: {
      if ((src_frame->visible_rect().size() ==
           dest_frame.visible_rect().size()) &&
          // libyuv doesn't have a NV12ToI444 method.
          (dest_frame.format() == PIXEL_FORMAT_I420 ||
           dest_frame.format() == PIXEL_FORMAT_I420A)) {
        return internals::NV12xToI420x(*src_frame, dest_frame)
                   ? OkStatus()
                   : EncoderStatus(
                         EncoderStatus::Codes::kFormatConversionError);
      }

      // Create a temporary frame wrapping the source frames's Y, A planes
      // to avoid unnecessary copies and allocations during the NV12 conversion.
      auto tmp_frame = WrapBiplanarFrameInTriplanarFrame(*src_frame);
      if (!tmp_frame) {
        return EncoderStatus::Codes::kScalingError;
      }

      internals::SplitUV(*src_frame, *tmp_frame);

      // Scale in I4xxx for simplicity. This will also take care of scaling the
      // Y, A planes directly into `dest_frame` due to the wrapper setup above.
      internals::I4xxxScale(*tmp_frame, dest_frame);
      return OkStatus();
    }

    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV12A:
      return internals::NV12xScale(*src_frame, dest_frame, kDefaultFiltering)
                 ? OkStatus()
                 : EncoderStatus(EncoderStatus::Codes::kScalingError);

    case PIXEL_FORMAT_P010LE: {
      scoped_refptr<VideoFrame> scaled_nv12_frame;
      if (src_frame->visible_rect().size() !=
          dest_frame.visible_rect().size()) {
        scaled_nv12_frame = CreateTempFrame(
            src_frame->format(), dest_frame.coded_size(),
            dest_frame.visible_rect(), dest_frame.natural_size());
        if (!scaled_nv12_frame ||
            !internals::NV12xScale(*src_frame, *scaled_nv12_frame,
                                   kDefaultFiltering)) {
          return EncoderStatus::Codes::kScalingError;
        }
        src_frame = scaled_nv12_frame.get();
      }
      return internals::NV12xToP010(*src_frame, dest_frame)
                 ? OkStatus()
                 : EncoderStatus(EncoderStatus::Codes::kFormatConversionError);
    }

    default:
      return EncoderStatus(EncoderStatus::Codes::kUnsupportedFrameFormat)
          .WithData("src", src_frame->AsHumanReadableString())
          .WithData("dst", dest_frame.AsHumanReadableString());
  }
}

EncoderStatus VideoFrameConverter::ConvertAndScaleHBD(
    const VideoFrame* src_frame,
    VideoFrame& dest_frame) {
  dest_frame.set_color_space(src_frame->ColorSpace());

  VideoPixelFormat target_hbd_format = PIXEL_FORMAT_UNKNOWN;
  bool is_12bit = src_frame->format() == PIXEL_FORMAT_YUV420P12 ||
                  src_frame->format() == PIXEL_FORMAT_YUV422P12 ||
                  src_frame->format() == PIXEL_FORMAT_YUV444P12;
  bool has_alpha = !IsOpaque(src_frame->format());

  // Map the 8-bit destination format to a matching 16-bit high bit depth layout
  // (matching subsampling and alpha channel) for processing.
  switch (dest_frame.format()) {
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV12A:
    case PIXEL_FORMAT_P010LE:
      target_hbd_format = has_alpha ? PIXEL_FORMAT_YUV420AP10
                                    : (is_12bit ? PIXEL_FORMAT_YUV420P12
                                                : PIXEL_FORMAT_YUV420P10);
      break;
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I422A:
      target_hbd_format = has_alpha ? PIXEL_FORMAT_YUV422AP10
                                    : (is_12bit ? PIXEL_FORMAT_YUV422P12
                                                : PIXEL_FORMAT_YUV422P10);
      break;
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_I444A:
      target_hbd_format = has_alpha ? PIXEL_FORMAT_YUV444AP10
                                    : (is_12bit ? PIXEL_FORMAT_YUV444P12
                                                : PIXEL_FORMAT_YUV444P10);
      break;
    default:
      return EncoderStatus(EncoderStatus::Codes::kUnsupportedFrameFormat)
          .WithData("src", src_frame->AsHumanReadableString())
          .WithData("dst", dest_frame.AsHumanReadableString());
  }

  // Perform spatial scaling and chroma subsampling conversion in 16-bit space.
  scoped_refptr<VideoFrame> scaled_hbd_frame;
  if (src_frame->format() != target_hbd_format ||
      src_frame->visible_rect().size() != dest_frame.visible_rect().size()) {
    scaled_hbd_frame =
        CreateTempFrame(target_hbd_format, dest_frame.coded_size(),
                        dest_frame.visible_rect(), dest_frame.natural_size());
    if (!scaled_hbd_frame) {
      return EncoderStatus::Codes::kScalingError;
    }
    internals::I4xxxScale_16(*src_frame, *scaled_hbd_frame);
    src_frame = scaled_hbd_frame.get();
  }

  // NV12/NV12A output requires down-converting to 8-bit first. We wrap
  // `dest_frame` in an I420x frame so that Y and A planes are down-converted
  // directly into `dest_frame`, avoiding extra allocations and copies.
  if (dest_frame.format() == PIXEL_FORMAT_NV12 ||
      dest_frame.format() == PIXEL_FORMAT_NV12A) {
    auto tmp_frame = WrapBiplanarFrameInTriplanarFrame(dest_frame);
    if (!tmp_frame) {
      return EncoderStatus::Codes::kScalingError;
    }
    internals::Convert16To8Plane(*src_frame, *tmp_frame);
    internals::MergeUV(*tmp_frame, dest_frame);
    return OkStatus();
  }

  if (dest_frame.format() == PIXEL_FORMAT_P010LE) {
    return internals::I4xxxPxxToP010(*src_frame, dest_frame)
               ? OkStatus()
               : EncoderStatus(EncoderStatus::Codes::kFormatConversionError);
  }

  // Down-convert each plane from 16-bit to matching 8-bit destination layout.
  internals::Convert16To8Plane(*src_frame, dest_frame);
  return OkStatus();
}

}  // namespace media
