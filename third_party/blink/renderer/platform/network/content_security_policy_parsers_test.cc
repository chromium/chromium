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
      {"", true},                    // Empty string
      {" \t\n\r  ", true},           // Only whitespace
      {";", true},                   // Only semicolon
      {";;;", true},                 // Multiple semicolons
      {" ; \t\n\r ; ", true},        // Semicolons and whitespace
      {"default-src 'self'", true},  // Single directive
      {"script-src data:;", true},   // Trailing semicolon
      {"default-src 'self' 'unsafe-inline'", true},  // Multiple values
      {"\t\n\r   default-src 'self'", true},         // Leading whitespace
      {"default-src \t\n\r   'self'", true},         // Inner whitespace
      {"script-src 'none'; invalid-directive ", true},
      {"script-src 'none'; invalid-directive;", true},
      {" script-src 'none' https://www.example.org   ; ;invalid-directive;  ;",
       true},
      {" 'self'", false},                        // Missing directive.
      {"default+src 'self'", false},             // Invalid directive character
      {"default-src 'self\xffinvalid;", false},  // Invalid value character
      {"script-src 'none', media-src 'none'", false},         // Comma separated
      {"script-src 'none'; /invalid-directive-name", false},  // Invalid second
  };

  for (const auto& testCase : testCases) {
    EXPECT_EQ(MatchesTheSerializedCSPGrammar(testCase.value),
              testCase.expected);
  }
}

}  // namespace blink
