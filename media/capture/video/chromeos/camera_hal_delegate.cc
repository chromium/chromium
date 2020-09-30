// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_hal_delegate.h"

#include <fcntl.h>
#include <sys/uio.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/flat_set.h"
#include "base/posix/safe_strerror.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/system/system_monitor.h"
#include "media/capture/video/chromeos/camera_app_device_bridge_impl.h"
#include "media/capture/video/chromeos/camera_buffer_factory.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "media/capture/video/chromeos/camera_metadata_utils.h"
#include "media/capture/video/chromeos/video_capture_device_chromeos_halv3.h"

namespace media {

namespace {

constexpr int32_t kDefaultFps = 30;

constexpr base::TimeDelta kEventWaitTimeoutSecs =
    base::TimeDelta::FromSeconds(1);

class LocalCameraClientObserver : public CameraClientObserver {
 public:
  explicit LocalCameraClientObserver(
      scoped_refptr<CameraHalDelegate> camera_hal_delegate)
      : camera_hal_delegate_(std::move(camera_hal_delegate)) {}

  void OnChannelCreated(
      mojo::PendingRemote<cros::mojom::CameraModule> camera_module) override {
    camera_hal_delegate_->SetCameraModule(std::move(camera_module));
  }

 private:
  scoped_refptr<CameraHalDelegate> camera_hal_delegate_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(LocalCameraClientObserver);
};

// chromeos::system::StatisticsProvider::IsRunningOnVM() is not available in
// unittest.
bool IsRunningOnVM() {
  static bool is_vm = []() {
    std::string output;
    if (!base::GetAppOutput({"crossystem", "inside_vm"}, &output)) {
      return false;
    }
    return output == "1";
  }();
  return is_vm;
}

bool IsVividLoaded() {
  std::string output;
  if (!base::GetAppOutput({"lsmod"}, &output)) {
    return false;
  }

  std::vector<base::StringPiece> lines = base::SplitStringPieceUsingSubstr(
      output, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  return std::any_of(lines.begin(), lines.end(), [](const auto& line) {
    return base::StartsWith(line, "vivid", base::CompareCase::SENSITIVE);
  });
}

void NotifyVideoCaptureDevicesChanged() {
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  // |monitor| might be nullptr in unittest.
  if (monitor) {
    monitor->ProcessDevicesChanged(
        base::SystemMonitor::DeviceType::DEVTYPE_VIDEO_CAPTURE);
  }
}

base::flat_set<int32_t> GetAvailableFramerates(
    const cros::mojom::CameraInfoPtr& camera_info) {
  base::flat_set<int32_t> candidates;
  auto available_fps_ranges = GetMetadataEntryAsSpan<int32_t>(
      camera_info->static_camera_characteristics,
      cros::mojom::CameraMetadataTag::
          ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);
  if (available_fps_ranges.empty()) {
    // If there is no available target fps ranges listed in metadata, we set a
    // default fps as candidate.
    LOG(WARNING) << "No available fps ranges in metadata. Set default fps as "
                    "candidate.";
    candidates.insert(kDefaultFps);
    return candidates;
  }

  // The available target fps ranges are stored as pairs int32s: (min, max) x n.
  const size_t kRangeMaxOffset = 1;
  const size_t kRangeSize = 2;

  for (size_t i = 0; i < available_fps_ranges.size(); i += kRangeSize) {
    candidates.insert(available_fps_ranges[i + kRangeMaxOffset]);
  }
  return candidates;
}

}  // namespace

CameraHalDelegate::CameraHalDelegate(
    scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner)
    : camera_module_has_been_set_(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::NOT_SIGNALED),
      builtin_camera_info_updated_(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::NOT_SIGNALED),
      external_camera_info_updated_(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::SIGNALED),
      has_camera_connected_(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED),
      num_builtin_cameras_(0),
      camera_buffer_factory_(new CameraBufferFactory()),
      ipc_task_runner_(std::move(ipc_task_runner)),
      camera_module_callbacks_(this),
      vendor_tag_ops_delegate_(ipc_task_runner_) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

CameraHalDelegate::~CameraHalDelegate() = default;

void CameraHalDelegate::RegisterCameraClient() {
  CameraHalDispatcherImpl::GetInstance()->AddClientObserver(
      std::make_unique<LocalCameraClientObserver>(this));
}

void CameraHalDelegate::SetCameraModule(
    mojo::PendingRemote<cros::mojom::CameraModule> camera_module) {
  ipc_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CameraHalDelegate::SetCameraModuleOnIpcThread,
                                this, std::move(camera_module)));
}

