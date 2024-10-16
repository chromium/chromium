// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/capture/video/chromeos/stream_buffer_manager.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/posix/safe_strerror.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "media/capture/video/chromeos/camera_buffer_factory.h"
#include "media/capture/video/chromeos/camera_metadata_utils.h"
#include "media/capture/video/chromeos/pixel_format_utils.h"
#include "media/capture/video/chromeos/request_builder.h"
#include "media/capture/video/chromeos/request_manager.h"
#include "media/capture/video/video_capture_buffer_pool.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/libyuv/include/libyuv.h"

namespace media {

StreamBufferManager::StreamBufferManager(
    CameraDeviceContext* device_context,
    bool video_capture_use_gmb,
    std::unique_ptr<CameraBufferFactory> camera_buffer_factory,
    std::unique_ptr<VideoCaptureBufferObserver> buffer_observer)
    : device_context_(device_context),
      buffer_observer_(std::move(buffer_observer)),
      video_capture_use_gmb_(video_capture_use_gmb),
      camera_buffer_factory_(std::move(camera_buffer_factory)) {}

StreamBufferManager::~StreamBufferManager() {
  DestroyCurrentStreamsAndBuffers();
}

void StreamBufferManager::ReserveBuffer(StreamType stream_type) {
  if (CanReserveBufferFromPool(stream_type)) {
    ReserveBufferFromPool(stream_type);
  } else {
    ReserveBufferFromFactory(stream_type);
  }
}

scoped_refptr<gpu::ClientSharedImage> StreamBufferManager::GetSharedImageById(
    StreamType stream_type,
    uint64_t buffer_ipc_id) {
  auto& stream_context = stream_context_[stream_type];
  int key = GetBufferKey(buffer_ipc_id);
  auto it = stream_context->buffers.find(key);
  if (it == stream_context->buffers.end()) {
    LOG(ERROR) << "Invalid buffer: " << key << " for stream: " << stream_type;
    return nullptr;
  }
  return it->second.shared_image;
}

std::optional<StreamBufferManager::Buffer>
StreamBufferManager::AcquireBufferForClientById(StreamType stream_type,
                                                uint64_t buffer_ipc_id,
                                                VideoCaptureFormat* format) {
  DCHECK(stream_context_.count(stream_type));
  auto& stream_context = stream_context_[stream_type];
  auto it = stream_context->buffers.find(GetBufferKey(buffer_ipc_id));
  if (it == stream_context->buffers.end()) {
    LOG(ERROR) << "Invalid buffer: " << buffer_ipc_id
               << " for stream: " << stream_type;
    return std::nullopt;
  }
  auto buffer_pair = std::move(it->second);
  stream_context->buffers.erase(it);
  *format = GetStreamCaptureFormat(stream_type);
  // We only support NV12 at the moment.
  DCHECK_EQ(format->pixel_format, PIXEL_FORMAT_NV12);

  int rotation = device_context_->GetCameraFrameRotation();
  if (rotation == 0 ||
      !device_context_->IsCameraFrameRotationEnabledAtSource()) {
    return std::move(buffer_pair.vcd_buffer);
  }

  if (rotation == 90 || rotation == 270) {
    format->frame_size =
        gfx::Size(format->frame_size.height(), format->frame_size.width());
  }

  const std::optional<gfx::BufferFormat> gfx_format =
      PixFormatVideoToGfx(format->pixel_format);
  DCHECK(gfx_format);
  const auto& original_shared_image = buffer_pair.shared_image;
  std::unique_ptr<gpu::ClientSharedImage::ScopedMapping>
      original_scoped_mapping;
  if (original_shared_image) {
    original_scoped_mapping = original_shared_image->Map();
  }
  if (!original_scoped_mapping) {
    LOG(WARNING) << "Failed to map original shared image.";
    return std::move(buffer_pair.vcd_buffer);
  }

  const size_t original_width = stream_context->buffer_dimension.width();
  const size_t original_height = stream_context->buffer_dimension.height();
  const size_t temp_uv_width = (original_width + 1) / 2;
  const size_t temp_uv_height = (original_height + 1) / 2;
  const size_t temp_uv_size = temp_uv_width * temp_uv_height;
  std::vector<uint8_t> temp_uv_buffer(temp_uv_size * 2);
  uint8_t* temp_u = temp_uv_buffer.data();
  uint8_t* temp_v = temp_u + temp_uv_size;

  // libyuv currently provides only NV12ToI420Rotate. We achieve NV12 rotation
  // by NV12ToI420Rotate then merge the I420 U and V planes into the final NV12
  // UV plane.
  auto translate_rotation = [](const int rotation) -> libyuv::RotationModeEnum {
    switch (rotation) {
      case 0:
        return libyuv::kRotate0;
      case 90:
        return libyuv::kRotate90;
      case 180:
        return libyuv::kRotate180;
      case 270:
        return libyuv::kRotate270;
    }
    return libyuv::kRotate0;
  };

  if (rotation == 180) {
    // We can reuse the original buffer in this case because the size is same.
    // Note that libyuv can in-place rotate the Y-plane by 180 degrees.
    libyuv::NV12ToI420Rotate(
        original_scoped_mapping->GetMemoryForPlane(0).data(),
        original_scoped_mapping->Stride(0),
        original_scoped_mapping->GetMemoryForPlane(1).data(),
        original_scoped_mapping->Stride(1),
        original_scoped_mapping->GetMemoryForPlane(0).data(),
        original_scoped_mapping->Stride(0), temp_u, temp_uv_width, temp_v,
        temp_uv_width, original_width, original_height,
        translate_rotation(rotation));
    libyuv::MergeUVPlane(temp_u, temp_uv_width, temp_v, temp_uv_width,
                         original_scoped_mapping->GetMemoryForPlane(1).data(),
                         original_scoped_mapping->Stride(1), temp_uv_width,
                         temp_uv_height);
    return std::move(buffer_pair.vcd_buffer);
  }

  // We have to reserve a new buffer because the size is different.
  Buffer rotated_buffer;
  auto client_type = kStreamClientTypeMap[static_cast<int>(stream_type)];
  if (!device_context_->ReserveVideoCaptureBufferFromPool(
          client_type, format->frame_size, format->pixel_format,
          &rotated_buffer)) {
    DLOG(WARNING) << "Failed to reserve video capture buffer";
    return std::move(buffer_pair.vcd_buffer);
  }

  // TODO(crbug.com/359601431): Get shared image directly from the
  // |rotated_buffer->handle_provider| once all VideoCaptureBufferTracker are
  // converted to create MappableSI.
  auto rotated_shared_image =
      camera_buffer_factory_->CreateSharedImageFromGmbHandle(
          rotated_buffer.handle_provider->GetGpuMemoryBufferHandle(),
          format->frame_size, *gfx_format, stream_context->buffer_usage);
  std::unique_ptr<gpu::ClientSharedImage::ScopedMapping> rotated_scoped_mapping;
  if (rotated_shared_image) {
    rotated_scoped_mapping = rotated_shared_image->Map();
  }
  if (!rotated_scoped_mapping) {
    LOG(WARNING) << "Failed to map rotated shared image.";
    return std::move(buffer_pair.vcd_buffer);
  }

  libyuv::NV12ToI420Rotate(original_scoped_mapping->GetMemoryForPlane(0).data(),
                           original_scoped_mapping->Stride(0),
                           original_scoped_mapping->GetMemoryForPlane(1).data(),
                           original_scoped_mapping->Stride(1),
                           rotated_scoped_mapping->GetMemoryForPlane(0).data(),
                           rotated_scoped_mapping->Stride(0), temp_u,
                           temp_uv_height, temp_v, temp_uv_height,
                           original_width, original_height,
                           translate_rotation(rotation));
  libyuv::MergeUVPlane(temp_u, temp_uv_height, temp_v, temp_uv_height,
                       rotated_scoped_mapping->GetMemoryForPlane(1).data(),
                       rotated_scoped_mapping->Stride(1), temp_uv_height,
                       temp_uv_width);
  return std::move(rotated_buffer);
}

VideoCaptureFormat StreamBufferManager::GetStreamCaptureFormat(
    StreamType stream_type) {
  return stream_context_[stream_type]->capture_format;
}

bool StreamBufferManager::HasFreeBuffers(
    const std::set<StreamType>& stream_types) {
  for (auto stream_type : stream_types) {
    if (stream_context_[stream_type]->free_buffers.empty()) {
      return false;
    }
  }
  return true;
}

size_t StreamBufferManager::GetFreeBufferCount(StreamType stream_type) {
  return stream_context_[stream_type]->free_buffers.size();
}

bool StreamBufferManager::HasStreamsConfigured(
    std::initializer_list<StreamType> stream_types) {
  for (auto stream_type : stream_types) {
    if (stream_context_.find(stream_type) == stream_context_.end()) {
      return false;
    }
  }
  return true;
}

void StreamBufferManager::SetUpStreamsAndBuffers(
    base::flat_map<ClientType, VideoCaptureParams> capture_params,
    const cros::mojom::CameraMetadataPtr& static_metadata,
    std::vector<cros::mojom::Camera3StreamPtr> streams) {
  DestroyCurrentStreamsAndBuffers();

  for (auto& stream : streams) {
    DVLOG(2) << "Stream " << stream->id
             << " stream_type: " << stream->stream_type
             << " configured: usage=" << stream->usage
             << " max_buffers=" << stream->max_buffers;

    const size_t kMaximumAllowedBuffers = 15;
    if (stream->max_buffers > kMaximumAllowedBuffers) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerHalRequestedTooManyBuffers,
          FROM_HERE,
          std::string("Camera HAL requested ") +
              base::NumberToString(stream->max_buffers) +
              std::string(" buffers which exceeds the allowed maximum "
                          "number of buffers"));
      return;
    }

