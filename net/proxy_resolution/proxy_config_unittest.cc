// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/proxy_config.h"

#include "base/json/json_writer.h"
#include "base/test/gtest_util.h"
#include "base/values.h"
#include "build/buildflag.h"
#include "net/base/proxy_string_util.h"
#include "net/net_buildflags.h"
#include "net/proxy_resolution/proxy_config_service_common_unittest.h"
#include "net/proxy_resolution/proxy_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

void ExpectProxyServerEquals(const char* expectation,
                             const ProxyList& proxy_list) {
  if (expectation == nullptr) {
    EXPECT_TRUE(proxy_list.IsEmpty());
  } else {
    EXPECT_EQ(expectation, proxy_list.ToDebugString());
  }
}

TEST(ProxyConfigTest, Equals) {
  // Test |ProxyConfig::auto_detect|.

  ProxyConfig config1;
  config1.set_auto_detect(true);

  ProxyConfig config2;
  config2.set_auto_detect(false);

  EXPECT_FALSE(config1.Equals(config2));
  EXPECT_FALSE(config2.Equals(config1));

  config2.set_auto_detect(true);

  EXPECT_TRUE(config1.Equals(config2));
  EXPECT_TRUE(config2.Equals(config1));

  // Test |ProxyConfig::pac_url|.

  config2.set_pac_url(GURL("http://wpad/wpad.dat"));

  EXPECT_FALSE(config1.Equals(config2));
  EXPECT_FALSE(config2.Equals(config1));

  config1.set_pac_url(GURL("http://wpad/wpad.dat"));

  EXPECT_TRUE(config1.Equals(config2));
  EXPECT_TRUE(config2.Equals(config1));

  // Test |ProxyConfig::proxy_rules|.

  config2.proxy_rules().type = ProxyConfig::ProxyRules::Type::PROXY_LIST;
  config2.proxy_rules().single_proxies.SetSingleProxyServer(
      ProxyUriToProxyServer("myproxy:80", ProxyServer::SCHEME_HTTP));

  EXPECT_FALSE(config1.Equals(config2));
  EXPECT_FALSE(config2.Equals(config1));

  config1.proxy_rules().type = ProxyConfig::ProxyRules::Type::PROXY_LIST;
  config1.proxy_rules().single_proxies.SetSingleProxyServer(
      ProxyUriToProxyServer("myproxy:100", ProxyServer::SCHEME_HTTP));

  EXPECT_FALSE(config1.Equals(config2));
  EXPECT_FALSE(config2.Equals(config1));

  config1.proxy_rules().single_proxies.SetSingleProxyServer(
      ProxyUriToProxyServer("myproxy", ProxyServer::SCHEME_HTTP));

  EXPECT_TRUE(config1.Equals(config2));
  EXPECT_TRUE(config2.Equals(config1));

  // Test |ProxyConfig::bypass_rules|.

  config2.proxy_rules().bypass_rules.AddRuleFromString("*.google.com");

  EXPECT_FALSE(config1.Equals(config2));
  EXPECT_FALSE(config2.Equals(config1));

  config1.proxy_rules().bypass_rules.AddRuleFromString("*.google.com");

  EXPECT_TRUE(config1.Equals(config2));
  EXPECT_TRUE(config2.Equals(config1));

  // Test |ProxyConfig::proxy_rules.reverse_bypass|.

  config2.proxy_rules().reverse_bypass = true;

  EXPECT_FALSE(config1.Equals(config2));
  EXPECT_FALSE(config2.Equals(config1));

  config1.proxy_rules().reverse_bypass = true;

  EXPECT_TRUE(config1.Equals(config2));
  EXPECT_TRUE(config2.Equals(config1));
}

