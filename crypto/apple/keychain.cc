// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/apple/keychain.h"

#include <memory>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "crypto/apple/keychain_secitem.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace crypto::apple {

// static
std::unique_ptr<Keychain> Keychain::DefaultKeychain() {
  return std::make_unique<KeychainSecItem>();
}

Keychain::Keychain() = default;
Keychain::~Keychain() = default;

}  // namespace crypto::apple
