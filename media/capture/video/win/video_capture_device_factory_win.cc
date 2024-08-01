// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/capture/video/win/video_capture_device_factory_win.h"

#include <objbase.h>

#include <mfapi.h>
#include <mferror.h>
#include <stddef.h>
#include <windows.devices.enumeration.h>
#include <windows.foundation.collections.h>
#include <wrl.h>
#include <wrl/client.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/strings/cstring_view.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/system/system_monitor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_variant.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "media/base/media_switches.h"
#include "media/base/win/mf_helpers.h"
#include "media/base/win/mf_initializer.h"
#include "media/capture/capture_switches.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "media/capture/video/video_capture_metrics.h"
#include "media/capture/video/win/metrics.h"
#include "media/capture/video/win/video_capture_device_mf_win.h"
#include "media/capture/video/win/video_capture_device_win.h"

using DevicesInfo = std::vector<media::VideoCaptureDeviceInfo>;
using base::win::GetActivationFactory;
using base::win::ScopedCoMem;
using base::win::ScopedHString;
using base::win::ScopedVariant;
using Microsoft::WRL::ComPtr;

namespace media {

BASE_FEATURE(kMediaFoundationD3D11VideoCaptureBlocklist,
             "MediaFoundationD3D11VideoCaptureBlocklist",
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

// In Windows device identifiers, the USB VID and PID are preceded by the string
// "vid_" or "pid_".  The identifiers are each 4 bytes long.
const char kVidPrefix[] = "vid_";  // Also contains '\0'.
const char kPidPrefix[] = "pid_";  // Also contains '\0'.
const size_t kVidPidSize = 4;

// AQS device selector string to filter enumerated DeviceInformation objects to
// KSCATEGORY_SENSOR_CAMERA (Class GUID 24e552d7-6523-47F7-a647-d3465bf1f5ca)
// OR KSCATEGORY_VIDEO_CAMERA (Class GUID e5323777-f976-4f5b-9b55-b94699c46e44).
const wchar_t* kVideoAndSensorCamerasAqsString =
    L"(System.Devices.InterfaceClassGuid:="
    L"\"{e5323777-f976-4f5b-9b55-b94699c46e44}\" AND "
    L"(System.Devices.WinPhone8CameraFlags:=[] OR "
    L"System.Devices.WinPhone8CameraFlags:<4096)) OR "
    L"System.Devices.InterfaceClassGuid:="
    L"\"{24e552d7-6523-47f7-a647-d3465bf1f5ca}\" AND "
    L"System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True";

// Class GUID for KSCATEGORY_VIDEO_CAMERA. Only devices from that category will
// contain this GUID in their |device_id|.
const char kVideoCameraGuid[] = "e5323777-f976-4f5b-9b55-b94699c46e44";

// Avoid enumerating and/or using certain devices due to they provoking crashes
// or any other reason (http://crbug.com/378494). This enum is defined for the
// purposes of UMA collection. Existing entries cannot be removed.
enum BlockedCameraNames {
  BLOCKED_CAMERA_GOOGLE_CAMERA_ADAPTER = 0,
  BLOCKED_CAMERA_IP_CAMERA = 1,
  BLOCKED_CAMERA_CYBERLINK_WEBCAM_SPLITTER = 2,
  BLOCKED_CAMERA_EPOCCAM = 3,
  // This one must be last, and equal to the previous enumerated value.
  BLOCKED_CAMERA_MAX = BLOCKED_CAMERA_EPOCCAM,
};

#define UWP_ENUM_ERROR_HANDLER(hr, err_log)                         \
  DLOG(WARNING) << err_log << logging::SystemErrorCodeToString(hr); \
  origin_task_runner_->PostTask(                                    \
      FROM_HERE, base::BindOnce(std::move(device_info_callback), nullptr))

// Blocked devices are identified by a characteristic prefix of the name.
// This prefix is used case-insensitively. This list must be kept in sync with
// |BlockedCameraNames|.
const char* const kBlockedCameraNames[] = {
    // Name of a fake DirectShow filter on computers with GTalk installed.
    "Google Camera Adapter",
    // The following software WebCams cause crashes.
    "IP Camera [JPEG/MJPEG]",
    "CyberLink Webcam Splitter",
    "EpocCam",
};
static_assert(std::size(kBlockedCameraNames) == BLOCKED_CAMERA_MAX + 1,
              "kBlockedCameraNames should be same size as "
              "BlockedCameraNames enum");

// Use this list only for USB webcams.
constexpr auto kModelIdsBlockedForMediaFoundation =
    base::MakeFixedFlatSet<std::string_view>(
        {// Devices using Empia 2860 or 2820 chips, see
         // https://crbug.com/849636.
         "eb1a:2860", "eb1a:2820", "1ce6:2820",
         // Elgato HD60 Pro
         "12ab:0380",
         // Sensoray 2253
         "1943:2253",
         // Dell E5440
         "0c45:64d0", "0c45:64d2",
         // Dell E7440
         "1bcf:2985",
         // Lenovo Thinkpad Model 20CG0006FMZ front and rear cameras, see
         // also https://crbug.com/924528.
         "04ca:7047", "04ca:7048",
         // HP Elitebook 840 G1
         "04f2:b3ed", "04f2:b3ca", "05c8:035d", "05c8:0369",
         // HP HD Camera. See https://crbug.com/1011888.
         "04ca:7095",
         // RBG/IR camera for Windows Hello Face Auth. See
         // https://crbug.com/984864.
         "13d3:5257",
         // Acer Aspire f5-573g. See https://crbug.com/1034644.
         "0bda:57f2",
         // Elgato Camlink 4k
         "0fd9:0066",
         // ACER Aspire VN7-571G. See https://crbug.com/1327948.
         "04f2:b469",
         // Hauppauge USB-Live2. See https://crbug.com/1447113.
         "2040:c200"});

// Use this list only for USB webcams.
constexpr auto kModelIdsBlockedForMediaFoundationD3D11VideoCapture =
    base::MakeFixedFlatSet<std::string_view>(
        {// D3D11 calls on textures produced by these cameras take so much time
         // that MFCaptureEngine fails with E_MF_SAMPLEALLOCATOREMPTY error
         "05a3:9331", "04f2:b6bf"});

// Use this list only for non-USB webcams.
constexpr auto kDisplayNamesBlockedForMediaFoundation =
    base::MakeFixedFlatSet<std::string_view>(
        {// VMware Virtual Webcams cause hangs when there is no physical Webcam.
         // See https://crbug.com/1044974.
         "VMware Virtual Webcam"});

const std::vector<
    std::pair<VideoCaptureApi, std::vector<std::pair<GUID, GUID>>>>&
GetMFAttributes() {
  if (base::FeatureList::IsEnabled(
          media::kIncludeIRCamerasInDeviceEnumeration)) {
    static const base::NoDestructor<std::vector<
        std::pair<VideoCaptureApi, std::vector<std::pair<GUID, GUID>>>>>
        mf_attributes({{{VideoCaptureApi::WIN_MEDIA_FOUNDATION,
                         {
                             {MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                              MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID},
                         }},
                        {VideoCaptureApi::WIN_MEDIA_FOUNDATION_SENSOR,
                         {{MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                           MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID},
                          {MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_CATEGORY,
                           KSCATEGORY_SENSOR_CAMERA}}}}});
    return *mf_attributes;
  }

  static const base::NoDestructor<std::vector<
      std::pair<VideoCaptureApi, std::vector<std::pair<GUID, GUID>>>>>
      mf_attributes({{VideoCaptureApi::WIN_MEDIA_FOUNDATION,
                      {
                          {MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                           MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID},
                      }}});
  return *mf_attributes;
}

bool IsDeviceBlockedForQueryingDetailedFrameRates(
    const std::string& display_name) {
  return display_name.find("WebcamMax") != std::string::npos;
}

bool IsDeviceBlockedForMediaFoundationByModelId(const std::string& model_id) {
  return kModelIdsBlockedForMediaFoundation.contains(model_id);
}

bool IsDeviceBlockedForMediaFoundationD3D11ByModelId(
    const std::string& model_id) {
  return base::FeatureList::IsEnabled(
             kMediaFoundationD3D11VideoCaptureBlocklist) &&
         kModelIdsBlockedForMediaFoundationD3D11VideoCapture.contains(model_id);
}

bool IsDeviceBlockedForMediaFoundationByDisplayName(
    const std::string& display_name) {
  return kDisplayNamesBlockedForMediaFoundation.contains(display_name);
}

HMODULE ExpandEnvironmentStringsAndLoadLibrary(base::wcstring_view path) {
  auto expanded_path = base::win::ExpandEnvironmentVariables(path);
  if (!expanded_path) {
    return nullptr;
  }

  return LoadLibraryExW(expanded_path->c_str(), nullptr,
                        LOAD_WITH_ALTERED_SEARCH_PATH);
}

bool LoadMediaFoundationDlls() {
  static constexpr base::wcstring_view kMfDLLs[] = {
      L"%WINDIR%\\system32\\mf.dll", L"%WINDIR%\\system32\\mfplat.dll",
      L"%WINDIR%\\system32\\mfreadwrite.dll",
      L"%WINDIR%\\system32\\MFCaptureEngine.dll"};

  // Mitigate the issues caused by loading DLLs on a background thread
  // (http://crbug/973868).
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY_REPEATEDLY();

  // Load required DLLs.
  for (const auto& kMfDLL : kMfDLLs) {
    if (!ExpandEnvironmentStringsAndLoadLibrary(kMfDLL)) {
      return false;
    }
  }

  // Load optional DLLs whose availability depends on Windows version.
  ExpandEnvironmentStringsAndLoadLibrary(
      L"%WINDIR%\\system32\\mfsensorgroup.dll");

  return true;
}

bool PrepareVideoCaptureAttributesMediaFoundation(
    const std::vector<std::pair<GUID, GUID>>& attributes_data,
    int count,
    IMFAttributes** attributes) {
  DCHECK(attributes);
  DCHECK(!*attributes);

  // Once https://bugs.chromium.org/p/chromium/issues/detail?id=791615 is fixed,
  // we must make sure that this method succeeds in capture_unittests context
  // when MediaFoundation is enabled.
  if (!VideoCaptureDeviceFactoryWin::PlatformSupportsMediaFoundation())
    return false;

  if (FAILED(MFCreateAttributes(attributes, count)))
    return false;

  for (const auto& value : attributes_data) {
    if (!SUCCEEDED((*attributes)->SetGUID(value.first, value.second)))
      return false;
  }
  return true;
}

bool IsDeviceBlocked(const std::string& name) {
  DCHECK_EQ(BLOCKED_CAMERA_MAX + 1,
            static_cast<int>(std::size(kBlockedCameraNames)));
  for (size_t i = 0; i < std::size(kBlockedCameraNames); ++i) {
    if (base::StartsWith(name, kBlockedCameraNames[i],
                         base::CompareCase::INSENSITIVE_ASCII)) {
      DVLOG(1) << "Enumerated blocked device: " << name;
      return true;
    }
  }
  return false;
}

std::string GetDeviceModelId(const std::string& device_id) {
  const size_t vid_prefix_size = sizeof(kVidPrefix) - 1;
  const size_t pid_prefix_size = sizeof(kPidPrefix) - 1;
  const size_t vid_location = device_id.find(kVidPrefix);
  if (vid_location == std::string::npos ||
      vid_location + vid_prefix_size + kVidPidSize > device_id.size()) {
    return std::string();
  }
  const size_t pid_location = device_id.find(kPidPrefix);
  if (pid_location == std::string::npos ||
      pid_location + pid_prefix_size + kVidPidSize > device_id.size()) {
    return std::string();
  }
  const std::string id_vendor =
      device_id.substr(vid_location + vid_prefix_size, kVidPidSize);
  const std::string id_product =
      device_id.substr(pid_location + pid_prefix_size, kVidPidSize);
  return id_vendor + ":" + id_product;
}

bool DevicesInfoContainsDeviceId(const DevicesInfo& devices_info,
                                 const std::string& device_id) {
  return base::Contains(devices_info, device_id,
                        [](const VideoCaptureDeviceInfo& device_info) {
                          return device_info.descriptor.device_id;
                        });
}

// Returns a non DirectShow descriptor DevicesInfo with the provided name and
// model.
DevicesInfo::const_iterator FindNonDirectShowDeviceInfoByNameAndModel(
    const DevicesInfo& devices_info,
    const std::string& name_and_model) {
  return base::ranges::find_if(
      devices_info,
      [name_and_model](const VideoCaptureDeviceInfo& device_info) {
        return device_info.descriptor.capture_api !=
                   VideoCaptureApi::WIN_DIRECT_SHOW &&
               name_and_model == device_info.descriptor.GetNameAndModel();
      });
}

void FindAndSetDefaultVideoCamera(
    std::vector<VideoCaptureDeviceInfo>* devices_info) {
  // When available, the default video camera should be external with
  // MEDIA_VIDEO_FACING_NONE. Otherwise, it should be internal with
  // MEDIA_VIDEO_FACING_USER. It occupies the first index in |devices_info|.
  for (auto it = devices_info->begin(); it != devices_info->end(); ++it) {
    // Default video camera belongs to KSCATEGORY_VIDEO_CAMERA.
    if (it->descriptor.device_id.find(kVideoCameraGuid) != std::string::npos) {
      if (it->descriptor.facing == VideoFacingMode::MEDIA_VIDEO_FACING_NONE) {
        std::iter_swap(devices_info->begin(), it);
        break;  // Stop iterating once an external video camera is found.
      } else if (it->descriptor.facing ==
                 VideoFacingMode::MEDIA_VIDEO_FACING_USER) {
        std::iter_swap(devices_info->begin(), it);
      }
    }
  }
}

}  // namespace

class VideoCaptureDeviceFactoryWin::ComThreadData
    : public base::RefCountedThreadSafe<ComThreadData> {
 public:
  ComThreadData(base::WeakPtr<VideoCaptureDeviceFactoryWin> device_factory,
                scoped_refptr<base::SingleThreadTaskRunner> com_thread_runner,
                scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner)
      : device_factory_(std::move(device_factory)),
        com_thread_runner_(std::move(com_thread_runner)),
        origin_task_runner_(std::move(origin_task_runner)) {}

  void EnumerateDevicesUWP(std::vector<VideoCaptureDeviceInfo> devices_info,
                           GetDevicesInfoCallback result_callback);

  void FoundAllDevicesUWP(
      std::vector<VideoCaptureDeviceInfo> devices_info,
      GetDevicesInfoCallback result_callback,
      IAsyncOperation<DeviceInformationCollection*>* operation);

 private:
  friend class base::RefCountedThreadSafe<ComThreadData>;
  ~ComThreadData() = default;

  std::unordered_set<
      raw_ptr<IAsyncOperation<DeviceInformationCollection*>, CtnExperimental>>
      async_ops_;
  base::WeakPtr<VideoCaptureDeviceFactoryWin> device_factory_;
  scoped_refptr<base::SingleThreadTaskRunner> com_thread_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;
};

class VideoCaptureDeviceFactoryWin::UsageReportHandler
    : public base::RefCountedThreadSafe<UsageReportHandler>,
      public IMFSensorActivitiesReportCallback {
 public:
  UsageReportHandler() : my_pid_(base::GetCurrentProcId()) {}

  // IUnknown
  IFACEMETHODIMP QueryInterface(REFIID riid, void** object) override {
    HRESULT hr = E_NOINTERFACE;
    if (riid == IID_IUnknown) {
      *object = this;
      hr = S_OK;
    } else if (riid == IID_IMFSensorActivitiesReportCallback) {
      *object = static_cast<IMFSensorActivitiesReportCallback*>(this);
      hr = S_OK;
    }
    if (SUCCEEDED(hr)) {
      AddRef();
    }

    return hr;
  }

  IFACEMETHODIMP_(ULONG) AddRef() override {
    base::RefCountedThreadSafe<UsageReportHandler>::AddRef();
    return 1U;
  }

  IFACEMETHODIMP_(ULONG) Release() override {
    base::RefCountedThreadSafe<UsageReportHandler>::Release();
    return 1U;
  }

  // IMFSensorActivitiesReportCallback
  IFACEMETHODIMP_(HRESULT)
  OnActivitiesReport(IMFSensorActivitiesReport* report) override {
    ULONG num_reports;
    std::map<std::string, CameraAvailability> report_availabilities;
    RETURN_IF_FAILED(report->GetCount(&num_reports));
    for (ULONG i = 0; i < num_reports; i++) {
      ComPtr<IMFSensorActivityReport> activity_report;
      WCHAR symbolic_name[1000] = L"";
      ULONG num_written;
      ULONG process_count;
      bool got_activity_report =
          SUCCEEDED(report->GetActivityReport(i, &activity_report)) &&
          SUCCEEDED(activity_report->GetSymbolicLink(symbolic_name, 1000,
                                                     &num_written)) &&
          SUCCEEDED(activity_report->GetProcessCount(&process_count));
      if (!got_activity_report) {
        continue;
      }
      std::optional<CameraAvailability> availability;
      for (ULONG j = 0; j < process_count; j++) {
        ComPtr<IMFSensorProcessActivity> process_activity;
        ULONG pid;
        BOOL is_streaming;
        bool got_process_info =
            SUCCEEDED(
                activity_report->GetProcessActivity(j, &process_activity)) &&
            SUCCEEDED(process_activity->GetProcessId(&pid)) &&
            SUCCEEDED(process_activity->GetStreamingState(&is_streaming));
        if (!got_process_info) {
          continue;
        }
        if (pid == my_pid_) {
          if (is_streaming) {
            // If this process is using the camera, it is known to be available.
            // No need to look at other processes.
            availability = CameraAvailability::kAvailable;
            break;
          }
        } else {
          if (is_streaming) {
            // If another process is using the camera, it is known to be
            // unavailable. No need to look at other processes.
            availability = CameraAvailability::
                kUnavailableExclusivelyUsedByOtherApplication;
            break;
          }
          // If another process is not using the camera, it might be available,
          // but need to continue looking at other processes.
          availability = CameraAvailability::kAvailable;
        }
      }
      if (!availability.has_value()) {
        continue;
      }
      std::string device_id = base::SysWideToUTF8(symbolic_name);
      std::transform(device_id.begin(), device_id.end(), device_id.begin(),
                     ::tolower);
      // It has been observed that different activity reports in the same
      // notification and for the same device can be contradictory. For example,
      // activity report 0 for device D can say process P (not self) is not
      // streaming, and activity report 1 for the same device D can say that the
      // same process P is streaming. In this case, experience shows that
      // process P is indeed streaming and the camera is unavailable.
      // To avoid replacing a correct unavailable state with an incorrect
      // available state from another report, only update the map if there is
      // no entry yet for the device or if the new state is that the device is
      // unavailable. See https://crbug.com/325590346.
      if ((report_availabilities.find(device_id) ==
           report_availabilities.end()) ||
          (*availability ==
           CameraAvailability::kUnavailableExclusivelyUsedByOtherApplication)) {
        report_availabilities[device_id] = *availability;
      }
    }

    if (report_availabilities.empty()) {
      // No state updates. Return.
      return S_OK;
    }

    UpdateAvailabilityCache(report_availabilities);
    return S_OK;
  }

  void UpdateDevicesInfoAvailability(
      std::vector<VideoCaptureDeviceInfo>* devices_info) {
    base::AutoLock lock(cache_lock_);
    std::set<std::string> device_ids;
    for (auto& info : *devices_info) {
      device_ids.insert(info.descriptor.device_id);
      auto it = availability_cache_.find(info.descriptor.device_id);
      if (it != availability_cache_.end()) {
        info.descriptor.availability = it->second;
      }
    }
    std::erase_if(availability_cache_, [&device_ids](const auto& entry) {
      return !base::Contains(device_ids, entry.first);
    });
  }

 protected:
  friend class base::RefCountedThreadSafe<UsageReportHandler>;
  virtual ~UsageReportHandler() = default;

 private:
  void UpdateAvailabilityCache(
      const std::map<std::string, CameraAvailability>& report_availabilities) {
    bool should_invoke_system_monitor = false;
    {
      base::AutoLock lock(cache_lock_);
      for (const auto& [device_id, availability] : report_availabilities) {
        auto it = availability_cache_.find(device_id);
        if (it == availability_cache_.end()) {
          availability_cache_[device_id] = availability;
          should_invoke_system_monitor = true;
        } else if (it->second != availability) {
          it->second = availability;
          should_invoke_system_monitor = true;
        }
      }
    }
    if (should_invoke_system_monitor) {
      if (auto* system_monitor = base::SystemMonitor::Get()) {
        system_monitor->ProcessDevicesChanged(
            base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
      }
    }
  }

  const base::ProcessId my_pid_;
  base::Lock cache_lock_;
  std::map<std::string, media::CameraAvailability> availability_cache_
      GUARDED_BY(cache_lock_);
};

// Returns true if the current platform supports the Media Foundation API
// and that the DLLs are available.  On Vista this API is an optional download
// but the API is advertised as a part of Windows 7 and onwards.  However,
// we've seen that the required DLLs are not available in some Win7
// distributions such as Windows 7 N and Windows 7 KN.
// static
bool VideoCaptureDeviceFactoryWin::PlatformSupportsMediaFoundation() {
  static const bool has_media_foundation =
      InitializeMediaFoundation() && LoadMediaFoundationDlls();
  return has_media_foundation;
}

VideoCaptureDeviceFactoryWin::VideoCaptureDeviceFactoryWin()
    : use_media_foundation_(
          base::FeatureList::IsEnabled(media::kMediaFoundationVideoCapture)),
      use_d3d11_with_media_foundation_(
          media::IsMediaFoundationD3D11VideoCaptureEnabled() &&
          switches::IsVideoCaptureUseGpuMemoryBufferEnabled()),
      com_thread_("Windows Video Capture COM Thread") {
  if (use_media_foundation_ && !PlatformSupportsMediaFoundation()) {
    use_media_foundation_ = false;
  }
  if (use_media_foundation_ &&
      switches::IsMediaFoundationCameraUsageMonitoringEnabled()) {
    CreateUsageMonitorAndReportHandler();
  }
}

VideoCaptureDeviceFactoryWin::~VideoCaptureDeviceFactoryWin() {
  if (monitor_) {
    monitor_->Stop();
  }
  if (com_thread_.IsRunning()) {
    com_thread_.Stop();
  }
}

VideoCaptureErrorOrDevice VideoCaptureDeviceFactoryWin::CreateDevice(
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  DCHECK(thread_checker_.CalledOnValidThread());
  UMA_HISTOGRAM_ENUMERATION("Media.VideoCapture.Win.DeviceFactory.CaptureApi",
                            device_descriptor.capture_api);

  switch (device_descriptor.capture_api) {
    case VideoCaptureApi::WIN_MEDIA_FOUNDATION:
      [[fallthrough]];
    case VideoCaptureApi::WIN_MEDIA_FOUNDATION_SENSOR: {
      DCHECK(PlatformSupportsMediaFoundation());
      ComPtr<IMFMediaSource> source;
      const bool banned_for_d3d11 =
          IsDeviceBlockedForMediaFoundationD3D11ByModelId(
              GetDeviceModelId(device_descriptor.device_id));

      MFSourceOutcome outcome = CreateDeviceSourceMediaFoundation(
          device_descriptor.device_id, device_descriptor.capture_api,
          banned_for_d3d11, &source);
      switch (outcome) {
        case MFSourceOutcome::kSuccess: {
          auto device = std::make_unique<VideoCaptureDeviceMFWin>(
              device_descriptor, std::move(source),
              banned_for_d3d11 ? nullptr : dxgi_device_manager_,
              base::SingleThreadTaskRunner::GetCurrentDefault());
          DVLOG(1) << " MediaFoundation Device: "
                   << device_descriptor.display_name();
          if (device->Init()) {
            LogCaptureDeviceHashedModelId(device_descriptor);
            return VideoCaptureErrorOrDevice(std::move(device));
          }
          return VideoCaptureErrorOrDevice(
              VideoCaptureError::kWinMediaFoundationDeviceInitializationFailed);
        }
        case MFSourceOutcome::kFailedSystemPermissions:
          return VideoCaptureErrorOrDevice(
              VideoCaptureError::kWinMediaFoundationSystemPermissionDenied);
        case MFSourceOutcome::kFailed:
          return VideoCaptureErrorOrDevice(
              VideoCaptureError::kWinMediaFoundationSourceCreationFailed);
      }
      NOTREACHED_IN_MIGRATION();
      break;
    }
    case VideoCaptureApi::WIN_DIRECT_SHOW: {
      ComPtr<IBaseFilter> capture_filter;
      if (!CreateDeviceFilterDirectShow(device_descriptor.device_id,
                                        &capture_filter)) {
        return VideoCaptureErrorOrDevice(
            VideoCaptureError::kWinDirectShowDeviceFilterCreationFailed);
      }
      auto device = std::make_unique<VideoCaptureDeviceWin>(
          device_descriptor, std::move(capture_filter));
      DVLOG(1) << " DirectShow Device: " << device_descriptor.display_name();
      if (device->Init()) {
        LogCaptureDeviceHashedModelId(device_descriptor);
        return VideoCaptureErrorOrDevice(std::move(device));
      }
      return VideoCaptureErrorOrDevice(
          VideoCaptureError::kWinDirectShowDeviceInitializationFailed);
    }
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return VideoCaptureErrorOrDevice(
      VideoCaptureError::kVideoCaptureDeviceFactoryWinUnknownError);
}

bool VideoCaptureDeviceFactoryWin::CreateDeviceEnumMonikerDirectShow(
    IEnumMoniker** enum_moniker) {
  DCHECK(enum_moniker);
  DCHECK(!*enum_moniker);

  // Mitigate the issues caused by loading DLLs on a background thread
  // (http://crbug/973868).
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

  ComPtr<ICreateDevEnum> dev_enum;
  HRESULT hr = ::CoCreateInstance(CLSID_SystemDeviceEnum, nullptr,
                                  CLSCTX_INPROC, IID_PPV_ARGS(&dev_enum));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create system device enumerator: "
                << logging::SystemErrorCodeToString(hr);
    return false;
  }

  hr = dev_enum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,
                                       enum_moniker, 0);
  // CreateClassEnumerator returns S_FALSE on some Windows OS
  // when no camera exist. Therefore the FAILED macro can't be used.
  if (hr != S_OK) {
    DLOG(ERROR) << "Failed to create video input device enum moniker: "
                << logging::SystemErrorCodeToString(hr);
    return false;
  }

  return true;
}

bool VideoCaptureDeviceFactoryWin::CreateDeviceFilterDirectShow(
    const std::string& device_id,
    IBaseFilter** capture_filter) {
  DCHECK(capture_filter);
  DCHECK(!*capture_filter);

  ComPtr<IEnumMoniker> enum_moniker;
  if (!CreateDeviceEnumMonikerDirectShow(&enum_moniker))
    return false;

  HRESULT hr = S_OK;
  for (ComPtr<IMoniker> moniker;
       enum_moniker->Next(1, &moniker, nullptr) == S_OK; moniker.Reset()) {
    ComPtr<IPropertyBag> prop_bag;
    hr = moniker->BindToStorage(0, 0, IID_PPV_ARGS(&prop_bag));
    if (FAILED(hr))
      continue;

    // Find |device_id| via DevicePath, Description or FriendlyName, whichever
    // is available first and is a VT_BSTR (i.e. String) type.
    static const wchar_t* kPropertyNames[] = {L"DevicePath", L"Description",
                                              L"FriendlyName"};

    ScopedVariant name;
    for (const auto* property_name : kPropertyNames) {
      prop_bag->Read(property_name, name.Receive(), 0);
      if (name.type() != VT_BSTR)
        continue;  // Continue to the next property.
      const std::string device_path(base::SysWideToUTF8(V_BSTR(name.ptr())));
      if (device_path.compare(device_id) != 0)
        break;  // Continue to the next moniker.
      // We have found the requested device.
      return CreateDeviceFilterDirectShow(std::move(moniker), capture_filter);
    }
  }

  if (SUCCEEDED(hr))
    hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
  DLOG(ERROR) << "Failed to find camera filter: "
              << logging::SystemErrorCodeToString(hr);
  return false;
}

bool VideoCaptureDeviceFactoryWin::CreateDeviceFilterDirectShow(
    ComPtr<IMoniker> moniker,
    IBaseFilter** capture_filter) {
  DCHECK(capture_filter);
  DCHECK(!*capture_filter);

  // Mitigate the issues caused by loading DLLs on a background thread
  // (http://crbug/973868).
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY_REPEATEDLY();

  HRESULT hr = moniker->BindToObject(0, 0, IID_PPV_ARGS(capture_filter));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to bind camera filter: "
                << logging::SystemErrorCodeToString(hr);
    return false;
  }
  return true;
}