void CameraHalDelegate::Reset() {
  ipc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDelegate::ResetMojoInterfaceOnIpcThread, this));
}

std::unique_ptr<VideoCaptureDevice> CameraHalDelegate::CreateDevice(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_screen_observer,
    const VideoCaptureDeviceDescriptor& device_descriptor,
    CameraAppDeviceBridgeImpl* camera_app_device_bridge) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!UpdateBuiltInCameraInfo()) {
    return nullptr;
  }
  int camera_id = GetCameraIdFromDeviceId(device_descriptor.device_id);
  if (camera_id == -1) {
    LOG(ERROR) << "Invalid camera device: " << device_descriptor.device_id;
    return nullptr;
  }

  if (camera_app_device_bridge) {
    auto* camera_app_device = camera_app_device_bridge->GetCameraAppDevice(
        device_descriptor.device_id);
    // Since the cleanup callback will be triggered when VideoCaptureDevice died
    // and |camera_app_device_bridge| is actually owned by
    // VideoCaptureServiceImpl, it should be safe to assume
    // |camera_app_device_bridge| is still valid here.
    auto cleanup_callback = base::BindOnce(
        [](const std::string& device_id, CameraAppDeviceBridgeImpl* bridge) {
          bridge->OnDeviceClosed(device_id);
        },
        device_descriptor.device_id, camera_app_device_bridge);
    return std::make_unique<VideoCaptureDeviceChromeOSHalv3>(
        std::move(task_runner_for_screen_observer), device_descriptor, this,
        camera_app_device, std::move(cleanup_callback));
  } else {
    return std::make_unique<VideoCaptureDeviceChromeOSHalv3>(
        std::move(task_runner_for_screen_observer), device_descriptor, this,
        nullptr, base::DoNothing());
  }
}

void CameraHalDelegate::GetSupportedFormats(
    int camera_id,
    VideoCaptureFormats* supported_formats) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const cros::mojom::CameraInfoPtr& camera_info = camera_info_[camera_id];

  base::flat_set<int32_t> candidate_fps_set =
      GetAvailableFramerates(camera_info);

  const cros::mojom::CameraMetadataEntryPtr* min_frame_durations =
      GetMetadataEntry(camera_info->static_camera_characteristics,
                       cros::mojom::CameraMetadataTag::
                           ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS);
  if (!min_frame_durations) {
    LOG(ERROR)
        << "Failed to get available min frame durations from camera info";
    return;
  }
  // The min frame durations are stored as tuples of four int64s:
  // (hal_pixel_format, width, height, ns) x n
  const size_t kStreamFormatOffset = 0;
  const size_t kStreamWidthOffset = 1;
  const size_t kStreamHeightOffset = 2;
  const size_t kStreamDurationOffset = 3;
  const size_t kStreamDurationSize = 4;
  int64_t* iter =
      reinterpret_cast<int64_t*>((*min_frame_durations)->data.data());
  for (size_t i = 0; i < (*min_frame_durations)->count;
       i += kStreamDurationSize) {
    auto hal_format =
        static_cast<cros::mojom::HalPixelFormat>(iter[kStreamFormatOffset]);
    int32_t width = base::checked_cast<int32_t>(iter[kStreamWidthOffset]);
    int32_t height = base::checked_cast<int32_t>(iter[kStreamHeightOffset]);
    int64_t duration = iter[kStreamDurationOffset];
    iter += kStreamDurationSize;

    if (hal_format == cros::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_BLOB) {
      // Skip BLOB formats and use it only for TakePicture() since it's
      // inefficient to stream JPEG frames for CrOS camera HAL.
      continue;
    }

    if (duration <= 0) {
      LOG(ERROR) << "Ignoring invalid frame duration: " << duration;
      continue;
    }
    float max_fps = 1.0 * 1000000000LL / duration;

    // There's no consumer information here to determine the buffer usage, so
    // hard-code the usage that all the clients should be using.
    constexpr gfx::BufferUsage kClientBufferUsage =
        gfx::BufferUsage::SCANOUT_VEA_READ_CAMERA_AND_CPU_READ_WRITE;
    const ChromiumPixelFormat cr_format =
        camera_buffer_factory_->ResolveStreamBufferFormat(hal_format,
                                                          kClientBufferUsage);
    if (cr_format.video_format == PIXEL_FORMAT_UNKNOWN) {
      continue;
    }

    for (auto fps : candidate_fps_set) {
      if (fps > max_fps) {
        continue;
      }

      VLOG(1) << "Supported format: " << width << "x" << height
              << " fps=" << fps << " format=" << cr_format.video_format;
      supported_formats->emplace_back(gfx::Size(width, height), fps,
                                      cr_format.video_format);
    }
  }
}

