// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_list.h"

#include <vector>

#include "base/logging.h"
#include "build/buildflag.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/log/net_log_with_source.h"
#include "net/net_buildflags.h"
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
    const char* debug_output;
  } tests[] = {
      // Valid inputs:
      {
          "PROXY foopy:10",
          "PROXY foopy:10",
      },
      {
          " DIRECT",  // leading space.
          "DIRECT",
      },
      {
          "PROXY foopy1 ; proxy foopy2;\t DIRECT",
          "PROXY foopy1:80;PROXY foopy2:80;DIRECT",
      },
      {
          "proxy foopy1 ; SOCKS foopy2",
          "PROXY foopy1:80;SOCKS foopy2:1080",
      },
      // Try putting DIRECT first.
      {
          "DIRECT ; proxy foopy1 ; DIRECT ; SOCKS5 foopy2;DIRECT ",
          "DIRECT;PROXY foopy1:80;DIRECT;SOCKS5 foopy2:1080;DIRECT",
      },
      // Try putting DIRECT consecutively.
      {
          "DIRECT ; proxy foopy1:80; DIRECT ; DIRECT",
          "DIRECT;PROXY foopy1:80;DIRECT;DIRECT",
      },

      // Invalid inputs (parts which aren't understood get
      // silently discarded):
      //
      // If the proxy list string parsed to empty, automatically fall-back to
      // DIRECT.
      {
          "PROXY-foopy:10",
          "DIRECT",
      },
      {
          "PROXY",
          "DIRECT",
      },
      {
          "PROXY foopy1 ; JUNK ; JUNK ; SOCKS5 foopy2 ; ;",
          "PROXY foopy1:80;SOCKS5 foopy2:1080",
      },
  };

  for (const auto& test : tests) {
    ProxyList list;
    list.SetFromPacString(test.pac_input);
    EXPECT_EQ(test.debug_output, list.ToDebugString());
    EXPECT_FALSE(list.IsEmpty());
  }
}

TEST(ProxyListTest, RemoveProxiesWithoutScheme) {
  const struct {
    const char* pac_input;
    int filter;
    const char* filtered_debug_output;
  } tests[] = {
      {
          "PROXY foopy:10 ; SOCKS5 foopy2 ; SOCKS foopy11 ; PROXY foopy3 ; "
          "DIRECT",
          // Remove anything that isn't HTTP.
          ProxyServer::SCHEME_HTTP,
          "PROXY foopy:10;PROXY foopy3:80;DIRECT",
      },
      {
          "PROXY foopy:10 ; SOCKS5 foopy2",
          // Remove anything that isn't HTTP or SOCKS5.
          ProxyServer::SCHEME_SOCKS4,
          "",
      },
  };

  for (const auto& test : tests) {
    ProxyList list;
    list.SetFromPacString(test.pac_input);
    list.RemoveProxiesWithoutScheme(test.filter);
    EXPECT_EQ(test.filtered_debug_output, list.ToDebugString());
  }
}

TEST(ProxyListTest, RemoveProxiesWithoutSchemeWithProxyChains) {
  const auto kProxyChainFooHttps = ProxyChain::ForIpProtection({
      ProxyServer::FromSchemeHostAndPort(ProxyServer::Scheme::SCHEME_HTTPS,
                                         "foo-a", 443),
      ProxyServer::FromSchemeHostAndPort(ProxyServer::Scheme::SCHEME_HTTPS,
                                         "foo-b", 443),
  });
  const auto kProxyChainBarMixed = ProxyChain::ForIpProtection({
      ProxyServer::FromSchemeHostAndPort(ProxyServer::Scheme::SCHEME_QUIC,
                                         "bar-a", 443),
      ProxyServer::FromSchemeHostAndPort(ProxyServer::Scheme::SCHEME_HTTPS,
                                         "bar-b", 443),
  });
  const ProxyChain kProxyChainGraultSocks = ProxyChain::FromSchemeHostAndPort(
      ProxyServer::Scheme::SCHEME_SOCKS4, "grault", 443);

  ProxyList list;
  list.AddProxyChain(kProxyChainFooHttps);
  list.AddProxyChain(kProxyChainBarMixed);
  list.AddProxyChain(kProxyChainGraultSocks);
  list.AddProxyChain(ProxyChain::Direct());

  // Remove anything that isn't entirely HTTPS.
  list.RemoveProxiesWithoutScheme(ProxyServer::SCHEME_HTTPS);

  std::vector<net::ProxyChain> expected = {
      kProxyChainFooHttps,
      ProxyChain::Direct(),
  };
  EXPECT_EQ(list.AllChains(), expected);
}

