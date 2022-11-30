// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WINHTTP_PROXY_INFO_H_
#define COMPONENTS_WINHTTP_PROXY_INFO_H_

#include <string>

namespace winhttp {

struct ProxyInfo {
  ProxyInfo();
  ProxyInfo(bool auto_detect,
            const std::wstring& auto_config_url,
            const std::wstring& proxy,
            const std::wstring& proxy_bypass);
  ~ProxyInfo();

  ProxyInfo(const ProxyInfo& proxy_info);
  ProxyInfo& operator=(const ProxyInfo& proxy_info);

  ProxyInfo(ProxyInfo&& proxy_info);
  ProxyInfo& operator=(ProxyInfo&& proxy_info);

  // Specifies the configuration is Web Proxy Auto Discovery (WPAD).
  bool auto_detect = false;

  // The url of the proxy auto configuration (PAC) script, if known.
  std::wstring auto_config_url;

  // Named proxy information.
  // The proxy string is usually something as "http=foo:80;https=bar:8080".
  // According to the documentation for WINHTTP_PROXY_INFO, multiple proxies
  // are separated by semicolons or whitespace.
  std::wstring proxy;
  std::wstring proxy_bypass;
};

}  // namespace winhttp

#endif  // COMPONENTS_WINHTTP_PROXY_INFO_H_
