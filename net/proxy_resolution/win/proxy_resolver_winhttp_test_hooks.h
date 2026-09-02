// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_WIN_PROXY_RESOLVER_WINHTTP_TEST_HOOKS_H_
#define NET_PROXY_RESOLUTION_WIN_PROXY_RESOLVER_WINHTTP_TEST_HOOKS_H_

#include <windows.h>

#include <winhttp.h>

#include "net/base/net_export.h"

namespace net {

using WinHttpGetProxyForUrlFunc = BOOL(WINAPI*)(HINTERNET,
                                                LPCWSTR,
                                                WINHTTP_AUTOPROXY_OPTIONS*,
                                                WINHTTP_PROXY_INFO*);

// Scoped override of the WinHttpGetProxyForUrl function used by
// ProxyResolverWinHttp. Passing nullptr restores the default
// (::WinHttpGetProxyForUrl).
class NET_EXPORT_PRIVATE ScopedWinHttpGetProxyForUrlOverride {
 public:
  explicit ScopedWinHttpGetProxyForUrlOverride(WinHttpGetProxyForUrlFunc func);
  ~ScopedWinHttpGetProxyForUrlOverride();

  ScopedWinHttpGetProxyForUrlOverride(
      const ScopedWinHttpGetProxyForUrlOverride&) = delete;
  ScopedWinHttpGetProxyForUrlOverride& operator=(
      const ScopedWinHttpGetProxyForUrlOverride&) = delete;

 private:
  WinHttpGetProxyForUrlFunc previous_func_;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_WIN_PROXY_RESOLVER_WINHTTP_TEST_HOOKS_H_
