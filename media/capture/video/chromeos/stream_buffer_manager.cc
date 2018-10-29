// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/stream_buffer_manager.h"

#include <sync/sync.h>
#include <memory>

#include "base/posix/safe_strerror.h"
#include "base/trace_event/trace_event.h"
#include "media/capture/video/chromeos/camera_buffer_factory.h"
#include "media/capture/video/chromeos/camera_device_context.h"
#include "media/capture/video/chromeos/camera_metadata_utils.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace media {

namespace {

size_t GetBufferIndex(uint64_t buffer_id) {
  return buffer_id & 0xFFFFFFFF;
}

StreamType StreamIdToStreamType(uint64_t stream_id) {
  switch (stream_id) {
    case 0:
      return StreamType::kPreview;
    case 1:
      return StreamType::kStillCapture;
    default:
      return StreamType::kUnknown;
  }
}

}  // namespace

StreamBufferManager::StreamBufferManager(
    cros::mojom::Camera3CallbackOpsRequest callback_ops_request,
    std::unique_ptr<StreamCaptureInterface> capture_interface,
    CameraDeviceContext* device_context,
    std::unique_ptr<CameraBufferFactory> camera_buffer_factory,
    base::RepeatingCallback<
        mojom::BlobPtr(const uint8_t* buffer,
                       const uint32_t bytesused,
                       const VideoCaptureFormat& capture_format,
                       int screen_rotation)> blobify_callback,
    scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner)
    : callback_ops_(this, std::move(callback_ops_request)),
      capture_interface_(std::move(capture_interface)),
      device_context_(device_context),
      camera_buffer_factory_(std::move(camera_buffer_factory)),
      blobify_callback_(std::move(blobify_callback)),
      ipc_task_runner_(std::move(ipc_task_runner)),
      capturing_(false),
      frame_number_(0),
      partial_result_count_(1),
      first_frame_shutter_time_(base::TimeTicks()),
      weak_ptr_factory_(this) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(callback_ops_.is_bound());
  DCHECK(device_context_);
}

StreamBufferManager::~StreamBufferManager() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  for (const auto& iter : stream_context_) {
    if (iter.second) {
      for (const auto& buf : iter.second->buffers) {
        if (buf) {
          buf->Unmap();
        }
      }
    }
  }
}

