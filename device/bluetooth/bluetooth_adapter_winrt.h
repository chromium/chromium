// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_WINRT_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_WINRT_H_

#include <windows.devices.bluetooth.h>
#include <windows.devices.enumeration.h>
#include <windows.devices.radios.h>
#include <wrl/client.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_export.h"

namespace base {
class ScopedClosureRunner;
}

namespace device {

class BluetoothAdvertisementWinrt;
class BluetoothDeviceWinrt;

class DEVICE_BLUETOOTH_EXPORT BluetoothAdapterWinrt : public BluetoothAdapter {
 public:
  // BluetoothAdapter:
  std::string GetAddress() const override;
  std::string GetName() const override;
  void SetName(const std::string& name,
               const base::Closure& callback,
               const ErrorCallback& error_callback) override;
  bool IsInitialized() const override;
  bool IsPresent() const override;
  bool CanPower() const override;
  bool IsPowered() const override;
  bool IsDiscoverable() const override;
  void SetDiscoverable(bool discoverable,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) override;
  bool IsDiscovering() const override;
  UUIDList GetUUIDs() const override;
  void CreateRfcommService(
      const BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override;
  void CreateL2capService(
      const BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override;
  void RegisterAdvertisement(
      std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
      const CreateAdvertisementCallback& callback,
      const AdvertisementErrorCallback& error_callback) override;
  std::vector<BluetoothAdvertisement*> GetPendingAdvertisementsForTesting()
      const override;
  BluetoothLocalGattService* GetGattService(
      const std::string& identifier) const override;

  ABI::Windows::Devices::Radios::IRadio* GetRadioForTesting();
  ABI::Windows::Devices::Enumeration::IDeviceWatcher*
  GetPoweredRadioWatcherForTesting();

 protected:
  friend class BluetoothAdapterWin;
  friend class BluetoothTestWinrt;

  BluetoothAdapterWinrt();
  ~BluetoothAdapterWinrt() override;

  void Init(InitCallback init_cb);
  // Allow tests to provide their own implementations of statics.
  void InitForTests(
      InitCallback init_cb,
      Microsoft::WRL::ComPtr<
          ABI::Windows::Devices::Bluetooth::IBluetoothAdapterStatics>
          bluetooth_adapter_statics,
      Microsoft::WRL::ComPtr<
          ABI::Windows::Devices::Enumeration::IDeviceInformationStatics>
          device_information_statics,
      Microsoft::WRL::ComPtr<ABI::Windows::Devices::Radios::IRadioStatics>
          radio_statics);

  // BluetoothAdapter:
  base::WeakPtr<BluetoothAdapter> GetWeakPtr() override;
  bool SetPoweredImpl(bool powered) override;
  void UpdateFilter(std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
                    DiscoverySessionResultCallback callback) override;
  void StartScanWithFilter(
      std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback) override;
  void StopScan(DiscoverySessionResultCallback callback) override;
  void RemovePairingDelegateInternal(
      BluetoothDevice::PairingDelegate* pairing_delegate) override;

  // Declared virtual so that it can be overridden by tests.
  virtual HRESULT ActivateBluetoothAdvertisementLEWatcherInstance(
      ABI::Windows::Devices::Bluetooth::Advertisement::
          IBluetoothLEAdvertisementWatcher** instance) const;

  virtual scoped_refptr<BluetoothAdvertisementWinrt> CreateAdvertisement()
      const;

  virtual std::unique_ptr<BluetoothDeviceWinrt> CreateDevice(
      uint64_t raw_address);

 private:
  struct StaticsInterfaces {
    StaticsInterfaces(
        Microsoft::WRL::ComPtr<IAgileReference>,   // IBluetoothStatics
        Microsoft::WRL::ComPtr<IAgileReference>,   // IDeviceInformationStatics
        Microsoft::WRL::ComPtr<IAgileReference>);  // IRadioStatics
    StaticsInterfaces();
    StaticsInterfaces(const StaticsInterfaces&);
    ~StaticsInterfaces();

    Microsoft::WRL::ComPtr<IAgileReference> adapter_statics;
    Microsoft::WRL::ComPtr<IAgileReference> device_information_statics;
    Microsoft::WRL::ComPtr<IAgileReference> radio_statics;
  };

  static StaticsInterfaces PerformSlowInitTasks();
  static StaticsInterfaces GetAgileReferencesForStatics(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Devices::Bluetooth::IBluetoothAdapterStatics>
          adapter_statics,
      Microsoft::WRL::ComPtr<
          ABI::Windows::Devices::Enumeration::IDeviceInformationStatics>
          device_information_statics,
      Microsoft::WRL::ComPtr<ABI::Windows::Devices::Radios::IRadioStatics>
          radio_statics);

  // CompleteInitAgile is a proxy to CompleteInit that resolves agile
  // references.
  void CompleteInitAgile(InitCallback init_cb, StaticsInterfaces statics);
  void CompleteInit(
      InitCallback init_cb,
      Microsoft::WRL::ComPtr<
          ABI::Windows::Devices::Bluetooth::IBluetoothAdapterStatics>
          bluetooth_adapter_statics,
      Microsoft::WRL::ComPtr<
          ABI::Windows::Devices::Enumeration::IDeviceInformationStatics>
          device_information_statics,
      Microsoft::WRL::ComPtr<ABI::Windows::Devices::Radios::IRadioStatics>
          radio_statics);

  void OnGetDefaultAdapter(
      base::ScopedClosureRunner on_init,
      Microsoft::WRL::ComPtr<
          ABI::Windows::Devices::Bluetooth::IBluetoothAdapter> adapter);

  void OnCreateFromIdAsync(
      base::ScopedClosureRunner on_init,
      Microsoft::WRL::ComPtr<
          ABI::Windows::Devices::Enumeration::IDeviceInformation>
          device_information);

  void OnRequestRadioAccess(
      base::ScopedClosureRunner on_init,
      ABI::Windows::Devices::Radios::RadioAccessStatus access_status);

  void OnGetRadio(
      base::ScopedClosureRunner on_init,
      Microsoft::WRL::ComPtr<ABI::Windows::Devices::Radios::IRadio> radio);

  void OnSetRadioState(
      ABI::Windows::Devices::Radios::RadioAccessStatus access_status);

  void OnRadioStateChanged(ABI::Windows::Devices::Radios::IRadio* radio,
                           IInspectable* object);

  void OnPoweredRadioAdded(
      ABI::Windows::Devices::Enumeration::IDeviceWatcher* watcher,
      ABI::Windows::Devices::Enumeration::IDeviceInformation* info);

  void OnPoweredRadioRemoved(
      ABI::Windows::Devices::Enumeration::IDeviceWatcher* watcher,
      ABI::Windows::Devices::Enumeration::IDeviceInformationUpdate* update);

  void OnPoweredRadiosEnumerated(
      ABI::Windows::Devices::Enumeration::IDeviceWatcher* watcher,
      IInspectable* object);

  void OnAdvertisementReceived(
      ABI::Windows::Devices::Bluetooth::Advertisement::
          IBluetoothLEAdvertisementWatcher* watcher,
      ABI::Windows::Devices::Bluetooth::Advertisement::
          IBluetoothLEAdvertisementReceivedEventArgs* received);

  void OnRegisterAdvertisement(BluetoothAdvertisement* advertisement,
                               const CreateAdvertisementCallback& callback);

  void OnRegisterAdvertisementError(
      BluetoothAdvertisement* advertisement,
      const AdvertisementErrorCallback& error_callback,
      BluetoothAdvertisement::ErrorCode error_code);

  void TryRemoveRadioStateChangedHandler();

  void TryRemovePoweredRadioEventHandlers();

  void RemoveAdvertisementReceivedHandler();

  bool is_initialized_ = false;
  std::string address_;
  std::string name_;
  std::unique_ptr<base::ScopedClosureRunner> on_init_;

  Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::IBluetoothAdapter>
      adapter_;

  Microsoft::WRL::ComPtr<ABI::Windows::Devices::Radios::IRadio> radio_;
  base::Optional<EventRegistrationToken> radio_state_changed_token_;

  Microsoft::WRL::ComPtr<ABI::Windows::Devices::Enumeration::IDeviceWatcher>
      powered_radio_watcher_;
  base::Optional<EventRegistrationToken> powered_radio_added_token_;
  base::Optional<EventRegistrationToken> powered_radio_removed_token_;
  base::Optional<EventRegistrationToken> powered_radios_enumerated_token_;
  size_t num_powered_radios_ = 0;

  std::vector<scoped_refptr<BluetoothAdvertisement>> pending_advertisements_;

  EventRegistrationToken advertisement_received_token_;
  Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::Advertisement::
                             IBluetoothLEAdvertisementWatcher>
      ble_advertisement_watcher_;

  Microsoft::WRL::ComPtr<
      ABI::Windows::Devices::Bluetooth::IBluetoothAdapterStatics>
      bluetooth_adapter_statics_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::Devices::Enumeration::IDeviceInformationStatics>
      device_information_statics_;
  Microsoft::WRL::ComPtr<ABI::Windows::Devices::Radios::IRadioStatics>
      radio_statics_;

  THREAD_CHECKER(thread_checker_);

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterWinrt> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterWinrt);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_WINRT_H_
