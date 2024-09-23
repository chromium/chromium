// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_device_watcher_mac.h"

#include <utility>

#include "base/apple/foundation_util.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "device/bluetooth/bluetooth_adapter_mac.h"

namespace device {

namespace {

constexpr char kBluetoothPlistFilePath[] =
    "/Library/Preferences/com.apple.Bluetooth.plist";

}  // namespace

// static
scoped_refptr<BluetoothLowEnergyDeviceWatcherMac>
BluetoothLowEnergyDeviceWatcherMac::CreateAndStartWatching(
    scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner,
    LowEnergyDeviceListUpdatedCallback
        low_energy_device_list_updated_callback) {
  auto watcher = base::MakeRefCounted<BluetoothLowEnergyDeviceWatcherMac>(
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
          std::move(low_energy_device_list_updated_callback)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

BluetoothLowEnergyDeviceWatcherMac::~BluetoothLowEnergyDeviceWatcherMac() {
  file_thread_task_runner_->DeleteSoon(FROM_HERE,
                                       property_list_watcher_.release());
}

void BluetoothLowEnergyDeviceWatcherMac::OnPropertyListFileChangedOnFileThread(
    const base::FilePath& path,
    bool error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (error) {
    LOG(WARNING) << "Failed to read com.apple.Bluetooth.plist.";
    return;
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
  if (!bluetooth_info_dictionary)
    return;

  auto parsed_data =
      ParseBluetoothDevicePropertyListData(bluetooth_info_dictionary);
  ui_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(low_energy_device_list_updated_callback_,
                                std::move(parsed_data)));
}

void BluetoothLowEnergyDeviceWatcherMac::Init() {
  file_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BluetoothLowEnergyDeviceWatcherMac::
                                    AddBluetoothPropertyListFileWatcher,
                                this));
}

void BluetoothLowEnergyDeviceWatcherMac::ReadBluetoothPropertyListFile() {
  file_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BluetoothLowEnergyDeviceWatcherMac::
                         OnPropertyListFileChangedOnFileThread,
                     this, BluetoothPlistFilePath(), false /* error */));
}

std::map<std::string, std::string>
BluetoothLowEnergyDeviceWatcherMac::ParseBluetoothDevicePropertyListData(
    NSDictionary* data) {
  std::map<std::string, std::string> updated_low_energy_devices_info;
  NSDictionary* low_energy_devices_info = data[@"CoreBluetoothCache"];
  if (!low_energy_devices_info)
    return updated_low_energy_devices_info;

  for (NSString* identifier in low_energy_devices_info) {
    NSDictionary* device_info = low_energy_devices_info[identifier];
    if (!device_info)
      continue;

    NSString* raw_device_address = device_info[@"DeviceAddress"];
    if (!raw_device_address)
      continue;

    NSString* formatted_device_address =
        [raw_device_address stringByReplacingOccurrencesOfString:@"-"
                                                      withString:@":"];
    updated_low_energy_devices_info[base::SysNSStringToUTF8(identifier)] =
        base::SysNSStringToUTF8(formatted_device_address);
  }

  return updated_low_energy_devices_info;
}

// static
const base::FilePath&
BluetoothLowEnergyDeviceWatcherMac::BluetoothPlistFilePath() {
  static const base::FilePath file_path(kBluetoothPlistFilePath);
  return file_path;
}

void BluetoothLowEnergyDeviceWatcherMac::AddBluetoothPropertyListFileWatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  property_list_watcher_->Watch(
      BluetoothPlistFilePath(), base::FilePathWatcher::Type::kNonRecursive,
      base::BindRepeating(&BluetoothLowEnergyDeviceWatcherMac::
                              OnPropertyListFileChangedOnFileThread,
                          this));
}

}  // namespace device