MFSourceOutcome VideoCaptureDeviceFactoryWin::CreateDeviceSourceMediaFoundation(
    const std::string& device_id,
    VideoCaptureApi capture_api,
    const bool banned_for_d3d11,
    IMFMediaSource** source) {
  DCHECK(source);
  DCHECK(!*source);

  ComPtr<IMFAttributes> attributes;
  DCHECK_EQ(GetMFAttributes()[0].first, VideoCaptureApi::WIN_MEDIA_FOUNDATION);
  const auto& attributes_data =
      capture_api == VideoCaptureApi::WIN_MEDIA_FOUNDATION
          ? GetMFAttributes()[0].second
          : GetMFAttributes()[1].second;
  // We allocate attributes_data.size() + 1 (+1 is for sym_link below) elements
  // in attributes store.
  if (!PrepareVideoCaptureAttributesMediaFoundation(
          attributes_data, attributes_data.size() + 1, &attributes)) {
    return MFSourceOutcome::kFailed;
  }

  attributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
                        base::SysUTF8ToWide(device_id).c_str());

  return CreateDeviceSourceMediaFoundation(std::move(attributes),
                                           banned_for_d3d11, source);
}

MFSourceOutcome VideoCaptureDeviceFactoryWin::CreateDeviceSourceMediaFoundation(
    ComPtr<IMFAttributes> attributes,
    const bool banned_for_d3d11,
    IMFMediaSource** source_out) {
  ComPtr<IMFMediaSource> source;
  HRESULT hr = MFCreateDeviceSource(attributes.Get(), &source);
  DLOG_IF(ERROR, FAILED(hr)) << "MFCreateDeviceSource failed: "
                             << logging::SystemErrorCodeToString(hr);
  if (hr == E_ACCESSDENIED)
    return MFSourceOutcome::kFailedSystemPermissions;

  if (SUCCEEDED(hr) && use_d3d11_with_media_foundation_ &&
      dxgi_device_manager_ && !banned_for_d3d11) {
    dxgi_device_manager_->RegisterWithMediaSource(source);
  }
  *source_out = source.Detach();
  return SUCCEEDED(hr) ? MFSourceOutcome::kSuccess : MFSourceOutcome::kFailed;
}

