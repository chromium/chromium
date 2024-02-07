// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_TASK_MANAGER_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_TASK_MANAGER_WIN_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/win/scoped_handle.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_export.h"

namespace base {

class SequencedTaskRunner;

}  // namespace base

namespace device {

namespace win {
class BluetoothClassicWrapper;
}  // namespace win

// Manages the blocking Bluetooth tasks using |SequencedWorkerPool|. It runs
// bluetooth tasks using |SequencedWorkerPool| and informs its observers of
// bluetooth adapter state changes and any other bluetooth device inquiry
// result.
//
// It delegates the blocking Windows API calls to |bluetooth_task_runner_|'s
// message loop, and receives responses via methods like OnAdapterStateChanged
// posted to UI thread.
class DEVICE_BLUETOOTH_EXPORT BluetoothTaskManagerWin
    : public base::RefCountedThreadSafe<BluetoothTaskManagerWin> {
 public:
  struct DEVICE_BLUETOOTH_EXPORT AdapterState {
    AdapterState();
    ~AdapterState();
    std::string name;
    std::string address;
    bool powered;
  };

  struct DEVICE_BLUETOOTH_EXPORT ServiceRecordState {
    ServiceRecordState();

    ServiceRecordState(const ServiceRecordState&) = delete;
    ServiceRecordState& operator=(const ServiceRecordState&) = delete;

    ~ServiceRecordState();
    // Properties common to Bluetooth Classic and LE devices.
    std::string name;
    // Properties specific to Bluetooth Classic devices.
    std::vector<uint8_t> sdp_bytes;
    // Properties specific to Bluetooth LE devices.
    BluetoothUUID gatt_uuid;
    uint16_t attribute_handle;
    // GATT service device path.
    // Note: Operation of the included characteristics and descriptors of this
    // service must use service device path instead of resident device device
    // path.
    base::FilePath path;
  };

  struct DEVICE_BLUETOOTH_EXPORT DeviceState {
    DeviceState();

    DeviceState(const DeviceState&) = delete;
    DeviceState& operator=(const DeviceState&) = delete;

    ~DeviceState();

    // Properties common to Bluetooth Classic and LE devices.
    std::string address;  // This uniquely identifies the device.
    std::optional<std::string> name;  // Friendly name
    bool visible;
    bool connected;
    bool authenticated;
    std::vector<std::unique_ptr<ServiceRecordState>> service_record_states;
    uint32_t bluetooth_class;
  };

  class DEVICE_BLUETOOTH_EXPORT Observer {
   public:
     virtual ~Observer() {}

     virtual void AdapterStateChanged(const AdapterState& state) {}
     virtual void DiscoveryStarted(bool success) {}
     virtual void DiscoveryStopped() {}
     // Called when the adapter has just been polled for the list of *all* known
     // devices. This includes devices previously paired, devices paired using
     // the underlying Operating System UI, and devices discovered recently due
     // to an active discovery session. Note that for a given device (address),
     // the associated state can change over time. For example, during a
     // discovery session, the "friendly" name may initially be "unknown" before
     // the actual name is retrieved in subsequent poll events.
     virtual void DevicesPolled(
         const std::vector<std::unique_ptr<DeviceState>>& devices) {}
  };

  explicit BluetoothTaskManagerWin(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  BluetoothTaskManagerWin(const BluetoothTaskManagerWin&) = delete;
  BluetoothTaskManagerWin& operator=(const BluetoothTaskManagerWin&) = delete;

  static scoped_refptr<BluetoothTaskManagerWin> CreateForTesting(
      std::unique_ptr<win::BluetoothClassicWrapper> classic_wrapper,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void Initialize();
  void InitializeWithBluetoothTaskRunner(
      scoped_refptr<base::SequencedTaskRunner> bluetooth_task_runner);

  void PostSetPoweredBluetoothTask(
      bool powered,
      base::OnceClosure callback,
      BluetoothAdapter::ErrorCallback error_callback);
  void PostStartDiscoveryTask();
  void PostStopDiscoveryTask();

 private:
  friend class base::RefCountedThreadSafe<BluetoothTaskManagerWin>;
  friend class BluetoothTaskManagerWinTest;

  static const int kPollIntervalMs;

  BluetoothTaskManagerWin(
      std::unique_ptr<win::BluetoothClassicWrapper> classic_wrapper,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);
  virtual ~BluetoothTaskManagerWin();

  // Logs Win32 errors occurring during polling on the worker thread. The method
  // may discard messages to avoid logging being too verbose.
  void LogPollingError(const char* message, int win32_error);

  // Notify all Observers of updated AdapterState. Should only be called on the
  // UI thread.
  void OnAdapterStateChanged(const AdapterState* state);
  void OnDiscoveryStarted(bool success);
  void OnDiscoveryStopped();
  void OnDevicesPolled(std::vector<std::unique_ptr<DeviceState>> devices);

  // Called on BluetoothTaskRunner.
  void StartPolling();
  void PollAdapter();
  void PostAdapterStateToUi();
  void SetPowered(bool powered,
                  base::OnceClosure callback,
                  BluetoothAdapter::ErrorCallback error_callback);

  // Starts discovery. Once the discovery starts, it issues a discovery inquiry
  // with a short timeout, then issues more inquiries with greater timeout
  // values. The discovery finishes when StopDiscovery() is called or timeout
  // has reached its maximum value.
  void StartDiscovery();
  void StopDiscovery();

  // Issues a device inquiry that runs for |timeout_multiplier| * 1.28 seconds.
  // This posts itself again with |timeout_multiplier| + 1 until
  // |timeout_multiplier| reaches the maximum value or stop discovery call is
  // received.
  void DiscoverDevices(int timeout_multiplier);

  // Fetch already known device information. Similar to |StartDiscovery|, except
  // this function does not issue a discovery inquiry. Instead it gets the
  // device info cached in the adapter.
  void GetKnownDevices();

  // Looks for Bluetooth Classic and Low Energy devices, as well as the services
  // exposed by those devices.
  bool SearchDevices(int timeout_multiplier,
                     bool search_cached_devices_only,
                     std::vector<std::unique_ptr<DeviceState>>* device_list);

  // Sends a device search API call to the adapter to look for Bluetooth Classic
  // devices.
  bool SearchClassicDevices(
      int timeout_multiplier,
      bool search_cached_devices_only,
      std::vector<std::unique_ptr<DeviceState>>* device_list);

  // Discover services for the devices in |device_list|.
  bool DiscoverServices(std::vector<std::unique_ptr<DeviceState>>* device_list,
                        bool search_cached_services_only);

  // Discover Bluetooth Classic services for the given |device_address|.
  bool DiscoverClassicDeviceServices(
      const std::string& device_address,
      const GUID& protocol_uuid,
      bool search_cached_services_only,
      std::vector<std::unique_ptr<ServiceRecordState>>* service_record_states);

  // Discover Bluetooth Classic services for the given |device_address|.
  // Returns a Win32 error code.
  int DiscoverClassicDeviceServicesWorker(
      const std::string& device_address,
      const GUID& protocol_uuid,
      bool search_cached_services_only,
      std::vector<std::unique_ptr<ServiceRecordState>>* service_record_states);

  // UI task runner reference.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  scoped_refptr<base::SequencedTaskRunner> bluetooth_task_runner_;

  // List of observers interested in event notifications.
  base::ObserverList<Observer>::Unchecked observers_;

  // indicates whether the adapter is in discovery mode or not.
  bool discovering_ = false;

  // Use for discarding too many log messages.
  base::TimeTicks current_logging_batch_ticks_;
  int current_logging_batch_count_ = 0;

  // Wrapper around the Windows Bluetooth APIs. Owns the radio handle.
  std::unique_ptr<win::BluetoothClassicWrapper> classic_wrapper_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_TASK_MANAGER_WIN_H_
