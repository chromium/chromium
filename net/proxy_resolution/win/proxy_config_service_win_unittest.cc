// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/proxy_resolution/win/proxy_config_service_win.h"

#include "net/base/net_errors.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service_common_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(ProxyConfigServiceWinTest, SetFromIEConfig) {
  // Like WINHTTP_CURRENT_USER_IE_PROXY_CONFIG, but with const strings.
  struct IEProxyConfig {
    BOOL auto_detect;
    const wchar_t* auto_config_url;
    const wchar_t* proxy;
    const wchar_t* proxy_bypass;
  };
  const struct {
    // Input.
    IEProxyConfig ie_config;

    // Expected outputs (fields of the ProxyConfig).
    bool auto_detect;
    GURL pac_url;
    ProxyRulesExpectation proxy_rules;
    const char* proxy_bypass_list;  // newline separated
  } tests[] = {
      // Auto detect.
      {
          {
              // Input.
              TRUE,     // fAutoDetect
              nullptr,  // lpszAutoConfigUrl
              nullptr,  // lpszProxy
              nullptr,  // lpszProxyBypass
          },

          // Expected result.
          true,    // auto_detect
          GURL(),  // pac_url
          ProxyRulesExpectation::Empty(),
      },

      // Valid PAC url
      {
          {
              // Input.
              FALSE,                    // fAutoDetect
              L"http://wpad/wpad.dat",  // lpszAutoConfigUrl
              nullptr,                  // lpszProxy
              nullptr,                  // lpszProxy_bypass
          },

          // Expected result.
          false,                         // auto_detect
          GURL("http://wpad/wpad.dat"),  // pac_url
          ProxyRulesExpectation::Empty(),
      },

      // Invalid PAC url string.
      {
          {
              // Input.
              FALSE,        // fAutoDetect
              L"wpad.dat",  // lpszAutoConfigUrl
              nullptr,      // lpszProxy
              nullptr,      // lpszProxy_bypass
          },

          // Expected result.
          false,   // auto_detect
          GURL(),  // pac_url
          ProxyRulesExpectation::Empty(),
      },

      // Single-host in proxy list.
      {
          {
              // Input.
              FALSE,              // fAutoDetect
              nullptr,            // lpszAutoConfigUrl
              L"www.google.com",  // lpszProxy
              nullptr,            // lpszProxy_bypass
          },

          // Expected result.
          false,                                              // auto_detect
          GURL(),                                             // pac_url
          ProxyRulesExpectation::Single("www.google.com:80",  // single proxy
                                        ""),                  // bypass rules
      },

      // Per-scheme proxy rules.
      {
          {
              // Input.
              FALSE,    // fAutoDetect
              nullptr,  // lpszAutoConfigUrl
              L"http=www.google.com:80;https=www.foo.com:110",  // lpszProxy
              nullptr,  // lpszProxy_bypass
          },

          // Expected result.
          false,                                                 // auto_detect
          GURL(),                                                // pac_url
          ProxyRulesExpectation::PerScheme("www.google.com:80",  // http
                                           "www.foo.com:110",    // https
                                           "",                   // ftp
                                           ""),                  // bypass rules
      },

      // SOCKS proxy configuration.
      {
          {
              // Input.
              FALSE,    // fAutoDetect
              nullptr,  // lpszAutoConfigUrl
              L"http=www.google.com:80;https=www.foo.com:110;"
              L"ftp=ftpproxy:20;socks=foopy:130",  // lpszProxy
              nullptr,                             // lpszProxy_bypass
          },

          // Expected result.
          // Note that "socks" is interprted as meaning "socks4", since that is
          // how
          // Internet Explorer applies the settings. For more details on this
          // policy, see:
          // http://code.google.com/p/chromium/issues/detail?id=55912#c2
          false,   // auto_detect
          GURL(),  // pac_url
          ProxyRulesExpectation::PerSchemeWithSocks(
              "www.google.com:80",   // http
              "www.foo.com:110",     // https
              "ftpproxy:20",         // ftp
              "socks4://foopy:130",  // socks
              ""),                   // bypass rules
      },

      // Bypass local names.
      {
          {
              // Input.
              TRUE,        // fAutoDetect
              nullptr,     // lpszAutoConfigUrl
              nullptr,     // lpszProxy
              L"<local>",  // lpszProxy_bypass
          },

          true,    // auto_detect
          GURL(),  // pac_url
          ProxyRulesExpectation::EmptyWithBypass("<local>"),
      },

      // Bypass "google.com" and local names, using semicolon as delimiter
      // (ignoring white space).
      {
          {
              // Input.
              TRUE,                     // fAutoDetect
              nullptr,                  // lpszAutoConfigUrl
              nullptr,                  // lpszProxy
              L"<local> ; google.com",  // lpszProxy_bypass
          },

          // Expected result.
          true,    // auto_detect
          GURL(),  // pac_url
          ProxyRulesExpectation::EmptyWithBypass("<local>,google.com"),
      },

      // Bypass "foo.com" and "google.com", using lines as delimiter.
      {
          {
              // Input.
              TRUE,                      // fAutoDetect
              nullptr,                   // lpszAutoConfigUrl
              nullptr,                   // lpszProxy
              L"foo.com\r\ngoogle.com",  // lpszProxy_bypass
          },

          // Expected result.
          true,    // auto_detect
          GURL(),  // pac_url
          ProxyRulesExpectation::EmptyWithBypass("foo.com,google.com"),
      },

      // Bypass "foo.com" and "google.com", using commas as delimiter.
      {
          {
              // Input.
              TRUE,                    // fAutoDetect
              nullptr,                 // lpszAutoConfigUrl
              nullptr,                 // lpszProxy
              L"foo.com, google.com",  // lpszProxy_bypass
          },

          // Expected result.
          true,    // auto_detect
          GURL(),  // pac_url
          ProxyRulesExpectation::EmptyWithBypass("foo.com,google.com"),
      },
  };

  for (size_t i = 0; i < std::size(tests); ++i) {
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ie_config = {
        tests[i].ie_config.auto_detect,
        const_cast<wchar_t*>(tests[i].ie_config.auto_config_url),
        const_cast<wchar_t*>(tests[i].ie_config.proxy),
        const_cast<wchar_t*>(tests[i].ie_config.proxy_bypass)};
    ProxyConfig config;
    ProxyConfigServiceWin::SetFromIEConfig(&config, ie_config);

    EXPECT_EQ(tests[i].auto_detect, config.auto_detect());
    EXPECT_EQ(tests[i].pac_url, config.pac_url());
    EXPECT_TRUE(tests[i].proxy_rules.Matches(config.proxy_rules()));
  }
}

}  // namespace net
