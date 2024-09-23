// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/scoped_fake_apple_keychain_v2.h"

#include <memory>

#include "crypto/fake_apple_keychain_v2.h"

namespace crypto {

ScopedFakeAppleKeychainV2::ScopedFakeAppleKeychainV2(
    const std::string& keychain_access_group)
    : keychain_(std::make_unique<FakeAppleKeychainV2>(keychain_access_group)) {
  AppleKeychainV2::SetInstanceOverride(keychain_.get());
}

ScopedFakeAppleKeychainV2::~ScopedFakeAppleKeychainV2() {
  AppleKeychainV2::ClearInstanceOverride();
}

void ScopedFakeAppleKeychainV2::SetUVMethod(UVMethod uv_method) {
  keychain_->set_uv_method(uv_method);
}

}  // namespace crypto
