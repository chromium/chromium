// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_H_

#include <memory>

#include "base/compiler_specific.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/public/mojom/test/fake_bluetooth.mojom-forward.h"
#include "device/bluetooth/test/fake_central.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace bluetooth {

// Implementation of FakeBluetooth in
// src/device/bluetooth/public/mojom/test/fake_bluetooth.mojom.
// Implemented on top of the C++ device/bluetooth API, mainly
// device/bluetooth/bluetooth_adapter_factory.h.
//
// Not intended for direct use by clients.  See README.md.
class FakeBluetooth : public mojom::FakeBluetooth {
 public:
  FakeBluetooth();
  ~FakeBluetooth() override;

  static void Create(mojo::PendingReceiver<mojom::FakeBluetooth> receiver);

  void SetLESupported(bool available, SetLESupportedCallback callback) override;
  void SimulateCentral(mojom::CentralState state,
                       SimulateCentralCallback callback) override;
  void AllResponsesConsumed(AllResponsesConsumedCallback callback) override;

 private:
  std::unique_ptr<device::BluetoothAdapterFactory::GlobalValuesForTesting>
      global_factory_values_;
  scoped_refptr<FakeCentral> fake_central_;
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_H_
