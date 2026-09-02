// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy_resolution/win/proxy_resolver_winhttp.h"

#include <windows.h>

#include <winhttp.h>

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/pac_file_data.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolver.h"
#include "net/proxy_resolution/win/proxy_resolver_winhttp_test_hooks.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

int g_call_count = 0;
std::wstring g_expected_pac_url;

BOOL WINAPI FakeGetProxyForUrlLoginFailure(HINTERNET /*session*/,
                                           LPCWSTR /*url*/,
                                           WINHTTP_AUTOPROXY_OPTIONS* options,
                                           WINHTTP_PROXY_INFO* /*info*/) {
  ++g_call_count;
  EXPECT_NE(nullptr, options);
  if (options) {
    EXPECT_FALSE(options->fAutoLogonIfChallenged);
    EXPECT_TRUE(options->dwFlags & WINHTTP_AUTOPROXY_CONFIG_URL);
    if (options->lpszAutoConfigUrl) {
      EXPECT_EQ(g_expected_pac_url, std::wstring(options->lpszAutoConfigUrl));
    }
  }
  ::SetLastError(ERROR_WINHTTP_LOGIN_FAILURE);
  return FALSE;
}

std::unique_ptr<ProxyResolver> CreateResolver(
    const scoped_refptr<PacFileData>& script_data) {
  ProxyResolverFactoryWinHttp factory;
  std::unique_ptr<ProxyResolver> resolver;
  std::unique_ptr<ProxyResolverFactory::Request> request;
  int rv = factory.CreateProxyResolver(script_data, &resolver,
                                       CompletionOnceCallback(), &request);
  EXPECT_EQ(OK, rv);
  return resolver;
}

struct AuthChallengeTestCase {
  scoped_refptr<PacFileData> script_data;
  std::wstring expected_pac_url;
};

class ProxyResolverWinHttpAuthChallengeTest
    : public testing::Test,
      public testing::WithParamInterface<AuthChallengeTestCase> {
 public:
  void SetUp() override {
    g_call_count = 0;
    g_expected_pac_url.clear();
  }
};

}  // namespace

// If the server hosting the PAC script requests authentication when the
// resolver tries to download it, the resolver must not send default
// credentials and must instead surface a failure to the caller. This mirrors
// the behaviour of PacFileFetcherImpl on the non-WinHTTP code path.
TEST_P(ProxyResolverWinHttpAuthChallengeTest, NoAutoLogonOnChallenge) {
  const AuthChallengeTestCase& test_case = GetParam();
  g_expected_pac_url = test_case.expected_pac_url;
  ScopedWinHttpGetProxyForUrlOverride override_hook(
      &FakeGetProxyForUrlLoginFailure);

  std::unique_ptr<ProxyResolver> resolver =
      CreateResolver(test_case.script_data);
  ASSERT_TRUE(resolver);

  ProxyInfo results;
  std::unique_ptr<ProxyResolver::Request> request;
  int rv = resolver->GetProxyForURL(
      GURL("https://example.test/"), NetworkAnonymizationKey(), &results,
      CompletionOnceCallback(), &request, NetLogWithSource());

  EXPECT_EQ(ERR_PROXY_AUTH_UNSUPPORTED, rv);
  EXPECT_EQ(1, g_call_count);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ProxyResolverWinHttpAuthChallengeTest,
    testing::Values(AuthChallengeTestCase{PacFileData::ForAutoDetect(),
                                          L"http://wpad/wpad.dat"},
                    AuthChallengeTestCase{
                        PacFileData::FromURL(GURL("http://pac.test/wpad.dat")),
                        L"http://pac.test/wpad.dat"}));

}  // namespace net
