// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/capture/video/linux/v4l2_capture_delegate_gpu_helper.h"

#include "base/trace_event/trace_event.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "media/capture/video/video_capture_gpu_channel_host.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

namespace {
constexpr media::VideoPixelFormat kTargetPixelFormat =
    media::VideoPixelFormat::PIXEL_FORMAT_NV12;
constexpr viz::SharedImageFormat kTargetSharedImageFormat =
    viz::MultiPlaneFormat::kNV12;

libyuv::FourCC VideoCaptureFormatToLibyuvFourcc(
    const VideoCaptureFormat& capture_format) {
  const int chopped_width = capture_format.frame_size.width() & 1;
  const int chopped_height = capture_format.frame_size.height() & 1;
  libyuv::FourCC fourcc_format = libyuv::FOURCC_ANY;

  switch (capture_format.pixel_format) {
    case PIXEL_FORMAT_UNKNOWN:  // Color format not set.
      break;
    case PIXEL_FORMAT_I420:
      DCHECK(!chopped_width && !chopped_height);
      fourcc_format = libyuv::FOURCC_I420;
      break;
    case PIXEL_FORMAT_YV12:
      DCHECK(!chopped_width && !chopped_height);
      fourcc_format = libyuv::FOURCC_YV12;
      break;
    case PIXEL_FORMAT_NV12:
      DCHECK(!chopped_width && !chopped_height);
      fourcc_format = libyuv::FOURCC_NV12;
      break;
    case PIXEL_FORMAT_NV21:
      DCHECK(!chopped_width && !chopped_height);
      fourcc_format = libyuv::FOURCC_NV21;
      break;
    case PIXEL_FORMAT_YUY2:
      DCHECK(!chopped_width);
      fourcc_format = libyuv::FOURCC_YUY2;
      break;
    case PIXEL_FORMAT_UYVY:
      DCHECK(!chopped_width);
      fourcc_format = libyuv::FOURCC_UYVY;
      break;
    case PIXEL_FORMAT_RGB24:
      fourcc_format = libyuv::FOURCC_RAW;
      break;
    case PIXEL_FORMAT_ARGB:
      // Windows platforms e.g. send the data vertically flipped sometimes.
      fourcc_format = libyuv::FOURCC_ARGB;
      break;
    case PIXEL_FORMAT_MJPEG:
      fourcc_format = libyuv::FOURCC_MJPG;
      break;
    default:
      NOTREACHED();
  }
  return fourcc_format;
}

libyuv::RotationMode TranslateRotation(int rotation_degrees) {
  DCHECK_EQ(0, rotation_degrees % 90) << " Rotation must be a multiple of 90, "
                                         "now: "
                                      << rotation_degrees;
  libyuv::RotationMode rotation_mode = libyuv::kRotate0;
  if (rotation_degrees == 90) {
    rotation_mode = libyuv::kRotate90;
  } else if (rotation_degrees == 180) {
    rotation_mode = libyuv::kRotate180;
  } else if (rotation_degrees == 270) {
    rotation_mode = libyuv::kRotate270;
  }
  return rotation_mode;
}

}  // namespace

V4L2CaptureDelegateGpuHelper::V4L2CaptureDelegateGpuHelper() = default;

V4L2CaptureDelegateGpuHelper::~V4L2CaptureDelegateGpuHelper() = default;

