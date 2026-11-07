// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WINHTTP_SCOPED_WINHTTP_PROXY_INFO_H_
#define COMPONENTS_WINHTTP_SCOPED_WINHTTP_PROXY_INFO_H_

#include <windows.h>

#include <winhttp.h>

#include <string>

namespace winhttp {

// Owns a WINHTTP_PROXY_INFO structure and the GlobalAlloc-ed strings it points
// to, and frees the strings with GlobalFree. Move-only; a moved-from object is
// empty.
class ScopedWinHttpProxyInfo {
 public:
  ScopedWinHttpProxyInfo() = default;

  ScopedWinHttpProxyInfo(const ScopedWinHttpProxyInfo& other) = delete;
  ScopedWinHttpProxyInfo& operator=(const ScopedWinHttpProxyInfo& other) =
      delete;
  ScopedWinHttpProxyInfo(ScopedWinHttpProxyInfo&& other) noexcept;
  ScopedWinHttpProxyInfo& operator=(ScopedWinHttpProxyInfo&& other) noexcept;

  ~ScopedWinHttpProxyInfo();

  // Returns true if a proxy server is set.
  bool has_proxy() const { return proxy_info_.lpszProxy; }

  void set_access_type(DWORD access_type) {
    proxy_info_.dwAccessType = access_type;
  }

  // Replaces the proxy string, unless `proxy` is empty or the allocation fails.
  void SetProxy(const std::wstring& proxy);

  // Replaces the proxy bypass string, unless `proxy_bypass` is empty or the
  // allocation fails.
  void SetProxyBypass(const std::wstring& proxy_bypass);

  // Returns the owned structure. Its strings are valid until the next call to
  // `SetProxy()`, `SetProxyBypass()`, or `Receive()`, or until this object is
  // moved from or destroyed.
  const WINHTTP_PROXY_INFO* get() const { return &proxy_info_; }

  // Frees the owned strings and returns the cleared structure for a Win32 API
  // to fill in. This object takes ownership of the strings, which must be
  // allocated with GlobalAlloc.
  WINHTTP_PROXY_INFO* Receive();

 private:
  // Frees the owned strings and sets the pointers to null.
  void FreeStrings();

  WINHTTP_PROXY_INFO proxy_info_ = {};
};

}  // namespace winhttp

#endif  // COMPONENTS_WINHTTP_SCOPED_WINHTTP_PROXY_INFO_H_