void CameraHalDelegate::GetDevicesInfo(
    VideoCaptureDeviceFactory::GetDevicesInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!UpdateBuiltInCameraInfo()) {
    std::move(callback).Run({});
    return;
  }

  if (!external_camera_info_updated_.TimedWait(kEventWaitTimeoutSecs)) {
    LOG(ERROR) << "Failed to get camera info from all external cameras";
  }

  if (IsRunningOnVM() && IsVividLoaded()) {
    has_camera_connected_.TimedWait(kEventWaitTimeoutSecs);
  }

  std::vector<VideoCaptureDeviceInfo> devices_info;

  {
    base::AutoLock info_lock(camera_info_lock_);
    base::AutoLock id_map_lock(device_id_to_camera_id_lock_);
    for (const auto& it : camera_info_) {
      int camera_id = it.first;
      const cros::mojom::CameraInfoPtr& camera_info = it.second;
      if (!camera_info) {
        continue;
      }

      auto get_vendor_string = [&](const std::string& key) -> const char* {
        const VendorTagInfo* info = vendor_tag_ops_delegate_.GetInfoByName(key);
        if (info == nullptr) {
          return nullptr;
        }
        auto val = GetMetadataEntryAsSpan<char>(
            camera_info->static_camera_characteristics, info->tag);
        return val.empty() ? nullptr : val.data();
      };

      VideoCaptureDeviceDescriptor desc;
      desc.capture_api = VideoCaptureApi::ANDROID_API2_LIMITED;
      desc.transport_type = VideoCaptureTransportType::OTHER_TRANSPORT;
      switch (camera_info->facing) {
        case cros::mojom::CameraFacing::CAMERA_FACING_BACK:
          desc.facing = VideoFacingMode::MEDIA_VIDEO_FACING_ENVIRONMENT;
          desc.device_id = base::NumberToString(camera_id);
          desc.set_display_name("Back Camera");
          break;
        case cros::mojom::CameraFacing::CAMERA_FACING_FRONT:
          desc.facing = VideoFacingMode::MEDIA_VIDEO_FACING_USER;
          desc.device_id = base::NumberToString(camera_id);
          desc.set_display_name("Front Camera");
          break;
        case cros::mojom::CameraFacing::CAMERA_FACING_EXTERNAL: {
          desc.facing = VideoFacingMode::MEDIA_VIDEO_FACING_NONE;

          // The webcam_private api expects that |device_id| to be set as the
          // corresponding device path for external cameras used in GVC system.
          auto* path = get_vendor_string("com.google.usb.devicePath");
          desc.device_id =
              path != nullptr ? path : base::NumberToString(camera_id);

          auto* name = get_vendor_string("com.google.usb.modelName");
          desc.set_display_name(name != nullptr ? name : "External Camera");

          break;
          // Mojo validates the input parameters for us so we don't need to
          // worry about malformed values.
        }
      }
      auto* vid = get_vendor_string("com.google.usb.vendorId");
      auto* pid = get_vendor_string("com.google.usb.productId");
      if (vid != nullptr && pid != nullptr) {
        desc.model_id = base::StrCat({vid, ":", pid});
      }
      desc.set_control_support(GetControlSupport(camera_info));
      device_id_to_camera_id_[desc.device_id] = camera_id;
      devices_info.emplace_back(desc);
      GetSupportedFormats(camera_id, &devices_info.back().supported_formats);
    }
  }

  // TODO(shik): Report external camera first when lid is closed.
  // TODO(jcliang): Remove this after JS API supports query camera facing
  // (http://crbug.com/543997).
  std::sort(
      devices_info.begin(), devices_info.end(),
      [](const VideoCaptureDeviceInfo& a, const VideoCaptureDeviceInfo& b) {
        return a.descriptor < b.descriptor;
      });
  DVLOG(1) << "Number of devices: " << devices_info.size();

  std::move(callback).Run(std::move(devices_info));
}