int V4L2CaptureDelegateGpuHelper::OnIncomingCapturedData(
    VideoCaptureDevice::Client* client,
    const uint8_t* sample,
    size_t sample_size,
    const VideoCaptureFormat& capture_format,
    const gfx::ColorSpace& data_color_space,
    int rotation,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    int frame_feedback_id) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "V4L2CaptureDelegateGpuHelper::OnIncomingCapturedData");
  if (!client) {
    return -1;
  }

  // Align destination resolution to be even.
  int dst_width = capture_format.frame_size.width() & ~1;
  int dst_height = capture_format.frame_size.height() & ~1;
  if (rotation == 90 || rotation == 270) {
    std::swap(dst_width, dst_height);
  }
  const gfx::Size dimensions(dst_width, dst_height);
  VideoCaptureDevice::Client::Buffer capture_buffer;
  auto reservation_result_code = client->ReserveOutputBuffer(
      dimensions, kTargetPixelFormat, frame_feedback_id, &capture_buffer,
      /*require_new_buffer_id=*/nullptr, /*retire_old_buffer_id=*/nullptr);
  if (reservation_result_code !=
      VideoCaptureDevice::Client::ReserveResult::kSucceeded) {
    DLOG(ERROR) << "Failed to reserve output capture buffer: "
                << (int)reservation_result_code;
    client->OnFrameDropped(
        ConvertReservationFailureToFrameDropReason(reservation_result_code));
    return -1;
  }

  auto sii =
      VideoCaptureGpuChannelHost::GetInstance().GetSharedImageInterface();
  if (!sii) {
    LOG(ERROR) << "Failed to get SharedImageInterface.";
    client->OnFrameDropped(ConvertReservationFailureToFrameDropReason(
        VideoCaptureDevice::Client::ReserveResult::kAllocationFailed));
    return -1;
  }

  // Setting some default usage in order to get a mappable shared image.
  constexpr auto si_usage =
      gpu::SHARED_IMAGE_USAGE_CPU_WRITE | gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
  auto shared_image = sii->CreateSharedImage(
      {kTargetSharedImageFormat, dimensions, gfx::ColorSpace(),
       gpu::SharedImageUsageSet(si_usage), "V4L2CaptureDelegateGpuHelper"},
      gpu::kNullSurfaceHandle, gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
      capture_buffer.handle_provider->GetGpuMemoryBufferHandle());
  if (!shared_image) {
    LOG(ERROR) << "Failed to create a mappable shared image.";
    client->OnFrameDropped(ConvertReservationFailureToFrameDropReason(
        VideoCaptureDevice::Client::ReserveResult::kAllocationFailed));
    return -1;
  }

  auto scoped_mapping = shared_image->Map();
  if (!scoped_mapping) {
    LOG(ERROR) << "Failed to map the shared image.";
    client->OnFrameDropped(ConvertReservationFailureToFrameDropReason(
        VideoCaptureDevice::Client::ReserveResult::kAllocationFailed));
    return -1;
  }

  int status = ConvertCaptureDataToNV12(
      sample, sample_size, capture_format, dimensions, data_color_space,
      rotation, scoped_mapping->GetMemoryForPlane(VideoFrame::Plane::kY).data(),
      scoped_mapping->GetMemoryForPlane(VideoFrame::Plane::kUV).data(),
      scoped_mapping->Stride(VideoFrame::Plane::kY),
      scoped_mapping->Stride(VideoFrame::Plane::kUV));

  if (status != 0) {
    DLOG(ERROR) << "Failed to convert capture data.";
    client->OnFrameDropped(ConvertReservationFailureToFrameDropReason(
        VideoCaptureDevice::Client::ReserveResult::kSucceeded));
    return status;
  }

  client->OnIncomingCapturedBufferExt(
      std::move(capture_buffer),
      VideoCaptureFormat(dimensions, capture_format.frame_rate,
                         kTargetPixelFormat),
      gfx::ColorSpace(), reference_time, timestamp, std::nullopt,
      gfx::Rect(dimensions), VideoFrameMetadata());
  return status;
}

