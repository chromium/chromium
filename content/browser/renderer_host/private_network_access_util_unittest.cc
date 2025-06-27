// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/private_network_access_util.h"

#include <array>
#include <ostream>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "content/common/features.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using AddressSpace = network::mojom::IPAddressSpace;
using RequestContext = PrivateNetworkRequestContext;
using Policy = network::mojom::PrivateNetworkRequestPolicy;

using ::testing::ElementsAreArray;

// Self-descriptive constants for `is_web_secure_context`.
constexpr bool kNonSecure = false;
constexpr bool kSecure = true;

// Input arguments to `DerivePrivateNetworkRequestPolicy()`.
struct DerivePolicyInput {
  bool is_web_secure_context;
  AddressSpace address_space;
  RequestContext request_context;

  // Helper for comparison operators.
  std::tuple<bool, AddressSpace, RequestContext> ToTuple() const {
    return {is_web_secure_context, address_space, request_context};
  }

  bool operator==(const DerivePolicyInput& other) const {
    return ToTuple() == other.ToTuple();
  }

  // Allows using inputs as keys of a map.
  bool operator<(const DerivePolicyInput& other) const {
    return ToTuple() < other.ToTuple();
  }
};

std::string_view RequestContextToStringPiece(RequestContext request_context) {
  switch (request_context) {
    case RequestContext::kSubresource:
      return "subresource";
    case RequestContext::kNavigation:
      return "navigation";
    case RequestContext::kWorker:
      return "worker";
  }
}

// For ease of debugging.
std::ostream& operator<<(std::ostream& out, const DerivePolicyInput& input) {
  return out << "{ " << input.address_space << ", "
             << (input.is_web_secure_context ? "secure" : "non-secure") << ", "
             << RequestContextToStringPiece(input.request_context) << " }";
}

Policy DerivePolicy(DerivePolicyInput input) {
  return DerivePrivateNetworkRequestPolicy(
      input.address_space, input.is_web_secure_context, input.request_context);
}

// Maps inputs to their default output (all feature flags left untouched).
std::map<DerivePolicyInput, Policy> DefaultPolicyMap() {
  return {
      //
      // `RequestContext::kSubresource`
      //
      {
          {kNonSecure, AddressSpace::kUnknown, RequestContext::kSubresource},
          Policy::kAllow,
      },
      {
          {kNonSecure, AddressSpace::kPublic, RequestContext::kSubresource},
          Policy::kWarn,
      },
      {
          {kNonSecure, AddressSpace::kPrivate, RequestContext::kSubresource},
          Policy::kWarn,
      },
      {
          {kNonSecure, AddressSpace::kLoopback, RequestContext::kSubresource},
          Policy::kWarn,
      },
      {
          {kSecure, AddressSpace::kUnknown, RequestContext::kSubresource},
          Policy::kAllow,
      },
      {
          {kSecure, AddressSpace::kPublic, RequestContext::kSubresource},
          Policy::kAllow,
      },
      {
          {kSecure, AddressSpace::kPrivate, RequestContext::kSubresource},
          Policy::kAllow,
      },
      {
          {kSecure, AddressSpace::kLoopback, RequestContext::kSubresource},
          Policy::kAllow,
      },
      //
      // `RequestContext::kWorker`
      //
      {
          {kNonSecure, AddressSpace::kUnknown, RequestContext::kWorker},
          Policy::kAllow,
      },
      {
          {kNonSecure, AddressSpace::kPublic, RequestContext::kWorker},
          Policy::kWarn,
      },
      {
          {kNonSecure, AddressSpace::kPrivate, RequestContext::kWorker},
          Policy::kWarn,
      },
      {
          {kNonSecure, AddressSpace::kLoopback, RequestContext::kWorker},
          Policy::kWarn,
      },
      {
          {kSecure, AddressSpace::kUnknown, RequestContext::kWorker},
          Policy::kAllow,
      },
      {
          {kSecure, AddressSpace::kPublic, RequestContext::kWorker},
          Policy::kAllow,
      },
      {
          {kSecure, AddressSpace::kPrivate, RequestContext::kWorker},
          Policy::kAllow,
      },
      {
          {kSecure, AddressSpace::kLoopback, RequestContext::kWorker},
          Policy::kAllow,
      },
      //
      // `RequestContext::kNavigation`
      //
      {
          {kNonSecure, AddressSpace::kUnknown, RequestContext::kNavigation},
          Policy::kAllow,
      },
      {
          {kNonSecure, AddressSpace::kPublic, RequestContext::kNavigation},
          Policy::kAllow,
      },
      {
          {kNonSecure, AddressSpace::kPrivate, RequestContext::kNavigation},
          Policy::kAllow,
      },
      {
          {kNonSecure, AddressSpace::kLoopback, RequestContext::kNavigation},
          Policy::kAllow,
      },
      {
          {kSecure, AddressSpace::kUnknown, RequestContext::kNavigation},
          Policy::kAllow,
      },
      {
          {kSecure, AddressSpace::kPublic, RequestContext::kNavigation},
          Policy::kAllow,
      },
      {
          {kSecure, AddressSpace::kPrivate, RequestContext::kNavigation},
          Policy::kAllow,
      },
      {
          {kSecure, AddressSpace::kLoopback, RequestContext::kNavigation},
          Policy::kAllow,
      },
  };
}

std::map<DerivePolicyInput, Policy> AllAllowMap() {
  std::map<DerivePolicyInput, Policy> result = DefaultPolicyMap();
  for (auto& entry : result) {
    entry.second = Policy::kAllow;
  }
  return result;
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

TEST(PrivateNetworkAccessUtilTest, DerivePolicyDisableWebSecurity) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableWebSecurity);

  TestPolicyMap(AllAllowMap());
}

TEST(PrivateNetworkAccessUtilTest, DerivePolicyLocalNetworkAccess) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["LocalNetworkAccessChecksWarn"] = "false";
  feature_list.InitAndEnableFeatureWithParameters(
      network::features::kLocalNetworkAccessChecks, params);

  std::map<DerivePolicyInput, Policy> expected = DefaultPolicyMap();
  for (auto& entry : expected) {
    entry.second = entry.first.is_web_secure_context ? Policy::kPermissionBlock
                                                     : Policy::kBlock;
  }
  TestPolicyMap(expected);
}

TEST(PrivateNetworkAccessUtilTest, DerivePolicyLocalNetworkAccessWarn) {
  base::test::ScopedFeatureList feature_list(
      network::features::kLocalNetworkAccessChecks);

  std::map<DerivePolicyInput, Policy> expected = DefaultPolicyMap();
  for (auto& entry : expected) {
    entry.second = Policy::kPermissionWarn;
  }
  TestPolicyMap(expected);
}

}  // namespace
}  // namespace content