VideoCaptureControlSupport CameraHalDelegate::GetControlSupport(
    const cros::mojom::CameraInfoPtr& camera_info) {
  VideoCaptureControlSupport control_support;

  auto is_vendor_range_valid = [&](const std::string& key) -> bool {
    const VendorTagInfo* info = vendor_tag_ops_delegate_.GetInfoByName(key);
    if (info == nullptr)
      return false;
    auto range = GetMetadataEntryAsSpan<int32_t>(
        camera_info->static_camera_characteristics, info->tag);
    return range.size() == 3 && range[0] < range[1];
  };

  if (is_vendor_range_valid("com.google.control.panRange"))
    control_support.pan = true;

  if (is_vendor_range_valid("com.google.control.tiltRange"))
    control_support.tilt = true;

  if (is_vendor_range_valid("com.google.control.zoomRange"))
    control_support.zoom = true;

  auto max_digital_zoom = GetMetadataEntryAsSpan<float>(
      camera_info->static_camera_characteristics,
      cros::mojom::CameraMetadataTag::
          ANDROID_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM);
  if (max_digital_zoom.size() == 1 && max_digital_zoom[0] > 1) {
    control_support.zoom = true;
  }

  return control_support;
}

cros::mojom::CameraInfoPtr CameraHalDelegate::GetCameraInfoFromDeviceId(
    const std::string& device_id) {
  base::AutoLock lock(camera_info_lock_);
  int camera_id = GetCameraIdFromDeviceId(device_id);
  if (camera_id == -1) {
    return {};
  }
  auto it = camera_info_.find(camera_id);
  if (it == camera_info_.end()) {
    return {};
  }
  return it->second.Clone();
}

const VendorTagInfo* CameraHalDelegate::GetVendorTagInfoByName(
    const std::string& full_name) {
  return vendor_tag_ops_delegate_.GetInfoByName(full_name);
}

void CameraHalDelegate::OpenDevice(
    int32_t camera_id,
    mojo::PendingReceiver<cros::mojom::Camera3DeviceOps> device_ops_receiver,
    OpenDeviceCallback callback) {
  DCHECK(!ipc_task_runner_->BelongsToCurrentThread());
  // This method may be called on any thread except |ipc_task_runner_|.
  // Currently this method is used by CameraDeviceDelegate to open a camera
  // device.
  camera_module_has_been_set_.Wait();
  ipc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDelegate::OpenDeviceOnIpcThread, this, camera_id,
                     std::move(device_ops_receiver), std::move(callback)));
}

