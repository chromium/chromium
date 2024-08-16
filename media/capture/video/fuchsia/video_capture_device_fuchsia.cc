// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/capture/video/fuchsia/video_capture_device_fuchsia.h"

#include <zircon/status.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "media/base/video_types.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/video_common.h"
#include "ui/gfx/buffer_format_util.h"

namespace media {

namespace {

size_t RoundUp(size_t value, size_t alignment) {
  return ((value + alignment - 1) / alignment) * alignment;
}

libyuv::FourCC GetFourccForPixelFormat(
    fuchsia::images2::PixelFormat src_pixel_format) {
  switch (src_pixel_format) {
    case fuchsia::images2::PixelFormat::I420:
      return libyuv::FourCC::FOURCC_I420;
    case fuchsia::images2::PixelFormat::YV12:
      return libyuv::FourCC::FOURCC_YV12;
    case fuchsia::images2::PixelFormat::NV12:
      return libyuv::FourCC::FOURCC_NV12;
    default:
      NOTREACHED();
  }
}

libyuv::RotationMode CameraOrientationToLibyuvRotation(
    fuchsia::camera3::Orientation orientation,
    bool* flip_y) {
  switch (orientation) {
    case fuchsia::camera3::Orientation::UP:
      *flip_y = false;
      return libyuv::RotationMode::kRotate0;

    case fuchsia::camera3::Orientation::DOWN:
      *flip_y = false;
      return libyuv::RotationMode::kRotate180;

    case fuchsia::camera3::Orientation::LEFT:
      *flip_y = false;
      return libyuv::RotationMode::kRotate270;

    case fuchsia::camera3::Orientation::RIGHT:
      *flip_y = false;
      return libyuv::RotationMode::kRotate90;

    case fuchsia::camera3::Orientation::UP_FLIPPED:
      *flip_y = true;
      return libyuv::RotationMode::kRotate180;

    case fuchsia::camera3::Orientation::DOWN_FLIPPED:
      *flip_y = true;
      return libyuv::RotationMode::kRotate0;

    case fuchsia::camera3::Orientation::LEFT_FLIPPED:
      *flip_y = true;
      return libyuv::RotationMode::kRotate90;

    case fuchsia::camera3::Orientation::RIGHT_FLIPPED:
      *flip_y = true;
      return libyuv::RotationMode::kRotate270;
  }
}

gfx::Size RotateSize(gfx::Size size, libyuv::RotationMode rotation) {
  switch (rotation) {
    case libyuv::RotationMode::kRotate0:
    case libyuv::RotationMode::kRotate180:
      return size;

    case libyuv::RotationMode::kRotate90:
    case libyuv::RotationMode::kRotate270:
      return gfx::Size(size.height(), size.width());
  }
}

}  // namespace

// static
VideoPixelFormat VideoCaptureDeviceFuchsia::GetConvertedPixelFormat(
    fuchsia::images2::PixelFormat format) {
  switch (format) {
    case fuchsia::images2::PixelFormat::I420:
    case fuchsia::images2::PixelFormat::YV12:
    case fuchsia::images2::PixelFormat::NV12:
      // Convert all YUV formats to I420 since consumers currently don't support
      // NV12 or YV12.
      return PIXEL_FORMAT_I420;

    default:
      LOG(ERROR) << "Camera uses unsupported pixel format "
                 << static_cast<int>(format);
      return PIXEL_FORMAT_UNKNOWN;
  }
}

// static
VideoPixelFormat VideoCaptureDeviceFuchsia::GetConvertedPixelFormat(
    fuchsia::sysmem::PixelFormatType format) {
  // All fuchsia.sysmem.PixelFormatType values are valid
  // fuchsia.images2.PixelFormat values with the same meaning, and this will
  // remain true because sysmem(1) won't be getting any new PixelFormatType
  // values.
  auto images2_pixel_format =
      static_cast<fuchsia::images2::PixelFormat>(fidl::ToUnderlying(format));
  return GetConvertedPixelFormat(images2_pixel_format);
}

bool VideoCaptureDeviceFuchsia::IsSupportedPixelFormat(
    fuchsia::images2::PixelFormat format) {
  return GetConvertedPixelFormat(format) != PIXEL_FORMAT_UNKNOWN;
}

VideoCaptureDeviceFuchsia::VideoCaptureDeviceFuchsia(
    fidl::InterfaceHandle<fuchsia::camera3::Device> device)
    : sysmem_allocator_("CrVideoCaptureDeviceFuchsia") {
  device_.Bind(std::move(device));
  device_.set_error_handler(
      fit::bind_member(this, &VideoCaptureDeviceFuchsia::OnDeviceError));
}

VideoCaptureDeviceFuchsia::~VideoCaptureDeviceFuchsia() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void VideoCaptureDeviceFuchsia::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<Client> client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(params.requested_format.pixel_format, PIXEL_FORMAT_I420);
  DCHECK(!client_);
  DCHECK(!stream_);

