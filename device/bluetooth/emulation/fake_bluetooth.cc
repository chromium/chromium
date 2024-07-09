// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/emulation/fake_bluetooth.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/public/mojom/emulation/fake_bluetooth.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace bluetooth {

using device::BluetoothAdapterFactory;

FakeBluetooth::FakeBluetooth()
    : global_factory_values_(
          BluetoothAdapterFactory::Get()->InitGlobalOverrideValues()) {}
FakeBluetooth::~FakeBluetooth() = default;

// static
void FakeBluetooth::Create(
    mojo::PendingReceiver<mojom::FakeBluetooth> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<FakeBluetooth>(),
                              std::move(receiver));
}

void FakeBluetooth::SetLESupported(bool supported,
                                   SetLESupportedCallback callback) {
  global_factory_values_->SetLESupported(supported);
  std::move(callback).Run();
}

void FakeBluetooth::SimulateCentral(mojom::CentralState state,
                                    SimulateCentralCallback callback) {
  mojo::PendingRemote<mojom::FakeCentral> fake_central;
  fake_central_ = base::MakeRefCounted<FakeCentral>(
      state, fake_central.InitWithNewPipeAndPassReceiver());
  device::BluetoothAdapterFactory::SetAdapterForTesting(fake_central_);
  std::move(callback).Run(std::move(fake_central));
}

void FakeBluetooth::AllResponsesConsumed(
    AllResponsesConsumedCallback callback) {
  if (fake_central_) {
    std::move(callback).Run(fake_central_->AllResponsesConsumed());
    return;
  }
  std::move(callback).Run(true);
}

}  // namespace bluetooth
