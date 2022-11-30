// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_SCOPED_NFC_OVERRIDER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_SCOPED_NFC_OVERRIDER_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/nfc_provider.mojom.h"

namespace device {

// A helper class which overrides NFC instance for testing. Note that for this
// override to work properly, it must be constructed in the same process that
// runs the Device Service implementation.
class ScopedNFCOverrider {
 public:
  ScopedNFCOverrider();
  ~ScopedNFCOverrider();
  ScopedNFCOverrider(const ScopedNFCOverrider&) = delete;
  ScopedNFCOverrider& operator=(const ScopedNFCOverrider&) = delete;

  // Return whether the mojo connection of NFC instance is connected.
  bool IsConnected();

 private:
  class FakeNFCProvider;

  void BindFakeNFCProvider(mojo::PendingReceiver<mojom::NFCProvider> receiver);

  std::unique_ptr<FakeNFCProvider> fake_nfc_provider_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_SCOPED_NFC_OVERRIDER_H_
