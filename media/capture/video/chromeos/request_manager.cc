// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/capture/video/chromeos/request_manager.h"

#include <sync/sync.h>

#include <initializer_list>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/posix/safe_strerror.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/typed_macros.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"
#include "media/capture/video/chromeos/camera_app_device_bridge_impl.h"
#include "media/capture/video/chromeos/camera_buffer_factory.h"
#include "media/capture/video/chromeos/camera_metadata_utils.h"
#include "media/capture/video/chromeos/camera_trace_utils.h"
#include "media/capture/video/chromeos/video_capture_features_chromeos.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace media {

namespace {

constexpr uint32_t kUndefinedFrameNumber = 0xFFFFFFFF;

// Choose a JPEG thumbnail size for the JPEG output stream size from the
// JPEG_AVAILABLE_THUMBNAIL_SIZES static metadata. Note that [0, 0] indicates no
// thumbnail should be generated, and can be returned by this function if
// there's no non-zero JPEG thumbnail size available.
gfx::Size GetJpegThumbnailSize(
    const cros::mojom::CameraMetadataPtr& static_metadata,
    const std::vector<cros::mojom::Camera3StreamPtr>& streams) {
  gfx::Size jpeg_size;
  gfx::Size portrait_jpeg_size;
  for (auto& stream : streams) {
    const StreamType stream_type = StreamIdToStreamType(stream->id);
    if (stream_type == StreamType::kJpegOutput) {
      jpeg_size = gfx::Size(base::checked_cast<int>(stream->width),
                            base::checked_cast<int>(stream->height));
    }
    if (stream_type == StreamType::kPortraitJpegOutput) {
      portrait_jpeg_size = gfx::Size(base::checked_cast<int>(stream->width),
                                     base::checked_cast<int>(stream->height));
      // The sizes of the JPEG stream and portrait JPEG stream should be the
      // same.
      CHECK_EQ(jpeg_size, portrait_jpeg_size);
    }
  }
  if (jpeg_size.IsEmpty())
    return gfx::Size();

  const auto available_sizes = GetMetadataEntryAsSpan<int32_t>(
      static_metadata,
      cros::mojom::CameraMetadataTag::ANDROID_JPEG_AVAILABLE_THUMBNAIL_SIZES);
  DCHECK_EQ(available_sizes.size() % 2, 0u);

  // Choose the thumbnail size with the closest aspect ratio to the JPEG size.
  // If there are multiple options, choose the smallest one.
  constexpr int kPrecisionFactor = 1000;
  const int target_aspect_ratio =
      kPrecisionFactor * jpeg_size.width() / jpeg_size.height();
  std::vector<std::tuple<int, int, int>> items;
  for (size_t i = 0; i < available_sizes.size(); i += 2) {
    const gfx::Size size(base::strict_cast<int>(available_sizes[i]),
                         base::strict_cast<int>(available_sizes[i + 1]));
    if (size.IsEmpty())
      continue;
    const int aspect_ratio = kPrecisionFactor * size.width() / size.height();
    items.emplace_back(std::abs(aspect_ratio - target_aspect_ratio),
                       size.width(), size.height());
  }
  const auto iter = std::min_element(items.begin(), items.end());
  if (iter == items.end())
    return gfx::Size();
  return gfx::Size(std::get<1>(*iter), std::get<2>(*iter));
}

}  // namespace

VideoCaptureBufferObserver::VideoCaptureBufferObserver(
    base::WeakPtr<RequestManager> request_manager)
    : request_manager_(std::move(request_manager)) {}

VideoCaptureBufferObserver::~VideoCaptureBufferObserver() = default;

void VideoCaptureBufferObserver::OnNewBuffer(
    ClientType client_type,
    cros::mojom::CameraBufferHandlePtr buffer) {
  if (request_manager_) {
    request_manager_->OnNewBuffer(client_type, std::move(buffer));
  }
}

void VideoCaptureBufferObserver::OnBufferRetired(ClientType client_type,
                                                 uint64_t buffer_id) {
  if (request_manager_) {
    request_manager_->OnBufferRetired(client_type, buffer_id);
  }
}

RequestManager::RequestManager(
    const std::string& device_id,
    mojo::PendingReceiver<cros::mojom::Camera3CallbackOps>
        callback_ops_receiver,
    std::unique_ptr<StreamCaptureInterface> capture_interface,
    CameraDeviceContext* device_context,
    VideoCaptureBufferType buffer_type,
    std::unique_ptr<CameraBufferFactory> camera_buffer_factory,
    BlobifyCallback blobify_callback,
    scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner,
    uint32_t device_api_version,
    bool use_buffer_management_apis)
    : device_id_(device_id),
      callback_ops_(this, std::move(callback_ops_receiver)),
      capture_interface_(std::move(capture_interface)),
      device_context_(device_context),
      video_capture_use_gmb_(buffer_type ==
                             VideoCaptureBufferType::kGpuMemoryBuffer),
      blobify_callback_(std::move(blobify_callback)),
      ipc_task_runner_(std::move(ipc_task_runner)),
      capturing_(false),
      partial_result_count_(1),
      first_frame_shutter_time_(base::TimeTicks()),
      device_api_version_(device_api_version),
      use_buffer_management_apis_(use_buffer_management_apis) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(callback_ops_.is_bound());
  DCHECK(device_context_);
  stream_buffer_manager_ = std::make_unique<StreamBufferManager>(
      device_context_, video_capture_use_gmb_, std::move(camera_buffer_factory),
      std::make_unique<VideoCaptureBufferObserver>(GetWeakPtr()));
  // We use base::Unretained() for the StreamBufferManager here since we
  // guarantee |request_buffer_callback| is only used by RequestBuilder. In
  // addition, since C++ destroys member variables in reverse order of
  // construction, we can ensure that RequestBuilder will be destroyed prior
  // to StreamBufferManager since RequestBuilder constructs after
  // StreamBufferManager.
  auto request_buffer_callback =
      base::BindRepeating(&StreamBufferManager::RequestBufferForCaptureRequest,
                          base::Unretained(stream_buffer_manager_.get()));
  request_builder_ = std::make_unique<RequestBuilder>(
      device_context_, std::move(request_buffer_callback),
      use_buffer_management_apis_);
}

