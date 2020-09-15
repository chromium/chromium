// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/advertisement.h"

#include "base/bind.h"
#include "base/callback_helpers.h"

namespace bluetooth {

Advertisement::Advertisement(
    scoped_refptr<device::BluetoothAdvertisement> bluetooth_advertisement)
    : bluetooth_advertisement_(std::move(bluetooth_advertisement)) {}

Advertisement::~Advertisement() {
  Unregister(base::DoNothing());
}

void Advertisement::Unregister(UnregisterCallback callback) {
  if (!bluetooth_advertisement_)
    return;

  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  bluetooth_advertisement_->Unregister(
      base::BindOnce(&Advertisement::OnUnregister,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback),
      base::BindOnce(&Advertisement::OnUnregisterError,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback));
}

void Advertisement::OnUnregister(UnregisterCallback callback) {
  bluetooth_advertisement_.reset();
  std::move(callback).Run();
}

void Advertisement::OnUnregisterError(
    UnregisterCallback callback,
    device::BluetoothAdvertisement::ErrorCode error_code) {
  DLOG(ERROR) << "Failed to unregister advertisement, error code: "
              << error_code;
  bluetooth_advertisement_.reset();
  std::move(callback).Run();
}

}  // namespace bluetooth
