// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/video_capture_device_factory_win.h"

#include <mfapi.h>
#include <mferror.h>
#include <objbase.h>
#include <stddef.h>
#include <windows.devices.enumeration.h>
#include <windows.foundation.collections.h>
#include <wrl.h>
#include <wrl/client.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "media/base/media_switches.h"
#include "media/base/win/mf_initializer.h"
#include "media/capture/video/win/metrics.h"
#include "media/capture/video/win/video_capture_device_mf_win.h"
#include "media/capture/video/win/video_capture_device_win.h"

using Descriptor = media::VideoCaptureDeviceDescriptor;
using Descriptors = media::VideoCaptureDeviceDescriptors;
using base::win::GetActivationFactory;
using base::win::ScopedCoMem;
using base::win::ScopedHString;
using base::win::ScopedVariant;
using Microsoft::WRL::ComPtr;

namespace media {

namespace {

// In Windows device identifiers, the USB VID and PID are preceded by the string
// "vid_" or "pid_".  The identifiers are each 4 bytes long.
const char kVidPrefix[] = "vid_";  // Also contains '\0'.
const char kPidPrefix[] = "pid_";  // Also contains '\0'.
const size_t kVidPidSize = 4;

// Avoid enumerating and/or using certain devices due to they provoking crashes
// or any other reason (http://crbug.com/378494). This enum is defined for the
// purposes of UMA collection. Existing entries cannot be removed.
enum BlacklistedCameraNames {
  BLACKLISTED_CAMERA_GOOGLE_CAMERA_ADAPTER = 0,
  BLACKLISTED_CAMERA_IP_CAMERA = 1,
  BLACKLISTED_CAMERA_CYBERLINK_WEBCAM_SPLITTER = 2,
  BLACKLISTED_CAMERA_EPOCCAM = 3,
  // This one must be last, and equal to the previous enumerated value.
  BLACKLISTED_CAMERA_MAX = BLACKLISTED_CAMERA_EPOCCAM,
};

#define UWP_ENUM_ERROR_HANDLER(hr, err_log)                         \
  DLOG(WARNING) << err_log << logging::SystemErrorCodeToString(hr); \
  origin_task_runner_->PostTask(FROM_HERE,                          \
                                base::BindOnce(device_info_callback, nullptr))

// Blacklisted devices are identified by a characteristic prefix of the name.
// This prefix is used case-insensitively. This list must be kept in sync with
// |BlacklistedCameraNames|.
const char* const kBlacklistedCameraNames[] = {
    // Name of a fake DirectShow filter on computers with GTalk installed.
    "Google Camera Adapter",
    // The following software WebCams cause crashes.
    "IP Camera [JPEG/MJPEG]", "CyberLink Webcam Splitter", "EpocCam",
};
static_assert(base::size(kBlacklistedCameraNames) == BLACKLISTED_CAMERA_MAX + 1,
              "kBlacklistedCameraNames should be same size as "
              "BlacklistedCameraNames enum");

const char* const kModelIdsBlacklistedForMediaFoundation[] = {
    // Devices using Empia 2860 or 2820 chips, see https://crbug.com/849636.
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
    // also https://crbug.com/924528
    "04ca:7047", "04ca:7048",
    // HP Elitebook 840 G1
    "04f2:b3ed", "04f2:b3ca", "05c8:035d", "05c8:0369",
    // HP HD Camera. See https://crbug.com/1011888.
    "04ca:7095",
    // RBG/IR camera for Windows Hello Face Auth. See https://crbug.com/984864.
    "13d3:5257"};

const std::pair<VideoCaptureApi, std::vector<std::pair<GUID, GUID>>>
    kMfAttributes[] = {{VideoCaptureApi::WIN_MEDIA_FOUNDATION,
                        {
                            {MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                             MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID},
                        }},
                       {VideoCaptureApi::WIN_MEDIA_FOUNDATION_SENSOR,
                        {{MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                          MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID},
                         {MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_CATEGORY,
                          KSCATEGORY_SENSOR_CAMERA}}}};

bool IsDeviceBlacklistedForQueryingDetailedFrameRates(
    const std::string& display_name) {
  return display_name.find("WebcamMax") != std::string::npos;
}

bool IsDeviceBlacklistedForMediaFoundationByModelId(
    const std::string& model_id) {
  return base::Contains(kModelIdsBlacklistedForMediaFoundation, model_id);
}

bool LoadMediaFoundationDlls() {
  static const wchar_t* const kMfDLLs[] = {
      L"%WINDIR%\\system32\\mf.dll", L"%WINDIR%\\system32\\mfplat.dll",
      L"%WINDIR%\\system32\\mfreadwrite.dll",
      L"%WINDIR%\\system32\\MFCaptureEngine.dll"};

  for (const wchar_t* kMfDLL : kMfDLLs) {
    wchar_t path[MAX_PATH] = {0};
    ExpandEnvironmentStringsW(kMfDLL, path, base::size(path));
    if (!LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH))
      return false;
  }
  return true;
}

bool PrepareVideoCaptureAttributesMediaFoundation(
    const std::vector<std::pair<GUID, GUID>>& attributes_data,
    int count,
    IMFAttributes** attributes) {
  // Once https://bugs.chromium.org/p/chromium/issues/detail?id=791615 is fixed,
  // we must make sure that this method succeeds in capture_unittests context
  // when MediaFoundation is enabled.
  if (!VideoCaptureDeviceFactoryWin::PlatformSupportsMediaFoundation() ||
      !InitializeMediaFoundation()) {
    return false;
  }

  if (FAILED(MFCreateAttributes(attributes, count)))
    return false;

  for (const auto& value : attributes_data) {
    if (!SUCCEEDED((*attributes)->SetGUID(value.first, value.second)))
      return false;
  }
  return true;
}

bool CreateVideoCaptureDeviceMediaFoundation(const Descriptor& descriptor,
                                             IMFMediaSource** source) {
  ComPtr<IMFAttributes> attributes;
  static_assert(
      base::size(kMfAttributes) == 2,
      "Implementation here asumes that kMfAttributes has size of two.");
  DCHECK_EQ(kMfAttributes[0].first, VideoCaptureApi::WIN_MEDIA_FOUNDATION);
  const auto& attributes_data =
      descriptor.capture_api == VideoCaptureApi::WIN_MEDIA_FOUNDATION
          ? kMfAttributes[0].second
          : kMfAttributes[1].second;
  // We allocate attributes_data.size() + 1 (+1 is for sym_link below) elements
  // in attributes store.
  if (!PrepareVideoCaptureAttributesMediaFoundation(
          attributes_data, attributes_data.size() + 1,
          attributes.GetAddressOf())) {
    return false;
  }

  attributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
                        base::SysUTF8ToWide(descriptor.device_id).c_str());
  return SUCCEEDED(MFCreateDeviceSource(attributes.Get(), source));
}

