// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/origin_access_entry.h"
#include "services/network/public/mojom/cors.mojom.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace cors {

namespace {

TEST(OriginAccessEntryTest, PublicSuffixListTest) {
  url::Origin origin = url::Origin::Create(GURL("http://www.google.com"));
  OriginAccessEntry entry1(
      "http", "google.com", OriginAccessEntry::kAllowSubdomains,
      network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);
  OriginAccessEntry entry2(
      "http", "hamster.com", OriginAccessEntry::kAllowSubdomains,
      network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);
  OriginAccessEntry entry3(
      "http", "com", OriginAccessEntry::kAllowSubdomains,
      network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);
  EXPECT_EQ(OriginAccessEntry::kMatchesOrigin, entry1.MatchesOrigin(origin));
  EXPECT_EQ(OriginAccessEntry::kDoesNotMatchOrigin,
            entry2.MatchesOrigin(origin));
  EXPECT_EQ(OriginAccessEntry::kMatchesOriginButIsPublicSuffix,
            entry3.MatchesOrigin(origin));
}

TEST(OriginAccessEntryTest, AllowSubdomainsTest) {
  struct TestCase {
    const std::string protocol;
    const std::string host;
    const std::string origin;
    OriginAccessEntry::MatchResult expected_origin;
    OriginAccessEntry::MatchResult expected_domain;
  } inputs[] = {
      {"http", "example.com", "http://example.com/",
       OriginAccessEntry::kMatchesOrigin, OriginAccessEntry::kMatchesOrigin},
      {"http", "example.com", "http://www.example.com/",
       OriginAccessEntry::kMatchesOrigin, OriginAccessEntry::kMatchesOrigin},
      {"http", "example.com", "http://www.www.example.com/",
       OriginAccessEntry::kMatchesOrigin, OriginAccessEntry::kMatchesOrigin},
      {"http", "www.example.com", "http://example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin,
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "www.example.com", "http://www.example.com/",
       OriginAccessEntry::kMatchesOrigin, OriginAccessEntry::kMatchesOrigin},
      {"http", "www.example.com", "http://www.www.example.com/",
       OriginAccessEntry::kMatchesOrigin, OriginAccessEntry::kMatchesOrigin},
      {"http", "com", "http://example.com/",
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix,
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix},
      {"http", "com", "http://www.example.com/",
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix,
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix},
      {"http", "com", "http://www.www.example.com/",
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix,
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix},
      {"https", "example.com", "http://example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin,
       OriginAccessEntry::kMatchesOrigin},
      {"https", "example.com", "http://www.example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin,
       OriginAccessEntry::kMatchesOrigin},
      {"https", "example.com", "http://www.www.example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin,
       OriginAccessEntry::kMatchesOrigin},
      {"http", "example.com", "http://beispiel.de/",
       OriginAccessEntry::kDoesNotMatchOrigin,
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "example.com", "https://beispiel.de/",
       OriginAccessEntry::kDoesNotMatchOrigin,
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "", "http://example.com/", OriginAccessEntry::kMatchesOrigin,
       OriginAccessEntry::kMatchesOrigin},
      {"http", "", "http://beispiel.de/", OriginAccessEntry::kMatchesOrigin,
       OriginAccessEntry::kMatchesOrigin},
      {"https", "", "http://beispiel.de/",
       OriginAccessEntry::kDoesNotMatchOrigin,
       OriginAccessEntry::kMatchesOrigin},
  };

  for (const auto& test : inputs) {
    SCOPED_TRACE(testing::Message()
                 << "Host: " << test.host << ", Origin: " << test.origin);
    url::Origin origin_to_test = url::Origin::Create(GURL(test.origin));
    OriginAccessEntry entry1(
        test.protocol, test.host, OriginAccessEntry::kAllowSubdomains,
        network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);
    EXPECT_EQ(test.expected_origin, entry1.MatchesOrigin(origin_to_test));
    EXPECT_EQ(test.expected_domain, entry1.MatchesDomain(origin_to_test));
  }
}

TEST(OriginAccessEntryTest, AllowRegisterableDomainsTest) {
  struct TestCase {
    const std::string protocol;
    const std::string host;
    const std::string origin;
    OriginAccessEntry::MatchResult expected;
  } inputs[] = {
      {"http", "example.com", "http://example.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "example.com", "http://www.example.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "example.com", "http://www.www.example.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "www.example.com", "http://example.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "www.example.com", "http://www.example.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "www.example.com", "http://www.www.example.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "com", "http://example.com/",
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix},
      {"http", "com", "http://www.example.com/",
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix},
      {"http", "com", "http://www.www.example.com/",
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix},
      {"https", "example.com", "http://example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"https", "example.com", "http://www.example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"https", "example.com", "http://www.www.example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "example.com", "http://beispiel.de/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "", "http://example.com/", OriginAccessEntry::kMatchesOrigin},
      {"http", "", "http://beispiel.de/", OriginAccessEntry::kMatchesOrigin},
      {"https", "", "http://beispiel.de/",
       OriginAccessEntry::kDoesNotMatchOrigin},
  };

  for (const auto& test : inputs) {
    url::Origin origin_to_test = url::Origin::Create(GURL(test.origin));
    OriginAccessEntry entry1(
        test.protocol, test.host, OriginAccessEntry::kAllowRegisterableDomains,
        network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);

    SCOPED_TRACE(testing::Message()
                 << "Host: " << test.host << ", Origin: " << test.origin
                 << ", Domain: " << entry1.registerable_domain());
    EXPECT_EQ(test.expected, entry1.MatchesOrigin(origin_to_test));
  }
}

TEST(OriginAccessEntryTest, AllowRegisterableDomainsTestWithDottedSuffix) {
  struct TestCase {
    const std::string protocol;
    const std::string host;
    const std::string origin;
    OriginAccessEntry::MatchResult expected;
  } inputs[] = {
      {"http", "example.appspot.com", "http://example.appspot.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "example.appspot.com", "http://www.example.appspot.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "example.appspot.com", "http://www.www.example.appspot.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "www.example.appspot.com", "http://example.appspot.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "www.example.appspot.com", "http://www.example.appspot.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "www.example.appspot.com", "http://www.www.example.appspot.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "appspot.com", "http://example.appspot.com/",
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix},
      {"http", "appspot.com", "http://www.example.appspot.com/",
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix},
      {"http", "appspot.com", "http://www.www.example.appspot.com/",
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix},
      {"https", "example.appspot.com", "http://example.appspot.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"https", "example.appspot.com", "http://www.example.appspot.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"https", "example.appspot.com", "http://www.www.example.appspot.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "example.appspot.com", "http://beispiel.de/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "", "http://example.appspot.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "", "http://beispiel.de/", OriginAccessEntry::kMatchesOrigin},
      {"https", "", "http://beispiel.de/",
       OriginAccessEntry::kDoesNotMatchOrigin},
  };

  for (const auto& test : inputs) {
    url::Origin origin_to_test = url::Origin::Create(GURL(test.origin));
    OriginAccessEntry entry1(
        test.protocol, test.host, OriginAccessEntry::kAllowRegisterableDomains,
        network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);

    SCOPED_TRACE(testing::Message()
                 << "Host: " << test.host << ", Origin: " << test.origin
                 << ", Domain: " << entry1.registerable_domain());
    EXPECT_EQ(test.expected, entry1.MatchesOrigin(origin_to_test));
  }
}

TEST(OriginAccessEntryTest, DisallowSubdomainsTest) {
  struct TestCase {
    const std::string protocol;
    const std::string host;
    const std::string origin;
    OriginAccessEntry::MatchResult expected;
  } inputs[] = {
      {"http", "example.com", "http://example.com/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "example.com", "http://www.example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "example.com", "http://www.www.example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "com", "http://example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "com", "http://www.example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "com", "http://www.www.example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"https", "example.com", "http://example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"https", "example.com", "http://www.example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"https", "example.com", "http://www.www.example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "example.com", "http://beispiel.de/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "", "http://example.com/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "", "http://beispiel.de/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"https", "", "http://beispiel.de/",
       OriginAccessEntry::kDoesNotMatchOrigin},
  };

  for (const auto& test : inputs) {
    SCOPED_TRACE(testing::Message()
                 << "Host: " << test.host << ", Origin: " << test.origin);
    url::Origin origin_to_test = url::Origin::Create(GURL(test.origin));
    OriginAccessEntry entry1(
        test.protocol, test.host, OriginAccessEntry::kDisallowSubdomains,
        network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);
    EXPECT_EQ(test.expected, entry1.MatchesOrigin(origin_to_test));
  }
}

TEST(OriginAccessEntryTest, IPAddressTest) {
  struct TestCase {
    const std::string protocol;
    const std::string host;
    bool is_ip_address;
  } inputs[] = {
      {"http", "1.1.1.1", true},
      {"http", "1.1.1.1.1", false},
      {"http", "example.com", false},
      {"http", "hostname.that.ends.with.a.number1", false},
      {"http", "2001:db8::1", false},
      {"http", "[2001:db8::1]", true},
      {"http", "2001:db8::a", false},
      {"http", "[2001:db8::a]", true},
      {"http", "", false},
  };

  for (const auto& test : inputs) {
    SCOPED_TRACE(testing::Message() << "Host: " << test.host);
    OriginAccessEntry entry(
        test.protocol, test.host, OriginAccessEntry::kDisallowSubdomains,
        network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);
    EXPECT_EQ(test.is_ip_address, entry.host_is_ip_address()) << test.host;
  }
}

TEST(OriginAccessEntryTest, IPAddressMatchingTest) {
  struct TestCase {
    const std::string protocol;
    const std::string host;
    const std::string origin;
    OriginAccessEntry::MatchResult expected;
  } inputs[] = {
      {"http", "192.0.0.123", "http://192.0.0.123/",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "0.0.123", "http://192.0.0.123/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "0.123", "http://192.0.0.123/",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "1.123", "http://192.0.0.123/",
       OriginAccessEntry::kDoesNotMatchOrigin},
  };

  for (const auto& test : inputs) {
    SCOPED_TRACE(testing::Message()
                 << "Host: " << test.host << ", Origin: " << test.origin);
    url::Origin origin_to_test = url::Origin::Create(GURL(test.origin));
    OriginAccessEntry entry1(
        test.protocol, test.host, OriginAccessEntry::kAllowSubdomains,
        network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);
    EXPECT_EQ(test.expected, entry1.MatchesOrigin(origin_to_test));

    OriginAccessEntry entry2(
        test.protocol, test.host, OriginAccessEntry::kDisallowSubdomains,
        network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);
    EXPECT_EQ(test.expected, entry2.MatchesOrigin(origin_to_test));
  }
}

}  // namespace

}  // namespace cors

}  // namespace network
