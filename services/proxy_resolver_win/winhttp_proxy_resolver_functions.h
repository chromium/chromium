// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_WIN_WINHTTP_PROXY_RESOLVER_FUNCTIONS_H_
#define SERVICES_PROXY_RESOLVER_WIN_WINHTTP_PROXY_RESOLVER_FUNCTIONS_H_

#include <windows.h>
#include <winhttp.h>

#include "base/no_destructor.h"

namespace proxy_resolver_win {

// Not all WinHttp APIs we'll be using exist in all versions of Windows.
// Several only exist in Windows 8+. Thus, each function entry point must be
// loaded dynamically.
struct WinHttpProxyResolverFunctions {
 public:
  WinHttpProxyResolverFunctions(const WinHttpProxyResolverFunctions&) = delete;
  WinHttpProxyResolverFunctions& operator=(
      const WinHttpProxyResolverFunctions&) = delete;

  bool are_all_functions_loaded() const;

  static const WinHttpProxyResolverFunctions& GetInstance();

  using WinHttpCreateProxyResolverFunc = decltype(WinHttpCreateProxyResolver)*;
  using WinHttpGetProxyForUrlExFunc = decltype(WinHttpGetProxyForUrlEx)*;
  using WinHttpGetProxyResultFunc = decltype(WinHttpGetProxyResult)*;
  using WinHttpFreeProxyResultFunc = decltype(WinHttpFreeProxyResult)*;

  WinHttpCreateProxyResolverFunc create_proxy_resolver = nullptr;
  WinHttpGetProxyForUrlExFunc get_proxy_for_url_ex = nullptr;
  WinHttpGetProxyResultFunc get_proxy_result = nullptr;
  WinHttpFreeProxyResultFunc free_proxy_result = nullptr;

 private:
  friend class base::NoDestructor<WinHttpProxyResolverFunctions>;

  WinHttpProxyResolverFunctions();
  ~WinHttpProxyResolverFunctions();
};

}  // namespace proxy_resolver_win

#endif  // SERVICES_PROXY_RESOLVER_WIN_WINHTTP_PROXY_RESOLVER_FUNCTIONS_H_
