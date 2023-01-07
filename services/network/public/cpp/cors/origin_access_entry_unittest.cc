// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/origin_access_entry.h"

#include "services/network/public/mojom/cors_origin_pattern.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network::cors {

namespace {

TEST(OriginAccessEntryTest, IsSubdomainOfHost) {
  struct TestCase {
    const std::string subdomain;
    const std::string host;
    const bool expected_result;
  } inputs[] = {
      {"foo.example.com", "example.com", true},
      {"bar.foo.example.com", "example.com", true},
      {"example.com", "com", true},
      {"example.com", "example.com", false},
      {"badexample.com", "example.com", false},
      {"", "", false},
      {"bar.Example.com", "example.com", false},
  };

  for (const auto& test : inputs) {
    SCOPED_TRACE(testing::Message()
                 << "subdomain: " << test.subdomain << ", Host: " << test.host);
    EXPECT_EQ(test.expected_result,
              IsSubdomainOfHost(test.subdomain, test.host));
  }
}

TEST(OriginAccessEntryTest, PublicSuffixListTest) {
  struct TestCase {
    const std::string host;
    OriginAccessEntry::MatchResult expected;
  } inputs[] = {
      {"google.com", OriginAccessEntry::kMatchesOrigin},
      {"hamster.com", OriginAccessEntry::kDoesNotMatchOrigin},
      {"com", OriginAccessEntry::kMatchesOriginButIsPublicSuffix},
  };

  // Implementation expects url::Origin to set the default port for the
  // specified scheme.
  const url::Origin origin = url::Origin::Create(GURL("http://www.google.com"));
  EXPECT_EQ(80, origin.port());

  for (const auto& test : inputs) {
    SCOPED_TRACE(testing::Message() << "Host: " << test.host);
    OriginAccessEntry entry(
        origin.scheme(), test.host, /*port=*/0,
        mojom::CorsDomainMatchMode::kAllowSubdomains,
        mojom::CorsPortMatchMode::kAllowAnyPort,
        mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
    EXPECT_EQ(test.expected, entry.MatchesOrigin(origin));
  }
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
    OriginAccessEntry entry(
        test.protocol, test.host, /*port=*/0,
        mojom::CorsDomainMatchMode::kAllowSubdomains,
        mojom::CorsPortMatchMode::kAllowAnyPort,
        mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
    EXPECT_EQ(test.expected_origin, entry.MatchesOrigin(origin_to_test));
    EXPECT_EQ(test.expected_domain, entry.MatchesDomain(origin_to_test.host()));
  }
}

TEST(OriginAccessEntryTest, AllowRegistrableDomainsTest) {
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

      // Table of examples from the HTML spec. (Except those based on
      // IP-address, which we don't support.)
      // https://html.spec.whatwg.org/multipage/origin.html#dom-document-domain
      {"http", "0.0.0.0", "http://0.0.0.0", OriginAccessEntry::kMatchesOrigin},
      {"http", "example.com", "http://example.com",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "example.com", "http://example.com.",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "example.com.", "http://example.com",
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http", "example.com", "http://www.example.com",
       OriginAccessEntry::kMatchesOrigin},
      {"http", "com", "http://example.com",
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix},
      {"http", "example", "http://example", OriginAccessEntry::kMatchesOrigin},
      {"http", "compute.amazonaws.com", "http://example.compute.amazonaws.com",
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix},
      {"http", "example.compute.amazonaws.com",
       "http://www.example.compute.amazonaws.com",
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix},
      {"http", "amazonaws.com", "http://www.example.compute.amazonaws.com",
       OriginAccessEntry::kMatchesOriginButIsPublicSuffix},
      {"http", "amazonaws.com", "http://test.amazonaws.com",
       OriginAccessEntry::kMatchesOrigin},

  };

  for (const auto& test : inputs) {
    url::Origin origin_to_test = url::Origin::Create(GURL(test.origin));
    OriginAccessEntry entry(
        test.protocol, test.host, /*port=*/0,
        mojom::CorsDomainMatchMode::kAllowRegistrableDomains,
        mojom::CorsPortMatchMode::kAllowAnyPort,
        mojom::CorsOriginAccessMatchPriority::kDefaultPriority);

    SCOPED_TRACE(testing::Message()
                 << "Host: " << test.host << ", Origin: " << test.origin
                 << ", Domain: " << entry.registrable_domain());
    EXPECT_EQ(test.expected, entry.MatchesOrigin(origin_to_test));
  }
}

TEST(OriginAccessEntryTest, AllowRegistrableDomainsTestWithDottedSuffix) {
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
    OriginAccessEntry entry(
        test.protocol, test.host, /*port=*/0,
        mojom::CorsDomainMatchMode::kAllowRegistrableDomains,
        mojom::CorsPortMatchMode::kAllowAnyPort,
        mojom::CorsOriginAccessMatchPriority::kDefaultPriority);

    SCOPED_TRACE(testing::Message()
                 << "Host: " << test.host << ", Origin: " << test.origin
                 << ", Domain: " << entry.registrable_domain());
    EXPECT_EQ(test.expected, entry.MatchesOrigin(origin_to_test));
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
    OriginAccessEntry entry(
        test.protocol, test.host, /*port=*/0,
        mojom::CorsDomainMatchMode::kDisallowSubdomains,
        mojom::CorsPortMatchMode::kAllowAnyPort,
        mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
    EXPECT_EQ(test.expected, entry.MatchesOrigin(origin_to_test));
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
        test.protocol, test.host, /*port=*/0,
        mojom::CorsDomainMatchMode::kDisallowSubdomains,
        mojom::CorsPortMatchMode::kAllowAnyPort,
        mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
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
        test.protocol, test.host, /*port=*/0,
        mojom::CorsDomainMatchMode::kAllowSubdomains,
        mojom::CorsPortMatchMode::kAllowAnyPort,
        mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
    EXPECT_EQ(test.expected, entry1.MatchesOrigin(origin_to_test));

    OriginAccessEntry entry2(
        test.protocol, test.host, /*port=*/0,
        mojom::CorsDomainMatchMode::kDisallowSubdomains,
        mojom::CorsPortMatchMode::kAllowAnyPort,
        mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
    EXPECT_EQ(test.expected, entry2.MatchesOrigin(origin_to_test));
  }
}

TEST(OriginAccessEntryTest, PortMatchingTest) {
  struct TestCase {
    const std::string origin;
    uint16_t port;
    mojom::CorsPortMatchMode mode;
    OriginAccessEntry::MatchResult expected;
  } inputs[] = {
      {"http://example.com/", 0, mojom::CorsPortMatchMode::kAllowAnyPort,
       OriginAccessEntry::kMatchesOrigin},
      {"http://example.com:8080/", 0, mojom::CorsPortMatchMode::kAllowAnyPort,
       OriginAccessEntry::kMatchesOrigin},
      {"http://example.com/", 80,
       mojom::CorsPortMatchMode::kAllowOnlySpecifiedPort,
       OriginAccessEntry::kMatchesOrigin},
      {"http://example.com/", 8080,
       mojom::CorsPortMatchMode::kAllowOnlySpecifiedPort,
       OriginAccessEntry::kDoesNotMatchOrigin},
      {"http://example.com:8080/", 80,
       mojom::CorsPortMatchMode::kAllowOnlySpecifiedPort,
       OriginAccessEntry::kDoesNotMatchOrigin},
  };

  for (const auto& test : inputs) {
    SCOPED_TRACE(testing::Message()
                 << "Port: " << test.port << ", Origin: " << test.origin);
    url::Origin origin_to_test = url::Origin::Create(GURL(test.origin));
    OriginAccessEntry entry(
        origin_to_test.scheme(), origin_to_test.host(), test.port,
        mojom::CorsDomainMatchMode::kAllowSubdomains, test.mode,
        mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
    EXPECT_EQ(test.expected, entry.MatchesOrigin(origin_to_test));
  }
}

TEST(OriginAccessEntryTest, CreateCorsOriginPattern) {
  const std::string kProtocol = "https";
  const std::string kDomain = "google.com";
  const uint16_t kPort = 443;
  const auto kDomainMatchMode = mojom::CorsDomainMatchMode::kAllowSubdomains;
  const auto kPortMatchMode = mojom::CorsPortMatchMode::kAllowOnlySpecifiedPort;
  const auto kPriority = mojom::CorsOriginAccessMatchPriority::kDefaultPriority;

  OriginAccessEntry entry(kProtocol, kDomain, kPort, kDomainMatchMode,
                          kPortMatchMode, kPriority);
  mojom::CorsOriginPatternPtr pattern = entry.CreateCorsOriginPattern();
  DCHECK_EQ(kProtocol, pattern->protocol);
  DCHECK_EQ(kDomain, pattern->domain);
  DCHECK_EQ(kPort, pattern->port);
  DCHECK_EQ(kDomainMatchMode, pattern->domain_match_mode);
  DCHECK_EQ(kPortMatchMode, pattern->port_match_mode);
  DCHECK_EQ(kPriority, pattern->priority);
}

}  // namespace

}  // namespace network::cors
