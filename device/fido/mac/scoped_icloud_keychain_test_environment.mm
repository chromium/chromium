// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/scoped_icloud_keychain_test_environment.h"

#include "device/fido/mac/fake_icloud_keychain_sys.h"
#include "device/fido/mac/icloud_keychain_sys.h"

namespace device::fido::icloud_keychain {

ScopedTestEnvironment::ScopedTestEnvironment(
    std::vector<DiscoverableCredentialMetadata> creds) {
  auto fake = base::MakeRefCounted<FakeSystemInterface>();
  fake->SetCredentials(std::move(creds));
  fake->set_auth_state(FakeSystemInterface::kAuthAuthorized);
  SetSystemInterfaceForTesting(fake);
  return;
}

ScopedTestEnvironment::~ScopedTestEnvironment() {
  SetSystemInterfaceForTesting(nullptr);
}

}  // namespace device::fido::icloud_keychain
