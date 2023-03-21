// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/video_decoder.h"

#include <linux/videodev2.h>

#include "base/bits.h"
#include "base/logging.h"
#include "media/gpu/v4l2/test/upstream_pix_fmt.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/codec/png_codec.h"

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
                           std::unique_ptr<V4L2Queue> OUTPUT_queue,
                           std::unique_ptr<V4L2Queue> CAPTURE_queue)
    : v4l2_ioctl_(std::move(v4l2_ioctl)),
      OUTPUT_queue_(std::move(OUTPUT_queue)),
      CAPTURE_queue_(std::move(CAPTURE_queue)) {}

VideoDecoder::~VideoDecoder() = default;

void VideoDecoder::Initialize() {
  // TODO(stevecho): remove VIDIOC_ENUM_FRAMESIZES ioctl call
  //   after b/193237015 is resolved.
  if (!v4l2_ioctl_->EnumFrameSizes(OUTPUT_queue_->fourcc()))
    LOG(INFO) << "EnumFrameSizes for OUTPUT queue failed.";

  if (!v4l2_ioctl_->SetFmt(OUTPUT_queue_))
    LOG(FATAL) << "SetFmt for OUTPUT queue failed.";

  gfx::Size coded_size;
  uint32_t num_planes;
  if (!v4l2_ioctl_->GetFmt(CAPTURE_queue_->type(), &coded_size, &num_planes))
    LOG(FATAL) << "GetFmt for CAPTURE queue failed.";

  CAPTURE_queue_->set_coded_size(coded_size);
  CAPTURE_queue_->set_num_planes(num_planes);

  // VIDIOC_TRY_FMT() ioctl is equivalent to VIDIOC_S_FMT
  // with one exception that it does not change driver state.
  // VIDIOC_TRY_FMT may or may not be needed; it's used by the stateful
  // Chromium V4L2VideoDecoder backend, see b/190733055#comment78.
  // TODO(b/190733055): try and remove it after landing all the code.
  if (!v4l2_ioctl_->TryFmt(CAPTURE_queue_))
    LOG(FATAL) << "TryFmt for CAPTURE queue failed.";

  if (!v4l2_ioctl_->SetFmt(CAPTURE_queue_))
    LOG(FATAL) << "SetFmt for CAPTURE queue failed.";

  // If there is a dynamic resolution change, the Initialization sequence will
  // be performed again, minus the allocation of OUTPUT queue buffers.
  if (IsResolutionChanged()) {
    if (!v4l2_ioctl_->ReqBufsWithCount(CAPTURE_queue_,
                                       number_of_buffers_in_capture_queue_)) {
      LOG(FATAL) << "ReqBufs for CAPTURE queue failed.";
    }
  } else {
    if (!v4l2_ioctl_->ReqBufs(OUTPUT_queue_))
      LOG(FATAL) << "ReqBufs for OUTPUT queue failed.";

    if (!v4l2_ioctl_->QueryAndMmapQueueBuffers(OUTPUT_queue_))
      LOG(FATAL) << "QueryAndMmapQueueBuffers for OUTPUT queue failed";

    if (!v4l2_ioctl_->ReqBufs(CAPTURE_queue_))
      LOG(FATAL) << "ReqBufs for CAPTURE queue failed.";
  }

  if (!v4l2_ioctl_->QueryAndMmapQueueBuffers(CAPTURE_queue_))
    LOG(FATAL) << "QueryAndMmapQueueBuffers for CAPTURE queue failed.";

  // Only 1 CAPTURE buffer is needed for 1st key frame decoding. Remaining
  // CAPTURE buffers will be queued after that.
  if (!v4l2_ioctl_->QBuf(CAPTURE_queue_, 0))
    LOG(FATAL) << "VIDIOC_QBUF failed for CAPTURE queue.";

  int media_request_fd;
  if (!v4l2_ioctl_->MediaIocRequestAlloc(&media_request_fd))
    LOG(FATAL) << "MEDIA_IOC_REQUEST_ALLOC failed";

  OUTPUT_queue_->set_media_request_fd(media_request_fd);

  if (!v4l2_ioctl_->StreamOn(OUTPUT_queue_->type()))
    LOG(FATAL) << "StreamOn for OUTPUT queue failed.";

  if (!v4l2_ioctl_->StreamOn(CAPTURE_queue_->type()))
    LOG(FATAL) << "StreamOn for CAPTURE queue failed.";
}

