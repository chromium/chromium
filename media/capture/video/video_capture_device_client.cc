// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_device_client.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/capture/video/scoped_buffer_pool_reservation.h"
#include "media/capture/video/video_capture_buffer_handle.h"
#include "media/capture/video/video_capture_buffer_pool.h"
#include "media/capture/video/video_frame_receiver.h"
#include "media/capture/video_capture_types.h"
#include "third_party/libyuv/include/libyuv.h"

#if defined(OS_CHROMEOS)
#include "media/capture/video/chromeos/video_capture_jpeg_decoder.h"
#endif  // defined(OS_CHROMEOS)

namespace {

bool IsFormatSupported(media::VideoPixelFormat pixel_format) {
  return (pixel_format == media::PIXEL_FORMAT_I420 ||
          // NV12 and MJPEG are used by GpuMemoryBuffer on Chrome OS.
          pixel_format == media::PIXEL_FORMAT_NV12 ||
          pixel_format == media::PIXEL_FORMAT_MJPEG ||
          pixel_format == media::PIXEL_FORMAT_Y16);
}

libyuv::RotationMode TranslateRotation(int rotation_degrees) {
  DCHECK_EQ(0, rotation_degrees % 90)
      << " Rotation must be a multiple of 90, now: " << rotation_degrees;
  libyuv::RotationMode rotation_mode = libyuv::kRotate0;
  if (rotation_degrees == 90)
    rotation_mode = libyuv::kRotate90;
  else if (rotation_degrees == 180)
    rotation_mode = libyuv::kRotate180;
  else if (rotation_degrees == 270)
    rotation_mode = libyuv::kRotate270;
  return rotation_mode;
}

void GetI420BufferAccess(
    const media::VideoCaptureDevice::Client::Buffer& buffer,
    const gfx::Size& dimensions,
    uint8_t** y_plane_data,
    uint8_t** u_plane_data,
    uint8_t** v_plane_data,
    int* y_plane_stride,
    int* uv_plane_stride) {
  *y_plane_data = buffer.handle_provider->GetHandleForInProcessAccess()->data();
  *u_plane_data = *y_plane_data + media::VideoFrame::PlaneSize(
                                      media::PIXEL_FORMAT_I420,
                                      media::VideoFrame::kYPlane, dimensions)
                                      .GetArea();
  *v_plane_data = *u_plane_data + media::VideoFrame::PlaneSize(
                                      media::PIXEL_FORMAT_I420,
                                      media::VideoFrame::kUPlane, dimensions)
                                      .GetArea();
  *y_plane_stride = dimensions.width();
  *uv_plane_stride = *y_plane_stride / 2;
}

gfx::ColorSpace OverrideColorSpaceForLibYuvConversion(
    const gfx::ColorSpace& color_space,
    const media::VideoPixelFormat pixel_format) {
  gfx::ColorSpace overriden_color_space = color_space;
  switch (pixel_format) {
    case media::PIXEL_FORMAT_UNKNOWN:  // Color format not set.
      break;
    case media::PIXEL_FORMAT_ARGB:
    case media::PIXEL_FORMAT_XRGB:
    case media::PIXEL_FORMAT_RGB24:
    case media::PIXEL_FORMAT_ABGR:
    case media::PIXEL_FORMAT_XBGR:
      // Check if we can merge data 's primary and transfer function into the
      // returned color space.
      if (color_space.IsValid()) {
        // The raw data is rgb so we expect its color space to only hold gamma
        // correction.
        DCHECK(color_space == color_space.GetAsFullRangeRGB());

        // This captured ARGB data is going to be converted to yuv using libyuv
        // ConvertToI420 which internally uses Rec601 coefficients. So build a
        // combined colorspace that contains both the above gamma correction
        // and the yuv conversion information.
        // TODO(julien.isorce): instead pass color space information to libyuv
        // once the support is added, see http://crbug.com/libyuv/835.
        overriden_color_space = color_space.GetWithMatrixAndRange(
            gfx::ColorSpace::MatrixID::SMPTE170M,
            gfx::ColorSpace::RangeID::LIMITED);
      } else {
        // Color space is not specified but it's probably safe to assume its
        // sRGB though, and so it would be valid to assume that libyuv's
        // ConvertToI420() is going to produce results in Rec601, or very close
        // to it.
        overriden_color_space = gfx::ColorSpace::CreateREC601();
      }
      break;
    default:
      break;
  }

  return overriden_color_space;
}

}  // anonymous namespace

