// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame_converter.h"

#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame_converter_internals.h"
#include "media/base/video_util.h"
#include "third_party/libyuv/include/libyuv.h"

namespace media {

namespace {

constexpr auto kDefaultFiltering = libyuv::kFilterBox;

struct Biplanar10BitFormatFamily {
  VideoPixelFormat biplanar_10bit;
  VideoPixelFormat planar_8bit;
  VideoPixelFormat biplanar_8bit;
};

constexpr Biplanar10BitFormatFamily kBiplanar10BitFormatFamilies[] = {
    {PIXEL_FORMAT_P010LE, PIXEL_FORMAT_I420, PIXEL_FORMAT_NV12},
    {PIXEL_FORMAT_P210LE, PIXEL_FORMAT_I422, PIXEL_FORMAT_NV16},
    {PIXEL_FORMAT_P410LE, PIXEL_FORMAT_I444, PIXEL_FORMAT_NV24},
};

const Biplanar10BitFormatFamily& Biplanar10BitFormatFamilyFor(
    VideoPixelFormat format) {
  for (const auto& candidate : kBiplanar10BitFormatFamilies) {
    if (candidate.biplanar_10bit == format) {
      return candidate;
    }
  }
  NOTREACHED();
}

}  // namespace

VideoFrameConverter::VideoFrameConverter()
    : frame_pool_(base::MakeRefCounted<FrameBufferPool>()) {}

VideoFrameConverter::~VideoFrameConverter() {
  frame_pool_->Shutdown();
}

// static
gfx::ColorSpace VideoFrameConverter::GetDestinationColorSpace(
    const VideoFrame& src_frame) {
  const auto& src_cs = src_frame.ColorSpace();
  if (!IsRGB(src_frame.format())) {
    return src_cs;  // YUV color spaces are unchanged.
  }

  if (!base::FeatureList::IsEnabled(kAccurateVideoFrameConverterColorSpace)) {
    return gfx::ColorSpace::CreateREC601();
  }

  // Invalid color spaces are coerced to limited range BT.709.
  if (!src_cs.IsValid()) {
    return gfx::ColorSpace::CreateREC709();
  }

  const auto primary_id = src_cs.GetPrimaryID();
  const auto transfer_id = src_cs.GetTransferID();
  const auto range_id = src_cs.GetRangeID();

  gfx::ColorSpace::MatrixID matrix_id;
  if (transfer_id == gfx::ColorSpace::TransferID::PQ ||
      transfer_id == gfx::ColorSpace::TransferID::HLG ||
      primary_id == gfx::ColorSpace::PrimaryID::BT2020) {
    matrix_id = gfx::ColorSpace::MatrixID::BT2020_NCL;
  } else if (primary_id == gfx::ColorSpace::PrimaryID::SMPTE170M ||
             primary_id == gfx::ColorSpace::PrimaryID::BT470M ||
             primary_id == gfx::ColorSpace::PrimaryID::BT470BG) {
    matrix_id = gfx::ColorSpace::MatrixID::SMPTE170M;
  } else {
    // Default to BT.709 matrix for BT.709, P3, and any other SDR gamuts.
    matrix_id = gfx::ColorSpace::MatrixID::BT709;
  }

  return gfx::ColorSpace(primary_id, transfer_id, matrix_id, range_id);
}

EncoderStatus VideoFrameConverter::ConvertAndScale(const VideoFrame& src_frame,
                                                   VideoFrame& dest_frame) {
  // Ensure color space and HDR metadata is propagated as necessary.
  const auto dest_color_space = GetDestinationColorSpace(src_frame);
  dest_frame.set_color_space(dest_color_space);
  dest_frame.set_hdr_metadata(
      dest_color_space.IsHDR() ? src_frame.hdr_metadata() : gfx::HDRMetadata());

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
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I422A:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_I444A:
      return ConvertAndScaleI4xxx(&src_frame, dest_frame);

    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV12A:
    case PIXEL_FORMAT_NV16:
    case PIXEL_FORMAT_NV24:
      return ConvertAndScaleNVxx(&src_frame, dest_frame);

    case PIXEL_FORMAT_P010LE:
    case PIXEL_FORMAT_P210LE:
    case PIXEL_FORMAT_P410LE:
      return ConvertAndScalePx10(&src_frame, dest_frame);

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
    case PIXEL_FORMAT_NV16:
      target_format = PIXEL_FORMAT_I422;
      break;
    case PIXEL_FORMAT_NV24:
      target_format = PIXEL_FORMAT_I444;
      break;
    case PIXEL_FORMAT_P010LE:
      target_format = PIXEL_FORMAT_YUV420P10;
      break;
    case PIXEL_FORMAT_P210LE:
      target_format = PIXEL_FORMAT_YUV422P10;
      break;
    case PIXEL_FORMAT_P410LE:
      target_format = PIXEL_FORMAT_YUV444P10;
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
    tmp_frame->set_color_space(src_frame->ColorSpace());
    src_frame = tmp_frame.get();
  }

