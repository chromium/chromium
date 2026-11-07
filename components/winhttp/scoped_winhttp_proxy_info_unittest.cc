// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/winhttp/scoped_winhttp_proxy_info.h"

#include <windows.h>

#include <winhttp.h>

#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace winhttp {

TEST(ScopedWinHttpProxyInfoTest, DefaultHasNoProxy) {
  ScopedWinHttpProxyInfo info;

  EXPECT_FALSE(info.has_proxy());
  EXPECT_EQ(info.get()->lpszProxyBypass, nullptr);
  EXPECT_EQ(info.get()->dwAccessType,
            static_cast<DWORD>(WINHTTP_ACCESS_TYPE_DEFAULT_PROXY));
}

TEST(ScopedWinHttpProxyInfoTest, BypassOnlyHasNoProxy) {
  ScopedWinHttpProxyInfo info;
  info.set_access_type(WINHTTP_ACCESS_TYPE_NO_PROXY);
  info.SetProxyBypass(L"bar");

  EXPECT_FALSE(info.has_proxy());
  EXPECT_EQ(info.get()->dwAccessType,
            static_cast<DWORD>(WINHTTP_ACCESS_TYPE_NO_PROXY));
  EXPECT_STREQ(info.get()->lpszProxyBypass, L"bar");
}

TEST(ScopedWinHttpProxyInfoTest, MoveConstructTransfersOwnership) {
  ScopedWinHttpProxyInfo source;
  source.set_access_type(WINHTTP_ACCESS_TYPE_NAMED_PROXY);
  source.SetProxy(L"foo:80");
  source.SetProxyBypass(L"bar");

  ScopedWinHttpProxyInfo target(std::move(source));

  EXPECT_FALSE(source.has_proxy());         // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(source.get()->lpszProxyBypass,  // NOLINT(bugprone-use-after-move)
            nullptr);
  EXPECT_EQ(source.get()->dwAccessType,  // NOLINT(bugprone-use-after-move)
            static_cast<DWORD>(WINHTTP_ACCESS_TYPE_DEFAULT_PROXY));
  ASSERT_TRUE(target.has_proxy());
  EXPECT_EQ(target.get()->dwAccessType,
            static_cast<DWORD>(WINHTTP_ACCESS_TYPE_NAMED_PROXY));
  EXPECT_STREQ(target.get()->lpszProxy, L"foo:80");
  EXPECT_STREQ(target.get()->lpszProxyBypass, L"bar");
}

TEST(ScopedWinHttpProxyInfoTest, MoveConstructFromEmpty) {
  ScopedWinHttpProxyInfo source;

  ScopedWinHttpProxyInfo target(std::move(source));

  EXPECT_FALSE(target.has_proxy());
  EXPECT_EQ(target.get()->lpszProxyBypass, nullptr);
  EXPECT_EQ(target.get()->dwAccessType,
            static_cast<DWORD>(WINHTTP_ACCESS_TYPE_DEFAULT_PROXY));
}

TEST(ScopedWinHttpProxyInfoTest, MoveAssignReplacesOwnedStrings) {
  ScopedWinHttpProxyInfo source;
  source.set_access_type(WINHTTP_ACCESS_TYPE_NAMED_PROXY);
  source.SetProxy(L"foo:80");
  source.SetProxyBypass(L"bar");

  ScopedWinHttpProxyInfo target;
  target.set_access_type(WINHTTP_ACCESS_TYPE_NO_PROXY);
  target.SetProxy(L"old:8080");
  target.SetProxyBypass(L"old_bypass");

  target = std::move(source);

  EXPECT_FALSE(source.has_proxy());         // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(source.get()->lpszProxyBypass,  // NOLINT(bugprone-use-after-move)
            nullptr);
  EXPECT_EQ(source.get()->dwAccessType,  // NOLINT(bugprone-use-after-move)
            static_cast<DWORD>(WINHTTP_ACCESS_TYPE_DEFAULT_PROXY));
  ASSERT_TRUE(target.has_proxy());
  EXPECT_EQ(target.get()->dwAccessType,
            static_cast<DWORD>(WINHTTP_ACCESS_TYPE_NAMED_PROXY));
  EXPECT_STREQ(target.get()->lpszProxy, L"foo:80");
  EXPECT_STREQ(target.get()->lpszProxyBypass, L"bar");
}

TEST(ScopedWinHttpProxyInfoTest, MoveAssignFromEmptyFreesOwnedStrings) {
  ScopedWinHttpProxyInfo source;

  ScopedWinHttpProxyInfo target;
  target.set_access_type(WINHTTP_ACCESS_TYPE_NAMED_PROXY);
  target.SetProxy(L"old:8080");
  target.SetProxyBypass(L"old_bypass");

  target = std::move(source);

  EXPECT_FALSE(target.has_proxy());
  EXPECT_EQ(target.get()->lpszProxyBypass, nullptr);
  EXPECT_EQ(target.get()->dwAccessType,
            static_cast<DWORD>(WINHTTP_ACCESS_TYPE_DEFAULT_PROXY));
}

TEST(ScopedWinHttpProxyInfoTest, SelfMoveAssignKeepsOwnedStrings) {
  ScopedWinHttpProxyInfo info;
  info.set_access_type(WINHTTP_ACCESS_TYPE_NAMED_PROXY);
  info.SetProxy(L"foo:80");
  info.SetProxyBypass(L"bar");

  ScopedWinHttpProxyInfo& alias = info;
  info = std::move(alias);

  ASSERT_TRUE(info.has_proxy());
  EXPECT_EQ(info.get()->dwAccessType,
            static_cast<DWORD>(WINHTTP_ACCESS_TYPE_NAMED_PROXY));
  EXPECT_STREQ(info.get()->lpszProxy, L"foo:80");
  EXPECT_STREQ(info.get()->lpszProxyBypass, L"bar");
}

TEST(ScopedWinHttpProxyInfoTest, SetReplacesOwnedStrings) {
  ScopedWinHttpProxyInfo info;
  info.SetProxy(L"old:8080");
  info.SetProxyBypass(L"old_bypass");

  info.SetProxy(L"foo:80");
  info.SetProxyBypass(L"bar");

  ASSERT_TRUE(info.has_proxy());
  EXPECT_STREQ(info.get()->lpszProxy, L"foo:80");
  EXPECT_STREQ(info.get()->lpszProxyBypass, L"bar");
}

TEST(ScopedWinHttpProxyInfoTest, SetEmptyKeepsOwnedStrings) {
  ScopedWinHttpProxyInfo info;
  info.SetProxy(L"foo:80");
  info.SetProxyBypass(L"bar");

  info.SetProxy(L"");
  info.SetProxyBypass(L"");

  ASSERT_TRUE(info.has_proxy());
  EXPECT_STREQ(info.get()->lpszProxy, L"foo:80");
  EXPECT_STREQ(info.get()->lpszProxyBypass, L"bar");
}

TEST(ScopedWinHttpProxyInfoTest, ReceiveFreesOwnedStrings) {
  ScopedWinHttpProxyInfo info;
  info.set_access_type(WINHTTP_ACCESS_TYPE_NAMED_PROXY);
  info.SetProxy(L"foo:80");
  info.SetProxyBypass(L"bar");

  WINHTTP_PROXY_INFO* received = info.Receive();

  ASSERT_EQ(received, info.get());
  EXPECT_FALSE(info.has_proxy());
  EXPECT_EQ(received->lpszProxyBypass, nullptr);
  EXPECT_EQ(received->dwAccessType,
            static_cast<DWORD>(WINHTTP_ACCESS_TYPE_DEFAULT_PROXY));
}

}  // namespace winhttp
