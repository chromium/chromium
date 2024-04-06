// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/system_info_handler.h"

#include <stdint.h>

#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/process/process_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/render_process_host.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_switches.h"
#include "media/base/video_codecs.h"

namespace content {
namespace protocol {

namespace {

using SystemInfo::GPUDevice;
using SystemInfo::GPUInfo;
using GetInfoCallback = SystemInfo::Backend::GetInfoCallback;

std::unique_ptr<SystemInfo::Size> GfxSizeToSystemInfoSize(
    const gfx::Size& size) {
  return SystemInfo::Size::Create()
      .SetWidth(size.width())
      .SetHeight(size.height())
      .Build();
}

// Give the GPU process a few seconds to provide GPU info.

// Linux and ChromeOS Debug builds need more time -- see Issue 796437,
// 1046598, and 1153667.
// Windows builds need more time -- see Issue 873112 and 1004472.
// Mac builds need more time - see Issue angleproject:6182.
#if ((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && !defined(NDEBUG)) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_OZONE)
static constexpr int kGPUInfoWatchdogTimeoutMultiplierOS = 3;
#else
static constexpr int kGPUInfoWatchdogTimeoutMultiplierOS = 1;
#endif

// ASAN builds need more time -- see Issue 1167875 and 1242771.
#ifdef ADDRESS_SANITIZER
static constexpr int kGPUInfoWatchdogTimeoutMultiplierASAN = 3;
#else
static constexpr int kGPUInfoWatchdogTimeoutMultiplierASAN = 1;
#endif

// Base increased from 5000 to 10000 -- see Issue 1220072.
static constexpr int kGPUInfoWatchdogTimeoutMs =
    10000 * kGPUInfoWatchdogTimeoutMultiplierOS *
    kGPUInfoWatchdogTimeoutMultiplierASAN;

class AuxGPUInfoEnumerator : public gpu::GPUInfo::Enumerator {
 public:
  explicit AuxGPUInfoEnumerator(base::Value::Dict* dictionary)
      : dictionary_(*dictionary) {}

 private:
  template <typename T>
  void MaybeSetAuxAttribute(const char* name, T value) {
    if (in_aux_attributes_)
      dictionary_->Set(name, value);
  }

  void AddInt64(const char* name, int64_t value) override {
    AddInt(name, value);
  }
  void AddInt(const char* name, int value) override {
    MaybeSetAuxAttribute(name, value);
  }
  void AddString(const char* name, const std::string& value) override {
    MaybeSetAuxAttribute(name, value);
  }
  void AddBool(const char* name, bool value) override {
    MaybeSetAuxAttribute(name, value);
  }
  void AddTimeDeltaInSecondsF(const char* name,
                              const base::TimeDelta& value) override {
    MaybeSetAuxAttribute(name, value.InSecondsF());
  }

  void AddBinary(const char* name,
                 const base::span<const uint8_t>& value) override {
    // TODO(penghuang): send vulkan info to DevTools.
  }

  void BeginGPUDevice() override {}

  void EndGPUDevice() override {}

  void BeginVideoDecodeAcceleratorSupportedProfile() override {}

  void EndVideoDecodeAcceleratorSupportedProfile() override {}

  void BeginVideoEncodeAcceleratorSupportedProfile() override {}

  void EndVideoEncodeAcceleratorSupportedProfile() override {}

  void BeginImageDecodeAcceleratorSupportedProfile() override {}

  void EndImageDecodeAcceleratorSupportedProfile() override {}

  void BeginOverlayInfo() override {}

  void EndOverlayInfo() override {}

  void BeginAuxAttributes() override {
    in_aux_attributes_ = true;
  }

  void EndAuxAttributes() override {
    in_aux_attributes_ = false;
  }