  client_ = std::move(client);

  if (!device_) {
    OnError(FROM_HERE, VideoCaptureError::kFuchsiaCameraDeviceDisconnected,
            "fuchsia.camera3.Device disconnected");
    return;
  }

  start_time_ = base::TimeTicks::Now();
  frames_received_ = 0;

  // TODO(crbug.com/40128395) Select stream_id based on requested resolution.
  device_->ConnectToStream(/*stream_id=*/0, stream_.NewRequest());
  stream_.set_error_handler(
      fit::bind_member(this, &VideoCaptureDeviceFuchsia::OnStreamError));

  WatchResolution();
  WatchOrientation();

  // Call SetBufferCollection() with a new buffer collection token to indicate
  // that we are interested in buffer collection negotiation. The collection
  // token will be returned back from WatchBufferCollection(). After that it
  // will be initialized in InitializeBufferCollection().
  stream_->SetBufferCollection(fuchsia::sysmem::BufferCollectionTokenHandle(
      sysmem_allocator_.CreateNewToken().Unbind().TakeChannel()));
  WatchBufferCollection();
}

void VideoCaptureDeviceFuchsia::StopAndDeAllocate() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DisconnectStream();
  client_.reset();
}

void VideoCaptureDeviceFuchsia::OnDeviceError(zx_status_t status) {
  OnError(FROM_HERE, VideoCaptureError::kFuchsiaCameraDeviceDisconnected,
          base::StringPrintf("fuchsia.camera3.Device disconnected: %s (%d)",
                             zx_status_get_string(status), status));
}

void VideoCaptureDeviceFuchsia::OnStreamError(zx_status_t status) {
  OnError(FROM_HERE, VideoCaptureError::kFuchsiaCameraStreamDisconnected,
          base::StringPrintf("fuchsia.camera3.Stream disconnected: %s (%d)",
                             zx_status_get_string(status), status));
}

void VideoCaptureDeviceFuchsia::DisconnectStream() {
  stream_.Unbind();
  buffer_collection_.reset();
  buffers_.clear();
  frame_size_.reset();
}

void VideoCaptureDeviceFuchsia::OnError(base::Location location,
                                        VideoCaptureError error,
                                        const std::string& reason) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DisconnectStream();

  if (client_) {
    client_->OnError(error, location, reason);
  }
}

void VideoCaptureDeviceFuchsia::WatchResolution() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  stream_->WatchResolution(fit::bind_member(
      this, &VideoCaptureDeviceFuchsia::OnWatchResolutionResult));
}

void VideoCaptureDeviceFuchsia::OnWatchResolutionResult(
    fuchsia::math::Size frame_size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  frame_size_ = gfx::Size(frame_size.width, frame_size.height);

  WatchResolution();
}

void VideoCaptureDeviceFuchsia::WatchOrientation() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  stream_->WatchOrientation(fit::bind_member(
      this, &VideoCaptureDeviceFuchsia::OnWatchOrientationResult));
}

void VideoCaptureDeviceFuchsia::OnWatchOrientationResult(
    fuchsia::camera3::Orientation orientation) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  orientation_ = orientation;
  WatchOrientation();
}

void VideoCaptureDeviceFuchsia::WatchBufferCollection() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  stream_->WatchBufferCollection(
      [this](fidl::InterfaceHandle<fuchsia::sysmem::BufferCollectionToken>
                 token_handle) {
        InitializeBufferCollection(
            fuchsia::sysmem2::BufferCollectionTokenHandle(
                token_handle.TakeChannel()));
        WatchBufferCollection();
      });
}