TEST(ProxyListTest, DeprioritizeBadProxyChains) {
  // Retry info that marks a proxy as being bad for a *very* long time (to avoid
  // the test depending on the current time.)
  ProxyRetryInfo proxy_retry_info;
  proxy_retry_info.bad_until = base::TimeTicks::Now() + base::Days(1);

  // Call DeprioritizeBadProxyChains with an empty map -- should have no effect.
  {
    ProxyList list;
    list.SetFromPacString("PROXY foopy1:80;PROXY foopy2:80;PROXY foopy3:80");

    ProxyRetryInfoMap retry_info_map;
    list.DeprioritizeBadProxyChains(retry_info_map);
    EXPECT_EQ("PROXY foopy1:80;PROXY foopy2:80;PROXY foopy3:80",
              list.ToDebugString());
  }

  // Call DeprioritizeBadProxyChains with 2 of the three chains marked as bad.
  // These proxies should be retried last.
  {
    ProxyList list;
    list.SetFromPacString("PROXY foopy1:80;PROXY foopy2:80;PROXY foopy3:80");

    ProxyRetryInfoMap retry_info_map;
    retry_info_map[ProxyUriToProxyChain(
        "foopy1:80", ProxyServer::SCHEME_HTTP)] = proxy_retry_info;
    retry_info_map[ProxyUriToProxyChain(
        "foopy3:80", ProxyServer::SCHEME_HTTP)] = proxy_retry_info;
    retry_info_map[ProxyUriToProxyChain("socks5://localhost:1080",
                                        ProxyServer::SCHEME_HTTP)] =
        proxy_retry_info;

    list.DeprioritizeBadProxyChains(retry_info_map);

    EXPECT_EQ("PROXY foopy2:80;PROXY foopy1:80;PROXY foopy3:80",
              list.ToDebugString());
  }

  // Call DeprioritizeBadProxyChains where ALL of the chains are marked as bad.
  // This should have no effect on the order.
  {
    ProxyList list;
    list.SetFromPacString("PROXY foopy1:80;PROXY foopy2:80;PROXY foopy3:80");

    ProxyRetryInfoMap retry_info_map;
    retry_info_map[ProxyUriToProxyChain(
        "foopy1:80", ProxyServer::SCHEME_HTTP)] = proxy_retry_info;
    retry_info_map[ProxyUriToProxyChain(
        "foopy2:80", ProxyServer::SCHEME_HTTP)] = proxy_retry_info;
    retry_info_map[ProxyUriToProxyChain(
        "foopy3:80", ProxyServer::SCHEME_HTTP)] = proxy_retry_info;

    list.DeprioritizeBadProxyChains(retry_info_map);

    EXPECT_EQ("PROXY foopy1:80;PROXY foopy2:80;PROXY foopy3:80",
              list.ToDebugString());
  }

  // Call DeprioritizeBadProxyChains with 2 of the three chains marked as bad.
  // Of the 2 bad proxies, one is to be reconsidered and should be retried last.
  // The other is not to be reconsidered and should be removed from the list.
  {
    ProxyList list;
    list.SetFromPacString("PROXY foopy1:80;PROXY foopy2:80;PROXY foopy3:80");

    ProxyRetryInfoMap retry_info_map;
    // |proxy_retry_info.reconsider defaults to true.
    retry_info_map[ProxyUriToProxyChain(
        "foopy1:80", ProxyServer::SCHEME_HTTP)] = proxy_retry_info;
    proxy_retry_info.try_while_bad = false;
    retry_info_map[ProxyUriToProxyChain(
        "foopy3:80", ProxyServer::SCHEME_HTTP)] = proxy_retry_info;
    proxy_retry_info.try_while_bad = true;
    retry_info_map[ProxyUriToProxyChain("socks5://localhost:1080",
                                        ProxyServer::SCHEME_SOCKS5)] =
        proxy_retry_info;

    list.DeprioritizeBadProxyChains(retry_info_map);

    EXPECT_EQ("PROXY foopy2:80;PROXY foopy1:80", list.ToDebugString());
  }
}

