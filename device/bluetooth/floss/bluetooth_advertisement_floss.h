// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_ADVERTISEMENT_FLOSS_H_
#define DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_ADVERTISEMENT_FLOSS_H_

#include <memory>

#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/floss/floss_advertiser_client.h"

namespace floss {

class BluetoothAdapterFloss;

// BluetoothAdvertisementFloss represents an BLE advertising set and
// provides methods to start/stop an advertising set and changing its
// parameters. It keeps advertisement data and parameters in the format
// required by Floss.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdvertisementFloss
    : public device::BluetoothAdvertisement {
 public:
  BluetoothAdvertisementFloss(
      std::unique_ptr<device::BluetoothAdvertisement::Data> data,
      const uint16_t interval_ms,
      scoped_refptr<BluetoothAdapterFloss> adapter);

  BluetoothAdvertisementFloss(const BluetoothAdvertisementFloss&) = delete;
  BluetoothAdvertisementFloss& operator=(const BluetoothAdvertisementFloss&) =
      delete;

  // BluetoothAdvertisement overrides:
  void Unregister(SuccessCallback success_callback,
                  ErrorCallback error_callback) override;

  // Starts advertising.
  void Start(
      SuccessCallback success_callback,
      device::BluetoothAdapter::AdvertisementErrorCallback error_callback);

  // Stops advertising.
  void Stop(
      SuccessCallback success_callback,
      device::BluetoothAdapter::AdvertisementErrorCallback error_callback);

  // Changes advertising interval.
  void SetAdvertisingInterval(
      const uint16_t interval_ms,
      SuccessCallback success_callback,
      device::BluetoothAdapter::AdvertisementErrorCallback error_callback);

  const AdvertisingSetParameters& params() const { return params_; }

 protected:
  void OnStartSuccess(SuccessCallback success_callback,
                      FlossAdvertiserClient::AdvertiserId adv_id);

  void OnStopSuccess(SuccessCallback success_callback);

  void OnSetAdvertisingIntervalSuccess(SuccessCallback success_callback);

 private:
  ~BluetoothAdvertisementFloss() override;

  FlossAdvertiserClient::AdvertiserId adv_id_;
  AdvertisingSetParameters params_;
  AdvertiseData adv_data_;
  AdvertiseData scan_rsp_;

  base::WeakPtrFactory<BluetoothAdvertisementFloss> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_ADVERTISEMENT_FLOSS_H_
