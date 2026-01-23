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

// Self-descriptive constants for `allow_on_non_secure_context`.
constexpr bool kDisallowNonSecure = false;
constexpr bool kAllowNonSecure = true;

// Input arguments to `DerivePrivateNetworkRequestPolicy()`.
struct DerivePolicyInput {
  bool is_web_secure_context;
  bool allow_on_non_secure_context;
  AddressSpace address_space;
  RequestContext request_context;
};

std::string_view RequestContextToStringPiece(RequestContext request_context) {
  switch (request_context) {
    case RequestContext::kSubresource:
      return "subresource";
    case RequestContext::kWorker:
      return "worker";
    case RequestContext::kMainFrameNavigation:
      return "main-frame-navigation";
    case RequestContext::kSubframeNavigation:
      return "subframe-navigation";
    case RequestContext::kFencedFrameNavigation:
      return "fenced-frame-navigation";
  }
}

// For ease of debugging.
std::ostream& operator<<(std::ostream& out, const DerivePolicyInput& input) {
  return out << "{ " << input.address_space << ", "
             << (input.is_web_secure_context ? "secure" : "non-secure") << ", "
             << (input.allow_on_non_secure_context ? "allow-non-secure"
                                                   : "disallow-non-secure")
             << ", " << RequestContextToStringPiece(input.request_context)
             << " }";
}

Policy DerivePolicy(DerivePolicyInput input) {
  return DerivePrivateNetworkRequestPolicy(
      input.address_space, input.is_web_secure_context,
      input.allow_on_non_secure_context, input.request_context);
}

// Maps inputs to their default output (all feature flags left untouched).
// NOTE: This is a vector of pairs so that iteration occurs in the same order
// as construction to allow easier maintenance (and we only ever iterate through
// all entries in the tests.)
std::vector<std::pair<DerivePolicyInput, Policy>> DefaultPolicyMap() {
  return {
      //
      // `RequestContext::kSubresource`
      //
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kUnknown,
           RequestContext::kSubresource},
          Policy::kAllow,
      },
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kPublic,
           RequestContext::kSubresource},
          Policy::kWarn,
      },
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kLocal,
           RequestContext::kSubresource},
          Policy::kWarn,
      },
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kLoopback,
           RequestContext::kSubresource},
          Policy::kWarn,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kUnknown,
           RequestContext::kSubresource},
          Policy::kAllow,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kPublic,
           RequestContext::kSubresource},
          Policy::kWarn,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kLocal,
           RequestContext::kSubresource},
          Policy::kWarn,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kLoopback,
           RequestContext::kSubresource},
          Policy::kWarn,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kUnknown,
           RequestContext::kSubresource},
          Policy::kAllow,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kPublic,
           RequestContext::kSubresource},
          Policy::kAllow,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kLocal,
           RequestContext::kSubresource},
          Policy::kAllow,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kLoopback,
           RequestContext::kSubresource},
          Policy::kAllow,
      },
      //
      // `RequestContext::kWorker`
      //
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kUnknown,
           RequestContext::kWorker},
          Policy::kAllow,
      },
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kPublic,
           RequestContext::kWorker},
          Policy::kWarn,
      },
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kLocal,
           RequestContext::kWorker},
          Policy::kWarn,
      },
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kLoopback,
           RequestContext::kWorker},
          Policy::kWarn,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kUnknown,
           RequestContext::kWorker},
          Policy::kAllow,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kPublic,
           RequestContext::kWorker},
          Policy::kWarn,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kLocal,
           RequestContext::kWorker},
          Policy::kWarn,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kLoopback,
           RequestContext::kWorker},
          Policy::kWarn,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kUnknown,
           RequestContext::kWorker},
          Policy::kAllow,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kPublic,
           RequestContext::kWorker},
          Policy::kAllow,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kLocal,
           RequestContext::kWorker},
          Policy::kAllow,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kLoopback,
           RequestContext::kWorker},
          Policy::kAllow,
      },
      //
      // `RequestContext::kMainFrameNavigation`
      //
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kUnknown,
           RequestContext::kMainFrameNavigation},
          Policy::kAllow,
      },
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kPublic,
           RequestContext::kMainFrameNavigation},
          Policy::kAllow,
      },
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kLocal,
           RequestContext::kMainFrameNavigation},
          Policy::kAllow,
      },
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kLoopback,
           RequestContext::kMainFrameNavigation},
          Policy::kAllow,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kUnknown,
           RequestContext::kMainFrameNavigation},
          Policy::kAllow,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kPublic,
           RequestContext::kMainFrameNavigation},
          Policy::kAllow,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kLocal,
           RequestContext::kMainFrameNavigation},
          Policy::kAllow,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kLoopback,
           RequestContext::kMainFrameNavigation},
          Policy::kAllow,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kUnknown,
           RequestContext::kMainFrameNavigation},
          Policy::kAllow,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kPublic,
           RequestContext::kMainFrameNavigation},
          Policy::kAllow,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kLocal,
           RequestContext::kMainFrameNavigation},
          Policy::kAllow,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kLoopback,
           RequestContext::kMainFrameNavigation},
          Policy::kAllow,
      },
  };
}

