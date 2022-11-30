// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/winhttp/scoped_hinternet.h"

#include <versionhelpers.h>
#include <windows.h>

namespace winhttp {

ScopedHInternet CreateSessionHandle(const wchar_t* user_agent,
                                    int proxy_access_type) {
  ScopedHInternet session_handle(
      ::WinHttpOpen(user_agent, proxy_access_type, WINHTTP_NO_PROXY_NAME,
                    WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC));

  // Allow TLS1.2 on Windows 7 and Windows 8. See KB3140245. TLS 1.2 is enabled
  // by default on Windows 8.1 and Windows 10.
  if (session_handle.is_valid() && ::IsWindows7OrGreater() &&
      !::IsWindows8Point1OrGreater()) {
    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 |
                      WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 |
                      WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
    ::WinHttpSetOption(session_handle.get(), WINHTTP_OPTION_SECURE_PROTOCOLS,
                       &protocols, sizeof(protocols));
  }
  return session_handle;
}

}  // namespace winhttp