    // A better way to tell the stream type here would be to check on the usage
    // flags of the stream.
    StreamType stream_type = StreamIdToStreamType(stream->id);
    auto stream_context = std::make_unique<StreamContext>();
    auto client_type = kStreamClientTypeMap[static_cast<int>(stream_type)];
    stream_context->capture_format =
        capture_params[client_type].requested_format;
    stream_context->stream = std::move(stream);

    switch (stream_type) {
      case StreamType::kPreviewOutput:
      case StreamType::kRecordingOutput: {
        stream_context->buffer_dimension = gfx::Size(
            stream_context->stream->width, stream_context->stream->height);
        stream_context->buffer_usage =
            gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE;
        break;
      }
      case StreamType::kPortraitJpegOutput:
      case StreamType::kJpegOutput: {
        auto jpeg_size = GetMetadataEntryAsSpan<int32_t>(
            static_metadata,
            cros::mojom::CameraMetadataTag::ANDROID_JPEG_MAX_SIZE);
        CHECK_EQ(jpeg_size.size(), 1u);
        stream_context->buffer_dimension = gfx::Size(jpeg_size[0], 1);
        stream_context->buffer_usage =
            gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE;
        break;
      }
      default: {
        NOTREACHED_IN_MIGRATION();
      }
    }
    const ChromiumPixelFormat stream_format =
        camera_buffer_factory_->ResolveStreamBufferFormat(
            stream_context->stream->format, stream_context->buffer_usage);
    // Internally we keep track of the VideoPixelFormat that's actually
    // supported by the camera instead of the one requested by the client.
    stream_context->capture_format.pixel_format = stream_format.video_format;