bool VideoCaptureDeviceFactoryWin::EnumerateDeviceSourcesMediaFoundation(
    Microsoft::WRL::ComPtr<IMFAttributes> attributes,
    IMFActivate*** devices,
    UINT32* count) {
  HRESULT hr = MFEnumDeviceSources(attributes.Get(), devices, count);
  DLOG_IF(ERROR, FAILED(hr))
      << "MFEnumDeviceSources failed: " << logging::SystemErrorCodeToString(hr);
  return SUCCEEDED(hr);
}

void VideoCaptureDeviceFactoryWin::GetDevicesInfo(
    GetDevicesInfoCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::vector<VideoCaptureDeviceInfo> devices_info;

  if (use_media_foundation_) {
    DCHECK(PlatformSupportsMediaFoundation());
    devices_info = GetDevicesInfoMediaFoundation();
    AugmentDevicesListWithDirectShowOnlyDevices(&devices_info);
  } else {
    devices_info = GetDevicesInfoDirectShow(devices_info);
  }

  com_thread_.init_com_with_mta(true);
  com_thread_.Start();
  com_thread_data_ =
      base::MakeRefCounted<VideoCaptureDeviceFactoryWin::ComThreadData>(
          weak_ptr_factory_.GetWeakPtr(), com_thread_.task_runner(),
          base::SingleThreadTaskRunner::GetCurrentDefault());
  com_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VideoCaptureDeviceFactoryWin::ComThreadData::EnumerateDevicesUWP,
          com_thread_data_, std::move(devices_info), std::move(callback)));
}