// Follows the dynamic resolution change sequence described in
// https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/dev-stateless-decoder.html#dynamic-resolution-change
VideoDecoder::Result VideoDecoder::HandleDynamicResolutionChange(
    const gfx::Size& new_resolution) {
  // Call VIDIOC_STREAMOFF() on both the OUTPUT and CAPTURE queues.
  if (!v4l2_ioctl_->StreamOff(OUTPUT_queue_->type()))
    LOG(FATAL) << "StreamOff for OUTPUT queue failed.";

  if (!v4l2_ioctl_->StreamOff(CAPTURE_queue_->type()))
    LOG(FATAL) << "StreamOff for CAPTURE queue failed.";

  // Free all CAPTURE buffers from the driver side by calling VIDIOC_REQBUFS()
  // on the CAPTURE queue with a buffer count of zero.
  if (!v4l2_ioctl_->ReqBufsWithCount(CAPTURE_queue_, 0))
    LOG(FATAL) << "Failed to free all buffers for CAPTURE queue";

  // Free queued CAPTURE buffer indexes that are tracked by the client side.
  CAPTURE_queue_->DequeueAllBufferIds();

  // Set the new resolution on OUTPUT queue. The driver will then pick up
  // the new resolution to be set on the coded size for CAPTURE queue.
  OUTPUT_queue_->set_display_size(new_resolution);
  OUTPUT_queue_->set_coded_size(new_resolution);

  CAPTURE_queue_->set_display_size(new_resolution);

  // Perform the initialization sequence again
  Initialize();
  is_resolution_changed_ = false;

  return VideoDecoder::kOk;
}

void VideoDecoder::ConvertToYUV(std::vector<uint8_t>& dest_y,
                                std::vector<uint8_t>& dest_u,
                                std::vector<uint8_t>& dest_v,
                                const gfx::Size& dest_size,
                                const MmappedBuffer::MmappedPlanes& planes,
                                const gfx::Size& src_size,
                                uint32_t fourcc) {
  const gfx::Size half_dest_size((dest_size.width() + 1) / 2,
                                 (dest_size.height() + 1) / 2);
  const uint32_t dest_y_stride = dest_size.width();
  const uint32_t dest_uv_stride = half_dest_size.width();

  dest_y.resize(dest_size.GetArea());
  dest_u.resize(half_dest_size.GetArea());
  dest_v.resize(half_dest_size.GetArea());

  if (fourcc == V4L2_PIX_FMT_NV12) {
    CHECK_EQ(planes.size(), 1u)
        << "NV12 should have exactly 1 plane but CAPTURE queue does not.";

    const uint8_t* src = static_cast<uint8_t*>(planes[0].start_addr);
    const uint8_t* src_uv = src + src_size.width() * src_size.height();

    libyuv::NV12ToI420(src, src_size.width(), src_uv, src_size.width(),
                       &dest_y[0], dest_y_stride, &dest_u[0], dest_uv_stride,
                       &dest_v[0], dest_uv_stride, dest_size.width(),
                       dest_size.height());
  } else if (fourcc == V4L2_PIX_FMT_MM21) {
    CHECK_EQ(planes.size(), 2u)
        << "MM21 should have exactly 2 planes but CAPTURE queue does not.";
    const uint8_t* src_y = static_cast<uint8_t*>(planes[0].start_addr);
    const uint8_t* src_uv = static_cast<uint8_t*>(planes[1].start_addr);

    libyuv::MM21ToI420(src_y, src_size.width(), src_uv, src_size.width(),
                       &dest_y[0], dest_y_stride, &dest_u[0], dest_uv_stride,
                       &dest_v[0], dest_uv_stride, dest_size.width(),
                       dest_size.height());
  } else {
    LOG(FATAL) << "Unsupported CAPTURE queue format";
  }
}

std::vector<uint8_t> VideoDecoder::ConvertYUVToPNG(uint8_t* y_plane,
                                                   uint8_t* u_plane,
                                                   uint8_t* v_plane,
                                                   const gfx::Size& size) {
  const size_t argb_stride = size.width() * 4;
  auto argb_data = std::make_unique<uint8_t[]>(argb_stride * size.height());

  size_t u_plane_padded_width, v_plane_padded_width;
  u_plane_padded_width = v_plane_padded_width =
      base::bits::AlignUp(size.width(), 2) / 2;

  // Note that we use J420ToARGB instead of I420ToARGB so that the
  // kYuvJPEGConstants YUV-to-RGB conversion matrix is used.
  const int convert_to_argb_result = libyuv::J420ToARGB(
      y_plane, size.width(), u_plane, u_plane_padded_width, v_plane,
      v_plane_padded_width, argb_data.get(),
      base::checked_cast<int>(argb_stride), size.width(), size.height());

  LOG_ASSERT(convert_to_argb_result == 0) << "Failed to convert to ARGB";

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