#if BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
TEST(ProxyConfigTest, EqualsMultiProxyChains) {
  ProxyConfig config1;
  ProxyConfig config2;

  config2.proxy_rules().type =
      ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME;
  config2.proxy_rules().proxies_for_https.SetSingleProxyChain(
      MultiProxyUrisToProxyChain("[https://foopy:443 https://hoopy:443]",
                                 ProxyServer::SCHEME_HTTPS));

  EXPECT_FALSE(config1.Equals(config2));
  EXPECT_FALSE(config2.Equals(config1));

  config1.proxy_rules().type =
      ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME;
  config1.proxy_rules().proxies_for_https.SetSingleProxyChain(
      MultiProxyUrisToProxyChain("[https://foopy:80 https://hoopy:80]",
                                 ProxyServer::SCHEME_HTTPS));

  EXPECT_FALSE(config1.Equals(config2));
  EXPECT_FALSE(config2.Equals(config1));

  config1.proxy_rules().proxies_for_https.SetSingleProxyChain(
      MultiProxyUrisToProxyChain("[https://foopy https://hoopy]",
                                 ProxyServer::SCHEME_HTTPS));

  EXPECT_TRUE(config1.Equals(config2));
  EXPECT_TRUE(config2.Equals(config1));
}
#endif  // BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)

struct ProxyConfigToValueTestCase {
  ProxyConfig config;
  const char* expected_value_json;
};

class ProxyConfigToValueTest
    : public ::testing::TestWithParam<ProxyConfigToValueTestCase> {};

TEST_P(ProxyConfigToValueTest, ToValueJSON) {
  const ProxyConfigToValueTestCase& test_case = GetParam();

  base::Value value = test_case.config.ToValue();

  std::string json_string;
  ASSERT_TRUE(base::JSONWriter::Write(value, &json_string));

  EXPECT_EQ(std::string(test_case.expected_value_json), json_string);
}

ProxyConfigToValueTestCase GetTestCaseDirect() {
  return {ProxyConfig::CreateDirect(), "{}"};
}

ProxyConfigToValueTestCase GetTestCaseAutoDetect() {
  return {ProxyConfig::CreateAutoDetect(), "{\"auto_detect\":true}"};
}

ProxyConfigToValueTestCase GetTestCasePacUrl() {
  ProxyConfig config;
  config.set_pac_url(GURL("http://www.example.com/test.pac"));

  return {std::move(config),
          "{\"pac_url\":\"http://www.example.com/test.pac\"}"};
}

ProxyConfigToValueTestCase GetTestCasePacUrlMandatory() {
  ProxyConfig config;
  config.set_pac_url(GURL("http://www.example.com/test.pac"));
  config.set_pac_mandatory(true);

  return {std::move(config),
          "{\"pac_mandatory\":true,\"pac_url\":\"http://www.example.com/"
          "test.pac\"}"};
}

ProxyConfigToValueTestCase GetTestCasePacUrlAndAutoDetect() {
  ProxyConfig config = ProxyConfig::CreateAutoDetect();
  config.set_pac_url(GURL("http://www.example.com/test.pac"));

  return {
      std::move(config),
      "{\"auto_detect\":true,\"pac_url\":\"http://www.example.com/test.pac\"}"};
}

ProxyConfigToValueTestCase GetTestCaseSingleProxy() {
  ProxyConfig config;
  config.proxy_rules().ParseFromString("https://proxy1:8080");

  return {std::move(config), "{\"single_proxy\":[\"[https://proxy1:8080]\"]}"};
}

ProxyConfigToValueTestCase GetTestCaseSingleProxyWithBypass() {
  ProxyConfig config;
  config.proxy_rules().ParseFromString("https://proxy1:8080");
  config.proxy_rules().bypass_rules.AddRuleFromString("*.google.com");
  config.proxy_rules().bypass_rules.AddRuleFromString("192.168.0.1/16");

  return {std::move(config),
          "{\"bypass_list\":[\"*.google.com\",\"192.168.0.1/"
          "16\"],\"single_proxy\":[\"[https://proxy1:8080]\"]}"};
}

ProxyConfigToValueTestCase GetTestCaseSingleProxyWithReversedBypass() {
  ProxyConfig config;
  config.proxy_rules().ParseFromString("https://proxy1:8080");
  config.proxy_rules().bypass_rules.AddRuleFromString("*.google.com");
  config.proxy_rules().reverse_bypass = true;

  return {std::move(config),
          "{\"bypass_list\":[\"*.google.com\"],\"reverse_bypass\":true,"
          "\"single_proxy\":[\"[https://proxy1:8080]\"]}"};
}