    stream_context_[stream_type] = std::move(stream_context);

    // Allocate buffers.
    for (size_t j = 0; j < stream_context_[stream_type]->stream->max_buffers;
         ++j) {
      ReserveBuffer(stream_type);
    }
    DVLOG(2) << "Allocated "
             << stream_context_[stream_type]->stream->max_buffers << " buffers";

    if (stream_context_[stream_type]->free_buffers.size() !=
        stream_context_[stream_type]->stream->max_buffers) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerFailedToReserveBuffers,
          FROM_HERE,
          StreamTypeToString(stream_type) +
              base::StringPrintf(
                  " needs %d buffers but only allocated %zd",
                  stream_context_[stream_type]->stream->max_buffers,
                  stream_context_[stream_type]->free_buffers.size()));
      return;
    }
  }
}

cros::mojom::Camera3StreamPtr StreamBufferManager::GetStreamConfiguration(
    StreamType stream_type) {
  if (!stream_context_.count(stream_type)) {
    return cros::mojom::Camera3Stream::New();
  }
  return stream_context_[stream_type]->stream.Clone();
}

std::optional<BufferInfo> StreamBufferManager::RequestBufferForCaptureRequest(
    StreamType stream_type) {
  VideoPixelFormat buffer_format =
      stream_context_[stream_type]->capture_format.pixel_format;
  uint32_t drm_format = PixFormatVideoToDrm(buffer_format);
  if (!drm_format) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3BufferManagerUnsupportedVideoPixelFormat,
        FROM_HERE,
        std::string("Unsupported video pixel format") +
            VideoPixelFormatToString(buffer_format));
    return {};
  }

  BufferInfo buffer_info;
  const auto& stream_context = stream_context_[stream_type];
  CHECK(!stream_context->free_buffers.empty());
  int key = stream_context->free_buffers.front();
  auto it = stream_context->buffers.find(key);
  CHECK(it != stream_context->buffers.end());
  stream_context->free_buffers.pop();
  buffer_info.ipc_id = GetBufferIpcId(stream_type, key);
  buffer_info.dimension = stream_context->buffer_dimension;
  buffer_info.gpu_memory_buffer_handle =
      it->second.shared_image->CloneGpuMemoryBufferHandle();
  buffer_info.drm_format = drm_format;
  buffer_info.hal_pixel_format = stream_context_[stream_type]->stream->format;
  buffer_info.modifier =
      buffer_info.gpu_memory_buffer_handle.native_pixmap_handle.modifier;
  return buffer_info;
}

