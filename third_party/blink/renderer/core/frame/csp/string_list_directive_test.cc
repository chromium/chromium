// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/string_list_directive.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/csp/csp_directive_list.h"

namespace blink {

class StringListDirectiveTest : public testing::Test {
 public:
  StringListDirectiveTest()
      : csp_(MakeGarbageCollected<ContentSecurityPolicy>()) {}

 protected:
  Persistent<ContentSecurityPolicy> csp_;
};

TEST_F(StringListDirectiveTest, TestAllowLists) {
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
    StringListDirective directive("trusted-types", test_case.directive,
                                  csp_.Get());

    Vector<String> allowed;
    String(test_case.should_be_allowed).Split(' ', allowed);
    for (const String& value : allowed) {
      SCOPED_TRACE(testing::Message()
                   << " trusted-types " << test_case.directive
                   << "; allow: " << value);
      EXPECT_TRUE(directive.Allows(value, false, violation_details));
      EXPECT_EQ(violation_details,
                ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed);
      EXPECT_EQ(directive.Allows(value, true, violation_details),
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
      EXPECT_FALSE(directive.Allows(value, false, violation_details));
      EXPECT_EQ(violation_details,
                ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
                    kDisallowedName);
      EXPECT_FALSE(directive.Allows(value, true, violation_details));
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
