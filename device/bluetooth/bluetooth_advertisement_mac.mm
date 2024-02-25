// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_advertisement_mac.h"

#include "base/functional/bind.h"
#import "base/task/single_thread_task_runner.h"
#include "device/bluetooth/bluetooth_adapter_mac.h"

namespace device {

BluetoothAdvertisementMac::BluetoothAdvertisementMac(
    std::optional<BluetoothAdvertisement::UUIDList> service_uuids,
    BluetoothAdapter::CreateAdvertisementCallback success_callback,
    BluetoothAdapter::AdvertisementErrorCallback error_callback,
    BluetoothLowEnergyAdvertisementManagerMac* advertisement_manager)
    : service_uuids_(std::move(service_uuids)),
      success_callback_(std::move(success_callback)),
      error_callback_(std::move(error_callback)),
      advertisement_manager_(advertisement_manager),
      status_(BluetoothAdvertisementMac::WAITING_FOR_ADAPTER) {}

void BluetoothAdvertisementMac::Unregister(SuccessCallback success_callback,
                                           ErrorCallback error_callback) {
  if (status_ == Status::UNREGISTERED) {
    std::move(success_callback).Run();
    return;
  }

  status_ = Status::UNREGISTERED;
  advertisement_manager_->UnregisterAdvertisement(
      this, std::move(success_callback), std::move(error_callback));
}

BluetoothAdvertisementMac::~BluetoothAdvertisementMac() {
  // This object should be owned by BluetoothLowEnergyAdvertisementManagerMac,
  // and will be cleaned up there.
}

void BluetoothAdvertisementMac::OnAdvertisementPending() {
  status_ = Status::ADVERTISEMENT_PENDING;
}

void BluetoothAdvertisementMac::OnAdvertisementError(
    base::SingleThreadTaskRunner* task_runner,
    BluetoothAdvertisement::ErrorCode error_code) {
  status_ = Status::ERROR_ADVERTISING;
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(std::move(error_callback_), error_code));
}

void BluetoothAdvertisementMac::OnAdvertisementSuccess(
    base::SingleThreadTaskRunner* task_runner) {
  status_ = Status::ADVERTISING;
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&BluetoothAdvertisementMac::InvokeSuccessCallback, this));
}

void BluetoothAdvertisementMac::OnAdapterReset() {
  status_ = Status::UNREGISTERED;
}

void BluetoothAdvertisementMac::InvokeSuccessCallback() {
  std::move(success_callback_).Run(this);
}

}  // namespace device
