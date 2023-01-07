// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(ContentSecurityPolicyParsers, MatchesTheSerializedCSPGrammar) {
  struct {
    String value;
    bool expected;
  } testCases[]{
      {"script-src 'none'; invalid-directive ", true},
      {"script-src 'none'; invalid-directive;", true},
      {" script-src 'none' https://www.example.org   ; ;invalid-directive;  ;",
       true},
      {"script-src 'none', media-src 'none'", false},
      {"script-src 'none'; /invalid-directive-name", false},
  };

  for (const auto& testCase : testCases) {
    EXPECT_EQ(MatchesTheSerializedCSPGrammar(testCase.value),
              testCase.expected);
  }
}

}  // namespace blink