bool IsDeviceBlackListed(const std::string& name) {
  DCHECK_EQ(BLACKLISTED_CAMERA_MAX + 1,
            static_cast<int>(base::size(kBlacklistedCameraNames)));
  for (size_t i = 0; i < base::size(kBlacklistedCameraNames); ++i) {
    if (base::StartsWith(name, kBlacklistedCameraNames[i],
                         base::CompareCase::INSENSITIVE_ASCII)) {
      DVLOG(1) << "Enumerated blacklisted device: " << name;
      UMA_HISTOGRAM_ENUMERATION("Media.VideoCapture.BlacklistedDevice", i,
                                BLACKLISTED_CAMERA_MAX + 1);
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

HRESULT EnumerateDirectShowDevices(IEnumMoniker** enum_moniker) {
  ComPtr<ICreateDevEnum> dev_enum;
  HRESULT hr = ::CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC,
                                  IID_PPV_ARGS(&dev_enum));
  if (FAILED(hr))
    return hr;

  hr = dev_enum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,
                                       enum_moniker, 0);
  return hr;
}

bool DescriptorsContainDeviceId(const Descriptors& descriptors,
                                const std::string& device_id) {
  return std::find_if(
             descriptors.begin(), descriptors.end(),
             [device_id](const VideoCaptureDeviceDescriptor& descriptor) {
               return device_id == descriptor.device_id;
             }) != descriptors.end();
}

// Returns a non DirectShow descriptor using the provided name and model
Descriptors::const_iterator FindNonDirectShowDescriptorByNameAndModel(
    const Descriptors& descriptors,
    const std::string& name_and_model) {
  return std::find_if(
      descriptors.begin(), descriptors.end(),
      [name_and_model](const VideoCaptureDeviceDescriptor& descriptor) {
        return descriptor.capture_api != VideoCaptureApi::WIN_DIRECT_SHOW &&
               name_and_model == descriptor.GetNameAndModel();
      });
}

void GetDeviceSupportedFormatsDirectShow(const Descriptor& descriptor,
                                         VideoCaptureFormats* formats) {
  DVLOG(1) << "GetDeviceSupportedFormatsDirectShow for "
           << descriptor.display_name();
  bool query_detailed_frame_rates =
      !IsDeviceBlacklistedForQueryingDetailedFrameRates(
          descriptor.display_name());
  CapabilityList capability_list;
  VideoCaptureDeviceWin::GetDeviceCapabilityList(
      descriptor.device_id, query_detailed_frame_rates, &capability_list);
  for (const auto& entry : capability_list) {
    formats->emplace_back(entry.supported_format);
    DVLOG(1) << descriptor.display_name() << " "
             << VideoCaptureFormat::ToString(entry.supported_format);
  }
}

void GetDeviceSupportedFormatsMediaFoundation(const Descriptor& descriptor,
                                              VideoCaptureFormats* formats) {
  DVLOG(1) << "GetDeviceSupportedFormatsMediaFoundation for "
           << descriptor.display_name();
  ComPtr<IMFMediaSource> source;
  if (!CreateVideoCaptureDeviceMediaFoundation(descriptor,
                                               source.GetAddressOf())) {
    return;
  }

  ComPtr<IMFSourceReader> reader;
  HRESULT hr = MFCreateSourceReaderFromMediaSource(source.Get(), NULL,
                                                   reader.GetAddressOf());
  if (FAILED(hr)) {
    DLOG(ERROR) << "MFCreateSourceReaderFromMediaSource failed: "
                << logging::SystemErrorCodeToString(hr);
    return;
  }

  DWORD stream_index = 0;
  ComPtr<IMFMediaType> type;
  while (SUCCEEDED(hr = reader->GetNativeMediaType(
                       static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
                       stream_index, type.GetAddressOf()))) {
    UINT32 width, height;
    hr = MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &width, &height);
    if (FAILED(hr)) {
      DLOG(ERROR) << "MFGetAttributeSize failed: "
                  << logging::SystemErrorCodeToString(hr);
      return;
    }
    VideoCaptureFormat capture_format;
    capture_format.frame_size.SetSize(width, height);

    UINT32 numerator, denominator;
    hr = MFGetAttributeRatio(type.Get(), MF_MT_FRAME_RATE, &numerator,
                             &denominator);
    if (FAILED(hr)) {
      DLOG(ERROR) << "MFGetAttributeSize failed: "
                  << logging::SystemErrorCodeToString(hr);
      return;
    }
    capture_format.frame_rate =
        denominator ? static_cast<float>(numerator) / denominator : 0.0f;

    GUID type_guid;
    hr = type->GetGUID(MF_MT_SUBTYPE, &type_guid);
    if (FAILED(hr)) {
      DLOG(ERROR) << "GetGUID failed: " << logging::SystemErrorCodeToString(hr);
      return;
    }
    VideoCaptureDeviceMFWin::GetPixelFormatFromMFSourceMediaSubtype(
        type_guid, &capture_format.pixel_format);
    type.Reset();
    ++stream_index;
    if (capture_format.pixel_format == PIXEL_FORMAT_UNKNOWN)
      continue;
    formats->push_back(capture_format);

    DVLOG(1) << descriptor.display_name() << " "
             << VideoCaptureFormat::ToString(capture_format);
  }
}

