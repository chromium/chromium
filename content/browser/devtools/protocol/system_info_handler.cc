// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/system_info_handler.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/process/process_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/render_process_host.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_switches.h"
#if defined(OS_CHROMEOS)
#include "gpu/config/gpu_util.h"
#endif

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
// Linux Debug builds need more time -- see Issue 796437.
// Windows builds need more time -- see Issue 873112 and 1004472.
#if (defined(OS_LINUX) && !defined(NDEBUG))
const int kGPUInfoWatchdogTimeoutMs = 20000;
#elif defined(OS_WIN)
const int kGPUInfoWatchdogTimeoutMs = 30000;
#else
const int kGPUInfoWatchdogTimeoutMs = 5000;
#endif

class AuxGPUInfoEnumerator : public gpu::GPUInfo::Enumerator {
 public:
  AuxGPUInfoEnumerator(protocol::DictionaryValue* dictionary)
      : dictionary_(dictionary),
        in_aux_attributes_(false) { }

  void AddInt64(const char* name, int64_t value) override {
    if (in_aux_attributes_)
      dictionary_->setDouble(name, value);
  }

  void AddInt(const char* name, int value) override {
    if (in_aux_attributes_)
      dictionary_->setInteger(name, value);
  }

  void AddString(const char* name, const std::string& value) override {
    if (in_aux_attributes_)
      dictionary_->setString(name, value);
  }

  void AddBool(const char* name, bool value) override {
    if (in_aux_attributes_)
      dictionary_->setBoolean(name, value);
  }

  void AddTimeDeltaInSecondsF(const char* name,
                              const base::TimeDelta& value) override {
    if (in_aux_attributes_)
      dictionary_->setDouble(name, value.InSecondsF());
  }

  void AddBinary(const char* name,
                 const base::span<const uint8_t>& value) override {
    // TODO(penghuang): send vulkan info to devtool
  }

  void BeginGPUDevice() override {}

  void EndGPUDevice() override {}

  void BeginVideoDecodeAcceleratorSupportedProfile() override {}

  void EndVideoDecodeAcceleratorSupportedProfile() override {}

  void BeginVideoEncodeAcceleratorSupportedProfile() override {}

  void EndVideoEncodeAcceleratorSupportedProfile() override {}

  void BeginImageDecodeAcceleratorSupportedProfile() override {}

  void EndImageDecodeAcceleratorSupportedProfile() override {}

  void BeginDx12VulkanVersionInfo() override {}

  void EndDx12VulkanVersionInfo() override {}

  void BeginAuxAttributes() override {
    in_aux_attributes_ = true;
  }

  void EndAuxAttributes() override {
    in_aux_attributes_ = false;
  }

 private:
  protocol::DictionaryValue* dictionary_;
  bool in_aux_attributes_;
};

