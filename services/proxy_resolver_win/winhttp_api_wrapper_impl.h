// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_WIN_WINHTTP_API_WRAPPER_IMPL_H_
#define SERVICES_PROXY_RESOLVER_WIN_WINHTTP_API_WRAPPER_IMPL_H_

#include <windows.h>

#include <winhttp.h>

#include <string>

#include "services/proxy_resolver_win/winhttp_api_wrapper.h"

namespace proxy_resolver_win {

// This is a utility class that encapsulates the memory management necessary for
// WINHTTP_CURRENT_USER_IE_PROXY_CONFIG in RAII style.
class ScopedIEConfig final {
 public:
  ScopedIEConfig();

  ScopedIEConfig(const ScopedIEConfig&) = delete;
  ScopedIEConfig& operator=(const ScopedIEConfig&) = delete;

  ~ScopedIEConfig();

  WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* config() { return &ie_config; }

 private:
  WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ie_config = {0};
};

// This is the implementation of WinHttpAPIWrapper that gets used in the
// product.
class WinHttpAPIWrapperImpl final : public WinHttpAPIWrapper {
 public:
  WinHttpAPIWrapperImpl();

  WinHttpAPIWrapperImpl(const WinHttpAPIWrapperImpl&) = delete;
  WinHttpAPIWrapperImpl& operator=(const WinHttpAPIWrapperImpl&) = delete;

  ~WinHttpAPIWrapperImpl() override;

  // WinHttpAPIWrapper Implementation
  bool CallWinHttpOpen() override;
  bool CallWinHttpSetTimeouts(int resolve_timeout,
                              int connect_timeout,
                              int send_timeout,
                              int receive_timeout) override;
  bool CallWinHttpSetStatusCallback(
      WINHTTP_STATUS_CALLBACK internet_callback) override;
  bool CallWinHttpGetIEProxyConfigForCurrentUser(
      WINHTTP_CURRENT_USER_IE_PROXY_CONFIG* ie_proxy_config) override;
  bool CallWinHttpCreateProxyResolver(HINTERNET* out_resolver_handle) override;
  bool CallWinHttpGetProxyForUrlEx(HINTERNET resolver_handle,
                                   const std::string& url,
                                   WINHTTP_AUTOPROXY_OPTIONS* autoproxy_options,
                                   DWORD_PTR context) override;
  bool CallWinHttpGetProxyResult(HINTERNET resolver_handle,
                                 WINHTTP_PROXY_RESULT* proxy_result) override;
  void CallWinHttpFreeProxyResult(WINHTTP_PROXY_RESULT* proxy_result) override;
  void CallWinHttpCloseHandle(HINTERNET internet_handle) override;

 private:
  // Closes |session_handle_|.
  void CloseSessionHandle();

  HINTERNET session_handle_ = nullptr;
};

}  // namespace proxy_resolver_win

#endif  // SERVICES_PROXY_RESOLVER_WIN_WINHTTP_API_WRAPPER_IMPL_H_