// Maps inputs to their output when LocalNetworkAccessChecks is enabled.
std::vector<std::pair<DerivePolicyInput, Policy>> LNAPolicyMap() {
  return {
      //
      // `RequestContext::kSubresource`
      //
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kUnknown,
           RequestContext::kSubresource},
          Policy::kBlock,
      },
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kPublic,
           RequestContext::kSubresource},
          Policy::kBlock,
      },
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kLocal,
           RequestContext::kSubresource},
          Policy::kBlock,
      },
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kLoopback,
           RequestContext::kSubresource},
          Policy::kBlock,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kUnknown,
           RequestContext::kSubresource},
          Policy::kPermissionBlock,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kPublic,
           RequestContext::kSubresource},
          Policy::kPermissionBlock,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kLocal,
           RequestContext::kSubresource},
          Policy::kPermissionBlock,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kLoopback,
           RequestContext::kSubresource},
          Policy::kPermissionBlock,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kUnknown,
           RequestContext::kSubresource},
          Policy::kPermissionBlock,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kPublic,
           RequestContext::kSubresource},
          Policy::kPermissionBlock,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kLocal,
           RequestContext::kSubresource},
          Policy::kPermissionBlock,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kLoopback,
           RequestContext::kSubresource},
          Policy::kPermissionBlock,
      },
      //
      // `RequestContext::kWorker`
      //
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kUnknown,
           RequestContext::kWorker},
          Policy::kBlock,
      },
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kPublic,
           RequestContext::kWorker},
          Policy::kBlock,
      },
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kLocal,
           RequestContext::kWorker},
          Policy::kBlock,
      },
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kLoopback,
           RequestContext::kWorker},
          Policy::kBlock,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kUnknown,
           RequestContext::kWorker},
          Policy::kPermissionBlock,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kPublic,
           RequestContext::kWorker},
          Policy::kPermissionBlock,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kLocal,
           RequestContext::kWorker},
          Policy::kPermissionBlock,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kLoopback,
           RequestContext::kWorker},
          Policy::kPermissionBlock,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kUnknown,
           RequestContext::kWorker},
          Policy::kPermissionBlock,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kPublic,
           RequestContext::kWorker},
          Policy::kPermissionBlock,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kLocal,
           RequestContext::kWorker},
          Policy::kPermissionBlock,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kLoopback,
           RequestContext::kWorker},
          Policy::kPermissionBlock,
      },
      //
      // `RequestContext::kMainFrameNavigation`
      //
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kUnknown,
           RequestContext::kMainFrameNavigation},
          Policy::kAllow,
      },
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kPublic,
           RequestContext::kMainFrameNavigation},
          Policy::kAllow,
      },
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kLocal,
           RequestContext::kMainFrameNavigation},
          Policy::kAllow,
      },
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kLoopback,
           RequestContext::kMainFrameNavigation},
          Policy::kAllow,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kUnknown,
           RequestContext::kMainFrameNavigation},
          Policy::kAllow,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kPublic,
           RequestContext::kMainFrameNavigation},
          Policy::kAllow,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kLocal,
           RequestContext::kMainFrameNavigation},
          Policy::kAllow,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kLoopback,
           RequestContext::kMainFrameNavigation},
          Policy::kAllow,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kUnknown,
           RequestContext::kMainFrameNavigation},
          Policy::kAllow,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kPublic,
           RequestContext::kMainFrameNavigation},
          Policy::kAllow,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kLocal,
           RequestContext::kMainFrameNavigation},
          Policy::kAllow,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kLoopback,
           RequestContext::kMainFrameNavigation},
          Policy::kAllow,
      },
      //
      // `RequestContext::kSubframeNavigation`
      //
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kUnknown,
           RequestContext::kSubframeNavigation},
          Policy::kBlock,
      },
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kPublic,
           RequestContext::kSubframeNavigation},
          Policy::kBlock,
      },
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kLocal,
           RequestContext::kSubframeNavigation},
          Policy::kBlock,
      },
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kLoopback,
           RequestContext::kSubframeNavigation},
          Policy::kBlock,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kUnknown,
           RequestContext::kSubframeNavigation},
          Policy::kPermissionBlock,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kPublic,
           RequestContext::kSubframeNavigation},
          Policy::kPermissionBlock,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kLocal,
           RequestContext::kSubframeNavigation},
          Policy::kPermissionBlock,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kLoopback,
           RequestContext::kSubframeNavigation},
          Policy::kPermissionBlock,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kUnknown,
           RequestContext::kSubframeNavigation},
          Policy::kPermissionBlock,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kPublic,
           RequestContext::kSubframeNavigation},
          Policy::kPermissionBlock,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kLocal,
           RequestContext::kSubframeNavigation},
          Policy::kPermissionBlock,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kLoopback,
           RequestContext::kSubframeNavigation},
          Policy::kPermissionBlock,
      },
      //
      // `RequestContext::kFencedFrameNavigation`
      //
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kUnknown,
           RequestContext::kFencedFrameNavigation},
          Policy::kBlock,
      },
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kPublic,
           RequestContext::kFencedFrameNavigation},
          Policy::kBlock,
      },
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kLocal,
           RequestContext::kFencedFrameNavigation},
          Policy::kBlock,
      },
      {
          {kNonSecure, kDisallowNonSecure, AddressSpace::kLoopback,
           RequestContext::kFencedFrameNavigation},
          Policy::kBlock,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kUnknown,
           RequestContext::kFencedFrameNavigation},
          Policy::kPermissionBlock,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kPublic,
           RequestContext::kFencedFrameNavigation},
          Policy::kPermissionBlock,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kLocal,
           RequestContext::kFencedFrameNavigation},
          Policy::kPermissionBlock,
      },
      {
          {kNonSecure, kAllowNonSecure, AddressSpace::kLoopback,
           RequestContext::kFencedFrameNavigation},
          Policy::kPermissionBlock,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kUnknown,
           RequestContext::kFencedFrameNavigation},
          Policy::kPermissionBlock,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kPublic,
           RequestContext::kFencedFrameNavigation},
          Policy::kPermissionBlock,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kLocal,
           RequestContext::kFencedFrameNavigation},
          Policy::kPermissionBlock,
      },
      {
          {kSecure, kDisallowNonSecure, AddressSpace::kLoopback,
           RequestContext::kFencedFrameNavigation},
          Policy::kPermissionBlock,
      },
  };
}

