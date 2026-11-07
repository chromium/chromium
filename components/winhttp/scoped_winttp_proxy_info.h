// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WINHTTP_SCOPED_WINTTP_PROXY_INFO_H_
#define COMPONENTS_WINHTTP_SCOPED_WINTTP_PROXY_INFO_H_

#include <windows.h>

#include <stddef.h>
#include <winhttp.h>

#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/logging.h"

namespace winhttp {

// Owns a WINHTTP_PROXY_INFO structure and the GlobalAlloc-ed strings it points
// to, and frees the strings with GlobalFree. Move-only.
class ScopedWinHttpProxyInfo {
 public:
  ScopedWinHttpProxyInfo() = default;

  ScopedWinHttpProxyInfo(const ScopedWinHttpProxyInfo& other) = delete;
  ScopedWinHttpProxyInfo& operator=(const ScopedWinHttpProxyInfo& other) =
      delete;
  ScopedWinHttpProxyInfo(ScopedWinHttpProxyInfo&& other) noexcept
      : proxy_info_(std::exchange(other.proxy_info_, {})) {}

  ScopedWinHttpProxyInfo& operator=(ScopedWinHttpProxyInfo&& other) noexcept {
    if (this != &other) {
      FreeStrings();
      proxy_info_ = std::exchange(other.proxy_info_, {});
    }
    return *this;
  }

  ~ScopedWinHttpProxyInfo() { FreeStrings(); }

  bool IsValid() const { return proxy_info_.lpszProxy; }

  void set_access_type(DWORD access_type) {
    proxy_info_.dwAccessType = access_type;
  }

  wchar_t* proxy() const { return proxy_info_.lpszProxy; }

  // Replaces the proxy string, unless `proxy` is empty or the allocation fails.
  void set_proxy(const std::wstring& proxy) {
    if (proxy.empty()) {
      return;
    }

    if (wchar_t* str = GlobalAlloc(proxy)) {
      FreeString(proxy_info_.lpszProxy);
      proxy_info_.lpszProxy = str;
    }
  }

  // Replaces the proxy bypass string, unless `proxy_bypass` is empty or the
  // allocation fails.
  void set_proxy_bypass(const std::wstring& proxy_bypass) {
    if (proxy_bypass.empty()) {
      return;
    }

    if (wchar_t* str = GlobalAlloc(proxy_bypass)) {
      FreeString(proxy_info_.lpszProxyBypass);
      proxy_info_.lpszProxyBypass = str;
    }
  }

  const WINHTTP_PROXY_INFO* get() const { return &proxy_info_; }

  // Frees the owned strings and returns the cleared structure for a Win32 API
  // to fill in. This object takes ownership of the strings, which must be
  // allocated with GlobalAlloc.
  WINHTTP_PROXY_INFO* receive() {
    FreeStrings();
    proxy_info_ = {};
    return &proxy_info_;
  }

 private:
  static void FreeString(wchar_t*& str) {
    if (str) {
      ::GlobalFree(str);
      str = nullptr;
    }
  }

  void FreeStrings() {
    FreeString(proxy_info_.lpszProxy);
    FreeString(proxy_info_.lpszProxyBypass);
  }

  wchar_t* GlobalAlloc(const std::wstring& str) {
    const size_t length = str.length() + 1;
    const size_t size_in_bytes = length * sizeof(wchar_t);
    wchar_t* string_mem =
        static_cast<wchar_t*>(::GlobalAlloc(GPTR, size_in_bytes));

    if (!string_mem) {
      PLOG(ERROR) << "GlobalAlloc failed to allocate " << size_in_bytes
                  << " bytes";
      return nullptr;
    }

    // SAFETY: `string_mem` was just allocated by `::GlobalAlloc` with room for
    // `length` wide characters.
    base::span<wchar_t> dest = UNSAFE_BUFFERS(base::span(string_mem, length));
    dest.copy_prefix_from(str);
    dest.back() = L'\0';
    return string_mem;
  }
  WINHTTP_PROXY_INFO proxy_info_ = {};
};

}  // namespace winhttp

#endif  // COMPONENTS_WINHTTP_SCOPED_WINTTP_PROXY_INFO_H_