std::unique_ptr<GPUDevice> GPUDeviceToProtocol(
    const gpu::GPUInfo::GPUDevice& device) {
  return GPUDevice::Create()
      .SetVendorId(device.vendor_id)
      .SetDeviceId(device.device_id)
#if defined(OS_WIN)
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
  std::unique_ptr<protocol::DictionaryValue> aux_attributes =
      protocol::DictionaryValue::create();
  AuxGPUInfoEnumerator enumerator(aux_attributes.get());
  gpu_info.EnumerateFields(&enumerator);
  enumerator.BeginAuxAttributes();
  enumerator.AddInt("processCrashCount", GpuProcessHost::GetGpuCrashCount());
  enumerator.EndAuxAttributes();

  std::unique_ptr<base::DictionaryValue> base_feature_status =
      GetFeatureStatus();
  std::unique_ptr<protocol::DictionaryValue> feature_status =
      protocol::DictionaryValue::cast(
          protocol::toProtocolValue(base_feature_status.get(), 1000));

  auto driver_bug_workarounds =
      std::make_unique<protocol::Array<std::string>>(GetDriverBugWorkarounds());

  auto decoding_profiles = std::make_unique<
      protocol::Array<SystemInfo::VideoDecodeAcceleratorCapability>>();
  for (const auto& profile :
       gpu_info.video_decode_accelerator_capabilities.supported_profiles) {
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
#if defined(OS_WIN)
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
    base::PostDelayedTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&SystemInfoHandlerGpuObserver::ObserverWatchdogCallback,
                       weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kGPUInfoWatchdogTimeoutMs));

    GpuDataManagerImpl::GetInstance()->AddObserver(this);
    OnGpuInfoUpdate();
  }

  void OnGpuInfoUpdate() override {
    if (!GpuDataManagerImpl::GetInstance()->IsGpuFeatureInfoAvailable())
      return;
    base::CommandLine* command = base::CommandLine::ForCurrentProcess();
    // Only wait for DX12/Vulkan info if requested at Chrome start up.
    if (!command->HasSwitch(
            switches::kDisableGpuProcessForDX12VulkanInfoCollection) &&
        command->HasSwitch(switches::kNoDelayForDX12VulkanInfoCollection) &&
        !GpuDataManagerImpl::GetInstance()->IsDx12VulkanVersionAvailable())
      return;
    UnregisterAndSendResponse();
  }

  void OnGpuProcessCrashed(base::TerminationStatus exit_code) override {
    UnregisterAndSendResponse();
  }

  void ObserverWatchdogCallback() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(OS_CHROMEOS)
    // TODO(zmo): CHECK everywhere once https://crbug.com/796386 is fixed.
    gpu::GpuFeatureInfo gpu_feature_info =
        gpu::ComputeGpuFeatureInfoWithHardwareAccelerationDisabled();
    GpuDataManagerImpl::GetInstance()->UpdateGpuFeatureInfo(gpu_feature_info,
                                                            base::nullopt);
    UnregisterAndSendResponse();
#else
    CHECK(false) << "Gathering system GPU info took more than 5 seconds.";
#endif
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

SystemInfoHandler::SystemInfoHandler()
    : DevToolsDomainHandler(SystemInfo::Metainfo::domainName) {
}

SystemInfoHandler::~SystemInfoHandler() {
}

void SystemInfoHandler::Wire(UberDispatcher* dispatcher) {
  SystemInfo::Dispatcher::wire(dispatcher, this);
}

void SystemInfoHandler::GetInfo(std::unique_ptr<GetInfoCallback> callback) {
  // We will be able to get more information from the GpuDataManager.
  // Register a transient observer with it to call us back when the
  // information is available.
  new SystemInfoHandlerGpuObserver(std::move(callback));
}

namespace {

std::unique_ptr<base::ProcessMetrics> CreateProcessMetrics(
    base::ProcessHandle handle) {
#if defined(OS_MACOSX)
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
  base::TimeDelta cpu_usage = pm->GetCumulativeCPUUsage();

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

std::unique_ptr<protocol::Array<protocol::SystemInfo::ProcessInfo>>
AddChildProcessInfo(
    std::unique_ptr<protocol::Array<protocol::SystemInfo::ProcessInfo>>
        process_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (BrowserChildProcessHostIterator it; !it.Done(); ++it) {
    const ChildProcessData& process_data = it.GetData();
    const base::Process& process = process_data.GetProcess();
    if (process.IsValid()) {
      process_info->emplace_back(
          MakeProcessInfo(process, process_data.metrics_name));
    }
  }

  return process_info;
}

}  // namespace

void SystemInfoHandler::GetProcessInfo(
    std::unique_ptr<GetProcessInfoCallback> callback) {
  auto process_info =
      std::make_unique<protocol::Array<SystemInfo::ProcessInfo>>();

  // Collect browser and renderer processes info on the UI thread.
  AddBrowserProcessInfo(process_info.get());
  AddRendererProcessInfo(process_info.get());

  // Collect child processes info on the IO thread.
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AddChildProcessInfo, std::move(process_info)),
      base::BindOnce(&GetProcessInfoCallback::sendSuccess,
                     std::move(callback)));
}

}  // namespace protocol
}  // namespace content
