// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADVERTISEMENT_WINRT_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADVERTISEMENT_WINRT_H_

#include <windows.devices.bluetooth.advertisement.h>
#include <wrl/client.h>

#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/bluetooth_export.h"

namespace device {

class DEVICE_BLUETOOTH_EXPORT BluetoothAdvertisementWinrt
    : public BluetoothAdvertisement {
 public:
  BluetoothAdvertisementWinrt();

  BluetoothAdvertisementWinrt(const BluetoothAdvertisementWinrt&) = delete;
  BluetoothAdvertisementWinrt& operator=(const BluetoothAdvertisementWinrt&) =
      delete;

  bool Initialize(
      std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data);
  void Register(SuccessCallback callback, ErrorCallback error_callback);

  // BluetoothAdvertisement:
  void Unregister(SuccessCallback success_callback,
                  ErrorCallback error_callback) override;

  ABI::Windows::Devices::Bluetooth::Advertisement::
      IBluetoothLEAdvertisementPublisher*
      GetPublisherForTesting();

 protected:
  ~BluetoothAdvertisementWinrt() override;

  // These are declared virtual so that they can be overridden by tests.
  virtual HRESULT GetBluetoothLEAdvertisementPublisherActivationFactory(
      ABI::Windows::Devices::Bluetooth::Advertisement::
          IBluetoothLEAdvertisementPublisherFactory** factory) const;

  virtual HRESULT ActivateBluetoothLEAdvertisementInstance(
      ABI::Windows::Devices::Bluetooth::Advertisement::
          IBluetoothLEAdvertisement** instance) const;

  virtual HRESULT GetBluetoothLEManufacturerDataFactory(
      ABI::Windows::Devices::Bluetooth::Advertisement::
          IBluetoothLEManufacturerDataFactory** factory) const;

 private:
  struct PendingCallbacks {
    PendingCallbacks(SuccessCallback callback, ErrorCallback error_callback);
    ~PendingCallbacks();

    SuccessCallback callback;
    ErrorCallback error_callback;
  };

  void OnStatusChanged(
      ABI::Windows::Devices::Bluetooth::Advertisement::
          IBluetoothLEAdvertisementPublisher* publisher,
      ABI::Windows::Devices::Bluetooth::Advertisement::
          IBluetoothLEAdvertisementPublisherStatusChangedEventArgs* changed);

  Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::Advertisement::
                             IBluetoothLEAdvertisementPublisher>
      publisher_;
  std::optional<EventRegistrationToken> status_changed_token_;
  std::unique_ptr<PendingCallbacks> pending_register_callbacks_;
  std::unique_ptr<PendingCallbacks> pending_unregister_callbacks_;

  base::WeakPtrFactory<BluetoothAdvertisementWinrt> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADVERTISEMENT_WINRT_H_
