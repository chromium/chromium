// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/fake_floss_advertiser_client.h"

#include "base/logging.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "device/bluetooth/floss/floss_dbus_client.h"

namespace floss {

FakeFlossAdvertiserClient::FakeFlossAdvertiserClient() = default;

FakeFlossAdvertiserClient::~FakeFlossAdvertiserClient() = default;

void FakeFlossAdvertiserClient::Init(dbus::Bus* bus,
                                     const std::string& service_name,
                                     const int adapter_index,
                                     base::Version version,
                                     base::OnceClosure on_ready) {
  version_ = version;
  std::move(on_ready).Run();
}

void FakeFlossAdvertiserClient::StartAdvertisingSet(
    const AdvertisingSetParameters& params,
    const AdvertiseData& adv_data,
    const std::optional<AdvertiseData> scan_rsp,
    const std::optional<PeriodicAdvertisingParameters> periodic_params,
    const std::optional<AdvertiseData> periodic_data,
    const int32_t duration,
    const int32_t max_ext_adv_events,
    StartSuccessCallback success_callback,
    ErrorCallback error_callback) {
  start_advertising_set_called_++;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(success_callback), adv_id_++));
}

void FakeFlossAdvertiserClient::StopAdvertisingSet(
    const AdvertiserId adv_id,
    StopSuccessCallback success_callback,
    ErrorCallback error_callback) {
  stop_advertising_set_called_++;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(success_callback));
}

void FakeFlossAdvertiserClient::SetAdvertisingParameters(
    const AdvertiserId adv_id,
    const AdvertisingSetParameters& params,
    SetAdvParamsSuccessCallback success_callback,
    ErrorCallback error_callback) {
  set_advertising_parameters_called_++;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(success_callback));
}

}  // namespace floss
