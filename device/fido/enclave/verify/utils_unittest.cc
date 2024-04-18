// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/verify/utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace device::enclave {

constexpr char kTestPem[] =
    "-----BEGIN PUBLIC KEY-----\n"
    "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEbwRcQY2YvUhp8QpjBDjDYtgrjAWJ\n"
    "a/ewUKu7URKVMbziN8Idzu7neKcvf2QKPkYXReply6fOufdXZJ+SPVqXBg==\n"
    "-----END PUBLIC KEY-----";

TEST(UtilsTest, LooksLikePem_WithValidPem_ReturnsTrue) {
  EXPECT_TRUE(device::enclave::LooksLikePem(kTestPem));
}

TEST(UtilsTest, LooksLikePem_WithInvalidPem_ReturnsFalse) {
  EXPECT_FALSE(device::enclave::LooksLikePem("This should return false"));
}

}  // namespace device::enclave