void StreamBufferManager::SetUpStreamsAndBuffers(
    VideoCaptureFormat capture_format,
    const cros::mojom::CameraMetadataPtr& static_metadata,
    std::vector<cros::mojom::Camera3StreamPtr> streams) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(!stream_context_[StreamType::kPreview]);

  // The partial result count metadata is optional; defaults to 1 in case it
  // is not set in the static metadata.
  const cros::mojom::CameraMetadataEntryPtr* partial_count = GetMetadataEntry(
      static_metadata,
      cros::mojom::CameraMetadataTag::ANDROID_REQUEST_PARTIAL_RESULT_COUNT);
  if (partial_count) {
    partial_result_count_ =
        *reinterpret_cast<int32_t*>((*partial_count)->data.data());
  }

  for (auto& stream : streams) {
    DVLOG(2) << "Stream " << stream->id
             << " configured: usage=" << stream->usage
             << " max_buffers=" << stream->max_buffers;

    const size_t kMaximumAllowedBuffers = 15;
    if (stream->max_buffers > kMaximumAllowedBuffers) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerHalRequestedTooManyBuffers,
          FROM_HERE,
          std::string("Camera HAL requested ") +
              std::to_string(stream->max_buffers) +
              std::string(" buffers which exceeds the allowed maximum "
                          "number of buffers"));
      return;
    }

    // A better way to tell the stream type here would be to check on the usage
    // flags of the stream.
    StreamType stream_type;
    if (stream->format ==
        cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YCbCr_420_888) {
      stream_type = StreamType::kPreview;
    } else {  // stream->format ==
              // cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_BLOB
      stream_type = StreamType::kStillCapture;
    }
    stream_context_[stream_type] = std::make_unique<StreamContext>();
    stream_context_[stream_type]->capture_format = capture_format;
    stream_context_[stream_type]->stream = std::move(stream);

    const ChromiumPixelFormat stream_format =
        camera_buffer_factory_->ResolveStreamBufferFormat(
            stream_context_[stream_type]->stream->format);
    stream_context_[stream_type]->capture_format.pixel_format =
        stream_format.video_format;

    // Allocate buffers.
    size_t num_buffers = stream_context_[stream_type]->stream->max_buffers;
    stream_context_[stream_type]->buffers.resize(num_buffers);
    int32_t buffer_width, buffer_height;
    if (stream_type == StreamType::kPreview) {
      buffer_width = stream_context_[stream_type]->stream->width;
      buffer_height = stream_context_[stream_type]->stream->height;
    } else {  // StreamType::kStillCapture
      const cros::mojom::CameraMetadataEntryPtr* jpeg_max_size =
          GetMetadataEntry(
              static_metadata,
              cros::mojom::CameraMetadataTag::ANDROID_JPEG_MAX_SIZE);
      buffer_width = *reinterpret_cast<int32_t*>((*jpeg_max_size)->data.data());
      buffer_height = 1;
    }
    for (size_t j = 0; j < num_buffers; ++j) {
      auto buffer = camera_buffer_factory_->CreateGpuMemoryBuffer(
          gfx::Size(buffer_width, buffer_height), stream_format.gfx_format);
      if (!buffer) {
        device_context_->SetErrorState(
            media::VideoCaptureError::
                kCrosHalV3BufferManagerFailedToCreateGpuMemoryBuffer,
            FROM_HERE, "Failed to create GpuMemoryBuffer");
        return;
      }
      bool ret = buffer->Map();
      if (!ret) {
        device_context_->SetErrorState(
            media::VideoCaptureError::
                kCrosHalV3BufferManagerFailedToMapGpuMemoryBuffer,
            FROM_HERE, "Failed to map GpuMemoryBuffer");
        return;
      }
      stream_context_[stream_type]->buffers[j] = std::move(buffer);
      stream_context_[stream_type]->free_buffers.push(
          GetBufferIpcId(stream_type, j));
    }
    DVLOG(2) << "Allocated "
             << stream_context_[stream_type]->stream->max_buffers << " buffers";
  }
}

void StreamBufferManager::StartPreview(
    cros::mojom::CameraMetadataPtr preview_settings) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(stream_context_[StreamType::kPreview]);
  DCHECK(repeating_request_settings_.is_null());

  capturing_ = true;
  repeating_request_settings_ = std::move(preview_settings);
  // We cannot use a loop to register all the free buffers in one shot here
  // because the camera HAL v3 API specifies that the client cannot call
  // ProcessCaptureRequest before the previous one returns.
  RegisterBuffer(StreamType::kPreview);
}

void StreamBufferManager::StopPreview(
    base::OnceCallback<void(int32_t)> callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  capturing_ = false;
  if (callback) {
    capture_interface_->Flush(std::move(callback));
  }
}

cros::mojom::Camera3StreamPtr StreamBufferManager::GetStreamConfiguration(
    StreamType stream_type) {
  if (!stream_context_.count(stream_type)) {
    return cros::mojom::Camera3Stream::New();
  }
  return stream_context_[stream_type]->stream.Clone();
}

void StreamBufferManager::TakePhoto(
    cros::mojom::CameraMetadataPtr settings,
    VideoCaptureDevice::TakePhotoCallback callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(stream_context_[StreamType::kStillCapture]);

  still_capture_callbacks_yet_to_be_processed_.push(std::move(callback));

  std::vector<uint8_t> frame_orientation(sizeof(int32_t));
  *reinterpret_cast<int32_t*>(frame_orientation.data()) =
      base::checked_cast<int32_t>(device_context_->GetCameraFrameOrientation());
  cros::mojom::CameraMetadataEntryPtr e =
      cros::mojom::CameraMetadataEntry::New();
  e->tag = cros::mojom::CameraMetadataTag::ANDROID_JPEG_ORIENTATION;
  e->type = cros::mojom::EntryType::TYPE_INT32;
  e->count = 1;
  e->data = std::move(frame_orientation);
  AddOrUpdateMetadataEntry(&settings, std::move(e));

  oneshot_request_settings_.push(std::move(settings));
  RegisterBuffer(StreamType::kStillCapture);
}

