// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_TASK_MANAGER_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_TASK_MANAGER_WIN_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/win/scoped_handle.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_low_energy_win.h"

namespace base {

class SequencedTaskRunner;

}  // namespace base

namespace device {

namespace win {
class BluetoothClassicWrapper;
class BluetoothLowEnergyWrapper;
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

   private:
    DISALLOW_COPY_AND_ASSIGN(ServiceRecordState);
  };

  struct DEVICE_BLUETOOTH_EXPORT DeviceState {
    DeviceState();
    ~DeviceState();

    bool is_bluetooth_classic() const { return path.empty(); }

    // Properties common to Bluetooth Classic and LE devices.
    std::string address;  // This uniquely identifies the device.
    base::Optional<std::string> name;  // Friendly name
    bool visible;
    bool connected;
    bool authenticated;
    std::vector<std::unique_ptr<ServiceRecordState>> service_record_states;
    // Properties specific to Bluetooth Classic devices.
    uint32_t bluetooth_class;
    // Properties specific to Bluetooth LE devices.
    base::FilePath path;

   private:
    DISALLOW_COPY_AND_ASSIGN(DeviceState);
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

  static scoped_refptr<BluetoothTaskManagerWin> CreateForTesting(
      std::unique_ptr<win::BluetoothClassicWrapper> classic_wrapper,
      std::unique_ptr<win::BluetoothLowEnergyWrapper> le_wrapper,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  static BluetoothUUID BluetoothLowEnergyUuidToBluetoothUuid(
      const BTH_LE_UUID& bth_le_uuid);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void Initialize();
  void InitializeWithBluetoothTaskRunner(
      scoped_refptr<base::SequencedTaskRunner> bluetooth_task_runner);

  void PostSetPoweredBluetoothTask(
      bool powered,
      const base::Closure& callback,
      const BluetoothAdapter::ErrorCallback& error_callback);
  void PostStartDiscoveryTask();
  void PostStopDiscoveryTask();

  // Callbacks of asynchronous operations of GATT service.
  typedef base::Callback<void(HRESULT)> HResultCallback;
  typedef base::Callback<
      void(std::unique_ptr<BTH_LE_GATT_CHARACTERISTIC>, uint16_t, HRESULT)>
      GetGattIncludedCharacteristicsCallback;
  typedef base::Callback<
      void(std::unique_ptr<BTH_LE_GATT_DESCRIPTOR>, uint16_t, HRESULT)>
      GetGattIncludedDescriptorsCallback;
  typedef base::Callback<void(std::unique_ptr<BTH_LE_GATT_CHARACTERISTIC_VALUE>,
                              HRESULT)>
      ReadGattCharacteristicValueCallback;
  typedef base::Callback<void(std::unique_ptr<std::vector<uint8_t>>)>
      GattCharacteristicValueChangedCallback;
  using GattEventRegistrationCallback =
      base::OnceCallback<void(PVOID, HRESULT)>;

  // Get all included characteristics of a given service. The service is
  // uniquely identified by its |uuid| and |attribute_handle| with service
  // device |service_path|. The result is returned asynchronously through
  // |callback|.
  void PostGetGattIncludedCharacteristics(
      const base::FilePath& service_path,
      const BluetoothUUID& uuid,
      uint16_t attribute_handle,
      const GetGattIncludedCharacteristicsCallback& callback);

  // Get all included descriptors of a given |characterisitc| in service
  // with |service_path|. The result is returned asynchronously through
  // |callback|.
  void PostGetGattIncludedDescriptors(
      const base::FilePath& service_path,
      const PBTH_LE_GATT_CHARACTERISTIC characteristic,
      const GetGattIncludedDescriptorsCallback& callback);

  // Post read the value of a given |characteristic| in service with
  // |service_path|. The result is returned asynchronously through |callback|.
  void PostReadGattCharacteristicValue(
      const base::FilePath& device_path,
      const PBTH_LE_GATT_CHARACTERISTIC characteristic,
      const ReadGattCharacteristicValueCallback& callback);

  // Post write the value of a given |characteristic| in service with
  // |service_path| to |new_value|. The operation result is returned
  // asynchronously through |callback|.
  void PostWriteGattCharacteristicValue(
      const base::FilePath& service_path,
      const PBTH_LE_GATT_CHARACTERISTIC characteristic,
      const std::vector<uint8_t>& new_value,
      const HResultCallback& callback);

  // Post a task to register to receive value changed notifications from
  // |characteristic| in service with |service_path|. |ccc_descriptor| is the
  // Client Characteristic Configuration descriptor. |registered_callback| is
  // the function to be invoked if the event occured. The operation result is
  // returned asynchronously through |callback|.
  void PostRegisterGattCharacteristicValueChangedEvent(
      const base::FilePath& service_path,
      const PBTH_LE_GATT_CHARACTERISTIC characteristic,
      const PBTH_LE_GATT_DESCRIPTOR ccc_descriptor,
      GattEventRegistrationCallback callback,
      const GattCharacteristicValueChangedCallback& registered_callback);

  // Post a task to unregister from value change notifications. |event_handle|
  // was returned by PostRegisterGattCharacteristicValueChangedEvent.
  void PostUnregisterGattCharacteristicValueChangedEvent(PVOID event_handle);

 private:
  friend class base::RefCountedThreadSafe<BluetoothTaskManagerWin>;
  friend class BluetoothTaskManagerWinTest;

  static const int kPollIntervalMs;

  BluetoothTaskManagerWin(
      std::unique_ptr<win::BluetoothClassicWrapper> classic_wrapper,
      std::unique_ptr<win::BluetoothLowEnergyWrapper> le_wrapper,
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
                  const base::Closure& callback,
                  const BluetoothAdapter::ErrorCallback& error_callback);

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

  // Enumerate Bluetooth Low Energy devices.
  bool SearchLowEnergyDevices(
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

  // Discover Bluetooth Low Energy services for the given |device_path|.
  bool DiscoverLowEnergyDeviceServices(
      const base::FilePath& device_path,
      std::vector<std::unique_ptr<ServiceRecordState>>* service_record_states);

  // Search for device paths of the GATT services in |*service_record_states|
  // from |device_address|.
  // Return true if we were able to match all services with a service device
  // path.
  bool SearchForGattServiceDevicePaths(
      const std::string device_address,
      std::vector<std::unique_ptr<ServiceRecordState>>* service_record_states);

  // GATT service related functions.
  void GetGattIncludedCharacteristics(
      base::FilePath device_path,
      BluetoothUUID uuid,
      uint16_t attribute_handle,
      const GetGattIncludedCharacteristicsCallback& callback);
  void GetGattIncludedDescriptors(
      base::FilePath service_path,
      BTH_LE_GATT_CHARACTERISTIC characteristic,
      const GetGattIncludedDescriptorsCallback& callback);
  void ReadGattCharacteristicValue(
      base::FilePath device_path,
      BTH_LE_GATT_CHARACTERISTIC characteristic,
      const ReadGattCharacteristicValueCallback& callback);
  void WriteGattCharacteristicValue(base::FilePath service_path,
                                    BTH_LE_GATT_CHARACTERISTIC characteristic,
                                    std::vector<uint8_t> new_value,
                                    const HResultCallback& callback);
  void RegisterGattCharacteristicValueChangedEvent(
      base::FilePath service_path,
      BTH_LE_GATT_CHARACTERISTIC characteristic,
      BTH_LE_GATT_DESCRIPTOR ccc_descriptor,
      GattEventRegistrationCallback callback,
      const GattCharacteristicValueChangedCallback& registered_callback);
  void UnregisterGattCharacteristicValueChangedEvent(PVOID event_handle);

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
  std::unique_ptr<win::BluetoothLowEnergyWrapper> le_wrapper_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothTaskManagerWin);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_TASK_MANAGER_WIN_H_
