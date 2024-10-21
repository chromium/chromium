// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/capture/video/chromeos/camera_app_device_impl.h"

#include <algorithm>
#include <cmath>

#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"
#include "media/capture/video/chromeos/camera_app_device_bridge_impl.h"
#include "media/capture/video/chromeos/camera_device_context.h"
#include "media/capture/video/chromeos/camera_metadata_utils.h"
#include "media/capture/video/chromeos/mojom/document_scanner.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/cros_system_api/mojo/service_constants.h"
#include "third_party/libyuv/include/libyuv.h"

namespace media {

namespace {

constexpr int kDetectionWidth = 256;
constexpr int kDetectionHeight = 256;

}  // namespace

class CameraAppDeviceImpl::DocumentScanner {
 public:
  using DetectCornersFromNV12ImageCallback =
      base::OnceCallback<void(bool success,
                              const std::vector<gfx::PointF>& results)>;

  DocumentScanner() {
    if (!ash::mojo_service_manager::IsServiceManagerBound()) {
      return;
    }
    ash::mojo_service_manager::GetServiceManagerProxy()->Request(
        chromeos::mojo_services::kCrosDocumentScanner, std::nullopt,
        document_scanner_remote_.BindNewPipeAndPassReceiver().PassPipe());
  }

  DocumentScanner(const DocumentScanner&) = delete;
  DocumentScanner& operator=(const DocumentScanner&) = delete;

  ~DocumentScanner() = default;

  void DetectCornersFromNV12Image(base::ReadOnlySharedMemoryRegion nv12_image,
                                  DetectCornersFromNV12ImageCallback callback) {
    document_scanner_remote_->DetectCornersFromNV12Image(
        std::move(nv12_image),
        base::BindOnce(
            [](DetectCornersFromNV12ImageCallback callback,
               cros::mojom::DetectCornersResultPtr detect_result) {
              std::move(callback).Run(detect_result->success,
                                      std::move(detect_result->corners));
            },
            std::move(callback)));
  }

 private:
  mojo::Remote<cros::mojom::CrosDocumentScanner> document_scanner_remote_;
};

// static
int CameraAppDeviceImpl::GetPortraitSegResultCode(
    const cros::mojom::CameraMetadataPtr* metadata) {
  auto portrait_mode_segmentation_result = GetMetadataEntryAsSpan<uint8_t>(
      *metadata, static_cast<cros::mojom::CameraMetadataTag>(
                     kPortraitModeSegmentationResultVendorKey));
  CHECK(!portrait_mode_segmentation_result.empty());
  return static_cast<int>(portrait_mode_segmentation_result[0]);
}

CameraAppDeviceImpl::CameraAppDeviceImpl(
    const std::string& device_id,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : device_id_(device_id),
      allow_new_ipc_weak_ptrs_(true),
      capture_intent_(cros::mojom::CaptureIntent::kDefault),
      camera_device_context_(nullptr),
      document_scanner_(ui_task_runner) {}

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

std::optional<gfx::Range> CameraAppDeviceImpl::GetFpsRange() {
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

void CameraAppDeviceImpl::OnCameraInfoUpdated(
    cros::mojom::CameraInfoPtr camera_info) {
  base::AutoLock lock(camera_info_lock_);
  camera_info_ = std::move(camera_info);

  if (!mojo_task_runner_) {
    return;
  }
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraAppDeviceImpl::NotifyCameraInfoUpdatedOnMojoThread,
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

void CameraAppDeviceImpl::TakePortraitModePhoto(
    mojo::PendingRemote<cros::mojom::StillCaptureResultObserver> observer,
    TakePortraitModePhotoCallback callback) {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());

  base::AutoLock lock(portrait_mode_callbacks_lock_);
  portrait_mode_observers_.reset();
  portrait_mode_observers_.Bind(std::move(observer));
  take_portrait_photo_callbacks_.reset();

  // Create two callbacks that will notify the client when the result is
  // returned. The `normal_photo_callback` is for the normal photo, and
  // `portrait_photo_callback` is for the portrait photo.
  PortraitModeCallbacks take_portrait_photo_callbacks;
  take_portrait_photo_callbacks.normal_photo_callback =
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&CameraAppDeviceImpl::NotifyPortraitResultOnMojoThread,
                         weak_ptr_factory_for_mojo_.GetWeakPtr(),
                         cros::mojom::Effect::kNoEffect));
  take_portrait_photo_callbacks.portrait_photo_callback =
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&CameraAppDeviceImpl::NotifyPortraitResultOnMojoThread,
                         weak_ptr_factory_for_mojo_.GetWeakPtr(),
                         cros::mojom::Effect::kPortraitMode));
  take_portrait_photo_callbacks_ = std::move(take_portrait_photo_callbacks);

  std::move(callback).Run();
}

