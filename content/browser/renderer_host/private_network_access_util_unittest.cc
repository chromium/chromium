// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <ostream>
#include <tuple>
#include <vector>

#include "content/browser/renderer_host/private_network_access_util.h"

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using AddressSpace = network::mojom::IPAddressSpace;
using Policy = network::mojom::PrivateNetworkRequestPolicy;

using ::testing::ElementsAreArray;

// Self-descriptive constants for `is_web_secure_context`.
constexpr bool kNonSecure = false;
constexpr bool kSecure = true;

// Input arguments to `DerivePrivateNetworkRequestPolicy()`.
struct DerivePolicyInput {
  bool is_web_secure_context;
  AddressSpace address_space;
  PrivateNetworkRequestContext private_network_request_context;

  // Helper for comparison operators.
  std::tuple<bool, AddressSpace, PrivateNetworkRequestContext> ToTuple() const {
    return {is_web_secure_context, address_space,
            private_network_request_context};
  }

  bool operator==(const DerivePolicyInput& other) const {
    return ToTuple() == other.ToTuple();
  }

  // Allows using inputs as keys of a map.
  bool operator<(const DerivePolicyInput& other) const {
    return ToTuple() < other.ToTuple();
  }
};

// For ease of debugging.
std::ostream& operator<<(std::ostream& out, const DerivePolicyInput& input) {
  return out << "{ " << input.address_space << ", "
             << (input.is_web_secure_context ? "secure" : "non-secure") << ", "
             << (input.private_network_request_context ==
                         PrivateNetworkRequestContext::kWorker
                     ? "worker"
                     : "subresource")
             << " }";
}

Policy DerivePolicy(DerivePolicyInput input) {
  return DerivePrivateNetworkRequestPolicy(
      input.address_space, input.is_web_secure_context,
      input.private_network_request_context);
}

// Maps inputs to their default output (all feature flags left untouched).
std::map<DerivePolicyInput, Policy> DefaultPolicyMap() {
  return {
      {{kNonSecure, AddressSpace::kUnknown,
        PrivateNetworkRequestContext::kSubresource},
       Policy::kAllow},
      {{kNonSecure, AddressSpace::kPublic,
        PrivateNetworkRequestContext::kSubresource},
       Policy::kBlock},
      {{kNonSecure, AddressSpace::kPrivate,
        PrivateNetworkRequestContext::kSubresource},
       Policy::kWarn},
      {{kNonSecure, AddressSpace::kLocal,
        PrivateNetworkRequestContext::kSubresource},
       Policy::kBlock},
      {{kSecure, AddressSpace::kUnknown,
        PrivateNetworkRequestContext::kSubresource},
       Policy::kAllow},
      {{kSecure, AddressSpace::kPublic,
        PrivateNetworkRequestContext::kSubresource},
       Policy::kPreflightWarn},
      {{kSecure, AddressSpace::kPrivate,
        PrivateNetworkRequestContext::kSubresource},
       Policy::kPreflightWarn},
      {{kSecure, AddressSpace::kLocal,
        PrivateNetworkRequestContext::kSubresource},
       Policy::kPreflightWarn},
      {{kNonSecure, AddressSpace::kUnknown,
        PrivateNetworkRequestContext::kWorker},
       Policy::kAllow},
      {{kNonSecure, AddressSpace::kPublic,
        PrivateNetworkRequestContext::kWorker},
       Policy::kWarn},
      {{kNonSecure, AddressSpace::kPrivate,
        PrivateNetworkRequestContext::kWorker},
       Policy::kWarn},
      {{kNonSecure, AddressSpace::kLocal,
        PrivateNetworkRequestContext::kWorker},
       Policy::kWarn},
      {{kSecure, AddressSpace::kUnknown, PrivateNetworkRequestContext::kWorker},
       Policy::kAllow},
      {{kSecure, AddressSpace::kPublic, PrivateNetworkRequestContext::kWorker},
       Policy::kPreflightWarn},
      {{kSecure, AddressSpace::kPrivate, PrivateNetworkRequestContext::kWorker},
       Policy::kPreflightWarn},
      {{kSecure, AddressSpace::kLocal, PrivateNetworkRequestContext::kWorker},
       Policy::kPreflightWarn},
  };
}

// Runs `DerivePolicy()` on each key and compares the result to the map value.
void TestPolicyMap(const std::map<DerivePolicyInput, Policy>& expected) {
  for (const auto& [input, policy] : expected) {
    EXPECT_EQ(DerivePolicy(input), policy) << input;
  }
}

TEST(PrivateNetworkAccessUtilTest, DerivePolicy) {
  TestPolicyMap(DefaultPolicyMap());
}

TEST(PrivateNetworkAccessUtilTest, DerivePolicyBlockFromInsecurePrivate) {
  base::test::ScopedFeatureList feature_list(
      features::kBlockInsecurePrivateNetworkRequestsFromPrivate);

  std::map<DerivePolicyInput, Policy> expected = DefaultPolicyMap();
  // Only need to override non-worker case because workers are by default
  // warnings only.
  expected[{kNonSecure, AddressSpace::kPrivate,
            PrivateNetworkRequestContext::kSubresource}] = Policy::kBlock;

  TestPolicyMap(expected);
}