RequestManager::~RequestManager() = default;

void RequestManager::SetUpStreamsAndBuffers(
    base::flat_map<ClientType, VideoCaptureParams> capture_params,
    const cros::mojom::CameraMetadataPtr& static_metadata,
    std::vector<cros::mojom::Camera3StreamPtr> streams) {
  auto request_keys = GetMetadataEntryAsSpan<int32_t>(
      static_metadata,
      cros::mojom::CameraMetadataTag::ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS);
  zero_shutter_lag_supported_ = base::Contains(
      request_keys,
      static_cast<int32_t>(
          cros::mojom::CameraMetadataTag::ANDROID_CONTROL_ENABLE_ZSL));
  VLOG(1) << "Zero-shutter lag is "
          << (zero_shutter_lag_supported_ ? "" : "not ") << "supported";

  // The partial result count metadata is optional; defaults to 1 in case it
  // is not set in the static metadata.
  const cros::mojom::CameraMetadataEntryPtr* partial_count = GetMetadataEntry(
      static_metadata,
      cros::mojom::CameraMetadataTag::ANDROID_REQUEST_PARTIAL_RESULT_COUNT);
  if (partial_count) {
    partial_result_count_ =
        *reinterpret_cast<int32_t*>((*partial_count)->data.data());
  }

  auto pipeline_depth = GetMetadataEntryAsSpan<uint8_t>(
      static_metadata,
      cros::mojom::CameraMetadataTag::ANDROID_REQUEST_PIPELINE_MAX_DEPTH);
  CHECK_EQ(pipeline_depth.size(), 1u);
  pipeline_depth_ = pipeline_depth[0];
  preview_buffers_queued_ = 0;

  // Set the last received frame number for each stream types to be undefined.
  for (const auto& stream : streams) {
    StreamType stream_type = StreamIdToStreamType(stream->id);
    last_received_frame_number_map_[stream_type] = kUndefinedFrameNumber;
  }

  jpeg_thumbnail_size_ = GetJpegThumbnailSize(static_metadata, streams);

  stream_buffer_manager_->SetUpStreamsAndBuffers(
      capture_params, static_metadata, std::move(streams));
}

cros::mojom::Camera3StreamPtr RequestManager::GetStreamConfiguration(
    StreamType stream_type) {
  return stream_buffer_manager_->GetStreamConfiguration(stream_type);
}

bool RequestManager::HasStreamsConfiguredForTakePhoto() {
  if (stream_buffer_manager_->IsPortraitModeSupported()) {
    return stream_buffer_manager_->HasStreamsConfigured(
        {StreamType::kPreviewOutput, StreamType::kJpegOutput,
         StreamType::kPortraitJpegOutput});
  } else {
    return stream_buffer_manager_->HasStreamsConfigured(
        {StreamType::kPreviewOutput, StreamType::kJpegOutput});
  }
}

void RequestManager::StartPreview(
    cros::mojom::CameraMetadataPtr preview_settings) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(repeating_request_settings_.is_null());

  capturing_ = true;
  repeating_request_settings_ = std::move(preview_settings);

  PrepareCaptureRequest();
}

void RequestManager::StopPreview(base::OnceCallback<void(int32_t)> callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  capturing_ = false;
  repeating_request_settings_ = nullptr;
  if (callback) {
    capture_interface_->Flush(std::move(callback));
  }
}

void RequestManager::TakePhoto(cros::mojom::CameraMetadataPtr settings,
                               VideoCaptureDevice::TakePhotoCallback callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  take_photo_callback_queue_.push(std::move(callback));
  take_photo_settings_queue_.push(std::move(settings));
}

void RequestManager::TakePortraitPhoto(cros::mojom::CameraMetadataPtr settings,
                                       TakePhotoCallbackMap callbacks_map) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  take_portrait_photo_callback_map_ = std::move(callbacks_map);
  take_photo_settings_queue_.push(std::move(settings));
}

base::WeakPtr<RequestManager> RequestManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void RequestManager::AddResultMetadataObserver(
    ResultMetadataObserver* observer) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(!result_metadata_observers_.count(observer));

  result_metadata_observers_.insert(observer);
}

void RequestManager::RemoveResultMetadataObserver(
    ResultMetadataObserver* observer) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(result_metadata_observers_.count(observer));

  result_metadata_observers_.erase(observer);
}

void RequestManager::OnNewBuffer(ClientType client_type,
                                 cros::mojom::CameraBufferHandlePtr buffer) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  capture_interface_->OnNewBuffer(client_type, std::move(buffer));
}

void RequestManager::OnBufferRetired(ClientType client_type,
                                     uint64_t buffer_id) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  capture_interface_->OnBufferRetired(client_type, buffer_id);
}