TEST(ProxyListTest, UpdateRetryInfoOnFallback) {
  // Retrying should put the first proxy on the retry list.
  {
    ProxyList list;
    ProxyRetryInfoMap retry_info_map;
    NetLogWithSource net_log;
    ProxyChain proxy_chain(
        ProxyUriToProxyChain("foopy1:80", ProxyServer::SCHEME_HTTP));
    std::vector<ProxyChain> bad_proxies;
    bad_proxies.push_back(proxy_chain);
    list.SetFromPacString("PROXY foopy1:80;PROXY foopy2:80;PROXY foopy3:80");
    list.UpdateRetryInfoOnFallback(&retry_info_map, base::Seconds(60), true,
                                   bad_proxies, ERR_PROXY_CONNECTION_FAILED,
                                   net_log);
    EXPECT_TRUE(retry_info_map.end() != retry_info_map.find(proxy_chain));
    EXPECT_EQ(ERR_PROXY_CONNECTION_FAILED,
              retry_info_map[proxy_chain].net_error);
    EXPECT_TRUE(retry_info_map.end() ==
                retry_info_map.find(ProxyUriToProxyChain(
                    "foopy2:80", ProxyServer::SCHEME_HTTP)));
    EXPECT_TRUE(retry_info_map.end() ==
                retry_info_map.find(ProxyUriToProxyChain(
                    "foopy3:80", ProxyServer::SCHEME_HTTP)));
  }
  // Retrying should put the first proxy on the retry list, even if there
  // was no network error.
  {
    ProxyList list;
    ProxyRetryInfoMap retry_info_map;
    NetLogWithSource net_log;
    ProxyChain proxy_chain(
        ProxyUriToProxyChain("foopy1:80", ProxyServer::SCHEME_HTTP));
    std::vector<ProxyChain> bad_proxies;
    bad_proxies.push_back(proxy_chain);
    list.SetFromPacString("PROXY foopy1:80;PROXY foopy2:80;PROXY foopy3:80");
    list.UpdateRetryInfoOnFallback(&retry_info_map, base::Seconds(60), true,
                                   bad_proxies, OK, net_log);
    EXPECT_TRUE(retry_info_map.end() != retry_info_map.find(proxy_chain));
    EXPECT_THAT(retry_info_map[proxy_chain].net_error, IsOk());
    EXPECT_TRUE(retry_info_map.end() ==
                retry_info_map.find(ProxyUriToProxyChain(
                    "foopy2:80", ProxyServer::SCHEME_HTTP)));
    EXPECT_TRUE(retry_info_map.end() ==
                retry_info_map.find(ProxyUriToProxyChain(
                    "foopy3:80", ProxyServer::SCHEME_HTTP)));
  }
  // Including another bad proxy should put both the first and the specified
  // proxy on the retry list.
  {
    ProxyList list;
    ProxyRetryInfoMap retry_info_map;
    NetLogWithSource net_log;
    ProxyChain proxy_chain(
        ProxyUriToProxyChain("foopy3:80", ProxyServer::SCHEME_HTTP));
    std::vector<ProxyChain> bad_proxies;
    bad_proxies.push_back(proxy_chain);
    list.SetFromPacString("PROXY foopy1:80;PROXY foopy2:80;PROXY foopy3:80");
    list.UpdateRetryInfoOnFallback(&retry_info_map, base::Seconds(60), true,
                                   bad_proxies, ERR_NAME_RESOLUTION_FAILED,
                                   net_log);
    EXPECT_TRUE(retry_info_map.end() !=
                retry_info_map.find(ProxyUriToProxyChain(
                    "foopy1:80", ProxyServer::SCHEME_HTTP)));
    EXPECT_EQ(ERR_NAME_RESOLUTION_FAILED,
              retry_info_map[proxy_chain].net_error);
    EXPECT_TRUE(retry_info_map.end() ==
                retry_info_map.find(ProxyUriToProxyChain(
                    "foopy2:80", ProxyServer::SCHEME_HTTP)));
    EXPECT_TRUE(retry_info_map.end() != retry_info_map.find(proxy_chain));
  }
  // If the first proxy is DIRECT, nothing is added to the retry list, even
  // if another bad proxy is specified.
  {
    ProxyList list;
    ProxyRetryInfoMap retry_info_map;
    NetLogWithSource net_log;
    ProxyChain proxy_chain(
        ProxyUriToProxyChain("foopy2:80", ProxyServer::SCHEME_HTTP));
    std::vector<ProxyChain> bad_proxies;
    bad_proxies.push_back(proxy_chain);
    list.SetFromPacString("DIRECT;PROXY foopy2:80;PROXY foopy3:80");
    list.UpdateRetryInfoOnFallback(&retry_info_map, base::Seconds(60), true,
                                   bad_proxies, OK, net_log);
    EXPECT_TRUE(retry_info_map.end() == retry_info_map.find(proxy_chain));
    EXPECT_TRUE(retry_info_map.end() ==
                retry_info_map.find(ProxyUriToProxyChain(
                    "foopy3:80", ProxyServer::SCHEME_HTTP)));
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
                                   std::vector<ProxyChain>(),
                                   ERR_PROXY_CONNECTION_FAILED, net_log);
    // Next, mark the same proxy as bad for 1 second. This call should have no
    // effect, since this would cause the bad proxy to be retried sooner than
    // the existing retry info.
    list.UpdateRetryInfoOnFallback(&retry_info_map, base::Seconds(1), false,
                                   std::vector<ProxyChain>(), OK, net_log);
    ProxyChain proxy_chain(
        ProxyUriToProxyChain("foopy1:80", ProxyServer::SCHEME_HTTP));
    EXPECT_TRUE(retry_info_map.end() != retry_info_map.find(proxy_chain));
    EXPECT_EQ(ERR_PROXY_CONNECTION_FAILED,
              retry_info_map[proxy_chain].net_error);
    EXPECT_TRUE(retry_info_map[proxy_chain].try_while_bad);
    EXPECT_EQ(base::Seconds(60), retry_info_map[proxy_chain].current_delay);
    EXPECT_GT(retry_info_map[proxy_chain].bad_until,
              base::TimeTicks::Now() + base::Seconds(30));
    EXPECT_TRUE(retry_info_map.end() ==
                retry_info_map.find(ProxyUriToProxyChain(
                    "foopy2:80", ProxyServer::SCHEME_HTTP)));
    EXPECT_TRUE(retry_info_map.end() ==
                retry_info_map.find(ProxyUriToProxyChain(
                    "foopy3:80", ProxyServer::SCHEME_HTTP)));
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
                                   std::vector<ProxyChain>(), OK, net_log);
    // Next, mark the same proxy as bad for 60 seconds. This call should replace
    // the existing retry info with the new 60 second retry info.
    list.UpdateRetryInfoOnFallback(&retry_info_map, base::Seconds(60), true,
                                   std::vector<ProxyChain>(),
                                   ERR_PROXY_CONNECTION_FAILED, net_log);
    ProxyChain proxy_chain(
        ProxyUriToProxyChain("foopy1:80", ProxyServer::SCHEME_HTTP));
    EXPECT_TRUE(retry_info_map.end() != retry_info_map.find(proxy_chain));
    EXPECT_EQ(ERR_PROXY_CONNECTION_FAILED,
              retry_info_map[proxy_chain].net_error);
    EXPECT_TRUE(retry_info_map[proxy_chain].try_while_bad);
    EXPECT_EQ(base::Seconds(60), retry_info_map[proxy_chain].current_delay);
    EXPECT_GT(retry_info_map[proxy_chain].bad_until,
              base::TimeTicks::Now() + base::Seconds(30));
    EXPECT_TRUE(retry_info_map.end() ==
                retry_info_map.find(ProxyUriToProxyChain(
                    "foopy2:80", ProxyServer::SCHEME_HTTP)));
    EXPECT_TRUE(retry_info_map.end() ==
                retry_info_map.find(ProxyUriToProxyChain(
                    "foopy3:80", ProxyServer::SCHEME_HTTP)));
  }
}