int V4L2CaptureDelegateGpuHelper::ConvertCaptureDataToNV12(
    const uint8_t* sample,
    size_t sample_size,
    const VideoCaptureFormat& capture_format,
    const gfx::Size& dimensions,
    const gfx::ColorSpace& data_color_space,
    int rotation,
    uint8_t* dst_y,
    uint8_t* dst_uv,
    int dst_stride_y,
    int dst_stride_uv) {
  const libyuv::FourCC fourcc =
      VideoCaptureFormatToLibyuvFourcc(capture_format);
  const libyuv::RotationMode rotation_mode = TranslateRotation(rotation);
  if (rotation_mode == libyuv::RotationMode::kRotate0 &&
      IsNV12ConvertSupported(fourcc)) {
    return FastConvertToNV12(sample, sample_size, capture_format, dst_y, dst_uv,
                             dst_stride_y, dst_stride_uv);
  }

  const size_t i420_size = VideoFrame::AllocationSize(
      VideoPixelFormat::PIXEL_FORMAT_I420, dimensions);
  i420_buffer_.reserve(i420_size);
  if (!i420_buffer_.data()) {
    return -1;
  }

  uint8_t* i420_y = i420_buffer_.data();
  uint8_t* i420_u =
      i420_y + VideoFrame::PlaneSize(VideoPixelFormat::PIXEL_FORMAT_I420,
                                     VideoFrame::Plane::kY, dimensions)
                   .GetArea();
  uint8_t* i420_v =
      i420_u + VideoFrame::PlaneSize(VideoPixelFormat::PIXEL_FORMAT_I420,
                                     VideoFrame::Plane::kU, dimensions)
                   .GetArea();
  std::vector<int32_t> i420_strides = VideoFrame::ComputeStrides(
      VideoPixelFormat::PIXEL_FORMAT_I420, dimensions);
  const int i420_stride_y = i420_strides[VideoFrame::Plane::kY];
  const int i420_stride_u = i420_strides[VideoFrame::Plane::kU];
  const int i420_stride_v = i420_strides[VideoFrame::Plane::kV];

  const int width = capture_format.frame_size.width();
  const int height = capture_format.frame_size.height();
  const int crop_width = width & ~1;
  const int crop_height = height & ~1;
  int status = libyuv::ConvertToI420(
      sample, sample_size, i420_y, i420_stride_y, i420_u, i420_stride_u, i420_v,
      i420_stride_v, 0, 0, width, height, crop_width, crop_height,
      rotation_mode, fourcc);
  if (status != 0) {
    return status;
  }

  status = libyuv::I420ToNV12(i420_y, i420_stride_y, i420_u, i420_stride_u,
                              i420_v, i420_stride_v, dst_y, dst_stride_y,
                              dst_uv, dst_stride_uv, width, height);

  return status;
}

int V4L2CaptureDelegateGpuHelper::FastConvertToNV12(
    const uint8_t* sample,
    size_t sample_size,
    const VideoCaptureFormat& capture_format,
    uint8_t* dst_y,
    uint8_t* dst_uv,
    int dst_stride_y,
    int dst_stride_uv) {
  const int src_width = capture_format.frame_size.width();
  const int src_height = capture_format.frame_size.height();
  const uint8_t* src_uv = sample + (src_width * src_height);

  const libyuv::FourCC fourcc =
      VideoCaptureFormatToLibyuvFourcc(capture_format);
  switch (libyuv::CanonicalFourCC(fourcc)) {
    case libyuv::FOURCC_YUY2:
      return libyuv::YUY2ToNV12(sample, src_width * 2, dst_y, dst_stride_y,
                                dst_uv, dst_stride_uv, src_width, src_height);
    case libyuv::FOURCC_MJPG:
      return libyuv::MJPGToNV12(sample, sample_size, dst_y, dst_stride_y,
                                dst_uv, dst_stride_uv, src_width, src_height,
                                src_width, src_height);
    case libyuv::FOURCC_NV12:
      return libyuv::NV12Copy(sample, src_width, src_uv, src_width, dst_y,
                              dst_stride_y, dst_uv, dst_stride_uv, src_width,
                              src_height);
    default:
      return -1;
  }
}

bool V4L2CaptureDelegateGpuHelper::IsNV12ConvertSupported(uint32_t fourcc) {
  switch (libyuv::CanonicalFourCC(fourcc)) {
    case libyuv::FOURCC_NV12:
    case libyuv::FOURCC_YUY2:
    case libyuv::FOURCC_MJPG:
      return true;
    default:
      return false;
  }
}

}  // namespace media