int CameraHalDelegate::GetCameraIdFromDeviceId(const std::string& device_id) {
  base::AutoLock lock(device_id_to_camera_id_lock_);
  auto it = device_id_to_camera_id_.find(device_id);
  if (it == device_id_to_camera_id_.end()) {
    return -1;
  }
  return it->second;
}

void CameraHalDelegate::SetCameraModuleOnIpcThread(
    mojo::PendingRemote<cros::mojom::CameraModule> camera_module) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  if (camera_module_.is_bound()) {
    LOG(ERROR) << "CameraModule is already bound";
    return;
  }
  if (camera_module.is_valid()) {
    camera_module_.Bind(std::move(camera_module));
    camera_module_.set_disconnect_handler(base::BindOnce(
        &CameraHalDelegate::ResetMojoInterfaceOnIpcThread, this));
  }
  camera_module_has_been_set_.Signal();
}

void CameraHalDelegate::ResetMojoInterfaceOnIpcThread() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  camera_module_.reset();
  camera_module_callbacks_.reset();
  vendor_tag_ops_delegate_.Reset();
  builtin_camera_info_updated_.Reset();
  camera_module_has_been_set_.Reset();
  has_camera_connected_.Reset();
  external_camera_info_updated_.Signal();

  // Clear all cached camera info, especially external cameras.
  camera_info_.clear();
  pending_external_camera_info_.clear();
}

bool CameraHalDelegate::UpdateBuiltInCameraInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!ipc_task_runner_->BelongsToCurrentThread());

  camera_module_has_been_set_.Wait();
  if (builtin_camera_info_updated_.IsSignaled()) {
    return true;
  }
  // The built-in camera are static per specification of the Android camera HAL
  // v3 specification.  We only update the built-in camera info once.
  ipc_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraHalDelegate::UpdateBuiltInCameraInfoOnIpcThread,
                     this));
  if (!builtin_camera_info_updated_.TimedWait(kEventWaitTimeoutSecs)) {
    LOG(ERROR) << "Timed out getting camera info";
    return false;
  }
  return true;
}

void CameraHalDelegate::UpdateBuiltInCameraInfoOnIpcThread() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  camera_module_->GetNumberOfCameras(base::BindOnce(
      &CameraHalDelegate::OnGotNumberOfCamerasOnIpcThread, this));
}

void CameraHalDelegate::OnGotNumberOfCamerasOnIpcThread(int32_t num_cameras) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  if (num_cameras < 0) {
    builtin_camera_info_updated_.Signal();
    LOG(ERROR) << "Failed to get number of cameras: " << num_cameras;
    return;
  }
  VLOG(1) << "Number of built-in cameras: " << num_cameras;
  num_builtin_cameras_ = num_cameras;
  // Per camera HAL v3 specification SetCallbacks() should be called after the
  // first time GetNumberOfCameras() is called, and before other CameraModule
  // functions are called.
  camera_module_->SetCallbacks(
      camera_module_callbacks_.BindNewPipeAndPassRemote(),
      base::BindOnce(&CameraHalDelegate::OnSetCallbacksOnIpcThread, this));

  camera_module_->GetVendorTagOps(
      vendor_tag_ops_delegate_.MakeReceiver(),
      base::BindOnce(&CameraHalDelegate::OnGotVendorTagOpsOnIpcThread, this));
}

void CameraHalDelegate::OnSetCallbacksOnIpcThread(int32_t result) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  if (result) {
    num_builtin_cameras_ = 0;
    builtin_camera_info_updated_.Signal();
    LOG(ERROR) << "Failed to set camera module callbacks: "
               << base::safe_strerror(-result);
    return;
  }

  if (num_builtin_cameras_ == 0) {
    builtin_camera_info_updated_.Signal();
    return;
  }

  for (size_t camera_id = 0; camera_id < num_builtin_cameras_; ++camera_id) {
    GetCameraInfoOnIpcThread(
        camera_id,
        base::BindOnce(&CameraHalDelegate::OnGotCameraInfoOnIpcThread, this,
                       camera_id));
  }
}