void VideoCaptureDeviceFactoryWin::ComThreadData::EnumerateDevicesUWP(
    std::vector<VideoCaptureDeviceInfo> devices_info,
    GetDevicesInfoCallback result_callback) {
  DCHECK_GE(base::win::OSInfo::GetInstance()->version_number().build, 10240u);

  // When an error occurs below, the `UWP_ENUM_ERROR_HANDLER()` macro runs
  // `device_info_callback` with a `nullptr` operation.
  auto device_info_callback = base::BindOnce(
      &VideoCaptureDeviceFactoryWin::ComThreadData::FoundAllDevicesUWP,
      scoped_refptr<ComThreadData>(this), std::move(devices_info),
      std::move(result_callback));

  ComPtr<ABI::Windows::Devices::Enumeration::IDeviceInformationStatics>
      dev_info_statics;
  // Calling `GetActivationFactory` may load the DLL containing the
  // `IDeviceInformationStatics` APIs. Temporarily increase the priority
  // of this background thread to prevent hangs caused by priority inversion.
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
  HRESULT hr = GetActivationFactory<
      ABI::Windows::Devices::Enumeration::IDeviceInformationStatics,
      RuntimeClass_Windows_Devices_Enumeration_DeviceInformation>(
      &dev_info_statics);
  if (FAILED(hr)) {
    UWP_ENUM_ERROR_HANDLER(hr, "DeviceInformation factory failed: ");
    return;
  }

  IAsyncOperation<DeviceInformationCollection*>* async_op;
  ScopedHString aqs_filter =
      ScopedHString::Create(kVideoAndSensorCamerasAqsString);
  hr = dev_info_statics->FindAllAsyncAqsFilter(aqs_filter.get(), &async_op);
  if (FAILED(hr)) {
    UWP_ENUM_ERROR_HANDLER(hr, "Find all devices asynchronously failed: ");
    return;
  }

  // Keep a reference to incomplete |asyn_op| for releasing later.
  async_ops_.insert(async_op);

  auto callback = Microsoft::WRL::Callback<
      ABI::Windows::Foundation::IAsyncOperationCompletedHandler<
          DeviceInformationCollection*>>(
      [com_thread_runner = com_thread_runner_,
       device_info_callback = std::move(device_info_callback)](
          IAsyncOperation<DeviceInformationCollection*>* operation,
          AsyncStatus status) mutable -> HRESULT {
        com_thread_runner->PostTask(
            FROM_HERE, base::BindOnce(std::move(device_info_callback),
                                      base::Unretained(operation)));
        return S_OK;
      });

  hr = async_op->put_Completed(callback.Get());
  if (FAILED(hr)) {
    DLOG(WARNING) << "Register async operation callback failed: "
                  << logging::SystemErrorCodeToString(hr);
    // Run the callback after the error to report no devices found.
    callback->Invoke(async_op, AsyncStatus::Completed);
  }
}