size_t StreamBufferManager::GetStreamNumber() {
  return stream_context_.size();
}

void StreamBufferManager::AddResultMetadataObserver(
    ResultMetadataObserver* observer) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(!result_metadata_observers_.count(observer));

  result_metadata_observers_.insert(observer);
}

void StreamBufferManager::RemoveResultMetadataObserver(
    ResultMetadataObserver* observer) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(result_metadata_observers_.count(observer));

  result_metadata_observers_.erase(observer);
}

void StreamBufferManager::SetCaptureMetadata(cros::mojom::CameraMetadataTag tag,
                                             cros::mojom::EntryType type,
                                             size_t count,
                                             std::vector<uint8_t> value) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  cros::mojom::CameraMetadataEntryPtr setting =
      cros::mojom::CameraMetadataEntry::New();

  setting->tag = tag;
  setting->type = type;
  setting->count = count;
  setting->data = std::move(value);

  capture_settings_override_.push_back(std::move(setting));
}

void StreamBufferManager::SetRepeatingCaptureMetadata(
    cros::mojom::CameraMetadataTag tag,
    cros::mojom::EntryType type,
    size_t count,
    std::vector<uint8_t> value) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  cros::mojom::CameraMetadataEntryPtr setting =
      cros::mojom::CameraMetadataEntry::New();

  setting->tag = tag;
  setting->type = type;
  setting->count = count;
  setting->data = std::move(value);

  capture_settings_repeating_override_[tag] = std::move(setting);
}

void StreamBufferManager::UnsetRepeatingCaptureMetadata(
    cros::mojom::CameraMetadataTag tag) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  auto it = capture_settings_repeating_override_.find(tag);
  if (it == capture_settings_repeating_override_.end()) {
    LOG(ERROR) << "Unset a non-existent metadata: " << tag;
    return;
  }
  capture_settings_repeating_override_.erase(it);
}

// static
uint64_t StreamBufferManager::GetBufferIpcId(StreamType stream_type,
                                             size_t index) {
  uint64_t id = 0;
  id |= static_cast<uint64_t>(stream_type) << 32;
  id |= index;
  return id;
}

void StreamBufferManager::ApplyCaptureSettings(
    cros::mojom::CameraMetadataPtr* capture_settings) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (capture_settings_override_.empty() &&
      capture_settings_repeating_override_.empty()) {
    return;
  }

  for (const auto& setting : capture_settings_repeating_override_) {
    AddOrUpdateMetadataEntry(capture_settings, setting.second.Clone());
  }

  for (auto& s : capture_settings_override_) {
    AddOrUpdateMetadataEntry(capture_settings, std::move(s));
  }
  capture_settings_override_.clear();
  SortCameraMetadata(capture_settings);
}

