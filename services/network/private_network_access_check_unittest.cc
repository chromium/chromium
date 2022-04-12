// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/private_network_access_check.h"

#include "base/strings/string_piece.h"
#include "base/test/metrics/histogram_tester.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

using Result = PrivateNetworkAccessCheckResult;

constexpr base::StringPiece kHistogramName =
    "Security.PrivateNetworkAccess.CheckResult";

TEST(PrivateNetworkAccessCheckTest, BlockedByLoadOption) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(PrivateNetworkAccessCheck(nullptr, mojom::IPAddressSpace::kUnknown,
                                      absl::nullopt,
                                      mojom::kURLLoadOptionBlockLocalRequest,
                                      mojom::IPAddressSpace::kPrivate),
            Result::kBlockedByLoadOption);

  histogram_tester.ExpectUniqueSample(kHistogramName,
                                      Result::kBlockedByLoadOption, 1);
}

TEST(PrivateNetworkAccessCheckTest, AllowedMissingClientSecurityState) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(PrivateNetworkAccessCheck(nullptr, mojom::IPAddressSpace::kUnknown,
                                      absl::nullopt, mojom::kURLLoadOptionNone,
                                      mojom::IPAddressSpace::kLocal),
            Result::kAllowedMissingClientSecurityState);

  histogram_tester.ExpectUniqueSample(
      kHistogramName, Result::kAllowedMissingClientSecurityState, 1);
}

TEST(PrivateNetworkAccessCheckTest, AllowedNoLessPublic) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState state;
  state.ip_address_space = mojom::IPAddressSpace::kPrivate;

  EXPECT_EQ(PrivateNetworkAccessCheck(&state, mojom::IPAddressSpace::kUnknown,
                                      absl::nullopt, mojom::kURLLoadOptionNone,
                                      mojom::IPAddressSpace::kPrivate),
            Result::kAllowedNoLessPublic);

  histogram_tester.ExpectUniqueSample(kHistogramName,
                                      Result::kAllowedNoLessPublic, 1);
}

TEST(PrivateNetworkAccessCheckTest, AllowedByPolicyAllow) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState state;
  state.ip_address_space = mojom::IPAddressSpace::kPublic;
  state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kAllow;

  EXPECT_EQ(PrivateNetworkAccessCheck(&state, mojom::IPAddressSpace::kUnknown,
                                      absl::nullopt, mojom::kURLLoadOptionNone,
                                      mojom::IPAddressSpace::kPrivate),
            Result::kAllowedByPolicyAllow);

  histogram_tester.ExpectUniqueSample(kHistogramName,
                                      Result::kAllowedByPolicyAllow, 1);
}

TEST(PrivateNetworkAccessCheckTest, AllowedByPolicyWarn) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState state;
  state.ip_address_space = mojom::IPAddressSpace::kPublic;
  state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kWarn;

  EXPECT_EQ(PrivateNetworkAccessCheck(&state, mojom::IPAddressSpace::kUnknown,
                                      absl::nullopt, mojom::kURLLoadOptionNone,
                                      mojom::IPAddressSpace::kPrivate),
            Result::kAllowedByPolicyWarn);

  histogram_tester.ExpectUniqueSample(kHistogramName,
                                      Result::kAllowedByPolicyWarn, 1);
}

TEST(PrivateNetworkAccessCheckTest, BlockedByPolicyBlock) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState state;
  state.ip_address_space = mojom::IPAddressSpace::kPublic;
  state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kBlock;

  EXPECT_EQ(PrivateNetworkAccessCheck(&state, mojom::IPAddressSpace::kUnknown,
                                      absl::nullopt, mojom::kURLLoadOptionNone,
                                      mojom::IPAddressSpace::kPrivate),
            Result::kBlockedByPolicyBlock);

  histogram_tester.ExpectUniqueSample(kHistogramName,
                                      Result::kBlockedByPolicyBlock, 1);
}

TEST(PrivateNetworkAccessCheckTest, BlockedByTargetIpAddressSpace) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(PrivateNetworkAccessCheck(nullptr, mojom::IPAddressSpace::kPublic,
                                      absl::nullopt, mojom::kURLLoadOptionNone,
                                      mojom::IPAddressSpace::kPrivate),
            Result::kBlockedByTargetIpAddressSpace);

  histogram_tester.ExpectUniqueSample(
      kHistogramName, Result::kBlockedByTargetIpAddressSpace, 1);
}

TEST(PrivateNetworkAccessCheckTest, AllowedByPolicyPreflightWarn) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState state;
  state.ip_address_space = mojom::IPAddressSpace::kPublic;
  state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightWarn;

  EXPECT_EQ(PrivateNetworkAccessCheck(&state, mojom::IPAddressSpace::kPublic,
                                      absl::nullopt, mojom::kURLLoadOptionNone,
                                      mojom::IPAddressSpace::kPrivate),
            Result::kAllowedByPolicyPreflightWarn);

  histogram_tester.ExpectUniqueSample(kHistogramName,
                                      Result::kAllowedByPolicyPreflightWarn, 1);
}

TEST(PrivateNetworkAccessCheckTest, AllowedByTargetIpAddressSpace) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(PrivateNetworkAccessCheck(nullptr, mojom::IPAddressSpace::kPrivate,
                                      absl::nullopt, mojom::kURLLoadOptionNone,
                                      mojom::IPAddressSpace::kPrivate),
            Result::kAllowedByTargetIpAddressSpace);

  histogram_tester.ExpectUniqueSample(
      kHistogramName, Result::kAllowedByTargetIpAddressSpace, 1);
}

TEST(PrivateNetworkAccessCheckTest, AllowedByPolicyPreflightWarnInconsistent) {
  base::HistogramTester histogram_tester;

  mojom::ClientSecurityState state;
  state.ip_address_space = mojom::IPAddressSpace::kPublic;
  state.private_network_request_policy =
      mojom::PrivateNetworkRequestPolicy::kPreflightWarn;

  EXPECT_EQ(PrivateNetworkAccessCheck(&state, mojom::IPAddressSpace::kUnknown,
                                      mojom::IPAddressSpace::kPublic,
                                      mojom::kURLLoadOptionNone,
                                      mojom::IPAddressSpace::kPrivate),
            Result::kAllowedByPolicyPreflightWarn);

  histogram_tester.ExpectUniqueSample(kHistogramName,
                                      Result::kAllowedByPolicyPreflightWarn, 1);
}

TEST(PrivateNetworkAccessCheckTest, BlockedByInconsistentIpAddressSpace) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(PrivateNetworkAccessCheck(nullptr, mojom::IPAddressSpace::kUnknown,
                                      mojom::IPAddressSpace::kPublic,
                                      mojom::kURLLoadOptionNone,
                                      mojom::IPAddressSpace::kPrivate),
            Result::kBlockedByInconsistentIpAddressSpace);

  histogram_tester.ExpectUniqueSample(
      kHistogramName, Result::kBlockedByInconsistentIpAddressSpace, 1);
}

}  // namespace
}  // namespace network
