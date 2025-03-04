// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/does_url_match_filter.h"

#include <set>
#include <string>
#include <vector>

#include "base/test/task_environment.h"
#include "net/base/network_isolation_key.h"
#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

namespace {

std::set<url::Origin> origins = {
    url::Origin::Create(GURL("http://www.origin1.com:8080")),
    url::Origin::Create(GURL("http://www.origin2.com:80")),
    url::Origin::Create(GURL("http://www.origin3.com:80")),
    url::Origin::Create(GURL("http://www.origin4.com:80"))};
std::set<std::string> domains = {"domain1.com", "domain2.com", "domain3.com",
                                 "domain4.com"};

using DoesUrlMatchFilterTest = testing::Test;

TEST_F(DoesUrlMatchFilterTest, EmptyOriginsAndDomainsLists) {
  std::set<url::Origin> empty_origins;
  std::set<std::string> empty_domains;

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

}  // namespace

}  // namespace net