void VideoCaptureDeviceFuchsia::InitializeBufferCollection(
    fidl::InterfaceHandle<fuchsia::sysmem2::BufferCollectionToken>
        token_handle) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Drop old buffers.
  buffer_collection_.reset();
  buffers_.clear();

  // Initialize the new collection.
  fuchsia::sysmem2::BufferCollectionTokenPtr token;
  token.Bind(std::move(token_handle));

  // Request just one buffer in collection constraints: each frame is copied as
  // soon as it's received.
  const size_t kMaxUsedOutputFrames = 1;

  // This is not an actual device driver, so the priority should be > 1. It's
  // also not a high-level system, so the name should be < 100.
  constexpr uint32_t kNamePriority = 10;

  // Sysmem calculates buffer size based on image constraints, so it doesn't
  // need to be specified explicitly.
  fuchsia::sysmem2::BufferCollectionConstraints constraints =
      VmoBuffer::GetRecommendedConstraints(kMaxUsedOutputFrames,
                                           /*min_buffer_size=*/std::nullopt,
                                           /*writable=*/false);
  buffer_collection_ = sysmem_allocator_.BindSharedCollection(std::move(token));
  buffer_collection_->Initialize(std::move(constraints), "CrVideoCaptureDevice",
                                 kNamePriority);
  buffer_collection_->AcquireBuffers(base::BindOnce(
      &VideoCaptureDeviceFuchsia::OnBuffersAcquired, base::Unretained(this)));
}

void VideoCaptureDeviceFuchsia::OnBuffersAcquired(
    std::vector<VmoBuffer> buffers,
    const fuchsia::sysmem2::SingleBufferSettings& buffer_settings) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Buffer collection allocation has failed. This case is not treated as an
  // error because the camera may create a new collection.
  if (buffers.empty()) {
    buffer_collection_.reset();
    return;
  }

  buffers_ = std::move(buffers);

  if (!buffer_settings.has_image_format_constraints()) {
    OnError(FROM_HERE, VideoCaptureError::kFuchsiaSysmemDidNotSetImageFormat,
            "Sysmem created buffer without image format constraints");
    return;
  }

  auto pixel_format = buffer_settings.image_format_constraints().pixel_format();
  if (!IsSupportedPixelFormat(pixel_format)) {
    OnError(FROM_HERE, VideoCaptureError::kFuchsiaUnsupportedPixelFormat,
            base::StringPrintf("Unsupported video frame format: %d",
                               static_cast<int>(pixel_format)));
    return;
  }

  buffers_format_ = fidl::Clone(buffer_settings.image_format_constraints());

  if (!started_) {
    started_ = true;
    client_->OnStarted();
    ReceiveNextFrame();
  }
}

void VideoCaptureDeviceFuchsia::ReceiveNextFrame() {
  stream_->GetNextFrame([this](fuchsia::camera3::FrameInfo frame_info) {
    ProcessNewFrame(std::move(frame_info));
    ReceiveNextFrame();
  });
}