ProxyConfigToValueTestCase GetTestCaseProxyPerScheme() {
  ProxyConfig config;
  config.proxy_rules().ParseFromString(
      "http=https://proxy1:8080;https=socks5://proxy2");
  config.proxy_rules().bypass_rules.AddRuleFromString("*.google.com");
  config.set_pac_url(GURL("http://wpad/wpad.dat"));
  config.set_auto_detect(true);

  return {
      std::move(config),
      "{\"auto_detect\":true,\"bypass_list\":[\"*.google.com\"],\"pac_url\":"
      "\"http://wpad/wpad.dat\",\"proxy_per_scheme\":{\"http\":[\"[https://"
      "proxy1:8080]\"],\"https\":[\"[socks5://proxy2:1080]\"]}}"};
}

ProxyConfigToValueTestCase GetTestCaseSingleProxyList() {
  ProxyConfig config;
  config.proxy_rules().ParseFromString(
      "https://proxy1:8080,http://proxy2,direct://");

  return {
      std::move(config),
      "{\"single_proxy\":[\"[https://proxy1:8080]\",\"[proxy2:80]\",\"direct://"
      "\"]}"};
}

#if BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
// Multi-proxy chains present
ProxyConfigToValueTestCase GetTestCaseMultiProxyChainProxyPerScheme() {
  ProxyConfig config;
  config.proxy_rules().ParseFromString(
      "http=[https://proxy1:8080 https://proxy2:8080];https=socks5://proxy2",
      /*allow_bracketed_proxy_chains=*/true);
  config.proxy_rules().bypass_rules.AddRuleFromString("*.google.com");
  config.set_pac_url(GURL("http://wpad/wpad.dat"));
  config.set_auto_detect(true);

  return {std::move(config),
          "{\"auto_detect\":true,\"bypass_list\":[\"*.google.com\"],\"pac_"
          "url\":\"http://wpad/"
          "wpad.dat\",\"proxy_per_scheme\":{\"http\":[\"[https://proxy1:8080, "
          "https://proxy2:8080]\"],\"https\":[\"[socks5://proxy2:1080]\"]}}"};
}
#endif  // BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)

INSTANTIATE_TEST_SUITE_P(
    All,
    ProxyConfigToValueTest,
    testing::Values(GetTestCaseDirect(),
                    GetTestCaseAutoDetect(),
                    GetTestCasePacUrl(),
                    GetTestCasePacUrlMandatory(),
                    GetTestCasePacUrlAndAutoDetect(),
                    GetTestCaseSingleProxy(),
                    GetTestCaseSingleProxyWithBypass(),
                    GetTestCaseSingleProxyWithReversedBypass(),
                    GetTestCaseProxyPerScheme(),
#if BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
                    GetTestCaseMultiProxyChainProxyPerScheme(),
#endif  // BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
                    GetTestCaseSingleProxyList()));

