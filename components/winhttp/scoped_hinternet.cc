// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/winhttp/scoped_hinternet.h"

#include <windows.h>

#include <versionhelpers.h>

#include <string_view>
#include <utility>

namespace winhttp {

ScopedHInternet CreateSessionHandle(std::wstring_view user_agent,
                                    int proxy_access_type,
                                    std::wstring_view proxy,
                                    std::wstring_view proxy_bypass) {
  ScopedHInternet session_handle(::WinHttpOpen(
      user_agent.data(), proxy_access_type,
      proxy.empty() ? WINHTTP_NO_PROXY_NAME : proxy.data(),
      proxy_bypass.empty() ? WINHTTP_NO_PROXY_BYPASS : proxy_bypass.data(),
      WINHTTP_FLAG_ASYNC));

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

  // WINHTTP_OPTION_DECOMPRESSION is supported on Windows 8.1 and higher.
  if (session_handle.is_valid() && ::IsWindows8Point1OrGreater()) {
    DWORD decompression_flag = WINHTTP_DECOMPRESSION_FLAG_ALL;
    ::WinHttpSetOption(session_handle.get(), WINHTTP_OPTION_DECOMPRESSION,
                       &decompression_flag, sizeof(decompression_flag));
  }
  return session_handle;
}

SharedHInternet::SharedHInternet(ScopedHInternet handle)
    : handle_(std::move(handle)) {}
SharedHInternet::~SharedHInternet() = default;

}  // namespace winhttp
