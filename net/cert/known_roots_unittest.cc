// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/known_roots.h"

#include <string.h>

#include <algorithm>

#include "net/base/hash_value.h"
#include "net/cert/root_cert_list_generated.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(KnownRootsTest, RootCertDataIsSorted) {
  EXPECT_TRUE(std::is_sorted(
      std::begin(kRootCerts), std::end(kRootCerts),
      [](const RootCertData& lhs, const RootCertData& rhs) {
        return memcmp(lhs.sha256_spki_hash, rhs.sha256_spki_hash, 32) < 0;
      }));
}

TEST(KnownRootsTest, UnknownHashReturnsNotFound) {
  SHA256HashValue empty_hash = {{0}};
  EXPECT_EQ(0, GetNetTrustAnchorHistogramIdForSPKI(HashValue(empty_hash)));
}

TEST(KnownRootsTest, FindsKnownRoot) {
  SHA256HashValue gts_root_r3_hash = {
      {0x41, 0x79, 0xed, 0xd9, 0x81, 0xef, 0x74, 0x74, 0x77, 0xb4, 0x96,
       0x26, 0x40, 0x8a, 0xf4, 0x3d, 0xaa, 0x2c, 0xa7, 0xab, 0x7f, 0x9e,
       0x08, 0x2c, 0x10, 0x60, 0xf8, 0x40, 0x96, 0x77, 0x43, 0x48}};
  EXPECT_EQ(485,
            GetNetTrustAnchorHistogramIdForSPKI(HashValue(gts_root_r3_hash)));
}

}  // namespace

}  // namespace net
