// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_service_linux.h"

#include <fcntl.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "device/udev_linux/scoped_udev.h"
#include "device/udev_linux/udev_watcher.h"
#include "services/device/hid/hid_connection_linux.h"

#if defined(OS_CHROMEOS)
#include "base/sys_info.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/permission_broker_client.h"
#endif  // defined(OS_CHROMEOS)

namespace device {

namespace {

const char kHidrawSubsystem[] = "hidraw";
const char kHIDID[] = "HID_ID";
const char kHIDName[] = "HID_NAME";
const char kHIDUnique[] = "HID_UNIQ";
const char kSysfsReportDescriptorKey[] = "report_descriptor";

}  // namespace

struct HidServiceLinux::ConnectParams {
  ConnectParams(scoped_refptr<HidDeviceInfo> device_info,
                const ConnectCallback& callback)
      : device_info(std::move(device_info)),
        callback(callback),
        task_runner(base::ThreadTaskRunnerHandle::Get()),
        blocking_task_runner(
            base::CreateSequencedTaskRunnerWithTraits(kBlockingTaskTraits)) {}
  ~ConnectParams() {}

  scoped_refptr<HidDeviceInfo> device_info;
  ConnectCallback callback;
  scoped_refptr<base::SequencedTaskRunner> task_runner;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner;
  base::ScopedFD fd;
};

class HidServiceLinux::BlockingTaskHelper : public UdevWatcher::Observer {
 public:
  BlockingTaskHelper(base::WeakPtr<HidServiceLinux> service)
      : service_(std::move(service)),
        task_runner_(base::ThreadTaskRunnerHandle::Get()) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  ~BlockingTaskHelper() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void Start() {
    base::AssertBlockingAllowedDeprecated();
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    watcher_ = UdevWatcher::StartWatching(this);
    watcher_->EnumerateExistingDevices();
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&HidServiceLinux::FirstEnumerationComplete, service_));
  }

 private:
  // UdevWatcher::Observer
  void OnDeviceAdded(ScopedUdevDevicePtr device) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    const char* device_path = udev_device_get_syspath(device.get());
    if (!device_path)
      return;
    HidPlatformDeviceId platform_device_id = device_path;

    const char* subsystem = udev_device_get_subsystem(device.get());
    if (!subsystem || strcmp(subsystem, kHidrawSubsystem) != 0)
      return;

    const char* str_property = udev_device_get_devnode(device.get());
    if (!str_property)
      return;
    std::string device_node = str_property;

    udev_device* parent = udev_device_get_parent(device.get());
    if (!parent)
      return;

    const char* hid_id = udev_device_get_property_value(parent, kHIDID);
    if (!hid_id)
      return;

    std::vector<std::string> parts = base::SplitString(
        hid_id, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (parts.size() != 3)
      return;

    uint32_t int_property = 0;
    if (!HexStringToUInt(base::StringPiece(parts[1]), &int_property) ||
        int_property > std::numeric_limits<uint16_t>::max()) {
      return;
    }
    uint16_t vendor_id = int_property;

    if (!HexStringToUInt(base::StringPiece(parts[2]), &int_property) ||
        int_property > std::numeric_limits<uint16_t>::max()) {
      return;
    }
    uint16_t product_id = int_property;

    std::string serial_number;
    str_property = udev_device_get_property_value(parent, kHIDUnique);
    if (str_property)
      serial_number = str_property;

    std::string product_name;
    str_property = udev_device_get_property_value(parent, kHIDName);
    if (str_property)
      product_name = str_property;

    const char* parent_sysfs_path = udev_device_get_syspath(parent);
    if (!parent_sysfs_path)
      return;
    base::FilePath report_descriptor_path =
        base::FilePath(parent_sysfs_path).Append(kSysfsReportDescriptorKey);
    std::string report_descriptor_str;
    if (!base::ReadFileToString(report_descriptor_path, &report_descriptor_str))
      return;

    scoped_refptr<HidDeviceInfo> device_info(new HidDeviceInfo(
        platform_device_id, vendor_id, product_id, product_name, serial_number,
        // TODO(reillyg): Detect Bluetooth. crbug.com/443335
        mojom::HidBusType::kHIDBusTypeUSB,
        std::vector<uint8_t>(report_descriptor_str.begin(),
                             report_descriptor_str.end()),
        device_node));

    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&HidServiceLinux::AddDevice, service_, device_info));
  }

  void OnDeviceRemoved(ScopedUdevDevicePtr device) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    const char* device_path = udev_device_get_syspath(device.get());
    if (device_path) {
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&HidServiceLinux::RemoveDevice, service_,
                                    std::string(device_path)));
    }
  }

  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<UdevWatcher> watcher_;

  // This weak pointer is only valid when checked on this task runner.
  base::WeakPtr<HidServiceLinux> service_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(BlockingTaskHelper);
};

