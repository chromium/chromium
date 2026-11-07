// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/winhttp/scoped_winttp_proxy_info.h"

#include <windows.h>

#include <winhttp.h>

#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace winhttp {

TEST(ScopedWinHttpProxyInfoTest, MoveConstructTransfersOwnership) {
  ScopedWinHttpProxyInfo source;
  source.set_access_type(WINHTTP_ACCESS_TYPE_NAMED_PROXY);
  source.set_proxy(L"foo:80");
  source.set_proxy_bypass(L"bar");

  ScopedWinHttpProxyInfo target(std::move(source));

  EXPECT_FALSE(source.IsValid());
  EXPECT_EQ(source.get()->lpszProxyBypass, nullptr);
  EXPECT_EQ(source.get()->dwAccessType,
            static_cast<DWORD>(WINHTTP_ACCESS_TYPE_DEFAULT_PROXY));
  ASSERT_TRUE(target.IsValid());
  EXPECT_EQ(target.get()->dwAccessType,
            static_cast<DWORD>(WINHTTP_ACCESS_TYPE_NAMED_PROXY));
  EXPECT_STREQ(target.get()->lpszProxy, L"foo:80");
  EXPECT_STREQ(target.get()->lpszProxyBypass, L"bar");
}

TEST(ScopedWinHttpProxyInfoTest, MoveConstructFromEmpty) {
  ScopedWinHttpProxyInfo source;

  ScopedWinHttpProxyInfo target(std::move(source));

  EXPECT_FALSE(target.IsValid());
  EXPECT_EQ(target.get()->lpszProxyBypass, nullptr);
  EXPECT_EQ(target.get()->dwAccessType,
            static_cast<DWORD>(WINHTTP_ACCESS_TYPE_DEFAULT_PROXY));
}

TEST(ScopedWinHttpProxyInfoTest, MoveAssignReplacesOwnedStrings) {
  ScopedWinHttpProxyInfo source;
  source.set_access_type(WINHTTP_ACCESS_TYPE_NAMED_PROXY);
  source.set_proxy(L"foo:80");
  source.set_proxy_bypass(L"bar");

  ScopedWinHttpProxyInfo target;
  target.set_access_type(WINHTTP_ACCESS_TYPE_NO_PROXY);
  target.set_proxy(L"old:8080");
  target.set_proxy_bypass(L"old_bypass");

  target = std::move(source);

  EXPECT_FALSE(source.IsValid());
  EXPECT_EQ(source.get()->lpszProxyBypass, nullptr);
  EXPECT_EQ(source.get()->dwAccessType,
            static_cast<DWORD>(WINHTTP_ACCESS_TYPE_DEFAULT_PROXY));
  ASSERT_TRUE(target.IsValid());
  EXPECT_EQ(target.get()->dwAccessType,
            static_cast<DWORD>(WINHTTP_ACCESS_TYPE_NAMED_PROXY));
  EXPECT_STREQ(target.get()->lpszProxy, L"foo:80");
  EXPECT_STREQ(target.get()->lpszProxyBypass, L"bar");
}

TEST(ScopedWinHttpProxyInfoTest, MoveAssignFromEmptyFreesOwnedStrings) {
  ScopedWinHttpProxyInfo source;

  ScopedWinHttpProxyInfo target;
  target.set_access_type(WINHTTP_ACCESS_TYPE_NAMED_PROXY);
  target.set_proxy(L"old:8080");
  target.set_proxy_bypass(L"old_bypass");

  target = std::move(source);

  EXPECT_FALSE(target.IsValid());
  EXPECT_EQ(target.get()->lpszProxyBypass, nullptr);
  EXPECT_EQ(target.get()->dwAccessType,
            static_cast<DWORD>(WINHTTP_ACCESS_TYPE_DEFAULT_PROXY));
}

TEST(ScopedWinHttpProxyInfoTest, SelfMoveAssignKeepsOwnedStrings) {
  ScopedWinHttpProxyInfo info;
  info.set_access_type(WINHTTP_ACCESS_TYPE_NAMED_PROXY);
  info.set_proxy(L"foo:80");
  info.set_proxy_bypass(L"bar");

  ScopedWinHttpProxyInfo& alias = info;
  info = std::move(alias);

  ASSERT_TRUE(info.IsValid());
  EXPECT_EQ(info.get()->dwAccessType,
            static_cast<DWORD>(WINHTTP_ACCESS_TYPE_NAMED_PROXY));
  EXPECT_STREQ(info.get()->lpszProxy, L"foo:80");
  EXPECT_STREQ(info.get()->lpszProxyBypass, L"bar");
}

TEST(ScopedWinHttpProxyInfoTest, SetReplacesOwnedStrings) {
  ScopedWinHttpProxyInfo info;
  info.set_proxy(L"old:8080");
  info.set_proxy_bypass(L"old_bypass");

  info.set_proxy(L"foo:80");
  info.set_proxy_bypass(L"bar");

  ASSERT_TRUE(info.IsValid());
  EXPECT_STREQ(info.get()->lpszProxy, L"foo:80");
  EXPECT_STREQ(info.get()->lpszProxyBypass, L"bar");
}

TEST(ScopedWinHttpProxyInfoTest, SetEmptyKeepsOwnedStrings) {
  ScopedWinHttpProxyInfo info;
  info.set_proxy(L"foo:80");
  info.set_proxy_bypass(L"bar");

  info.set_proxy(L"");
  info.set_proxy_bypass(L"");

  ASSERT_TRUE(info.IsValid());
  EXPECT_STREQ(info.get()->lpszProxy, L"foo:80");
  EXPECT_STREQ(info.get()->lpszProxyBypass, L"bar");
}

TEST(ScopedWinHttpProxyInfoTest, ReceiveFreesOwnedStrings) {
  ScopedWinHttpProxyInfo info;
  info.set_access_type(WINHTTP_ACCESS_TYPE_NAMED_PROXY);
  info.set_proxy(L"foo:80");
  info.set_proxy_bypass(L"bar");

  WINHTTP_PROXY_INFO* received = info.receive();

  ASSERT_EQ(received, info.get());
  EXPECT_FALSE(info.IsValid());
  EXPECT_EQ(received->lpszProxyBypass, nullptr);
  EXPECT_EQ(received->dwAccessType,
            static_cast<DWORD>(WINHTTP_ACCESS_TYPE_DEFAULT_PROXY));
}

}  // namespace winhttp
