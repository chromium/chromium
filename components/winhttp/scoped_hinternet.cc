// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/winhttp/scoped_hinternet.h"

#include <windows.h>

#include <utility>

#include "base/logging.h"

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
  if (!session_handle.is_valid()) {
    return session_handle;
  }

  if (DWORD decompression_flag = WINHTTP_DECOMPRESSION_FLAG_ALL;
      !::WinHttpSetOption(session_handle.get(), WINHTTP_OPTION_DECOMPRESSION,
                          &decompression_flag, sizeof(decompression_flag))) {
    VLOG(1) << "Failed to configure WINHTTP_DECOMPRESSION_FLAG_ALL";
  }

  return session_handle;
}

SharedHInternet::SharedHInternet(ScopedHInternet handle)
    : handle_(std::move(handle)) {}
SharedHInternet::~SharedHInternet() = default;

}  // namespace winhttp