TEST(PrivateNetworkAccessUtilTest, DerivePolicyBlockFromInsecureUnknown) {
  base::test::ScopedFeatureList feature_list(
      features::kBlockInsecurePrivateNetworkRequestsFromUnknown);

  std::map<DerivePolicyInput, Policy> expected = DefaultPolicyMap();
  expected[{kNonSecure, AddressSpace::kUnknown,
            PrivateNetworkRequestContext::kSubresource}] = Policy::kBlock;
  // Workers are currently in warning-only mode.
  expected[{kNonSecure, AddressSpace::kUnknown,
            PrivateNetworkRequestContext::kWorker}] = Policy::kWarn;

  TestPolicyMap(expected);
}

TEST(PrivateNetworkAccessUtilTest, DerivePolicyNoPreflights) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {}, {features::kPrivateNetworkAccessSendPreflights});

  std::map<DerivePolicyInput, Policy> expected = DefaultPolicyMap();
  expected[{kSecure, AddressSpace::kPublic,
            PrivateNetworkRequestContext::kSubresource}] = Policy::kAllow;
  expected[{kSecure, AddressSpace::kPrivate,
            PrivateNetworkRequestContext::kSubresource}] = Policy::kAllow;
  expected[{kSecure, AddressSpace::kLocal,
            PrivateNetworkRequestContext::kSubresource}] = Policy::kAllow;
  expected[{kSecure, AddressSpace::kPublic,
            PrivateNetworkRequestContext::kWorker}] = Policy::kAllow;
  expected[{kSecure, AddressSpace::kPrivate,
            PrivateNetworkRequestContext::kWorker}] = Policy::kAllow;
  expected[{kSecure, AddressSpace::kLocal,
            PrivateNetworkRequestContext::kWorker}] = Policy::kAllow;

  TestPolicyMap(expected);
}

TEST(PrivateNetworkAccessUtilTest, DerivePolicyRespectPreflightResults) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kPrivateNetworkAccessRespectPreflightResults},
      {features::kPrivateNetworkAccessForWorkersWarningOnly});

  std::map<DerivePolicyInput, Policy> expected = DefaultPolicyMap();
  expected[{kSecure, AddressSpace::kPublic,
            PrivateNetworkRequestContext::kSubresource}] =
      Policy::kPreflightBlock;
  expected[{kSecure, AddressSpace::kPrivate,
            PrivateNetworkRequestContext::kSubresource}] =
      Policy::kPreflightBlock;
  expected[{kSecure, AddressSpace::kLocal,
            PrivateNetworkRequestContext::kSubresource}] =
      Policy::kPreflightBlock;
  expected[{kSecure, AddressSpace::kPublic,
            PrivateNetworkRequestContext::kWorker}] = Policy::kPreflightBlock;
  expected[{kSecure, AddressSpace::kPrivate,
            PrivateNetworkRequestContext::kWorker}] = Policy::kPreflightBlock;
  expected[{kSecure, AddressSpace::kLocal,
            PrivateNetworkRequestContext::kWorker}] = Policy::kPreflightBlock;

  // Overriding these because we've disabled warnings only.
  expected[{kNonSecure, AddressSpace::kPublic,
            PrivateNetworkRequestContext::kWorker}] = Policy::kBlock;
  expected[{kNonSecure, AddressSpace::kLocal,
            PrivateNetworkRequestContext::kWorker}] = Policy::kBlock;

  TestPolicyMap(expected);
}

TEST(PrivateNetworkAccessUtilTest, DerivePolicyWarningOnlyForWorkers) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kPrivateNetworkAccessForWorkersWarningOnly,
       features::kPrivateNetworkAccessRespectPreflightResults},
      {});

  std::map<DerivePolicyInput, Policy> expected = DefaultPolicyMap();
  // Only need to override non-worker case because workers are by default
  // warnings only.
  expected[{kSecure, AddressSpace::kPublic,
            PrivateNetworkRequestContext::kSubresource}] =
      Policy::kPreflightBlock;
  expected[{kSecure, AddressSpace::kPrivate,
            PrivateNetworkRequestContext::kSubresource}] =
      Policy::kPreflightBlock;
  expected[{kSecure, AddressSpace::kLocal,
            PrivateNetworkRequestContext::kSubresource}] =
      Policy::kPreflightBlock;

  TestPolicyMap(expected);
}

TEST(PrivateNetworkAccessUtilTest, DerivePolicyDisableWebSecurity) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableWebSecurity);

  for (const auto& [input, policy] : DefaultPolicyMap()) {
    // Ignore the value, we only use the map as a list of keys.
    EXPECT_EQ(DerivePolicy(input), Policy::kAllow) << input;
  }
}

}  // namespace
}  // namespace content
