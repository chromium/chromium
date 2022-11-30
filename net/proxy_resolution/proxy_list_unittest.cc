// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_list.h"

#include <vector>

#include "net/base/net_errors.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_retry_info.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsOk;

namespace net {

namespace {

// Test parsing from a PAC string.
TEST(ProxyListTest, SetFromPacString) {
  const struct {
    const char* pac_input;
    const char* pac_output;
  } tests[] = {
    // Valid inputs:
    {  "PROXY foopy:10",
       "PROXY foopy:10",
    },
    {  " DIRECT",  // leading space.
       "DIRECT",
    },
    {  "PROXY foopy1 ; proxy foopy2;\t DIRECT",
       "PROXY foopy1:80;PROXY foopy2:80;DIRECT",
    },
    {  "proxy foopy1 ; SOCKS foopy2",
       "PROXY foopy1:80;SOCKS foopy2:1080",
    },
    // Try putting DIRECT first.
    {  "DIRECT ; proxy foopy1 ; DIRECT ; SOCKS5 foopy2;DIRECT ",
       "DIRECT;PROXY foopy1:80;DIRECT;SOCKS5 foopy2:1080;DIRECT",
    },
    // Try putting DIRECT consecutively.
    {  "DIRECT ; proxy foopy1:80; DIRECT ; DIRECT",
       "DIRECT;PROXY foopy1:80;DIRECT;DIRECT",
    },

    // Invalid inputs (parts which aren't understood get
    // silently discarded):
    //
    // If the proxy list string parsed to empty, automatically fall-back to
    // DIRECT.
    {  "PROXY-foopy:10",
       "DIRECT",
    },
    {  "PROXY",
       "DIRECT",
    },
    {  "PROXY foopy1 ; JUNK ; JUNK ; SOCKS5 foopy2 ; ;",
       "PROXY foopy1:80;SOCKS5 foopy2:1080",
    },
  };

  for (const auto& test : tests) {
    ProxyList list;
    list.SetFromPacString(test.pac_input);
    EXPECT_EQ(test.pac_output, list.ToPacString());
    EXPECT_FALSE(list.IsEmpty());
  }
}

TEST(ProxyListTest, RemoveProxiesWithoutScheme) {
  const struct {
    const char* pac_input;
    int filter;
    const char* filtered_pac_output;
  } tests[] = {
    {  "PROXY foopy:10 ; SOCKS5 foopy2 ; SOCKS foopy11 ; PROXY foopy3 ; DIRECT",
       // Remove anything that isn't HTTP or DIRECT.
       ProxyServer::SCHEME_DIRECT | ProxyServer::SCHEME_HTTP,
       "PROXY foopy:10;PROXY foopy3:80;DIRECT",
    },
    {  "PROXY foopy:10 ; SOCKS5 foopy2",
       // Remove anything that isn't HTTP or SOCKS5.
       ProxyServer::SCHEME_DIRECT | ProxyServer::SCHEME_SOCKS4,
       "",
    },
  };

  for (const auto& test : tests) {
    ProxyList list;
    list.SetFromPacString(test.pac_input);
    list.RemoveProxiesWithoutScheme(test.filter);
    EXPECT_EQ(test.filtered_pac_output, list.ToPacString());
  }
}

TEST(ProxyListTest, DeprioritizeBadProxies) {
  // Retry info that marks a proxy as being bad for a *very* long time (to avoid
  // the test depending on the current time.)
  ProxyRetryInfo proxy_retry_info;
  proxy_retry_info.bad_until = base::TimeTicks::Now() + base::Days(1);

  // Call DeprioritizeBadProxies with an empty map -- should have no effect.
  {
    ProxyList list;
    list.SetFromPacString("PROXY foopy1:80;PROXY foopy2:80;PROXY foopy3:80");

    ProxyRetryInfoMap retry_info_map;
    list.DeprioritizeBadProxies(retry_info_map);
    EXPECT_EQ("PROXY foopy1:80;PROXY foopy2:80;PROXY foopy3:80",
              list.ToPacString());
  }

  // Call DeprioritizeBadProxies with 2 of the three proxies marked as bad.
  // These proxies should be retried last.
  {
    ProxyList list;
    list.SetFromPacString("PROXY foopy1:80;PROXY foopy2:80;PROXY foopy3:80");

    ProxyRetryInfoMap retry_info_map;
    retry_info_map["foopy1:80"] = proxy_retry_info;
    retry_info_map["foopy3:80"] = proxy_retry_info;
    retry_info_map["socks5://localhost:1080"] = proxy_retry_info;

    list.DeprioritizeBadProxies(retry_info_map);

    EXPECT_EQ("PROXY foopy2:80;PROXY foopy1:80;PROXY foopy3:80",
              list.ToPacString());
  }

  // Call DeprioritizeBadProxies where ALL of the proxies are marked as bad.
  // This should have no effect on the order.
  {
    ProxyList list;
    list.SetFromPacString("PROXY foopy1:80;PROXY foopy2:80;PROXY foopy3:80");

    ProxyRetryInfoMap retry_info_map;
    retry_info_map["foopy1:80"] = proxy_retry_info;
    retry_info_map["foopy2:80"] = proxy_retry_info;
    retry_info_map["foopy3:80"] = proxy_retry_info;

    list.DeprioritizeBadProxies(retry_info_map);

    EXPECT_EQ("PROXY foopy1:80;PROXY foopy2:80;PROXY foopy3:80",
              list.ToPacString());
  }

  // Call DeprioritizeBadProxies with 2 of the three proxies marked as bad. Of
  // the 2 bad proxies, one is to be reconsidered and should be retried last.
  // The other is not to be reconsidered and should be removed from the list.
  {
    ProxyList list;
    list.SetFromPacString("PROXY foopy1:80;PROXY foopy2:80;PROXY foopy3:80");

    ProxyRetryInfoMap retry_info_map;
    // |proxy_retry_info.reconsider defaults to true.
    retry_info_map["foopy1:80"] = proxy_retry_info;
    proxy_retry_info.try_while_bad = false;
    retry_info_map["foopy3:80"] = proxy_retry_info;
    proxy_retry_info.try_while_bad = true;
    retry_info_map["socks5://localhost:1080"] = proxy_retry_info;

    list.DeprioritizeBadProxies(retry_info_map);

    EXPECT_EQ("PROXY foopy2:80;PROXY foopy1:80",
              list.ToPacString());
  }
}

TEST(ProxyListTest, UpdateRetryInfoOnFallback) {
  // Retrying should put the first proxy on the retry list.
  {
    ProxyList list;
    ProxyRetryInfoMap retry_info_map;
    NetLogWithSource net_log;
    ProxyServer proxy_server(
        ProxyUriToProxyServer("foopy1:80", ProxyServer::SCHEME_HTTP));
    std::vector<ProxyServer> bad_proxies;
    bad_proxies.push_back(proxy_server);
    list.SetFromPacString("PROXY foopy1:80;PROXY foopy2:80;PROXY foopy3:80");
    list.UpdateRetryInfoOnFallback(&retry_info_map, base::Seconds(60), true,
                                   bad_proxies, ERR_PROXY_CONNECTION_FAILED,
                                   net_log);
    EXPECT_TRUE(retry_info_map.end() != retry_info_map.find("foopy1:80"));
    EXPECT_EQ(ERR_PROXY_CONNECTION_FAILED,
              retry_info_map[ProxyServerToProxyUri(proxy_server)].net_error);
    EXPECT_TRUE(retry_info_map.end() == retry_info_map.find("foopy2:80"));
    EXPECT_TRUE(retry_info_map.end() == retry_info_map.find("foopy3:80"));
  }
  // Retrying should put the first proxy on the retry list, even if there
  // was no network error.
  {
    ProxyList list;
    ProxyRetryInfoMap retry_info_map;
    NetLogWithSource net_log;
    ProxyServer proxy_server(
        ProxyUriToProxyServer("foopy1:80", ProxyServer::SCHEME_HTTP));
    std::vector<ProxyServer> bad_proxies;
    bad_proxies.push_back(proxy_server);
    list.SetFromPacString("PROXY foopy1:80;PROXY foopy2:80;PROXY foopy3:80");
    list.UpdateRetryInfoOnFallback(&retry_info_map, base::Seconds(60), true,
                                   bad_proxies, OK, net_log);
    EXPECT_TRUE(retry_info_map.end() != retry_info_map.find("foopy1:80"));
    EXPECT_THAT(retry_info_map[ProxyServerToProxyUri(proxy_server)].net_error,
                IsOk());
    EXPECT_TRUE(retry_info_map.end() == retry_info_map.find("foopy2:80"));
    EXPECT_TRUE(retry_info_map.end() == retry_info_map.find("foopy3:80"));
  }
  // Including another bad proxy should put both the first and the specified
  // proxy on the retry list.
  {
    ProxyList list;
    ProxyRetryInfoMap retry_info_map;
    NetLogWithSource net_log;
    ProxyServer proxy_server =
        ProxyUriToProxyServer("foopy3:80", ProxyServer::SCHEME_HTTP);
    std::vector<ProxyServer> bad_proxies;
    bad_proxies.push_back(proxy_server);
    list.SetFromPacString("PROXY foopy1:80;PROXY foopy2:80;PROXY foopy3:80");
    list.UpdateRetryInfoOnFallback(&retry_info_map, base::Seconds(60), true,
                                   bad_proxies, ERR_NAME_RESOLUTION_FAILED,
                                   net_log);
    EXPECT_TRUE(retry_info_map.end() != retry_info_map.find("foopy1:80"));
    EXPECT_EQ(ERR_NAME_RESOLUTION_FAILED,
              retry_info_map[ProxyServerToProxyUri(proxy_server)].net_error);
    EXPECT_TRUE(retry_info_map.end() == retry_info_map.find("foopy2:80"));
    EXPECT_TRUE(retry_info_map.end() != retry_info_map.find("foopy3:80"));
  }
  // If the first proxy is DIRECT, nothing is added to the retry list, even
  // if another bad proxy is specified.
  {
    ProxyList list;
    ProxyRetryInfoMap retry_info_map;
    NetLogWithSource net_log;
    ProxyServer proxy_server =
        ProxyUriToProxyServer("foopy2:80", ProxyServer::SCHEME_HTTP);
    std::vector<ProxyServer> bad_proxies;
    bad_proxies.push_back(proxy_server);
    list.SetFromPacString("DIRECT;PROXY foopy2:80;PROXY foopy3:80");
    list.UpdateRetryInfoOnFallback(&retry_info_map, base::Seconds(60), true,
                                   bad_proxies, OK, net_log);
    EXPECT_TRUE(retry_info_map.end() == retry_info_map.find("foopy2:80"));
    EXPECT_TRUE(retry_info_map.end() == retry_info_map.find("foopy3:80"));
  }
  // If the bad proxy is already on the retry list, and the old retry info would
  // cause the proxy to be retried later than the newly specified retry info,
  // then the old retry info should be kept.
  {
    ProxyList list;
    ProxyRetryInfoMap retry_info_map;
    NetLogWithSource net_log;
    list.SetFromPacString("PROXY foopy1:80;PROXY foopy2:80;PROXY foopy3:80");

    // First, mark the proxy as bad for 60 seconds.
    list.UpdateRetryInfoOnFallback(&retry_info_map, base::Seconds(60), true,
                                   std::vector<ProxyServer>(),
                                   ERR_PROXY_CONNECTION_FAILED, net_log);
    // Next, mark the same proxy as bad for 1 second. This call should have no
    // effect, since this would cause the bad proxy to be retried sooner than
    // the existing retry info.
    list.UpdateRetryInfoOnFallback(&retry_info_map, base::Seconds(1), false,
                                   std::vector<ProxyServer>(), OK, net_log);
    EXPECT_TRUE(retry_info_map.end() != retry_info_map.find("foopy1:80"));
    EXPECT_EQ(ERR_PROXY_CONNECTION_FAILED,
              retry_info_map["foopy1:80"].net_error);
    EXPECT_TRUE(retry_info_map["foopy1:80"].try_while_bad);
    EXPECT_EQ(base::Seconds(60), retry_info_map["foopy1:80"].current_delay);
    EXPECT_GT(retry_info_map["foopy1:80"].bad_until,
              base::TimeTicks::Now() + base::Seconds(30));
    EXPECT_TRUE(retry_info_map.end() == retry_info_map.find("foopy2:80"));
    EXPECT_TRUE(retry_info_map.end() == retry_info_map.find("foopy3:80"));
  }
  // If the bad proxy is already on the retry list, and the newly specified
  // retry info would cause the proxy to be retried later than the old retry
  // info, then the old retry info should be replaced with the new retry info.
  {
    ProxyList list;
    ProxyRetryInfoMap retry_info_map;
    NetLogWithSource net_log;
    list.SetFromPacString("PROXY foopy1:80;PROXY foopy2:80;PROXY foopy3:80");

    // First, mark the proxy as bad for 1 second.
    list.UpdateRetryInfoOnFallback(&retry_info_map, base::Seconds(1), false,
                                   std::vector<ProxyServer>(), OK, net_log);
    // Next, mark the same proxy as bad for 60 seconds. This call should replace
    // the existing retry info with the new 60 second retry info.
    list.UpdateRetryInfoOnFallback(&retry_info_map, base::Seconds(60), true,
                                   std::vector<ProxyServer>(),
                                   ERR_PROXY_CONNECTION_FAILED, net_log);

    EXPECT_TRUE(retry_info_map.end() != retry_info_map.find("foopy1:80"));
    EXPECT_EQ(ERR_PROXY_CONNECTION_FAILED,
              retry_info_map["foopy1:80"].net_error);
    EXPECT_TRUE(retry_info_map["foopy1:80"].try_while_bad);
    EXPECT_EQ(base::Seconds(60), retry_info_map["foopy1:80"].current_delay);
    EXPECT_GT(retry_info_map["foopy1:80"].bad_until,
              base::TimeTicks::Now() + base::Seconds(30));
    EXPECT_TRUE(retry_info_map.end() == retry_info_map.find("foopy2:80"));
    EXPECT_TRUE(retry_info_map.end() == retry_info_map.find("foopy3:80"));
  }
}

}  // anonymous namespace

}  // namespace net
