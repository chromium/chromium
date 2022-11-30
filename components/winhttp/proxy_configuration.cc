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

ProxyConfiguration::ProxyConfiguration(const ProxyInfo& proxy_info)
    : proxy_info_(proxy_info) {}

int ProxyConfiguration::access_type() const {
  return DoGetAccessType();
}

int ProxyConfiguration::DoGetAccessType() const {
  const bool is_using_named_proxy = !proxy_info_.auto_detect &&
                                    proxy_info_.auto_config_url.empty() &&
                                    !proxy_info_.proxy.empty();

  return is_using_named_proxy ? WINHTTP_ACCESS_TYPE_NAMED_PROXY
                              : WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
}

absl::optional<ScopedWinHttpProxyInfo> ProxyConfiguration::GetProxyForUrl(
    HINTERNET session_handle,
    const GURL& url) const {
  return DoGetProxyForUrl(session_handle, url);
}

absl::optional<ScopedWinHttpProxyInfo> ProxyConfiguration::DoGetProxyForUrl(
    HINTERNET session_handle,
    const GURL& url) const {
  // Detect proxy settings using Web Proxy Auto Detection (WPAD).
  WINHTTP_AUTOPROXY_OPTIONS auto_proxy_options = {0};

  // Per MSDN, setting fAutoLogonIfChallenged to false first may work
  // if Windows cached the proxy config.
  auto_proxy_options.fAutoLogonIfChallenged = false;

  bool try_auto_proxy = false;

  if (proxy_info_.auto_detect) {
    auto_proxy_options.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
    auto_proxy_options.dwAutoDetectFlags =
        WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A;
    try_auto_proxy = true;
  }

  // PAC Url was specified, let system auto detect given the PAC url.
  if (!proxy_info_.auto_config_url.empty()) {
    auto_proxy_options.dwFlags |= WINHTTP_AUTOPROXY_CONFIG_URL;
    auto_proxy_options.lpszAutoConfigUrl = proxy_info_.auto_config_url.c_str();
    try_auto_proxy = true;
  }

  // Find the proxy server for the url.
  ScopedWinHttpProxyInfo winhttp_proxy_info = {};
  if (try_auto_proxy) {
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

    if (!success) {
      PLOG(ERROR) << "Failed to get proxy for url";
      return {};
    }
  } else {
    winhttp_proxy_info.set_proxy(proxy_info_.proxy);
    winhttp_proxy_info.set_proxy_bypass(proxy_info_.proxy_bypass);
  }

  if (!winhttp_proxy_info.IsValid())
    return {};

  return winhttp_proxy_info;
}

void SetProxyForRequest(
    const HINTERNET request_handle,
    const absl::optional<ScopedWinHttpProxyInfo>& winhttp_proxy_info) {
  // Set the proxy option on the request handle.
  if (winhttp_proxy_info.has_value() && winhttp_proxy_info.value().IsValid()) {
    const ScopedWinHttpProxyInfo& proxy_info = winhttp_proxy_info.value();
    VLOG(1) << "Setting proxy " << proxy_info.proxy();
    auto hr = SetOption(request_handle, WINHTTP_OPTION_PROXY,
                        const_cast<WINHTTP_PROXY_INFO*>(proxy_info.get()));
    if (FAILED(hr)) {
      PLOG(ERROR) << "Failed to set WINHTTP_OPTION_PROXY: 0x" << std::hex << hr;
    }
  }
}

int AutoProxyConfiguration::DoGetAccessType() const {
  return WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY;
}

absl::optional<ScopedWinHttpProxyInfo> AutoProxyConfiguration::DoGetProxyForUrl(
    HINTERNET,
    const GURL&) const {
  // When using automatic proxy settings, Windows will resolve the proxy
  // for us.
  DVLOG(3) << "Auto-proxy: skip getting proxy for a url";
  return {};
}

}  // namespace winhttp