bool IsEnclosureLocationSupported() {
  // DeviceInformation class is only available in Win10 onwards (v10.0.10240.0).
  if (base::win::GetVersion() < base::win::Version::WIN10) {
    DVLOG(1) << "DeviceInformation not supported before Windows 10";
    return false;
  }

  if (!(base::win::ResolveCoreWinRTDelayload() &&
        ScopedHString::ResolveCoreWinRTStringDelayload())) {
    DLOG(ERROR) << "Failed loading functions from combase.dll";
    return false;
  }

  return true;
}

}  // namespace

// Returns true if the current platform supports the Media Foundation API
// and that the DLLs are available.  On Vista this API is an optional download
// but the API is advertised as a part of Windows 7 and onwards.  However,
// we've seen that the required DLLs are not available in some Win7
// distributions such as Windows 7 N and Windows 7 KN.
// static
bool VideoCaptureDeviceFactoryWin::PlatformSupportsMediaFoundation() {
  static bool g_dlls_available = LoadMediaFoundationDlls();
  return g_dlls_available;
}

VideoCaptureDeviceFactoryWin::VideoCaptureDeviceFactoryWin()
    : use_media_foundation_(
          base::FeatureList::IsEnabled(media::kMediaFoundationVideoCapture)),
      com_thread_("Windows Video Capture COM Thread") {
  mf_enum_device_sources_func_ =
      PlatformSupportsMediaFoundation() ? MFEnumDeviceSources : nullptr;
  direct_show_enum_devices_func_ =
      base::BindRepeating(&EnumerateDirectShowDevices);

  mf_get_supported_formats_func_ =
      base::BindRepeating(&GetDeviceSupportedFormatsMediaFoundation);
  direct_show_get_supported_formats_func_ =
      base::BindRepeating(&GetDeviceSupportedFormatsDirectShow);

  if (use_media_foundation_ && !PlatformSupportsMediaFoundation()) {
    use_media_foundation_ = false;
    LogVideoCaptureWinBackendUsed(
        VideoCaptureWinBackendUsed::kUsingDirectShowAsFallback);
  } else if (use_media_foundation_) {
    LogVideoCaptureWinBackendUsed(
        VideoCaptureWinBackendUsed::kUsingMediaFoundationAsDefault);
  } else {
    LogVideoCaptureWinBackendUsed(
        VideoCaptureWinBackendUsed::kUsingDirectShowAsDefault);
  }
}