TEST(ProxyConfigTest, ParseProxyRules) {
  const struct {
    const char* proxy_rules;

    ProxyConfig::ProxyRules::Type type;
    // These will be PAC-stle strings, eg 'PROXY foo.com'
    const char* single_proxy;
    const char* proxy_for_http;
    const char* proxy_for_https;
    const char* proxy_for_ftp;
    const char* fallback_proxy;
  } tests[] = {
      // One HTTP proxy for all schemes.
      {
          "myproxy:80",

          ProxyConfig::ProxyRules::Type::PROXY_LIST,
          "PROXY myproxy:80",
          nullptr,
          nullptr,
          nullptr,
          nullptr,
      },

      // Multiple HTTP proxies for all schemes.
      {
          "myproxy:80,https://myotherproxy",

          ProxyConfig::ProxyRules::Type::PROXY_LIST,
          "PROXY myproxy:80;HTTPS myotherproxy:443",
          nullptr,
          nullptr,
          nullptr,
          nullptr,
      },

      // Only specify a proxy server for "http://" urls.
      {
          "http=myproxy:80",

          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
          nullptr,
          "PROXY myproxy:80",
          nullptr,
          nullptr,
          nullptr,
      },

      // Specify an HTTP proxy for "ftp://" and a SOCKS proxy for "https://"
      // urls.
      {
          "ftp=ftp-proxy ; https=socks4://foopy",

          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
          nullptr,
          nullptr,
          "SOCKS foopy:1080",
          "PROXY ftp-proxy:80",
          nullptr,
      },

      // Give a scheme-specific proxy as well as a non-scheme specific.
      // The first entry "foopy" takes precedence marking this list as
      // Type::PROXY_LIST.
      {
          "foopy ; ftp=ftp-proxy",

          ProxyConfig::ProxyRules::Type::PROXY_LIST,
          "PROXY foopy:80",
          nullptr,
          nullptr,
          nullptr,
          nullptr,
      },

      // Give a scheme-specific proxy as well as a non-scheme specific.
      // The first entry "ftp=ftp-proxy" takes precedence marking this list as
      // Type::PROXY_LIST_PER_SCHEME.
      {
          "ftp=ftp-proxy ; foopy",

          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
          nullptr,
          nullptr,
          nullptr,
          "PROXY ftp-proxy:80",
          nullptr,
      },

      // Include a list of entries for a single scheme.
      {
          "ftp=ftp1,ftp2,ftp3",

          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
          nullptr,
          nullptr,
          nullptr,
          "PROXY ftp1:80;PROXY ftp2:80;PROXY ftp3:80",
          nullptr,
      },

      // Include multiple entries for the same scheme -- they accumulate.
      {
          "http=http1,http2; http=http3",

          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
          nullptr,
          "PROXY http1:80;PROXY http2:80;PROXY http3:80",
          nullptr,
          nullptr,
          nullptr,
      },

      // Include lists of entries for multiple schemes.
      {
          "ftp=ftp1,ftp2,ftp3 ; http=http1,http2; ",

          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
          nullptr,
          "PROXY http1:80;PROXY http2:80",
          nullptr,
          "PROXY ftp1:80;PROXY ftp2:80;PROXY ftp3:80",
          nullptr,
      },

      // Include non-default proxy schemes.
      {
          "http=https://secure_proxy; ftp=socks4://socks_proxy; "
          "https=socks://foo",

          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
          nullptr,
          "HTTPS secure_proxy:443",
          "SOCKS5 foo:1080",
          "SOCKS socks_proxy:1080",
          nullptr,
      },

      // Only SOCKS proxy present, others being blank.
      {
          "socks=foopy",

          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
          nullptr,
          nullptr,
          nullptr,
          nullptr,
          "SOCKS foopy:1080",
      },

      // SOCKS proxy present along with other proxies too
      {
          "http=httpproxy ; https=httpsproxy ; ftp=ftpproxy ; socks=foopy ",

          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
          nullptr,
          "PROXY httpproxy:80",
          "PROXY httpsproxy:80",
          "PROXY ftpproxy:80",
          "SOCKS foopy:1080",
      },

      // SOCKS proxy (with modifier) present along with some proxies
      // (FTP being blank)
      {
          "http=httpproxy ; https=httpsproxy ; socks=socks5://foopy ",

          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
          nullptr,
          "PROXY httpproxy:80",
          "PROXY httpsproxy:80",
          nullptr,
          "SOCKS5 foopy:1080",
      },

      // Include unsupported schemes -- they are discarded.
      {
          "crazy=foopy ; foo=bar ; https=myhttpsproxy",

          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
          nullptr,
          nullptr,
          "PROXY myhttpsproxy:80",
          nullptr,
          nullptr,
      },

      // direct:// as first option for a scheme.
      {
          "http=direct://,myhttpproxy; https=direct://",

          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
          nullptr,
          "DIRECT;PROXY myhttpproxy:80",
          "DIRECT",
          nullptr,
          nullptr,
      },

      // direct:// as a second option for a scheme.
      {
          "http=myhttpproxy,direct://",

          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
          nullptr,
          "PROXY myhttpproxy:80;DIRECT",
          nullptr,
          nullptr,
          nullptr,
      },

      // Multi-proxy bracketed URIs will result in no proxy being set
      {
          "http=[https://proxy1:8080 https://proxy2:8080]",
          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
          nullptr,
          nullptr,
          nullptr,
          nullptr,
          nullptr,
      }

  };

  ProxyConfig config;

  for (const auto& test : tests) {
    config.proxy_rules().ParseFromString(test.proxy_rules);
    EXPECT_EQ(test.type, config.proxy_rules().type);
    ExpectProxyServerEquals(test.single_proxy,
                            config.proxy_rules().single_proxies);
    ExpectProxyServerEquals(test.proxy_for_http,
                            config.proxy_rules().proxies_for_http);
    ExpectProxyServerEquals(test.proxy_for_https,
                            config.proxy_rules().proxies_for_https);
    ExpectProxyServerEquals(test.proxy_for_ftp,
                            config.proxy_rules().proxies_for_ftp);
    ExpectProxyServerEquals(test.fallback_proxy,
                            config.proxy_rules().fallback_proxies);
  }
}

