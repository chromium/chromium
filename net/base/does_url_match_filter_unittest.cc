// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/does_url_match_filter.h"

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

namespace {

const base::flat_set<url::Origin> origins = {
    url::Origin::Create(GURL("http://www.origin1.com:8080")),
    url::Origin::Create(GURL("http://www.origin2.com:80")),
    url::Origin::Create(GURL("http://www.origin3.com:80")),
    url::Origin::Create(GURL("http://www.origin4.com:80"))};
const base::flat_set<std::string> domains = {"domain1.com", "domain2.com",
                                             "domain3.com", "domain4.com"};

using DoesUrlMatchFilterTest = testing::Test;

TEST_F(DoesUrlMatchFilterTest, EmptyOriginsAndDomainsLists) {
  base::flat_set<url::Origin> empty_origins;
  base::flat_set<std::string> empty_domains;

  const GURL url("http://www.test.com");

  // A url shall not match an empty sets of origins and domains, and a
  // kFalseIfMatches filter shall return true.
  EXPECT_TRUE(DoesUrlMatchFilter(UrlFilterType::kFalseIfMatches, empty_origins,
                                 empty_domains, url));

  // A url shall not match an empty sets of origins and domains, and a
  // kTrueIfMatches filter shall return false.
  EXPECT_FALSE(DoesUrlMatchFilter(UrlFilterType::kTrueIfMatches, empty_origins,
                                  empty_domains, url));
}

TEST_F(DoesUrlMatchFilterTest, UrlDoesNotMatchOriginsOrDomains) {
  const GURL url("http://www.test.com");

  // A url that does not match a kFalseIfMatches filter shall be removed
  EXPECT_TRUE(DoesUrlMatchFilter(UrlFilterType::kFalseIfMatches, origins,
                                 domains, url));

  // A url that does not match a kTrueIfMatches filter shall not be removed
  EXPECT_FALSE(
      DoesUrlMatchFilter(UrlFilterType::kTrueIfMatches, origins, domains, url));
}

TEST_F(DoesUrlMatchFilterTest, UrlMatchesOrigin) {
  const GURL match_with_origin("http://www.origin3.com");

  // A url that matches a kFalseIfMatches filter shall not be removed
  EXPECT_FALSE(DoesUrlMatchFilter(UrlFilterType::kFalseIfMatches, origins,
                                  domains, match_with_origin));

  // A url that matches a kTrueIfMatches filter shall be removed
  EXPECT_TRUE(DoesUrlMatchFilter(UrlFilterType::kTrueIfMatches, origins,
                                 domains, match_with_origin));
}

TEST_F(DoesUrlMatchFilterTest, UrlMatchesDomain) {
  const GURL match_with_domain("http://A.domain2.com:80");

  // A url that matches a kFalseIfMatches filter shall not be removed
  EXPECT_FALSE(DoesUrlMatchFilter(UrlFilterType::kFalseIfMatches, origins,
                                  domains, match_with_domain));

  // A url that matches a kTrueIfMatches filter shall be removed
  EXPECT_TRUE(DoesUrlMatchFilter(UrlFilterType::kTrueIfMatches, origins,
                                 domains, match_with_domain));
}

TEST_F(DoesUrlMatchFilterTest, IPv4AddressMatches) {
  const GURL match_with_domain("http://192.168.0.1:8080/");
  const base::flat_set<std::string> ipv4_domains = {"192.168.0.1"};

  // A url that matches a kFalseIfMatches filter shall not be removed
  EXPECT_FALSE(DoesUrlMatchFilter(UrlFilterType::kFalseIfMatches, {},
                                  ipv4_domains, match_with_domain));

  // A url that matches a kTrueIfMatches filter shall be removed
  EXPECT_TRUE(DoesUrlMatchFilter(UrlFilterType::kTrueIfMatches, {},
                                 ipv4_domains, match_with_domain));
}

TEST_F(DoesUrlMatchFilterTest, IPv6AddressMatches) {
  const GURL match_with_domain("https://[2001:db8::1]/");
  const base::flat_set<std::string> ipv6_domains = {"[2001:db8::1]"};

  // A url that matches a kFalseIfMatches filter shall not be removed
  EXPECT_FALSE(DoesUrlMatchFilter(UrlFilterType::kFalseIfMatches, {},
                                  ipv6_domains, match_with_domain));

  // A url that matches a kTrueIfMatches filter shall be removed
  EXPECT_TRUE(DoesUrlMatchFilter(UrlFilterType::kTrueIfMatches, {},
                                 ipv6_domains, match_with_domain));
}

TEST_F(DoesUrlMatchFilterTest, LocalhostMatches) {
  const GURL match_with_domain("http://localhost/");
  const base::flat_set<std::string> localhost = {"localhost"};

  // A url that matches a kFalseIfMatches filter shall not be removed
  EXPECT_FALSE(DoesUrlMatchFilter(UrlFilterType::kFalseIfMatches, {}, localhost,
                                  match_with_domain));

  // A url that matches a kTrueIfMatches filter shall be removed
  EXPECT_TRUE(DoesUrlMatchFilter(UrlFilterType::kTrueIfMatches, {}, localhost,
                                 match_with_domain));
}

TEST_F(DoesUrlMatchFilterTest, BareHostnameMatches) {
  const GURL match_with_domain("http://go/");
  const base::flat_set<std::string> bare_hostname = {"go"};

  // A url that matches a kFalseIfMatches filter shall not be removed
  EXPECT_FALSE(DoesUrlMatchFilter(UrlFilterType::kFalseIfMatches, {},
                                  bare_hostname, match_with_domain));

  // A url that matches a kTrueIfMatches filter shall be removed
  EXPECT_TRUE(DoesUrlMatchFilter(UrlFilterType::kTrueIfMatches, {},
                                 bare_hostname, match_with_domain));
}

TEST_F(DoesUrlMatchFilterTest, BlobUrlMatchesOrigin) {
  const GURL match_with_origin(
      "blob:http://www.origin3.com/550e8400-e29b-41d4-a716-446655440000");

  EXPECT_TRUE(DoesUrlMatchFilter(UrlFilterType::kTrueIfMatches, origins,
                                 domains, match_with_origin));
}

TEST_F(DoesUrlMatchFilterTest, BlobUrlMatchesDomain) {
  const GURL match_with_domain(
      "blob:http://a.domain2.com/550e8400-e29b-41d4-a716-446655440000");

  EXPECT_TRUE(DoesUrlMatchFilter(UrlFilterType::kTrueIfMatches, origins,
                                 domains, match_with_domain));
}

TEST_F(DoesUrlMatchFilterTest, BlobUrlMismatch) {
  const GURL nomatch(
      "blob:http://mismatch.example/550e8400-e29b-41d4-a716-446655440000");

  EXPECT_FALSE(DoesUrlMatchFilter(UrlFilterType::kTrueIfMatches, origins,
                                  domains, nomatch));
}

TEST_F(DoesUrlMatchFilterTest, FilesystemUrlMatchesOrigin) {
  const GURL match_with_origin(
      "filesystem:http://www.origin2.com/temporary/test-file.txt");

  EXPECT_TRUE(DoesUrlMatchFilter(UrlFilterType::kTrueIfMatches, origins,
                                 domains, match_with_origin));
}

TEST_F(DoesUrlMatchFilterTest, FilesystemUrlMatchesDomain) {
  const GURL match_with_domain(
      "filesystem:http://any.domain3.com/temporary/test-file.txt");

  EXPECT_TRUE(DoesUrlMatchFilter(UrlFilterType::kTrueIfMatches, origins,
                                 domains, match_with_domain));
}

TEST_F(DoesUrlMatchFilterTest, FilesystemUrlMismatch) {
  const GURL nomatch(
      "filesystem:http://mismatch.example/temporary/test-file.txt");

  EXPECT_FALSE(DoesUrlMatchFilter(UrlFilterType::kTrueIfMatches, origins,
                                  domains, nomatch));
}

}  // namespace

}  // namespace net