void RequestManager::SetCaptureMetadata(cros::mojom::CameraMetadataTag tag,
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

void RequestManager::SetRepeatingCaptureMetadata(
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

void RequestManager::UnsetRepeatingCaptureMetadata(
    cros::mojom::CameraMetadataTag tag) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  auto it = capture_settings_repeating_override_.find(tag);
  if (it == capture_settings_repeating_override_.end()) {
    LOG(ERROR) << "Unset a non-existent metadata: " << tag;
    return;
  }
  capture_settings_repeating_override_.erase(it);
}

void RequestManager::SetPortraitModeVendorKey(
    cros::mojom::CameraMetadataPtr* settings) {
  auto e = BuildMetadataEntry(
      static_cast<cros::mojom::CameraMetadataTag>(kPortraitModeVendorKey),
      uint8_t{1});
  AddOrUpdateMetadataEntry(settings, std::move(e));
}

void RequestManager::SetJpegOrientation(
    cros::mojom::CameraMetadataPtr* settings,
    int32_t orientation) {
  auto e = BuildMetadataEntry(
      cros::mojom::CameraMetadataTag::ANDROID_JPEG_ORIENTATION, orientation);
  AddOrUpdateMetadataEntry(settings, std::move(e));
}

void RequestManager::SetJpegThumbnailSize(
    cros::mojom::CameraMetadataPtr* settings) const {
  std::vector<uint8_t> data(sizeof(int32_t) * 2);
  auto* data_i32 = reinterpret_cast<int32_t*>(data.data());
  data_i32[0] = base::checked_cast<int32_t>(jpeg_thumbnail_size_.width());
  data_i32[1] = base::checked_cast<int32_t>(jpeg_thumbnail_size_.height());
  cros::mojom::CameraMetadataEntryPtr e =
      cros::mojom::CameraMetadataEntry::New();
  e->tag = cros::mojom::CameraMetadataTag::ANDROID_JPEG_THUMBNAIL_SIZE;
  e->type = cros::mojom::EntryType::TYPE_INT32;
  e->count = data.size() / sizeof(int32_t);
  e->data = std::move(data);
  AddOrUpdateMetadataEntry(settings, std::move(e));
}

void RequestManager::SetZeroShutterLag(cros::mojom::CameraMetadataPtr* settings,
                                       bool enabled) {
  auto e = BuildMetadataEntry(
      cros::mojom::CameraMetadataTag::ANDROID_CONTROL_ENABLE_ZSL,
      static_cast<uint8_t>(enabled));
  AddOrUpdateMetadataEntry(settings, std::move(e));
}

void RequestManager::PrepareCaptureRequest() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (!capturing_) {
    return;
  }

  // We has several possible combinations of streams:
  //
  // If ZSL is enabled by default, the preview stream is not included in still
  // capture request.
  // 1. Preview (YuvOutput)
  // 2. Preview + Recording (YuvOutput)
  // 3. Capture (BlobOutput)
  // 4. Capture + Portrait Capture (BlobOutput + BlobOutput)
  //
  // If ZSL is not supported, the preview stream is included in still capture
  // request.
  // 1. Preview (YuvOutput)
  // 2. Preview + Recording (YuvOutput)
  // 3. Preview + Capture (YuvOutput + BlobOutput)

  std::set<StreamType> stream_types;
  cros::mojom::CameraMetadataPtr settings;
  VideoCaptureDevice::TakePhotoCallback callback = base::NullCallback();
  TakePhotoCallbackMap callbacks_map;

  bool is_portrait_request = false;
  bool is_preview_request = false;
  bool is_oneshot_request = false;
  bool is_recording_request = false;

  // First, check if there are pending portrait requests.
  is_portrait_request =
      TryPreparePortraitModeRequest(&stream_types, &settings, &callbacks_map);

  // If there is no pending portrait request, then check if there are pending
  // one-shot requests. And also try to put preview in the request.
  if (!is_portrait_request) {
    if (!zero_shutter_lag_supported_) {
      is_preview_request = TryPreparePreviewRequest(&stream_types, &settings);

      // Order matters here. If the preview request and oneshot request are both
      // added in single capture request, the settings will be overridden by the
      // later.
      is_oneshot_request =
          TryPrepareOneShotRequest(&stream_types, &settings, &callback);
    } else {
      // Zero-shutter lag could potentially give a frame from the past. Don't
      // prepare a preview request when a one shot request has been prepared.
      is_oneshot_request =
          TryPrepareOneShotRequest(&stream_types, &settings, &callback);

      if (!is_oneshot_request) {
        is_preview_request = TryPreparePreviewRequest(&stream_types, &settings);
      }
    }
  }

  if (is_preview_request) {
    is_recording_request = TryPrepareRecordingRequest(&stream_types);
  }

  if (!is_portrait_request && !is_oneshot_request && !is_preview_request &&
      !is_recording_request) {
    // We have to keep the pipeline full.
    if (preview_buffers_queued_ < pipeline_depth_) {
      ipc_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&RequestManager::PrepareCaptureRequest, GetWeakPtr()));
    }
    return;
  }

  // Sets crop region if there is a value set from Camera app.
  auto camera_app_device =
      CameraAppDeviceBridgeImpl::GetInstance()->GetWeakCameraAppDevice(
          device_id_);
  if (camera_app_device) {
    auto crop_region = camera_app_device->GetCropRegion();
    if (crop_region.has_value()) {
      SetCaptureMetadata(
          cros::mojom::CameraMetadataTag::ANDROID_SCALER_CROP_REGION,
          cros::mojom::EntryType::TYPE_INT32, crop_region->size(),
          SerializeMetadataValueFromSpan<int32_t>(*crop_region));
    }
  }

  auto capture_request = request_builder_->BuildRequest(std::move(stream_types),
                                                        std::move(settings));
  CHECK_GT(capture_request->output_buffers.size(), 0u);

  CaptureResult& pending_result =
      pending_results_[capture_request->frame_number];
  pending_result.unsubmitted_buffer_count =
      capture_request->output_buffers.size();
  pending_result.still_capture_callback = std::move(callback);
  pending_result.portrait_callbacks_map = std::move(callbacks_map);

  if (is_preview_request) {
    ++preview_buffers_queued_;
  }

  UpdateCaptureSettings(&capture_request->settings);
  if (device_api_version_ >= cros::mojom::CAMERA_DEVICE_API_VERSION_3_5) {
    capture_request->physcam_settings =
        std::vector<cros::mojom::Camera3PhyscamMetadataPtr>();
  }
  capture_interface_->ProcessCaptureRequest(
      std::move(capture_request),
      base::BindOnce(&RequestManager::OnProcessedCaptureRequest, GetWeakPtr()));
}

