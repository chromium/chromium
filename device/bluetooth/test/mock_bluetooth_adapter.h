// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_ADAPTER_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_ADAPTER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace device {

class MockBluetoothAdapter : public BluetoothAdapter {
 public:
  class Observer : public BluetoothAdapter::Observer {
   public:
    explicit Observer(scoped_refptr<BluetoothAdapter> adapter);

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

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
  };

  MockBluetoothAdapter();

  bool IsInitialized() const override { return true; }

  void Initialize(base::OnceClosure callback) override;
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  void Shutdown() override;
#endif
  MOCK_METHOD1(AddObserver, void(BluetoothAdapter::Observer*));
  MOCK_METHOD1(RemoveObserver, void(BluetoothAdapter::Observer*));
  MOCK_CONST_METHOD0(GetAddress, std::string());
  MOCK_CONST_METHOD0(GetName, std::string());
  MOCK_METHOD3(SetName,
               void(const std::string& name,
                    base::OnceClosure callback,
                    ErrorCallback error_callback));
  MOCK_CONST_METHOD0(IsPresent, bool());
  MOCK_CONST_METHOD0(IsPowered, bool());
  MOCK_CONST_METHOD0(GetOsPermissionStatus, PermissionStatus());
  MOCK_METHOD1(RequestSystemPermission, void(RequestSystemPermissionCallback));
  MOCK_CONST_METHOD0(CanPower, bool());
  MOCK_METHOD3(SetPowered,
               void(bool powered,
                    base::OnceClosure callback,
                    ErrorCallback error_callback));
  MOCK_CONST_METHOD0(IsDiscoverable, bool());
  MOCK_METHOD3(SetDiscoverable,
               void(bool discoverable,
                    base::OnceClosure callback,
                    ErrorCallback error_callback));
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  MOCK_CONST_METHOD0(GetDiscoverableTimeout, base::TimeDelta());
#endif

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
                    CreateServiceCallback callback,
                    CreateServiceErrorCallback error_callback));
  MOCK_METHOD4(CreateL2capService,
               void(const BluetoothUUID& uuid,
                    const ServiceOptions& options,
                    CreateServiceCallback callback,
                    CreateServiceErrorCallback error_callback));
  MOCK_CONST_METHOD1(GetGattService,
                     BluetoothLocalGattService*(const std::string& identifier));
  MOCK_METHOD3(CreateLocalGattService,
               base::WeakPtr<BluetoothLocalGattService>(
                   const BluetoothUUID& uuid,
                   bool is_primary,
                   BluetoothLocalGattService::Delegate* delegate));

#if BUILDFLAG(IS_CHROMEOS)
  MOCK_METHOD3(SetServiceAllowList,
               void(const UUIDList& uuids,
                    base::OnceClosure callback,
                    ErrorCallback error_callback));
  MOCK_METHOD0(GetLowEnergyScanSessionHardwareOffloadingStatus,
               LowEnergyScanSessionHardwareOffloadingStatus());
  MOCK_METHOD2(
      StartLowEnergyScanSession,
      std::unique_ptr<BluetoothLowEnergyScanSession>(
          std::unique_ptr<BluetoothLowEnergyScanFilter> filter,
          base::WeakPtr<BluetoothLowEnergyScanSession::Delegate> delegate));
  MOCK_METHOD0(GetSupportedRoles, std::vector<BluetoothRole>());
  MOCK_CONST_METHOD0(IsExtendedAdvertisementsAvailable, bool());
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  MOCK_METHOD0(SetStandardChromeOSAdapterName, void());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  MOCK_METHOD4(
      ConnectDevice,
      void(const std::string& address,
           const std::optional<BluetoothDevice::AddressType>& address_type,
           ConnectDeviceCallback callback,
           ConnectDeviceErrorCallback error_callback));

#endif

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
  base::ObserverList<
      device::BluetoothAdapter::Observer>::UncheckedAndDanglingUntriaged&
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
      CreateAdvertisementCallback callback,
      AdvertisementErrorCallback error_callback) override;
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  void SetAdvertisingInterval(
      const base::TimeDelta& min,
      const base::TimeDelta& max,
      base::OnceClosure callback,
      AdvertisementErrorCallback error_callback) override;
  void ResetAdvertising(base::OnceClosure callback,
                        AdvertisementErrorCallback error_callback) override;
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
