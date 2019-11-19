// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_storage/system_storage_api.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "content/public/browser/browser_thread.h"

using storage_monitor::StorageMonitor;

namespace extensions {

using api::system_storage::StorageUnitInfo;
namespace EjectDevice = api::system_storage::EjectDevice;
namespace GetAvailableCapacity = api::system_storage::GetAvailableCapacity;

SystemStorageGetInfoFunction::SystemStorageGetInfoFunction() {
}

SystemStorageGetInfoFunction::~SystemStorageGetInfoFunction() {
}

ExtensionFunction::ResponseAction SystemStorageGetInfoFunction::Run() {
  StorageInfoProvider::Get()->StartQueryInfo(base::Bind(
      &SystemStorageGetInfoFunction::OnGetStorageInfoCompleted, this));
  return RespondLater();
}

void SystemStorageGetInfoFunction::OnGetStorageInfoCompleted(bool success) {
  if (success) {
    Respond(ArgumentList(api::system_storage::GetInfo::Results::Create(
        StorageInfoProvider::Get()->storage_unit_info_list())));
  } else {
    Respond(Error("Error occurred when querying storage information."));
  }
}

SystemStorageEjectDeviceFunction::~SystemStorageEjectDeviceFunction() {
}

ExtensionFunction::ResponseAction SystemStorageEjectDeviceFunction::Run() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<EjectDevice::Params> params(
      EjectDevice::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  StorageMonitor::GetInstance()->EnsureInitialized(
      base::Bind(&SystemStorageEjectDeviceFunction::OnStorageMonitorInit,
                 this,
                 params->id));
  // EnsureInitialized() above can result in synchronous Respond().
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void SystemStorageEjectDeviceFunction::OnStorageMonitorInit(
    const std::string& transient_device_id) {
  DCHECK(StorageMonitor::GetInstance()->IsInitialized());
  StorageMonitor* monitor = StorageMonitor::GetInstance();
  std::string device_id_str =
      StorageMonitor::GetInstance()->GetDeviceIdForTransientId(
          transient_device_id);

  if (device_id_str.empty()) {
    HandleResponse(StorageMonitor::EJECT_NO_SUCH_DEVICE);
    return;
  }

  monitor->EjectDevice(
      device_id_str,
      base::Bind(&SystemStorageEjectDeviceFunction::HandleResponse, this));
}

void SystemStorageEjectDeviceFunction::HandleResponse(
    StorageMonitor::EjectStatus status) {
  api::system_storage::EjectDeviceResultCode result =
      api::system_storage::EJECT_DEVICE_RESULT_CODE_FAILURE;
  switch (status) {
    case StorageMonitor::EJECT_OK:
      result = api::system_storage::EJECT_DEVICE_RESULT_CODE_SUCCESS;
      break;
    case StorageMonitor::EJECT_IN_USE:
      result = api::system_storage::EJECT_DEVICE_RESULT_CODE_IN_USE;
      break;
    case StorageMonitor::EJECT_NO_SUCH_DEVICE:
      result = api::system_storage::EJECT_DEVICE_RESULT_CODE_NO_SUCH_DEVICE;
      break;
    case StorageMonitor::EJECT_FAILURE:
      result = api::system_storage::EJECT_DEVICE_RESULT_CODE_FAILURE;
  }

  Respond(OneArgument(
      std::make_unique<base::Value>(api::system_storage::ToString(result))));
}

SystemStorageGetAvailableCapacityFunction::
    SystemStorageGetAvailableCapacityFunction()
    : query_runner_(base::CreateSequencedTaskRunner(
          base::TaskTraits(base::ThreadPool(),
                           base::TaskPriority::BEST_EFFORT,
                           base::MayBlock(),
                           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN))) {}

SystemStorageGetAvailableCapacityFunction::
    ~SystemStorageGetAvailableCapacityFunction() {
}

ExtensionFunction::ResponseAction
SystemStorageGetAvailableCapacityFunction::Run() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<GetAvailableCapacity::Params> params(
      GetAvailableCapacity::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  StorageMonitor::GetInstance()->EnsureInitialized(base::Bind(
      &SystemStorageGetAvailableCapacityFunction::OnStorageMonitorInit,
      this,
      params->id));
  return RespondLater();
}

void SystemStorageGetAvailableCapacityFunction::OnStorageMonitorInit(
    const std::string& transient_id) {
  base::PostTaskAndReplyWithResult(
      query_runner_.get(), FROM_HERE,
      base::BindOnce(
          &StorageInfoProvider::GetStorageFreeSpaceFromTransientIdAsync,
          StorageInfoProvider::Get(), transient_id),
      base::BindOnce(
          &SystemStorageGetAvailableCapacityFunction::OnQueryCompleted, this,
          transient_id));
}

void SystemStorageGetAvailableCapacityFunction::OnQueryCompleted(
    const std::string& transient_id,
    double available_capacity) {
  bool success = available_capacity >= 0;
  if (success) {
    api::system_storage::StorageAvailableCapacityInfo result;
    result.id = transient_id;
    result.available_capacity = available_capacity;
    Respond(OneArgument(result.ToValue()));
  } else {
    Respond(Error("Error occurred when querying available capacity."));
  }
}

}  // namespace extensions
