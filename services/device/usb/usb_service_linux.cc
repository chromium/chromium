// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/usb/usb_service_linux.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/device_event_log/device_event_log.h"
#include "device/udev_linux/udev_watcher.h"
#include "services/device/usb/usb_device_handle.h"
#include "services/device/usb/usb_device_linux.h"
#include "services/device/usb/webusb_descriptors.h"

namespace device {

namespace {

constexpr std::string_view kUsbSubsystem = "usb";

// Standard USB requests and descriptor types:
const uint16_t kUsbVersion2_1 = 0x0210;

const uint8_t kDeviceClassHub = 0x09;
constexpr int kUsbClassMassStorage = 0x08;

bool ShouldReadDescriptors(const UsbDeviceLinux& device) {
  if (device.usb_version() < kUsbVersion2_1)
    return false;

  // Avoid detaching the usb-storage driver.
  // TODO(crbug.com/40168206): We should read descriptors for composite mass
  // storage devices.
  auto* configuration = device.GetActiveConfiguration();
  if (configuration) {
    for (const auto& interface : configuration->interfaces) {
      for (const auto& alternate : interface->alternates) {
        if (alternate->class_code == kUsbClassMassStorage)
          return false;
      }
    }
  }

  return true;
}

void OnReadDescriptors(base::OnceClosure callback,
                       scoped_refptr<UsbDeviceHandle> device_handle,
                       const GURL& landing_page) {
  UsbDeviceLinux* device =
      static_cast<UsbDeviceLinux*>(device_handle->GetDevice().get());

  if (landing_page.is_valid())
    device->set_webusb_landing_page(landing_page);

  device_handle->Close();
  std::move(callback).Run();
}

void OnDeviceOpenedToReadDescriptors(
    base::OnceClosure callback,
    scoped_refptr<UsbDeviceHandle> device_handle) {
  if (device_handle) {
    ReadWebUsbDescriptors(
        device_handle,
        base::BindOnce(&OnReadDescriptors, std::move(callback), device_handle));
  } else {
    std::move(callback).Run();
  }
}

}  // namespace

class UsbServiceLinux::BlockingTaskRunnerHelper : public UdevWatcher::Observer {
 public:
  explicit BlockingTaskRunnerHelper(
      base::WeakPtr<UsbServiceLinux> service,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  BlockingTaskRunnerHelper(const BlockingTaskRunnerHelper&) = delete;
  BlockingTaskRunnerHelper& operator=(const BlockingTaskRunnerHelper&) = delete;

  ~BlockingTaskRunnerHelper() override;

 private:
  // UdevWatcher::Observer
  void OnDeviceAdded(ScopedUdevDevicePtr device) override;
  void OnDeviceRemoved(ScopedUdevDevicePtr device) override;
  void OnDeviceChanged(ScopedUdevDevicePtr device) override;

  std::unique_ptr<UdevWatcher> watcher_;

  // |service_| can only be checked for validity on |task_runner_|'s sequence.
  base::WeakPtr<UsbServiceLinux> service_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

UsbServiceLinux::BlockingTaskRunnerHelper::BlockingTaskRunnerHelper(
    base::WeakPtr<UsbServiceLinux> service,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : service_(std::move(service)), task_runner_(std::move(task_runner)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // Initializing udev for device enumeration and monitoring may fail. In that
  // case this service will continue to exist but no devices will be found.
  const std::vector<UdevWatcher::Filter> filters = {{kUsbSubsystem, ""}};
  watcher_ = UdevWatcher::StartWatching(this, filters);
  if (watcher_)
    watcher_->EnumerateExistingDevices();

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UsbServiceLinux::HelperStarted, service_));
}

UsbServiceLinux::BlockingTaskRunnerHelper::~BlockingTaskRunnerHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void UsbServiceLinux::BlockingTaskRunnerHelper::OnDeviceAdded(
    ScopedUdevDevicePtr device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  const char* subsystem = udev_device_get_subsystem(device.get());
  CHECK(subsystem);
  CHECK_EQ(subsystem, kUsbSubsystem);

  const char* value = udev_device_get_devnode(device.get());
  if (!value)
    return;
  std::string device_path = value;

  const char* sysfs_path = udev_device_get_syspath(device.get());
  if (!sysfs_path)
    return;

  base::FilePath descriptors_path =
      base::FilePath(sysfs_path).Append("descriptors");
  std::string descriptors_str;
  if (!base::ReadFileToString(descriptors_path, &descriptors_str))
    return;

  std::unique_ptr<UsbDeviceDescriptor> descriptor(new UsbDeviceDescriptor());
  if (!descriptor->Parse(base::make_span(
          reinterpret_cast<const uint8_t*>(descriptors_str.data()),
          descriptors_str.size()))) {
    return;
  }

  if (descriptor->device_info->class_code == kDeviceClassHub) {
    // Don't try to enumerate hubs. We never want to connect to a hub.
    return;
  }

  value = udev_device_get_sysattr_value(device.get(), "manufacturer");
  if (value)
    descriptor->device_info->manufacturer_name = base::UTF8ToUTF16(value);

  value = udev_device_get_sysattr_value(device.get(), "product");
  if (value)
    descriptor->device_info->product_name = base::UTF8ToUTF16(value);

  value = udev_device_get_sysattr_value(device.get(), "serial");
  if (value)
    descriptor->device_info->serial_number = base::UTF8ToUTF16(value);

  unsigned active_configuration = 0;
  value = udev_device_get_sysattr_value(device.get(), "bConfigurationValue");
  if (value)
    base::StringToUint(value, &active_configuration);
  descriptor->device_info->active_configuration = active_configuration;

  unsigned bus_number = 0;
  value = udev_device_get_sysattr_value(device.get(), "busnum");
  if (value)
    base::StringToUint(value, &bus_number);
  descriptor->device_info->bus_number = bus_number;

  unsigned port_number = 0;
  value = udev_device_get_sysattr_value(device.get(), "devnum");
  if (value)
    base::StringToUint(value, &port_number);
  descriptor->device_info->port_number = port_number;

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UsbServiceLinux::OnDeviceAdded, service_,
                                device_path, std::move(descriptor)));
}