void StreamBufferManager::ReleaseBufferFromCaptureResult(
    StreamType stream_type,
    uint64_t buffer_ipc_id) {
  stream_context_[stream_type]->free_buffers.push(GetBufferKey(buffer_ipc_id));
}

gfx::Size StreamBufferManager::GetBufferDimension(StreamType stream_type) {
  DCHECK(stream_context_.count(stream_type));
  return stream_context_[stream_type]->buffer_dimension;
}

bool StreamBufferManager::IsPortraitModeSupported() {
  return stream_context_.find(StreamType::kPortraitJpegOutput) !=
         stream_context_.end();
}

bool StreamBufferManager::IsRecordingSupported() {
  return stream_context_.find(StreamType::kRecordingOutput) !=
         stream_context_.end();
}

scoped_refptr<gpu::ClientSharedImage>
StreamBufferManager::CreateSharedImageFromGmbHandle(
    gfx::GpuMemoryBufferHandle handle,
    const VideoCaptureFormat& format,
    gfx::BufferUsage buffer_usage) {
  std::optional<gfx::BufferFormat> buffer_format =
      PixFormatVideoToGfx(format.pixel_format);
  CHECK(buffer_format);
  auto shared_image = camera_buffer_factory_->CreateSharedImageFromGmbHandle(
      std::move(handle), format.frame_size, *buffer_format, buffer_usage);
  if (!shared_image) {
    LOG(ERROR) << "Failed to create mappable shared image.";
  }
  return shared_image;
}

// static
uint64_t StreamBufferManager::GetBufferIpcId(StreamType stream_type, int key) {
  uint64_t id = 0;
  id |= static_cast<uint64_t>(stream_type) << 32;
  DCHECK_GE(key, 0);
  id |= static_cast<uint32_t>(key);
  return id;
}

// static
int StreamBufferManager::GetBufferKey(uint64_t buffer_ipc_id) {
  return buffer_ipc_id & 0xFFFFFFFF;
}

bool StreamBufferManager::CanReserveBufferFromPool(StreamType stream_type) {
  return video_capture_use_gmb_;
}

void StreamBufferManager::ReserveBufferFromFactory(StreamType stream_type) {
  auto& stream_context = stream_context_[stream_type];
  std::optional<gfx::BufferFormat> gfx_format =
      PixFormatVideoToGfx(stream_context->capture_format.pixel_format);
  if (!gfx_format) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3BufferManagerFailedToCreateMappableSI,
        FROM_HERE, "Unsupported video pixel format.");
    return;
  }
  auto shared_image = camera_buffer_factory_->CreateSharedImage(
      stream_context->buffer_dimension, *gfx_format,
      stream_context->buffer_usage);
  if (!shared_image) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3BufferManagerFailedToCreateMappableSI,
        FROM_HERE, "Failed to create mappable shared image.");
    return;
  }

  // All the shared_images are allocated from the factory in bulk when the
  // streams are configured.  Here we simply use the sequence of the allocated
  // buffer as the buffer id.
  int key = stream_context->buffers.size() + 1;
  stream_context->free_buffers.push(key);
  stream_context->buffers.emplace(
      key, BufferPair(std::move(shared_image), std::nullopt));
}

