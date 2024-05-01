// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/video_decoder.h"

#include <linux/videodev2.h>
#include <algorithm>
#include <vector>

#include "base/bits.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "media/base/video_types.h"
#include "media/gpu/v4l2/test/upstream_pix_fmt.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace v4l2_test {

uint32_t FileFourccToDriverFourcc(uint32_t header_fourcc) {
  if (header_fourcc == V4L2_PIX_FMT_VP9) {
    LOG(INFO) << "OUTPUT format mapped from VP90 to VP9F.";
    return V4L2_PIX_FMT_VP9_FRAME;
  } else if (header_fourcc == V4L2_PIX_FMT_AV1) {
    LOG(INFO) << "OUTPUT format mapped from AV01 to AV1F.";
    return V4L2_PIX_FMT_AV1_FRAME;
  } else if (header_fourcc == V4L2_PIX_FMT_VP8) {
    LOG(INFO) << "OUTPUT format mapped from VP80 to VP8F.";
    return V4L2_PIX_FMT_VP8_FRAME;
  }

  return header_fourcc;
}

VideoDecoder::VideoDecoder(std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
                           gfx::Size display_resolution)
    : v4l2_ioctl_(std::move(v4l2_ioctl)),
      display_resolution_(display_resolution) {}

VideoDecoder::~VideoDecoder() = default;

void VideoDecoder::NegotiateCAPTUREFormat() {
  constexpr uint32_t kPreferredFormats[] = {
      V4L2_PIX_FMT_NV12, V4L2_PIX_FMT_MM21, V4L2_PIX_FMT_MT2T};

  struct v4l2_format fmt;

  memset(&fmt, 0, sizeof(fmt));
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

  v4l2_ioctl_->GetFmt(&fmt);
  uint32_t fourcc = fmt.fmt.pix_mp.pixelformat;

  // Check to see if if the format returned is one that can be used. The driver
  // may prefer a different format than what is needed. If
  // not, negotiations need to be done to see if the preferred format can
  // be used.
  if (!base::Contains(kPreferredFormats, fourcc)) {
    bool format_found = false;
    for (const auto& preferred_fourcc : kPreferredFormats) {
      VLOG(1) << "Trying to see if preferred format ("
              << media::FourccToString(preferred_fourcc)
              << ") is supported by the driver.";
      fmt.fmt.pix_mp.pixelformat = preferred_fourcc;

      v4l2_ioctl_->TryFmt(&fmt);
      VLOG(1) << "Driver returned format ("
              << media::FourccToString(fmt.fmt.pix_mp.pixelformat) << ").";

      if (fmt.fmt.pix_mp.pixelformat == preferred_fourcc) {
        VLOG(1) << "Preferred format ("
                << media::FourccToString(preferred_fourcc)
                << ") being used for CAPTURE queue.";
        fourcc = preferred_fourcc;
        format_found = true;
        break;
      }
    }
    if (!format_found) {
      LOG(FATAL) << "Unable to choose preferred format, TryFmt is returning ("
                 << media::FourccToString(fmt.fmt.pix_mp.pixelformat) << ").";
    }
  }

  CAPTURE_queue_->set_fourcc(fourcc);
  CAPTURE_queue_->set_resolution(
      gfx::Size(fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height));
  CAPTURE_queue_->set_num_planes(fmt.fmt.pix_mp.num_planes);

  v4l2_ioctl_->SetFmt(CAPTURE_queue_);

  LOG_ASSERT((V4L2_PIX_FMT_MM21 == fourcc &&
              CAPTURE_queue_->num_planes() == fmt.fmt.pix_mp.num_planes) ||
             (V4L2_PIX_FMT_NV12 == fourcc &&
              CAPTURE_queue_->num_planes() == fmt.fmt.pix_mp.num_planes) ||
             (V4L2_PIX_FMT_MT2T == fourcc &&
              CAPTURE_queue_->num_planes() == fmt.fmt.pix_mp.num_planes))
      << media::FourccToString(fourcc)
      << " does not have the correct number of planes: "
      << static_cast<uint32_t>(fmt.fmt.pix_mp.num_planes);
}

void VideoDecoder::CreateOUTPUTQueue(uint32_t compressed_fourcc) {
  // TODO(stevecho): might need to consider using more than 1 file descriptor
  // (fd) & buffer with the output queue for 4K60 requirement.
  // https://buganizer.corp.google.com/issues/202214561#comment31
  OUTPUT_queue_ = std::make_unique<V4L2Queue>(
      V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, display_resolution_, V4L2_MEMORY_MMAP);
  OUTPUT_queue_->set_fourcc(compressed_fourcc);

  v4l2_ioctl_->SetFmt(OUTPUT_queue_);
  v4l2_ioctl_->ReqBufs(OUTPUT_queue_, kNumberOfBuffersInOutputQueue);
  v4l2_ioctl_->QueryAndMmapQueueBuffers(OUTPUT_queue_);

  int media_request_fd;
  v4l2_ioctl_->MediaIocRequestAlloc(&media_request_fd);

  OUTPUT_queue_->set_media_request_fd(media_request_fd);

  v4l2_ioctl_->StreamOn(OUTPUT_queue_->type());
}

