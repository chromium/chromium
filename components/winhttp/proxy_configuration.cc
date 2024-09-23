// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/winhttp/proxy_configuration.h"

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "components/winhttp/net_util.h"
#include "components/winhttp/proxy_info.h"
#include "components/winhttp/scoped_winttp_proxy_info.h"
#include "url/gurl.h"

namespace winhttp {

void SetProxyForRequest(
    HINTERNET request_handle,
    const std::optional<ScopedWinHttpProxyInfo>& winhttp_proxy_info) {
  if (winhttp_proxy_info.has_value() && winhttp_proxy_info.value().IsValid()) {
    const ScopedWinHttpProxyInfo& proxy_info = winhttp_proxy_info.value();
    VLOG(1) << "Setting proxy: " << *(proxy_info.get());
    HRESULT hr = SetOption(request_handle, WINHTTP_OPTION_PROXY,
                           const_cast<WINHTTP_PROXY_INFO*>(proxy_info.get()));
    if (FAILED(hr)) {
      PLOG(ERROR) << "Failed to set WINHTTP_OPTION_PROXY: 0x" << std::hex << hr;
    }
  }
}

ProxyConfiguration::ProxyConfiguration(const ProxyInfo& proxy_info)
    : proxy_info_(proxy_info) {}

int ProxyConfiguration::access_type() const {
  return DoGetAccessType();
}

std::wstring ProxyConfiguration::proxy() const {
  return proxy_info_.proxy;
}

std::wstring ProxyConfiguration::proxy_bypass() const {
  return proxy_info_.proxy_bypass;
}

// The access type is used when initializing a WinHTTP session and its handle.
// If the configuration specifies a PAC script, then the actual proxy is
// resolved by calling `WinHttpGetProxyForUrl`, and set on the request handle
// later on.
int ProxyConfiguration::DoGetAccessType() const {
  if (proxy_info_.auto_detect) {
    return WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY;
  } else if (!proxy_info_.proxy.empty()) {
    return WINHTTP_ACCESS_TYPE_NAMED_PROXY;
  } else {
    return WINHTTP_ACCESS_TYPE_NO_PROXY;
  }
}

std::optional<ScopedWinHttpProxyInfo> ProxyConfiguration::GetProxyForUrl(
    HINTERNET session_handle,
    const GURL& url) const {
  return DoGetProxyForUrl(session_handle, url);
}

std::optional<ScopedWinHttpProxyInfo> ProxyConfiguration::DoGetProxyForUrl(
    HINTERNET session_handle,
    const GURL& url) const {
  WINHTTP_AUTOPROXY_OPTIONS auto_proxy_options = {0};
  if (proxy_info_.auto_detect) {
    VLOG(1) << __func__ << ": proxy auto-detect.";
    auto_proxy_options.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
    auto_proxy_options.dwAutoDetectFlags =
        WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A;
  }
  if (!proxy_info_.auto_config_url.empty()) {
    VLOG(1) << __func__ << ": proxy PAC=" << proxy_info_.auto_config_url;
    auto_proxy_options.dwFlags |= WINHTTP_AUTOPROXY_CONFIG_URL;
    auto_proxy_options.lpszAutoConfigUrl = proxy_info_.auto_config_url.c_str();
  }

  ScopedWinHttpProxyInfo winhttp_proxy_info;
  if (auto_proxy_options.dwFlags) {
    // Per MSDN, setting `fAutoLogonIfChallenged` to false first may work
    // if Windows cached the proxy config.
    auto_proxy_options.fAutoLogonIfChallenged = false;
    const std::wstring url_str = base::SysUTF8ToWide(url.spec());
    bool success = ::WinHttpGetProxyForUrl(session_handle, url_str.c_str(),
                                           &auto_proxy_options,
                                           winhttp_proxy_info.receive());
    if (!success && ::GetLastError() == ERROR_WINHTTP_LOGIN_FAILURE) {
      auto_proxy_options.fAutoLogonIfChallenged = true;
      success = ::WinHttpGetProxyForUrl(session_handle, url_str.c_str(),
                                        &auto_proxy_options,
                                        winhttp_proxy_info.receive());
    }
    VLOG_IF(1, !success) << "Failed to auto-detect proxy info.";
  } else {
    winhttp_proxy_info.set_access_type(WINHTTP_ACCESS_TYPE_NAMED_PROXY);
    winhttp_proxy_info.set_proxy(proxy_info_.proxy);
    winhttp_proxy_info.set_proxy_bypass(proxy_info_.proxy_bypass);
  }

  if (!winhttp_proxy_info.IsValid()) {
    return std::nullopt;
  }
  return winhttp_proxy_info;
}

int AutoProxyConfiguration::DoGetAccessType() const {
  return WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY;
}

std::optional<ScopedWinHttpProxyInfo> AutoProxyConfiguration::DoGetProxyForUrl(
    HINTERNET,
    const GURL& url) const {
  // Windows resolves the proxy when auto-proxy option is used.
  VLOG(1) << "Auto-proxy: winhttp uses the user settings for proxy.";
  return {};
}

}  // namespace winhttp