bool RequestManager::TryPreparePortraitModeRequest(
    std::set<StreamType>* stream_types,
    cros::mojom::CameraMetadataPtr* settings,
    TakePhotoCallbackMap* callbacks_map) {
  if (take_photo_settings_queue_.empty() ||
      !take_portrait_photo_callback_map_[StreamType::kJpegOutput] ||
      !take_portrait_photo_callback_map_[StreamType::kPortraitJpegOutput] ||
      !stream_buffer_manager_->HasFreeBuffers({StreamType::kJpegOutput}) ||
      !stream_buffer_manager_->HasFreeBuffers(
          {StreamType::kPortraitJpegOutput})) {
    return false;
  }
  stream_types->insert(
      {StreamType::kJpegOutput, StreamType::kPortraitJpegOutput});
  *callbacks_map = std::move(take_portrait_photo_callback_map_);

  // Prepare metadata by adding extra metadata.
  *settings = std::move(take_photo_settings_queue_.front());
  SetPortraitModeVendorKey(settings);
  SetJpegOrientation(settings, device_context_->GetCameraFrameRotation());
  SetJpegThumbnailSize(settings);
  SetZeroShutterLag(settings, true);
  take_photo_settings_queue_.pop();
  return true;
}

bool RequestManager::TryPreparePreviewRequest(
    std::set<StreamType>* stream_types,
    cros::mojom::CameraMetadataPtr* settings) {
  if (preview_buffers_queued_ == pipeline_depth_) {
    return false;
  }
  if (!stream_buffer_manager_->HasFreeBuffers({StreamType::kPreviewOutput})) {
    // Try our best to reserve an usable buffer.  If the reservation still
    // fails, then we'd have to drop the camera frame.
    DLOG(WARNING) << "Late request for reserving preview buffer";
    stream_buffer_manager_->ReserveBuffer(StreamType::kPreviewOutput);
    if (!stream_buffer_manager_->HasFreeBuffers({StreamType::kPreviewOutput})) {
      DLOG(WARNING) << "No free buffer for preview stream";
      return false;
    }
  }

  stream_types->insert({StreamType::kPreviewOutput});
  *settings = repeating_request_settings_.Clone();
  return true;
}

bool RequestManager::TryPrepareOneShotRequest(
    std::set<StreamType>* stream_types,
    cros::mojom::CameraMetadataPtr* settings,
    VideoCaptureDevice::TakePhotoCallback* callback) {
  if (take_photo_settings_queue_.empty() ||
      take_photo_callback_queue_.empty() ||
      !stream_buffer_manager_->HasFreeBuffers({StreamType::kJpegOutput})) {
    return false;
  }
  stream_types->insert({StreamType::kJpegOutput});
  *callback = std::move(take_photo_callback_queue_.front());
  take_photo_callback_queue_.pop();

  *settings = std::move(take_photo_settings_queue_.front());
  SetJpegOrientation(settings, device_context_->GetCameraFrameRotation());
  SetJpegThumbnailSize(settings);
  SetZeroShutterLag(settings, true);
  take_photo_settings_queue_.pop();
  return true;
}

bool RequestManager::TryPrepareRecordingRequest(
    std::set<StreamType>* stream_types) {
  if (!stream_buffer_manager_->IsRecordingSupported()) {
    return false;
  }

  if (!stream_buffer_manager_->HasFreeBuffers({StreamType::kRecordingOutput})) {
    // Try our best to reserve an usable buffer.  If the reservation still
    // fails, then we'd have to drop the camera frame.
    DLOG(WARNING) << "Late request for reserving recording buffer";
    stream_buffer_manager_->ReserveBuffer(StreamType::kRecordingOutput);
    if (!stream_buffer_manager_->HasFreeBuffers(
            {StreamType::kRecordingOutput})) {
      DLOG(WARNING) << "No free buffer for recording stream";
      return false;
    }
  }

  stream_types->insert({StreamType::kRecordingOutput});
  return true;
}

void RequestManager::OnProcessedCaptureRequest(int32_t result) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (!capturing_) {
    return;
  }
  if (result != 0) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3BufferManagerProcessCaptureRequestFailed,
        FROM_HERE,
        std::string("Process capture request failed: ") +
            base::safe_strerror(-result));
    return;
  }

  PrepareCaptureRequest();
}

