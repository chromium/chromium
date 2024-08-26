// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/capture/video/video_capture_device_client.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/video_types.h"
#include "media/capture/mojom/video_capture_buffer.mojom-forward.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/mojom/video_capture_types.mojom-forward.h"
#include "media/capture/video/scoped_buffer_pool_reservation.h"
#include "media/capture/video/video_capture_buffer_handle.h"
#include "media/capture/video/video_capture_buffer_pool.h"
#include "media/capture/video/video_frame_receiver.h"
#include "media/capture/video_capture_types.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/libyuv/include/libyuv.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "media/capture/video/chromeos/video_capture_jpeg_decoder.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
#include "media/base/media_switches.h"
#include "media/capture/video/video_capture_effects_processor.h"
#endif  //  BUILDFLAG(ENABLE_VIDEO_EFFECTS)

namespace {

bool IsFormatSupported(media::VideoPixelFormat pixel_format) {
  return (pixel_format == media::PIXEL_FORMAT_I420 ||
          // NV12 and MJPEG are used by GpuMemoryBuffer on Chrome OS.
          pixel_format == media::PIXEL_FORMAT_NV12 ||
          pixel_format == media::PIXEL_FORMAT_MJPEG ||
          pixel_format == media::PIXEL_FORMAT_Y16);
}

libyuv::RotationMode TranslateRotation(int rotation_degrees) {
  CHECK_EQ(0, rotation_degrees % 90)
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
                                      media::VideoFrame::Plane::kY, dimensions)
                                      .GetArea();
  *v_plane_data = *u_plane_data + media::VideoFrame::PlaneSize(
                                      media::PIXEL_FORMAT_I420,
                                      media::VideoFrame::Plane::kU, dimensions)
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
        // Color space is not specified but it is probably safe to assume it is
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

struct FourccAndFlip {
  libyuv::FourCC fourcc_format = libyuv::FOURCC_ANY;
  bool flip = false;
};

FourccAndFlip GetFourccAndFlipFromPixelFormat(
    const media::VideoCaptureFormat& format,
    bool flip_y) {
  const int is_width_odd = format.frame_size.width() & 1;
  const int is_height_odd = format.frame_size.height() & 1;

  switch (format.pixel_format) {
    case media::PIXEL_FORMAT_UNKNOWN:  // Color format not set.
      return {};
    case media::PIXEL_FORMAT_I420:
      CHECK(!is_width_odd && !is_height_odd);
      return {libyuv::FOURCC_I420};
    case media::PIXEL_FORMAT_YV12:
      CHECK(!is_width_odd && !is_height_odd);
      return {libyuv::FOURCC_YV12};
    case media::PIXEL_FORMAT_NV12:
      CHECK(!is_width_odd && !is_height_odd);
      return {libyuv::FOURCC_NV12};
    case media::PIXEL_FORMAT_NV21:
      CHECK(!is_width_odd && !is_height_odd);
      return {libyuv::FOURCC_NV21};
    case media::PIXEL_FORMAT_YUY2:
      CHECK(!is_width_odd && !is_height_odd);
      return {libyuv::FOURCC_YUY2};
    case media::PIXEL_FORMAT_UYVY:
      CHECK(!is_width_odd && !is_height_odd);
      return {libyuv::FOURCC_UYVY};
    case media::PIXEL_FORMAT_RGB24:
      if constexpr (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) {
        // Linux RGB24 defines red at lowest byte address,
        // see http://linuxtv.org/downloads/v4l-dvb-apis/packed-rgb.html.
        return {libyuv::FOURCC_RAW};
      } else if constexpr (BUILDFLAG(IS_WIN)) {
        // Windows RGB24 defines blue at lowest byte,
        // see https://msdn.microsoft.com/en-us/library/windows/desktop/dd407253

        // TODO(wjia): Currently, for RGB24 on WIN, capture device always passes
        // in positive src_width and src_height. Remove this hardcoded value
        // when negative src_height is supported. The negative src_height
        // indicates that vertical flipping is needed.
        return {libyuv::FOURCC_24BG, true};
      } else {
        NOTREACHED()
            << "RGB24 is only available in Linux and Windows platforms";
      }
    case media::PIXEL_FORMAT_ARGB:
      // Windows platforms e.g. send the data vertically flipped sometimes.
      return {libyuv::FOURCC_ARGB, flip_y};
    case media::PIXEL_FORMAT_MJPEG:
      return {libyuv::FOURCC_MJPG};
    default:
      NOTREACHED();
  }
}

}  // anonymous namespace

