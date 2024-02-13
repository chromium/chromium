// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_ADVERTISER_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_ADVERTISER_CLIENT_H_

#include "base/logging.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/floss_advertiser_client.h"

namespace floss {

class DEVICE_BLUETOOTH_EXPORT FakeFlossAdvertiserClient
    : public FlossAdvertiserClient {
 public:
  FakeFlossAdvertiserClient();
  ~FakeFlossAdvertiserClient() override;

  // Fake overrides.
  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const int adapter_index,
            base::Version version,
            base::OnceClosure on_ready) override;

  void StartAdvertisingSet(
      const AdvertisingSetParameters& params,
      const AdvertiseData& adv_data,
      const std::optional<AdvertiseData> scan_rsp,
      const std::optional<PeriodicAdvertisingParameters> periodic_params,
      const std::optional<AdvertiseData> periodic_data,
      const int32_t duration,
      const int32_t max_ext_adv_events,
      StartSuccessCallback success_callback,
      ErrorCallback error_callback) override;

  void StopAdvertisingSet(const AdvertiserId adv_id,
                          StopSuccessCallback success_callback,
                          ErrorCallback error_callback) override;

  void SetAdvertisingParameters(const AdvertiserId adv_id,
                                const AdvertisingSetParameters& params,
                                SetAdvParamsSuccessCallback success_callback,
                                ErrorCallback error_callback) override;

 protected:
  friend class BluetoothFlossTest;
  FRIEND_TEST(BluetoothFlossTest, SetAdvertisingInterval);

 private:
  uint32_t start_advertising_set_called_ = 0;
  uint32_t stop_advertising_set_called_ = 0;
  uint32_t set_advertising_parameters_called_ = 0;

  AdvertiserId adv_id_ = 0x70000000;
  base::WeakPtrFactory<FakeFlossAdvertiserClient> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FAKE_FLOSS_ADVERTISER_CLIENT_H_