void RequestManager::ProcessCaptureResult(
    cros::mojom::Camera3CaptureResultPtr result) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  uint32_t frame_number = result->frame_number;
  if (!capturing_) {
    if (result->output_buffers) {
      for (auto& stream_buffer : result->output_buffers.value()) {
        TRACE_EVENT_END("camera",
                        GetTraceTrack(CameraTraceEvent::kCaptureStream,
                                      frame_number, stream_buffer->stream_id));
      }
    }
    TRACE_EVENT("camera", "Capture Result", "frame_number", frame_number);
    TRACE_EVENT_END("camera", GetTraceTrack(CameraTraceEvent::kCaptureRequest,
                                            frame_number));
    return;
  }
  // A new partial result may be created in either ProcessCaptureResult or
  // Notify.
  CaptureResult& pending_result = pending_results_[frame_number];

  // |result->partial_result| is set to 0 if the capture result contains only
  // the result buffer handles and no result metadata.
  if (result->partial_result != 0) {
    uint32_t result_id = result->partial_result;
    if (result_id > partial_result_count_) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerInvalidPendingResultId,
          FROM_HERE,
          std::string("Invalid pending_result id: ") +
              base::NumberToString(result_id));
      return;
    }
    if (pending_result.partial_metadata_received.count(result_id)) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerReceivedDuplicatedPartialMetadata,
          FROM_HERE,
          std::string("Received duplicated partial metadata: ") +
              base::NumberToString(result_id));
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
              base::NumberToString(result->output_buffers->size()));
      return;
    }

    for (auto& stream_buffer : result->output_buffers.value()) {
      auto stream_id = stream_buffer->stream_id;
      DVLOG(2) << "Received capture result for frame " << frame_number
               << " stream_id: " << stream_id;
      StreamType stream_type = StreamIdToStreamType(stream_id);
      if (stream_type == StreamType::kUnknown) {
        device_context_->SetErrorState(
            media::VideoCaptureError::
                kCrosHalV3BufferManagerInvalidTypeOfOutputBuffersReceived,
            FROM_HERE,
            std::string("Invalid type of output buffers received: ") +
                base::NumberToString(stream_id));
        return;
      }

      // The camera HAL v3 API specifies that only one capture result can carry
      // the result buffer for any given frame number.
      if (last_received_frame_number_map_[stream_type] ==
          kUndefinedFrameNumber) {
        last_received_frame_number_map_[stream_type] = frame_number;
      } else {
        if (last_received_frame_number_map_[stream_type] == frame_number) {
          device_context_->SetErrorState(
              media::VideoCaptureError::
                  kCrosHalV3BufferManagerReceivedMultipleResultBuffersForFrame,
              FROM_HERE,
              std::string("Received multiple result buffers for frame ") +
                  base::NumberToString(frame_number) +
                  std::string(" for stream ") +
                  base::NumberToString(stream_id));
          return;
        } else if (last_received_frame_number_map_[stream_type] >
                   frame_number) {
          device_context_->SetErrorState(
              media::VideoCaptureError::
                  kCrosHalV3BufferManagerReceivedFrameIsOutOfOrder,
              FROM_HERE,
              std::string("Received frame is out-of-order; expect frame number "
                          "greater than ") +
                  base::NumberToString(
                      last_received_frame_number_map_[stream_type]) +
                  std::string(" but got ") +
                  base::NumberToString(frame_number));
        } else {
          last_received_frame_number_map_[stream_type] = frame_number;
        }
      }

      if (stream_buffer->status ==
          cros::mojom::Camera3BufferStatus::CAMERA3_BUFFER_STATUS_ERROR) {
        // If the buffer is marked as error, its content is discarded for this
        // frame.  Send the buffer to the free list directly through
        // SubmitCaptureResult.
        SubmitCaptureResult(frame_number, stream_type,
                            std::move(stream_buffer));
      } else {
        pending_result.buffers[stream_type] = std::move(stream_buffer);
      }
      TRACE_EVENT_END("camera", GetTraceTrack(CameraTraceEvent::kCaptureStream,
                                              frame_number, stream_id));
    }
  }

  TRACE_EVENT("camera", "Capture Result", "frame_number", frame_number);
  TrySubmitPendingBuffers(frame_number);
}

void RequestManager::TrySubmitPendingBuffers(uint32_t frame_number) {
  if (!pending_results_.count(frame_number)) {
    return;
  }

  CaptureResult& pending_result = pending_results_[frame_number];

  // If the metadata is not ready, or the shutter time is not set, just
  // returned.
  bool is_ready_to_submit =
      pending_result.partial_metadata_received.size() > 0 &&
      *pending_result.partial_metadata_received.rbegin() ==
          partial_result_count_ &&
      !pending_result.reference_time.is_null();
  if (!is_ready_to_submit) {
    return;
  }

  if (!pending_result.buffers.empty()) {
    // Put pending buffers into local map since |pending_result| might be
    // deleted in SubmitCaptureResult(). We should not reference pending_result
    // after SubmitCaptureResult() is triggered.
    std::map<StreamType, cros::mojom::Camera3StreamBufferPtr> buffers =
        std::move(pending_result.buffers);
    for (auto& it : buffers) {
      SubmitCaptureResult(frame_number, it.first, std::move(it.second));
    }
  }
  TRACE_EVENT_END(
      "camera", GetTraceTrack(CameraTraceEvent::kCaptureRequest, frame_number));
}

