// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/system_info_handler.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_task_traits.h"
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

// Give the GPU process a few seconds to provide GPU info.
// Linux Debug builds need more time -- see Issue 796437.
// Windows builds need more time -- see Issue 873112.
#if (defined(OS_LINUX) && !defined(NDEBUG)) || defined(OS_WIN)
const int kGPUInfoWatchdogTimeoutMs = 20000;
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

  void BeginGPUDevice() override {}

  void EndGPUDevice() override {}

  void BeginVideoDecodeAcceleratorSupportedProfile() override {}

  void EndVideoDecodeAcceleratorSupportedProfile() override {}

  void BeginVideoEncodeAcceleratorSupportedProfile() override {}

  void EndVideoEncodeAcceleratorSupportedProfile() override {}

  void BeginOverlayCapability() override {}

  void EndOverlayCapability() override {}

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
  return GPUDevice::Create().SetVendorId(device.vendor_id)
                            .SetDeviceId(device.device_id)
                            .SetVendorString(device.vendor_string)
                            .SetDeviceString(device.device_string)
                            .Build();
}

void SendGetInfoResponse(std::unique_ptr<GetInfoCallback> callback) {
  gpu::GPUInfo gpu_info = GpuDataManagerImpl::GetInstance()->GetGPUInfo();
  std::unique_ptr<protocol::Array<GPUDevice>> devices =
      protocol::Array<GPUDevice>::create();
  devices->addItem(GPUDeviceToProtocol(gpu_info.gpu));
  for (const auto& device : gpu_info.secondary_gpus)
    devices->addItem(GPUDeviceToProtocol(device));

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

  std::unique_ptr<protocol::Array<std::string>> driver_bug_workarounds =
      protocol::Array<std::string>::create();
  for (const std::string& s : GetDriverBugWorkarounds())
      driver_bug_workarounds->addItem(s);

  std::unique_ptr<GPUInfo> gpu = GPUInfo::Create()
      .SetDevices(std::move(devices))
      .SetAuxAttributes(std::move(aux_attributes))
      .SetFeatureStatus(std::move(feature_status))
      .SetDriverBugWorkarounds(std::move(driver_bug_workarounds))
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
      : callback_(std::move(callback)),
        weak_factory_(this) {
    base::PostDelayedTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&SystemInfoHandlerGpuObserver::ObserverWatchdogCallback,
                       weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kGPUInfoWatchdogTimeoutMs));

    GpuDataManagerImpl::GetInstance()->AddObserver(this);
    OnGpuInfoUpdate();
  }

  void OnGpuInfoUpdate() override {
    if (GpuDataManagerImpl::GetInstance()->IsGpuFeatureInfoAvailable())
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
  base::WeakPtrFactory<SystemInfoHandlerGpuObserver> weak_factory_;
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

}  // namespace protocol
}  // namespace content