void CameraHalDelegate::OnGotVendorTagOpsOnIpcThread() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  vendor_tag_ops_delegate_.Initialize();
}

void CameraHalDelegate::GetCameraInfoOnIpcThread(
    int32_t camera_id,
    GetCameraInfoCallback callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  camera_module_->GetCameraInfo(camera_id, std::move(callback));
}

void CameraHalDelegate::OnGotCameraInfoOnIpcThread(
    int32_t camera_id,
    int32_t result,
    cros::mojom::CameraInfoPtr camera_info) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << "Got camera info of camera " << camera_id;
  if (result) {
    LOG(ERROR) << "Failed to get camera info. Camera id: " << camera_id;
  }
  SortCameraMetadata(&camera_info->static_camera_characteristics);

  base::AutoLock lock(camera_info_lock_);
  camera_info_[camera_id] = std::move(camera_info);

  if (camera_id < base::checked_cast<int32_t>(num_builtin_cameras_)) {
    // |camera_info_| might contain some entries for external cameras as well,
    // we should check all built-in cameras explicitly.
    bool all_updated = [&]() {
      for (size_t i = 0; i < num_builtin_cameras_; i++) {
        if (camera_info_.find(i) == camera_info_.end()) {
          return false;
        }
      }
      return true;
    }();

    if (all_updated) {
      builtin_camera_info_updated_.Signal();
    }
  } else {
    // It's an external camera.
    pending_external_camera_info_.erase(camera_id);
    if (pending_external_camera_info_.empty()) {
      external_camera_info_updated_.Signal();
    }
    NotifyVideoCaptureDevicesChanged();
  }

  if (camera_info_.size() == 1) {
    has_camera_connected_.Signal();
  }
}

void CameraHalDelegate::OpenDeviceOnIpcThread(
    int32_t camera_id,
    mojo::PendingReceiver<cros::mojom::Camera3DeviceOps> device_ops_receiver,
    OpenDeviceCallback callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  camera_module_->OpenDevice(camera_id, std::move(device_ops_receiver),
                             std::move(callback));
}

// CameraModuleCallbacks implementations.
void CameraHalDelegate::CameraDeviceStatusChange(
    int32_t camera_id,
    cros::mojom::CameraDeviceStatus new_status) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  VLOG(1) << "camera_id = " << camera_id << ", new_status = " << new_status;
  base::AutoLock lock(camera_info_lock_);
  auto it = camera_info_.find(camera_id);
  switch (new_status) {
    case cros::mojom::CameraDeviceStatus::CAMERA_DEVICE_STATUS_PRESENT:
      if (it == camera_info_.end()) {
        // Get info for the newly connected external camera.
        // |has_camera_connected_| might be signaled in
        // OnGotCameraInfoOnIpcThread().
        pending_external_camera_info_.insert(camera_id);
        if (pending_external_camera_info_.size() == 1) {
          external_camera_info_updated_.Reset();
        }
        GetCameraInfoOnIpcThread(
            camera_id,
            base::BindOnce(&CameraHalDelegate::OnGotCameraInfoOnIpcThread, this,
                           camera_id));
      } else {
        LOG(WARNING) << "Ignore duplicated camera_id = " << camera_id;
      }
      break;
    case cros::mojom::CameraDeviceStatus::CAMERA_DEVICE_STATUS_NOT_PRESENT:
      if (it != camera_info_.end()) {
        camera_info_.erase(it);
        if (camera_info_.empty()) {
          has_camera_connected_.Reset();
        }
        NotifyVideoCaptureDevicesChanged();
      } else {
        LOG(WARNING) << "Ignore nonexistent camera_id = " << camera_id;
      }
      break;
    default:
      NOTREACHED() << "Unexpected new status " << new_status;
  }
}

void CameraHalDelegate::TorchModeStatusChange(
    int32_t camera_id,
    cros::mojom::TorchModeStatus new_status) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  // Do nothing here as we don't care about torch mode status.
}

}  // namespace media