TEST(ProxyListTest, ToPacString) {
  ProxyList list;
  list.AddProxyChain(ProxyChain::FromSchemeHostAndPort(
      ProxyServer::Scheme::SCHEME_HTTPS, "foo", 443));
  EXPECT_EQ(list.ToPacString(), "HTTPS foo:443");
  // ToPacString should fail for proxy chains.
  list.AddProxyChain(ProxyChain::ForIpProtection({
      ProxyServer::FromSchemeHostAndPort(ProxyServer::Scheme::SCHEME_HTTPS,
                                         "foo-a", 443),
      ProxyServer::FromSchemeHostAndPort(ProxyServer::Scheme::SCHEME_HTTPS,
                                         "foo-b", 443),
  }));
  EXPECT_DEATH_IF_SUPPORTED(list.ToPacString(), "");
}

TEST(ProxyListTest, ToDebugString) {
  ProxyList list;
  list.AddProxyChain(ProxyChain::FromSchemeHostAndPort(
      ProxyServer::Scheme::SCHEME_HTTPS, "foo", 443));
  list.AddProxyChain(ProxyChain::ForIpProtection({
      ProxyServer::FromSchemeHostAndPort(ProxyServer::Scheme::SCHEME_HTTPS,
                                         "foo-a", 443),
      ProxyServer::FromSchemeHostAndPort(ProxyServer::Scheme::SCHEME_HTTPS,
                                         "foo-b", 443),
  }));

  EXPECT_EQ(
      list.ToDebugString(),
      "HTTPS foo:443;[https://foo-a:443, https://foo-b:443] (IP Protection)");
}