void StreamBufferManager::RegisterBuffer(StreamType stream_type) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(stream_context_[stream_type]);

  if (!capturing_) {
    return;
  }

  if (stream_context_[stream_type]->free_buffers.empty()) {
    return;
  }

  uint64_t buffer_id = stream_context_[stream_type]->free_buffers.front();
  stream_context_[stream_type]->free_buffers.pop();
  const gfx::GpuMemoryBuffer* buffer =
      stream_context_[stream_type]->buffers[GetBufferIndex(buffer_id)].get();

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
    return;
  }
  cros::mojom::HalPixelFormat hal_pixel_format =
      stream_context_[stream_type]->stream->format;

  gfx::NativePixmapHandle buffer_handle =
      buffer->CloneHandle().native_pixmap_handle;
  // Take ownership of FD at index 0.
  base::ScopedFD fd(buffer_handle.fds[0].fd);
  // There should be only one FD. Close all remaining FDs if there are any.
  DCHECK_EQ(buffer_handle.fds.size(), 1U);
  for (size_t i = 1; i < buffer_handle.fds.size(); ++i)
    base::ScopedFD scoped_fd(buffer_handle.fds[i].fd);

  size_t num_planes = buffer_handle.planes.size();
  std::vector<StreamCaptureInterface::Plane> planes(num_planes);
  for (size_t i = 0; i < num_planes; ++i) {
    int dup_fd = dup(fd.get());
    if (dup_fd == -1) {
      device_context_->SetErrorState(
          media::VideoCaptureError::kCrosHalV3BufferManagerFailedToDupFd,
          FROM_HERE, "Failed to dup fd");
      return;
    }
    planes[i].fd =
        mojo::WrapPlatformHandle(mojo::PlatformHandle(base::ScopedFD(dup_fd)));
    if (!planes[i].fd.is_valid()) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerFailedToWrapGpuMemoryHandle,
          FROM_HERE, "Failed to wrap gpu memory handle");
      return;
    }
    planes[i].stride = buffer_handle.planes[i].stride;
    planes[i].offset = buffer_handle.planes[i].offset;
  }
  if (stream_type == StreamType::kStillCapture) {
    still_capture_callbacks_currently_processing_.push(
        std::move(still_capture_callbacks_yet_to_be_processed_.front()));
    still_capture_callbacks_yet_to_be_processed_.pop();
  }
  // We reuse BufferType::GRALLOC here since on ARC++ we are using DMA-buf-based
  // gralloc buffers.
  capture_interface_->RegisterBuffer(
      buffer_id, cros::mojom::Camera3DeviceOps::BufferType::GRALLOC, drm_format,
      hal_pixel_format, buffer->GetSize().width(), buffer->GetSize().height(),
      std::move(planes),
      base::BindOnce(&StreamBufferManager::OnRegisteredBuffer,
                     weak_ptr_factory_.GetWeakPtr(), stream_type, buffer_id));
  DVLOG(2) << "Registered buffer " << buffer_id;
}

void StreamBufferManager::OnRegisteredBuffer(StreamType stream_type,
                                             uint64_t buffer_id,
                                             int32_t result) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(stream_context_[stream_type]);

  if (!capturing_) {
    return;
  }
  if (result) {
    device_context_->SetErrorState(
        media::VideoCaptureError::kCrosHalV3BufferManagerFailedToRegisterBuffer,
        FROM_HERE,
        std::string("Failed to register buffer: ") +
            base::safe_strerror(-result));
    return;
  }
  stream_context_[stream_type]->registered_buffers.push(buffer_id);
  ProcessCaptureRequest();
}

