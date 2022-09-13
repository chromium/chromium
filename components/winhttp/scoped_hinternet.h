// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WINHTTP_SCOPED_HINTERNET_H_
#define COMPONENTS_WINHTTP_SCOPED_HINTERNET_H_

#include <windows.h>
#include <winhttp.h>

#include "base/scoped_generic.h"

namespace winhttp {

namespace internal {

struct ScopedHInternetTraits {
  static HINTERNET InvalidValue() { return nullptr; }
  static void Free(HINTERNET handle) {
    if (handle != InvalidValue())
      WinHttpCloseHandle(handle);
  }
};

}  // namespace internal

// Manages the lifetime of HINTERNET handles allocated by WinHTTP.
using ScopedHInternet =
    base::ScopedGeneric<HINTERNET, internal::ScopedHInternetTraits>;

// Creates a new WinHttp session using the given user agent and properly
// configured for the Windows OS version.
ScopedHInternet CreateSessionHandle(const wchar_t* user_agent,
                                    int proxy_access_type);

}  // namespace winhttp

#endif  // COMPONENTS_WINHTTP_SCOPED_HINTERNET_H_