#if BUILDFLAG(ENABLE_QUIC_PROXY_SUPPORT)
// When the bool to allow quic proxy support is false, there should be no valid
// proxies in the config.
TEST(ProxyConfigTest, ParseProxyRulesQuicIsNotAllowed) {
  ProxyConfig config;

  config.proxy_rules().ParseFromString("quic://foopy:443",
                                       /*allow_bracketed_proxy_chains=*/false,
                                       /*is_quic_allowed=*/false);

  EXPECT_EQ(ProxyConfig::ProxyRules::Type::PROXY_LIST,
            config.proxy_rules().type);
  ExpectProxyServerEquals(nullptr, config.proxy_rules().single_proxies);
  ExpectProxyServerEquals(nullptr, config.proxy_rules().proxies_for_http);
  ExpectProxyServerEquals(nullptr, config.proxy_rules().proxies_for_https);
  ExpectProxyServerEquals(nullptr, config.proxy_rules().proxies_for_ftp);
  ExpectProxyServerEquals(nullptr, config.proxy_rules().fallback_proxies);
}

// When the bool to allow quic proxy support is true, a valid quic proxy should
// be found in the config.
TEST(ProxyConfigTest, ParseProxyRulesQuicIsAllowed) {
  ProxyConfig config;

  config.proxy_rules().ParseFromString("quic://foopy:443",
                                       /*allow_bracketed_proxy_chains=*/false,
                                       /*is_quic_allowed=*/true);

  EXPECT_EQ(ProxyConfig::ProxyRules::Type::PROXY_LIST,
            config.proxy_rules().type);
  ExpectProxyServerEquals("QUIC foopy:443",
                          config.proxy_rules().single_proxies);
  ExpectProxyServerEquals(nullptr, config.proxy_rules().proxies_for_http);
  ExpectProxyServerEquals(nullptr, config.proxy_rules().proxies_for_https);
  ExpectProxyServerEquals(nullptr, config.proxy_rules().proxies_for_ftp);
  ExpectProxyServerEquals(nullptr, config.proxy_rules().fallback_proxies);
}
#endif  // BUILDFLAG(ENABLE_QUIC_PROXY_SUPPORT)

#if !BUILDFLAG(ENABLE_QUIC_PROXY_SUPPORT)
// When the build flag is disabled for QUIC support, `ParseFromString` should
// not allow QUIC proxy support by setting bool to true. A true value should
// crash.
TEST(ProxyConfigTest,
     ParseProxyRulesDisallowQuicProxySupportIfBuildFlagDisabled) {
  ProxyConfig config;

  EXPECT_CHECK_DEATH(config.proxy_rules().ParseFromString(
      "quic://foopy:443",
      /*allow_bracketed_proxy_chains=*/false, /*is_quic_allowed=*/true));
}
#endif  // !BUILDFLAG(ENABLE_QUIC_PROXY_SUPPORT)

