// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_storage/system_storage_api.h"

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/system_storage/storage_info_provider.h"

using storage_monitor::StorageMonitor;

namespace extensions {

using api::system_storage::StorageUnitInfo;
namespace EjectDevice = api::system_storage::EjectDevice;
namespace GetAvailableCapacity = api::system_storage::GetAvailableCapacity;

ExtensionFunction::ResponseAction SystemStorageGetInfoFunction::Run() {
  StorageInfoProvider::Get()->StartQueryInfo(base::BindOnce(
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

ExtensionFunction::ResponseAction SystemStorageEjectDeviceFunction::Run() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::optional<EjectDevice::Params> params =
      EjectDevice::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  StorageMonitor::GetInstance()->EnsureInitialized(
      base::BindOnce(&SystemStorageEjectDeviceFunction::OnStorageMonitorInit,
                     this, params->id));
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
      base::BindOnce(&SystemStorageEjectDeviceFunction::HandleResponse, this));
}

void SystemStorageEjectDeviceFunction::HandleResponse(
    StorageMonitor::EjectStatus status) {
  api::system_storage::EjectDeviceResultCode result =
      api::system_storage::EjectDeviceResultCode::kFailure;
  switch (status) {
    case StorageMonitor::EJECT_OK:
      result = api::system_storage::EjectDeviceResultCode::kSuccess;
      break;
    case StorageMonitor::EJECT_IN_USE:
      result = api::system_storage::EjectDeviceResultCode::kInUse;
      break;
    case StorageMonitor::EJECT_NO_SUCH_DEVICE:
      result = api::system_storage::EjectDeviceResultCode::kNoSuchDevice;
      break;
    case StorageMonitor::EJECT_FAILURE:
      result = api::system_storage::EjectDeviceResultCode::kFailure;
  }

  Respond(WithArguments(api::system_storage::ToString(result)));
}

SystemStorageGetAvailableCapacityFunction::
    SystemStorageGetAvailableCapacityFunction()
    : query_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

SystemStorageGetAvailableCapacityFunction::
    ~SystemStorageGetAvailableCapacityFunction() = default;

ExtensionFunction::ResponseAction
SystemStorageGetAvailableCapacityFunction::Run() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::optional<GetAvailableCapacity::Params> params =
      GetAvailableCapacity::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  StorageMonitor::GetInstance()->EnsureInitialized(base::BindOnce(
      &SystemStorageGetAvailableCapacityFunction::OnStorageMonitorInit, this,
      params->id));
  return RespondLater();
}

void SystemStorageGetAvailableCapacityFunction::OnStorageMonitorInit(
    const std::string& transient_id) {
  query_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
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
    Respond(WithArguments(result.ToValue()));
  } else {
    Respond(Error("Error occurred when querying available capacity."));
  }
}

}  // namespace extensions
