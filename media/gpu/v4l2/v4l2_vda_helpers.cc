// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_vda_helpers.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/color_plane_layout.h"
#include "media/base/video_codecs.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/v4l2/v4l2_image_processor_backend.h"
#include "media/parsers/h264_parser.h"
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
#include "media/parsers/h265_parser.h"
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

namespace media {
namespace v4l2_vda_helpers {

std::optional<Fourcc> FindImageProcessorInputFormat(V4L2Device* vda_device) {
  std::vector<uint32_t> processor_input_formats =
      V4L2ImageProcessorBackend::GetSupportedInputFormats();

  struct v4l2_fmtdesc fmtdesc;
  memset(&fmtdesc, 0, sizeof(fmtdesc));
  fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  while (vda_device->Ioctl(VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
    if (base::Contains(processor_input_formats, fmtdesc.pixelformat)) {
      DVLOGF(3) << "Image processor input format=" << fmtdesc.description;
      return Fourcc::FromV4L2PixFmt(fmtdesc.pixelformat);
    }
    ++fmtdesc.index;
  }
  return std::nullopt;
}

std::optional<Fourcc> FindImageProcessorOutputFormat(V4L2Device* ip_device) {
  // Prefer YVU420 and NV12 because ArcGpuVideoDecodeAccelerator only supports
  // single physical plane.
  static constexpr uint32_t kPreferredFormats[] = {V4L2_PIX_FMT_NV12,
                                                   V4L2_PIX_FMT_YVU420};
  auto preferred_formats_first = [](uint32_t a, uint32_t b) -> bool {
    auto* iter_a = base::ranges::find(kPreferredFormats, a);
    auto* iter_b = base::ranges::find(kPreferredFormats, b);
    return iter_a < iter_b;
  };

  std::vector<uint32_t> processor_output_formats =
      V4L2ImageProcessorBackend::GetSupportedOutputFormats();

  // Move the preferred formats to the front.
  std::sort(processor_output_formats.begin(), processor_output_formats.end(),
            preferred_formats_first);

  for (uint32_t processor_output_format : processor_output_formats) {
    auto fourcc = Fourcc::FromV4L2PixFmt(processor_output_format);
    if (fourcc && ip_device->CanCreateEGLImageFrom(*fourcc)) {
      DVLOGF(3) << "Image processor output format=" << processor_output_format;
      return fourcc;
    }
  }

  return std::nullopt;
}

std::unique_ptr<ImageProcessor> CreateImageProcessor(
    const Fourcc vda_output_format,
    const Fourcc ip_output_format,
    const gfx::Size& vda_output_coded_size,
    const gfx::Size& ip_output_coded_size,
    const gfx::Rect& visible_rect,
    VideoFrame::StorageType output_storage_type,
    size_t nb_buffers,
    scoped_refptr<V4L2Device> image_processor_device,
    ImageProcessor::OutputMode image_processor_output_mode,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    ImageProcessor::ErrorCB error_cb) {
  DCHECK_EQ(vda_output_coded_size, ip_output_coded_size);
  DCHECK(gfx::Rect(ip_output_coded_size).Contains(visible_rect));

  // TODO(crbug.com/917798): Use ImageProcessorFactory::Create() once we remove
  //     |image_processor_device_| from V4L2VideoDecodeAccelerator.
  auto image_processor = ImageProcessor::Create(
      base::BindRepeating(&V4L2ImageProcessorBackend::Create,
                          image_processor_device, nb_buffers),
      ImageProcessor::PortConfig(vda_output_format, vda_output_coded_size, {},
                                 visible_rect, VideoFrame::STORAGE_DMABUFS),
      ImageProcessor::PortConfig(ip_output_format, ip_output_coded_size, {},
                                 visible_rect, output_storage_type),
      image_processor_output_mode, std::move(error_cb),
      std::move(client_task_runner));
  if (!image_processor)
    return nullptr;

  if (image_processor->output_config().size != ip_output_coded_size) {
    VLOGF(1) << "Image processor should be able to use the requested output "
             << "coded size " << ip_output_coded_size.ToString()
             << " without adjusting to "
             << image_processor->output_config().size.ToString();
    return nullptr;
  }

  if (image_processor->input_config().size != vda_output_coded_size) {
    VLOGF(1) << "Image processor should be able to take the output coded "
             << "size of decoder " << vda_output_coded_size.ToString()
             << " without adjusting to "
             << image_processor->input_config().size.ToString();
    return nullptr;
  }

  return image_processor;
}

gfx::Size NativePixmapSizeFromHandle(const gfx::NativePixmapHandle& handle,
                                     const Fourcc fourcc,
                                     const gfx::Size& current_size) {
  const uint32_t stride = handle.planes[0].stride;
  const uint32_t horiz_bits_per_pixel =
      VideoFrame::PlaneHorizontalBitsPerPixel(fourcc.ToVideoPixelFormat(), 0);
  DCHECK_NE(horiz_bits_per_pixel, 0u);
  // Stride must fit exactly on a byte boundary (8 bits per byte)
  DCHECK_EQ((stride * 8) % horiz_bits_per_pixel, 0u);

  // Actual width of buffer is stride (in bits) divided by bits per pixel.
  int adjusted_coded_width = stride * 8 / horiz_bits_per_pixel;
  // If the buffer is multi-planar, then the height of the buffer does not
  // matter as long as it covers the visible area and we can just return
  // the current height.
  // For single-planar however, the actual height can be inferred by dividing
  // the start offset of the second plane by the stride of the first plane,
  // since the second plane is supposed to start right after the first one.
  int adjusted_coded_height =
      handle.planes.size() > 1 && handle.planes[1].offset != 0
          ? handle.planes[1].offset / adjusted_coded_width
          : current_size.height();

  DCHECK_GE(adjusted_coded_width, current_size.width());
  DCHECK_GE(adjusted_coded_height, current_size.height());

  return gfx::Size(adjusted_coded_width, adjusted_coded_height);
}

// static
std::unique_ptr<InputBufferFragmentSplitter>
InputBufferFragmentSplitter::CreateFromProfile(
    media::VideoCodecProfile profile) {
  switch (VideoCodecProfileToVideoCodec(profile)) {
    case VideoCodec::kH264:
      return std::make_unique<
          v4l2_vda_helpers::H264InputBufferFragmentSplitter>();
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case VideoCodec::kHEVC:
      return std::make_unique<
          v4l2_vda_helpers::HEVCInputBufferFragmentSplitter>();
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case VideoCodec::kVP8:
    case VideoCodec::kVP9:
      // VP8/VP9 don't need any frame splitting, use the default implementation.
      return std::make_unique<v4l2_vda_helpers::InputBufferFragmentSplitter>();
    default:
      LOG(ERROR) << "Unhandled profile: " << profile;
      return nullptr;
  }
}

bool InputBufferFragmentSplitter::AdvanceFrameFragment(const uint8_t* data,
                                                       size_t size,
                                                       size_t* endpos) {
  *endpos = size;
  return true;
}

void InputBufferFragmentSplitter::Reset() {}

bool InputBufferFragmentSplitter::IsPartialFramePending() const {
  return false;
}

H264InputBufferFragmentSplitter::H264InputBufferFragmentSplitter()
    : h264_parser_(new H264Parser()) {}

H264InputBufferFragmentSplitter::~H264InputBufferFragmentSplitter() = default;

bool H264InputBufferFragmentSplitter::AdvanceFrameFragment(const uint8_t* data,
                                                           size_t size,
                                                           size_t* endpos) {
  DCHECK(h264_parser_);

  // For H264, we need to feed HW one frame at a time.  This is going to take
  // some parsing of our input stream.
  h264_parser_->SetStream(data, size);
  H264NALU nalu;
  H264Parser::Result result;
  bool has_frame_data = false;
  *endpos = 0;

  // Keep on peeking the next NALs while they don't indicate a frame
  // boundary.
  while (true) {
    bool end_of_frame = false;
    result = h264_parser_->AdvanceToNextNALU(&nalu);
    if (result == H264Parser::kInvalidStream ||
        result == H264Parser::kUnsupportedStream) {
      return false;
    }
    if (result == H264Parser::kEOStream) {
      // We've reached the end of the buffer before finding a frame boundary.
      if (has_frame_data)
        partial_frame_pending_ = true;
      *endpos = size;
      return true;
    }
    switch (nalu.nal_unit_type) {
      case H264NALU::kNonIDRSlice:
      case H264NALU::kIDRSlice:
        if (nalu.size < 1)
          return false;

        has_frame_data = true;

        // For these two, if the "first_mb_in_slice" field is zero, start a
        // new frame and return.  This field is Exp-Golomb coded starting on
        // the eighth data bit of the NAL; a zero value is encoded with a
        // leading '1' bit in the byte, which we can detect as the byte being
        // (unsigned) greater than or equal to 0x80.
        if (nalu.data[1] >= 0x80) {
          end_of_frame = true;
          break;
        }
        break;
      case H264NALU::kSEIMessage:
      case H264NALU::kSPS:
      case H264NALU::kPPS:
      case H264NALU::kAUD:
      case H264NALU::kEOSeq:
      case H264NALU::kEOStream:
      case H264NALU::kFiller:
      case H264NALU::kSPSExt:
      case H264NALU::kPrefix:
      case H264NALU::kSubsetSPS:
      case H264NALU::kDPS:
      case H264NALU::kReserved17:
      case H264NALU::kReserved18:
        // These unconditionally signal a frame boundary.
        end_of_frame = true;
        break;
      default:
        // For all others, keep going.
        break;
    }
    if (end_of_frame) {
      if (!partial_frame_pending_ && *endpos == 0) {
        // The frame was previously restarted, and we haven't filled the
        // current frame with any contents yet.  Start the new frame here and
        // continue parsing NALs.
      } else {
        // The frame wasn't previously restarted and/or we have contents for
        // the current frame; signal the start of a new frame here: we don't
        // have a partial frame anymore.
        partial_frame_pending_ = false;
        return true;
      }
    }
    *endpos = (nalu.data + base::checked_cast<size_t>(nalu.size)) - data;
  }
  NOTREACHED();
}

void H264InputBufferFragmentSplitter::Reset() {
  partial_frame_pending_ = false;
  h264_parser_.reset(new H264Parser());
}

bool H264InputBufferFragmentSplitter::IsPartialFramePending() const {
  return partial_frame_pending_;
}

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
HEVCInputBufferFragmentSplitter::HEVCInputBufferFragmentSplitter()
    : h265_parser_(new H265Parser()) {}

HEVCInputBufferFragmentSplitter::~HEVCInputBufferFragmentSplitter() = default;

bool HEVCInputBufferFragmentSplitter::AdvanceFrameFragment(const uint8_t* data,
                                                           size_t size,
                                                           size_t* endpos) {
  DCHECK(h265_parser_);

  // For HEVC, we need to feed HW one frame at a time. This parses the bitstream
  // to determine frame boundaries.
  h265_parser_->SetStream(data, size);
  H265NALU nalu;
  H265Parser::Result result;
  *endpos = 0;

  // Keep on peeking the next NALs while they don't indicate a frame
  // boundary.
  while (true) {
    bool end_of_frame = false;
    result = h265_parser_->AdvanceToNextNALU(&nalu);
    if (result == H265Parser::kInvalidStream ||
        result == H265Parser::kUnsupportedStream) {
      return false;
    }
    if (result == H265Parser::kEOStream) {
      // We've reached the end of the buffer before finding a frame boundary.
      if (*endpos != 0)
        partial_frame_pending_ = true;
      *endpos = size;
      return true;
    }
    switch (nalu.nal_unit_type) {
      case H265NALU::BLA_W_LP:
      case H265NALU::BLA_W_RADL:
      case H265NALU::BLA_N_LP:
      case H265NALU::IDR_W_RADL:
      case H265NALU::IDR_N_LP:
      case H265NALU::TRAIL_N:
      case H265NALU::TRAIL_R:
      case H265NALU::TSA_N:
      case H265NALU::TSA_R:
      case H265NALU::STSA_N:
      case H265NALU::STSA_R:
      case H265NALU::RADL_N:
      case H265NALU::RADL_R:
      case H265NALU::RASL_N:
      case H265NALU::RASL_R:
      case H265NALU::CRA_NUT: {
        // Rec. ITU-T H.265, section 7.3.1.2 NAL unit header syntax
        constexpr uint8_t kHevcNaluHeaderSize = 2;

        // These NALU's have a slice header which starts after the NAL unit
        // header. This ensures that there is enough data in the NALU to contain
        // contain at least one byte of the slice header.
        if (nalu.size <= kHevcNaluHeaderSize)
          return false;

        // From Rec. ITU-T H.265, section 7.3.6 Slice segment header syntax, the
        // first bit in the slice header is first_slice_segment_in_pic_flag. If
        // it is set, then the slice starts a new frame.
        if ((nalu.data[kHevcNaluHeaderSize] & 0x80) == 0x80) {
          if (slice_data_pending_) {
            end_of_frame = true;
            break;
          }
        }
        slice_data_pending_ = true;
        break;
      }
      case H265NALU::SPS_NUT:
      case H265NALU::PPS_NUT:
        // Only creates a new frame if there is already slice data. This groups
        // the SPS and PPS with the first frame. This is needed for the Venus
        // driver for HEVC. Curiously, the same treatment is not needed for
        // H.264.
        // TODO(b/227247905): If a different policy is needed for the Stateless
        // backend, then make the behavior externally configurable.
        if (slice_data_pending_) {
          end_of_frame = true;
        }
        break;
      case H265NALU::EOS_NUT:
      case H265NALU::EOB_NUT:
      case H265NALU::AUD_NUT:
      case H265NALU::RSV_NVCL41:
      case H265NALU::RSV_NVCL42:
      case H265NALU::RSV_NVCL43:
      case H265NALU::RSV_NVCL44:
      case H265NALU::UNSPEC48:
      case H265NALU::UNSPEC49:
      case H265NALU::UNSPEC50:
      case H265NALU::UNSPEC51:
      case H265NALU::UNSPEC52:
      case H265NALU::UNSPEC53:
      case H265NALU::UNSPEC54:
      case H265NALU::UNSPEC55:
        // These unconditionally signal a frame boundary.
        end_of_frame = true;
        break;
      default:
        // For all others, keep going.
        break;
    }
    if (end_of_frame) {
      if (!partial_frame_pending_ && *endpos == 0) {
        // The frame was previously restarted, and we haven't filled the
        // current frame with any contents yet. Start the new frame here and
        // continue parsing NALs.
      } else {
        // The frame wasn't previously restarted and/or we have contents for
        // the current frame; signal the start of a new frame here: we don't
        // have a partial frame anymore.
        partial_frame_pending_ = false;
        slice_data_pending_ = false;
        return true;
      }
    }
    *endpos = (nalu.data + base::checked_cast<size_t>(nalu.size)) - data;
  }
  NOTREACHED();
}

void HEVCInputBufferFragmentSplitter::Reset() {
  partial_frame_pending_ = false;
  slice_data_pending_ = false;
  h265_parser_.reset(new H265Parser());
}

bool HEVCInputBufferFragmentSplitter::IsPartialFramePending() const {
  return partial_frame_pending_;
}
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

}  // namespace v4l2_vda_helpers
}  // namespace media
