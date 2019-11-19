// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/origin_policy/origin_policy_parser.h"
#include "services/network/public/mojom/origin_policy_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// Unit tests for OriginPolicyParser.
//
// These are fairly simple "smoke tests". The majority of test coverage is
// expected from wpt/origin-policy/ end-to-end tests.

namespace {

void AssertEmptyPolicy(
    const network::OriginPolicyContentsPtr& policy_contents) {
  ASSERT_EQ(0u, policy_contents->features.size());
  ASSERT_EQ(0u, policy_contents->content_security_policies.size());
  ASSERT_EQ(0u, policy_contents->content_security_policies_report_only.size());
}

}  // namespace

namespace network {

TEST(OriginPolicyParser, Empty) {
  auto policy_contents = OriginPolicyParser::Parse("");
  AssertEmptyPolicy(policy_contents);
}

TEST(OriginPolicyParser, Invalid) {
  auto policy_contents = OriginPolicyParser::Parse("potato potato potato");
  AssertEmptyPolicy(policy_contents);
}

TEST(OriginPolicyParser, ValidButEmpty) {
  auto policy_contents = OriginPolicyParser::Parse("{}");
  AssertEmptyPolicy(policy_contents);
}

TEST(OriginPolicyParser, SimpleCSP) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "content-security-policy": [{
          "policy": "script-src 'self' 'unsafe-inline'"
      }] }
  )");
  ASSERT_EQ(policy_contents->content_security_policies.size(), 1U);
  ASSERT_EQ(policy_contents->content_security_policies[0],
            "script-src 'self' 'unsafe-inline'");
}

TEST(OriginPolicyParser, DoubleCSP) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "content-security-policy": [{
          "policy": "script-src 'self' 'unsafe-inline'",
          "report-only": false
        },{
          "policy": "script-src 'self' 'https://example.com/'",
          "report-only": true
      }] }
  )");
  ASSERT_EQ(policy_contents->content_security_policies.size(), 1U);
  ASSERT_EQ(policy_contents->content_security_policies_report_only.size(), 1U);

  ASSERT_EQ(policy_contents->content_security_policies[0],
            "script-src 'self' 'unsafe-inline'");
  ASSERT_EQ(policy_contents->content_security_policies_report_only[0],
            "script-src 'self' 'https://example.com/'");
}

TEST(OriginPolicyParser, HalfDoubleCSP) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "content-security-policy": [{
          "policy": "script-src 'self' 'unsafe-inline'",
        },{
          "policies": "script-src 'self' 'https://example.com/'",
      }] }
  )");
  AssertEmptyPolicy(policy_contents);
}

TEST(OriginPolicyParser, CSPWithoutCSP) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "content-security-policy": [{
          "police": "script-src 'self' 'unsafe-inline'",
          "report-only": false
        }] }
  )");
  AssertEmptyPolicy(policy_contents);
}

TEST(OriginPolicyParser, ExtraFieldsDontBreakParsing) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "potatoes": "are better than kale",
        "content-security-policy": [{
          "report-only": false,
          "potatoes": "are best",
          "policy": "script-src 'self' 'unsafe-inline'"
        }],
        "other": {
          "name": "Sieglinde",
          "value": "best of potatoes"
      }}
  )");
  ASSERT_EQ(policy_contents->content_security_policies.size(), 1U);
  ASSERT_EQ(policy_contents->content_security_policies[0],
            "script-src 'self' 'unsafe-inline'");
}

TEST(OriginPolicyParser, CSPDispositionEnforce) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "content-security-policy": [{
          "policy": "script-src 'self'",
          "report-only": false
        }] }
  )");
  ASSERT_EQ(policy_contents->content_security_policies.size(), 1U);
  ASSERT_EQ(policy_contents->content_security_policies[0], "script-src 'self'");
}

TEST(OriginPolicyParser, CSPDispositionReport) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "content-security-policy": [{
          "policy": "script-src 'self'",
          "report-only": true
        }] }
  )");
  ASSERT_EQ(policy_contents->content_security_policies_report_only.size(), 1U);
  ASSERT_EQ(policy_contents->content_security_policies_report_only[0],
            "script-src 'self'");
}

TEST(OriginPolicyParser, CSPDispositionInvalid) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "content-security-policy": [{
          "policy": "script-src 'self'",
          "report-only": "potato"
        }] }
  )");
  ASSERT_EQ(policy_contents->content_security_policies.size(), 1U);
  ASSERT_EQ(policy_contents->content_security_policies[0], "script-src 'self'");
}

TEST(OriginPolicyParser, CSPDispositionAbsent) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "content-security-policy": [{
          "policy": "script-src 'self'"
        }] }
  )");
  ASSERT_EQ(policy_contents->content_security_policies.size(), 1U);
  ASSERT_EQ(policy_contents->content_security_policies[0], "script-src 'self'");
}

TEST(OriginPolicyParser, FeatureOne) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "feature-policy": ["geolocation 'self' http://maps.google.com"] } )");
  ASSERT_EQ(1U, policy_contents->features.size());
  ASSERT_EQ("geolocation 'self' http://maps.google.com",
            policy_contents->features[0]);
}

TEST(OriginPolicyParser, FeatureTwo) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "feature-policy": ["geolocation 'self' http://maps.google.com",
                     "camera https://example.com"]} )");
  ASSERT_EQ(2U, policy_contents->features.size());
  ASSERT_EQ("geolocation 'self' http://maps.google.com",
            policy_contents->features[0]);
  ASSERT_EQ("camera https://example.com", policy_contents->features[1]);
}

TEST(OriginPolicyParser, FeatureTwoPolicies) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "feature-policy": ["geolocation 'self' http://maps.google.com"],
        "feature-policy": ["camera https://example.com"] } )");

  // TODO(vogelheim): Determine whether this is the correct behaviour.
  ASSERT_EQ(1U, policy_contents->features.size());
}

TEST(OriginPolicyParser, FeatureComma) {
  auto policy_contents = OriginPolicyParser::Parse(R"(
      { "feature-policy": ["geolocation 'self' http://maps.google.com, camera https://example.com"]} )");

  // TODO: Determine what to do with this case !
  ASSERT_EQ(1U, policy_contents->features.size());
}

}  // namespace network