VideoCaptureDeviceFactoryWin::~VideoCaptureDeviceFactoryWin() {
  if (com_thread_.IsRunning()) {
    com_thread_.Stop();
  }
}

std::unique_ptr<VideoCaptureDevice> VideoCaptureDeviceFactoryWin::CreateDevice(
    const Descriptor& device_descriptor) {
  DCHECK(thread_checker_.CalledOnValidThread());
  switch (device_descriptor.capture_api) {
    case VideoCaptureApi::WIN_MEDIA_FOUNDATION:
      FALLTHROUGH;
    case VideoCaptureApi::WIN_MEDIA_FOUNDATION_SENSOR: {
      DCHECK(PlatformSupportsMediaFoundation());
      ComPtr<IMFMediaSource> source;
      if (!CreateVideoCaptureDeviceMediaFoundation(device_descriptor,
                                                   source.GetAddressOf())) {
        break;
      }
      std::unique_ptr<VideoCaptureDevice> device(
          new VideoCaptureDeviceMFWin(device_descriptor, source));
      DVLOG(1) << " MediaFoundation Device: "
               << device_descriptor.display_name();
      if (static_cast<VideoCaptureDeviceMFWin*>(device.get())->Init())
        return device;
      break;
    }
    case VideoCaptureApi::WIN_DIRECT_SHOW: {
      DVLOG(1) << " DirectShow Device: " << device_descriptor.display_name();
      std::unique_ptr<VideoCaptureDevice> device(
          new VideoCaptureDeviceWin(device_descriptor));
      if (static_cast<VideoCaptureDeviceWin*>(device.get())->Init())
        return device;
      break;
    }
    default:
      NOTREACHED();
      break;
  }
  return nullptr;
}

