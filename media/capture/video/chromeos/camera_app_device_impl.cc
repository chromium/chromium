// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_app_device_impl.h"

#include <cmath>

#include "base/cxx17_backports.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/video/chromeos/camera_app_device_bridge_impl.h"
#include "media/capture/video/chromeos/camera_device_context.h"
#include "media/capture/video/chromeos/camera_metadata_utils.h"
#include "third_party/libyuv/include/libyuv.h"

namespace media {

namespace {

constexpr int kDetectionWidth = 256;
constexpr int kDetectionHeight = 256;

void OnStillCaptureDone(media::mojom::ImageCapture::TakePhotoCallback callback,
                        int status,
                        mojom::BlobPtr blob) {
  DCHECK_EQ(status, kReprocessSuccess);
  std::move(callback).Run(std::move(blob));
}

}  // namespace

ReprocessTask::ReprocessTask() = default;

ReprocessTask::ReprocessTask(ReprocessTask&& other)
    : effect(other.effect),
      callback(std::move(other.callback)),
      extra_metadata(std::move(other.extra_metadata)) {}

ReprocessTask::~ReprocessTask() = default;

// static
int CameraAppDeviceImpl::GetReprocessReturnCode(
    cros::mojom::Effect effect,
    const cros::mojom::CameraMetadataPtr* metadata) {
  if (effect == cros::mojom::Effect::PORTRAIT_MODE) {
    auto portrait_mode_segmentation_result = GetMetadataEntryAsSpan<uint8_t>(
        *metadata, static_cast<cros::mojom::CameraMetadataTag>(
                       kPortraitModeSegmentationResultVendorKey));
    DCHECK(!portrait_mode_segmentation_result.empty());
    return static_cast<int>(portrait_mode_segmentation_result[0]);
  }
  return kReprocessSuccess;
}

// static
ReprocessTaskQueue CameraAppDeviceImpl::GetSingleShotReprocessOptions(
    media::mojom::ImageCapture::TakePhotoCallback take_photo_callback) {
  ReprocessTaskQueue result_task_queue;
  ReprocessTask still_capture_task;
  still_capture_task.effect = cros::mojom::Effect::NO_EFFECT;
  still_capture_task.callback =
      base::BindOnce(&OnStillCaptureDone, std::move(take_photo_callback));
  // Explicitly disable edge enhancement and noise reduction for YUV -> JPG
  // conversion.
  DisableEeNr(&still_capture_task);
  result_task_queue.push(std::move(still_capture_task));
  return result_task_queue;
}

CameraAppDeviceImpl::CameraAppDeviceImpl(const std::string& device_id,
                                         cros::mojom::CameraInfoPtr camera_info)
    : device_id_(device_id),
      allow_new_ipc_weak_ptrs_(true),
      camera_info_(std::move(camera_info)),
      capture_intent_(cros::mojom::CaptureIntent::DEFAULT),
      camera_device_context_(nullptr) {}

CameraAppDeviceImpl::~CameraAppDeviceImpl() {
  // If the instance is bound, then this instance should only be destroyed when
  // the mojo connection is dropped, which also happens on the mojo thread.
  DCHECK(!mojo_task_runner_ || mojo_task_runner_->BelongsToCurrentThread());

  // All the weak pointers of |weak_ptr_factory_| should be invalidated on
  // camera device IPC thread before destroying CameraAppDeviceImpl.
  DCHECK(!weak_ptr_factory_.HasWeakPtrs());
}

void CameraAppDeviceImpl::BindReceiver(
    mojo::PendingReceiver<cros::mojom::CameraAppDevice> receiver) {
  mojo_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  receivers_.Add(this, std::move(receiver));
  receivers_.set_disconnect_handler(
      base::BindRepeating(&CameraAppDeviceImpl::OnMojoConnectionError,
                          weak_ptr_factory_for_mojo_.GetWeakPtr()));
  document_scanner_service_ = ash::DocumentScannerServiceClient::Create();
}

base::WeakPtr<CameraAppDeviceImpl> CameraAppDeviceImpl::GetWeakPtr() {
  return allow_new_ipc_weak_ptrs_ ? weak_ptr_factory_.GetWeakPtr() : nullptr;
}

void CameraAppDeviceImpl::ResetOnDeviceIpcThread(base::OnceClosure callback,
                                                 bool should_disable_new_ptrs) {
  if (should_disable_new_ptrs) {
    allow_new_ipc_weak_ptrs_ = false;
  }
  weak_ptr_factory_.InvalidateWeakPtrs();
  std::move(callback).Run();
}

void CameraAppDeviceImpl::ConsumeReprocessOptions(
    media::mojom::ImageCapture::TakePhotoCallback take_photo_callback,
    base::OnceCallback<void(ReprocessTaskQueue)> consumption_callback) {
  ReprocessTaskQueue result_task_queue;

  ReprocessTask still_capture_task;
  still_capture_task.effect = cros::mojom::Effect::NO_EFFECT;
  still_capture_task.callback =
      base::BindOnce(&OnStillCaptureDone, std::move(take_photo_callback));
  // Explicitly disable edge enhancement and noise reduction for YUV -> JPG
  // conversion.
  DisableEeNr(&still_capture_task);
  result_task_queue.push(std::move(still_capture_task));

  base::AutoLock lock(reprocess_tasks_lock_);
  while (!reprocess_task_queue_.empty()) {
    result_task_queue.push(std::move(reprocess_task_queue_.front()));
    reprocess_task_queue_.pop();
  }
  std::move(consumption_callback).Run(std::move(result_task_queue));
}

absl::optional<gfx::Range> CameraAppDeviceImpl::GetFpsRange() {
  base::AutoLock lock(fps_ranges_lock_);

  return specified_fps_range_;
}

gfx::Size CameraAppDeviceImpl::GetStillCaptureResolution() {
  base::AutoLock lock(still_capture_resolution_lock_);

  return still_capture_resolution_;
}

cros::mojom::CaptureIntent CameraAppDeviceImpl::GetCaptureIntent() {
  base::AutoLock lock(capture_intent_lock_);
  return capture_intent_;
}

void CameraAppDeviceImpl::OnResultMetadataAvailable(
    const cros::mojom::CameraMetadataPtr& metadata,
    cros::mojom::StreamType streamType) {
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraAppDeviceImpl::NotifyResultMetadataOnMojoThread,
                     weak_ptr_factory_for_mojo_.GetWeakPtr(), metadata.Clone(),
                     streamType));
}

