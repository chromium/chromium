// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/trusted_types_directive.h"

#include "services/network/public/mojom/content_security_policy.mojom-blink-forward.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"

namespace blink {

namespace {

network::mojom::blink::CSPTrustedTypesPtr ParseTrustedTypes(
    const String& value) {
  Vector<network::mojom::blink::ContentSecurityPolicyPtr> parsed =
      ParseContentSecurityPolicies(
          "trusted-types " + value,
          network::mojom::blink::ContentSecurityPolicyType::kEnforce,
          network::mojom::blink::ContentSecurityPolicySource::kHTTP,
          KURL("https://example.test"));
  return std::move(parsed[0]->trusted_types);
}

}  // namespace

TEST(TrustedTypesDirectiveTest, TestAllowLists) {
  struct {
    const char* directive;
    const char* should_be_allowed;
    const char* should_not_be_allowed;
    bool allow_dupes;
  } test_cases[] = {
      {"bla", "bla", "blubb", false},
      {"*", "bla blubb", "", false},
      {"", "", "bla blubb", false},
      {"*", "bla a.b 123 a-b", "'bla' abc*def a,e a+b", false},
      {"* 'allow-duplicates'", "bla blubb", "", true},
      {"'allow-duplicates' *", "bla blubb", "", true},
      {"bla 'allow-duplicates'", "bla", "blubb", true},
      {"'allow-duplicates' bla", "bla", "blub", true},
      {"'allow-duplicates'", "", "bla blub", true},
      {"'allow-duplicates' bla blubb", "bla blubb", "blubber", true},
      {"'none'", "", "default none abc", false},
      {"'none' default", "default", "none abc", false},
      {"* 'none'", "default none abc", "", false},
      {"'allow-duplicates' 'none'", "", "default none abc", true},
  };
  ContentSecurityPolicy::AllowTrustedTypePolicyDetails violation_details;

  for (const auto& test_case : test_cases) {
    network::mojom::blink::CSPTrustedTypesPtr directive =
        ParseTrustedTypes(test_case.directive);

    Vector<String> allowed;
    String(test_case.should_be_allowed).Split(' ', allowed);
    for (const String& value : allowed) {
      SCOPED_TRACE(testing::Message()
                   << " trusted-types " << test_case.directive
                   << "; allow: " << value);
      EXPECT_TRUE(
          CSPTrustedTypesAllows(*directive, value, false, violation_details));
      EXPECT_EQ(violation_details,
                ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed);
      EXPECT_EQ(
          CSPTrustedTypesAllows(*directive, value, true, violation_details),
          test_case.allow_dupes);
      if (test_case.allow_dupes) {
        EXPECT_EQ(
            violation_details,
            ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed);
      } else {
        EXPECT_EQ(violation_details,
                  ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
                      kDisallowedDuplicateName);
      }
    }

    Vector<String> not_allowed;
    String(test_case.should_not_be_allowed).Split(' ', not_allowed);
    for (const String& value : not_allowed) {
      SCOPED_TRACE(testing::Message()
                   << " trusted-types " << test_case.directive
                   << "; do not allow: " << value);
      EXPECT_FALSE(
          CSPTrustedTypesAllows(*directive, value, false, violation_details));
      EXPECT_EQ(violation_details,
                ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
                    kDisallowedName);
      EXPECT_FALSE(
          CSPTrustedTypesAllows(*directive, value, true, violation_details));
      if (!test_case.allow_dupes || value == "default") {
        EXPECT_EQ(violation_details,
                  ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
                      kDisallowedDuplicateName);
      } else {
        EXPECT_EQ(violation_details,
                  ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
                      kDisallowedName);
      }
    }
  }
}

}  // namespace blink