#if !BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
// In release builds, `ParseFromString` should not allow parsing of multi-proxy
// chains by setting bool to true. A true value should crash.
TEST(ProxyConfigTest, ParseProxyRulesDisallowMultiProxyChainsInReleaseBuilds) {
  ProxyConfig config;

  EXPECT_CHECK_DEATH(config.proxy_rules().ParseFromString(
      "http=[https://proxy1:8080 https://proxy2:8080]", true));
}
#endif  // !BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)

#if BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
// Tests for multi-proxy chains which are currently only allowed in debug mode.
TEST(ProxyConfigTest, MultiProxyChainsParseProxyRules) {
  const struct {
    const char* proxy_rules;

    ProxyConfig::ProxyRules::Type type;
    // For multi-proxy chains, proxies within a single chain will be formatted
    // within a bracket separated by a space and comma.
    const char* single_proxy;
    const char* proxy_for_http;
    const char* proxy_for_https;
    const char* proxy_for_ftp;
    const char* fallback_proxy;
  } tests[] = {

      // One HTTP proxy for all schemes.
      {
          "[https://proxy1:8080]",
          ProxyConfig::ProxyRules::Type::PROXY_LIST,
          "HTTPS proxy1:8080",
          nullptr,
          nullptr,
          nullptr,
          nullptr,
      },

      // Multiple proxies for all schemes.
      {
          "[https://proxy1:8080 https://proxy2:8080],[https://proxy3:8080]",
          ProxyConfig::ProxyRules::Type::PROXY_LIST,
          "[https://proxy1:8080, https://proxy2:8080];HTTPS proxy3:8080",
          nullptr,
          nullptr,
          nullptr,
          nullptr,
      },

      // Only specify a proxy chain for "http://" urls.
      {
          "http=[https://proxy1:8080 https://proxy2:8080]",
          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
          nullptr,
          "[https://proxy1:8080, https://proxy2:8080]",
          nullptr,
          nullptr,
          nullptr,
      },

      // Specify different multi-proxy chains for different schemes.
      {
          "http=[https://proxy1:8080 https://proxy2:8080] ; "
          "https=[https://proxy3:8080 https://proxy4:8080]",
          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
          nullptr,
          "[https://proxy1:8080, https://proxy2:8080]",
          "[https://proxy3:8080, https://proxy4:8080]",
          nullptr,
          nullptr,
      },

      // Give a scheme-specific proxy as well as a non-scheme specific.
      // The first entry takes precedence marking this list as Type::PROXY_LIST.
      {
          "[https://proxy1:8080 https://proxy2:8080] ; "
          "http=[https://proxy3:8080 https://proxy4:8080]",
          ProxyConfig::ProxyRules::Type::PROXY_LIST,
          "[https://proxy1:8080, https://proxy2:8080]",
          nullptr,
          nullptr,
          nullptr,
          nullptr,
      },

      // Give a scheme-specific proxy as well as a non-scheme specific.
      // The first entry takes precedence marking this list as
      // Type::PROXY_LIST_PER_SCHEME.
      {
          "http=[https://proxy3:8080 https://proxy4:8080] ; "
          "[https://proxy1:8080 https://proxy2:8080]",
          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
          nullptr,
          "[https://proxy3:8080, https://proxy4:8080]",
          nullptr,
          nullptr,
          nullptr,
      },

      // Include a list of entries for a single scheme.
      {
          "ftp=[https://proxy1:80 https://proxy2:80],[https://proxy3:80 "
          "https://proxy4:80],[https://proxy5:80 https://proxy6:80]",
          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
          nullptr,
          nullptr,
          nullptr,
          "[https://proxy1:80, https://proxy2:80];[https://proxy3:80, "
          "https://proxy4:80];[https://proxy5:80, https://proxy6:80]",
          nullptr,
      },

      // Include multiple entries for the same scheme -- they accumulate.
      {
          "http=[https://proxy1:80 https://proxy2:80]; http=[https://proxy3:80 "
          "https://proxy4:80],[https://proxy5:80 https://proxy6:80]",
          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
          nullptr,
          "[https://proxy1:80, https://proxy2:80];[https://proxy3:80, "
          "https://proxy4:80];[https://proxy5:80, https://proxy6:80]",
          nullptr,
          nullptr,
          nullptr,
      },

      // Include lists of entries for multiple schemes.
      {
          "http=[https://proxy1:80 https://proxy2:80]; ftp=[https://proxy3:80 "
          "https://proxy4:80],[https://proxy5:80 https://proxy6:80]",
          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
          nullptr,
          "[https://proxy1:80, https://proxy2:80]",
          nullptr,
          "[https://proxy3:80, https://proxy4:80];[https://proxy5:80, "
          "https://proxy6:80]",
          nullptr,
      },

      // Include unsupported schemes -- they are discarded.
      {
          "crazy=[https://proxy1:80 https://proxy2:80] ; foo=bar ; "
          "https=[https://proxy3:80 https://proxy4:80]",

          ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME,
          nullptr,
          nullptr,
          "[https://proxy3:80, https://proxy4:80]",
          nullptr,
          nullptr,
      },
  };

  ProxyConfig config;

  for (const auto& test : tests) {
    config.proxy_rules().ParseFromString(test.proxy_rules, true);
    EXPECT_EQ(test.type, config.proxy_rules().type);
    ExpectProxyServerEquals(test.single_proxy,
                            config.proxy_rules().single_proxies);
    ExpectProxyServerEquals(test.proxy_for_http,
                            config.proxy_rules().proxies_for_http);
    ExpectProxyServerEquals(test.proxy_for_https,
                            config.proxy_rules().proxies_for_https);
    ExpectProxyServerEquals(test.proxy_for_ftp,
                            config.proxy_rules().proxies_for_ftp);
    ExpectProxyServerEquals(test.fallback_proxy,
                            config.proxy_rules().fallback_proxies);
  }
}
#endif  // BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)

