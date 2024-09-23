// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_FAKE_ICLOUD_KEYCHAIN_H_
#define DEVICE_FIDO_MAC_FAKE_ICLOUD_KEYCHAIN_H_

#include <memory>
#include <optional>

namespace device::fido::icloud_keychain {

struct Fake {
  virtual ~Fake() {}
};

// Override iCloud Keychain actions with a fake that just reports that the user
// has granted iCloud Keychain permission.
std::unique_ptr<Fake> NewFake();

// Override iCloud Keychain actions with a fake that reports the given
// permission value.
std::unique_ptr<Fake> NewFakeWithPermission(std::optional<bool> permission);

}  // namespace device::fido::icloud_keychain

#endif  // DEVICE_FIDO_MAC_FAKE_ICLOUD_KEYCHAIN_H_
