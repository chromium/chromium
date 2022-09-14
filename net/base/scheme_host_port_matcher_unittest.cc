// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/scheme_host_port_matcher.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(SchemeHostPortMatcherTest, ParseMultipleRules) {
  SchemeHostPortMatcher matcher =
      SchemeHostPortMatcher::FromRawString(".google.com , .foobar.com:30");
  EXPECT_EQ(2u, matcher.rules().size());

  EXPECT_TRUE(matcher.Includes(GURL("http://baz.google.com:40")));
  EXPECT_FALSE(matcher.Includes(GURL("http://google.com:40")));
  EXPECT_TRUE(matcher.Includes(GURL("http://bar.foobar.com:30")));
  EXPECT_FALSE(matcher.Includes(GURL("http://bar.foobar.com")));
  EXPECT_FALSE(matcher.Includes(GURL("http://bar.foobar.com:33")));
}

TEST(SchemeHostPortMatcherTest, WithBadInputs) {
  SchemeHostPortMatcher matcher = SchemeHostPortMatcher::FromRawString(
      ":// , , .google.com , , http://baz");

  EXPECT_EQ(2u, matcher.rules().size());
  EXPECT_EQ("*.google.com", matcher.rules()[0]->ToString());
  EXPECT_EQ("http://baz", matcher.rules()[1]->ToString());

  EXPECT_TRUE(matcher.Includes(GURL("http://baz.google.com:40")));
  EXPECT_TRUE(matcher.Includes(GURL("http://baz")));
  EXPECT_FALSE(matcher.Includes(GURL("http://google.com")));
}

// Tests that URLMatcher does not include logic specific to ProxyBypassRules.
//  * Should not implicitly bypass localhost or link-local addresses
//  * Should not match proxy bypass specific rules like <-loopback> and <local>
//
// Historically, SchemeHostPortMatcher was refactored out of ProxyBypassRules.
// This test confirms that the layering separation is as expected.
TEST(SchemeHostPortMatcherTest, DoesNotMimicProxyBypassRules) {
  // Should not parse <-loopback> as its own rule (will treat it as a hostname
  // rule).
  SchemeHostPortMatcher matcher =
      SchemeHostPortMatcher::FromRawString("<-loopback>");
  EXPECT_EQ(1u, matcher.rules().size());
  EXPECT_EQ("<-loopback>", matcher.rules().front()->ToString());

  // Should not parse <local> as its own rule (will treat it as a hostname
  // rule).
  matcher = SchemeHostPortMatcher::FromRawString("<local>");
  EXPECT_EQ(1u, matcher.rules().size());
  EXPECT_EQ("<local>", matcher.rules().front()->ToString());

  // Should not implicitly match localhost or link-local addresses.
  matcher = SchemeHostPortMatcher::FromRawString("www.example.com");
  EXPECT_EQ(SchemeHostPortMatcherResult::kNoMatch,
            matcher.Evaluate(GURL("http://localhost")));
  EXPECT_EQ(SchemeHostPortMatcherResult::kNoMatch,
            matcher.Evaluate(GURL("http://169.254.1.1")));
}

}  // namespace

}  // namespace net
