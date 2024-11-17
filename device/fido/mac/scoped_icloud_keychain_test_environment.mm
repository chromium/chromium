// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/scoped_icloud_keychain_test_environment.h"

#include "device/fido/mac/fake_icloud_keychain_sys.h"
#include "device/fido/mac/icloud_keychain_sys.h"

namespace device::fido::icloud_keychain {

ScopedTestEnvironment::ScopedTestEnvironment(
    std::vector<DiscoverableCredentialMetadata> creds) {
  fake_ = base::MakeRefCounted<FakeSystemInterface>();
  fake_->SetCredentials(std::move(creds));
  fake_->set_auth_state(FakeSystemInterface::kAuthAuthorized);
  SetSystemInterfaceForTesting(fake_);
  return;
}

ScopedTestEnvironment::~ScopedTestEnvironment() {
  SetSystemInterfaceForTesting(nullptr);
}

void ScopedTestEnvironment::SetMakeCredentialCallback(
    base::RepeatingCallback<void(const CtapMakeCredentialRequest&)> callback) {
  fake_->SetMakeCredentialCallback(std::move(callback));
}

void ScopedTestEnvironment::SetGetAssertionCallback(
    base::RepeatingCallback<void(const CtapGetAssertionRequest&)> callback) {
  fake_->SetGetAssertionCallback(std::move(callback));
}

}  // namespace device::fido::icloud_keychain