void StreamBufferManager::ProcessCaptureRequest() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(stream_context_[StreamType::kPreview]);

  cros::mojom::Camera3CaptureRequestPtr request =
      cros::mojom::Camera3CaptureRequest::New();
  request->frame_number = frame_number_;

  CaptureResult& pending_result = pending_results_[frame_number_];

  if (!stream_context_[StreamType::kPreview]->registered_buffers.empty()) {
    cros::mojom::Camera3StreamBufferPtr buffer =
        cros::mojom::Camera3StreamBuffer::New();
    buffer->stream_id = static_cast<uint64_t>(StreamType::kPreview);
    buffer->buffer_id =
        stream_context_[StreamType::kPreview]->registered_buffers.front();
    stream_context_[StreamType::kPreview]->registered_buffers.pop();
    buffer->status = cros::mojom::Camera3BufferStatus::CAMERA3_BUFFER_STATUS_OK;

    DVLOG(2) << "Requested capture for stream " << StreamType::kPreview
             << " in frame " << frame_number_;
    request->settings = repeating_request_settings_.Clone();
    request->output_buffers.push_back(std::move(buffer));
  }

  if (stream_context_.count(StreamType::kStillCapture) &&
      !stream_context_[StreamType::kStillCapture]->registered_buffers.empty()) {
    DCHECK(!still_capture_callbacks_currently_processing_.empty());
    cros::mojom::Camera3StreamBufferPtr buffer =
        cros::mojom::Camera3StreamBuffer::New();
    buffer->stream_id = static_cast<uint64_t>(StreamType::kStillCapture);
    buffer->buffer_id =
        stream_context_[StreamType::kStillCapture]->registered_buffers.front();
    stream_context_[StreamType::kStillCapture]->registered_buffers.pop();
    buffer->status = cros::mojom::Camera3BufferStatus::CAMERA3_BUFFER_STATUS_OK;

    DVLOG(2) << "Requested capture for stream " << StreamType::kStillCapture
             << " in frame " << frame_number_;
    // Use the still capture settings and override the preview ones.
    request->settings = std::move(oneshot_request_settings_.front());
    oneshot_request_settings_.pop();
    pending_result.still_capture_callback =
        std::move(still_capture_callbacks_currently_processing_.front());
    still_capture_callbacks_currently_processing_.pop();
    request->output_buffers.push_back(std::move(buffer));
  }

  pending_result.unsubmitted_buffer_count = request->output_buffers.size();

  ApplyCaptureSettings(&request->settings);
  capture_interface_->ProcessCaptureRequest(
      std::move(request),
      base::BindOnce(&StreamBufferManager::OnProcessedCaptureRequest,
                     weak_ptr_factory_.GetWeakPtr()));
  frame_number_++;
}

void StreamBufferManager::OnProcessedCaptureRequest(int32_t result) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (!capturing_) {
    return;
  }
  if (result) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3BufferManagerProcessCaptureRequestFailed,
        FROM_HERE,
        std::string("Process capture request failed: ") +
            base::safe_strerror(-result));
    return;
  }
  // Keeps the preview stream going.
  RegisterBuffer(StreamType::kPreview);
}

void StreamBufferManager::ProcessCaptureResult(
    cros::mojom::Camera3CaptureResultPtr result) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (!capturing_) {
    return;
  }
  uint32_t frame_number = result->frame_number;
  // A new partial result may be created in either ProcessCaptureResult or
  // Notify.
  CaptureResult& pending_result = pending_results_[frame_number];

  // |result->pending_result| is set to 0 if the capture result contains only
  // the result buffer handles and no result metadata.
  if (result->partial_result) {
    uint32_t result_id = result->partial_result;
    if (result_id > partial_result_count_) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerInvalidPendingResultId,
          FROM_HERE,
          std::string("Invalid pending_result id: ") +
              std::to_string(result_id));
      return;
    }
    if (pending_result.partial_metadata_received.count(result_id)) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerReceivedDuplicatedPartialMetadata,
          FROM_HERE,
          std::string("Received duplicated partial metadata: ") +
              std::to_string(result_id));
      return;
    }
    DVLOG(2) << "Received partial result " << result_id << " for frame "
             << frame_number;
    pending_result.partial_metadata_received.insert(result_id);
    MergeMetadata(&pending_result.metadata, result->result);
  }

  if (result->output_buffers) {
    if (result->output_buffers->size() > kMaxConfiguredStreams) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerIncorrectNumberOfOutputBuffersReceived,
          FROM_HERE,
          std::string("Incorrect number of output buffers received: ") +
              std::to_string(result->output_buffers->size()));
      return;
    }
    for (auto& stream_buffer : result->output_buffers.value()) {
      DVLOG(2) << "Received capture result for frame " << frame_number
               << " stream_id: " << stream_buffer->stream_id;
      StreamType stream_type = StreamIdToStreamType(stream_buffer->stream_id);
      if (stream_type == StreamType::kUnknown) {
        device_context_->SetErrorState(
            media::VideoCaptureError::
                kCrosHalV3BufferManagerInvalidTypeOfOutputBuffersReceived,
            FROM_HERE,
            std::string("Invalid type of output buffers received: ") +
                std::to_string(stream_buffer->stream_id));
        return;
      }

      // The camera HAL v3 API specifies that only one capture result can carry
      // the result buffer for any given frame number.
      if (stream_context_[stream_type]->capture_results_with_buffer.count(
              frame_number)) {
        device_context_->SetErrorState(
            media::VideoCaptureError::
                kCrosHalV3BufferManagerReceivedMultipleResultBuffersForFrame,
            FROM_HERE,
            std::string("Received multiple result buffers for frame ") +
                std::to_string(frame_number) + std::string(" for stream ") +
                std::to_string(stream_buffer->stream_id));
        return;
      }

      pending_result.buffers[stream_type] = std::move(stream_buffer);
      stream_context_[stream_type]->capture_results_with_buffer[frame_number] =
          &pending_result;
      if (pending_result.buffers[stream_type]->status ==
          cros::mojom::Camera3BufferStatus::CAMERA3_BUFFER_STATUS_ERROR) {
        // If the buffer is marked as error, its content is discarded for this
        // frame.  Send the buffer to the free list directly through
        // SubmitCaptureResult.
        SubmitCaptureResult(frame_number, stream_type);
      }
    }
  }

  for (const auto& iter : stream_context_) {
    TRACE_EVENT1("camera", "Capture Result", "frame_number", frame_number);
    SubmitCaptureResultIfComplete(frame_number, iter.first);
  }
}

