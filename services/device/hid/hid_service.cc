// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_service.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"

#if defined(OS_LINUX) && defined(USE_UDEV)
#include "services/device/hid/hid_service_linux.h"
#elif defined(OS_MACOSX)
#include "services/device/hid/hid_service_mac.h"
#elif defined(OS_WIN)
#include "services/device/hid/hid_service_win.h"
#endif

namespace device {

void HidService::Observer::OnDeviceAdded(mojom::HidDeviceInfoPtr device_info) {}

void HidService::Observer::OnDeviceRemoved(
    mojom::HidDeviceInfoPtr device_info) {}

// static
constexpr base::TaskTraits HidService::kBlockingTaskTraits;

// static
std::unique_ptr<HidService> HidService::Create() {
#if defined(OS_LINUX) && defined(USE_UDEV)
  return base::WrapUnique(new HidServiceLinux());
#elif defined(OS_MACOSX)
  return base::WrapUnique(new HidServiceMac());
#elif defined(OS_WIN)
  return base::WrapUnique(new HidServiceWin());
#else
  return nullptr;
#endif
}

void HidService::GetDevices(GetDevicesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool was_empty = pending_enumerations_.empty();
  pending_enumerations_.push_back(std::move(callback));
  if (enumeration_ready_ && was_empty) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&HidService::RunPendingEnumerations, GetWeakPtr()));
  }
}

void HidService::AddObserver(HidService::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void HidService::RemoveObserver(HidService::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

HidService::HidService() = default;

HidService::~HidService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void HidService::AddDevice(scoped_refptr<HidDeviceInfo> device_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string device_guid =
      FindDeviceIdByPlatformDeviceId(device_info->platform_device_id());
  if (device_guid.empty()) {
    devices_[device_info->device_guid()] = device_info;

    HID_LOG(USER) << "HID device "
                  << (enumeration_ready_ ? "added" : "detected")
                  << ": vendorId=" << device_info->vendor_id()
                  << ", productId=" << device_info->product_id() << ", name='"
                  << device_info->product_name() << "', serial='"
                  << device_info->serial_number() << "', deviceId='"
                  << device_info->platform_device_id() << "'";

    if (enumeration_ready_) {
      for (auto& observer : observer_list_)
        observer.OnDeviceAdded(device_info->device()->Clone());
    }
  }
}

void HidService::RemoveDevice(const HidPlatformDeviceId& platform_device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string device_guid = FindDeviceIdByPlatformDeviceId(platform_device_id);
  if (!device_guid.empty()) {
    HID_LOG(USER) << "HID device removed: deviceId='" << platform_device_id
                  << "'";
    DCHECK(base::Contains(devices_, device_guid));

    scoped_refptr<HidDeviceInfo> device_info = devices_[device_guid];
    if (enumeration_ready_) {
      for (auto& observer : observer_list_)
        observer.OnDeviceRemoved(device_info->device()->Clone());
    }
    devices_.erase(device_guid);
  }
}

void HidService::RunPendingEnumerations() {
  DCHECK(enumeration_ready_);
  DCHECK(!pending_enumerations_.empty());

  std::vector<GetDevicesCallback> callbacks;
  callbacks.swap(pending_enumerations_);

  // Clone and pass mojom::HidDeviceInfoPtr vector for each clients.
  for (auto& callback : callbacks) {
    std::vector<mojom::HidDeviceInfoPtr> devices;
    for (const auto& map_entry : devices_)
      devices.push_back(map_entry.second->device()->Clone());
    std::move(callback).Run(std::move(devices));
  }
}

void HidService::FirstEnumerationComplete() {
  enumeration_ready_ = true;
  if (!pending_enumerations_.empty()) {
    RunPendingEnumerations();
  }
}

std::string HidService::FindDeviceIdByPlatformDeviceId(
    const HidPlatformDeviceId& platform_device_id) {
  for (const auto& map_entry : devices_) {
    if (map_entry.second->platform_device_id() == platform_device_id) {
      return map_entry.first;
    }
  }
  return std::string();
}

}  // namespace device