void VideoCaptureDeviceFactoryWin::GetDeviceDescriptors(
    VideoCaptureDeviceDescriptors* device_descriptors) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (use_media_foundation_) {
    GetDeviceDescriptorsMediaFoundation(device_descriptors);
    AugmentDescriptorListWithDirectShowOnlyDevices(device_descriptors);
  } else {
    GetDeviceDescriptorsDirectShow(device_descriptors);
  }
}

void VideoCaptureDeviceFactoryWin::GetCameraLocationsAsync(
    std::unique_ptr<VideoCaptureDeviceDescriptors> device_descriptors,
    DeviceDescriptorsCallback result_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (IsEnclosureLocationSupported()) {
    origin_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    com_thread_.init_com_with_mta(true);
    com_thread_.Start();
    com_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoCaptureDeviceFactoryWin::EnumerateDevicesUWP,
                       base::Unretained(this), std::move(device_descriptors),
                       std::move(result_callback)));
  } else {
    DeviceInfoReady(std::move(device_descriptors), std::move(result_callback));
  }
}

void VideoCaptureDeviceFactoryWin::EnumerateDevicesUWP(
    std::unique_ptr<VideoCaptureDeviceDescriptors> device_descriptors,
    DeviceDescriptorsCallback result_callback) {
  DCHECK_GE(base::win::OSInfo::GetInstance()->version_number().build, 10240);

  VideoCaptureDeviceFactoryWin* factory = this;
  scoped_refptr<base::SingleThreadTaskRunner> com_thread_runner =
      com_thread_.task_runner();

  // The |device_info_callback| created by base::BindRepeating() is copyable,
  // which is necessary for the below lambda function of |callback| for the
  // asynchronous operation. The reason is to permanently capture anything in a
  // lambda, it must be copyable, merely movable is insufficient.
  auto device_info_callback = base::BindRepeating(
      &VideoCaptureDeviceFactoryWin::FoundAllDevicesUWP,
      base::Unretained(factory), base::Passed(&device_descriptors),
      base::Passed(&result_callback));
  auto callback =
      Microsoft::WRL::Callback<
          ABI::Windows::Foundation::IAsyncOperationCompletedHandler<
              DeviceInformationCollection*>>(
          [com_thread_runner, device_info_callback](
              IAsyncOperation<DeviceInformationCollection*>* operation,
              AsyncStatus status) -> HRESULT {
            com_thread_runner->PostTask(
                FROM_HERE, base::BindOnce(device_info_callback,
                                          base::Unretained(operation)));
            return S_OK;
          });

  ComPtr<ABI::Windows::Devices::Enumeration::IDeviceInformationStatics>
      dev_info_statics;
  HRESULT hr = GetActivationFactory<
      ABI::Windows::Devices::Enumeration::IDeviceInformationStatics,
      RuntimeClass_Windows_Devices_Enumeration_DeviceInformation>(
      &dev_info_statics);
  if (FAILED(hr)) {
    UWP_ENUM_ERROR_HANDLER(hr, "DeviceInformation factory failed: ");
    return;
  }

  IAsyncOperation<DeviceInformationCollection*>* async_op;
  hr = dev_info_statics->FindAllAsyncDeviceClass(
      ABI::Windows::Devices::Enumeration::DeviceClass_VideoCapture, &async_op);
  if (FAILED(hr)) {
    UWP_ENUM_ERROR_HANDLER(hr, "Find all devices asynchronously failed: ");
    return;
  }

  hr = async_op->put_Completed(callback.Get());
  if (FAILED(hr)) {
    UWP_ENUM_ERROR_HANDLER(hr, "Register async operation callback failed: ");
    return;
  }

  // Keep a reference to incomplete |asyn_op| for releasing later.
  async_ops_.insert(async_op);
}