void VideoDecoder::CreateCAPTUREQueue(uint32_t num_buffers) {
  // TODO(stevecho): enable V4L2_MEMORY_DMABUF memory for CAPTURE queue.
  // |num_planes| represents separate memory buffers, not planes for Y, U, V.
  // https://www.kernel.org/doc/html/v5.10/userspace-api/media/v4l/pixfmt-v4l2-mplane.html#c.V4L.v4l2_plane_pix_format
  CAPTURE_queue_ = std::make_unique<V4L2Queue>(
      V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, gfx::Size(0, 0), V4L2_MEMORY_MMAP);

  NegotiateCAPTUREFormat();

  LOG_ASSERT(gfx::Rect(CAPTURE_queue_->resolution())
                 .Contains(gfx::Rect(OUTPUT_queue_->resolution())))
      << "Display size is not contained within the coded size. DRC?";

  v4l2_ioctl_->ReqBufs(CAPTURE_queue_, num_buffers);
  v4l2_ioctl_->QueryAndMmapQueueBuffers(CAPTURE_queue_);
  // Only 1 CAPTURE buffer is needed for 1st key frame decoding. Remaining
  // CAPTURE buffers will be queued after that.
  if (!v4l2_ioctl_->QBuf(CAPTURE_queue_, 0)) {
    LOG(FATAL) << "VIDIOC_QBUF failed for CAPTURE queue.";
  }

  v4l2_ioctl_->StreamOn(CAPTURE_queue_->type());
}

// Follows the dynamic resolution change sequence described in
// https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/dev-stateless-decoder.html#dynamic-resolution-change
void VideoDecoder::HandleDynamicResolutionChange(
    const gfx::Size& new_resolution) {
  // Call VIDIOC_STREAMOFF() on both the OUTPUT and CAPTURE queues.
  v4l2_ioctl_->StreamOff(OUTPUT_queue_->type());
  v4l2_ioctl_->StreamOff(CAPTURE_queue_->type());

  // Store the buffer count before clearing so the amount to reallocate
  // is known.
  const uint32_t num_buffers = CAPTURE_queue_->num_buffers();

  // Free all CAPTURE buffers from the driver side by calling VIDIOC_REQBUFS()
  // on the CAPTURE queue with a buffer count of zero.
  v4l2_ioctl_->ReqBufs(CAPTURE_queue_, 0);

  // Free queued CAPTURE buffer indexes that are tracked by the client side.
  CAPTURE_queue_->DequeueAllBufferIds();

  // Set the new resolution on OUTPUT queue. The driver will then pick up
  // the new resolution to be set on the coded size for CAPTURE queue.
  OUTPUT_queue_->set_resolution(new_resolution);
  v4l2_ioctl_->SetFmt(OUTPUT_queue_);

  NegotiateCAPTUREFormat();

  v4l2_ioctl_->ReqBufs(CAPTURE_queue_, num_buffers);
  v4l2_ioctl_->QueryAndMmapQueueBuffers(CAPTURE_queue_);

  // Only 1 CAPTURE buffer is needed for 1st key frame decoding. Remaining
  // CAPTURE buffers will be queued after that.
  if (!v4l2_ioctl_->QBuf(CAPTURE_queue_, 0)) {
    LOG(FATAL) << "VIDIOC_QBUF failed for CAPTURE queue.";
  }

  v4l2_ioctl_->StreamOn(OUTPUT_queue_->type());
  v4l2_ioctl_->StreamOn(CAPTURE_queue_->type());
}

