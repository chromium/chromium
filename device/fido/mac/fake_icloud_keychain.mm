// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/fake_icloud_keychain.h"

#include "device/fido/mac/fake_icloud_keychain_sys.h"

namespace device::fido::icloud_keychain {

namespace {

struct API_AVAILABLE(macos(13.3)) FakeImpl : public Fake {
 public:
  FakeImpl(std::optional<bool> permission)
      : fake_(base::MakeRefCounted<FakeSystemInterface>()) {
    if (!permission) {
      fake_->set_auth_state(SystemInterface::kAuthNotAuthorized);
    } else if (*permission) {
      fake_->set_auth_state(SystemInterface::kAuthAuthorized);
    } else {
      fake_->set_auth_state(SystemInterface::kAuthDenied);
    }
    SetSystemInterfaceForTesting(fake_);
  }

  ~FakeImpl() override { SetSystemInterfaceForTesting(nullptr); }

  scoped_refptr<FakeSystemInterface> fake_;
};

}  // namespace

std::unique_ptr<Fake> NewFake() {
  return NewFakeWithPermission(true);
}

std::unique_ptr<Fake> NewFakeWithPermission(std::optional<bool> permission) {
  if (@available(macOS 13.5, *)) {
    return std::make_unique<FakeImpl>(permission);
  }
  return nullptr;
}

}  // namespace device::fido::icloud_keychain
