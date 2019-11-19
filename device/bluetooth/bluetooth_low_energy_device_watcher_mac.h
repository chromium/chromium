// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DEVICE_WATCHER_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DEVICE_WATCHER_MAC_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/post_task.h"
#include "device/bluetooth/bluetooth_export.h"

@class NSDictionary;

namespace device {

// Manages watching and reading system bluetooth property list file in
// background thread to obtain a list of known Bluetooth low energy devices.
class DEVICE_BLUETOOTH_EXPORT BluetoothLowEnergyDeviceWatcherMac
    : public base::RefCountedThreadSafe<BluetoothLowEnergyDeviceWatcherMac> {
 public:
  using LowEnergyDeviceListUpdatedCallback =
      base::RepeatingCallback<void(std::map<std::string, std::string>)>;

  static scoped_refptr<BluetoothLowEnergyDeviceWatcherMac>
  CreateAndStartWatching(
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
      LowEnergyDeviceListUpdatedCallback
          update_low_energy_device_list_callback);

  BluetoothLowEnergyDeviceWatcherMac(
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
      LowEnergyDeviceListUpdatedCallback
          update_low_energy_device_list_callback);

 protected:
  virtual ~BluetoothLowEnergyDeviceWatcherMac();

  // Read system bluetooth property list file for change and fetches
  // identifier and device address of system paired bluetooth devices.
  void OnPropertyListFileChangedOnFileThread(const base::FilePath& path,
                                             bool error);

  // Overriden in tests.
  virtual void Init();
  virtual void ReadBluetoothPropertyListFile();

  std::map<std::string, std::string> ParseBluetoothDevicePropertyListData(
      NSDictionary* data);

  LowEnergyDeviceListUpdatedCallback low_energy_device_list_updated_callback() {
    return low_energy_device_list_updated_callback_;
  }

  scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner() {
    return ui_thread_task_runner_;
  }

 private:
  friend class BluetoothAdapterMac;
  friend class base::RefCountedThreadSafe<BluetoothLowEnergyDeviceWatcherMac>;

  void AddBluetoothPropertyListFileWatcher();

  static const base::FilePath& BluetoothPlistFilePath();

  // Thread runner to watch, read, and parse bluetooth property list file.
  scoped_refptr<base::SequencedTaskRunner> file_thread_task_runner_ =
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
  scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner_;
  LowEnergyDeviceListUpdatedCallback low_energy_device_list_updated_callback_;
  std::unique_ptr<base::FilePathWatcher> property_list_watcher_ =
      std::make_unique<base::FilePathWatcher>();

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(BluetoothLowEnergyDeviceWatcherMac);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DEVICE_WATCHER_MAC_H_
