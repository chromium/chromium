// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_device_watcher_mac.h"

#include <optional>
#include <utility>

#include "base/apple/foundation_util.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"

namespace device {

namespace {

constexpr char kBluetoothPlistFilePath[] =
    "/Library/Preferences/com.apple.Bluetooth.plist";

const base::FilePath& BluetoothPlistFilePath() {
  static const base::FilePath file_path(kBluetoothPlistFilePath);
  return file_path;
}

}  // namespace

// static
std::unique_ptr<BluetoothLowEnergyDeviceWatcherMac>
BluetoothLowEnergyDeviceWatcherMac::CreateAndStartWatching(
    scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner,
    LowEnergyDeviceListUpdatedCallback
        low_energy_device_list_updated_callback) {
  auto watcher = std::make_unique<BluetoothLowEnergyDeviceWatcherMac>(
      std::move(ui_thread_task_runner),
      std::move(low_energy_device_list_updated_callback));
  watcher->Init();
  return watcher;
}

BluetoothLowEnergyDeviceWatcherMac::BluetoothLowEnergyDeviceWatcherMac(
    scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner,
    LowEnergyDeviceListUpdatedCallback low_energy_device_list_updated_callback)
    : ui_thread_task_runner_(std::move(ui_thread_task_runner)),
      low_energy_device_list_updated_callback_(
          std::move(low_energy_device_list_updated_callback)) {}

BluetoothLowEnergyDeviceWatcherMac::~BluetoothLowEnergyDeviceWatcherMac() {
  if (destroy_callback_for_testing_) {
    std::move(destroy_callback_for_testing_).Run();
  }
}

// static
std::optional<std::map<std::string, std::string>>
BluetoothLowEnergyDeviceWatcherMac::OnPropertyListFileChangedOnFileThread(
    const base::FilePath& path,
    bool error) {
  if (error) {
    LOG(WARNING) << "Failed to read com.apple.Bluetooth.plist.";
    return std::nullopt;
  }

  // Bluetooth property list file is expected to have the following format:
  //
  //   "CoreBluetoothCache" => {
  //    "E7F8589A-A7D9-4B94-9A08-D89076A159F4" => {
  //      "DeviceAddress" => "11-11-11-11-11-11"
  //      "DeviceAddressType" => 1
  //      "ServiceChangedHandle" => 3
  //      "ServiceChangedSubscribed" => 0
  //      "ServiceDiscoveryComplete" => 0
  //    }
  //    "D3CAC59E-C501-4599-97DA-2DF491544EEE" => {
  //      "DeviceAddress" => "22-22-22-22-22-22"
  //      "DeviceAddressType" => 1
  //      "ServiceChangedHandle" => 3
  //      "ServiceChangedSubscribed" => 0
  //      "ServiceDiscoveryComplete" => 0
  //    }
  //  }
  NSURL* plist_file = base::apple::FilePathToNSURL(path);
  NSDictionary* bluetooth_info_dictionary =
      [NSDictionary dictionaryWithContentsOfURL:plist_file error:nil];

  // |bluetooth_info_dictionary| is nil if there was an error reading the file
  // or if the content of the read file cannot be represented by a dictionary.
  if (!bluetooth_info_dictionary) {
    return std::nullopt;
  }

  return ParseBluetoothDevicePropertyListData(bluetooth_info_dictionary);
}

// static
void BluetoothLowEnergyDeviceWatcherMac::
    OnPropertyListFileChangedAndRunCallback(
        base::WeakPtr<BluetoothLowEnergyDeviceWatcherMac> weak_watcher,
        scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner,
        const base::FilePath& path,
        bool error) {
  auto parsed_data = OnPropertyListFileChangedOnFileThread(path, error);
  ui_thread_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&BluetoothLowEnergyDeviceWatcherMac::
                                    RunLowEnergyDeviceListUpdatedCallback,
                                weak_watcher, parsed_data));
}

void BluetoothLowEnergyDeviceWatcherMac::Init() {
  // Call |base::FilePathWatcher::Watch()| on |file_thread_task_runner_| to
  // watch for changes to the bluetooth property list file.
  property_list_watcher_
      .AsyncCall(base::IgnoreResult(&base::FilePathWatcher::Watch))
      .WithArgs(BluetoothPlistFilePath(),
                base::FilePathWatcher::Type::kNonRecursive,
                base::BindRepeating(&BluetoothLowEnergyDeviceWatcherMac::
                                        OnPropertyListFileChangedAndRunCallback,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    ui_thread_task_runner_));
}

void BluetoothLowEnergyDeviceWatcherMac::ReadBluetoothPropertyListFile() {
  // OnPropertyListFileChangedOnFileThread() should be called on
  // |file_thread_task_runner_|, and the result should be posted back to current
  // task runner to invoke |low_energy_device_list_updated_callback_|.
  file_thread_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&BluetoothLowEnergyDeviceWatcherMac::
                         OnPropertyListFileChangedOnFileThread,
                     BluetoothPlistFilePath(), false /* error */),
      base::BindOnce(&BluetoothLowEnergyDeviceWatcherMac::
                         RunLowEnergyDeviceListUpdatedCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

// static
std::map<std::string, std::string>
BluetoothLowEnergyDeviceWatcherMac::ParseBluetoothDevicePropertyListData(
    NSDictionary* data) {
  std::map<std::string, std::string> updated_low_energy_devices_info;
  NSDictionary* low_energy_devices_info = data[@"CoreBluetoothCache"];
  if (!low_energy_devices_info) {
    return updated_low_energy_devices_info;
  }

  for (NSString* identifier in low_energy_devices_info) {
    NSDictionary* device_info = low_energy_devices_info[identifier];
    if (!device_info) {
      continue;
    }

    NSString* raw_device_address = device_info[@"DeviceAddress"];
    if (!raw_device_address) {
      continue;
    }

    NSString* formatted_device_address =
        [raw_device_address stringByReplacingOccurrencesOfString:@"-"
                                                      withString:@":"];
    updated_low_energy_devices_info[base::SysNSStringToUTF8(identifier)] =
        base::SysNSStringToUTF8(formatted_device_address);
  }

  return updated_low_energy_devices_info;
}

void BluetoothLowEnergyDeviceWatcherMac::RunLowEnergyDeviceListUpdatedCallback(
    const std::optional<std::map<std::string, std::string>>&
        low_energy_devices_info) {
  CHECK(ui_thread_task_runner_->RunsTasksInCurrentSequence());
  // If |low_energy_devices_info| is null, it indicates that the property list
  // file has not been read successfully.
  // |low_energy_device_list_updated_callback_| will not be called.
  if (low_energy_devices_info.has_value() &&
      low_energy_device_list_updated_callback_) {
    low_energy_device_list_updated_callback_.Run(*low_energy_devices_info);
  }
}

}  // namespace device
