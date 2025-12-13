// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/apple/scoped_fake_keychain_v2.h"

#include <memory>

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

void ScopedFakeKeychainV2::SetUVMethod(UVMethod uv_method) {
  keychain_->set_uv_method(uv_method);
}

}  // namespace crypto::apple