TEST(ProxyConfigTest, ProxyRulesSetBypassFlag) {
  // Test whether the did_bypass_proxy() flag is set in proxy info correctly.
  ProxyConfig::ProxyRules rules;
  ProxyInfo result;

  rules.ParseFromString("http=httpproxy:80");
  rules.bypass_rules.AddRuleFromString(".com");

  rules.Apply(GURL("http://example.com"), &result);
  EXPECT_TRUE(result.is_direct_only());
  EXPECT_TRUE(result.did_bypass_proxy());

  rules.Apply(GURL("http://example.org"), &result);
  EXPECT_FALSE(result.is_direct());
  EXPECT_FALSE(result.did_bypass_proxy());

  // Try with reversed bypass rules.
  rules.reverse_bypass = true;

  rules.Apply(GURL("http://example.org"), &result);
  EXPECT_TRUE(result.is_direct_only());
  EXPECT_TRUE(result.did_bypass_proxy());

  rules.Apply(GURL("http://example.com"), &result);
  EXPECT_FALSE(result.is_direct());
  EXPECT_FALSE(result.did_bypass_proxy());
}

static const char kWsUrl[] = "ws://example.com/echo";
static const char kWssUrl[] = "wss://example.com/echo";

class ProxyConfigWebSocketTest : public ::testing::Test {
 protected:
  void ParseFromString(const std::string& rules) {
    rules_.ParseFromString(rules);
  }
  void Apply(const GURL& gurl) { rules_.Apply(gurl, &info_); }
  std::string ToDebugString() const { return info_.ToDebugString(); }

  static GURL WsUrl() { return GURL(kWsUrl); }
  static GURL WssUrl() { return GURL(kWssUrl); }

  ProxyConfig::ProxyRules rules_;
  ProxyInfo info_;
};

// If a single proxy is set for all protocols, WebSocket uses it.
TEST_F(ProxyConfigWebSocketTest, UsesProxy) {
  ParseFromString("proxy:3128");
  Apply(WsUrl());
  EXPECT_EQ("PROXY proxy:3128", ToDebugString());
}

