// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/cert/symantec_certs.h"

#include <algorithm>

#include "net/base/hash_value.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// Tests that IsLegacySymantecCert() returns false for non-Symantec hash values.
TEST(SymantecCertsTest, IsUnrelatedCertSymantecLegacyCert) {
  SHA256HashValue unrelated_hash_value = {{0x01, 0x02}};
  HashValueVector unrelated_hashes;
  unrelated_hashes.push_back(HashValue(unrelated_hash_value));
  EXPECT_FALSE(IsLegacySymantecCert(unrelated_hashes));
}

// Tests that IsLegacySymantecCert() works correctly for excluded and
// non-excluded Symantec roots.
TEST(SymantecCertsTest, IsLegacySymantecCert) {
  SHA256HashValue symantec_hash_value = {
      {0xb2, 0xde, 0xf5, 0x36, 0x2a, 0xd3, 0xfa, 0xcd, 0x04, 0xbd, 0x29,
       0x04, 0x7a, 0x43, 0x84, 0x4f, 0x76, 0x70, 0x34, 0xea, 0x48, 0x92,
       0xf8, 0x0e, 0x56, 0xbe, 0xe6, 0x90, 0x24, 0x3e, 0x25, 0x02}};
  SHA256HashValue google_hash_value = {
      {0xec, 0x72, 0x29, 0x69, 0xcb, 0x64, 0x20, 0x0a, 0xb6, 0x63, 0x8f,
       0x68, 0xac, 0x53, 0x8e, 0x40, 0xab, 0xab, 0x5b, 0x19, 0xa6, 0x48,
       0x56, 0x61, 0x04, 0x2a, 0x10, 0x61, 0xc4, 0x61, 0x27, 0x76}};

  // Test that IsLegacySymantecCert returns true for a Symantec root.
  HashValueVector hashes;
  hashes.push_back(HashValue(symantec_hash_value));
  EXPECT_TRUE(IsLegacySymantecCert(hashes));

  // ... but false when the chain includes a root on the exceptions list.
  hashes.push_back(HashValue(google_hash_value));
  EXPECT_FALSE(IsLegacySymantecCert(hashes));
}

TEST(SymantecCertsTest, AreSortedArrays) {
  ASSERT_TRUE(
      std::is_sorted(kSymantecRoots, kSymantecRoots + kSymantecRootsLength));
  ASSERT_TRUE(std::is_sorted(kSymantecExceptions,
                             kSymantecExceptions + kSymantecExceptionsLength));
  ASSERT_TRUE(std::is_sorted(kSymantecManagedCAs,
                             kSymantecManagedCAs + kSymantecManagedCAsLength));
}

}  // namespace net