void VideoCaptureDeviceFactoryWin::ComThreadData::FoundAllDevicesUWP(
    std::vector<VideoCaptureDeviceInfo> devices_info,
    GetDevicesInfoCallback result_callback,
    IAsyncOperation<DeviceInformationCollection*>* operation) {
  if (!operation) {
    origin_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoCaptureDeviceFactoryWin::DeviceInfoReady,
                       device_factory_, std::move(devices_info),
                       std::move(result_callback)));
    return;
  }

  ComPtr<ABI::Windows::Foundation::Collections::IVectorView<
      ABI::Windows::Devices::Enumeration::DeviceInformation*>>
      devices;
  operation->GetResults(&devices);

  unsigned int count = 0;
  if (devices) {
    devices->get_Size(&count);
  }

  for (unsigned int j = 0; j < count; ++j) {
    ComPtr<ABI::Windows::Devices::Enumeration::IDeviceInformation> device_info;
    HRESULT hr = devices->GetAt(j, &device_info);
    if (SUCCEEDED(hr)) {
      HSTRING id;
      device_info->get_Id(&id);

      std::string device_id = ScopedHString(id).GetAsUTF8();
      transform(device_id.begin(), device_id.end(), device_id.begin(),
                ::tolower);

      ComPtr<ABI::Windows::Devices::Enumeration::IEnclosureLocation>
          enclosure_location;
      hr = device_info->get_EnclosureLocation(&enclosure_location);
      if (FAILED(hr)) {
        break;
      }

      VideoFacingMode facing = VideoFacingMode::MEDIA_VIDEO_FACING_NONE;
      if (enclosure_location) {
        ABI::Windows::Devices::Enumeration::Panel panel;
        enclosure_location->get_Panel(&panel);
        switch (panel) {
          case ABI::Windows::Devices::Enumeration::Panel_Unknown:
            facing = VideoFacingMode::MEDIA_VIDEO_FACING_NONE;
            break;
          case ABI::Windows::Devices::Enumeration::Panel_Front:
            facing = VideoFacingMode::MEDIA_VIDEO_FACING_USER;
            break;
          case ABI::Windows::Devices::Enumeration::Panel_Back:
            facing = VideoFacingMode::MEDIA_VIDEO_FACING_ENVIRONMENT;
            break;
          default:
            facing = VideoFacingMode::MEDIA_VIDEO_FACING_NONE;
        }
      }

      for (auto& device : devices_info) {
        if (!device.descriptor.device_id.compare(device_id)) {
          device.descriptor.facing = facing;
          break;
        }
      }
    }
  }

  FindAndSetDefaultVideoCamera(&devices_info);

  origin_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoCaptureDeviceFactoryWin::DeviceInfoReady,
                                device_factory_, std::move(devices_info),
                                std::move(result_callback)));

  auto it = async_ops_.find(operation);
  CHECK(it != async_ops_.end(), base::NotFatalUntil::M130);
  (*it)->Release();
  async_ops_.erase(it);
}

