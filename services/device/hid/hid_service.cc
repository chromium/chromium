// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_service.h"

#include <sstream>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"

#if (defined(OS_LINUX) || defined(OS_CHROMEOS)) && defined(USE_UDEV)
#include "services/device/hid/hid_service_linux.h"
#elif defined(OS_MAC)
#include "services/device/hid/hid_service_mac.h"
#elif defined(OS_WIN)
#include "services/device/hid/hid_service_win.h"
#endif

namespace device {

namespace {

// Formats the platform device IDs in |platform_device_id_map| into a
// comma-separated list for logging. The report IDs are not logged.
std::string PlatformDeviceIdsToString(
    const HidDeviceInfo::PlatformDeviceIdMap& platform_device_id_map) {
  std::ostringstream buf("'");
  bool first = true;
  for (const auto& entry : platform_device_id_map) {
    if (!first)
      buf << "', '";
    first = false;
    buf << entry.platform_device_id;
  }
  buf << "'";
  return buf.str();
}

}  // namespace

void HidService::Observer::OnDeviceAdded(mojom::HidDeviceInfoPtr device_info) {}

void HidService::Observer::OnDeviceRemoved(
    mojom::HidDeviceInfoPtr device_info) {}

// static
constexpr base::TaskTraits HidService::kBlockingTaskTraits;

// static
std::unique_ptr<HidService> HidService::Create() {
#if (defined(OS_LINUX) || defined(OS_CHROMEOS)) && defined(USE_UDEV)
  return base::WrapUnique(new HidServiceLinux());
#elif defined(OS_MAC)
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

  base::Optional<std::string> found_guid = base::nullopt;
  for (const auto& entry : device_info->platform_device_id_map()) {
    if ((found_guid = FindDeviceGuidInDeviceMap(entry.platform_device_id)))
      break;
  }
  if (!found_guid) {
    devices_[device_info->device_guid()] = device_info;

    HID_LOG(USER) << "HID device "
                  << (enumeration_ready_ ? "added" : "detected")
                  << ": vendorId=" << device_info->vendor_id()
                  << ", productId=" << device_info->product_id() << ", name='"
                  << device_info->product_name() << "', serial='"
                  << device_info->serial_number() << "', deviceIds=["
                  << PlatformDeviceIdsToString(
                         device_info->platform_device_id_map())
                  << "]";

    if (enumeration_ready_) {
      for (auto& observer : observer_list_)
        observer.OnDeviceAdded(device_info->device()->Clone());
    }
  }
}

void HidService::RemoveDevice(const HidPlatformDeviceId& platform_device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto found_guid = FindDeviceGuidInDeviceMap(platform_device_id);
  if (found_guid) {
    HID_LOG(USER) << "HID device removed: deviceId='" << platform_device_id
                  << "'";
    DCHECK(base::Contains(devices_, *found_guid));

    scoped_refptr<HidDeviceInfo> device_info = devices_[*found_guid];
    if (enumeration_ready_) {
      for (auto& observer : observer_list_)
        observer.OnDeviceRemoved(device_info->device()->Clone());
    }
    devices_.erase(*found_guid);
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

base::Optional<std::string> HidService::FindDeviceGuidInDeviceMap(
    const HidPlatformDeviceId& platform_device_id) {
  for (const auto& device_entry : devices_) {
    const auto& platform_device_map =
        device_entry.second->platform_device_id_map();
    for (const auto& platform_device_entry : platform_device_map) {
      if (platform_device_entry.platform_device_id == platform_device_id)
        return device_entry.first;
    }
  }
  return base::nullopt;
}

}  // namespace device