TEST(ProxyListTest, ToValue) {
  ProxyList list;
  list.AddProxyChain(ProxyChain::FromSchemeHostAndPort(
      ProxyServer::Scheme::SCHEME_HTTPS, "foo", 443));
  list.AddProxyChain(ProxyChain::ForIpProtection({
      ProxyServer::FromSchemeHostAndPort(ProxyServer::Scheme::SCHEME_HTTPS,
                                         "foo-a", 443),
      ProxyServer::FromSchemeHostAndPort(ProxyServer::Scheme::SCHEME_HTTPS,
                                         "foo-b", 443),
  }));

  base::Value expected(base::Value::Type::LIST);
  base::Value::List& exp_list = expected.GetList();
  exp_list.Append("[https://foo:443]");
  exp_list.Append("[https://foo-a:443, https://foo-b:443] (IP Protection)");

  EXPECT_EQ(list.ToValue(), expected);
}

#if BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
// The following tests are for non-release builds where multi-proxy chains are
// permitted outside of Ip Protection.

TEST(ProxyListTest,
     NonIpProtectionMultiProxyChainRemoveProxiesWithoutSchemeWithProxyChains) {
  const ProxyChain kProxyChainFooHttps({
      ProxyServer::FromSchemeHostAndPort(ProxyServer::Scheme::SCHEME_HTTPS,
                                         "foo-a", 443),
      ProxyServer::FromSchemeHostAndPort(ProxyServer::Scheme::SCHEME_HTTPS,
                                         "foo-b", 443),
  });
  const ProxyChain kProxyChainBarMixed({
      ProxyServer::FromSchemeHostAndPort(ProxyServer::Scheme::SCHEME_QUIC,
                                         "bar-a", 443),
      ProxyServer::FromSchemeHostAndPort(ProxyServer::Scheme::SCHEME_HTTPS,
                                         "bar-b", 443),
  });
  const ProxyChain kProxyChainGraultSocks = ProxyChain::FromSchemeHostAndPort(
      ProxyServer::Scheme::SCHEME_SOCKS4, "grault", 443);

  ProxyList list;
  list.AddProxyChain(kProxyChainFooHttps);
  list.AddProxyChain(kProxyChainBarMixed);
  list.AddProxyChain(kProxyChainGraultSocks);
  list.AddProxyChain(ProxyChain::Direct());

  // Remove anything that isn't entirely HTTPS.
  list.RemoveProxiesWithoutScheme(ProxyServer::SCHEME_HTTPS);

  std::vector<net::ProxyChain> expected = {
      kProxyChainFooHttps,
      ProxyChain::Direct(),
  };
  EXPECT_EQ(list.AllChains(), expected);
}