std::vector<std::pair<DerivePolicyInput, Policy>> AllAllowMap() {
  std::vector<std::pair<DerivePolicyInput, Policy>> result = DefaultPolicyMap();
  for (auto& entry : result) {
    entry.second = Policy::kAllow;
  }
  return result;
}

// Runs `DerivePolicy()` on each DerivePolicyInput and compares the result to
// the expected value.
void TestPolicyMap(
    const std::vector<std::pair<DerivePolicyInput, Policy>>& expected) {
  for (const auto& [input, policy] : expected) {
    EXPECT_EQ(DerivePolicy(input), policy) << input;
  }
}

TEST(PrivateNetworkAccessUtilTest, DerivePolicyLocalNetworkAccessDiabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      network::features::kLocalNetworkAccessChecks);
  TestPolicyMap(DefaultPolicyMap());
}

TEST(PrivateNetworkAccessUtilTest, DerivePolicyDisableWebSecurity) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableWebSecurity);

  TestPolicyMap(AllAllowMap());
}

// Test the configuration in LNA blocking mode.
TEST(PrivateNetworkAccessUtilTest, DerivePolicyLocalNetworkAccess) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["LocalNetworkAccessChecksWarn"] = "false";
  feature_list.InitAndEnableFeatureWithParameters(
      network::features::kLocalNetworkAccessChecks, params);

  std::vector<std::pair<DerivePolicyInput, Policy>> expected = LNAPolicyMap();
  TestPolicyMap(expected);
}

// Test the configuration in LNA warning-only mode.
TEST(PrivateNetworkAccessUtilTest, DerivePolicyLocalNetworkAccessWarn) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["LocalNetworkAccessChecksWarn"] = "true";
  feature_list.InitAndEnableFeatureWithParameters(
      network::features::kLocalNetworkAccessChecks, params);

  // Warning-only LNA should just be the LNA policy map but k*Block
  // is downgraded to kPermissionWarn.
  std::vector<std::pair<DerivePolicyInput, Policy>> expected = LNAPolicyMap();
  for (auto& entry : expected) {
    if (entry.second == Policy::kPermissionBlock) {
      entry.second = Policy::kPermissionWarn;
    } else if (entry.second == Policy::kBlock) {
      entry.second = Policy::kPermissionWarn;
    }
  }
  TestPolicyMap(expected);
}

}  // namespace
}  // namespace content
