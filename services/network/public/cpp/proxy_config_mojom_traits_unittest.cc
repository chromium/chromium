// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/proxy_config_mojom_traits.h"
#include "services/network/public/cpp/proxy_config_with_annotation_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_string_util.h"
#include "net/proxy_resolution/proxy_bypass_rules.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/proxy_config_with_annotation.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {
namespace {

// Tests that serializing and then deserializing |original_config| to send it
// over Mojo results in a ProxyConfigWithAnnotation that matches it.
bool TestProxyConfigRoundTrip(net::ProxyConfigWithAnnotation& original_config) {
  net::ProxyConfigWithAnnotation copied_config;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::ProxyConfigWithAnnotation>(
          original_config, copied_config));

  return original_config.value().Equals(copied_config.value()) &&
         original_config.traffic_annotation() ==
             copied_config.traffic_annotation();
}

TEST(ProxyConfigTraitsTest, AutoDetect) {
  net::ProxyConfigWithAnnotation proxy_config(
      net::ProxyConfig::CreateAutoDetect(), TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_TRUE(TestProxyConfigRoundTrip(proxy_config));
}

TEST(ProxyConfigTraitsTest, Direct) {
  net::ProxyConfigWithAnnotation proxy_config =
      net::ProxyConfigWithAnnotation::CreateDirect();
  EXPECT_TRUE(TestProxyConfigRoundTrip(proxy_config));
}

TEST(ProxyConfigTraitsTest, FromSystem) {
  net::ProxyConfig base_config;
  base_config.set_from_system(true);
  net::ProxyConfigWithAnnotation proxy_config(base_config,
                                              TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_TRUE(TestProxyConfigRoundTrip(proxy_config));
}

TEST(ProxyConfigTraitsTest, CustomPacURL) {
  net::ProxyConfigWithAnnotation proxy_config(
      net::ProxyConfig::CreateFromCustomPacURL(GURL("http://foo/")),
      TRAFFIC_ANNOTATION_FOR_TESTS);

  EXPECT_TRUE(TestProxyConfigRoundTrip(proxy_config));
}

TEST(ProxyConfigTraitsTest, MultiProxy) {
  net::ProxyConfig proxy_config;
  proxy_config.proxy_rules().type =
      net::ProxyConfig::ProxyRules::Type::PROXY_LIST;
  proxy_config.proxy_rules().single_proxies.AddProxyChain(net::ProxyChain(
      {ProxyUriToProxyServer("foo:333", net::ProxyServer::SCHEME_HTTPS),
       ProxyUriToProxyServer("foo:444", net::ProxyServer::SCHEME_HTTPS)}));
  net::ProxyConfigWithAnnotation annotated_config(proxy_config,
                                                  TRAFFIC_ANNOTATION_FOR_TESTS);
  EXPECT_TRUE(TestProxyConfigRoundTrip(annotated_config));
}

TEST(ProxyConfigTraitsTest, ProxyRules) {
  // Test cases copied directly from proxy_config_unittests, so should be
  // correctly formatted and have good coverage.
  const char* kTestCases[] = {
      "myproxy:80",
      "myproxy:80,https://myotherproxy",
      "http=myproxy:80",
      "ftp=ftp-proxy ; https=socks4://foopy",
      "foopy ; ftp=ftp-proxy",
      "ftp=ftp-proxy ; foopy",
      "ftp=ftp1,ftp2,ftp3",
      "http=http1,http2; http=http3",
      "ftp=ftp1,ftp2,ftp3 ; http=http1,http2; ",
      ("http=https://secure_proxy; ftp=socks4://socks_proxy; "
       "https=socks://foo"),
      "socks=foopy",
      "http=httpproxy ; https=httpsproxy ; ftp=ftpproxy ; socks=foopy ",
      "http=httpproxy ; https=httpsproxy ; socks=socks5://foopy ",
      "crazy=foopy ; foo=bar ; https=myhttpsproxy",
      "http=direct://,myhttpproxy; https=direct://",
      "http=myhttpproxy,direct://",
  };

  for (const char* test_case : kTestCases) {
    net::ProxyConfig proxy_config;
    proxy_config.proxy_rules().ParseFromString(test_case);
    net::ProxyConfigWithAnnotation annotated_config(
        proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_TRUE(TestProxyConfigRoundTrip(annotated_config));
  }
}

TEST(ProxyConfigTraitsTest, BypassRules) {
  // These should cover every one of the rule types documented in
  // proxy_bypass_rules.h.
  const char* kTestCases[] = {
      ".foo.com",
      "*foo1.com:80, foo2.com",
      "*",
      "<local>",
      "http://1.2.3.4:99",
      "1.2.3.4/16",
      "fe80::/10",
      "<-loopback>",
      "[e1f3:dEaD::3]",
  };

  for (const char* test_case : kTestCases) {
    net::ProxyConfig proxy_config;
    proxy_config.proxy_rules().ParseFromString("myproxy:80");
    proxy_config.proxy_rules().bypass_rules.ParseFromString(test_case);
    proxy_config.proxy_rules().reverse_bypass = false;
    // Make sure that the test case is properly formatted.
    EXPECT_GE(proxy_config.proxy_rules().bypass_rules.rules().size(), 1u);

    net::ProxyConfigWithAnnotation annotated_config(
        proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_TRUE(TestProxyConfigRoundTrip(annotated_config));

    // Run the test again, except reversing the meaning of the bypass rules.
    proxy_config.proxy_rules().reverse_bypass = true;
    annotated_config = net::ProxyConfigWithAnnotation(
        proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_TRUE(TestProxyConfigRoundTrip(annotated_config));
  }
}

}  // namespace
}  // namespace network
