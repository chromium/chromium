// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame_converter.h"

#include "media/base/video_frame_converter_internals.h"
#include "third_party/libyuv/include/libyuv.h"

namespace media {

constexpr auto kDefaultFiltering = libyuv::kFilterBox;

VideoFrameConverter::VideoFrameConverter() = default;
VideoFrameConverter::~VideoFrameConverter() = default;

EncoderStatus VideoFrameConverter::ConvertAndScale(const VideoFrame& src_frame,
                                                   VideoFrame& dest_frame) {
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

bool VideoFrameConverter::AllocateScratchSpace(size_t needed_size) {
  if (needed_size < scratch_space_size_) {
    return true;
  }
  // Using unchecked malloc allows us to fail gracefully when OOM instead of
  // triggering a crash.
  uint8_t* data;
  if (!base::UncheckedMalloc(needed_size, reinterpret_cast<void**>(&data))) {
    return false;
  }
  scratch_space_.reset(data);
  scratch_space_size_ = needed_size;
  return true;
}

scoped_refptr<VideoFrame> VideoFrameConverter::WrapNV12xFrameInI420xFrame(
    const VideoFrame& frame) {
  DCHECK(frame.format() == PIXEL_FORMAT_NV12 ||
         frame.format() == PIXEL_FORMAT_NV12A);

  // Allocate scratch space for split U, V planes.
  const auto u_plane_size = VideoFrame::PlaneSize(
      PIXEL_FORMAT_I420, VideoFrame::kUPlane, frame.coded_size());
  const auto v_plane_size = VideoFrame::PlaneSize(
      PIXEL_FORMAT_I420, VideoFrame::kVPlane, frame.coded_size());
  if (!AllocateScratchSpace(u_plane_size.GetArea() + v_plane_size.GetArea())) {
    return nullptr;
  }

  // Link Y, A planes of the destination buffer with new temporary frame.
  if (IsOpaque(frame.format())) {
    return VideoFrame::WrapExternalYuvData(
        PIXEL_FORMAT_I420, frame.coded_size(), frame.visible_rect(),
        frame.natural_size(), frame.stride(VideoFrame::kYPlane),
        u_plane_size.width(), v_plane_size.width(),
        frame.data(VideoFrame::kYPlane), scratch_space_.get(),
        scratch_space_.get() + u_plane_size.GetArea(), frame.timestamp());
  }

  return VideoFrame::WrapExternalYuvaData(
      PIXEL_FORMAT_I420A, frame.coded_size(), frame.visible_rect(),
      frame.natural_size(), frame.stride(VideoFrame::kYPlane),
      u_plane_size.width(), v_plane_size.width(),
      frame.stride(VideoFrame::kAPlaneTriPlanar),
      frame.data(VideoFrame::kYPlane), scratch_space_.get(),
      scratch_space_.get() + u_plane_size.GetArea(),
      frame.data(VideoFrame::kAPlaneTriPlanar), frame.timestamp());
}

EncoderStatus VideoFrameConverter::ConvertAndScaleRGB(
    const VideoFrame* src_frame,
    VideoFrame& dest_frame) {
  scoped_refptr<VideoFrame> tmp_frame;
  if (src_frame->visible_rect().size() != dest_frame.visible_rect().size()) {
    tmp_frame = frame_pool_.CreateFrame(
        src_frame->format(), dest_frame.coded_size(), dest_frame.visible_rect(),
        dest_frame.natural_size(), dest_frame.timestamp());
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
      return internals::ARGBToI420x(*src_frame, dest_frame, kDefaultFiltering)
                 ? OkStatus()
                 : EncoderStatus(EncoderStatus::Codes::kFormatConversionError);

    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV12A:
      return internals::ARGBToNV12x(*src_frame, dest_frame, kDefaultFiltering)
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
      internals::I4xxxScale(*src_frame, dest_frame, kDefaultFiltering);
      return OkStatus();

    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV12A: {
      if (src_frame->visible_rect().size() ==
          dest_frame.visible_rect().size()) {
        // Note: libyuv has I422ToNV12 and I444ToNV12 functions; though the I422
        // one just converts to I420 internally first...
        return internals::I420xToNV12x(*src_frame, dest_frame,
                                       kDefaultFiltering)
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
      internals::I4xxxScale(*src_frame, *tmp_frame, kDefaultFiltering);
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
    case PIXEL_FORMAT_I420A: {
      if (src_frame->visible_rect().size() ==
          dest_frame.visible_rect().size()) {
        return internals::NV12xToI420x(*src_frame, dest_frame,
                                       kDefaultFiltering)
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
      internals::I4xxxScale(*tmp_frame, dest_frame, kDefaultFiltering);
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