// See RFC6455 Section 4.1. item 3, "_Proxy Usage_". Note that this favors a
// SOCKSv4 proxy (although technically the spec only notes SOCKSv5).
TEST_F(ProxyConfigWebSocketTest, PrefersSocksV4) {
  ParseFromString(
      "http=proxy:3128 ; https=sslproxy:3128 ; socks=socksproxy:1080");
  Apply(WsUrl());
  EXPECT_EQ("SOCKS socksproxy:1080", ToDebugString());
}

// See RFC6455 Section 4.1. item 3, "_Proxy Usage_".
TEST_F(ProxyConfigWebSocketTest, PrefersSocksV5) {
  ParseFromString(
      "http=proxy:3128 ; https=sslproxy:3128 ; socks=socks5://socksproxy:1080");
  Apply(WsUrl());
  EXPECT_EQ("SOCKS5 socksproxy:1080", ToDebugString());
}

TEST_F(ProxyConfigWebSocketTest, PrefersHttpsToHttp) {
  ParseFromString("http=proxy:3128 ; https=sslproxy:3128");
  Apply(WssUrl());
  EXPECT_EQ("PROXY sslproxy:3128", ToDebugString());
}

// Tests when a proxy-per-url-scheme configuration was used, and proxies are
// specified for http://, https://, and a fallback proxy (non-SOCKS).
// Even though the fallback proxy is not SOCKS, it is still favored over the
// proxy for http://* and https://*.
TEST_F(ProxyConfigWebSocketTest, PrefersNonSocksFallbackOverHttps) {
  // The notation for "socks=" is abused to set the "fallback proxy".
  ParseFromString(
      "http=proxy:3128 ; https=sslproxy:3128; socks=https://httpsproxy");
  EXPECT_EQ("HTTPS httpsproxy:443", rules_.fallback_proxies.ToDebugString());
  Apply(WssUrl());
  EXPECT_EQ("HTTPS httpsproxy:443", ToDebugString());
}

// Tests when a proxy-per-url-scheme configuration was used, and the fallback
// proxy is a non-SOCKS proxy, and no proxy was given for https://* or
// http://*. The fallback proxy is used.
TEST_F(ProxyConfigWebSocketTest, UsesNonSocksFallbackProxy) {
  // The notation for "socks=" is abused to set the "fallback proxy".
  ParseFromString("ftp=ftpproxy:3128; socks=https://httpsproxy");
  EXPECT_EQ("HTTPS httpsproxy:443", rules_.fallback_proxies.ToDebugString());
  Apply(WssUrl());
  EXPECT_EQ("HTTPS httpsproxy:443", ToDebugString());
}

TEST_F(ProxyConfigWebSocketTest, PrefersHttpsEvenForWs) {
  ParseFromString("http=proxy:3128 ; https=sslproxy:3128");
  Apply(WsUrl());
  EXPECT_EQ("PROXY sslproxy:3128", ToDebugString());
}

TEST_F(ProxyConfigWebSocketTest, PrefersHttpToDirect) {
  ParseFromString("http=proxy:3128");
  Apply(WssUrl());
  EXPECT_EQ("PROXY proxy:3128", ToDebugString());
}

TEST_F(ProxyConfigWebSocketTest, IgnoresFtpProxy) {
  ParseFromString("ftp=ftpproxy:3128");
  Apply(WssUrl());
  EXPECT_EQ("DIRECT", ToDebugString());
}

TEST_F(ProxyConfigWebSocketTest, ObeysBypassRules) {
  ParseFromString("http=proxy:3128 ; https=sslproxy:3128");
  rules_.bypass_rules.AddRuleFromString(".chromium.org");
  Apply(GURL("wss://codereview.chromium.org/feed"));
  EXPECT_EQ("DIRECT", ToDebugString());
}

TEST_F(ProxyConfigWebSocketTest, ObeysLocalBypass) {
  ParseFromString("http=proxy:3128 ; https=sslproxy:3128");
  rules_.bypass_rules.AddRuleFromString("<local>");
  Apply(GURL("ws://localhost/feed"));
  EXPECT_EQ("DIRECT", ToDebugString());
}

}  // namespace
}  // namespace net
