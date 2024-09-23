// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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
      tmp_frame->natural_size(),
      tmp_frame->writable_data(VideoFrame::Plane::kARGB),
      VideoFrame::AllocationSize(override_format, tmp_frame->coded_size()),
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

  void* fb_id;
  auto* scratch_space = frame_pool_->GetFrameBuffer(tmp_size, &fb_id);
  if (!scratch_space) {
    return nullptr;
  }

  auto tmp_frame = VideoFrame::WrapExternalData(
      format, coded_size, visible_rect, natural_size, scratch_space, tmp_size,
      base::TimeDelta());
  if (tmp_frame) {
    tmp_frame->AddDestructionObserver(frame_pool_->CreateFrameCallback(fb_id));
  }
  frame_pool_->ReleaseFrameBuffer(fb_id);
  return tmp_frame;
}

scoped_refptr<VideoFrame> VideoFrameConverter::WrapNV12xFrameInI420xFrame(
    const VideoFrame& frame) {
  DCHECK(frame.format() == PIXEL_FORMAT_NV12 ||
         frame.format() == PIXEL_FORMAT_NV12A);

  // What happens below is a bit complicated. We create an I420x frame with
  // freshly allocated U, V planes, while the Y, A planes come from `frame`.
  // This is done to avoid unnecessary copies of the Y, A planes when converting
  // to and from NV12x formats.

  // 1. Allocate scratch space for U, V planes.
  const auto u_plane_size = VideoFrame::PlaneSize(
      PIXEL_FORMAT_I420, VideoFrame::Plane::kU, frame.coded_size());
  const auto v_plane_size = VideoFrame::PlaneSize(
      PIXEL_FORMAT_I420, VideoFrame::Plane::kV, frame.coded_size());

  void* fb_id;
  auto* scratch_space = frame_pool_->GetFrameBuffer(
      u_plane_size.GetArea() + v_plane_size.GetArea(), &fb_id);
  if (!scratch_space) {
    return nullptr;
  }

  // 2. Link Y, A planes of `frame` plus `scratch_space` in a new frame.
  scoped_refptr<media::VideoFrame> wrapped_frame;
  if (IsOpaque(frame.format())) {
    wrapped_frame = VideoFrame::WrapExternalYuvData(
        PIXEL_FORMAT_I420, frame.coded_size(), frame.visible_rect(),
        frame.natural_size(), frame.stride(VideoFrame::Plane::kY),
        u_plane_size.width(), v_plane_size.width(),
        frame.data(VideoFrame::Plane::kY), scratch_space,
        scratch_space + u_plane_size.GetArea(), frame.timestamp());
  } else {
    wrapped_frame = VideoFrame::WrapExternalYuvaData(
        PIXEL_FORMAT_I420A, frame.coded_size(), frame.visible_rect(),
        frame.natural_size(), frame.stride(VideoFrame::Plane::kY),
        u_plane_size.width(), v_plane_size.width(),
        frame.stride(VideoFrame::Plane::kATriPlanar),
        frame.data(VideoFrame::Plane::kY), scratch_space,
        scratch_space + u_plane_size.GetArea(),
        frame.data(VideoFrame::Plane::kATriPlanar), frame.timestamp());
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
      auto tmp_frame = WrapNV12xFrameInI420xFrame(dest_frame);
      if (!tmp_frame) {
        return EncoderStatus::Codes::kScalingError;
      }

      // Scale in I4xxx for simplicity. This will also take care of scaling the
      // Y, A planes directly into `dest_frame` due to the wrapper setup above.
      internals::I4xxxScale(*src_frame, *tmp_frame);
      internals::MergeUV(*tmp_frame, dest_frame);
      return OkStatus();
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
      auto tmp_frame = WrapNV12xFrameInI420xFrame(*src_frame);
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

    default:
      return EncoderStatus(EncoderStatus::Codes::kUnsupportedFrameFormat)
          .WithData("src", src_frame->AsHumanReadableString())
          .WithData("dst", dest_frame.AsHumanReadableString());
  }
}

}  // namespace media
