// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_service_linux.h"

#include <fcntl.h>
#include <linux/input.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/device_event_log/device_event_log.h"
#include "device/udev_linux/scoped_udev.h"
#include "device/udev_linux/udev_watcher.h"
#include "services/device/hid/hid_connection_linux.h"

// TODO(huangs): Enable for IS_CHROMEOS_LACROS. This will simplify crosapi so
// that it won't need to pass HidManager around (crbug.com/1109621).
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/system/sys_info.h"
#include "chromeos/dbus/permission_broker/permission_broker_client.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace device {

namespace {

const char kDevtypeUsbDevice[] = "usb_device";
const char kSubsystemBluetooth[] = "bluetooth";
const char kSubsystemHid[] = "hid";
const char kSubsystemHidraw[] = "hidraw";
const char kSubsystemUsb[] = "usb";
const char kHIDID[] = "HID_ID";
const char kHIDName[] = "HID_NAME";
const char kHIDUnique[] = "HID_UNIQ";
const char kSysfsReportDescriptorKey[] = "report_descriptor";
const char kKernelHciPrefix[] = "hci";

// Walks up the sysfs device tree starting at |device| and returns the first
// ancestor in the "hid" subsystem. Returns nullptr on failure.
udev_device* FindFirstHidAncestor(udev_device* device) {
  udev_device* ancestor = device;
  do {
    const char* subsystem = udev_device_get_subsystem(ancestor);
    if (!subsystem)
      return nullptr;
    if (strcmp(subsystem, kSubsystemHid) == 0)
      return ancestor;
  } while ((ancestor = udev_device_get_parent(ancestor)));
  return nullptr;
}

// Walks up the sysfs device tree starting at |device| and returns the first
// ancestor not in the "hid" or "hidraw" subsystems. Returns nullptr on failure.
udev_device* FindFirstNonHidAncestor(udev_device* device) {
  udev_device* ancestor = device;
  do {
    const char* subsystem = udev_device_get_subsystem(ancestor);
    if (!subsystem)
      return nullptr;
    if (strcmp(subsystem, kSubsystemHid) != 0 &&
        strcmp(subsystem, kSubsystemHidraw) != 0) {
      return ancestor;
    }
  } while ((ancestor = udev_device_get_parent(ancestor)));
  return nullptr;
}

// Returns the sysfs path for a USB device |usb_device|, or nullptr if the sysfs
// path could not be retrieved. |usb_device| must be a device in the "usb"
// subsystem.
//
// Some USB devices expose multiple interfaces. If |usb_device| refers to a
// single USB interface, walk up the device tree to find the ancestor that
// represents the physical device.
const char* GetUsbDeviceSyspath(udev_device* usb_device) {
  do {
    const char* subsystem = udev_device_get_subsystem(usb_device);
    if (!subsystem || strcmp(subsystem, kSubsystemUsb) != 0)
      return nullptr;

    const char* devtype = udev_device_get_devtype(usb_device);
    if (!devtype)
      return nullptr;

    // Use the syspath of the first ancestor with devtype "usb_device".
    if (strcmp(devtype, kDevtypeUsbDevice) == 0)
      return udev_device_get_syspath(usb_device);
  } while ((usb_device = udev_device_get_parent(usb_device)));
  return nullptr;
}

// Returns the sysfs path for a Bluetooth Classic device |bt_device|, or nullptr
// if the sysfs path could not be retrieved. |bt_device| must be a device in the
// "bluetooth" subsystem.
const char* GetBluetoothDeviceSyspath(udev_device* bt_device) {
  do {
    const char* subsystem = udev_device_get_subsystem(bt_device);
    if (!subsystem || strcmp(subsystem, kSubsystemBluetooth) != 0)
      return nullptr;

    // Look for a sysname like "hci0:123".
    const char* sysfs_name = udev_device_get_sysname(bt_device);
    if (!sysfs_name)
      return nullptr;

    std::vector<std::string> parts = base::SplitString(
        sysfs_name, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (parts.size() == 2 && base::StartsWith(parts[0], kKernelHciPrefix,
                                              base::CompareCase::SENSITIVE)) {
      return udev_device_get_syspath(bt_device);
    }
  } while ((bt_device = udev_device_get_parent(bt_device)));
  return nullptr;
}

// Returns the physical device ID for a device |hidraw_device|. On Linux, the
// physical device ID is the sysfs path to the device node that represents the
// physical device if it is available. When the physical device node is not
// available, the sysfs path of the HID interface is returned instead. Returns
// nullptr on failure.
const char* GetPhysicalDeviceId(udev_device* hidraw_device) {
  const char* subsystem = udev_device_get_subsystem(hidraw_device);
  if (!subsystem || strcmp(subsystem, kSubsystemHidraw) != 0)
    return nullptr;

  udev_device* hid_ancestor = FindFirstHidAncestor(hidraw_device);
  if (!hid_ancestor)
    return nullptr;
  const char* hid_sysfs_path = udev_device_get_syspath(hid_ancestor);

  udev_device* ancestor = FindFirstNonHidAncestor(hid_ancestor);
  if (!ancestor)
    return hid_sysfs_path;

  const char* ancestor_subsystem = udev_device_get_subsystem(ancestor);
  if (!ancestor_subsystem)
    return hid_sysfs_path;

  if (strcmp(ancestor_subsystem, kSubsystemUsb) == 0) {
    const char* usb_sysfs_path = GetUsbDeviceSyspath(ancestor);
    if (usb_sysfs_path)
      return usb_sysfs_path;
  }

  if (strcmp(ancestor_subsystem, kSubsystemBluetooth) == 0) {
    const char* bt_sysfs_path = GetBluetoothDeviceSyspath(ancestor);
    if (bt_sysfs_path)
      return bt_sysfs_path;
  }

  return hid_sysfs_path;
}

// Convert from a Linux |bus_id| (defined in linux/input.h) to a
// mojom::HidBusType.
mojom::HidBusType BusTypeFromLinuxBusId(uint16_t bus_id) {
  switch (bus_id) {
    case BUS_USB:
      return mojom::HidBusType::kHIDBusTypeUSB;
    case BUS_BLUETOOTH:
      return mojom::HidBusType::kHIDBusTypeBluetooth;
    default:
      break;
  }
  return mojom::HidBusType::kHIDBusTypeUnknown;
}

}  // namespace

struct HidServiceLinux::ConnectParams {
  ConnectParams(scoped_refptr<HidDeviceInfo> device_info,
                bool allow_protected_reports,
                bool allow_fido_reports,
                ConnectCallback callback)
      : device_info(std::move(device_info)),
        allow_protected_reports(allow_protected_reports),
        allow_fido_reports(allow_fido_reports),
        callback(std::move(callback)),
        task_runner(base::SequencedTaskRunner::GetCurrentDefault()),
        blocking_task_runner(
            base::ThreadPool::CreateSequencedTaskRunner(kBlockingTaskTraits)) {}
  ~ConnectParams() {}

