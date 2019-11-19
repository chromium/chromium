// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADVERTISEMENT_WINRT_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADVERTISEMENT_WINRT_H_

#include <windows.devices.bluetooth.advertisement.h>
#include <wrl/client.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/bluetooth_export.h"

namespace device {

class DEVICE_BLUETOOTH_EXPORT BluetoothAdvertisementWinrt
    : public BluetoothAdvertisement {
 public:
  BluetoothAdvertisementWinrt();
  bool Initialize(
      std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data);
  void Register(SuccessCallback callback, ErrorCallback error_callback);

  // BluetoothAdvertisement:
  void Unregister(const SuccessCallback& success_callback,
                  const ErrorCallback& error_callback) override;

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
  base::Optional<EventRegistrationToken> status_changed_token_;
  std::unique_ptr<PendingCallbacks> pending_register_callbacks_;
  std::unique_ptr<PendingCallbacks> pending_unregister_callbacks_;

  base::WeakPtrFactory<BluetoothAdvertisementWinrt> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdvertisementWinrt);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADVERTISEMENT_WINRT_H_