void VideoCaptureDeviceFactoryWin::FoundAllDevicesUWP(
    std::unique_ptr<VideoCaptureDeviceDescriptors> device_descriptors,
    DeviceDescriptorsCallback result_callback,
    IAsyncOperation<DeviceInformationCollection*>* operation) {
  if (!operation) {
    origin_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoCaptureDeviceFactoryWin::DeviceInfoReady,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(device_descriptors),
                       std::move(result_callback)));
    return;
  }

  ComPtr<ABI::Windows::Foundation::Collections::IVectorView<
      ABI::Windows::Devices::Enumeration::DeviceInformation*>>
      devices;
  operation->GetResults(devices.GetAddressOf());

  unsigned int count = 0;
  if (devices) {
    devices->get_Size(&count);
  }

  for (unsigned int j = 0; j < count; ++j) {
    ComPtr<ABI::Windows::Devices::Enumeration::IDeviceInformation> device_info;
    HRESULT hr = devices->GetAt(j, device_info.GetAddressOf());
    if (SUCCEEDED(hr)) {
      HSTRING id;
      device_info->get_Id(&id);

      std::string device_id = ScopedHString(id).GetAsUTF8();
      transform(device_id.begin(), device_id.end(), device_id.begin(),
                ::tolower);
      const std::string model_id = GetDeviceModelId(device_id);

      ComPtr<ABI::Windows::Devices::Enumeration::IEnclosureLocation>
          enclosure_location;
      hr =
          device_info->get_EnclosureLocation(enclosure_location.GetAddressOf());
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

      for (Descriptor& descriptor : *device_descriptors) {
        if (!descriptor.model_id.compare(model_id)) {
          descriptor.facing = facing;
          break;
        }
      }
    }
  }

  origin_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoCaptureDeviceFactoryWin::DeviceInfoReady,
                     base::Unretained(this), std::move(device_descriptors),
                     std::move(result_callback)));

  auto it = async_ops_.find(operation);
  DCHECK(it != async_ops_.end());
  (*it)->Release();
  async_ops_.erase(it);
}

void VideoCaptureDeviceFactoryWin::DeviceInfoReady(
    std::unique_ptr<VideoCaptureDeviceDescriptors> device_descriptors,
    DeviceDescriptorsCallback result_callback) {
  if (com_thread_.IsRunning()) {
    com_thread_.Stop();
  }

  std::move(result_callback).Run(std::move(device_descriptors));
}

void VideoCaptureDeviceFactoryWin::GetDeviceDescriptorsMediaFoundation(
    Descriptors* device_descriptors) {
  DVLOG(1) << " GetDeviceDescriptorsMediaFoundation";
  // Recent non-RGB (depth, IR) cameras could be marked as sensor cameras in
  // driver inf file and MFEnumDeviceSources enumerates them only if attribute
  // KSCATEGORY_SENSOR_CAMERA is supplied. We enumerate twice. As it is possible
  // that SENSOR_CAMERA is also in VIDEO_CAMERA category, we prevent duplicate
  // entries. https://crbug.com/807293
  for (const auto& api_attributes : kMfAttributes) {
    ScopedCoMem<IMFActivate*> devices;
    UINT32 count;
    if (!EnumerateVideoDevicesMediaFoundation(api_attributes.second, &devices,
                                              &count)) {
      return;
    }
    const bool list_was_empty = !device_descriptors->size();
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
          if (IsDeviceBlacklistedForMediaFoundationByModelId(model_id))
            continue;
          if (list_was_empty ||
              !DescriptorsContainDeviceId(*device_descriptors, device_id)) {
            device_descriptors->emplace_back(
                base::SysWideToUTF8(std::wstring(name, name_size)), device_id,
                model_id, api_attributes.first);
          }
        }
      }
      DLOG_IF(ERROR, FAILED(hr)) << "GetAllocatedString failed: "
                                 << logging::SystemErrorCodeToString(hr);
      devices[i]->Release();
    }
  }
}