void StreamBufferManager::Notify(cros::mojom::Camera3NotifyMsgPtr message) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (!capturing_) {
    return;
  }
  if (message->type == cros::mojom::Camera3MsgType::CAMERA3_MSG_ERROR) {
    uint32_t frame_number = message->message->get_error()->frame_number;
    uint64_t error_stream_id = message->message->get_error()->error_stream_id;
    StreamType stream_type = StreamIdToStreamType(error_stream_id);
    if (stream_type == StreamType::kUnknown) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerUnknownStreamInCamera3NotifyMsg,
          FROM_HERE,
          std::string("Unknown stream in Camera3NotifyMsg: ") +
              std::to_string(error_stream_id));
      return;
    }
    cros::mojom::Camera3ErrorMsgCode error_code =
        message->message->get_error()->error_code;
    HandleNotifyError(frame_number, stream_type, error_code);
  } else {  // cros::mojom::Camera3MsgType::CAMERA3_MSG_SHUTTER
    uint32_t frame_number = message->message->get_shutter()->frame_number;
    uint64_t shutter_time = message->message->get_shutter()->timestamp;
    DVLOG(2) << "Received shutter time for frame " << frame_number;
    if (!shutter_time) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerReceivedInvalidShutterTime,
          FROM_HERE,
          std::string("Received invalid shutter time: ") +
              std::to_string(shutter_time));
      return;
    }
    CaptureResult& pending_result = pending_results_[frame_number];
    // Shutter timestamp is in ns.
    base::TimeTicks reference_time =
        base::TimeTicks::FromInternalValue(shutter_time / 1000);
    pending_result.reference_time = reference_time;
    if (first_frame_shutter_time_.is_null()) {
      // Record the shutter time of the first frame for calculating the
      // timestamp.
      first_frame_shutter_time_ = reference_time;
    }
    pending_result.timestamp = reference_time - first_frame_shutter_time_;
    for (const auto& iter : stream_context_) {
      SubmitCaptureResultIfComplete(frame_number, iter.first);
    }
  }
}