namespace media {

class BufferPoolBufferHandleProvider
    : public VideoCaptureDevice::Client::Buffer::HandleProvider {
 public:
  BufferPoolBufferHandleProvider(
      scoped_refptr<VideoCaptureBufferPool> buffer_pool,
      int buffer_id)
      : buffer_pool_(std::move(buffer_pool)), buffer_id_(buffer_id) {}

  base::UnsafeSharedMemoryRegion DuplicateAsUnsafeRegion() override {
    return buffer_pool_->DuplicateAsUnsafeRegion(buffer_id_);
  }
  mojo::ScopedSharedBufferHandle DuplicateAsMojoBuffer() override {
    return buffer_pool_->DuplicateAsMojoBuffer(buffer_id_);
  }
  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle() override {
    return buffer_pool_->GetGpuMemoryBufferHandle(buffer_id_);
  }
  std::unique_ptr<VideoCaptureBufferHandle> GetHandleForInProcessAccess()
      override {
    return buffer_pool_->GetHandleForInProcessAccess(buffer_id_);
  }

 private:
  const scoped_refptr<VideoCaptureBufferPool> buffer_pool_;
  const int buffer_id_;
};

#if defined(OS_CHROMEOS)
VideoCaptureDeviceClient::VideoCaptureDeviceClient(
    VideoCaptureBufferType target_buffer_type,
    std::unique_ptr<VideoFrameReceiver> receiver,
    scoped_refptr<VideoCaptureBufferPool> buffer_pool,
    VideoCaptureJpegDecoderFactoryCB optional_jpeg_decoder_factory_callback)
    : target_buffer_type_(target_buffer_type),
      receiver_(std::move(receiver)),
      optional_jpeg_decoder_factory_callback_(
          std::move(optional_jpeg_decoder_factory_callback)),
      buffer_pool_(std::move(buffer_pool)),
      last_captured_pixel_format_(PIXEL_FORMAT_UNKNOWN) {
  on_started_using_gpu_cb_ =
      base::Bind(&VideoFrameReceiver::OnStartedUsingGpuDecode,
                 base::Unretained(receiver_.get()));
}
#else
VideoCaptureDeviceClient::VideoCaptureDeviceClient(
    VideoCaptureBufferType target_buffer_type,
    std::unique_ptr<VideoFrameReceiver> receiver,
    scoped_refptr<VideoCaptureBufferPool> buffer_pool)
    : target_buffer_type_(target_buffer_type),
      receiver_(std::move(receiver)),
      buffer_pool_(std::move(buffer_pool)),
      last_captured_pixel_format_(PIXEL_FORMAT_UNKNOWN) {}
#endif  // defined(OS_CHROMEOS)

VideoCaptureDeviceClient::~VideoCaptureDeviceClient() {
  for (int buffer_id : buffer_ids_known_by_receiver_)
    receiver_->OnBufferRetired(buffer_id);
  receiver_->OnStopped();
}

// static
VideoCaptureDevice::Client::Buffer VideoCaptureDeviceClient::MakeBufferStruct(
    scoped_refptr<VideoCaptureBufferPool> buffer_pool,
    int buffer_id,
    int frame_feedback_id) {
  return Buffer(
      buffer_id, frame_feedback_id,
      std::make_unique<BufferPoolBufferHandleProvider>(buffer_pool, buffer_id),
      std::make_unique<ScopedBufferPoolReservation<ProducerReleaseTraits>>(
          buffer_pool, buffer_id));
}

void VideoCaptureDeviceClient::OnIncomingCapturedData(
    const uint8_t* data,
    int length,
    const VideoCaptureFormat& format,
    const gfx::ColorSpace& data_color_space,
    int rotation,
    bool flip_y,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    int frame_feedback_id) {
  DFAKE_SCOPED_RECURSIVE_LOCK(call_from_producer_);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceClient::OnIncomingCapturedData");

  if (last_captured_pixel_format_ != format.pixel_format) {
    OnLog("Pixel format: " + VideoPixelFormatToString(format.pixel_format));
    last_captured_pixel_format_ = format.pixel_format;

#if defined(OS_CHROMEOS)
    if (format.pixel_format == PIXEL_FORMAT_MJPEG &&
        optional_jpeg_decoder_factory_callback_) {
      external_jpeg_decoder_ =
          std::move(optional_jpeg_decoder_factory_callback_).Run();
      DCHECK(external_jpeg_decoder_);
      external_jpeg_decoder_->Initialize();
    }
#endif  // defined(OS_CHROMEOS)
  }

  if (!format.IsValid()) {
    receiver_->OnFrameDropped(
        VideoCaptureFrameDropReason::kDeviceClientFrameHasInvalidFormat);
    return;
  }

  if (format.pixel_format == PIXEL_FORMAT_Y16) {
    return OnIncomingCapturedY16Data(data, length, format, reference_time,
                                     timestamp, frame_feedback_id);
  }

  // |chopped_{width,height} and |new_unrotated_{width,height}| are the lowest
  // bit decomposition of {width, height}, grabbing the odd and even parts.
  const int chopped_width = format.frame_size.width() & 1;
  const int chopped_height = format.frame_size.height() & 1;
  const int new_unrotated_width = format.frame_size.width() & ~1;
  const int new_unrotated_height = format.frame_size.height() & ~1;

  int destination_width = new_unrotated_width;
  int destination_height = new_unrotated_height;
  if (rotation == 90 || rotation == 270)
    std::swap(destination_width, destination_height);

  libyuv::RotationMode rotation_mode = TranslateRotation(rotation);

  const gfx::Size dimensions(destination_width, destination_height);
  Buffer buffer;
  auto reservation_result_code = ReserveOutputBuffer(
      dimensions, PIXEL_FORMAT_I420, frame_feedback_id, &buffer);
  if (reservation_result_code != ReserveResult::kSucceeded) {
    receiver_->OnFrameDropped(
        ConvertReservationFailureToFrameDropReason(reservation_result_code));
    return;
  }

  DCHECK(dimensions.height());
  DCHECK(dimensions.width());

  uint8_t* y_plane_data;
  uint8_t* u_plane_data;
  uint8_t* v_plane_data;
  int yplane_stride, uv_plane_stride;
  GetI420BufferAccess(buffer, dimensions, &y_plane_data, &u_plane_data,
                      &v_plane_data, &yplane_stride, &uv_plane_stride);

  int crop_x = 0;
  int crop_y = 0;
  libyuv::FourCC fourcc_format = libyuv::FOURCC_ANY;

  bool flip = false;
  switch (format.pixel_format) {
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
      DCHECK(!chopped_width && !chopped_height);
      fourcc_format = libyuv::FOURCC_YUY2;
      break;
    case PIXEL_FORMAT_RGB24:
// Linux RGB24 defines red at lowest byte address,
// see http://linuxtv.org/downloads/v4l-dvb-apis/packed-rgb.html.
// Windows RGB24 defines blue at lowest byte,
// see https://msdn.microsoft.com/en-us/library/windows/desktop/dd407253
#if defined(OS_LINUX)
      fourcc_format = libyuv::FOURCC_RAW;
#elif defined(OS_WIN)
      fourcc_format = libyuv::FOURCC_24BG;
#else
      NOTREACHED() << "RGB24 is only available in Linux and Windows platforms";
#endif
#if defined(OS_WIN)
      // TODO(wjia): Currently, for RGB24 on WIN, capture device always passes
      // in positive src_width and src_height. Remove this hardcoded value when
      // negative src_height is supported. The negative src_height indicates
      // that vertical flipping is needed.
      flip = true;
#endif
      break;
    case PIXEL_FORMAT_ARGB:
      // Windows platforms e.g. send the data vertically flipped sometimes.
      flip = flip_y;
      fourcc_format = libyuv::FOURCC_ARGB;
      break;
    case PIXEL_FORMAT_MJPEG:
      fourcc_format = libyuv::FOURCC_MJPG;
      break;
    default:
      NOTREACHED();
  }

  const gfx::ColorSpace color_space = OverrideColorSpaceForLibYuvConversion(
      data_color_space, format.pixel_format);

  // The input |length| can be greater than the required buffer size because of
  // paddings and/or alignments, but it cannot be smaller.
  DCHECK_GE(static_cast<size_t>(length), format.ImageAllocationSize());

#if defined(OS_CHROMEOS)
  if (external_jpeg_decoder_) {
    const VideoCaptureJpegDecoder::STATUS status =
        external_jpeg_decoder_->GetStatus();
    if (status == VideoCaptureJpegDecoder::FAILED) {
      external_jpeg_decoder_.reset();
    } else if (status == VideoCaptureJpegDecoder::INIT_PASSED &&
               format.pixel_format == PIXEL_FORMAT_MJPEG && rotation == 0 &&
               !flip) {
      if (on_started_using_gpu_cb_)
        std::move(on_started_using_gpu_cb_).Run();
      external_jpeg_decoder_->DecodeCapturedData(
          data, length, format, reference_time, timestamp, std::move(buffer));
      return;
    }
  }
#endif  // defined(OS_CHROMEOS)

  // libyuv::ConvertToI420 use Rec601 to convert RGB to YUV.
  if (libyuv::ConvertToI420(
          data, length, y_plane_data, yplane_stride, u_plane_data,
          uv_plane_stride, v_plane_data, uv_plane_stride, crop_x, crop_y,
          format.frame_size.width(),
          (flip ? -1 : 1) * format.frame_size.height(), new_unrotated_width,
          new_unrotated_height, rotation_mode, fourcc_format) != 0) {
    DLOG(WARNING) << "Failed to convert buffer's pixel format to I420 from "
                  << VideoPixelFormatToString(format.pixel_format);
    receiver_->OnFrameDropped(
        VideoCaptureFrameDropReason::kDeviceClientLibyuvConvertToI420Failed);
    return;
  }

  const VideoCaptureFormat output_format =
      VideoCaptureFormat(dimensions, format.frame_rate, PIXEL_FORMAT_I420);
  OnIncomingCapturedBufferExt(std::move(buffer), output_format, color_space,
                              reference_time, timestamp, gfx::Rect(dimensions),
                              VideoFrameMetadata());
}

void VideoCaptureDeviceClient::OnIncomingCapturedGfxBuffer(
    gfx::GpuMemoryBuffer* buffer,
    const VideoCaptureFormat& frame_format,
    int clockwise_rotation,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    int frame_feedback_id) {
  TRACE_EVENT0("media",
               "VideoCaptureDeviceClient::OnIncomingCapturedGfxBuffer");

  if (last_captured_pixel_format_ != frame_format.pixel_format) {
    OnLog("Pixel format: " +
          VideoPixelFormatToString(frame_format.pixel_format));
    last_captured_pixel_format_ = frame_format.pixel_format;
  }

  if (!frame_format.IsValid())
    return;

  int destination_width = buffer->GetSize().width();
  int destination_height = buffer->GetSize().height();
  if (clockwise_rotation == 90 || clockwise_rotation == 270)
    std::swap(destination_width, destination_height);

  libyuv::RotationMode rotation_mode = TranslateRotation(clockwise_rotation);

  const gfx::Size dimensions(destination_width, destination_height);
  Buffer output_buffer;
  const auto reservation_result_code = ReserveOutputBuffer(
      dimensions, PIXEL_FORMAT_I420, frame_feedback_id, &output_buffer);

  // Failed to reserve I420 output buffer, so drop the frame.
  if (reservation_result_code != ReserveResult::kSucceeded) {
    receiver_->OnFrameDropped(
        ConvertReservationFailureToFrameDropReason(reservation_result_code));
    return;
  }

  uint8_t* y_plane_data;
  uint8_t* u_plane_data;
  uint8_t* v_plane_data;
  int y_plane_stride, uv_plane_stride;
  GetI420BufferAccess(output_buffer, dimensions, &y_plane_data, &u_plane_data,
                      &v_plane_data, &y_plane_stride, &uv_plane_stride);

  int ret = -EINVAL;
  switch (frame_format.pixel_format) {
    case PIXEL_FORMAT_NV12:
      ret = libyuv::NV12ToI420Rotate(
          reinterpret_cast<uint8_t*>(buffer->memory(0)), buffer->stride(0),
          reinterpret_cast<uint8_t*>(buffer->memory(1)), buffer->stride(1),
          y_plane_data, y_plane_stride, u_plane_data, uv_plane_stride,
          v_plane_data, uv_plane_stride, buffer->GetSize().width(),
          buffer->GetSize().height(), rotation_mode);
      break;

    default:
      LOG(ERROR) << "Unsupported format: "
                 << VideoPixelFormatToString(frame_format.pixel_format);
  }
  if (ret) {
    DLOG(WARNING) << "Failed to convert buffer's pixel format to I420 from "
                  << VideoPixelFormatToString(frame_format.pixel_format);
    return;
  }

  const VideoCaptureFormat output_format = VideoCaptureFormat(
      dimensions, frame_format.frame_rate, PIXEL_FORMAT_I420);
  OnIncomingCapturedBuffer(std::move(output_buffer), output_format,
                           reference_time, timestamp);
}

VideoCaptureDevice::Client::ReserveResult
VideoCaptureDeviceClient::ReserveOutputBuffer(const gfx::Size& frame_size,
                                              VideoPixelFormat pixel_format,
                                              int frame_feedback_id,
                                              Buffer* buffer) {
  DFAKE_SCOPED_RECURSIVE_LOCK(call_from_producer_);
  DCHECK_GT(frame_size.width(), 0);
  DCHECK_GT(frame_size.height(), 0);
  DCHECK(IsFormatSupported(pixel_format));

  int buffer_id_to_drop = VideoCaptureBufferPool::kInvalidId;
  int buffer_id = VideoCaptureBufferPool::kInvalidId;
  auto reservation_result_code = buffer_pool_->ReserveForProducer(
      frame_size, pixel_format, nullptr, frame_feedback_id, &buffer_id,
      &buffer_id_to_drop);
  if (buffer_id_to_drop != VideoCaptureBufferPool::kInvalidId) {
    // |buffer_pool_| has decided to release a buffer. Notify receiver in case
    // the buffer has already been shared with it.
    auto entry_iter =
        std::find(buffer_ids_known_by_receiver_.begin(),
                  buffer_ids_known_by_receiver_.end(), buffer_id_to_drop);
    if (entry_iter != buffer_ids_known_by_receiver_.end()) {
      buffer_ids_known_by_receiver_.erase(entry_iter);
      receiver_->OnBufferRetired(buffer_id_to_drop);
    }
  }
  if (reservation_result_code != ReserveResult::kSucceeded)
    return reservation_result_code;

  DCHECK_NE(VideoCaptureBufferPool::kInvalidId, buffer_id);

  if (!base::Contains(buffer_ids_known_by_receiver_, buffer_id)) {
    media::mojom::VideoBufferHandlePtr buffer_handle =
        media::mojom::VideoBufferHandle::New();
    switch (target_buffer_type_) {
      case VideoCaptureBufferType::kSharedMemory:
        buffer_handle->set_shared_buffer_handle(
            buffer_pool_->DuplicateAsMojoBuffer(buffer_id));
        break;
      case VideoCaptureBufferType::kSharedMemoryViaRawFileDescriptor:
        buffer_handle->set_shared_memory_via_raw_file_descriptor(
            buffer_pool_->CreateSharedMemoryViaRawFileDescriptorStruct(
                buffer_id));
        break;
      case VideoCaptureBufferType::kMailboxHolder:
        NOTREACHED();
        break;
      case VideoCaptureBufferType::kGpuMemoryBuffer:
        buffer_handle->set_gpu_memory_buffer_handle(
            buffer_pool_->GetGpuMemoryBufferHandle(buffer_id));
        break;
    }
    receiver_->OnNewBuffer(buffer_id, std::move(buffer_handle));
    buffer_ids_known_by_receiver_.push_back(buffer_id);
  }

  *buffer = MakeBufferStruct(buffer_pool_, buffer_id, frame_feedback_id);
  return ReserveResult::kSucceeded;
}

void VideoCaptureDeviceClient::OnIncomingCapturedBuffer(
    Buffer buffer,
    const VideoCaptureFormat& format,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp) {
  DFAKE_SCOPED_RECURSIVE_LOCK(call_from_producer_);
  OnIncomingCapturedBufferExt(
      std::move(buffer), format, gfx::ColorSpace(), reference_time, timestamp,
      gfx::Rect(format.frame_size), VideoFrameMetadata());
}

void VideoCaptureDeviceClient::OnIncomingCapturedBufferExt(
    Buffer buffer,
    const VideoCaptureFormat& format,
    const gfx::ColorSpace& color_space,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    gfx::Rect visible_rect,
    const VideoFrameMetadata& additional_metadata) {
  DFAKE_SCOPED_RECURSIVE_LOCK(call_from_producer_);

  VideoFrameMetadata metadata;
  metadata.MergeMetadataFrom(&additional_metadata);
  metadata.SetDouble(VideoFrameMetadata::FRAME_RATE, format.frame_rate);
  metadata.SetTimeTicks(VideoFrameMetadata::REFERENCE_TIME, reference_time);

  mojom::VideoFrameInfoPtr info = mojom::VideoFrameInfo::New();
  info->timestamp = timestamp;
  info->pixel_format = format.pixel_format;
  info->color_space = color_space;
  info->coded_size = format.frame_size;
  info->visible_rect = visible_rect;
  info->metadata = metadata.GetInternalValues().Clone();

  buffer_pool_->HoldForConsumers(buffer.id, 1);
  receiver_->OnFrameReadyInBuffer(
      buffer.id, buffer.frame_feedback_id,
      std::make_unique<ScopedBufferPoolReservation<ConsumerReleaseTraits>>(
          buffer_pool_, buffer.id),
      std::move(info));
}

void VideoCaptureDeviceClient::OnError(VideoCaptureError error,
                                       const base::Location& from_here,
                                       const std::string& reason) {
  const std::string log_message = base::StringPrintf(
      "error@ %s, %s, OS message: %s", from_here.ToString().c_str(),
      reason.c_str(),
      logging::SystemErrorCodeToString(logging::GetLastSystemErrorCode())
          .c_str());
  DLOG(ERROR) << log_message;
  OnLog(log_message);
  receiver_->OnError(error);
}

void VideoCaptureDeviceClient::OnFrameDropped(
    VideoCaptureFrameDropReason reason) {
  receiver_->OnFrameDropped(reason);
}

void VideoCaptureDeviceClient::OnLog(const std::string& message) {
  receiver_->OnLog(message);
}

void VideoCaptureDeviceClient::OnStarted() {
  receiver_->OnStarted();
}

double VideoCaptureDeviceClient::GetBufferPoolUtilization() const {
  return buffer_pool_->GetBufferPoolUtilization();
}

void VideoCaptureDeviceClient::OnIncomingCapturedY16Data(
    const uint8_t* data,
    int length,
    const VideoCaptureFormat& format,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    int frame_feedback_id) {
  Buffer buffer;
  const auto reservation_result_code = ReserveOutputBuffer(
      format.frame_size, PIXEL_FORMAT_Y16, frame_feedback_id, &buffer);
  // The input |length| can be greater than the required buffer size because of
  // paddings and/or alignments, but it cannot be smaller.
  DCHECK_GE(static_cast<size_t>(length), format.ImageAllocationSize());
  // Failed to reserve output buffer, so drop the frame.
  if (reservation_result_code != ReserveResult::kSucceeded) {
    receiver_->OnFrameDropped(
        ConvertReservationFailureToFrameDropReason(reservation_result_code));
    return;
  }
  auto buffer_access = buffer.handle_provider->GetHandleForInProcessAccess();
  memcpy(buffer_access->data(), data, length);
  const VideoCaptureFormat output_format = VideoCaptureFormat(
      format.frame_size, format.frame_rate, PIXEL_FORMAT_Y16);
  OnIncomingCapturedBuffer(std::move(buffer), output_format, reference_time,
                           timestamp);
}

}  // namespace media
