// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_service_linux.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "device/udev_linux/udev_watcher.h"
#include "services/device/usb/usb_device_handle.h"
#include "services/device/usb/usb_device_linux.h"
#include "services/device/usb/webusb_descriptors.h"

namespace device {

namespace {

// Standard USB requests and descriptor types:
const uint16_t kUsbVersion2_1 = 0x0210;

const uint8_t kDeviceClassHub = 0x09;

void OnReadDescriptors(base::OnceCallback<void(bool)> callback,
                       scoped_refptr<UsbDeviceHandle> device_handle,
                       const GURL& landing_page) {
  UsbDeviceLinux* device =
      static_cast<UsbDeviceLinux*>(device_handle->GetDevice().get());

  if (landing_page.is_valid())
    device->set_webusb_landing_page(landing_page);

  device_handle->Close();
  std::move(callback).Run(true /* success */);
}

void OnDeviceOpenedToReadDescriptors(
    base::OnceCallback<void(bool)> callback,
    scoped_refptr<UsbDeviceHandle> device_handle) {
  if (device_handle) {
    ReadWebUsbDescriptors(
        device_handle,
        base::BindOnce(&OnReadDescriptors, std::move(callback), device_handle));
  } else {
    std::move(callback).Run(false /* failure */);
  }
}

}  // namespace

class UsbServiceLinux::BlockingTaskRunnerHelper : public UdevWatcher::Observer {
 public:
  explicit BlockingTaskRunnerHelper(base::WeakPtr<UsbServiceLinux> service);
  ~BlockingTaskRunnerHelper() override;

  void Start();

 private:
  // UdevWatcher::Observer
  void OnDeviceAdded(ScopedUdevDevicePtr device) override;
  void OnDeviceRemoved(ScopedUdevDevicePtr device) override;
  void OnDeviceChanged(ScopedUdevDevicePtr device) override;

  std::unique_ptr<UdevWatcher> watcher_;

  // |service_| can only be checked for validity on |task_runner_|'s sequence.
  base::WeakPtr<UsbServiceLinux> service_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(BlockingTaskRunnerHelper);
};

UsbServiceLinux::BlockingTaskRunnerHelper::BlockingTaskRunnerHelper(
    base::WeakPtr<UsbServiceLinux> service)
    : service_(service), task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  // Detaches from the sequence on which this object was created. It will be
  // bound to its owning sequence when Start() is called.
  sequence_checker_.DetachFromSequence();
}

UsbServiceLinux::BlockingTaskRunnerHelper::~BlockingTaskRunnerHelper() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
}

void UsbServiceLinux::BlockingTaskRunnerHelper::Start() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // Initializing udev for device enumeration and monitoring may fail. In that
  // case this service will continue to exist but no devices will be found.
  watcher_ = UdevWatcher::StartWatching(this);
  if (watcher_)
    watcher_->EnumerateExistingDevices();

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UsbServiceLinux::HelperStarted, service_));
}

void UsbServiceLinux::BlockingTaskRunnerHelper::OnDeviceAdded(
    ScopedUdevDevicePtr device) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  const char* subsystem = udev_device_get_subsystem(device.get());
  if (!subsystem || strcmp(subsystem, "usb") != 0)
    return;

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
  if (!descriptor->Parse(std::vector<uint8_t>(descriptors_str.begin(),
                                              descriptors_str.end()))) {
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
  DCHECK(sequence_checker_.CalledOnValidSequence());
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

UsbServiceLinux::UsbServiceLinux()
    : UsbService(),
      blocking_task_runner_(CreateBlockingTaskRunner()),
      helper_(nullptr, base::OnTaskRunnerDeleter(blocking_task_runner_)) {
  helper_.reset(new BlockingTaskRunnerHelper(weak_factory_.GetWeakPtr()));
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BlockingTaskRunnerHelper::Start,
                                base::Unretained(helper_.get())));
}

UsbServiceLinux::~UsbServiceLinux() {
  NotifyWillDestroyUsbService();
}

void UsbServiceLinux::GetDevices(const GetDevicesCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (enumeration_ready())
    UsbService::GetDevices(callback);
  else
    enumeration_callbacks_.push_back(callback);
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
  if (device->usb_version() >= kUsbVersion2_1) {
    device->Open(
        base::BindOnce(&OnDeviceOpenedToReadDescriptors,
                       base::BindOnce(&UsbServiceLinux::DeviceReady,
                                      weak_factory_.GetWeakPtr(), device)));
  } else {
    DeviceReady(device, true /* success */);
  }
}

void UsbServiceLinux::DeviceReady(scoped_refptr<UsbDeviceLinux> device,
                                  bool success) {
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
  auto it = devices_by_path_.find(device->device_path());
  if (it == devices_by_path_.end()) {
    success = false;
  } else if (success) {
    DCHECK(!base::Contains(devices(), device->guid()));
    devices()[device->guid()] = device;

    USB_LOG(USER) << "USB device added: path=" << device->device_path()
                  << " vendor=" << device->vendor_id() << " \""
                  << device->manufacturer_string()
                  << "\", product=" << device->product_id() << " \""
                  << device->product_string() << "\", serial=\""
                  << device->serial_number() << "\", guid=" << device->guid();
  } else {
    devices_by_path_.erase(it);
  }

  if (enumeration_became_ready) {
    std::vector<scoped_refptr<UsbDevice>> result;
    result.reserve(devices().size());
    for (const auto& map_entry : devices())
      result.push_back(map_entry.second);
    for (const auto& callback : enumeration_callbacks_)
      callback.Run(result);
    enumeration_callbacks_.clear();
  } else if (success && enumeration_ready()) {
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
    for (const auto& callback : enumeration_callbacks_)
      callback.Run(result);
    enumeration_callbacks_.clear();
  }
}

}  // namespace device