  const raw_ref<protocol::DictionaryValue, DanglingUntriaged> dictionary_;
  bool in_aux_attributes_ = false;
};

std::unique_ptr<GPUDevice> GPUDeviceToProtocol(
    const gpu::GPUInfo::GPUDevice& device) {
  return GPUDevice::Create()
      .SetVendorId(device.vendor_id)
      .SetDeviceId(device.device_id)
#if BUILDFLAG(IS_WIN)
      .SetSubSysId(device.sub_sys_id)
      .SetRevision(device.revision)
#endif
      .SetVendorString(device.vendor_string)
      .SetDeviceString(device.device_string)
      .SetDriverVendor(device.driver_vendor)
      .SetDriverVersion(device.driver_version)
      .Build();
}

std::unique_ptr<SystemInfo::VideoDecodeAcceleratorCapability>
VideoDecodeAcceleratorSupportedProfileToProtocol(
    const gpu::VideoDecodeAcceleratorSupportedProfile& profile) {
  return SystemInfo::VideoDecodeAcceleratorCapability::Create()
      .SetProfile(media::GetProfileName(
          static_cast<media::VideoCodecProfile>(profile.profile)))
      .SetMaxResolution(GfxSizeToSystemInfoSize(profile.max_resolution))
      .SetMinResolution(GfxSizeToSystemInfoSize(profile.min_resolution))
      .Build();
}

std::unique_ptr<SystemInfo::VideoEncodeAcceleratorCapability>
VideoEncodeAcceleratorSupportedProfileToProtocol(
    const gpu::VideoEncodeAcceleratorSupportedProfile& profile) {
  return SystemInfo::VideoEncodeAcceleratorCapability::Create()
      .SetProfile(media::GetProfileName(
          static_cast<media::VideoCodecProfile>(profile.profile)))
      .SetMaxResolution(GfxSizeToSystemInfoSize(profile.max_resolution))
      .SetMaxFramerateNumerator(profile.max_framerate_numerator)
      .SetMaxFramerateDenominator(profile.max_framerate_denominator)
      .Build();
}

std::unique_ptr<SystemInfo::ImageDecodeAcceleratorCapability>
ImageDecodeAcceleratorSupportedProfileToProtocol(
    const gpu::ImageDecodeAcceleratorSupportedProfile& profile) {
  auto subsamplings = std::make_unique<protocol::Array<std::string>>();
  for (const auto subsampling : profile.subsamplings) {
    switch (subsampling) {
      case gpu::ImageDecodeAcceleratorSubsampling::k420:
        subsamplings->emplace_back(SystemInfo::SubsamplingFormatEnum::Yuv420);
        break;
      case gpu::ImageDecodeAcceleratorSubsampling::k422:
        subsamplings->emplace_back(SystemInfo::SubsamplingFormatEnum::Yuv422);
        break;
      case gpu::ImageDecodeAcceleratorSubsampling::k444:
        subsamplings->emplace_back(SystemInfo::SubsamplingFormatEnum::Yuv444);
        break;
    }
  }

  SystemInfo::ImageType image_type;
  switch (profile.image_type) {
    case gpu::ImageDecodeAcceleratorType::kJpeg:
      image_type = SystemInfo::ImageTypeEnum::Jpeg;
      break;
    case gpu::ImageDecodeAcceleratorType::kWebP:
      image_type = SystemInfo::ImageTypeEnum::Webp;
      break;
    case gpu::ImageDecodeAcceleratorType::kUnknown:
      image_type = SystemInfo::ImageTypeEnum::Unknown;
      break;
  }

  return SystemInfo::ImageDecodeAcceleratorCapability::Create()
      .SetImageType(image_type)
      .SetMaxDimensions(GfxSizeToSystemInfoSize(profile.max_encoded_dimensions))
      .SetMinDimensions(GfxSizeToSystemInfoSize(profile.min_encoded_dimensions))
      .SetSubsamplings(std::move(subsamplings))
      .Build();
}

void SendGetInfoResponse(std::unique_ptr<GetInfoCallback> callback) {
  gpu::GPUInfo gpu_info = GpuDataManagerImpl::GetInstance()->GetGPUInfo();
  auto devices = std::make_unique<protocol::Array<GPUDevice>>();
  // The active device should be the 0th device
  for (size_t i = 0; i < gpu_info.secondary_gpus.size(); ++i) {
    if (gpu_info.secondary_gpus[i].active)
      devices->emplace_back(GPUDeviceToProtocol(gpu_info.secondary_gpus[i]));
  }
  devices->emplace_back(GPUDeviceToProtocol(gpu_info.gpu));
  for (size_t i = 0; i < gpu_info.secondary_gpus.size(); ++i) {
    if (gpu_info.secondary_gpus[i].active)
      continue;
    devices->emplace_back(GPUDeviceToProtocol(gpu_info.secondary_gpus[i]));
  }
  auto aux_attributes = std::make_unique<base::Value::Dict>();
  AuxGPUInfoEnumerator enumerator(aux_attributes.get());
  gpu_info.EnumerateFields(&enumerator);
  aux_attributes->Set("processCrashCount", GpuProcessHost::GetGpuCrashCount());
  aux_attributes->Set(
      "visibilityCallbackCallCount",
      static_cast<int>(gpu_info.visibility_callback_call_count));

  auto feature_status = std::make_unique<base::Value::Dict>(
      std::move(GetFeatureStatus().GetDict()));
  auto driver_bug_workarounds =
      std::make_unique<protocol::Array<std::string>>(GetDriverBugWorkarounds());

  auto decoding_profiles = std::make_unique<
      protocol::Array<SystemInfo::VideoDecodeAcceleratorCapability>>();
  for (const auto& profile :
       gpu_info.video_decode_accelerator_supported_profiles) {
    decoding_profiles->emplace_back(
        VideoDecodeAcceleratorSupportedProfileToProtocol(profile));
  }

  auto encoding_profiles = std::make_unique<
      protocol::Array<SystemInfo::VideoEncodeAcceleratorCapability>>();
  for (const auto& profile :
       gpu_info.video_encode_accelerator_supported_profiles) {
    encoding_profiles->emplace_back(
        VideoEncodeAcceleratorSupportedProfileToProtocol(profile));
  }

  auto image_profiles = std::make_unique<
      protocol::Array<SystemInfo::ImageDecodeAcceleratorCapability>>();
  for (const auto& profile :
       gpu_info.image_decode_accelerator_supported_profiles) {
    image_profiles->emplace_back(
        ImageDecodeAcceleratorSupportedProfileToProtocol(profile));
  }

  std::unique_ptr<GPUInfo> gpu =
      GPUInfo::Create()
          .SetDevices(std::move(devices))
          .SetAuxAttributes(std::move(aux_attributes))
          .SetFeatureStatus(std::move(feature_status))
          .SetDriverBugWorkarounds(std::move(driver_bug_workarounds))
          .SetVideoDecoding(std::move(decoding_profiles))
          .SetVideoEncoding(std::move(encoding_profiles))
          .SetImageDecoding(std::move(image_profiles))
          .Build();

  base::CommandLine* command = base::CommandLine::ForCurrentProcess();
#if BUILDFLAG(IS_WIN)
  std::string command_string =
      base::WideToUTF8(command->GetCommandLineString());
#else
  std::string command_string = command->GetCommandLineString();
#endif

  callback->sendSuccess(std::move(gpu), gpu_info.machine_model_name,
                        gpu_info.machine_model_version, command_string);
}

}  // namespace

class SystemInfoHandlerGpuObserver : public content::GpuDataManagerObserver {
 public:
  explicit SystemInfoHandlerGpuObserver(
      std::unique_ptr<GetInfoCallback> callback)
      : callback_(std::move(callback)) {
    GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SystemInfoHandlerGpuObserver::ObserverWatchdogCallback,
                       weak_factory_.GetWeakPtr()),
        base::Milliseconds(kGPUInfoWatchdogTimeoutMs));