  scoped_refptr<HidDeviceInfo> device_info;
  bool allow_protected_reports;
  bool allow_fido_reports;
  ConnectCallback callback;
  scoped_refptr<base::SequencedTaskRunner> task_runner;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner;
  base::ScopedFD fd;
};

class HidServiceLinux::BlockingTaskRunnerHelper : public UdevWatcher::Observer {
 public:
  BlockingTaskRunnerHelper(base::WeakPtr<HidServiceLinux> service,
                           scoped_refptr<base::SequencedTaskRunner> task_runner)
      : service_(std::move(service)), task_runner_(std::move(task_runner)) {
    watcher_ = UdevWatcher::StartWatching(this);
    watcher_->EnumerateExistingDevices();
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&HidServiceLinux::FirstEnumerationComplete, service_));
  }

  BlockingTaskRunnerHelper(const BlockingTaskRunnerHelper&) = delete;
  BlockingTaskRunnerHelper& operator=(const BlockingTaskRunnerHelper&) = delete;

  ~BlockingTaskRunnerHelper() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

 private:
  // UdevWatcher::Observer
  void OnDeviceAdded(ScopedUdevDevicePtr device) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    const char* device_path = udev_device_get_syspath(device.get());
    if (!device_path)
      return;
    HidPlatformDeviceId platform_device_id = device_path;

    const char* subsystem = udev_device_get_subsystem(device.get());
    if (!subsystem || strcmp(subsystem, kSubsystemHidraw) != 0)
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
    if (!base::HexStringToUInt(parts[0], &int_property) ||
        int_property > std::numeric_limits<uint16_t>::max()) {
      return;
    }
    auto bus_type = BusTypeFromLinuxBusId(int_property);

    if (!base::HexStringToUInt(parts[1], &int_property) ||
        int_property > std::numeric_limits<uint16_t>::max()) {
      return;
    }
    uint16_t vendor_id = int_property;

    if (!base::HexStringToUInt(parts[2], &int_property) ||
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

    const char* physical_device_id = GetPhysicalDeviceId(device.get());
    if (!physical_device_id) {
      HID_LOG(EVENT) << "GetPhysicalDeviceId failed for '" << device_path
                     << "'";
      return;
    }

    auto device_info = base::MakeRefCounted<HidDeviceInfo>(
        platform_device_id, physical_device_id, vendor_id, product_id,
        product_name, serial_number, bus_type,
        std::vector<uint8_t>(report_descriptor_str.begin(),
                             report_descriptor_str.end()),
        device_node);

    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&HidServiceLinux::AddDevice, service_, device_info));
  }

  void OnDeviceRemoved(ScopedUdevDevicePtr device) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    const char* device_path = udev_device_get_syspath(device.get());
    if (device_path) {
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&HidServiceLinux::RemoveDevice, service_,
                                    std::string(device_path)));
    }
  }

  void OnDeviceChanged(ScopedUdevDevicePtr) override {}

  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<UdevWatcher> watcher_;

  // This weak pointer is only valid when checked on this task runner.
  base::WeakPtr<HidServiceLinux> service_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