void UsbServiceLinux::BlockingTaskRunnerHelper::OnDeviceRemoved(
    ScopedUdevDevicePtr device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  const char* device_path = udev_device_get_devnode(device.get());
  if (device_path) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&UsbServiceLinux::OnDeviceRemoved, service_,
                                  std::string(device_path)));
  }
}

void UsbServiceLinux::BlockingTaskRunnerHelper::OnDeviceChanged(
    ScopedUdevDevicePtr) {}

UsbServiceLinux::UsbServiceLinux() {
  helper_ = base::SequenceBound<BlockingTaskRunnerHelper>(
      CreateBlockingTaskRunner(), weak_factory_.GetWeakPtr(),
      base::SequencedTaskRunner::GetCurrentDefault());
}

UsbServiceLinux::~UsbServiceLinux() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NotifyWillDestroyUsbService();
}

void UsbServiceLinux::GetDevices(GetDevicesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (enumeration_ready())
    UsbService::GetDevices(std::move(callback));
  else
    enumeration_callbacks_.push_back(std::move(callback));
}

void UsbServiceLinux::OnDeviceAdded(
    const std::string& device_path,
    std::unique_ptr<UsbDeviceDescriptor> descriptor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (base::Contains(devices_by_path_, device_path)) {
    USB_LOG(ERROR) << "Got duplicate add event for path: " << device_path;
    return;
  }

  // Devices that appear during initial enumeration are gathered into the first
  // result returned by GetDevices() and prevent device add/remove notifications
  // from being sent.
  if (!enumeration_ready())
    ++first_enumeration_countdown_;

  scoped_refptr<UsbDeviceLinux> device(
      new UsbDeviceLinux(device_path, std::move(descriptor)));
  devices_by_path_[device->device_path()] = device;

  if (ShouldReadDescriptors(*device)) {
    device->Open(
        base::BindOnce(&OnDeviceOpenedToReadDescriptors,
                       base::BindOnce(&UsbServiceLinux::DeviceReady,
                                      weak_factory_.GetWeakPtr(), device)));
  } else {
    DeviceReady(device);
  }
}

void UsbServiceLinux::DeviceReady(scoped_refptr<UsbDeviceLinux> device) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool enumeration_became_ready = false;
  if (!enumeration_ready()) {
    DCHECK_GT(first_enumeration_countdown_, 0u);
    first_enumeration_countdown_--;
    if (enumeration_ready())
      enumeration_became_ready = true;
  }

  // If |device| was disconnected while descriptors were being read then it
  // will have been removed from |devices_by_path_|.
  bool device_added = base::Contains(devices_by_path_, device->device_path());
  if (device_added) {
    DCHECK(!base::Contains(devices(), device->guid()));
    devices()[device->guid()] = device;

    USB_LOG(USER) << "USB device added: path=" << device->device_path()
                  << " vendor=" << device->vendor_id() << " \""
                  << device->manufacturer_string()
                  << "\", product=" << device->product_id() << " \""
                  << device->product_string() << "\", serial=\""
                  << device->serial_number() << "\", guid=" << device->guid();
  }

  if (enumeration_became_ready) {
    std::vector<scoped_refptr<UsbDevice>> result;
    result.reserve(devices().size());
    for (const auto& map_entry : devices())
      result.push_back(map_entry.second);
    for (auto& callback : enumeration_callbacks_)
      std::move(callback).Run(result);
    enumeration_callbacks_.clear();
  } else if (device_added && enumeration_ready()) {
    NotifyDeviceAdded(device);
  }
}

void UsbServiceLinux::OnDeviceRemoved(const std::string& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto by_path_it = devices_by_path_.find(path);
  if (by_path_it == devices_by_path_.end())
    return;

  scoped_refptr<UsbDeviceLinux> device = by_path_it->second;
  devices_by_path_.erase(by_path_it);
  device->OnDisconnect();

  auto by_guid_it = devices().find(device->guid());
  if (by_guid_it != devices().end() && enumeration_ready()) {
    USB_LOG(USER) << "USB device removed: path=" << device->device_path()
                  << " guid=" << device->guid();

    devices().erase(by_guid_it);
    NotifyDeviceRemoved(device);
  }
}

void UsbServiceLinux::HelperStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  helper_started_ = true;
  if (enumeration_ready()) {
    std::vector<scoped_refptr<UsbDevice>> result;
    result.reserve(devices().size());
    for (const auto& map_entry : devices())
      result.push_back(map_entry.second);
    for (auto& callback : enumeration_callbacks_)
      std::move(callback).Run(result);
    enumeration_callbacks_.clear();
  }
}

}  // namespace device