void VideoCaptureDeviceFuchsia::ProcessNewFrame(
    fuchsia::camera3::FrameInfo frame_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(client_);

  if (buffers_.empty()) {
    DLOG(WARNING) << "Dropping frame received before sysmem collection has "
                     "been initialized.";
    return;
  }

  size_t index = frame_info.buffer_index;
  if (index >= buffers_.size()) {
    OnError(FROM_HERE, VideoCaptureError::kFuchsiaSysmemInvalidBufferIndex,
            base::StringPrintf("Received frame with invalid buffer_index=%zu",
                               index));
    return;
  }

  // Calculate coded frame dimensions for the buffer collection based on the
  // sysmem collection constraints. This logic should match
  // LogicalBufferCollection::Allocate() in sysmem.
  size_t src_coded_width =
      RoundUp(std::max(buffers_format_.min_size().width,
                       buffers_format_.required_max_size().width),
              buffers_format_.size_alignment().width);
  size_t src_coded_height =
      RoundUp(std::max(buffers_format_.min_size().height,
                       buffers_format_.required_max_size().height),
              buffers_format_.size_alignment().height);
  size_t src_stride =
      RoundUp(std::max(static_cast<size_t>(buffers_format_.min_bytes_per_row()),
                       src_coded_width),
              buffers_format_.bytes_per_row_divisor());
  gfx::Size visible_size =
      frame_size_.value_or(gfx::Size(src_coded_width, src_coded_height));
  gfx::Size nonrotated_output_size((visible_size.width() + 1) & ~1,
                                   (visible_size.height() + 1) & ~1);

  bool flip_y;
  libyuv::RotationMode rotation =
      CameraOrientationToLibyuvRotation(orientation_, &flip_y);

  gfx::Size output_size = RotateSize(nonrotated_output_size, rotation);
  visible_size = RotateSize(visible_size, rotation);

  base::TimeTicks reference_time =
      base::TimeTicks::FromZxTime(frame_info.timestamp);
  base::TimeDelta timestamp =
      std::max(reference_time - start_time_, base::TimeDelta());

  ++frames_received_;
  float frame_rate =
      (timestamp.is_positive())
          ? static_cast<float>(frames_received_) / timestamp.InSecondsF()
          : 0.0;
  VideoCaptureFormat capture_format(output_size, frame_rate, PIXEL_FORMAT_I420);

  Client::Buffer buffer;
  Client::ReserveResult result = client_->ReserveOutputBuffer(
      capture_format.frame_size, capture_format.pixel_format,
      /*frame_feedback_id=*/0, &buffer, /*require_new_buffer_id=*/nullptr,
      /*retire_old_buffer_id=*/nullptr);
  if (result != Client::ReserveResult::kSucceeded) {
    DLOG(WARNING) << "Failed to allocate output buffer for a video frame";
    return;
  }

  auto src_span = buffers_[index].GetMemory();
  if (src_span.empty()) {
    OnError(FROM_HERE, VideoCaptureError::kFuchsiaFailedToMapSysmemBuffer,
            "Failed to map buffers allocated by sysmem");
    return;
  }

  // For all supported formats (I420, NV12 and YV12) the U and V channels are
  // subsampled at 2x in both directions, so together they occupy half of the
  // space needed for the Y plane and the total buffer size is 3/2 of the Y
  // plane size.
  size_t src_buffer_size = src_coded_height * src_stride * 3 / 2;
  if (src_span.size() < src_buffer_size) {
    OnError(FROM_HERE, VideoCaptureError::kFuchsiaSysmemInvalidBufferSize,
            "Sysmem allocated buffer that's smaller than expected");
    return;
  }

  std::unique_ptr<VideoCaptureBufferHandle> output_handle =
      buffer.handle_provider->GetHandleForInProcessAccess();

  // Calculate offsets and strides for the output buffer.
  uint8_t* dst_y = output_handle->data();
  int dst_stride_y = output_size.width();
  size_t dst_y_plane_size = output_size.width() * output_size.height();
  uint8_t* dst_u = dst_y + dst_y_plane_size;
  int dst_stride_u = output_size.width() / 2;
  uint8_t* dst_v = dst_u + dst_y_plane_size / 4;
  int dst_stride_v = output_size.width() / 2;

  // Check that the output fits in the buffer.
  const uint8_t* dst_end = dst_v + dst_y_plane_size / 4;
  CHECK_LE(dst_end, output_handle->data() + output_handle->mapped_size());

  // Vertical flip is indicated to ConvertToI420() by negating src_height.
  int flipped_src_height = static_cast<int>(src_coded_height);
  if (flip_y)
    flipped_src_height = -flipped_src_height;

  auto four_cc = GetFourccForPixelFormat(buffers_format_.pixel_format());

  libyuv::ConvertToI420(src_span.data(), src_span.size(), dst_y, dst_stride_y,
                        dst_u, dst_stride_u, dst_v, dst_stride_v,
                        /*crop_x=*/0, /*crop_y=*/0, src_stride,
                        flipped_src_height, nonrotated_output_size.width(),
                        nonrotated_output_size.height(), rotation, four_cc);

  client_->OnIncomingCapturedBufferExt(
      std::move(buffer), capture_format, gfx::ColorSpace(), reference_time,
      timestamp, std::nullopt, gfx::Rect(visible_size), VideoFrameMetadata());

  // Frame buffer is returned to the device by dropping the |frame_info|.
}

}  // namespace media
