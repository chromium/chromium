// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_filter.h"

#include <memory>
#include <ostream>

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

static const char* const server_allowlist_array[] = {
    "google.com", "linkedin.com", "book.com", ".chromium.org", ".gag", "gog"};

enum { ALL_SERVERS_MATCH = (1 << base::size(server_allowlist_array)) - 1 };

struct UrlData {
  GURL url;
  HttpAuth::Target target;
  bool matches;
  int match_bits;
};

static const UrlData urls[] = {
  { GURL(std::string()), HttpAuth::AUTH_NONE, false, 0 },
  { GURL("http://foo.cn"), HttpAuth::AUTH_PROXY, true, ALL_SERVERS_MATCH },
  { GURL("http://foo.cn"), HttpAuth::AUTH_SERVER, false, 0 },
  { GURL("http://slashdot.org"), HttpAuth::AUTH_NONE, false, 0 },
  { GURL("http://www.google.com"), HttpAuth::AUTH_SERVER, true, 1 << 0 },
  { GURL("http://www.google.com"), HttpAuth::AUTH_PROXY, true,
    ALL_SERVERS_MATCH },
  { GURL("https://login.facebook.com/login.php?login_attempt=1"),
    HttpAuth::AUTH_NONE, false, 0 },
  { GURL("http://codereview.chromium.org/634002/show"), HttpAuth::AUTH_SERVER,
    true, 1 << 3 },
  { GURL("http://code.google.com/p/chromium/issues/detail?id=34505"),
    HttpAuth::AUTH_SERVER, true, 1 << 0 },
  { GURL("http://code.google.com/p/chromium/issues/list?can=2&q=label:"
         "spdy&sort=owner&colspec=ID%20Stars%20Pri%20Area%20Type%20Status%20"
         "Summary%20Modified%20Owner%20Mstone%20OS"),
    HttpAuth::AUTH_SERVER, true, 1 << 3 },
  { GURL("https://www.linkedin.com/secure/login?trk=hb_signin"),
    HttpAuth::AUTH_SERVER, true, 1 << 1 },
  { GURL("http://www.linkedin.com/mbox?displayMBoxItem=&"
         "itemID=I1717980652_2&trk=COMM_HP_MSGVW_MEBC_MEBC&goback=.hom"),
    HttpAuth::AUTH_SERVER, true, 1 << 1 },
  { GURL("http://news.slashdot.org/story/10/02/18/190236/"
         "New-Plan-Lets-Top-HS-Students-Graduate-2-Years-Early"),
    HttpAuth::AUTH_PROXY, true, ALL_SERVERS_MATCH },
  { GURL("http://codereview.chromium.org/646068/diff/4001/5003"),
    HttpAuth::AUTH_SERVER, true, 1 << 3 },
  { GURL("http://codereview.chromium.gag/646068/diff/4001/5003"),
    HttpAuth::AUTH_SERVER, true, 1 << 4 },
  { GURL("http://codereview.chromium.gog/646068/diff/4001/5003"),
    HttpAuth::AUTH_SERVER, true, 1 << 5 },
};

}   // namespace

TEST(HttpAuthFilterTest, EmptyFilter) {
  // Create an empty filter
  HttpAuthFilterAllowlist filter((std::string()));
  for (size_t i = 0; i < base::size(urls); i++) {
    EXPECT_EQ(urls[i].target == HttpAuth::AUTH_PROXY,
              filter.IsValid(urls[i].url, urls[i].target))
        << " " << i << ": " << urls[i].url;
  }
}

TEST(HttpAuthFilterTest, NonEmptyFilter) {
  // Create an non-empty filter
  std::string server_allowlist_filter_string;
  for (size_t i = 0; i < base::size(server_allowlist_array); ++i) {
    if (!server_allowlist_filter_string.empty())
      server_allowlist_filter_string += ",";
    server_allowlist_filter_string += "*";
    server_allowlist_filter_string += server_allowlist_array[i];
  }
  HttpAuthFilterAllowlist filter(server_allowlist_filter_string);
  for (size_t i = 0; i < base::size(urls); i++) {
    EXPECT_EQ(urls[i].matches, filter.IsValid(urls[i].url, urls[i].target))
        << " " << i << ": " << urls[i].url;
  }
}

}   // namespace net