void VideoCaptureDeviceFactoryWin::UpdateDevicesInfoAvailability(
    std::vector<VideoCaptureDeviceInfo>* devices_info) {
  if (report_handler_) {
    report_handler_->UpdateDevicesInfoAvailability(devices_info);
  }
}

void VideoCaptureDeviceFactoryWin::DeviceInfoReady(
    std::vector<VideoCaptureDeviceInfo> devices_info,
    GetDevicesInfoCallback result_callback) {
  if (com_thread_.IsRunning()) {
    com_thread_.Stop();
    com_thread_data_.reset();
  }
  UpdateDevicesInfoAvailability(&devices_info);

  std::move(result_callback).Run(std::move(devices_info));
}

DevicesInfo VideoCaptureDeviceFactoryWin::GetDevicesInfoMediaFoundation() {
  DVLOG(1) << " GetDevicesInfoMediaFoundation";

  DevicesInfo devices_info;

  if (use_d3d11_with_media_foundation_ && !dxgi_device_manager_) {
    dxgi_device_manager_ = DXGIDeviceManager::Create(luid_);
  }

  // Recent non-RGB (depth, IR) cameras could be marked as sensor cameras in
  // driver inf file and MFEnumDeviceSources enumerates them only if attribute
  // KSCATEGORY_SENSOR_CAMERA is supplied. We enumerate twice. As it is possible
  // that SENSOR_CAMERA is also in VIDEO_CAMERA category, we prevent duplicate
  // entries. https://crbug.com/807293
  for (const auto& api_attributes : GetMFAttributes()) {
    ComPtr<IMFAttributes> attributes;
    if (!PrepareVideoCaptureAttributesMediaFoundation(
            api_attributes.second, api_attributes.second.size(), &attributes)) {
      return {};
    }
    ScopedCoMem<IMFActivate*> devices;
    UINT32 count;
    if (!EnumerateDeviceSourcesMediaFoundation(std::move(attributes), &devices,
                                               &count)) {
      return {};
    }
    const bool list_was_empty = devices_info.empty();
    for (UINT32 i = 0; i < count; ++i) {
      ScopedCoMem<wchar_t> name;
      UINT32 name_size;
      HRESULT hr = devices[i]->GetAllocatedString(
          MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &name, &name_size);
      if (SUCCEEDED(hr)) {
        ScopedCoMem<wchar_t> id;
        UINT32 id_size;
        hr = devices[i]->GetAllocatedString(
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &id,
            &id_size);
        if (SUCCEEDED(hr)) {
          const std::string device_id =
              base::SysWideToUTF8(std::wstring(id, id_size));
          const std::string model_id = GetDeviceModelId(device_id);
          const std::string display_name =
              base::SysWideToUTF8(std::wstring(name, name_size));
          if (IsDeviceBlockedForMediaFoundationByModelId(model_id) ||
              IsDeviceBlockedForMediaFoundationByDisplayName(display_name)) {
            continue;
          }
          if (list_was_empty ||
              !DevicesInfoContainsDeviceId(devices_info, device_id)) {
            ComPtr<IMFMediaSource> source;
            VideoCaptureControlSupport control_support;
            VideoCaptureFormats supported_formats;
            const bool banned_for_d3d11 =
                IsDeviceBlockedForMediaFoundationD3D11ByModelId(model_id);
            if (CreateDeviceSourceMediaFoundation(
                    device_id, api_attributes.first, banned_for_d3d11,
                    &source) == MFSourceOutcome::kSuccess) {
              control_support =
                  VideoCaptureDeviceMFWin::GetControlSupport(source);
              supported_formats = GetSupportedFormatsMediaFoundation(
                  source, banned_for_d3d11, display_name);
            }
            devices_info.emplace_back(VideoCaptureDeviceDescriptor(
                display_name, device_id, model_id, api_attributes.first,
                control_support));
            devices_info.back().supported_formats =
                std::move(supported_formats);
          }
        }
      }
      DLOG_IF(ERROR, FAILED(hr)) << "GetAllocatedString failed: "
                                 << logging::SystemErrorCodeToString(hr);
      devices[i]->Release();
    }
  }

  return devices_info;
}