    GpuDataManagerImpl::GetInstance()->AddObserver(this);
    OnGpuInfoUpdate();
  }

  void OnGpuInfoUpdate() override {
    if (!GpuDataManagerImpl::GetInstance()->IsGpuFeatureInfoAvailable())
      return;
    base::CommandLine* command = base::CommandLine::ForCurrentProcess();
    // Only wait for DX12/Vulkan info if requested at Chrome start up.
    if (!command->HasSwitch(
            switches::kDisableGpuProcessForDX12InfoCollection) &&
        command->HasSwitch(switches::kNoDelayForDX12VulkanInfoCollection) &&
        !GpuDataManagerImpl::GetInstance()->IsDx12VulkanVersionAvailable())
      return;
    UnregisterAndSendResponse();
  }

  void OnGpuProcessCrashed() override { UnregisterAndSendResponse(); }

  void ObserverWatchdogCallback() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    CHECK(false) << "Gathering system GPU info took more than "
                 << (kGPUInfoWatchdogTimeoutMs / 1000) << " seconds.";
  }

  void UnregisterAndSendResponse() {
    GpuDataManagerImpl::GetInstance()->RemoveObserver(this);
    SendGetInfoResponse(std::move(callback_));
    delete this;
  }

 private:
  std::unique_ptr<GetInfoCallback> callback_;
  base::WeakPtrFactory<SystemInfoHandlerGpuObserver> weak_factory_{this};
};

