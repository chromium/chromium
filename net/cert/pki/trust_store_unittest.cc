// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/pki/trust_store.h"

#include <set>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// Other tests use comparisons on the ToDebugString values as test
// expectations, so confirm that each CertificateTrust of interest has a
// unique string value.
TEST(CertificateTrustTest, ToDebugStringUniqueness) {
  std::vector<CertificateTrust> trust_settings = {
      // Meaningful combinations with trust anchor
      CertificateTrust::ForTrustAnchor(),
      CertificateTrust::ForTrustAnchor().WithEnforceAnchorConstraints(),
      CertificateTrust::ForTrustAnchor().WithEnforceAnchorExpiry(),
      CertificateTrust::ForTrustAnchor().WithRequireAnchorBasicConstraints(),
      CertificateTrust::ForTrustAnchor()
          .WithEnforceAnchorConstraints()
          .WithEnforceAnchorExpiry(),
      CertificateTrust::ForTrustAnchor()
          .WithEnforceAnchorConstraints()
          .WithEnforceAnchorExpiry()
          .WithRequireAnchorBasicConstraints(),

      // Meaningful combinations with trust anchor or leaf
      CertificateTrust::ForTrustAnchorOrLeaf(),

      CertificateTrust::ForTrustAnchorOrLeaf().WithEnforceAnchorConstraints(),
      CertificateTrust::ForTrustAnchorOrLeaf().WithEnforceAnchorExpiry(),
      CertificateTrust::ForTrustAnchorOrLeaf().WithRequireLeafSelfSigned(),

      CertificateTrust::ForTrustAnchorOrLeaf()
          .WithEnforceAnchorConstraints()
          .WithEnforceAnchorExpiry(),
      CertificateTrust::ForTrustAnchorOrLeaf()
          .WithEnforceAnchorConstraints()
          .WithRequireLeafSelfSigned(),
      CertificateTrust::ForTrustAnchorOrLeaf()
          .WithEnforceAnchorExpiry()
          .WithRequireLeafSelfSigned(),

      CertificateTrust::ForTrustAnchorOrLeaf()
          .WithEnforceAnchorConstraints()
          .WithEnforceAnchorExpiry()
          .WithRequireLeafSelfSigned(),

      CertificateTrust::ForTrustAnchorOrLeaf()
          .WithEnforceAnchorConstraints()
          .WithEnforceAnchorExpiry()
          .WithRequireAnchorBasicConstraints()
          .WithRequireLeafSelfSigned(),

      // Meaningful combinations with trusted leaf
      CertificateTrust::ForTrustedLeaf(),
      CertificateTrust::ForTrustedLeaf().WithRequireLeafSelfSigned(),

      CertificateTrust::ForUnspecified(),
      CertificateTrust::ForDistrusted(),
  };
  std::set<std::string> strings;
  for (const auto& trust : trust_settings) {
    strings.insert(trust.ToDebugString());

    absl::optional<CertificateTrust> round_tripped_trust =
        CertificateTrust::FromDebugString(trust.ToDebugString());
    ASSERT_TRUE(round_tripped_trust) << " for " << trust.ToDebugString();
    EXPECT_EQ(trust.ToDebugString(), round_tripped_trust->ToDebugString());
  }
  EXPECT_EQ(strings.size(), trust_settings.size());
}

}  // namespace net
