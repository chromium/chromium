// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver_win/winhttp_proxy_resolver_functions.h"

#include "base/no_destructor.h"

namespace proxy_resolver_win {

WinHttpProxyResolverFunctions::WinHttpProxyResolverFunctions() {
  HMODULE winhttp_module =
      LoadLibraryEx(L"winhttp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (winhttp_module) {
    create_proxy_resolver = reinterpret_cast<WinHttpCreateProxyResolverFunc>(
        ::GetProcAddress(winhttp_module, "WinHttpCreateProxyResolver"));
    get_proxy_for_url_ex = reinterpret_cast<WinHttpGetProxyForUrlExFunc>(
        ::GetProcAddress(winhttp_module, "WinHttpGetProxyForUrlEx"));
    get_proxy_result = reinterpret_cast<WinHttpGetProxyResultFunc>(
        ::GetProcAddress(winhttp_module, "WinHttpGetProxyResult"));
    free_proxy_result = reinterpret_cast<WinHttpFreeProxyResultFunc>(
        ::GetProcAddress(winhttp_module, "WinHttpFreeProxyResult"));
  }
}

// Never called due to base::NoDestructor.
WinHttpProxyResolverFunctions::~WinHttpProxyResolverFunctions() = default;

bool WinHttpProxyResolverFunctions::are_all_functions_loaded() const {
  return create_proxy_resolver && get_proxy_for_url_ex && get_proxy_result &&
         free_proxy_result;
}

// static
const WinHttpProxyResolverFunctions&
WinHttpProxyResolverFunctions::GetInstance() {
  // This is a singleton for performance reasons. This avoids having to load
  // proxy resolver functions multiple times.
  static base::NoDestructor<WinHttpProxyResolverFunctions> instance;
  return *instance;
}

}  // namespace proxy_resolver_win
