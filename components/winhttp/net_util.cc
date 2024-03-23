// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/winhttp/net_util.h"

#include <ostream>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/strings/sys_string_conversions.h"

namespace winhttp {

HRESULT HRESULTFromLastError() {
  const DWORD error_code = ::GetLastError();
  return (error_code != NO_ERROR) ? HRESULT_FROM_WIN32(error_code) : E_FAIL;
}

HRESULT QueryHeadersString(HINTERNET request_handle,
                           uint32_t info_level,
                           const wchar_t* name,
                           std::wstring* value) {
  DWORD num_bytes = 0;
  ::WinHttpQueryHeaders(request_handle, info_level, name,
                        WINHTTP_NO_OUTPUT_BUFFER, &num_bytes,
                        WINHTTP_NO_HEADER_INDEX);
  const HRESULT hr = HRESULTFromLastError();
  if (hr != HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) {
    return hr;
  }
  std::vector<wchar_t> buffer(num_bytes / sizeof(wchar_t));
  if (!::WinHttpQueryHeaders(request_handle, info_level, name, &buffer.front(),
                             &num_bytes, WINHTTP_NO_HEADER_INDEX)) {
    return HRESULTFromLastError();
  }
  CHECK_EQ(num_bytes % sizeof(wchar_t), 0u);
  buffer.resize(num_bytes / sizeof(wchar_t));
  value->assign(buffer.begin(), buffer.end());
  return S_OK;
}

HRESULT QueryHeadersInt(HINTERNET request_handle,
                        uint32_t info_level,
                        const wchar_t* name,
                        int* value) {
  info_level |= WINHTTP_QUERY_FLAG_NUMBER;
  DWORD num_bytes = sizeof(*value);
  if (!::WinHttpQueryHeaders(request_handle, info_level, name, value,
                             &num_bytes, WINHTTP_NO_HEADER_INDEX)) {
    return HRESULTFromLastError();
  }
  return S_OK;
}

std::ostream& operator<<(std::ostream& os,
                         const WINHTTP_PROXY_INFO& proxy_info) {
  os << "access type=" <<
      [&proxy_info] {
        switch (proxy_info.dwAccessType) {
          case WINHTTP_ACCESS_TYPE_NO_PROXY:
            return "no proxy";
          case WINHTTP_ACCESS_TYPE_DEFAULT_PROXY:
            return "default proxy";
          case WINHTTP_ACCESS_TYPE_NAMED_PROXY:
            return "named proxy";
          default:
            return "unknown";
        }
      }()
     << ", proxy="
     << base::SysWideToUTF8(proxy_info.lpszProxy ? proxy_info.lpszProxy
                                                 : L"null")
     << ", bypass="
     << base::SysWideToUTF8(
            proxy_info.lpszProxyBypass ? proxy_info.lpszProxyBypass : L"null");
  return os;
}

}  // namespace winhttp