// `ToPacString` should only be called if the list contains no multi-proxy
// chains, as those cannot be represented in PAC syntax. This is not an issue in
// release builds because a `ProxyChain` constructed with multiple proxy servers
// would automatically default to an empty, invalid
// `ProxyChain` (unless for Ip Protection); however, in non-release builds,
// multi-proxy chains are permitted which means they must be CHECKED when this
// function is called.
TEST(ProxyListTest, NonIpProtectionMultiProxyChainToPacString) {
  ProxyList list;
  list.AddProxyChain(ProxyChain::FromSchemeHostAndPort(
      ProxyServer::Scheme::SCHEME_HTTPS, "foo", 443));
  EXPECT_EQ(list.ToPacString(), "HTTPS foo:443");
  // ToPacString should fail for proxy chains.
  list.AddProxyChain(ProxyChain({
      ProxyServer::FromSchemeHostAndPort(ProxyServer::Scheme::SCHEME_HTTPS,
                                         "foo-a", 443),
      ProxyServer::FromSchemeHostAndPort(ProxyServer::Scheme::SCHEME_HTTPS,
                                         "foo-b", 443),
  }));
  EXPECT_DEATH_IF_SUPPORTED(list.ToPacString(), "");
}

TEST(ProxyListTest, NonIpProtectionMultiProxyChainToDebugString) {
  ProxyList list;
  list.AddProxyChain(ProxyChain::FromSchemeHostAndPort(
      ProxyServer::Scheme::SCHEME_HTTPS, "foo", 443));
  list.AddProxyChain(ProxyChain({
      ProxyServer::FromSchemeHostAndPort(ProxyServer::Scheme::SCHEME_HTTPS,
                                         "foo-a", 443),
      ProxyServer::FromSchemeHostAndPort(ProxyServer::Scheme::SCHEME_HTTPS,
                                         "foo-b", 443),
  }));

  EXPECT_EQ(list.ToDebugString(),
            "HTTPS foo:443;[https://foo-a:443, https://foo-b:443]");
}

TEST(ProxyListTest, NonIpProtectionMultiProxyChainToValue) {
  ProxyList list;
  list.AddProxyChain(ProxyChain::FromSchemeHostAndPort(
      ProxyServer::Scheme::SCHEME_HTTPS, "foo", 443));
  list.AddProxyChain(ProxyChain({
      ProxyServer::FromSchemeHostAndPort(ProxyServer::Scheme::SCHEME_HTTPS,
                                         "foo-a", 443),
      ProxyServer::FromSchemeHostAndPort(ProxyServer::Scheme::SCHEME_HTTPS,
                                         "foo-b", 443),
  }));

  base::Value expected(base::Value::Type::LIST);
  base::Value::List& exp_list = expected.GetList();
  exp_list.Append("[https://foo:443]");
  exp_list.Append("[https://foo-a:443, https://foo-b:443]");

  EXPECT_EQ(list.ToValue(), expected);
}
#endif  // BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)

}  // anonymous namespace

}  // namespace net
