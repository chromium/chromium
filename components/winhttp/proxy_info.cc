// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/winhttp/proxy_info.h"

#include <string>

namespace winhttp {

ProxyInfo::ProxyInfo() = default;
ProxyInfo::~ProxyInfo() = default;
ProxyInfo::ProxyInfo(bool auto_detect,
                     const std::wstring& auto_config_url,
                     const std::wstring& proxy,
                     const std::wstring& proxy_bypass)
    : auto_detect(auto_detect),
      auto_config_url(auto_config_url),
      proxy(proxy),
      proxy_bypass(proxy_bypass) {}

ProxyInfo::ProxyInfo(const ProxyInfo& proxy_info) = default;
ProxyInfo& ProxyInfo::operator=(const ProxyInfo& proxy_info) = default;

ProxyInfo::ProxyInfo(ProxyInfo&& proxy_info) = default;
ProxyInfo& ProxyInfo::operator=(ProxyInfo&& proxy_info) = default;

}  // namespace winhttp