// Adds descriptors that are only reported by the DirectShow API.
// Replaces MediaFoundation descriptors with corresponding DirectShow
// ones if the MediaFoundation one has no supported formats,
// but the DirectShow one does.
void VideoCaptureDeviceFactoryWin::
    AugmentDescriptorListWithDirectShowOnlyDevices(
        VideoCaptureDeviceDescriptors* device_descriptors) {
  // DirectShow virtual cameras are not supported by MediaFoundation.
  // To overcome this, based on device name and model, we append
  // missing DirectShow device descriptor to full descriptors list.
  Descriptors direct_show_descriptors;
  GetDeviceDescriptorsDirectShow(&direct_show_descriptors);
  for (const auto& direct_show_descriptor : direct_show_descriptors) {
    // DirectShow can produce two descriptors with same name and model.
    // If those descriptors are missing from MediaFoundation, we want them both
    // appended to the full descriptors list.
    // Therefore, we prevent duplication by always comparing a DirectShow
    // descriptor with a MediaFoundation one.

    Descriptors::const_iterator matching_non_direct_show_descriptor =
        FindNonDirectShowDescriptorByNameAndModel(
            *device_descriptors, direct_show_descriptor.GetNameAndModel());

    // Devices like the Pinnacle Dazzle, appear both in DirectShow and
    // MediaFoundation. In MediaFoundation, they will have no supported video
    // format while in DirectShow they will have at least one video format.
    // Therefore, we must prioritize the MediaFoundation descriptor if it has at
    // least one supported format
    if (matching_non_direct_show_descriptor != device_descriptors->end()) {
      if (GetNumberOfSupportedFormats(*matching_non_direct_show_descriptor) > 0)
        continue;
      if (GetNumberOfSupportedFormats(direct_show_descriptor) == 0)
        continue;
      device_descriptors->erase(matching_non_direct_show_descriptor);
    }
    device_descriptors->emplace_back(direct_show_descriptor);
  }
}

bool VideoCaptureDeviceFactoryWin::EnumerateVideoDevicesMediaFoundation(
    const std::vector<std::pair<GUID, GUID>>& attributes_data,
    IMFActivate*** devices,
    UINT32* count) {
  ComPtr<IMFAttributes> attributes;
  if (!PrepareVideoCaptureAttributesMediaFoundation(
          attributes_data, attributes_data.size(), attributes.GetAddressOf())) {
    return false;
  }
  return SUCCEEDED(
      mf_enum_device_sources_func_(attributes.Get(), devices, count));
}

void VideoCaptureDeviceFactoryWin::GetDeviceDescriptorsDirectShow(
    Descriptors* device_descriptors) {
  DCHECK(device_descriptors);
  DVLOG(1) << __func__;

  ComPtr<IEnumMoniker> enum_moniker;
  HRESULT hr = direct_show_enum_devices_func_.Run(enum_moniker.GetAddressOf());
  // CreateClassEnumerator returns S_FALSE on some Windows OS
  // when no camera exist. Therefore the FAILED macro can't be used.
  if (hr != S_OK)
    return;

  // Enumerate all video capture devices.
  for (ComPtr<IMoniker> moniker;
       enum_moniker->Next(1, moniker.GetAddressOf(), NULL) == S_OK;
       moniker.Reset()) {
    ComPtr<IPropertyBag> prop_bag;
    hr = moniker->BindToStorage(0, 0, IID_PPV_ARGS(&prop_bag));
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
    if (IsDeviceBlackListed(device_name))
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

    device_descriptors->emplace_back(device_name, id, model_id,
                                     VideoCaptureApi::WIN_DIRECT_SHOW);
  }
}

int VideoCaptureDeviceFactoryWin::GetNumberOfSupportedFormats(
    const Descriptor& device) {
  VideoCaptureFormats formats;
  GetApiSpecificSupportedFormats(device, &formats);
  return formats.size();
}

void VideoCaptureDeviceFactoryWin::GetApiSpecificSupportedFormats(
    const Descriptor& device,
    VideoCaptureFormats* formats) {
  if (device.capture_api != VideoCaptureApi::WIN_DIRECT_SHOW)
    mf_get_supported_formats_func_.Run(device, formats);
  else
    direct_show_get_supported_formats_func_.Run(device, formats);
}

void VideoCaptureDeviceFactoryWin::GetSupportedFormats(
    const Descriptor& device,
    VideoCaptureFormats* formats) {
  DCHECK(thread_checker_.CalledOnValidThread());
  GetApiSpecificSupportedFormats(device, formats);
}

}  // namespace media
