// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_filter.h"

#include <memory>
#include <ostream>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace net {

namespace {

static const char* const server_allowlist_array[] = {
    "google.com", "linkedin.com", "book.com", ".chromium.org", ".gag", "gog"};

struct SchemeHostPortData {
  url::SchemeHostPort scheme_host_port;
  HttpAuth::Target target;
  bool matches;
};

static const SchemeHostPortData kTestCases[] = {
    {url::SchemeHostPort(), HttpAuth::AUTH_NONE, false},
    {url::SchemeHostPort(GURL("http://foo.cn")), HttpAuth::AUTH_PROXY, true},
    {url::SchemeHostPort(GURL("http://foo.cn")), HttpAuth::AUTH_SERVER, false},
    {url::SchemeHostPort(GURL("http://slashdot.org")), HttpAuth::AUTH_NONE,
     false},
    {url::SchemeHostPort(GURL("http://www.google.com")), HttpAuth::AUTH_SERVER,
     true},
    {url::SchemeHostPort(GURL("http://www.google.com")), HttpAuth::AUTH_PROXY,
     true},
    {url::SchemeHostPort(GURL("https://login.facebook.com")),
     HttpAuth::AUTH_NONE, false},
    {url::SchemeHostPort(GURL("http://codereview.chromium.org")),
     HttpAuth::AUTH_SERVER, true},
    {url::SchemeHostPort(GURL("http://code.google.com")), HttpAuth::AUTH_SERVER,
     true},
    {url::SchemeHostPort(GURL("https://www.linkedin.com")),
     HttpAuth::AUTH_SERVER, true},
    {url::SchemeHostPort(GURL("http://news.slashdot.org")),
     HttpAuth::AUTH_PROXY, true},
    {url::SchemeHostPort(GURL("http://codereview.chromium.org")),
     HttpAuth::AUTH_SERVER, true},
    {url::SchemeHostPort(GURL("http://codereview.chromium.gag")),
     HttpAuth::AUTH_SERVER, true},
    {url::SchemeHostPort(GURL("http://codereview.chromium.gog")),
     HttpAuth::AUTH_SERVER, true},
};

}   // namespace

TEST(HttpAuthFilterTest, EmptyFilter) {
  // Create an empty filter
  HttpAuthFilterAllowlist filter((std::string()));
  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.target == HttpAuth::AUTH_PROXY,
              filter.IsValid(test_case.scheme_host_port, test_case.target))
        << test_case.scheme_host_port.Serialize();
  }
}

TEST(HttpAuthFilterTest, NonEmptyFilter) {
  // Create an non-empty filter
  std::string server_allowlist_filter_string;
  for (const auto* server : server_allowlist_array) {
    if (!server_allowlist_filter_string.empty())
      server_allowlist_filter_string += ",";
    server_allowlist_filter_string += "*";
    server_allowlist_filter_string += server;
  }
  HttpAuthFilterAllowlist filter(server_allowlist_filter_string);
  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.matches,
              filter.IsValid(test_case.scheme_host_port, test_case.target))
        << test_case.scheme_host_port.Serialize();
  }
}

}   // namespace net