HidServiceLinux::HidServiceLinux()
    : blocking_task_runner_(
          base::CreateSequencedTaskRunnerWithTraits(kBlockingTaskTraits)),
      weak_factory_(this) {
  helper_ = std::make_unique<BlockingTaskHelper>(weak_factory_.GetWeakPtr());
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BlockingTaskHelper::Start,
                                base::Unretained(helper_.get())));
}

HidServiceLinux::~HidServiceLinux() {
  blocking_task_runner_->DeleteSoon(FROM_HERE, helper_.release());
}

base::WeakPtr<HidService> HidServiceLinux::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void HidServiceLinux::Connect(const std::string& device_guid,
                              const ConnectCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto& map_entry = devices().find(device_guid);
  if (map_entry == devices().end()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, nullptr));
    return;
  }
  scoped_refptr<HidDeviceInfo> device_info = map_entry->second;

  auto params = std::make_unique<ConnectParams>(device_info, callback);

#if defined(OS_CHROMEOS)
  chromeos::PermissionBrokerClient* client =
      chromeos::DBusThreadManager::Get()->GetPermissionBrokerClient();
  DCHECK(client) << "Could not get permission broker client.";
  chromeos::PermissionBrokerClient::ErrorCallback error_callback =
      base::Bind(&HidServiceLinux::OnPathOpenError,
                 params->device_info->device_node(), params->callback);
  client->OpenPath(
      device_info->device_node(),
      base::Bind(&HidServiceLinux::OnPathOpenComplete, base::Passed(&params)),
      error_callback);
#else
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
      params->blocking_task_runner;
  blocking_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&HidServiceLinux::OpenOnBlockingThread,
                                std::move(params)));
#endif  // defined(OS_CHROMEOS)
}

#if defined(OS_CHROMEOS)

// static
void HidServiceLinux::OnPathOpenComplete(std::unique_ptr<ConnectParams> params,
                                         base::ScopedFD fd) {
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
      params->blocking_task_runner;
  params->fd = std::move(fd);
  blocking_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&HidServiceLinux::FinishOpen, std::move(params)));
}

// static
void HidServiceLinux::OnPathOpenError(const std::string& device_path,
                                      const ConnectCallback& callback,
                                      const std::string& error_name,
                                      const std::string& error_message) {
  HID_LOG(EVENT) << "Permission broker failed to open '" << device_path
                 << "': " << error_name << ": " << error_message;
  callback.Run(nullptr);
}

#else

// static
void HidServiceLinux::OpenOnBlockingThread(
    std::unique_ptr<ConnectParams> params) {
  base::AssertBlockingAllowedDeprecated();
  scoped_refptr<base::SequencedTaskRunner> task_runner = params->task_runner;

  base::FilePath device_path(params->device_info->device_node());
  base::File device_file;
  int flags =
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE;
  device_file.Initialize(device_path, flags);
  if (!device_file.IsValid()) {
    base::File::Error file_error = device_file.error_details();

    if (file_error == base::File::FILE_ERROR_ACCESS_DENIED) {
      HID_LOG(EVENT)
          << "Access denied opening device read-write, trying read-only.";
      flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
      device_file.Initialize(device_path, flags);
    }
  }
  if (!device_file.IsValid()) {
    HID_LOG(EVENT) << "Failed to open '" << params->device_info->device_node()
                   << "': "
                   << base::File::ErrorToString(device_file.error_details());
    task_runner->PostTask(FROM_HERE, base::BindOnce(params->callback, nullptr));
    return;
  }
  params->fd.reset(device_file.TakePlatformFile());
  FinishOpen(std::move(params));
}

#endif  // defined(OS_CHROMEOS)

// static
void HidServiceLinux::FinishOpen(std::unique_ptr<ConnectParams> params) {
  base::AssertBlockingAllowedDeprecated();
  scoped_refptr<base::SequencedTaskRunner> task_runner = params->task_runner;

  if (!base::SetNonBlocking(params->fd.get())) {
    HID_PLOG(ERROR) << "Failed to set the non-blocking flag on the device fd";
    task_runner->PostTask(FROM_HERE, base::BindOnce(params->callback, nullptr));
    return;
  }

  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&HidServiceLinux::CreateConnection, std::move(params)));
}

// static
void HidServiceLinux::CreateConnection(std::unique_ptr<ConnectParams> params) {
  DCHECK(params->fd.is_valid());
  params->callback.Run(base::MakeRefCounted<HidConnectionLinux>(
      std::move(params->device_info), std::move(params->fd),
      std::move(params->blocking_task_runner)));
}

}  // namespace device
