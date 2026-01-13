// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/apple/scoped_fake_keychain_v2.h"

#include <MacTypes.h>

#include <memory>
#include <string>

#include "crypto/apple/fake_keychain_v2.h"

namespace crypto::apple {

ScopedFakeKeychainV2::ScopedFakeKeychainV2(
    const std::string& keychain_access_group)
    : keychain_(std::make_unique<FakeKeychainV2>(keychain_access_group)) {
  KeychainV2::SetInstanceOverride(keychain_.get());
}

ScopedFakeKeychainV2::~ScopedFakeKeychainV2() {
  KeychainV2::ClearInstanceOverride();
}

void ScopedFakeKeychainV2::SetUVMethod(FakeKeychainV2::UVMethod uv_method) {
  keychain_->set_uv_method(uv_method);
}

void ScopedFakeKeychainV2::SetFindGenericResult(OSStatus result) {
  keychain_->set_find_generic_result(result);
}

bool ScopedFakeKeychainV2::called_add_generic() {
  return keychain_->called_add_generic();
}

std::string ScopedFakeKeychainV2::GetEncryptionPassword() {
  return keychain_->GetEncryptionPassword();
}

}  // namespace crypto::apple
