// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_service.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/not_fatal_until.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/usb/usb_device.h"
#include "services/device/usb/usb_device_handle.h"

#if BUILDFLAG(IS_ANDROID)
#include "services/device/usb/usb_service_android.h"
#elif defined(USE_UDEV)
#include "services/device/usb/usb_service_linux.h"
#elif BUILDFLAG(IS_MAC)
#include "services/device/usb/usb_service_impl.h"
#elif BUILDFLAG(IS_WIN)
#include "services/device/usb/usb_service_win.h"
#endif

namespace device {

UsbService::Observer::~Observer() = default;

void UsbService::Observer::OnDeviceAdded(scoped_refptr<UsbDevice> device) {}

void UsbService::Observer::OnDeviceRemoved(scoped_refptr<UsbDevice> device) {}

void UsbService::Observer::OnDeviceRemovedCleanup(
    scoped_refptr<UsbDevice> device) {}

void UsbService::Observer::WillDestroyUsbService() {}

// Declare storage for this constexpr.
constexpr base::TaskTraits UsbService::kBlockingTaskTraits;

// static
std::unique_ptr<UsbService> UsbService::Create() {
#if BUILDFLAG(IS_ANDROID)
  return base::WrapUnique(new UsbServiceAndroid());
#elif defined(USE_UDEV)
  return base::WrapUnique(new UsbServiceLinux());
#elif BUILDFLAG(IS_WIN)
  return base::WrapUnique(new UsbServiceWin());
#elif BUILDFLAG(IS_MAC)
  return base::WrapUnique(new UsbServiceImpl());
#else
  return nullptr;
#endif
}

// static
scoped_refptr<base::SequencedTaskRunner>
UsbService::CreateBlockingTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(kBlockingTaskTraits);
}

UsbService::~UsbService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

UsbService::UsbService() {}

scoped_refptr<UsbDevice> UsbService::GetDevice(const std::string& guid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = devices_.find(guid);
  if (it == devices_.end())
    return nullptr;
  return it->second;
}

void UsbService::GetDevices(GetDevicesCallback callback) {
  std::vector<scoped_refptr<UsbDevice>> devices;
  devices.reserve(devices_.size());
  for (const auto& map_entry : devices_)
    devices.push_back(map_entry.second);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), devices));
}

void UsbService::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.AddObserver(observer);
}

void UsbService::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.RemoveObserver(observer);
}

void UsbService::AddDeviceForTesting(scoped_refptr<UsbDevice> device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!base::Contains(devices_, device->guid()));
  devices_[device->guid()] = device;
  testing_devices_.insert(device->guid());
  NotifyDeviceAdded(device);
}

void UsbService::RemoveDeviceForTesting(const std::string& device_guid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Allow only devices added with AddDeviceForTesting to be removed with this
  // method.
  auto testing_devices_it = testing_devices_.find(device_guid);
  if (testing_devices_it != testing_devices_.end()) {
    auto devices_it = devices_.find(device_guid);
    CHECK(devices_it != devices_.end(), base::NotFatalUntil::M130);
    scoped_refptr<UsbDevice> device = devices_it->second;
    devices_.erase(devices_it);
    testing_devices_.erase(testing_devices_it);
    NotifyDeviceRemoved(device);
  }
}

void UsbService::GetTestDevices(
    std::vector<scoped_refptr<UsbDevice>>* devices) {
  devices->clear();
  devices->reserve(testing_devices_.size());
  for (const std::string& guid : testing_devices_) {
    auto it = devices_.find(guid);
    CHECK(it != devices_.end(), base::NotFatalUntil::M130);
    devices->push_back(it->second);
  }
}

void UsbService::NotifyDeviceAdded(scoped_refptr<UsbDevice> device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : observer_list_)
    observer.OnDeviceAdded(device);
}

void UsbService::NotifyDeviceRemoved(scoped_refptr<UsbDevice> device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : observer_list_)
    observer.OnDeviceRemoved(device);
  device->NotifyDeviceRemoved();
  for (auto& observer : observer_list_)
    observer.OnDeviceRemovedCleanup(device);
}

void UsbService::NotifyWillDestroyUsbService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observer_list_)
    observer.WillDestroyUsbService();
}

}  // namespace device