void RequestManager::Notify(cros::mojom::Camera3NotifyMsgPtr message) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (!capturing_) {
    return;
  }
  auto camera_app_device =
      CameraAppDeviceBridgeImpl::GetInstance()->GetWeakCameraAppDevice(
          device_id_);
  if (message->type == cros::mojom::Camera3MsgType::CAMERA3_MSG_ERROR) {
    auto error = std::move(message->message->get_error());
    uint32_t frame_number = error->frame_number;
    if (pending_results_.count(frame_number)) {
      CaptureResult& pending_result = pending_results_[frame_number];
      if (camera_app_device &&
          (pending_result.still_capture_callback ||
           pending_result.portrait_callbacks_map[StreamType::kJpegOutput])) {
        camera_app_device->OnShutterDone();
      }
    }
    uint64_t error_stream_id = error->error_stream_id;
    StreamType stream_type = StreamIdToStreamType(error_stream_id);
    if (stream_type == StreamType::kUnknown) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerUnknownStreamInCamera3NotifyMsg,
          FROM_HERE,
          std::string("Unknown stream in Camera3NotifyMsg: ") +
              base::NumberToString(error_stream_id));
      return;
    }
    cros::mojom::Camera3ErrorMsgCode error_code = error->error_code;
    HandleNotifyError(frame_number, stream_type, error_code);
  } else if (message->type ==
             cros::mojom::Camera3MsgType::CAMERA3_MSG_SHUTTER) {
    auto shutter = std::move(message->message->get_shutter());
    uint32_t frame_number = shutter->frame_number;
    uint64_t shutter_time = shutter->timestamp;
    DVLOG(2) << "Received shutter time for frame " << frame_number;
    if (!shutter_time) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerReceivedInvalidShutterTime,
          FROM_HERE,
          std::string("Received invalid shutter time: ") +
              base::NumberToString(shutter_time));
      return;
    }
    CaptureResult& pending_result = pending_results_[frame_number];
    pending_result.shutter_timestamp = shutter_time;
    // Shutter timestamp is in ns.
    base::TimeTicks reference_time =
        base::TimeTicks() + base::Microseconds(shutter_time / 1000);
    pending_result.reference_time = reference_time;
    if (first_frame_shutter_time_.is_null()) {
      // Record the shutter time of the first frame for calculating the
      // timestamp.
      first_frame_shutter_time_ = reference_time;
    }
    pending_result.timestamp = reference_time - first_frame_shutter_time_;

    if (camera_app_device &&
        (pending_result.still_capture_callback ||
         pending_result.portrait_callbacks_map[StreamType::kJpegOutput])) {
      camera_app_device->OnShutterDone();
    }

    TrySubmitPendingBuffers(frame_number);
  }
}

void RequestManager::HandleNotifyError(
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
          base::NumberToString(frame_number);
      break;

    case cros::mojom::Camera3ErrorMsgCode::CAMERA3_MSG_ERROR_RESULT:
      // An error has occurred in producing the output metadata buffer for a
      // result; the output metadata will not be available for the frame
      // specified by |frame_number|.  Subsequent requests are unaffected.
      warning_msg = std::string(
                        "An error occurred while producing result "
                        "metadata for frame ") +
                    base::NumberToString(frame_number);
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
              "An error occurred while filling output buffer for frame ") +
          base::NumberToString(frame_number);
      break;

    default:
      // To eliminate the warning for not handling CAMERA3_MSG_NUM_ERRORS
      break;
  }

  LOG(WARNING) << warning_msg << " with type = " << stream_type;
  device_context_->LogToClient(warning_msg);

  // If the buffer is already returned by the HAL, submit it and we're done.
  if (pending_results_.count(frame_number)) {
    auto it = pending_results_[frame_number].buffers.find(stream_type);
    if (it != pending_results_[frame_number].buffers.end()) {
      auto stream_buffer = std::move(it->second);
      pending_results_[frame_number].buffers.erase(stream_type);
      SubmitCaptureResult(frame_number, stream_type, std::move(stream_buffer));
    }
  }
}