void CameraAppDeviceImpl::OnShutterDone() {
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraAppDeviceImpl::NotifyShutterDoneOnMojoThread,
                     weak_ptr_factory_for_mojo_.GetWeakPtr()));
}

void CameraAppDeviceImpl::SetCameraDeviceContext(
    CameraDeviceContext* camera_device_context) {
  base::AutoLock lock(camera_device_context_lock_);
  camera_device_context_ = camera_device_context;
}

void CameraAppDeviceImpl::MaybeDetectDocumentCorners(
    std::unique_ptr<gpu::GpuMemoryBufferImpl> gmb,
    VideoRotation rotation) {
  if (!ash::DocumentScannerServiceClient::IsSupported()) {
    return;
  }
  {
    base::AutoLock lock(document_corners_observers_lock_);
    if (document_corners_observers_.empty()) {
      return;
    }
  }
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraAppDeviceImpl::DetectDocumentCornersOnMojoThread,
                     weak_ptr_factory_for_mojo_.GetWeakPtr(), std::move(gmb),
                     rotation));
}

bool CameraAppDeviceImpl::IsMultipleStreamsEnabled() {
  base::AutoLock lock(multi_stream_lock_);
  return multi_stream_enabled_;
}

void CameraAppDeviceImpl::GetCameraInfo(GetCameraInfoCallback callback) {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());
  DCHECK(camera_info_);

  std::move(callback).Run(camera_info_.Clone());
}

void CameraAppDeviceImpl::SetReprocessOptions(
    const std::vector<cros::mojom::Effect>& effects,
    mojo::PendingRemote<cros::mojom::ReprocessResultListener> listener,
    SetReprocessOptionsCallback callback) {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());

  base::AutoLock lock(reprocess_tasks_lock_);
  reprocess_listener_.reset();
  reprocess_listener_.Bind(std::move(listener));
  reprocess_task_queue_ = {};
  for (const auto& effect : effects) {
    ReprocessTask task;
    task.effect = effect;
    task.callback = media::BindToCurrentLoop(
        base::BindOnce(&CameraAppDeviceImpl::SetReprocessResultOnMojoThread,
                       weak_ptr_factory_for_mojo_.GetWeakPtr(), effect));

    if (effect == cros::mojom::Effect::PORTRAIT_MODE) {
      auto e = BuildMetadataEntry(
          static_cast<cros::mojom::CameraMetadataTag>(kPortraitModeVendorKey),
          1);
      task.extra_metadata.push_back(std::move(e));
    }
    reprocess_task_queue_.push(std::move(task));
  }
  std::move(callback).Run();
}