void StreamBufferManager::HandleNotifyError(
    uint32_t frame_number,
    StreamType stream_type,
    cros::mojom::Camera3ErrorMsgCode error_code) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  std::string warning_msg;

  switch (error_code) {
    case cros::mojom::Camera3ErrorMsgCode::CAMERA3_MSG_ERROR_DEVICE:
      // Fatal error and no more frames will be produced by the device.
      device_context_->SetErrorState(
          media::VideoCaptureError::kCrosHalV3BufferManagerFatalDeviceError,
          FROM_HERE, "Fatal device error");
      return;

    case cros::mojom::Camera3ErrorMsgCode::CAMERA3_MSG_ERROR_REQUEST:
      // An error has occurred in processing the request; the request
      // specified by |frame_number| has been dropped by the camera device.
      // Subsequent requests are unaffected.
      //
      // The HAL will call ProcessCaptureResult with the buffers' state set to
      // STATUS_ERROR.  The content of the buffers will be dropped and the
      // buffers will be reused in SubmitCaptureResult.
      warning_msg =
          std::string("An error occurred while processing request for frame ") +
          std::to_string(frame_number);
      break;

    case cros::mojom::Camera3ErrorMsgCode::CAMERA3_MSG_ERROR_RESULT:
      // An error has occurred in producing the output metadata buffer for a
      // result; the output metadata will not be available for the frame
      // specified by |frame_number|.  Subsequent requests are unaffected.
      warning_msg = std::string(
                        "An error occurred while producing result "
                        "metadata for frame ") +
                    std::to_string(frame_number);
      break;

    case cros::mojom::Camera3ErrorMsgCode::CAMERA3_MSG_ERROR_BUFFER:
      // An error has occurred in placing the output buffer into a stream for
      // a request. |frame_number| specifies the request for which the buffer
      // was dropped, and |stream_type| specifies the stream that dropped
      // the buffer.
      //
      // The HAL will call ProcessCaptureResult with the buffer's state set to
      // STATUS_ERROR.  The content of the buffer will be dropped and the
      // buffer will be reused in SubmitCaptureResult.
      warning_msg =
          std::string(
              "An error occurred while filling output buffer of stream ") +
          StreamTypeToString(stream_type) + std::string(" in frame ") +
          std::to_string(frame_number);
      break;

    default:
      // To eliminate the warning for not handling CAMERA3_MSG_NUM_ERRORS
      break;
  }

  LOG(WARNING) << warning_msg << stream_type;
  device_context_->LogToClient(warning_msg);
  // If the buffer is already returned by the HAL, submit it and we're done.
  if (pending_results_.count(frame_number) &&
      pending_results_[frame_number].buffers.count(stream_type)) {
    SubmitCaptureResult(frame_number, stream_type);
  }
}

void StreamBufferManager::SubmitCaptureResultIfComplete(
    uint32_t frame_number,
    StreamType stream_type) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (!pending_results_.count(frame_number)) {
    // The capture result may be discarded in case of error.
    return;
  }

  CaptureResult& pending_result = pending_results_[frame_number];
  if (!stream_context_[stream_type]->capture_results_with_buffer.count(
          frame_number) ||
      pending_result.partial_metadata_received.size() < partial_result_count_ ||
      pending_result.reference_time == base::TimeTicks()) {
    // We can only submit the result buffer of |frame_number| for |stream_type|
    // when:
    //   1. The result buffer for |stream_type| is received, and
    //   2. All the result metadata are received, and
    //   3. The shutter time is received.
    return;
  }
  SubmitCaptureResult(frame_number, stream_type);
}

