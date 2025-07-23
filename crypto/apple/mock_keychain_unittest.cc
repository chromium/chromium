// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/apple/mock_keychain.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

TEST(MockAppleKeychain, Basic) {
  crypto::apple::MockKeychain keychain;
  const auto password = keychain.GetEncryptionPassword();
  ASSERT_FALSE(password.empty());
}