// Adds descriptors that are only reported by the DirectShow API.
// Replaces MediaFoundation descriptors with corresponding DirectShow
// ones if the MediaFoundation one has no supported formats,
// but the DirectShow one does.
void VideoCaptureDeviceFactoryWin::AugmentDevicesListWithDirectShowOnlyDevices(
    DevicesInfo* devices_info) {
  // DirectShow virtual cameras are not supported by MediaFoundation.
  // To overcome this, based on device name and model, we append
  // missing DirectShow device descriptor to full devices list.
  DevicesInfo direct_show_devices_info =
      GetDevicesInfoDirectShow(*devices_info);
  for (const auto& direct_show_device_info : direct_show_devices_info) {
    // DirectShow can produce two descriptors with same name and model.
    // If those descriptors are missing from MediaFoundation, we want them both
    // appended to the full descriptors list.
    // Therefore, we prevent duplication by always comparing a DirectShow
    // descriptor with a MediaFoundation one.

    DevicesInfo::const_iterator matching_non_direct_show_device =
        FindNonDirectShowDeviceInfoByNameAndModel(
            *devices_info,
            direct_show_device_info.descriptor.GetNameAndModel());

    // Devices like the Pinnacle Dazzle, appear both in DirectShow and
    // MediaFoundation. In MediaFoundation, they will have no supported video
    // format while in DirectShow they will have at least one video format.
    // We should delete MediaFoundation descriptor with no supported formats
    // and use the DirectShow instead.
    if (matching_non_direct_show_device != devices_info->end()) {
      if (direct_show_device_info.supported_formats.size() == 0) {
        // Skip this DirectShow device if it has no supported formats,
        // because the MediaFoundation one should be used instead.
        continue;
      }
      // Devices, already known from MediaFoundation, shouldn't be queried with
      // DirectShow.
      DCHECK(matching_non_direct_show_device->supported_formats.size() == 0);
      devices_info->erase(matching_non_direct_show_device);
    }
    devices_info->emplace_back(direct_show_device_info);
  }
}

