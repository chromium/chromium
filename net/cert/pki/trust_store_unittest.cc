// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/pki/trust_store.h"

#include <set>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

constexpr CertificateTrust TrustAnchorEnforcingExpirationAndConstraints() {
  CertificateTrust result;
  result.type = CertificateTrustType::TRUSTED_ANCHOR;
  result.enforce_anchor_constraints = true;
  result.enforce_anchor_expiry = true;
  return result;
}

// Other tests use comparisons on the ToDebugString values as test
// expectations, so confirm that each CertificateTrust of interest has a
// unique string value.
TEST(CertificateTrustTest, ToDebugStringUniqueness) {
  std::vector<CertificateTrust> trust_settings = {
      CertificateTrust::ForTrustAnchor(),
      CertificateTrust::ForTrustAnchorEnforcingConstraints(),
      CertificateTrust::ForTrustAnchorEnforcingExpiration(),
      TrustAnchorEnforcingExpirationAndConstraints(),
      CertificateTrust::ForUnspecified(),
      CertificateTrust::ForDistrusted(),
  };
  std::set<std::string> strings;
  for (const auto& trust : trust_settings) {
    strings.insert(trust.ToDebugString());
  }
  EXPECT_EQ(strings.size(), trust_settings.size());
}

}  // namespace net