void RequestManager::RequestStreamBuffers(
    std::vector<cros::mojom::Camera3BufferRequestPtr> buffer_reqs,
    RequestStreamBuffersCallback callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (!capturing_) {
    std::move(callback).Run(cros::mojom::Camera3BufferRequestStatus::
                                CAMERA3_BUF_REQ_FAILED_CONFIGURING,
                            {});
    return;
  }

  // Validate arguments.
  if (buffer_reqs.empty()) {
    std::move(callback).Run(cros::mojom::Camera3BufferRequestStatus::
                                CAMERA3_BUF_REQ_FAILED_ILLEGAL_ARGUMENTS,
                            {});
    return;
  }
  std::set<uint64_t> stream_ids;
  for (const auto& req : buffer_reqs) {
    StreamType stream_type = StreamIdToStreamType(req->stream_id);
    if (stream_type == StreamType::kUnknown ||
        stream_ids.count(req->stream_id) > 0) {
      std::move(callback).Run(cros::mojom::Camera3BufferRequestStatus::
                                  CAMERA3_BUF_REQ_FAILED_ILLEGAL_ARGUMENTS,
                              {});
      return;
    }
    stream_ids.insert(req->stream_id);
  }

  std::vector<mojo::StructPtr<cros::mojom::Camera3StreamBufferRet>> rets;
  size_t error_count = 0;
  for (const auto& req : buffer_reqs) {
    rets.push_back(cros::mojom::Camera3StreamBufferRet::New());
    auto& ret = rets.back();

    ret->stream_id = req->stream_id;
    StreamType stream_type = StreamIdToStreamType(req->stream_id);
    if (stream_buffer_manager_->GetFreeBufferCount(stream_type) <
        req->num_buffers_requested) {
      ret->status = cros::mojom::Camera3StreamBufferReqStatus::
          CAMERA3_PS_BUF_REQ_MAX_BUFFER_EXCEEDED;
      ++error_count;
      continue;
    }

    ret->status =
        cros::mojom::Camera3StreamBufferReqStatus::CAMERA3_PS_BUF_REQ_OK;
    ret->output_buffers = std::vector<cros::mojom::Camera3StreamBufferPtr>();
    for (size_t i = 0; i < req->num_buffers_requested; ++i) {
      std::optional<BufferInfo> buffer_info =
          stream_buffer_manager_->RequestBufferForCaptureRequest(stream_type);
      if (!buffer_info.has_value()) {
        // Return buffers to |stream_buffer_manager_|.
        for (const auto& b : *ret->output_buffers) {
          stream_buffer_manager_->ReleaseBufferFromCaptureResult(stream_type,
                                                                 b->buffer_id);
        }
        ret->status = cros::mojom::Camera3StreamBufferReqStatus::
            CAMERA3_PS_BUF_REQ_UNKNOWN_ERROR;
        ++error_count;
        break;
      }
      cros::mojom::Camera3StreamBufferPtr stream_buffer =
          request_builder_->CreateStreamBuffer(stream_type,
                                               std::move(*buffer_info));
      ret->output_buffers->push_back(std::move(stream_buffer));
    }
  }

  cros::mojom::Camera3BufferRequestStatus status =
      cros::mojom::Camera3BufferRequestStatus::CAMERA3_BUF_REQ_OK;
  if (error_count == buffer_reqs.size()) {
    status =
        cros::mojom::Camera3BufferRequestStatus::CAMERA3_BUF_REQ_FAILED_UNKNOWN;
  } else if (error_count > 0) {
    status =
        cros::mojom::Camera3BufferRequestStatus::CAMERA3_BUF_REQ_FAILED_PARTIAL;
  }

  std::move(callback).Run(status, std::move(rets));
}

void RequestManager::ReturnStreamBuffers(
    std::vector<cros::mojom::Camera3StreamBufferPtr> buffers) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  for (const auto& buffer : buffers) {
    StreamType stream_type = StreamIdToStreamType(buffer->stream_id);
    if (stream_type == StreamType::kUnknown) {
      continue;
    }
    stream_buffer_manager_->ReleaseBufferFromCaptureResult(stream_type,
                                                           buffer->buffer_id);
  }
}

void RequestManager::SubmitCaptureResult(
    uint32_t frame_number,
    StreamType stream_type,
    cros::mojom::Camera3StreamBufferPtr stream_buffer) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(pending_results_.count(frame_number));

  CaptureResult& pending_result = pending_results_[frame_number];
  DVLOG(2) << "Submit capture result of frame " << frame_number
           << " for stream " << static_cast<int>(stream_type);
  for (ResultMetadataObserver* observer : result_metadata_observers_) {
    observer->OnResultMetadataAvailable(frame_number, pending_result.metadata);
  }

  auto camera_app_device =
      CameraAppDeviceBridgeImpl::GetInstance()->GetWeakCameraAppDevice(
          device_id_);
  if (camera_app_device) {
    camera_app_device->OnResultMetadataAvailable(
        pending_result.metadata,
        static_cast<cros::mojom::StreamType>(stream_type));
  }

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
    if (sync_wait(fence.GetFD().get(), kSyncWaitTimeoutMs)) {
      device_context_->SetErrorState(
          media::VideoCaptureError::
              kCrosHalV3BufferManagerSyncWaitOnReleaseFenceTimedOut,
          FROM_HERE, "Sync wait on release fence timed out");
      return;
    }
  }

  uint64_t buffer_ipc_id = stream_buffer->buffer_id;
  // Deliver the captured data to client.
  if (stream_buffer->status ==
      cros::mojom::Camera3BufferStatus::CAMERA3_BUFFER_STATUS_OK) {
    if (stream_type == StreamType::kPreviewOutput ||
        stream_type == StreamType::kRecordingOutput) {
      SubmitCapturedPreviewRecordingBuffer(frame_number, buffer_ipc_id,
                                           stream_type);
    } else if (stream_type == StreamType::kJpegOutput ||
               stream_type == StreamType::kPortraitJpegOutput) {
      SubmitCapturedJpegBuffer(frame_number, buffer_ipc_id, stream_type);
    }
  } else {
    stream_buffer_manager_->ReleaseBufferFromCaptureResult(stream_type,
                                                           buffer_ipc_id);
  }

  if (stream_type == StreamType::kPreviewOutput) {
    --preview_buffers_queued_;
  }

  pending_result.unsubmitted_buffer_count--;

  if (pending_result.unsubmitted_buffer_count == 0) {
    pending_results_.erase(frame_number);
  }
  // Every time a buffer is released, try to prepare another capture request
  // again.
  PrepareCaptureRequest();
}