  const bool is_abgr = src_frame->format() == PIXEL_FORMAT_XBGR ||
                       src_frame->format() == PIXEL_FORMAT_ABGR;
  const auto* matrix = internals::GetArgbConstantsForColorSpace(
      dest_frame.ColorSpace(), is_abgr);

  switch (dest_frame.format()) {
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_I420A:
      return internals::ARGBToI420x(*src_frame, dest_frame, matrix)
                 ? OkStatus()
                 : EncoderStatus(EncoderStatus::Codes::kFormatConversionError);

    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I422A:
      return internals::ARGBToI422x(*src_frame, dest_frame, matrix)
                 ? OkStatus()
                 : EncoderStatus(EncoderStatus::Codes::kFormatConversionError);

    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_I444A:
      return internals::ARGBToI444x(*src_frame, dest_frame, matrix)
                 ? OkStatus()
                 : EncoderStatus(EncoderStatus::Codes::kFormatConversionError);

    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV12A:
      return internals::ARGBToNV12x(*src_frame, dest_frame, matrix)
                 ? OkStatus()
                 : EncoderStatus(EncoderStatus::Codes::kFormatConversionError);

    case PIXEL_FORMAT_P010LE: {
      auto tmp_nv12_frame =
          CreateTempFrame(PIXEL_FORMAT_NV12, dest_frame.coded_size(),
                          dest_frame.visible_rect(), dest_frame.natural_size());
      if (!tmp_nv12_frame ||
          !internals::ARGBToNV12x(*src_frame, *tmp_nv12_frame, matrix)) {
        return EncoderStatus(EncoderStatus::Codes::kFormatConversionError);
      }
      return internals::NVxxToPx10(*tmp_nv12_frame, dest_frame)
                 ? OkStatus()
                 : EncoderStatus(EncoderStatus::Codes::kFormatConversionError);
    }

    case PIXEL_FORMAT_NV16:
    case PIXEL_FORMAT_NV24: {
      auto wrapped_frame = WrapBiplanarFrameInTriplanarFrame(dest_frame);
      const bool converted =
          wrapped_frame &&
          (dest_frame.format() == PIXEL_FORMAT_NV16
               ? internals::ARGBToI422x(*src_frame, *wrapped_frame, matrix)
               : internals::ARGBToI444x(*src_frame, *wrapped_frame, matrix));
      if (!converted) {
        return EncoderStatus(EncoderStatus::Codes::kFormatConversionError);
      }
      internals::MergeUV(*wrapped_frame, dest_frame);
      return OkStatus();
    }

    case PIXEL_FORMAT_P210LE:
    case PIXEL_FORMAT_P410LE: {
      const VideoPixelFormat tmp_format =
          dest_frame.format() == PIXEL_FORMAT_P210LE ? PIXEL_FORMAT_I422
                                                     : PIXEL_FORMAT_I444;
      auto conversion_frame =
          CreateTempFrame(tmp_format, dest_frame.coded_size(),
                          dest_frame.visible_rect(), dest_frame.natural_size());
      const bool converted =
          conversion_frame &&
          (tmp_format == PIXEL_FORMAT_I422
               ? internals::ARGBToI422x(*src_frame, *conversion_frame, matrix)
               : internals::ARGBToI444x(*src_frame, *conversion_frame, matrix));
      if (!converted) {
        return EncoderStatus(EncoderStatus::Codes::kFormatConversionError);
      }
      return ConvertAndScaleI4xxx(conversion_frame.get(), dest_frame);
    }

    case PIXEL_FORMAT_YUV420P10: {
      auto tmp_i420_frame =
          CreateTempFrame(PIXEL_FORMAT_I420, dest_frame.coded_size(),
                          dest_frame.visible_rect(), dest_frame.natural_size());
      if (!tmp_i420_frame ||
          !internals::ARGBToI420x(*src_frame, *tmp_i420_frame, matrix)) {
        return EncoderStatus(EncoderStatus::Codes::kFormatConversionError);
      }
      internals::Convert8To16Plane(*tmp_i420_frame, dest_frame);
      return OkStatus();
    }

    case PIXEL_FORMAT_YUV444P10: {
      auto tmp_i444_frame =
          CreateTempFrame(PIXEL_FORMAT_I444, dest_frame.coded_size(),
                          dest_frame.visible_rect(), dest_frame.natural_size());
      if (!tmp_i444_frame ||
          !internals::ARGBToI444x(*src_frame, *tmp_i444_frame, matrix)) {
        return EncoderStatus(EncoderStatus::Codes::kFormatConversionError);
      }
      internals::Convert8To16Plane(*tmp_i444_frame, dest_frame);
      return OkStatus();
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
  switch (dest_frame.format()) {
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I422A:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_I444A:
      internals::I4xxxScale(*src_frame, dest_frame);
      return OkStatus();

    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV12A:
    case PIXEL_FORMAT_NV16:
    case PIXEL_FORMAT_NV24: {
      const auto src_sampling =
          VideoPixelFormatToChromaSampling(src_frame->format());
      const auto dest_sampling =
          VideoPixelFormatToChromaSampling(dest_frame.format());
      if (src_frame->visible_rect().size() ==
              dest_frame.visible_rect().size() &&
          (src_sampling == dest_sampling ||
           ((dest_frame.format() == PIXEL_FORMAT_NV12 ||
             dest_frame.format() == PIXEL_FORMAT_NV12A) &&
            (src_sampling == VideoChromaSampling::k420 ||
             src_sampling == VideoChromaSampling::k444)))) {
        return internals::I4xxxToNVxx(*src_frame, dest_frame)
                   ? OkStatus()
                   : EncoderStatus(
                         EncoderStatus::Codes::kFormatConversionError);
      }

      // Create a temporary frame wrapping the destination frame's Y, A planes
      // to avoid unnecessary copies and allocations during biplanar conversion.
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

    case PIXEL_FORMAT_P010LE:
    case PIXEL_FORMAT_P210LE:
    case PIXEL_FORMAT_P410LE: {
      // Due to limited libyuv primitives the following is a bit complicated.
      // 1. Perform scaling from I4xxx to matching 8-bit planar.
      const auto& format_family =
          Biplanar10BitFormatFamilyFor(dest_frame.format());
      scoped_refptr<VideoFrame> intermediate_frame;
      if (src_frame->visible_rect().size() !=
              dest_frame.visible_rect().size() ||
          VideoPixelFormatToChromaSampling(src_frame->format()) !=
              VideoPixelFormatToChromaSampling(format_family.planar_8bit)) {
        intermediate_frame = CreateTempFrame(
            format_family.planar_8bit, dest_frame.coded_size(),
            dest_frame.visible_rect(), dest_frame.natural_size());
        if (!intermediate_frame) {
          return EncoderStatus::Codes::kScalingError;
        }
        internals::I4xxxScale(*src_frame, *intermediate_frame);
        src_frame = intermediate_frame.get();
      }

      // 2. Wrap Y plane from the biplanar destination plus UV scratch space
      // into a temporary planar 10-bit frame to avoid Y plane copy.
      auto tmp_frame = WrapBiplanarFrameInTriplanarFrame(dest_frame);
      if (!tmp_frame) {
        return EncoderStatus::Codes::kScalingError;
      }

      // 3. Convert 8-bit planar to 10-bit planar.
      internals::Convert8To16Plane(*src_frame, *tmp_frame);

      // 4. Convert planar 10-bit to biplanar P010 / P210 / P410.
      return internals::I4xxxPxxToPx10(*tmp_frame, dest_frame)
                 ? OkStatus()
                 : EncoderStatus(EncoderStatus::Codes::kFormatConversionError);
    }

    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV444P10: {
      auto tmp_format = dest_frame.format() == PIXEL_FORMAT_YUV420P10
                            ? PIXEL_FORMAT_I420
                            : PIXEL_FORMAT_I444;
      if (src_frame->format() == tmp_format &&
          src_frame->visible_rect().size() ==
              dest_frame.visible_rect().size()) {
        internals::Convert8To16Plane(*src_frame, dest_frame);
        return OkStatus();
      }
      auto tmp_frame =
          CreateTempFrame(tmp_format, dest_frame.coded_size(),
                          dest_frame.visible_rect(), dest_frame.natural_size());
      if (!tmp_frame) {
        return EncoderStatus::Codes::kScalingError;
      }
      internals::I4xxxScale(*src_frame, *tmp_frame);
      internals::Convert8To16Plane(*tmp_frame, dest_frame);
      return OkStatus();
    }

    default:
      return EncoderStatus(EncoderStatus::Codes::kUnsupportedFrameFormat)
          .WithData("src", src_frame->AsHumanReadableString())
          .WithData("dst", dest_frame.AsHumanReadableString());
  }
}

EncoderStatus VideoFrameConverter::ConvertAndScaleNVxx(
    const VideoFrame* src_frame,
    VideoFrame& dest_frame) {
  DCHECK(src_frame->format() == PIXEL_FORMAT_NV12 ||
         src_frame->format() == PIXEL_FORMAT_NV12A ||
         src_frame->format() == PIXEL_FORMAT_NV16 ||
         src_frame->format() == PIXEL_FORMAT_NV24);

  // De-interleave through a triplanar wrapper so the planar path can handle
  // scaling and chroma conversion without copying source Y/A planes.
  auto convert_via_planar = [&]() -> EncoderStatus {
    auto tmp_frame = WrapBiplanarFrameInTriplanarFrame(*src_frame);
    if (!tmp_frame) {
      return EncoderStatus::Codes::kScalingError;
    }
    internals::SplitUV(*src_frame, *tmp_frame);
    tmp_frame->set_color_space(src_frame->ColorSpace());
    return ConvertAndScaleI4xxx(tmp_frame.get(), dest_frame);
  };

  switch (dest_frame.format()) {
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I422A:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_I444A: {
      if (src_frame->visible_rect().size() ==
              dest_frame.visible_rect().size() &&
          VideoPixelFormatToChromaSampling(src_frame->format()) ==
              VideoPixelFormatToChromaSampling(dest_frame.format())) {
        internals::NVxxToI4xxx(*src_frame, dest_frame);
        return OkStatus();
      }
      return convert_via_planar();
    }

    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV12A:
    case PIXEL_FORMAT_NV16:
    case PIXEL_FORMAT_NV24:
      return internals::NVxxScale(*src_frame, dest_frame, kDefaultFiltering)
                 ? OkStatus()
                 : EncoderStatus(EncoderStatus::Codes::kScalingError);

    case PIXEL_FORMAT_P010LE:
    case PIXEL_FORMAT_P210LE:
    case PIXEL_FORMAT_P410LE: {
      scoped_refptr<VideoFrame> intermediate_frame;
      if (src_frame->visible_rect().size() !=
              dest_frame.visible_rect().size() ||
          VideoPixelFormatToChromaSampling(src_frame->format()) !=
              VideoPixelFormatToChromaSampling(dest_frame.format())) {
        const auto& format_family =
            Biplanar10BitFormatFamilyFor(dest_frame.format());
        intermediate_frame = CreateTempFrame(
            format_family.biplanar_8bit, dest_frame.coded_size(),
            dest_frame.visible_rect(), dest_frame.natural_size());
        if (!intermediate_frame) {
          return EncoderStatus::Codes::kScalingError;
        }
        if (!internals::NVxxScale(*src_frame, *intermediate_frame,
                                  kDefaultFiltering)) {
          return EncoderStatus::Codes::kScalingError;
        }
        src_frame = intermediate_frame.get();
      }
      return internals::NVxxToPx10(*src_frame, dest_frame)
                 ? OkStatus()
                 : EncoderStatus(EncoderStatus::Codes::kFormatConversionError);
    }

    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV444P10:
      return convert_via_planar();

    default:
      return EncoderStatus(EncoderStatus::Codes::kUnsupportedFrameFormat)
          .WithData("src", src_frame->AsHumanReadableString())
          .WithData("dst", dest_frame.AsHumanReadableString());
  }
}

EncoderStatus VideoFrameConverter::ConvertAndScaleHBD(
    const VideoFrame* src_frame,
    VideoFrame& dest_frame) {
  VideoPixelFormat target_hbd_format = PIXEL_FORMAT_UNKNOWN;
  bool is_12bit = src_frame->format() == PIXEL_FORMAT_YUV420P12 ||
                  src_frame->format() == PIXEL_FORMAT_YUV422P12 ||
                  src_frame->format() == PIXEL_FORMAT_YUV444P12;
  const bool preserve_alpha = !IsOpaque(dest_frame.format());

  // If destination is already an HBD planar format, scale and convert directly.
  if (dest_frame.format() == PIXEL_FORMAT_YUV420P10 ||
      dest_frame.format() == PIXEL_FORMAT_YUV444P10) {
    internals::I4xxxScale_16(*src_frame, dest_frame);
    if (is_12bit) {
      internals::Shift12To10(dest_frame);
    }
    return OkStatus();
  }

  // Map the 8-bit destination format to a matching 16-bit high bit depth layout
  // (matching subsampling and alpha channel) for processing.
  switch (dest_frame.format()) {
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV12A:
    case PIXEL_FORMAT_P010LE:
    case PIXEL_FORMAT_YUV420P10:
      target_hbd_format = preserve_alpha ? PIXEL_FORMAT_YUV420AP10
                                         : (is_12bit ? PIXEL_FORMAT_YUV420P12
                                                     : PIXEL_FORMAT_YUV420P10);
      break;
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_I444A:
    case PIXEL_FORMAT_NV24:
    case PIXEL_FORMAT_P410LE:
    case PIXEL_FORMAT_YUV444P10:
      target_hbd_format = preserve_alpha ? PIXEL_FORMAT_YUV444AP10
                                         : (is_12bit ? PIXEL_FORMAT_YUV444P12
                                                     : PIXEL_FORMAT_YUV444P10);
      break;
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I422A:
    case PIXEL_FORMAT_NV16:
    case PIXEL_FORMAT_P210LE:
      target_hbd_format = preserve_alpha ? PIXEL_FORMAT_YUV422AP10
                                         : (is_12bit ? PIXEL_FORMAT_YUV422P12
                                                     : PIXEL_FORMAT_YUV422P10);
      break;
    default:
      return EncoderStatus(EncoderStatus::Codes::kUnsupportedFrameFormat)
          .WithData("src", src_frame->AsHumanReadableString())
          .WithData("dst", dest_frame.AsHumanReadableString());
  }

  // Perform spatial scaling and chroma subsampling conversion in 16-bit space.
  scoped_refptr<VideoFrame> scaled_hbd_frame;
  const bool source_layout_is_compatible =
      src_frame->format() == target_hbd_format ||
      (!preserve_alpha && !IsOpaque(src_frame->format()) &&
       BitDepth(src_frame->format()) == BitDepth(target_hbd_format) &&
       VideoPixelFormatToChromaSampling(src_frame->format()) ==
           VideoPixelFormatToChromaSampling(target_hbd_format));
  if (!source_layout_is_compatible ||
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

  // NV12x / NV16 / NV24 output requires down-converting to 8-bit first. We wrap
  // `dest_frame` in an I4xxx frame so that Y and A planes are down-converted
  // directly into `dest_frame`, avoiding extra allocations and copies.
  if (dest_frame.format() == PIXEL_FORMAT_NV12 ||
      dest_frame.format() == PIXEL_FORMAT_NV12A ||
      dest_frame.format() == PIXEL_FORMAT_NV16 ||
      dest_frame.format() == PIXEL_FORMAT_NV24) {
    auto tmp_frame = WrapBiplanarFrameInTriplanarFrame(dest_frame);
    if (!tmp_frame) {
      return EncoderStatus::Codes::kScalingError;
    }
    internals::Convert16To8Plane(*src_frame, *tmp_frame);
    internals::MergeUV(*tmp_frame, dest_frame);
    return OkStatus();
  }

  if (dest_frame.format() == PIXEL_FORMAT_P010LE ||
      dest_frame.format() == PIXEL_FORMAT_P210LE ||
      dest_frame.format() == PIXEL_FORMAT_P410LE) {
    return internals::I4xxxPxxToPx10(*src_frame, dest_frame)
               ? OkStatus()
               : EncoderStatus(EncoderStatus::Codes::kFormatConversionError);
  }

  // Down-convert each plane from 16-bit to matching 8-bit destination layout.
  internals::Convert16To8Plane(*src_frame, dest_frame);
  return OkStatus();
}

EncoderStatus VideoFrameConverter::ConvertAndScalePx10(
    const VideoFrame* src_frame,
    VideoFrame& dest_frame) {
  DCHECK(src_frame->format() == PIXEL_FORMAT_P010LE ||
         src_frame->format() == PIXEL_FORMAT_P210LE ||
         src_frame->format() == PIXEL_FORMAT_P410LE);

  if (src_frame->format() == dest_frame.format() &&
      src_frame->visible_rect().size() == dest_frame.visible_rect().size()) {
    internals::CopyVisiblePlanes(*src_frame, dest_frame);
    return OkStatus();
  }

  const bool dest_is_biplanar_8bit = dest_frame.format() == PIXEL_FORMAT_NV12 ||
                                     dest_frame.format() == PIXEL_FORMAT_NV16 ||
                                     dest_frame.format() == PIXEL_FORMAT_NV24;
  const bool dest_is_planar_10bit =
      dest_frame.format() == PIXEL_FORMAT_YUV420P10 ||
      dest_frame.format() == PIXEL_FORMAT_YUV444P10;
  if ((dest_is_biplanar_8bit || dest_is_planar_10bit) &&
      src_frame->visible_rect().size() == dest_frame.visible_rect().size() &&
      VideoPixelFormatToChromaSampling(src_frame->format()) ==
          VideoPixelFormatToChromaSampling(dest_frame.format())) {
    if (dest_is_biplanar_8bit) {
      internals::Px10ToNVxx(*src_frame, dest_frame);
      return OkStatus();
    }
    return internals::Px10ToIx10(*src_frame, dest_frame)
               ? OkStatus()
               : EncoderStatus(EncoderStatus::Codes::kFormatConversionError);
  }

  VideoPixelFormat planar_format = PIXEL_FORMAT_UNKNOWN;
  switch (src_frame->format()) {
    case PIXEL_FORMAT_P010LE:
      planar_format = PIXEL_FORMAT_YUV420P10;
      break;
    case PIXEL_FORMAT_P210LE:
      planar_format = PIXEL_FORMAT_YUV422P10;
      break;
    case PIXEL_FORMAT_P410LE:
      planar_format = PIXEL_FORMAT_YUV444P10;
      break;
    default:
      NOTREACHED();
  }

  // libyuv has no general Px10 scaler; de-interleave then reuse the HBD planar
  // path.
  auto planar_frame =
      CreateTempFrame(planar_format, src_frame->coded_size(),
                      src_frame->visible_rect(), src_frame->natural_size());
  if (!planar_frame) {
    return EncoderStatus::Codes::kScalingError;
  }
  if (!internals::Px10ToIx10(*src_frame, *planar_frame)) {
    return EncoderStatus(EncoderStatus::Codes::kFormatConversionError);
  }
  planar_frame->set_color_space(src_frame->ColorSpace());
  return ConvertAndScaleHBD(planar_frame.get(), dest_frame);
}

}  // namespace media
