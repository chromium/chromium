// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_vda_helpers.h"

#include "base/bind.h"
#include "media/base/color_plane_layout.h"
#include "media/base/video_codecs.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/v4l2/v4l2_image_processor_backend.h"
#include "media/video/h264_parser.h"

namespace media {
namespace v4l2_vda_helpers {

base::Optional<Fourcc> FindImageProcessorInputFormat(V4L2Device* vda_device) {
  std::vector<uint32_t> processor_input_formats =
      V4L2ImageProcessorBackend::GetSupportedInputFormats();

  struct v4l2_fmtdesc fmtdesc;
  memset(&fmtdesc, 0, sizeof(fmtdesc));
  fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  while (vda_device->Ioctl(VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
    if (std::find(processor_input_formats.begin(),
                  processor_input_formats.end(),
                  fmtdesc.pixelformat) != processor_input_formats.end()) {
      DVLOGF(3) << "Image processor input format=" << fmtdesc.description;
      return Fourcc::FromV4L2PixFmt(fmtdesc.pixelformat);
    }
    ++fmtdesc.index;
  }
  return base::nullopt;
}

base::Optional<Fourcc> FindImageProcessorOutputFormat(V4L2Device* ip_device) {
  // Prefer YVU420 and NV12 because ArcGpuVideoDecodeAccelerator only supports
  // single physical plane.
  static constexpr uint32_t kPreferredFormats[] = {V4L2_PIX_FMT_NV12,
                                                   V4L2_PIX_FMT_YVU420};
  auto preferred_formats_first = [](uint32_t a, uint32_t b) -> bool {
    auto* iter_a = std::find(std::begin(kPreferredFormats),
                             std::end(kPreferredFormats), a);
    auto* iter_b = std::find(std::begin(kPreferredFormats),
                             std::end(kPreferredFormats), b);
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

  return base::nullopt;
}

std::unique_ptr<ImageProcessor> CreateImageProcessor(
    const Fourcc vda_output_format,
    const Fourcc ip_output_format,
    const gfx::Size& vda_output_coded_size,
    const gfx::Size& ip_output_coded_size,
    const gfx::Size& visible_size,
    VideoFrame::StorageType output_storage_type,
    size_t nb_buffers,
    scoped_refptr<V4L2Device> image_processor_device,
    ImageProcessor::OutputMode image_processor_output_mode,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    ImageProcessor::ErrorCB error_cb) {
  // TODO(crbug.com/917798): Use ImageProcessorFactory::Create() once we remove
  //     |image_processor_device_| from V4L2VideoDecodeAccelerator.
  auto image_processor = ImageProcessor::Create(
      base::BindRepeating(&V4L2ImageProcessorBackend::Create,
                          image_processor_device, nb_buffers),
      ImageProcessor::PortConfig(vda_output_format, vda_output_coded_size, {},
                                 gfx::Rect(visible_size),
                                 {VideoFrame::STORAGE_DMABUFS}),
      ImageProcessor::PortConfig(ip_output_format, ip_output_coded_size, {},
                                 gfx::Rect(visible_size),
                                 {output_storage_type}),
      {image_processor_output_mode}, VIDEO_ROTATION_0, std::move(error_cb),
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
    case kCodecH264:
      return std::make_unique<
          v4l2_vda_helpers::H264InputBufferFragmentSplitter>();
    case kCodecVP8:
    case kCodecVP9:
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
      case H264NALU::kReserved14:
      case H264NALU::kReserved15:
      case H264NALU::kReserved16:
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
    *endpos = (nalu.data + nalu.size) - data;
  }
  NOTREACHED();
  return false;
}

void H264InputBufferFragmentSplitter::Reset() {
  partial_frame_pending_ = false;
  h264_parser_.reset(new H264Parser());
}

bool H264InputBufferFragmentSplitter::IsPartialFramePending() const {
  return partial_frame_pending_;
}

}  // namespace v4l2_vda_helpers
}  // namespace media