void StreamBufferManager::SubmitCaptureResult(uint32_t frame_number,
                                              StreamType stream_type) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(pending_results_.count(frame_number));
  DCHECK(stream_context_[stream_type]->capture_results_with_buffer.count(
      frame_number));

  CaptureResult& pending_result =
      *stream_context_[stream_type]->capture_results_with_buffer[frame_number];
  if (stream_context_[stream_type]
          ->capture_results_with_buffer.begin()
          ->first != frame_number) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3BufferManagerReceivedFrameIsOutOfOrder,
        FROM_HERE,
        std::string("Received frame is out-of-order; expect ") +
            std::to_string(pending_results_.begin()->first) +
            std::string(" but got ") + std::to_string(frame_number));
    return;
  }

  DVLOG(2) << "Submit capture result of frame " << frame_number
           << " for stream " << static_cast<int>(stream_type);
  for (auto* iter : result_metadata_observers_) {
    iter->OnResultMetadataAvailable(pending_result.metadata);
  }

  DCHECK(pending_result.buffers[stream_type]);
  const cros::mojom::Camera3StreamBufferPtr& stream_buffer =
      pending_result.buffers[stream_type];
  uint64_t buffer_id = stream_buffer->buffer_id;

  // Wait on release fence before delivering the result buffer to client.
  if (stream_buffer->release_fence.is_valid()) {
    const int kSyncWaitTimeoutMs = 1000;
    mojo::PlatformHandle fence =
        mojo::UnwrapPlatformHandle(std::move(stream_buffer->release_fence));
    if (!fence.is_valid()) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerFailedToUnwrapReleaseFenceFd,
          FROM_HERE, "Failed to unwrap release fence fd");
      return;
    }
    if (!sync_wait(fence.GetFD().get(), kSyncWaitTimeoutMs)) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerSyncWaitOnReleaseFenceTimedOut,
          FROM_HERE, "Sync wait on release fence timed out");
      return;
    }
  }

  // Deliver the captured data to client.
  if (stream_buffer->status !=
      cros::mojom::Camera3BufferStatus::CAMERA3_BUFFER_STATUS_ERROR) {
    size_t buffer_index = GetBufferIndex(buffer_id);
    gfx::GpuMemoryBuffer* buffer =
        stream_context_[stream_type]->buffers[buffer_index].get();
    if (stream_type == StreamType::kPreview) {
      device_context_->SubmitCapturedData(
          buffer, stream_context_[StreamType::kPreview]->capture_format,
          pending_result.reference_time, pending_result.timestamp);
      ipc_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&StreamBufferManager::RegisterBuffer,
                         weak_ptr_factory_.GetWeakPtr(), StreamType::kPreview));
    } else {  // StreamType::kStillCapture
      DCHECK(pending_result.still_capture_callback);
      const Camera3JpegBlob* header = reinterpret_cast<Camera3JpegBlob*>(
          reinterpret_cast<uintptr_t>(buffer->memory(0)) +
          buffer->GetSize().width() - sizeof(Camera3JpegBlob));
      if (header->jpeg_blob_id != kCamera3JpegBlobId) {
        device_context_->SetErrorState(
            media::VideoCaptureError::kCrosHalV3BufferManagerInvalidJpegBlob,
            FROM_HERE, "Invalid JPEG blob");
        return;
      }
      // Still capture result from HALv3 already has orientation info in EXIF,
      // so just provide 0 as screen rotation in |blobify_callback_| parameters.
      mojom::BlobPtr blob = blobify_callback_.Run(
          reinterpret_cast<uint8_t*>(buffer->memory(0)), header->jpeg_size,
          stream_context_[stream_type]->capture_format, 0);
      if (blob) {
        std::move(pending_result.still_capture_callback).Run(std::move(blob));
      } else {
        LOG(ERROR) << "Failed to blobify the captured JPEG image";
      }
    }
  }

  stream_context_[stream_type]->free_buffers.push(buffer_id);
  stream_context_[stream_type]->capture_results_with_buffer.erase(frame_number);
  pending_result.unsubmitted_buffer_count--;
  if (!pending_result.unsubmitted_buffer_count) {
    pending_results_.erase(frame_number);
  }

  if (stream_type == StreamType::kPreview) {
    // Always keep the preview stream running.
    RegisterBuffer(StreamType::kPreview);
  } else {  // stream_type == StreamType::kStillCapture
    if (!still_capture_callbacks_yet_to_be_processed_.empty()) {
      RegisterBuffer(StreamType::kStillCapture);
    }
  }
}

StreamBufferManager::StreamContext::StreamContext() = default;

StreamBufferManager::StreamContext::~StreamContext() = default;

StreamBufferManager::CaptureResult::CaptureResult()
    : metadata(cros::mojom::CameraMetadata::New()),
      unsubmitted_buffer_count(0) {}

StreamBufferManager::CaptureResult::~CaptureResult() = default;

}  // namespace media
