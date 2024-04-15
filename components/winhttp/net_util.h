// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WINHTTP_NET_UTIL_H_
#define COMPONENTS_WINHTTP_NET_UTIL_H_

#include <windows.h>

#include <stdint.h>
#include <winhttp.h>

#include <ostream>
#include <string>

#include "base/check_op.h"

namespace winhttp {

std::ostream& operator<<(std::ostream& os,
                         const WINHTTP_PROXY_INFO& proxy_info);

// Returns the last error as an HRESULT or E_FAIL if last error is NO_ERROR.
// This is not a drop in replacement for the HRESULT_FROM_WIN32 macro.
// The macro maps a NO_ERROR to S_OK, whereas the HRESULTFromLastError maps a
// NO_ERROR to E_FAIL.
HRESULT HRESULTFromLastError();

// Returns HTTP response headers from the given request as strings.
HRESULT QueryHeadersString(HINTERNET request_handle,
                           uint32_t info_level,
                           const wchar_t* name,
                           std::wstring* value);

// Returns HTTP response headers from the given request as integers.
HRESULT QueryHeadersInt(HINTERNET request_handle,
                        uint32_t info_level,
                        const wchar_t* name,
                        int* value);

// Queries WinHTTP options for the given |handle|. Returns S_OK if the call
// is successful.
template <typename T>
HRESULT QueryOption(HINTERNET handle, uint32_t option, T* value) {
  DWORD num_bytes = sizeof(*value);
  if (!::WinHttpQueryOption(handle, option, value, &num_bytes)) {
    return HRESULTFromLastError();
  }
  CHECK_EQ(sizeof(*value), num_bytes);
  return S_OK;
}

// Sets WinHTTP options for the given |handle|. Returns S_OK if the call
// is successful.
template <typename T>
HRESULT SetOption(HINTERNET handle, uint32_t option, T* value) {
  if (!::WinHttpSetOption(handle, option, value, sizeof(T))) {
    return HRESULTFromLastError();
  }
  return S_OK;
}

template <typename T>
HRESULT SetOption(HINTERNET handle, uint32_t option, T value) {
  return SetOption(handle, option, &value);
}

}  // namespace winhttp

#endif  // COMPONENTS_WINHTTP_NET_UTIL_H_
