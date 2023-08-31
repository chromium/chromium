// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_preferences.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace net {

TEST(HttpAuthPreferencesTest, DisableCnameLookup) {
  HttpAuthPreferences http_auth_preferences;
  EXPECT_FALSE(http_auth_preferences.NegotiateDisableCnameLookup());
  http_auth_preferences.set_negotiate_disable_cname_lookup(true);
  EXPECT_TRUE(http_auth_preferences.NegotiateDisableCnameLookup());
}

TEST(HttpAuthPreferencesTest, NegotiateEnablePort) {
  HttpAuthPreferences http_auth_preferences;
  EXPECT_FALSE(http_auth_preferences.NegotiateEnablePort());
  http_auth_preferences.set_negotiate_enable_port(true);
  EXPECT_TRUE(http_auth_preferences.NegotiateEnablePort());
}

#if BUILDFLAG(IS_POSIX)
TEST(HttpAuthPreferencesTest, DisableNtlmV2) {
  HttpAuthPreferences http_auth_preferences;
  EXPECT_TRUE(http_auth_preferences.NtlmV2Enabled());
  http_auth_preferences.set_ntlm_v2_enabled(false);
  EXPECT_FALSE(http_auth_preferences.NtlmV2Enabled());
}
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_ANDROID)
TEST(HttpAuthPreferencesTest, AuthAndroidNegotiateAccountType) {
  HttpAuthPreferences http_auth_preferences;
  EXPECT_EQ(std::string(),
            http_auth_preferences.AuthAndroidNegotiateAccountType());
  http_auth_preferences.set_auth_android_negotiate_account_type("foo");
  EXPECT_EQ(std::string("foo"),
            http_auth_preferences.AuthAndroidNegotiateAccountType());
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
TEST(HttpAuthPreferencesTest, AllowGssapiLibraryLoad) {
  HttpAuthPreferences http_auth_preferences;
  EXPECT_TRUE(http_auth_preferences.AllowGssapiLibraryLoad());
  http_auth_preferences.set_allow_gssapi_library_load(false);
  EXPECT_FALSE(http_auth_preferences.AllowGssapiLibraryLoad());
}
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

TEST(HttpAuthPreferencesTest, AuthServerAllowlist) {
  HttpAuthPreferences http_auth_preferences;
  // Check initial value
  EXPECT_FALSE(http_auth_preferences.CanUseDefaultCredentials(
      url::SchemeHostPort(GURL("abc"))));
  http_auth_preferences.SetServerAllowlist("*");
  EXPECT_TRUE(http_auth_preferences.CanUseDefaultCredentials(
      url::SchemeHostPort(GURL("abc"))));
}

TEST(HttpAuthPreferencesTest, DelegationType) {
  using DelegationType = HttpAuth::DelegationType;
  HttpAuthPreferences http_auth_preferences;
  // Check initial value
  EXPECT_EQ(DelegationType::kNone, http_auth_preferences.GetDelegationType(
                                       url::SchemeHostPort(GURL("abc"))));

  http_auth_preferences.SetDelegateAllowlist("*");
  EXPECT_EQ(DelegationType::kUnconstrained,
            http_auth_preferences.GetDelegationType(
                url::SchemeHostPort(GURL("abc"))));

  http_auth_preferences.set_delegate_by_kdc_policy(true);
  EXPECT_EQ(DelegationType::kByKdcPolicy,
            http_auth_preferences.GetDelegationType(
                url::SchemeHostPort(GURL("abc"))));

  http_auth_preferences.SetDelegateAllowlist("");
  EXPECT_EQ(DelegationType::kNone, http_auth_preferences.GetDelegationType(
                                       url::SchemeHostPort(GURL("abc"))));
}

TEST(HttpAuthPreferencesTest, HttpAuthSchemesFilter) {
  HttpAuthPreferences http_auth_preferences;
  http_auth_preferences.set_http_auth_scheme_filter(
      base::BindRepeating([](const url::SchemeHostPort& scheme_host_port) {
        return scheme_host_port.GetURL() == GURL("https://www.google.com");
      }));
  EXPECT_TRUE(http_auth_preferences.IsAllowedToUseAllHttpAuthSchemes(
      url::SchemeHostPort(GURL("https://www.google.com"))));
  EXPECT_FALSE(http_auth_preferences.IsAllowedToUseAllHttpAuthSchemes(
      url::SchemeHostPort(GURL("https://www.example.com"))));
}

}  // namespace net