DevicesInfo VideoCaptureDeviceFactoryWin::GetDevicesInfoDirectShow(
    const DevicesInfo& known_devices) {
  DVLOG(1) << __func__;

  ComPtr<IEnumMoniker> enum_moniker;
  if (!CreateDeviceEnumMonikerDirectShow(&enum_moniker))
    return {};

  DevicesInfo devices_info;

  // Enumerate all video capture devices.
  for (ComPtr<IMoniker> moniker;
       enum_moniker->Next(1, &moniker, nullptr) == S_OK; moniker.Reset()) {
    ComPtr<IPropertyBag> prop_bag;
    HRESULT hr = moniker->BindToStorage(0, 0, IID_PPV_ARGS(&prop_bag));
    if (FAILED(hr))
      continue;

    // Find the description or friendly name.
    ScopedVariant name;
    hr = prop_bag->Read(L"Description", name.Receive(), 0);
    if (FAILED(hr))
      hr = prop_bag->Read(L"FriendlyName", name.Receive(), 0);

    if (FAILED(hr) || name.type() != VT_BSTR)
      continue;

    const std::string device_name(base::SysWideToUTF8(V_BSTR(name.ptr())));
    if (IsDeviceBlocked(device_name))
      continue;

    name.Reset();
    hr = prop_bag->Read(L"DevicePath", name.Receive(), 0);
    std::string id;
    if (FAILED(hr) || name.type() != VT_BSTR) {
      id = device_name;
    } else {
      DCHECK_EQ(name.type(), VT_BSTR);
      id = base::SysWideToUTF8(V_BSTR(name.ptr()));
    }

    const std::string model_id = GetDeviceModelId(id);

    auto device_descriptor = VideoCaptureDeviceDescriptor(
        device_name, id, model_id, VideoCaptureApi::WIN_DIRECT_SHOW,
        VideoCaptureControlSupport());

    DevicesInfo::const_iterator matching_non_direct_show_device =
        FindNonDirectShowDeviceInfoByNameAndModel(
            known_devices, device_descriptor.GetNameAndModel());

    // Skip the DirectShow device, if the same device is already known from
    // MediaFoundation and has some supported formats, since the MediaFoundation
    // descriptor would be used in the end.
    if (matching_non_direct_show_device != known_devices.end() &&
        matching_non_direct_show_device->supported_formats.size() > 0) {
      continue;
    }

    VideoCaptureControlSupport control_support;
    VideoCaptureFormats supported_formats;
    ComPtr<IBaseFilter> capture_filter;
    if (CreateDeviceFilterDirectShow(std::move(moniker), &capture_filter)) {
      control_support =
          VideoCaptureDeviceWin::GetControlSupport(capture_filter);
      supported_formats =
          GetSupportedFormatsDirectShow(capture_filter, device_name);
    }
    device_descriptor.set_control_support(control_support);
    devices_info.emplace_back(device_descriptor);
    devices_info.back().supported_formats = std::move(supported_formats);
  }

  return devices_info;
}

VideoCaptureFormats VideoCaptureDeviceFactoryWin::GetSupportedFormatsDirectShow(
    ComPtr<IBaseFilter> capture_filter,
    const std::string& display_name) {
  VideoCaptureFormats formats;
  bool query_detailed_frame_rates =
      !IsDeviceBlockedForQueryingDetailedFrameRates(display_name);
  CapabilityList capability_list;
  VideoCaptureDeviceWin::GetDeviceCapabilityList(
      capture_filter, query_detailed_frame_rates, &capability_list);
  for (const auto& entry : capability_list) {
    formats.emplace_back(entry.supported_format);
    DVLOG(1) << display_name << " "
             << VideoCaptureFormat::ToString(entry.supported_format);
  }
  return formats;
}

VideoCaptureFormats
VideoCaptureDeviceFactoryWin::GetSupportedFormatsMediaFoundation(
    ComPtr<IMFMediaSource> source,
    const bool banned_for_d3d11,
    const std::string& display_name) {
  ComPtr<IMFAttributes> source_reader_attributes;
  const bool dxgi_device_manager_available =
      (dxgi_device_manager_ != nullptr) && !banned_for_d3d11;
  if (dxgi_device_manager_available) {
    dxgi_device_manager_->RegisterWithMediaSource(source);

    HRESULT hr = MFCreateAttributes(&source_reader_attributes, 1);
    if (SUCCEEDED(hr)) {
      dxgi_device_manager_->RegisterInSourceReaderAttributes(
          source_reader_attributes.Get());
    } else {
      DLOG(ERROR) << "MFCreateAttributes failed: "
                  << logging::SystemErrorCodeToString(hr);
    }
  }

  ComPtr<IMFSourceReader> reader;
  HRESULT hr = MFCreateSourceReaderFromMediaSource(
      source.Get(), source_reader_attributes.Get(), &reader);
  if (FAILED(hr)) {
    DLOG(ERROR) << "MFCreateSourceReaderFromMediaSource failed: "
                << logging::SystemErrorCodeToString(hr);
    return {};
  }

  VideoCaptureFormats formats;

  DWORD stream_index = 0;
  ComPtr<IMFMediaType> type;

  while (SUCCEEDED(hr = reader->GetNativeMediaType(
                       static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
                       stream_index, &type))) {
    UINT32 width, height;
    hr = MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &width, &height);
    if (FAILED(hr)) {
      DLOG(ERROR) << "MFGetAttributeSize failed: "
                  << logging::SystemErrorCodeToString(hr);
      return {};
    }
    VideoCaptureFormat capture_format;
    capture_format.frame_size.SetSize(width, height);

    UINT32 numerator, denominator;
    hr = MFGetAttributeRatio(type.Get(), MF_MT_FRAME_RATE, &numerator,
                             &denominator);
    if (FAILED(hr)) {
      DLOG(ERROR) << "MFGetAttributeSize failed: "
                  << logging::SystemErrorCodeToString(hr);
      return {};
    }
    capture_format.frame_rate =
        denominator ? static_cast<float>(numerator) / denominator : 0.0f;

    GUID type_guid;
    hr = type->GetGUID(MF_MT_SUBTYPE, &type_guid);
    if (FAILED(hr)) {
      DLOG(ERROR) << "GetGUID failed: " << logging::SystemErrorCodeToString(hr);
      return {};
    }
    VideoCaptureDeviceMFWin::GetPixelFormatFromMFSourceMediaSubtype(
        type_guid, /*use_hardware_format=*/dxgi_device_manager_available,
        &capture_format.pixel_format);
    type.Reset();
    ++stream_index;
    if (capture_format.pixel_format == PIXEL_FORMAT_UNKNOWN)
      continue;
    formats.push_back(capture_format);

    DVLOG(1) << display_name << " "
             << VideoCaptureFormat::ToString(capture_format);
  }

  return formats;
}

scoped_refptr<DXGIDeviceManager>
VideoCaptureDeviceFactoryWin::GetDxgiDeviceManager() {
  return dxgi_device_manager_;
}

void VideoCaptureDeviceFactoryWin::OnGpuInfoUpdate(const CHROME_LUID& luid) {
  luid_ = luid;
  if (dxgi_device_manager_) {
    dxgi_device_manager_->OnGpuInfoUpdate(luid_);
  }
}

void VideoCaptureDeviceFactoryWin::CreateUsageMonitorAndReportHandler() {
  scoped_refptr<UsageReportHandler> report_handler =
      base::MakeRefCounted<UsageReportHandler>();
  if (CreateMFSensorActivityMonitor(report_handler.get(), &monitor_)) {
    report_handler_ = std::move(report_handler);
    HRESULT hr = monitor_->Start();
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to start usage monitor";
    }
  } else {
    DLOG(ERROR) << "Failed to create usage monitor";
  }
}

}  // namespace media