// static
VideoDecoder::BitDepth VideoDecoder::ConvertToYUV(
    std::vector<uint8_t>& dest_y,
    std::vector<uint8_t>& dest_u,
    std::vector<uint8_t>& dest_v,
    const gfx::Size& dest_size,
    const MmappedBuffer::MmappedPlanes& planes,
    const gfx::Size& src_size,
    uint32_t fourcc) {
  const gfx::Size half_dest_size((dest_size.width() + 1) / 2,
                                 (dest_size.height() + 1) / 2);
  const uint32_t dest_full_stride = dest_size.width();
  const uint32_t dest_half_stride = half_dest_size.width();

  dest_y.resize(dest_size.GetArea());
  dest_u.resize(half_dest_size.GetArea());
  dest_v.resize(half_dest_size.GetArea());

  if (fourcc == V4L2_PIX_FMT_NV12) {
    CHECK_EQ(planes.size(), 1u)
        << "NV12 should have exactly 1 plane but CAPTURE queue does not.";

    const uint8_t* src = static_cast<uint8_t*>(planes[0].start_addr);
    const uint8_t* src_uv = src + src_size.width() * src_size.height();

    libyuv::NV12ToI420(src, src_size.width(), src_uv, src_size.width(),
                       &dest_y[0], dest_full_stride, &dest_u[0],
                       dest_half_stride, &dest_v[0], dest_half_stride,
                       dest_size.width(), dest_size.height());
    return BitDepth::Depth8;
  } else if (fourcc == V4L2_PIX_FMT_MM21) {
    CHECK_EQ(planes.size(), 2u)
        << "MM21 should have exactly 2 planes but CAPTURE queue does not.";
    const uint8_t* src_y = static_cast<uint8_t*>(planes[0].start_addr);
    const uint8_t* src_uv = static_cast<uint8_t*>(planes[1].start_addr);

    libyuv::MM21ToI420(src_y, src_size.width(), src_uv, src_size.width(),
                       &dest_y[0], dest_full_stride, &dest_u[0],
                       dest_half_stride, &dest_v[0], dest_half_stride,
                       dest_size.width(), dest_size.height());
    return BitDepth::Depth8;
  } else if (fourcc == V4L2_PIX_FMT_MT2T) {
    CHECK_EQ(planes.size(), 2u)
        << "MT2T should have exactly 2 planes but CAPTURE queue does not.";

    const uint8_t* src_y = static_cast<uint8_t*>(planes[0].start_addr);
    const uint8_t* src_uv = static_cast<uint8_t*>(planes[1].start_addr);

    dest_y.resize(dest_size.GetArea() * 2);
    dest_u.resize(half_dest_size.GetArea() * 2);
    dest_v.resize(half_dest_size.GetArea() * 2);

    std::vector<uint16_t> tmp_y(dest_size.GetArea());
    std::vector<uint16_t> tmp_uv(dest_size.GetArea());

    // stride is 5/4 because MT2T is a packed 10bit format
    const uint32_t src_stride_mt2t = (src_size.width() * 5) >> 2;

    libyuv::MT2TToP010(src_y, src_stride_mt2t, src_uv, src_stride_mt2t,
                       &tmp_y[0], dest_full_stride, &tmp_uv[0],
                       dest_full_stride, dest_size.width(), dest_size.height());

    libyuv::P010ToI010(
        &tmp_y[0], dest_full_stride, &tmp_uv[0], dest_full_stride,
        reinterpret_cast<uint16_t*>(&dest_y[0]), dest_full_stride,
        reinterpret_cast<uint16_t*>(&dest_u[0]), dest_half_stride,
        reinterpret_cast<uint16_t*>(&dest_v[0]), dest_half_stride,
        dest_size.width(), dest_size.height());

    return BitDepth::Depth16;
  } else {
    LOG(FATAL) << "Unsupported CAPTURE queue format";
  }
}

std::vector<uint8_t> VideoDecoder::ConvertYUVToPNG(uint8_t* y_plane,
                                                   uint8_t* u_plane,
                                                   uint8_t* v_plane,
                                                   const gfx::Size& size,
                                                   BitDepth bit_depth) {
  const size_t argb_stride = size.width() * 4;
  auto argb_data = std::make_unique<uint8_t[]>(argb_stride * size.height());

  size_t u_plane_padded_width, v_plane_padded_width;
  u_plane_padded_width = v_plane_padded_width =
      base::bits::AlignUpDeprecatedDoNotUse(size.width(), 2) / 2;

  if (bit_depth == BitDepth::Depth8) {
    // Note that we use J420ToARGB instead of I420ToARGB so that the
    // kYuvJPEGConstants YUV-to-RGB conversion matrix is used.
    const int convert_to_argb_result = libyuv::J420ToARGB(
        y_plane, size.width(), u_plane, u_plane_padded_width, v_plane,
        v_plane_padded_width, argb_data.get(),
        base::checked_cast<int>(argb_stride), size.width(), size.height());

    LOG_ASSERT(convert_to_argb_result == 0) << "Failed to convert to ARGB";
  } else if (bit_depth == BitDepth::Depth16) {
    const int convert_to_argb_result = libyuv::I010ToARGB(
        reinterpret_cast<const uint16_t*>(y_plane), size.width(),
        reinterpret_cast<const uint16_t*>(u_plane), u_plane_padded_width,
        reinterpret_cast<const uint16_t*>(v_plane), v_plane_padded_width,
        argb_data.get(), base::checked_cast<int>(argb_stride), size.width(),
        size.height());

    LOG_ASSERT(convert_to_argb_result == 0) << "Failed to convert to ARGB";
  } else {
    LOG(FATAL) << bit_depth << " is not a valid number of bits / pixel";
  }

  std::vector<uint8_t> image_buffer;
  const bool encode_to_png_result = gfx::PNGCodec::Encode(
      argb_data.get(), gfx::PNGCodec::FORMAT_BGRA, size, argb_stride,
      true /*discard_transparency*/, std::vector<gfx::PNGCodec::Comment>(),
      &image_buffer);
  LOG_ASSERT(encode_to_png_result) << "Failed to encode to PNG";

  return image_buffer;
}
}  // namespace v4l2_test
}  // namespace media