void CameraAppDeviceImpl::SetFpsRange(const gfx::Range& fps_range,
                                      SetFpsRangeCallback callback) {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());

  const int entry_length = 2;

  auto& static_metadata = camera_info_->static_camera_characteristics;
  auto available_fps_range_entries = GetMetadataEntryAsSpan<int32_t>(
      static_metadata, cros::mojom::CameraMetadataTag::
                           ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);
  DCHECK(available_fps_range_entries.size() % entry_length == 0);

  bool is_valid = false;
  int min_fps = static_cast<int>(fps_range.GetMin());
  int max_fps = static_cast<int>(fps_range.GetMax());
  for (size_t i = 0; i < available_fps_range_entries.size();
       i += entry_length) {
    if (available_fps_range_entries[i] == min_fps &&
        available_fps_range_entries[i + 1] == max_fps) {
      is_valid = true;
      break;
    }
  }

  base::AutoLock lock(fps_ranges_lock_);

  if (is_valid) {
    specified_fps_range_ = fps_range;
  } else {
    specified_fps_range_ = {};
  }
  std::move(callback).Run(is_valid);
}

void CameraAppDeviceImpl::SetStillCaptureResolution(
    const gfx::Size& resolution,
    SetStillCaptureResolutionCallback callback) {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());

  base::AutoLock lock(still_capture_resolution_lock_);
  still_capture_resolution_ = resolution;
  std::move(callback).Run();
}

void CameraAppDeviceImpl::SetCaptureIntent(
    cros::mojom::CaptureIntent capture_intent,
    SetCaptureIntentCallback callback) {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());

  {
    base::AutoLock lock(capture_intent_lock_);
    capture_intent_ = capture_intent;
  }
  // Reset fps range for VCD to determine it if not explicitly set by app.
  {
    base::AutoLock lock(fps_ranges_lock_);
    specified_fps_range_ = {};
  }
  std::move(callback).Run();
}

void CameraAppDeviceImpl::AddResultMetadataObserver(
    mojo::PendingRemote<cros::mojom::ResultMetadataObserver> observer,
    cros::mojom::StreamType stream_type,
    AddResultMetadataObserverCallback callback) {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());

  stream_to_metadata_observers_map_[stream_type].Add(std::move(observer));
  std::move(callback).Run();
}

void CameraAppDeviceImpl::AddCameraEventObserver(
    mojo::PendingRemote<cros::mojom::CameraEventObserver> observer,
    AddCameraEventObserverCallback callback) {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());

  camera_event_observers_.Add(std::move(observer));
  std::move(callback).Run();
}

void CameraAppDeviceImpl::SetCameraFrameRotationEnabledAtSource(
    bool is_enabled,
    SetCameraFrameRotationEnabledAtSourceCallback callback) {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());

  bool is_success = false;
  {
    base::AutoLock lock(camera_device_context_lock_);
    if (camera_device_context_) {
      camera_device_context_->SetCameraFrameRotationEnabledAtSource(is_enabled);
      is_success = true;
    }
  }
  std::move(callback).Run(is_success);
}

void CameraAppDeviceImpl::GetCameraFrameRotation(
    GetCameraFrameRotationCallback callback) {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());

  uint32_t rotation = 0;
  {
    base::AutoLock lock(camera_device_context_lock_);
    if (camera_device_context_ &&
        !camera_device_context_->IsCameraFrameRotationEnabledAtSource()) {
      // The camera rotation value can only be [0, 90, 180, 270].
      rotation = static_cast<uint32_t>(
          camera_device_context_->GetCameraFrameRotation());
    }
  }
  std::move(callback).Run(rotation);
}

void CameraAppDeviceImpl::RegisterDocumentCornersObserver(
    mojo::PendingRemote<cros::mojom::DocumentCornersObserver> observer,
    RegisterDocumentCornersObserverCallback callback) {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());

  base::AutoLock lock(document_corners_observers_lock_);
  document_corners_observers_.Add(std::move(observer));
  std::move(callback).Run();
}

void CameraAppDeviceImpl::SetMultipleStreamsEnabled(
    bool enabled,
    SetMultipleStreamsEnabledCallback callback) {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());

  base::AutoLock lock(multi_stream_lock_);
  multi_stream_enabled_ = enabled;
  std::move(callback).Run();
}

// static
void CameraAppDeviceImpl::DisableEeNr(ReprocessTask* task) {
  auto ee_entry =
      BuildMetadataEntry(cros::mojom::CameraMetadataTag::ANDROID_EDGE_MODE,
                         cros::mojom::AndroidEdgeMode::ANDROID_EDGE_MODE_OFF);
  auto nr_entry = BuildMetadataEntry(
      cros::mojom::CameraMetadataTag::ANDROID_NOISE_REDUCTION_MODE,
      cros::mojom::AndroidNoiseReductionMode::ANDROID_NOISE_REDUCTION_MODE_OFF);
  task->extra_metadata.push_back(std::move(ee_entry));
  task->extra_metadata.push_back(std::move(nr_entry));
}