void StreamBufferManager::ReserveBufferFromPool(StreamType stream_type) {
  auto& stream_context = stream_context_[stream_type];
  std::optional<gfx::BufferFormat> gfx_format =
      PixFormatVideoToGfx(stream_context->capture_format.pixel_format);
  if (!gfx_format) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3BufferManagerFailedToCreateMappableSI,
        FROM_HERE, "Unsupported video pixel format.");
    return;
  }
  Buffer vcd_buffer;
  auto client_type = kStreamClientTypeMap[static_cast<int>(stream_type)];
  int require_new_buffer_id = VideoCaptureBufferPool::kInvalidId;
  int retire_old_buffer_id = VideoCaptureBufferPool::kInvalidId;
  if (!device_context_->ReserveVideoCaptureBufferFromPool(
          client_type, stream_context->buffer_dimension,
          stream_context->capture_format.pixel_format, &vcd_buffer,
          &require_new_buffer_id, &retire_old_buffer_id)) {
    DLOG(WARNING) << "Failed to reserve video capture buffer.";
    return;
  }
  // TODO(b/333813928): This is a temporary solution to fix the cros camera
  // service crash until we figure out the crash root cause.
  const bool kEnableBufferSynchronizationWithCameraService = false;
  if (kEnableBufferSynchronizationWithCameraService &&
      retire_old_buffer_id != VideoCaptureBufferPool::kInvalidId) {
    buffer_observer_->OnBufferRetired(
        client_type, GetBufferIpcId(stream_type, retire_old_buffer_id));
  }

  // TODO(crbug.com/359601431): Get shared image directly from the
  // |vcd_buffer.handle_provider| once all VideoCaptureBufferTracker are
  // converted to create MappableSI.
  auto shared_image = camera_buffer_factory_->CreateSharedImageFromGmbHandle(
      vcd_buffer.handle_provider->GetGpuMemoryBufferHandle(),
      stream_context->buffer_dimension, *gfx_format,
      stream_context->buffer_usage);
  if (!shared_image) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3BufferManagerFailedToCreateMappableSI,
        FROM_HERE, "Failed to create mappable shared image.");
    return;
  }

  if (kEnableBufferSynchronizationWithCameraService &&
      require_new_buffer_id != VideoCaptureBufferPool::kInvalidId) {
    gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle =
        shared_image->CloneGpuMemoryBufferHandle();
    gfx::NativePixmapHandle& native_pixmap_handle =
        gpu_memory_buffer_handle.native_pixmap_handle;
    auto buffer_handle = cros::mojom::CameraBufferHandle::New();
    buffer_handle->buffer_id = GetBufferIpcId(stream_type, vcd_buffer.id);
    buffer_handle->drm_format =
        PixFormatVideoToDrm(stream_context->capture_format.pixel_format);
    buffer_handle->hal_pixel_format = stream_context->stream->format;
    buffer_handle->has_modifier = true;
    buffer_handle->modifier = native_pixmap_handle.modifier;
    buffer_handle->width = stream_context->buffer_dimension.width();
    buffer_handle->height = stream_context->buffer_dimension.height();

    size_t num_planes = native_pixmap_handle.planes.size();
    std::vector<StreamCaptureInterface::Plane> planes(num_planes);
    for (size_t i = 0; i < num_planes; ++i) {
      mojo::ScopedHandle mojo_fd = mojo::WrapPlatformHandle(
          mojo::PlatformHandle(std::move(native_pixmap_handle.planes[i].fd)));
      if (!mojo_fd.is_valid()) {
        device_context_->SetErrorState(
            media::VideoCaptureError::
                kCrosHalV3BufferManagerFailedToWrapGpuMemoryHandle,
            FROM_HERE, "Failed to wrap gpu memory handle");
      }
      buffer_handle->fds.push_back(std::move(mojo_fd));
      buffer_handle->strides.push_back(native_pixmap_handle.planes[i].stride);
      buffer_handle->offsets.push_back(native_pixmap_handle.planes[i].offset);
    }
    buffer_observer_->OnNewBuffer(client_type, std::move(buffer_handle));
  }
  stream_context->free_buffers.push(vcd_buffer.id);
  const int id = vcd_buffer.id;
  stream_context->buffers.insert(std::make_pair(
      id, BufferPair(std::move(shared_image), std::move(vcd_buffer))));
}

void StreamBufferManager::DestroyCurrentStreamsAndBuffers() {
  stream_context_.clear();
}

StreamBufferManager::BufferPair::BufferPair(
    scoped_refptr<gpu::ClientSharedImage> input_shared_image,
    std::optional<Buffer> input_vcd_buffer)
    : shared_image(std::move(input_shared_image)),
      vcd_buffer(std::move(input_vcd_buffer)) {}

StreamBufferManager::BufferPair::BufferPair(
    StreamBufferManager::BufferPair&& other) {
  shared_image = std::move(other.shared_image);
  vcd_buffer = std::move(other.vcd_buffer);
}

StreamBufferManager::BufferPair::~BufferPair() = default;

StreamBufferManager::StreamContext::StreamContext() = default;

StreamBufferManager::StreamContext::~StreamContext() = default;

}  // namespace media
