// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/winhttp/scoped_winhttp_proxy_info.h"

#include <windows.h>

#include <stddef.h>
#include <winhttp.h>

#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/logging.h"

namespace winhttp {
namespace {

// Frees `str` with GlobalFree, if set, and sets it to null.
void FreeString(wchar_t*& str) {
  if (str) {
    ::GlobalFree(str);
    str = nullptr;
  }
}

// Returns a GlobalAlloc-ed copy of `str`, or null if the allocation fails.
wchar_t* AllocGlobalString(const std::wstring& str) {
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

}  // namespace

ScopedWinHttpProxyInfo::ScopedWinHttpProxyInfo(
    ScopedWinHttpProxyInfo&& other) noexcept
    : proxy_info_(std::exchange(other.proxy_info_, {})) {}

ScopedWinHttpProxyInfo& ScopedWinHttpProxyInfo::operator=(
    ScopedWinHttpProxyInfo&& other) noexcept {
  if (this != &other) {
    FreeStrings();
    proxy_info_ = std::exchange(other.proxy_info_, {});
  }
  return *this;
}

ScopedWinHttpProxyInfo::~ScopedWinHttpProxyInfo() {
  FreeStrings();
}

void ScopedWinHttpProxyInfo::SetProxy(const std::wstring& proxy) {
  if (proxy.empty()) {
    return;
  }

  if (wchar_t* str = AllocGlobalString(proxy)) {
    FreeString(proxy_info_.lpszProxy);
    proxy_info_.lpszProxy = str;
  }
}

void ScopedWinHttpProxyInfo::SetProxyBypass(const std::wstring& proxy_bypass) {
  if (proxy_bypass.empty()) {
    return;
  }

  if (wchar_t* str = AllocGlobalString(proxy_bypass)) {
    FreeString(proxy_info_.lpszProxyBypass);
    proxy_info_.lpszProxyBypass = str;
  }
}

WINHTTP_PROXY_INFO* ScopedWinHttpProxyInfo::Receive() {
  FreeStrings();
  proxy_info_ = {};
  return &proxy_info_;
}

void ScopedWinHttpProxyInfo::FreeStrings() {
  FreeString(proxy_info_.lpszProxy);
  FreeString(proxy_info_.lpszProxyBypass);
}

}  // namespace winhttp
