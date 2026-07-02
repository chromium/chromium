// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DEVICE_WATCHER_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DEVICE_WATCHER_MAC_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "device/bluetooth/bluetooth_export.h"

@class NSDictionary;

namespace device {

// Manages watching and reading system bluetooth property list file in
// background thread to obtain a list of known Bluetooth low energy devices.
class DEVICE_BLUETOOTH_EXPORT BluetoothLowEnergyDeviceWatcherMac {
 public:
  using LowEnergyDeviceListUpdatedCallback =
      base::RepeatingCallback<void(std::map<std::string, std::string>)>;

  static std::unique_ptr<BluetoothLowEnergyDeviceWatcherMac>
  CreateAndStartWatching(
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
      LowEnergyDeviceListUpdatedCallback
          update_low_energy_device_list_callback);

  BluetoothLowEnergyDeviceWatcherMac(
      const BluetoothLowEnergyDeviceWatcherMac&) = delete;
  BluetoothLowEnergyDeviceWatcherMac& operator=(
      const BluetoothLowEnergyDeviceWatcherMac&) = delete;

  BluetoothLowEnergyDeviceWatcherMac(
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
      LowEnergyDeviceListUpdatedCallback
          update_low_energy_device_list_callback);

  virtual ~BluetoothLowEnergyDeviceWatcherMac();

  void set_destroy_callback_for_testing(base::OnceClosure callback) {
    destroy_callback_for_testing_ = std::move(callback);
  }

 protected:
  // Read system bluetooth property list file for change and fetches
  // identifier and device address of system paired bluetooth devices.
  // It returns std::nullopt while reading error occurs.
  static std::optional<std::map<std::string, std::string>>
  OnPropertyListFileChangedOnFileThread(const base::FilePath& path, bool error);

  // Call OnPropertyListFileChangedOnFileThread() first, and then it calls
  // RunLowEnergyDeviceListUpdatedCallback() in `ui_thread_task_runner`.
  static void OnPropertyListFileChangedAndRunCallback(
      base::WeakPtr<BluetoothLowEnergyDeviceWatcherMac> weak_watcher,
      scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner,
      const base::FilePath& path,
      bool error);

  static std::map<std::string, std::string>
  ParseBluetoothDevicePropertyListData(NSDictionary* data);

  // Overriden in tests.
  virtual void Init();
  virtual void ReadBluetoothPropertyListFile();

  LowEnergyDeviceListUpdatedCallback low_energy_device_list_updated_callback() {
    return low_energy_device_list_updated_callback_;
  }

  scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner() {
    return ui_thread_task_runner_;
  }

 private:
  friend class BluetoothLowEnergyAdapterApple;

  // Run `low_energy_device_list_updated_callback_` immediately if
  // `low_energy_devices_info` has value and
  // `low_energy_device_list_updated_callback_` is not null.
  // This method must be called on `ui_thread_task_runner_`.
  void RunLowEnergyDeviceListUpdatedCallback(
      const std::optional<std::map<std::string, std::string>>&
          low_energy_devices_info);

  // Thread runner to watch, read, and parse bluetooth property list file.
  scoped_refptr<base::SequencedTaskRunner> file_thread_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
  scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner_;
  LowEnergyDeviceListUpdatedCallback low_energy_device_list_updated_callback_;

  // `property_list_watcher_` is ensured to be created, destroyed, and used on
  // `file_thread_task_runner_`.
  base::SequenceBound<base::FilePathWatcher> property_list_watcher_{
      file_thread_task_runner_};

  base::OnceClosure destroy_callback_for_testing_;

  base::WeakPtrFactory<BluetoothLowEnergyDeviceWatcherMac> weak_ptr_factory_{
      this};
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DEVICE_WATCHER_MAC_H_
