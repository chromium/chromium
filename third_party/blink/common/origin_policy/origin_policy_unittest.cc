// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/origin_policy/origin_policy.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// Unit tests for OriginPolicy / OriginPolicyParser.
//
// These are fairly simple "smoke tests". The majority of test coverage is
// expected from wpt/origin-policy/ end-to-end tests.

TEST(OriginPolicy, Empty) {
  auto policy = blink::OriginPolicy::From("");
  ASSERT_FALSE(policy);
}

TEST(OriginPolicy, Invalid) {
  auto policy = blink::OriginPolicy::From("potato potato potato");
  ASSERT_FALSE(policy);
}

TEST(OriginPolicy, ValidButEmpty) {
  auto policy = blink::OriginPolicy::From("{}");
  ASSERT_TRUE(policy);
  ASSERT_TRUE(policy->GetContentSecurityPolicies().empty());
}

TEST(OriginPolicy, SimpleCSP) {
  auto policy = blink::OriginPolicy::From(R"(
      { "content-security-policy": [{
          "policy": "script-src 'self' 'unsafe-inline'"
      }] }
  )");
  ASSERT_TRUE(policy);
  ASSERT_EQ(policy->GetContentSecurityPolicies().size(), 1U);
  ASSERT_EQ(policy->GetContentSecurityPolicies()[0].policy,
            "script-src 'self' 'unsafe-inline'");
}

TEST(OriginPolicy, DoubleCSP) {
  auto policy = blink::OriginPolicy::From(R"(
      { "content-security-policy": [{
          "policy": "script-src 'self' 'unsafe-inline'",
          "report-only": false
        },{
          "policy": "script-src 'self' 'https://example.com/'",
          "report-only": true
      }] }
  )");
  ASSERT_TRUE(policy);
  ASSERT_EQ(policy->GetContentSecurityPolicies().size(), 2U);
  ASSERT_EQ(policy->GetContentSecurityPolicies()[0].policy,
            "script-src 'self' 'unsafe-inline'");
  ASSERT_FALSE(policy->GetContentSecurityPolicies()[0].report_only);
  ASSERT_EQ(policy->GetContentSecurityPolicies()[1].policy,
            "script-src 'self' 'https://example.com/'");
  ASSERT_TRUE(policy->GetContentSecurityPolicies()[1].report_only);
}

TEST(OriginPolicy, HalfDoubleCSP) {
  auto policy = blink::OriginPolicy::From(R"(
      { "content-security-policy": [{
          "policy": "script-src 'self' 'unsafe-inline'",
        },{
          "policies": "script-src 'self' 'https://example.com/'",
      }] }
  )");
  ASSERT_FALSE(policy);
}

TEST(OriginPolicy, CSPWithoutCSP) {
  auto policy = blink::OriginPolicy::From(R"(
      { "content-security-policy": [{
          "police": "script-src 'self' 'unsafe-inline'",
          "report-only": false
        }] }
  )");
  ASSERT_FALSE(policy);
}