HidServiceLinux::HidServiceLinux() {
  helper_ = base::SequenceBound<BlockingTaskRunnerHelper>(
      base::ThreadPool::CreateSequencedTaskRunner(kBlockingTaskTraits),
      weak_factory_.GetWeakPtr(),
      base::SequencedTaskRunner::GetCurrentDefault());
}

HidServiceLinux::~HidServiceLinux() = default;

base::WeakPtr<HidService> HidServiceLinux::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void HidServiceLinux::Connect(const std::string& device_guid,
                              bool allow_protected_reports,
                              bool allow_fido_reports,
                              ConnectCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& map_entry = devices().find(device_guid);
  if (map_entry == devices().end()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), nullptr));
    return;
  }
  scoped_refptr<HidDeviceInfo> device_info = map_entry->second;

// TODO(huangs): Enable for IS_CHROMEOS_LACROS for crbug.com/1223456.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  chromeos::PermissionBrokerClient::Get()->OpenPath(
      device_info->device_node(),
      base::BindOnce(&HidServiceLinux::OnPathOpenComplete,
                     std::make_unique<ConnectParams>(
                         device_info, allow_protected_reports,
                         allow_fido_reports, std::move(split_callback.first))),
      base::BindOnce(&HidServiceLinux::OnPathOpenError,
                     device_info->device_node(),
                     std::move(split_callback.second)));
#else
  auto params =
      std::make_unique<ConnectParams>(device_info, allow_protected_reports,
                                      allow_fido_reports, std::move(callback));
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
      params->blocking_task_runner;
  blocking_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&HidServiceLinux::OpenOnBlockingThread,
                                std::move(params)));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

// static
void HidServiceLinux::OnPathOpenComplete(std::unique_ptr<ConnectParams> params,
                                         base::ScopedFD fd) {
  params->fd = std::move(fd);
  FinishOpen(std::move(params));
}

// static
void HidServiceLinux::OnPathOpenError(const std::string& device_path,
                                      ConnectCallback callback,
                                      const std::string& error_name,
                                      const std::string& error_message) {
  HID_LOG(EVENT) << "Permission broker failed to open '" << device_path
                 << "': " << error_name << ": " << error_message;
  std::move(callback).Run(nullptr);
}

#else

// static
void HidServiceLinux::OpenOnBlockingThread(
    std::unique_ptr<ConnectParams> params) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
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
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(std::move(params->callback), nullptr));
    return;
  }
  params->fd.reset(device_file.TakePlatformFile());

  task_runner->PostTask(FROM_HERE, base::BindOnce(&HidServiceLinux::FinishOpen,
                                                  std::move(params)));
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// static
void HidServiceLinux::FinishOpen(std::unique_ptr<ConnectParams> params) {
  DCHECK(params->fd.is_valid());

  if (!base::SetNonBlocking(params->fd.get())) {
    HID_PLOG(DEBUG) << "Failed to set the non-blocking flag on the device fd";
    std::move(params->callback).Run(nullptr);
    return;
  }

  std::move(params->callback)
      .Run(base::MakeRefCounted<HidConnectionLinux>(
          std::move(params->device_info), std::move(params->fd),
          std::move(params->blocking_task_runner),
          params->allow_protected_reports, params->allow_fido_reports));
}

}  // namespace device
