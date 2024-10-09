// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WINHTTP_SCOPED_WINTTP_PROXY_INFO_H_
#define COMPONENTS_WINHTTP_SCOPED_WINTTP_PROXY_INFO_H_

#include <windows.h>

#include <winhttp.h>

#include <string>

#include "base/logging.h"

namespace winhttp {

// Wrapper class for the WINHTTP_PROXY_INFO structure.
// Note that certain Win32 APIs expected the strings to be allocated with
// with GlobalAlloc.
class ScopedWinHttpProxyInfo {
 public:
  ScopedWinHttpProxyInfo() = default;

  ScopedWinHttpProxyInfo(const ScopedWinHttpProxyInfo& other) = delete;
  ScopedWinHttpProxyInfo& operator=(const ScopedWinHttpProxyInfo& other) =
      delete;
  ScopedWinHttpProxyInfo(ScopedWinHttpProxyInfo&& other) {
    proxy_info_.dwAccessType = other.proxy_info_.dwAccessType;
    proxy_info_.lpszProxy = other.proxy_info_.lpszProxy;
    other.proxy_info_.lpszProxy = nullptr;

    proxy_info_.lpszProxyBypass = other.proxy_info_.lpszProxyBypass;
    other.proxy_info_.lpszProxyBypass = nullptr;
  }

  ScopedWinHttpProxyInfo& operator=(ScopedWinHttpProxyInfo&& other) {
    proxy_info_.dwAccessType = other.proxy_info_.dwAccessType;
    proxy_info_.lpszProxy = other.proxy_info_.lpszProxy;
    other.proxy_info_.lpszProxy = nullptr;

    proxy_info_.lpszProxyBypass = other.proxy_info_.lpszProxyBypass;
    other.proxy_info_.lpszProxyBypass = nullptr;
    return *this;
  }

  ~ScopedWinHttpProxyInfo() {
    if (proxy_info_.lpszProxy) {
      ::GlobalFree(proxy_info_.lpszProxy);
    }

    if (proxy_info_.lpszProxyBypass) {
      ::GlobalFree(proxy_info_.lpszProxyBypass);
    }
  }

  bool IsValid() const { return proxy_info_.lpszProxy; }

  void set_access_type(DWORD access_type) {
    proxy_info_.dwAccessType = access_type;
  }

  wchar_t* proxy() const { return proxy_info_.lpszProxy; }

  void set_proxy(const std::wstring& proxy) {
    if (proxy.empty()) {
      return;
    }

    proxy_info_.lpszProxy = GlobalAlloc(proxy);
  }

  void set_proxy_bypass(const std::wstring& proxy_bypass) {
    if (proxy_bypass.empty()) {
      return;
    }

    proxy_info_.lpszProxyBypass = GlobalAlloc(proxy_bypass);
  }

  const WINHTTP_PROXY_INFO* get() const { return &proxy_info_; }

  WINHTTP_PROXY_INFO* receive() { return &proxy_info_; }

 private:
  wchar_t* GlobalAlloc(const std::wstring& str) {
    const size_t size_in_bytes = (str.length() + 1) * sizeof(wchar_t);
    wchar_t* string_mem =
        reinterpret_cast<wchar_t*>(::GlobalAlloc(GPTR, size_in_bytes));

    if (!string_mem) {
      PLOG(ERROR) << "GlobalAlloc failed to allocate " << size_in_bytes
                  << " bytes";
      return nullptr;
    }

    memcpy(string_mem, str.data(), size_in_bytes);
    return string_mem;
  }
  WINHTTP_PROXY_INFO proxy_info_ = {};
};

}  // namespace winhttp

#endif  // COMPONENTS_WINHTTP_SCOPED_WINTTP_PROXY_INFO_H_