TEST(OriginPolicy, ExtraFieldsDontBreakParsing) {
  auto policy = blink::OriginPolicy::From(R"(
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
  ASSERT_TRUE(policy);
  ASSERT_EQ(policy->GetContentSecurityPolicies().size(), 1U);
  ASSERT_EQ(policy->GetContentSecurityPolicies()[0].policy,
            "script-src 'self' 'unsafe-inline'");
}

TEST(OriginPolicy, CSPDispositionEnforce) {
  auto policy = blink::OriginPolicy::From(R"(
      { "content-security-policy": [{
          "policy": "script-src 'self'",
          "report-only": false
        }] }
  )");
  ASSERT_FALSE(policy->GetContentSecurityPolicies()[0].report_only);
}

TEST(OriginPolicy, CSPDispositionReport) {
  auto policy = blink::OriginPolicy::From(R"(
      { "content-security-policy": [{
          "policy": "script-src 'self'",
          "report-only": true
        }] }
  )");
  ASSERT_TRUE(policy->GetContentSecurityPolicies()[0].report_only);
}

TEST(OriginPolicy, CSPDispositionInvalid) {
  auto policy = blink::OriginPolicy::From(R"(
      { "content-security-policy": [{
          "policy": "script-src 'self'",
          "report-only": "potato"
        }] }
  )");
  ASSERT_FALSE(policy->GetContentSecurityPolicies()[0].report_only);
}

TEST(OriginPolicy, CSPDispositionAbsent) {
  auto policy = blink::OriginPolicy::From(R"(
      { "content-security-policy": [{
          "policy": "script-src 'self'"
        }] }
  )");
  ASSERT_FALSE(policy->GetContentSecurityPolicies()[0].report_only);
}

TEST(OriginPolicy, FeatureOne) {
  auto policy = blink::OriginPolicy::From(R"(
      { "feature-policy": ["geolocation 'self' http://maps.google.com"] } )");
  ASSERT_EQ(1U, policy->GetFeaturePolicies().size());
  ASSERT_EQ("geolocation 'self' http://maps.google.com",
            policy->GetFeaturePolicies()[0]);
}

TEST(OriginPolicy, FeatureTwo) {
  auto policy = blink::OriginPolicy::From(R"(
      { "feature-policy": ["geolocation 'self' http://maps.google.com",
                     "camera https://example.com"]} )");
  ASSERT_EQ(2U, policy->GetFeaturePolicies().size());
  ASSERT_EQ("geolocation 'self' http://maps.google.com",
            policy->GetFeaturePolicies()[0]);
  ASSERT_EQ("camera https://example.com", policy->GetFeaturePolicies()[1]);
}

TEST(OriginPolicy, FeatureTwoPolicies) {
  auto policy = blink::OriginPolicy::From(R"(
      { "feature-policy": ["geolocation 'self' http://maps.google.com"],
        "feature-policy": ["camera https://example.com"] } )");

  // TODO(vogelheim): Determine whether this is the correct behaviour.
  ASSERT_EQ(1U, policy->GetFeaturePolicies().size());
}

TEST(OriginPolicy, FeatureComma) {
  auto policy = blink::OriginPolicy::From(R"(
      { "feature-policy": ["geolocation 'self' http://maps.google.com, camera https://example.com"]} )");

  // TODO: Determine what to do with this case !
  ASSERT_EQ(1U, policy->GetFeaturePolicies().size());
}

TEST(OriginPolicy, FirstPartySetSimpleValid) {
  auto policy = blink::OriginPolicy::From(R"(
    { "first-party-set": ["https://example.com/", "https://not-example.com/"] } )");

  ASSERT_TRUE(policy);
  ASSERT_EQ(2U, policy->GetFirstPartySet().size());
  ASSERT_EQ(1U, policy->GetFirstPartySet().count(
                    url::Origin::Create(GURL("https://example.com/"))));
  ASSERT_EQ(1U, policy->GetFirstPartySet().count(
                    url::Origin::Create(GURL("https://not-example.com/"))));
}

TEST(OriginPolicy, FirstPartySetInvalidUrl) {
  auto policy = blink::OriginPolicy::From(R"(
    { "first-party-set": ["https://example.com/", "what even is this"] } )");

  ASSERT_TRUE(policy);
  ASSERT_EQ(0U, policy->GetFirstPartySet().size());
}

TEST(OriginPolicy, FirstPartySetEmptyList) {
  auto policy = blink::OriginPolicy::From(R"(
    { "first-party-set": [ ] } )");

  ASSERT_TRUE(policy);
  ASSERT_EQ(0U, policy->GetFirstPartySet().size());
}

TEST(OriginPolicy, FirstPartySetWithPath) {
  auto policy = blink::OriginPolicy::From(R"(
    { "first-party-set": ["https://example.com/some-path"] } )");

  ASSERT_TRUE(policy);
  ASSERT_EQ(1U, policy->GetFirstPartySet().size());
  ASSERT_EQ(1U, policy->GetFirstPartySet().count(
                    url::Origin::Create(GURL("https://example.com/"))));
}

TEST(OriginPolicy, FirstPartySetWithPort) {
  auto policy = blink::OriginPolicy::From(R"(
    { "first-party-set": ["https://example.com:1000/", "https://not-example.com/"] } )");

  ASSERT_TRUE(policy);
  ASSERT_EQ(2U, policy->GetFirstPartySet().size());
  ASSERT_EQ(1U, policy->GetFirstPartySet().count(
                    url::Origin::Create(GURL("https://example.com:1000/"))));
  ASSERT_EQ(1U, policy->GetFirstPartySet().count(
                    url::Origin::Create(GURL("https://not-example.com/"))));
}

TEST(OriginPolicy, FirstPartySetMissingScheme) {
  auto policy = blink::OriginPolicy::From(R"(
    { "first-party-set": ["example.com/"] } )");

  ASSERT_TRUE(policy);
  ASSERT_EQ(0U, policy->GetFirstPartySet().size());
}

// Since we use json_parser, and it keeps the last of duplicated elements when
// parsing, the last "first-party-set" is the "true" one.
// TODO(andypaicu): figure out if this is fine, or if it needs to change
TEST(OriginPolicy, FirstPartySetMultipleLists) {
  auto policy = blink::OriginPolicy::From(R"(
    { "first-party-set": ["https://example.com/"],
      "first-party-set": ["https://not-example.com/"] } )");

  ASSERT_EQ(1U, policy->GetFirstPartySet().size());
  ASSERT_EQ(1U, policy->GetFirstPartySet().count(
                    url::Origin::Create(GURL("https://not-example.com/"))));
}

TEST(OriginPolicy, FirstPartySetMultipleSameOrigin) {
  auto policy = blink::OriginPolicy::From(R"(
    { "first-party-set": ["https://example.com/", "https://example.com/some-path", "https://example.com:443/some-other-path"] } )");

  ASSERT_EQ(1U, policy->GetFirstPartySet().size());
  ASSERT_EQ(1U, policy->GetFirstPartySet().count(
                    url::Origin::Create(GURL("https://example.com/"))));
}

TEST(OriginPolicy, FirstPartySetListItemNotAString) {
  struct {
    const char* policy;
  } tests[] = {
      {R"({ "first-party-set": [ "https://example.com", { "https://example.com/": "https://example.com/" }, "https://example.com" ] } )"},
      {R"({ "first-party-set": [ "https://example.com", [ "https://example.com/" ], "https://example.com" ] } )"},
      {R"({ "first-party-set": [ "https://example.com", 123, "https://example.com" ] } )"},
      {R"({ "first-party-set": [ "https://example.com", 123.4, "https://example.com" ] } )"},
      {R"({ "first-party-set": [ "https://example.com", true, "https://example.com" ] } )"},
  };

  for (const auto& test : tests) {
    auto policy = blink::OriginPolicy::From(test.policy);
    ASSERT_TRUE(policy);
    ASSERT_EQ(0U, policy->GetFirstPartySet().size());
  }
}

TEST(OriginPolicy, FirstPartySetNotAList) {
  struct {
    const char* policy;
  } tests[] = {
      {R"({ "first-party-set": "https://example.com/" } )"},
      {R"({ "first-party-set": { "https://example.com/": "https://example.com/" } } )"},
      {R"({ "first-party-set": 1234 } )"},
      {R"({ "first-party-set": 1234.6 } )"},
      {R"({ "first-party-set": true } )"},
  };

  for (const auto& test : tests) {
    auto policy = blink::OriginPolicy::From(test.policy);
    ASSERT_TRUE(policy);
    ASSERT_EQ(0U, policy->GetFirstPartySet().size());
  }
}

TEST(OriginPolicy, FirstPartySetWithCSPAndFeatures) {
  auto policy = blink::OriginPolicy::From(R"(
    { "first-party-set": ["https://example.com/", "https://not-example.com/"],
      "feature-policy": ["geolocation 'self' http://maps.google.com"],
      "content-security-policy": [{
          "policy": "script-src 'self' 'unsafe-inline'"
      }]
    })");

  ASSERT_TRUE(policy);
  ASSERT_EQ(2U, policy->GetFirstPartySet().size());
  ASSERT_EQ(1U, policy->GetFirstPartySet().count(
                    url::Origin::Create(GURL("https://example.com/"))));
  ASSERT_EQ(1U, policy->GetFirstPartySet().count(
                    url::Origin::Create(GURL("https://not-example.com/"))));

  ASSERT_EQ(policy->GetContentSecurityPolicies().size(), 1U);
  ASSERT_EQ(policy->GetContentSecurityPolicies()[0].policy,
            "script-src 'self' 'unsafe-inline'");

  ASSERT_EQ(1U, policy->GetFeaturePolicies().size());
  ASSERT_EQ("geolocation 'self' http://maps.google.com",
            policy->GetFeaturePolicies()[0]);
}