void RequestManager::SubmitCapturedPreviewRecordingBuffer(
    uint32_t frame_number,
    uint64_t buffer_ipc_id,
    StreamType stream_type) {
  const CaptureResult& pending_result = pending_results_[frame_number];
  auto client_type = kStreamClientTypeMap[static_cast<int>(stream_type)];

  if (video_capture_use_gmb_) {
    VideoCaptureFormat format;
    std::optional<VideoCaptureDevice::Client::Buffer> buffer =
        stream_buffer_manager_->AcquireBufferForClientById(
            stream_type, buffer_ipc_id, &format);
    CHECK(buffer);

    // TODO: Figure out the right color space for the camera frame.  We may need
    // to populate the camera metadata with the color space reported by the V4L2
    // device.
    VideoFrameMetadata metadata;
    if (!device_context_->IsCameraFrameRotationEnabledAtSource()) {
      // Camera frame rotation at source is disabled, so we record the intended
      // video frame rotation in the metadata.  The consumer of the video frame
      // is responsible for taking care of the frame rotation.
      auto translate_rotation = [](const int rotation) -> VideoRotation {
        switch (rotation) {
          case 0:
            return VIDEO_ROTATION_0;
          case 90:
            return VIDEO_ROTATION_90;
          case 180:
            return VIDEO_ROTATION_180;
          case 270:
            return VIDEO_ROTATION_270;
        }
        return VIDEO_ROTATION_0;
      };
      metadata.transformation =
          translate_rotation(device_context_->GetCameraFrameRotation());
    } else {
      // All frames are pre-rotated to the display orientation.
      metadata.transformation = VIDEO_ROTATION_0;
    }

    auto camera_app_device =
        CameraAppDeviceBridgeImpl::GetInstance()->GetWeakCameraAppDevice(
            device_id_);
    if (camera_app_device && stream_type == StreamType::kPreviewOutput) {
      camera_app_device->MaybeDetectDocumentCorners(
          stream_buffer_manager_->CreateGpuMemoryBuffer(
              buffer->handle_provider->GetGpuMemoryBufferHandle(), format,
              gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE),
          metadata.transformation->rotation);
    }

    device_context_->SubmitCapturedVideoCaptureBuffer(
        client_type, std::move(*buffer), format, pending_result.reference_time,
        pending_result.timestamp, metadata);
    // |buffer| ownership is transferred to client, so we need to reserve a
    // new video buffer.
    stream_buffer_manager_->ReserveBuffer(stream_type);
  } else {
    gfx::GpuMemoryBuffer* gmb = stream_buffer_manager_->GetGpuMemoryBufferById(
        stream_type, buffer_ipc_id);
    CHECK(gmb);
    device_context_->SubmitCapturedGpuMemoryBuffer(
        client_type, gmb,
        stream_buffer_manager_->GetStreamCaptureFormat(stream_type),
        pending_result.reference_time, pending_result.timestamp);
    stream_buffer_manager_->ReleaseBufferFromCaptureResult(stream_type,
                                                           buffer_ipc_id);
  }
}

void RequestManager::SubmitCapturedJpegBuffer(uint32_t frame_number,
                                              uint64_t buffer_ipc_id,
                                              StreamType stream_type) {
  CaptureResult& pending_result = pending_results_[frame_number];
  gfx::Size buffer_dimension =
      stream_buffer_manager_->GetBufferDimension(stream_type);
  gfx::GpuMemoryBuffer* gmb = stream_buffer_manager_->GetGpuMemoryBufferById(
      stream_type, buffer_ipc_id);
  CHECK(gmb);
  if (!gmb->Map()) {
    device_context_->SetErrorState(
        media::VideoCaptureError::
            kCrosHalV3BufferManagerFailedToCreateGpuMemoryBuffer,
        FROM_HERE, "Failed to map GPU memory buffer");
    return;
  }
  absl::Cleanup unmap_gmb = [gmb] { gmb->Unmap(); };

  const Camera3JpegBlob* header = reinterpret_cast<Camera3JpegBlob*>(
      reinterpret_cast<const uintptr_t>(gmb->memory(0)) +
      buffer_dimension.width() - sizeof(Camera3JpegBlob));
  if (header->jpeg_blob_id != kCamera3JpegBlobId) {
    device_context_->SetErrorState(
        media::VideoCaptureError::kCrosHalV3BufferManagerInvalidJpegBlob,
        FROM_HERE, "Invalid JPEG blob");
    return;
  }
  // Still capture result from HALv3 already has orientation info in EXIF,
  // so just provide 0 as screen rotation in |blobify_callback_| parameters.
  mojom::BlobPtr blob = blobify_callback_.Run(
      reinterpret_cast<const uint8_t*>(gmb->memory(0)), header->jpeg_size,
      stream_buffer_manager_->GetStreamCaptureFormat(stream_type), 0);
  if (blob) {
    if (stream_type == StreamType::kJpegOutput &&
        pending_result.portrait_callbacks_map[StreamType::kJpegOutput]) {
      std::move(pending_result.portrait_callbacks_map[StreamType::kJpegOutput])
          .Run(0, std::move(blob));
    } else if (stream_type == StreamType::kPortraitJpegOutput &&
               pending_result
                   .portrait_callbacks_map[StreamType::kPortraitJpegOutput]) {
      int status = CameraAppDeviceImpl::GetPortraitSegResultCode(
          &pending_result.metadata);
      std::move(pending_result
                    .portrait_callbacks_map[StreamType::kPortraitJpegOutput])
          .Run(status, std::move(blob));
    } else if (pending_result.still_capture_callback) {
      std::move(pending_result.still_capture_callback).Run(std::move(blob));
    }
  } else {
    // TODO(wtlee): If it is fatal, we should set error state here.
    LOG(ERROR) << "Failed to blobify the captured JPEG image";
  }

  stream_buffer_manager_->ReleaseBufferFromCaptureResult(stream_type,
                                                         buffer_ipc_id);
}

void RequestManager::UpdateCaptureSettings(
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

RequestManager::CaptureResult::CaptureResult()
    : metadata(cros::mojom::CameraMetadata::New()),
      unsubmitted_buffer_count(0) {}

RequestManager::CaptureResult::~CaptureResult() = default;

}  // namespace media