namespace media {

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/40070224): When this code path has been verified on
// Canary, change to enabled-by-default.
BASE_FEATURE(kFallbackToSharedMemoryIfNotNv12OnMac,
             "FallbackToSharedMemoryIfNotNv12OnMac",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

namespace {

class ScopedAccessPermissionEndWithCallback
    : public VideoCaptureDevice::Client::Buffer::ScopedAccessPermission {
 public:
  explicit ScopedAccessPermissionEndWithCallback(base::OnceClosure closure)
      : closure_(std::move(closure)) {}
  ~ScopedAccessPermissionEndWithCallback() override {
    std::move(closure_).Run();
  }

 private:
  base::OnceClosure closure_;
};

}  // anonymous namespace

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

VideoEffectsContext::VideoEffectsContext(
    mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor> remote)
    : video_effects_processor_(std::move(remote)) {}

VideoEffectsContext::VideoEffectsContext(VideoEffectsContext&& other) = default;
VideoEffectsContext& VideoEffectsContext::operator=(
    VideoEffectsContext&& other) = default;

VideoEffectsContext::~VideoEffectsContext() = default;

mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>&&
VideoEffectsContext::TakeVideoEffectsProcessor() {
  return std::move(video_effects_processor_);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
VideoCaptureDeviceClient::VideoCaptureDeviceClient(
    std::unique_ptr<VideoFrameReceiver> receiver,
    scoped_refptr<VideoCaptureBufferPool> buffer_pool,
    VideoCaptureJpegDecoderFactoryCB optional_jpeg_decoder_factory_callback)
    : receiver_(std::move(receiver)),
      optional_jpeg_decoder_factory_callback_(
          std::move(optional_jpeg_decoder_factory_callback)),
      buffer_pool_(std::move(buffer_pool)),
      last_captured_pixel_format_(PIXEL_FORMAT_UNKNOWN) {
  on_started_using_gpu_cb_ =
      base::BindOnce(&VideoFrameReceiver::OnStartedUsingGpuDecode,
                     base::Unretained(receiver_.get()));
}
#else
VideoCaptureDeviceClient::VideoCaptureDeviceClient(
    std::unique_ptr<VideoFrameReceiver> receiver,
    scoped_refptr<VideoCaptureBufferPool> buffer_pool,
    VideoEffectsContext video_effects_context)
    : receiver_(std::move(receiver)),
      buffer_pool_(std::move(buffer_pool)),
      last_captured_pixel_format_(PIXEL_FORMAT_UNKNOWN) {
#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
  effects_processor_.emplace(video_effects_context.TakeVideoEffectsProcessor());
#endif  // BUILDFLAG(ENABLE_VIDEO_EFFECTS)
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

VideoCaptureDeviceClient::~VideoCaptureDeviceClient() {
  for (int buffer_id : buffer_ids_known_by_receiver_) {
    receiver_->OnBufferRetired(buffer_id);
  }
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

void VideoCaptureDeviceClient::OnCaptureConfigurationChanged() {
  receiver_->OnCaptureConfigurationChanged();
}

#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
void VideoCaptureDeviceClient::OnPostProcessDone(
    base::expected<PostProcessDoneInfo, video_effects::mojom::PostProcessError>
        post_process_info_or_error) {
  if (!post_process_info_or_error.has_value()) {
    // On post-process failure, report that a frame was dropped. We cannot
    // fall back to the unprocessed frame because some privacy-preserving
    // effect could have been applied. The decision to disable misbehaving
    // effects must be made by the user.
    receiver_->OnFrameDropped(
        VideoCaptureFrameDropReason::kPostProcessingFailed);
    return;
  }

  Buffer buffer = std::move(post_process_info_or_error->buffer);
  mojom::VideoFrameInfoPtr info = std::move(post_process_info_or_error->info);

  buffer_pool_->HoldForConsumers(buffer.id, 1);
  receiver_->OnFrameReadyInBuffer(ReadyFrameInBuffer(
      buffer.id, buffer.frame_feedback_id,
      std::make_unique<ScopedBufferPoolReservation<ConsumerReleaseTraits>>(
          buffer_pool_, buffer.id),
      std::move(info)));
}
#endif

void VideoCaptureDeviceClient::OnIncomingCapturedData(
    const uint8_t* data,
    int length,
    const VideoCaptureFormat& format,
    const gfx::ColorSpace& data_color_space,
    int rotation,
    bool flip_y,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    std::optional<base::TimeTicks> capture_begin_timestamp,
    int frame_feedback_id) {
  DFAKE_SCOPED_RECURSIVE_LOCK(call_from_producer_);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceClient::OnIncomingCapturedData");

  // The input |length| can be greater than the required buffer size because of
  // paddings and/or alignments, but it cannot be smaller.
  CHECK_GE(static_cast<size_t>(length),
           media::VideoFrame::AllocationSize(format.pixel_format,
                                             format.frame_size));

  if (last_captured_pixel_format_ != format.pixel_format) {
    OnLog("Pixel format: " + VideoPixelFormatToString(format.pixel_format));
    last_captured_pixel_format_ = format.pixel_format;

#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (format.pixel_format == PIXEL_FORMAT_MJPEG &&
        optional_jpeg_decoder_factory_callback_) {
      external_jpeg_decoder_ =
          std::move(optional_jpeg_decoder_factory_callback_).Run();
      CHECK(external_jpeg_decoder_);
      external_jpeg_decoder_->Initialize();
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  if (!format.IsValid()) {
    receiver_->OnFrameDropped(
        VideoCaptureFrameDropReason::kDeviceClientFrameHasInvalidFormat);
    return;
  }

  if (format.pixel_format == PIXEL_FORMAT_Y16) {
    return OnIncomingCapturedY16Data(data, length, format, reference_time,
                                     timestamp, capture_begin_timestamp,
                                     frame_feedback_id);
  }

  // |new_unrotated_{width,height}| are the dimensions of the output buffer that
  // satisfy the video pixel format requirements. For I420, this is equivalent
  // to rounding to nearest even number (away from zero, eg 13 becomes 14, -13
  // becomes -14).
  const int new_unrotated_width = format.frame_size.width() & ~1;
  const int new_unrotated_height = format.frame_size.height() & ~1;

  int destination_width = new_unrotated_width;
  int destination_height = new_unrotated_height;
  if (rotation == 90 || rotation == 270)
    std::swap(destination_width, destination_height);

  const gfx::Size dimensions(destination_width, destination_height);
  CHECK(dimensions.height());
  CHECK(dimensions.width());

  const libyuv::RotationMode rotation_mode = TranslateRotation(rotation);

  Buffer buffer;
  auto reservation_result_code = ReserveOutputBuffer(
      dimensions, PIXEL_FORMAT_I420, frame_feedback_id, &buffer,
      /*require_new_buffer_id=*/nullptr, /*retire_old_buffer_id=*/nullptr);
  if (reservation_result_code != ReserveResult::kSucceeded) {
    receiver_->OnFrameDropped(
        ConvertReservationFailureToFrameDropReason(reservation_result_code));
    return;
  }

#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
  if (base::FeatureList::IsEnabled(media::kCameraMicEffects)) {
    VideoFrameMetadata metadata;
    // Note: we are not setting `metadata.is_webgpu_compatible` here since we
    // have not verified whether the buffer pool returns frames that are
    // WebGPU-compatible across all platforms.
    metadata.frame_rate = format.frame_rate;
    metadata.reference_time = reference_time;
    metadata.capture_begin_time = capture_begin_timestamp;

    mojom::VideoFrameInfoPtr info = mojom::VideoFrameInfo::New(
        timestamp, metadata, format.pixel_format, format.frame_size,
        gfx::Rect(format.frame_size), buffer.is_premapped, data_color_space,
        mojom::PlaneStridesPtr{});

    // Must happen here since we move out of `buffer` in the call below:
    const VideoCaptureBufferType buffer_type =
        buffer_pool_->GetBufferType(buffer.id);

    // The `buffer` was already reserved above but has not yet been reported as
    // ready to the `receiver_`. Once the post-processor has completed, we will
    // call `OnPostProcessDone()` & thus notify the receiver from there.
    effects_processor_->PostProcessData(
        base::make_span(data, base::checked_cast<size_t>(length)),
        std::move(info), std::move(buffer),
        VideoCaptureFormat(format.frame_size, format.frame_rate,
                           VideoPixelFormat::PIXEL_FORMAT_I420),
        buffer_type,
        base::BindOnce(&VideoCaptureDeviceClient::OnPostProcessDone,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }
#endif

  const auto [fourcc_format, flip] =
      GetFourccAndFlipFromPixelFormat(format, flip_y);

  uint8_t* y_plane_data;
  uint8_t* u_plane_data;
  uint8_t* v_plane_data;
  int yplane_stride, uv_plane_stride;
  GetI420BufferAccess(buffer, dimensions, &y_plane_data, &u_plane_data,
                      &v_plane_data, &yplane_stride, &uv_plane_stride);

  const gfx::ColorSpace color_space = OverrideColorSpaceForLibYuvConversion(
      data_color_space, format.pixel_format);

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // libyuv::ConvertToI420 uses Rec601 to convert RGB to YUV.
  if (libyuv::ConvertToI420(
          data, length, y_plane_data, yplane_stride, u_plane_data,
          uv_plane_stride, v_plane_data, uv_plane_stride, /*crop_x=*/0,
          /*crop_y=*/0, format.frame_size.width(),
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
  OnIncomingCapturedBufferExt(
      std::move(buffer), output_format, color_space, reference_time, timestamp,
      capture_begin_timestamp, gfx::Rect(dimensions), VideoFrameMetadata());
}

void VideoCaptureDeviceClient::OnIncomingCapturedGfxBuffer(
    gfx::GpuMemoryBuffer* buffer,
    const VideoCaptureFormat& frame_format,
    int clockwise_rotation,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    std::optional<base::TimeTicks> capture_begin_timestamp,
    int frame_feedback_id) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceClient::OnIncomingCapturedGfxBuffer");

  if (last_captured_pixel_format_ != frame_format.pixel_format) {
    OnLog("Pixel format: " +
          VideoPixelFormatToString(frame_format.pixel_format));
    last_captured_pixel_format_ = frame_format.pixel_format;
  }

  if (!frame_format.IsValid()) {
    receiver_->OnFrameDropped(
        VideoCaptureFrameDropReason::kDeviceClientFrameHasInvalidFormat);
    return;
  }

  int destination_width = buffer->GetSize().width();
  int destination_height = buffer->GetSize().height();
  if (clockwise_rotation == 90 || clockwise_rotation == 270)
    std::swap(destination_width, destination_height);

  libyuv::RotationMode rotation_mode = TranslateRotation(clockwise_rotation);

  const gfx::Size dimensions(destination_width, destination_height);
  Buffer output_buffer;
  const auto reservation_result_code = ReserveOutputBuffer(
      dimensions, PIXEL_FORMAT_I420, frame_feedback_id, &output_buffer,
      /*require_new_buffer_id=*/nullptr, /*retire_old_buffer_id=*/nullptr);

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

  if (!buffer->Map()) {
    LOG(ERROR) << "Failed to map GPU memory buffer";
    receiver_->OnFrameDropped(
        VideoCaptureFrameDropReason::kGpuMemoryBufferMapFailed);
    return;
  }
  absl::Cleanup scoped_unmap = [buffer] { buffer->Unmap(); };

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
    receiver_->OnFrameDropped(
        VideoCaptureFrameDropReason::kDeviceClientLibyuvConvertToI420Failed);
    return;
  }

  const VideoCaptureFormat output_format = VideoCaptureFormat(
      dimensions, frame_format.frame_rate, PIXEL_FORMAT_I420);
  OnIncomingCapturedBuffer(std::move(output_buffer), output_format,
                           reference_time, timestamp, capture_begin_timestamp);
}

void VideoCaptureDeviceClient::OnIncomingCapturedExternalBuffer(
    CapturedExternalVideoBuffer buffer,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    std::optional<base::TimeTicks> capture_begin_timestamp,
    const gfx::Rect& visible_rect) {
  ReadyFrameInBuffer ready_frame;
  if (CreateReadyFrameFromExternalBuffer(
          std::move(buffer), reference_time, timestamp, capture_begin_timestamp,
          visible_rect, &ready_frame) != ReserveResult::kSucceeded) {
    DVLOG(2) << __func__
             << " CreateReadyFrameFromExternalBuffer failed: reservation "
                "tracker failed.";
    return;
  }
  receiver_->OnFrameReadyInBuffer(std::move(ready_frame));
}

VideoCaptureDevice::Client::ReserveResult
VideoCaptureDeviceClient::CreateReadyFrameFromExternalBuffer(
    CapturedExternalVideoBuffer buffer,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    std::optional<base::TimeTicks> capture_begin_timestamp,
    const gfx::Rect& visible_rect,
    ReadyFrameInBuffer* ready_buffer) {
  // Reserve an ID for this buffer that will not conflict with any of the IDs
  // used by |buffer_pool_|.
  int buffer_id_to_drop = VideoCaptureBufferPool::kInvalidId;
  int buffer_id = VideoCaptureBufferPool::kInvalidId;
  // Use std::move to transfer the handle ownership here since the buffer will
  // be created and confirm each ScopedHandle can only have one owner at a
  // time. Because the subsequent code mojom::VideoFrameInfoPtr needs to use the
  // buffer information, so here use |buffer_for_reserve_id| instead of
  // std::move(buffer).
  CapturedExternalVideoBuffer buffer_for_reserve_id =
      CapturedExternalVideoBuffer(std::move(buffer.handle), buffer.format,
                                  buffer.color_space);
#if BUILDFLAG(IS_WIN)
  buffer_for_reserve_id.imf_buffer = std::move(buffer.imf_buffer);
#endif
  VideoCaptureDevice::Client::ReserveResult reservation_result_code =
      buffer_pool_->ReserveIdForExternalBuffer(std::move(buffer_for_reserve_id),
                                               visible_rect.size(),
                                               &buffer_id_to_drop, &buffer_id);
  // If a buffer to retire was specified, retire one.
  if (buffer_id_to_drop != VideoCaptureBufferPool::kInvalidId) {
    auto entry_iter =
        base::ranges::find(buffer_ids_known_by_receiver_, buffer_id_to_drop);
    if (entry_iter != buffer_ids_known_by_receiver_.end()) {
      buffer_ids_known_by_receiver_.erase(entry_iter);
      receiver_->OnBufferRetired(buffer_id_to_drop);
    }
  }

  if (reservation_result_code != ReserveResult::kSucceeded) {
    return reservation_result_code;
  }

  // Register the buffer with the receiver if it is new.
  if (!base::Contains(buffer_ids_known_by_receiver_, buffer_id)) {
    // On windows, 'GetGpuMemoryBufferHandle' will duplicate a new handle which
    // refers to the same object as the original handle.
    // https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle
    media::mojom::VideoBufferHandlePtr buffer_handle =
        media::mojom::VideoBufferHandle::NewGpuMemoryBufferHandle(
            buffer_pool_->GetGpuMemoryBufferHandle(buffer_id));
    receiver_->OnNewBuffer(buffer_id, std::move(buffer_handle));
    buffer_ids_known_by_receiver_.push_back(buffer_id);
  }

  // Construct the ready frame, to be passed on to the |receiver_| by the caller
  // of this method.
  VideoFrameMetadata metadata;
  // Note: we are not setting `metadata.is_webgpu_compatible` here since we
  // have not verified whether the external buffer is WebGPU-compatible on all
  // platforms.
  metadata.frame_rate = buffer.format.frame_rate;
  metadata.reference_time = reference_time;
  metadata.capture_begin_time = capture_begin_timestamp;

  mojom::VideoFrameInfoPtr info = mojom::VideoFrameInfo::New(
      timestamp, metadata, buffer.format.pixel_format, buffer.format.frame_size,
      visible_rect, /*is_premapped=*/false, buffer.color_space,
      mojom::PlaneStridesPtr{});

  buffer_pool_->HoldForConsumers(buffer_id, 1);
  buffer_pool_->RelinquishProducerReservation(buffer_id);

  *ready_buffer = ReadyFrameInBuffer(
      buffer_id, 0 /* frame_feedback_id */,
      std::make_unique<ScopedBufferPoolReservation<ConsumerReleaseTraits>>(
          buffer_pool_, buffer_id),
      std::move(info));
  return VideoCaptureDevice::Client::ReserveResult::kSucceeded;
}

VideoCaptureDevice::Client::ReserveResult
VideoCaptureDeviceClient::ReserveOutputBuffer(const gfx::Size& frame_size,
                                              VideoPixelFormat pixel_format,
                                              int frame_feedback_id,
                                              Buffer* buffer,
                                              int* require_new_buffer_id,
                                              int* retire_old_buffer_id) {
  DFAKE_SCOPED_RECURSIVE_LOCK(call_from_producer_);
  CHECK_GT(frame_size.width(), 0);
  CHECK_GT(frame_size.height(), 0);
  CHECK(IsFormatSupported(pixel_format));

  int buffer_id_to_drop = VideoCaptureBufferPool::kInvalidId;
  if (require_new_buffer_id) {
    *require_new_buffer_id = VideoCaptureBufferPool::kInvalidId;
  }
  if (retire_old_buffer_id) {
    *retire_old_buffer_id = VideoCaptureBufferPool::kInvalidId;
  }
  int buffer_id = VideoCaptureBufferPool::kInvalidId;
  auto reservation_result_code = buffer_pool_->ReserveForProducer(
      frame_size, pixel_format, nullptr, frame_feedback_id, &buffer_id,
      &buffer_id_to_drop);
  if (buffer_id_to_drop != VideoCaptureBufferPool::kInvalidId) {
    // |buffer_pool_| has decided to release a buffer. Notify receiver in case
    // the buffer has already been shared with it.
    auto entry_iter =
        base::ranges::find(buffer_ids_known_by_receiver_, buffer_id_to_drop);
    if (entry_iter != buffer_ids_known_by_receiver_.end()) {
      buffer_ids_known_by_receiver_.erase(entry_iter);
      if (retire_old_buffer_id) {
        *retire_old_buffer_id = buffer_id_to_drop;
      }
      receiver_->OnBufferRetired(buffer_id_to_drop);
    }
  }
  if (reservation_result_code != ReserveResult::kSucceeded) {
    DVLOG(2) << __func__ << " reservation failed";
    return reservation_result_code;
  }

  CHECK_NE(VideoCaptureBufferPool::kInvalidId, buffer_id);

  if (!base::Contains(buffer_ids_known_by_receiver_, buffer_id)) {
    const VideoCaptureBufferType target_buffer_type =
        buffer_pool_->GetBufferType(buffer_id);

    media::mojom::VideoBufferHandlePtr buffer_handle;
    switch (target_buffer_type) {
      case VideoCaptureBufferType::kSharedMemory:
        buffer_handle = media::mojom::VideoBufferHandle::NewUnsafeShmemRegion(
            buffer_pool_->DuplicateAsUnsafeRegion(buffer_id));
        break;
      case VideoCaptureBufferType::kMailboxHolder:
        NOTREACHED();
      case VideoCaptureBufferType::kGpuMemoryBuffer:
        buffer_handle =
            media::mojom::VideoBufferHandle::NewGpuMemoryBufferHandle(
                buffer_pool_->GetGpuMemoryBufferHandle(buffer_id));
        break;
    }
    receiver_->OnNewBuffer(buffer_id, std::move(buffer_handle));
    if (require_new_buffer_id) {
      *require_new_buffer_id = buffer_id;
    }
    buffer_ids_known_by_receiver_.push_back(buffer_id);
  }

  *buffer = MakeBufferStruct(buffer_pool_, buffer_id, frame_feedback_id);
  return ReserveResult::kSucceeded;
}

void VideoCaptureDeviceClient::OnIncomingCapturedBuffer(
    Buffer buffer,
    const VideoCaptureFormat& format,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    std::optional<base::TimeTicks> capture_begin_timestamp) {
  DFAKE_SCOPED_RECURSIVE_LOCK(call_from_producer_);
  OnIncomingCapturedBufferExt(
      std::move(buffer), format, gfx::ColorSpace(), reference_time, timestamp,
      capture_begin_timestamp, gfx::Rect(format.frame_size),
      VideoFrameMetadata());
}

void VideoCaptureDeviceClient::OnIncomingCapturedBufferExt(
    Buffer buffer,
    const VideoCaptureFormat& format,
    const gfx::ColorSpace& color_space,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    std::optional<base::TimeTicks> capture_begin_timestamp,
    gfx::Rect visible_rect,
    const VideoFrameMetadata& additional_metadata) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "VideoCaptureDeviceClient::OnIncomingCapturedBufferExt");
  DFAKE_SCOPED_RECURSIVE_LOCK(call_from_producer_);

  VideoFrameMetadata metadata = additional_metadata;
  // Note: we are not setting `metadata.is_webgpu_compatible` here since we
  // have not verified whether the buffer pool returns frames that are
  // WebGPU-compatible across all platforms.
  metadata.frame_rate = format.frame_rate;
  metadata.reference_time = reference_time;
  metadata.capture_begin_time = capture_begin_timestamp;

  mojom::VideoFrameInfoPtr info = mojom::VideoFrameInfo::New(
      timestamp, metadata, format.pixel_format, format.frame_size, visible_rect,
      buffer.is_premapped, color_space, mojom::PlaneStridesPtr{});

#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
  if (base::FeatureList::IsEnabled(media::kCameraMicEffects)) {
    // We need to allocate the output buffer since the post-processor cannot
    // operate in-place. This new `out_buffer`, along with original `buffer`,
    // will be considered as held for producer until the post-processor has
    // finished processing their contents, after which the `buffer` should be
    // marked as unused (`RelinquishProducerReservation()`) and `out_buffer`
    // will be marked as held for consumer.
    // Note that this means we're allocating 2x as many buffers as we'd have
    // allocated without the video effects. It may be possible to hold on to
    // the input buffer for less time than what is needed to post-process it
    // - it could be released once the processor has imported it into the
    // graphical API it uses to run the post-processing logic.
    // TODO(https://crbug.com/339141106): Consider having an additional pool
    // for post-processing output buffers, separate from the pool used to
    // allocate the original buffers.

    Buffer out_buffer;
    const VideoCaptureDevice::Client::ReserveResult reserve_result =
        ReserveOutputBuffer(format.frame_size, format.pixel_format,
                            buffer.frame_feedback_id, &out_buffer, nullptr,
                            nullptr);

    const bool reserve_succeeded =
        reserve_result == VideoCaptureDevice::Client::ReserveResult::kSucceeded;

    if (reserve_succeeded) {
      // Must happen here since we move out of `buffer` & `out_buffer` in the
      // call to post-processor:
      const VideoCaptureBufferType in_buffer_type =
          buffer_pool_->GetBufferType(buffer.id);

      const VideoCaptureBufferType out_buffer_type =
          buffer_pool_->GetBufferType(out_buffer.id);

      // The buffers were reserved but has not yet been reported as ready to the
      // `receiver_`. Once the post-processor has completed, we will call
      // `OnPostProcessDone()` & thus notify the receiver from there.

      // TODO(https://crbug.com/345688428): drop the frame if we're already
      // waiting for processing to finish for too many. Maybe if pool
      // utilization is approaching 70%?
      effects_processor_->PostProcessBuffer(
          std::move(buffer), std::move(info), in_buffer_type,
          std::move(out_buffer), format, out_buffer_type,
          base::BindOnce(&VideoCaptureDeviceClient::OnPostProcessDone,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    } else {
      // We weren't able to reserve the buffer for the post-processor's
      // result. We could either drop the frame or deliver the unprocessed
      // buffer to the consumer, but since post-processing can apply
      // privacy-preserving effects, we should not deliver unprocessed frames
      // without user intervention, hence we report failure.

      receiver_->OnFrameDropped(
          ConvertReservationFailureToFrameDropReason(reserve_result));
      return;
    }
  }
#endif

  buffer_pool_->HoldForConsumers(buffer.id, 1);
  receiver_->OnFrameReadyInBuffer(ReadyFrameInBuffer(
      buffer.id, buffer.frame_feedback_id,
      std::make_unique<ScopedBufferPoolReservation<ConsumerReleaseTraits>>(
          buffer_pool_, buffer.id),
      std::move(info)));
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
    std::optional<base::TimeTicks> capture_begin_timestamp,
    int frame_feedback_id) {
  Buffer buffer;
  const auto reservation_result_code = ReserveOutputBuffer(
      format.frame_size, PIXEL_FORMAT_Y16, frame_feedback_id, &buffer,
      /*require_new_buffer_id=*/nullptr, /*retire_old_buffer_id=*/nullptr);
  // The input |length| can be greater than the required buffer size because of
  // paddings and/or alignments, but it cannot be smaller.
  CHECK_GE(static_cast<size_t>(length),
           media::VideoFrame::AllocationSize(format.pixel_format,
                                             format.frame_size));
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
                           timestamp, capture_begin_timestamp);
}
}  // namespace media
