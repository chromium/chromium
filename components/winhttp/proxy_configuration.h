// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WINHTTP_PROXY_CONFIGURATION_H_
#define COMPONENTS_WINHTTP_PROXY_CONFIGURATION_H_

#include <windows.h>

#include <winhttp.h>

#include <optional>

#include "base/memory/ref_counted.h"
#include "components/winhttp/proxy_info.h"
#include "components/winhttp/scoped_winttp_proxy_info.h"

class GURL;

namespace winhttp {

// On Windows 8.1 and above, we can use Auto Proxy mode in WinHTTP and let
// the OS configure the proxy.
// This is represented by the AutoProxyConfiguration class.
// When a proxy related policy is set or when using Windows 8 or below,
// we'd set proxy manually using ProxyConfiguration class.
//
// Default WinHTTP proxy strategy - provide proxy server info to WinHTTP.
// Used when policy is set or on Windows 8 and below.
// This class can use either provided proxy information from the
// IE Config or a policy policy setting or query per request proxy info
// with WPAD (Web Proxy Auto Discovery).
class ProxyConfiguration : public base::RefCounted<ProxyConfiguration> {
 public:
  ProxyConfiguration() = default;
  explicit ProxyConfiguration(const ProxyInfo& proxy_info);
  ProxyConfiguration(const ProxyConfiguration&) = delete;
  ProxyConfiguration& operator=(const ProxyConfiguration&) = delete;

  int access_type() const;
  std::wstring proxy() const;
  std::wstring proxy_bypass() const;

  std::optional<ScopedWinHttpProxyInfo> GetProxyForUrl(HINTERNET session_handle,
                                                       const GURL& url) const;
 protected:
  virtual ~ProxyConfiguration() = default;

 private:
  friend class base::RefCounted<ProxyConfiguration>;

  virtual int DoGetAccessType() const;
  virtual std::optional<ScopedWinHttpProxyInfo> DoGetProxyForUrl(
      HINTERNET session_handle,
      const GURL& url) const;

  ProxyInfo proxy_info_;
};

// Auto proxy strategy - let WinHTTP detect proxy settings.
// This is only available on Windows 8.1 and above.
class AutoProxyConfiguration final : public ProxyConfiguration {
 protected:
  ~AutoProxyConfiguration() override = default;

 private:
  // Overrides for ProxyConfiguration.
  int DoGetAccessType() const override;
  std::optional<ScopedWinHttpProxyInfo> DoGetProxyForUrl(
      HINTERNET session_handle,
      const GURL& url) const override;
};

// Sets proxy info on a request handle, if WINHTTP_PROXY_INFO is provided.
void SetProxyForRequest(
    HINTERNET request_handle,
    const std::optional<ScopedWinHttpProxyInfo>& winhttp_proxy_info);

}  // namespace winhttp

#endif  // COMPONENTS_WINHTTP_PROXY_CONFIGURATION_H_