SystemInfoHandler::SystemInfoHandler(bool is_browser_session)
    : DevToolsDomainHandler(SystemInfo::Metainfo::domainName),
      is_browser_session_(is_browser_session) {}

SystemInfoHandler::~SystemInfoHandler() = default;

void SystemInfoHandler::Wire(UberDispatcher* dispatcher) {
  SystemInfo::Dispatcher::wire(dispatcher, this);
}

void SystemInfoHandler::GetInfo(std::unique_ptr<GetInfoCallback> callback) {
  if (!is_browser_session_) {
    callback->sendFailure(Response::ServerError(
        "SystemInfo.getInfo is only supported on the browser target"));
    return;
  }

  // We will be able to get more information from the GpuDataManager.
  // Register a transient observer with it to call us back when the
  // information is available.
  new SystemInfoHandlerGpuObserver(std::move(callback));
}

namespace {

std::unique_ptr<base::ProcessMetrics> CreateProcessMetrics(
    base::ProcessHandle handle) {
#if BUILDFLAG(IS_MAC)
  return base::ProcessMetrics::CreateProcessMetrics(
      handle, content::BrowserChildProcessHost::GetPortProvider());
#else
  return base::ProcessMetrics::CreateProcessMetrics(handle);
#endif
}

std::unique_ptr<protocol::SystemInfo::ProcessInfo> MakeProcessInfo(
    const base::Process& process,
    const String& process_type) {
  std::unique_ptr<base::ProcessMetrics> pm =
      CreateProcessMetrics(process.Handle());
  const base::TimeDelta cpu_usage =
      pm->GetCumulativeCPUUsage().value_or(base::TimeDelta());

  return SystemInfo::ProcessInfo::Create()
      .SetId(process.Pid())
      .SetType(process_type)
      .SetCpuTime(cpu_usage.InSecondsF())
      .Build();
}

void AddBrowserProcessInfo(
    protocol::Array<protocol::SystemInfo::ProcessInfo>* process_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  process_info->emplace_back(
      MakeProcessInfo(base::Process::Current(), "browser"));
}

void AddRendererProcessInfo(
    protocol::Array<protocol::SystemInfo::ProcessInfo>* process_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    RenderProcessHost* host = it.GetCurrentValue();
    if (host->GetProcess().IsValid()) {
      process_info->emplace_back(
          MakeProcessInfo(host->GetProcess(), "renderer"));
    }
  }
}

void AddChildProcessInfo(
    protocol::Array<protocol::SystemInfo::ProcessInfo>* process_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (BrowserChildProcessHostIterator it; !it.Done(); ++it) {
    const ChildProcessData& process_data = it.GetData();
    const base::Process& process = process_data.GetProcess();
    if (process.IsValid()) {
      process_info->emplace_back(
          MakeProcessInfo(process, process_data.metrics_name));
    }
  }
}

}  // namespace

void SystemInfoHandler::GetProcessInfo(
    std::unique_ptr<GetProcessInfoCallback> callback) {
  if (!is_browser_session_) {
    callback->sendFailure(Response::ServerError(
        "SystemInfo.getProcessInfo is only supported on the browser target"));
    return;
  }

  auto process_info =
      std::make_unique<protocol::Array<SystemInfo::ProcessInfo>>();

  AddBrowserProcessInfo(process_info.get());
  AddRendererProcessInfo(process_info.get());
  AddChildProcessInfo(process_info.get());
  callback->sendSuccess(std::move(process_info));
}

Response SystemInfoHandler::GetFeatureState(const String& in_featureState,
                                            bool* featureEnabled) {
  return Response::InvalidParams("Unknown feature");
}

}  // namespace protocol
}  // namespace content
