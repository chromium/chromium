// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_ADVERTISEMENT_H_
#define DEVICE_BLUETOOTH_ADVERTISEMENT_H_

#include "base/memory/scoped_refptr.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"

namespace bluetooth {

// Implementation of Mojo Advertisement in
// device/bluetooth/public/mojom/adapter.mojom.
// Uses the platform abstraction of //device/bluetooth.
// An instance of this class is constructed by Adapter and strongly bound to its
// MessagePipe. When the instance is destroyed, the underlying
// BluetoothAdvertisement is destroyed.
class Advertisement : public mojom::Advertisement {
 public:
  explicit Advertisement(
      scoped_refptr<device::BluetoothAdvertisement> bluetooth_advertisement);
  ~Advertisement() override;
  Advertisement(const Advertisement&) = delete;
  Advertisement& operator=(const Advertisement&) = delete;

  // mojom::Advertisement:
  void Unregister(UnregisterCallback callback) override;

 private:
  void OnUnregister(UnregisterCallback callback);
  void OnUnregisterError(UnregisterCallback callback,
                         device::BluetoothAdvertisement::ErrorCode error_code);

  scoped_refptr<device::BluetoothAdvertisement> bluetooth_advertisement_;

  base::WeakPtrFactory<Advertisement> weak_ptr_factory_{this};
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_ADVERTISEMENT_H_
