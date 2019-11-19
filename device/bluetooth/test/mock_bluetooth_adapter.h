// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_ADAPTER_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_ADAPTER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class MockBluetoothAdapter : public BluetoothAdapter {
 public:
  class Observer : public BluetoothAdapter::Observer {
   public:
    Observer(scoped_refptr<BluetoothAdapter> adapter);
    ~Observer() override;

    MOCK_METHOD2(AdapterPresentChanged, void(BluetoothAdapter*, bool));
    MOCK_METHOD2(AdapterPoweredChanged, void(BluetoothAdapter*, bool));
    MOCK_METHOD2(AdapterDiscoveringChanged, void(BluetoothAdapter*, bool));
    MOCK_METHOD2(DeviceAdded, void(BluetoothAdapter*, BluetoothDevice*));
    MOCK_METHOD2(DeviceChanged, void(BluetoothAdapter*, BluetoothDevice*));
    MOCK_METHOD2(DeviceRemoved, void(BluetoothAdapter*, BluetoothDevice*));
    MOCK_METHOD3(GattCharacteristicValueChanged,
                 void(BluetoothAdapter*,
                      BluetoothRemoteGattCharacteristic*,
                      const std::vector<uint8_t>&));

   private:
    const scoped_refptr<BluetoothAdapter> adapter_;

    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  MockBluetoothAdapter();

  bool IsInitialized() const override { return true; }

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
  void Shutdown() override;
#endif
  MOCK_METHOD1(AddObserver, void(BluetoothAdapter::Observer*));
  MOCK_METHOD1(RemoveObserver, void(BluetoothAdapter::Observer*));
  MOCK_CONST_METHOD0(GetAddress, std::string());
  MOCK_CONST_METHOD0(GetName, std::string());
  MOCK_METHOD3(SetName,
               void(const std::string& name,
                    const base::Closure& callback,
                    const ErrorCallback& error_callback));
  MOCK_CONST_METHOD0(IsPresent, bool());
  MOCK_CONST_METHOD0(IsPowered, bool());
  MOCK_CONST_METHOD0(CanPower, bool());
  MOCK_METHOD3(SetPowered,
               void(bool powered,
                    const base::Closure& callback,
                    const ErrorCallback& error_callback));
  MOCK_CONST_METHOD0(IsDiscoverable, bool());
  MOCK_METHOD3(SetDiscoverable,
               void(bool discoverable,
                    const base::Closure& callback,
                    const ErrorCallback& error_callback));
  MOCK_CONST_METHOD0(IsDiscovering, bool());
  MOCK_METHOD2(
      StartScanWithFilter_,
      void(const BluetoothDiscoveryFilter*,
           base::OnceCallback<void(/*is_error=*/bool,
                                   UMABluetoothDiscoverySessionOutcome)>&
               callback));
  MOCK_METHOD2(
      UpdateFilter_,
      void(const BluetoothDiscoveryFilter*,
           base::OnceCallback<void(/*is_error=*/bool,
                                   UMABluetoothDiscoverySessionOutcome)>&
               callback));
  MOCK_METHOD1(StopScan, void(DiscoverySessionResultCallback callback));
  MOCK_METHOD3(SetDiscoveryFilterRaw,
               void(const BluetoothDiscoveryFilter*,
                    const base::RepeatingClosure& callback,
                    DiscoverySessionErrorCallback& error_callback));
  MOCK_CONST_METHOD0(GetDevices, BluetoothAdapter::ConstDeviceList());
  MOCK_METHOD1(GetDevice, BluetoothDevice*(const std::string& address));
  MOCK_CONST_METHOD1(GetDevice,
                     const BluetoothDevice*(const std::string& address));
  MOCK_CONST_METHOD0(GetUUIDs, UUIDList());
  MOCK_METHOD2(AddPairingDelegate,
               void(BluetoothDevice::PairingDelegate* pairing_delegate,
                    enum PairingDelegatePriority priority));
  MOCK_METHOD1(RemovePairingDelegate,
               void(BluetoothDevice::PairingDelegate* pairing_delegate));
  MOCK_METHOD0(DefaultPairingDelegate, BluetoothDevice::PairingDelegate*());
  MOCK_METHOD4(CreateRfcommService,
               void(const BluetoothUUID& uuid,
                    const ServiceOptions& options,
                    const CreateServiceCallback& callback,
                    const CreateServiceErrorCallback& error_callback));
  MOCK_METHOD4(CreateL2capService,
               void(const BluetoothUUID& uuid,
                    const ServiceOptions& options,
                    const CreateServiceCallback& callback,
                    const CreateServiceErrorCallback& error_callback));
  MOCK_CONST_METHOD1(GetGattService,
                     BluetoothLocalGattService*(const std::string& identifier));

  // BluetoothAdapter is supposed to manage the lifetime of BluetoothDevices.
  // This method takes ownership of the MockBluetoothDevice. This is only for
  // convenience as far testing is concerned and it's possible to write test
  // cases without using these functions.
  void AddMockDevice(std::unique_ptr<MockBluetoothDevice> mock_device);
  std::unique_ptr<MockBluetoothDevice> RemoveMockDevice(
      const std::string& address);
  BluetoothAdapter::ConstDeviceList GetConstMockDevices();
  BluetoothAdapter::DeviceList GetMockDevices();

  // The observers are maintained by the default behavior of AddObserver() and
  // RemoveObserver(). Test fakes can use this function to notify the observers
  // about events.
  base::ObserverList<device::BluetoothAdapter::Observer>::Unchecked&
  GetObservers() {
    return observers_;
  }

 protected:
  base::WeakPtr<BluetoothAdapter> GetWeakPtr() override;
  bool SetPoweredImpl(bool powered) override;
  void StartScanWithFilter(
      std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback) override;
  void UpdateFilter(std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
                    DiscoverySessionResultCallback callback) override;
  void RegisterAdvertisement(
      std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
      const CreateAdvertisementCallback& callback,
      const AdvertisementErrorCallback& error_callback) override;
#if defined(OS_CHROMEOS) || defined(OS_LINUX)
  void SetAdvertisingInterval(
      const base::TimeDelta& min,
      const base::TimeDelta& max,
      const base::Closure& callback,
      const AdvertisementErrorCallback& error_callback) override;
  void ResetAdvertising(
      const base::Closure& callback,
      const AdvertisementErrorCallback& error_callback) override;
#endif
  ~MockBluetoothAdapter() override;

  MOCK_METHOD1(RemovePairingDelegateInternal,
               void(BluetoothDevice::PairingDelegate* pairing_delegate));

  std::vector<std::unique_ptr<MockBluetoothDevice>> mock_devices_;

  // This must be the last field in the class so that weak pointers are
  // invalidated first.
  base::WeakPtrFactory<MockBluetoothAdapter> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_ADAPTER_H_