void CameraAppDeviceImpl::OnMojoConnectionError() {
  CameraAppDeviceBridgeImpl::GetInstance()->OnDeviceMojoDisconnected(
      device_id_);
}

bool CameraAppDeviceImpl::IsCloseToPreviousDetectionRequest() {
  return document_detection_timer_ &&
         document_detection_timer_->Elapsed().InMilliseconds() < 300;
}

void CameraAppDeviceImpl::DetectDocumentCornersOnMojoThread(
    std::unique_ptr<gpu::GpuMemoryBufferImpl> image,
    VideoRotation rotation) {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());
  DCHECK(document_scanner_service_);

  if (!document_scanner_service_->IsLoaded() ||
      IsCloseToPreviousDetectionRequest() ||
      has_ongoing_document_detection_task_) {
    return;
  }

  DCHECK(image);
  if (!image->Map()) {
    LOG(ERROR) << "Failed to map frame buffer";
    return;
  }
  auto frame_size = image->GetSize();
  int width = frame_size.width();
  int height = frame_size.height();

  base::MappedReadOnlyRegion memory = base::ReadOnlySharedMemoryRegion::Create(
      kDetectionWidth * kDetectionHeight * 3 / 2);

  auto* y_data = memory.mapping.GetMemoryAs<uint8_t>();
  auto* uv_data = y_data + kDetectionWidth * kDetectionHeight;

  int status = libyuv::NV12Scale(
      static_cast<uint8_t*>(image->memory(0)), image->stride(0),
      static_cast<uint8_t*>(image->memory(1)), image->stride(1), width, height,
      y_data, kDetectionWidth, uv_data, kDetectionWidth, kDetectionWidth,
      kDetectionHeight, libyuv::FilterMode::kFilterNone);
  image->Unmap();
  if (status != 0) {
    LOG(ERROR) << "Failed to scale buffer";
    return;
  }

  has_ongoing_document_detection_task_ = true;
  document_detection_timer_ = std::make_unique<base::ElapsedTimer>();
  // Since we destroy |document_scanner_service_| on mojo thread and this
  // callback is also called on mojo thread, it should be safe to just use
  // base::Unretained(this) here.
  document_scanner_service_->DetectCornersFromNV12Image(
      std::move(memory.region),
      base::BindOnce(
          &CameraAppDeviceImpl::OnDetectedDocumentCornersOnMojoThread,
          base::Unretained(this), rotation));
}

void CameraAppDeviceImpl::OnDetectedDocumentCornersOnMojoThread(
    VideoRotation rotation,
    bool success,
    const std::vector<gfx::PointF>& corners) {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());

  has_ongoing_document_detection_task_ = false;
  if (!success) {
    LOG(ERROR) << "Failed to detect document corners";
    return;
  }

  // Rotate a point in coordination space {x: [0.0, 1.0], y: [0.0, 1.0]} with
  // anchor point {x: 0.5, y: 0.5}.
  auto rotate_corner = [&](const gfx::PointF& corner) -> gfx::PointF {
    float x = base::clamp(corner.x(), 0.0f, 1.0f);
    float y = base::clamp(corner.y(), 0.0f, 1.0f);

    switch (rotation) {
      case VIDEO_ROTATION_0:
        return {x, y};
      case VIDEO_ROTATION_90:
        return {1.0f - y, x};
      case VIDEO_ROTATION_180:
        return {1.0f - x, 1.0f - y};
      case VIDEO_ROTATION_270:
        return {y, 1.0f - x};
      default:
        NOTREACHED();
    }
  };

  std::vector<gfx::PointF> rotated_corners;
  for (auto& corner : corners) {
    rotated_corners.push_back(rotate_corner(corner));
  }

  base::AutoLock lock(document_corners_observers_lock_);
  for (auto& observer : document_corners_observers_) {
    observer->OnDocumentCornersUpdated(rotated_corners);
  }
}

void CameraAppDeviceImpl::SetReprocessResultOnMojoThread(
    cros::mojom::Effect effect,
    const int32_t status,
    media::mojom::BlobPtr blob) {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());

  base::AutoLock lock(reprocess_tasks_lock_);
  reprocess_listener_->OnReprocessDone(effect, status, std::move(blob));
}

void CameraAppDeviceImpl::NotifyShutterDoneOnMojoThread() {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());

  for (auto& observer : camera_event_observers_) {
    observer->OnShutterDone();
  }
}

void CameraAppDeviceImpl::NotifyResultMetadataOnMojoThread(
    cros::mojom::CameraMetadataPtr metadata,
    cros::mojom::StreamType streamType) {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());

  auto& metadata_observers = stream_to_metadata_observers_map_[streamType];
  for (auto& observer : metadata_observers) {
    observer->OnMetadataAvailable(metadata.Clone());
  }
}

}  // namespace media