void CameraAppDeviceImpl::SetFpsRange(const gfx::Range& fps_range,
                                      SetFpsRangeCallback callback) {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());

  const int entry_length = 2;

  base::AutoLock camera_info_lock(camera_info_lock_);
  if (!camera_info_) {
    LOG(ERROR) << "Camera info is still not available at this moment";
    std::move(callback).Run(false);
    return;
  }
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

void CameraAppDeviceImpl::RegisterCameraInfoObserver(
    mojo::PendingRemote<cros::mojom::CameraInfoObserver> observer,
    RegisterCameraInfoObserverCallback callback) {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());

  camera_info_observers_.Add(std::move(observer));
  std::move(callback).Run();

  NotifyCameraInfoUpdatedOnMojoThread();
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

  if (IsCloseToPreviousDetectionRequest() ||
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
  if (!memory.IsValid()) {
    LOG(ERROR) << "Failed to allocate shared memory";
    return;
  }
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

  document_scanner_.AsyncCall(&DocumentScanner::DetectCornersFromNV12Image)
      .WithArgs(std::move(memory.region),
                base::BindPostTaskToCurrentDefault(base::BindOnce(
                    &CameraAppDeviceImpl::OnDetectedDocumentCornersOnMojoThread,
                    weak_ptr_factory_for_mojo_.GetWeakPtr(), rotation)));
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
    float x = std::clamp(corner.x(), 0.0f, 1.0f);
    float y = std::clamp(corner.y(), 0.0f, 1.0f);

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
        NOTREACHED_IN_MIGRATION();
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

void CameraAppDeviceImpl::NotifyPortraitResultOnMojoThread(
    cros::mojom::Effect effect,
    const int32_t status,
    media::mojom::BlobPtr blob) {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());

  portrait_mode_observers_->OnStillCaptureDone(effect, status, std::move(blob));
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

void CameraAppDeviceImpl::NotifyCameraInfoUpdatedOnMojoThread() {
  DCHECK(mojo_task_runner_->BelongsToCurrentThread());

  base::AutoLock lock(camera_info_lock_);
  if (!camera_info_) {
    return;
  }
  for (auto& observer : camera_info_observers_) {
    observer->OnCameraInfoUpdated(camera_info_.Clone());
  }
}

std::optional<PortraitModeCallbacks>
CameraAppDeviceImpl::ConsumePortraitModeCallbacks() {
  base::AutoLock lock(portrait_mode_callbacks_lock_);
  std::optional<PortraitModeCallbacks> callbacks;
  if (take_portrait_photo_callbacks_.has_value()) {
    callbacks = std::move(take_portrait_photo_callbacks_);
    take_portrait_photo_callbacks_.reset();
  }
  return callbacks;
}

void CameraAppDeviceImpl::SetCropRegion(const gfx::Rect& crop_region,
                                        SetCropRegionCallback callback) {
  CHECK(mojo_task_runner_->BelongsToCurrentThread());

  base::AutoLock lock(crop_region_lock_);
  crop_region_ = {
      crop_region.x(),
      crop_region.y(),
      crop_region.width(),
      crop_region.height(),
  };

  std::move(callback).Run();
}

void CameraAppDeviceImpl::ResetCropRegion(ResetCropRegionCallback callback) {
  CHECK(mojo_task_runner_->BelongsToCurrentThread());

  base::AutoLock lock(crop_region_lock_);
  crop_region_.reset();

  std::move(callback).Run();
}

std::optional<std::vector<int32_t>> CameraAppDeviceImpl::GetCropRegion() {
  base::AutoLock lock(crop_region_lock_);
  return crop_region_;
}

}  // namespace media
