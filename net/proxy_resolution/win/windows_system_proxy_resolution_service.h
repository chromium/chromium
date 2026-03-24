// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_WIN_WINDOWS_SYSTEM_PROXY_RESOLUTION_SERVICE_H_
#define NET_PROXY_RESOLUTION_WIN_WINDOWS_SYSTEM_PROXY_RESOLUTION_SERVICE_H_

#include <memory>
#include <string>

#include "net/base/net_export.h"
#include "net/proxy_resolution/system_proxy_resolution_service.h"
#include "net/proxy_resolution/win/winhttp_status.h"

namespace net {

class NetLog;
class WindowsSystemProxyResolutionRequest;
class WindowsSystemProxyResolver;

// This class decides which proxy server(s) to use for a particular URL request.
// It does NOT support passing in fetched proxy configurations. Instead, it
// relies entirely on WinHttp APIs to determine the proxy that should be used
// for each network request.
class NET_EXPORT WindowsSystemProxyResolutionService
    : public SystemProxyResolutionService {
 public:
  [[nodiscard]] static bool IsSupported();

  // Creates a WindowsSystemProxyResolutionService or returns nullptr if the
  // runtime dependencies are not satisfied.
  static std::unique_ptr<WindowsSystemProxyResolutionService> Create(
      std::unique_ptr<WindowsSystemProxyResolver> windows_system_proxy_resolver,
      NetLog* net_log);

  WindowsSystemProxyResolutionService(
      const WindowsSystemProxyResolutionService&) = delete;
  WindowsSystemProxyResolutionService& operator=(
      const WindowsSystemProxyResolutionService&) = delete;

  ~WindowsSystemProxyResolutionService() override;

  // ProxyResolutionService implementation
  int ResolveProxy(const GURL& url,
                   const std::string& method,
                   const NetworkAnonymizationKey& network_anonymization_key,
                   ProxyInfo* results,
                   CompletionOnceCallback callback,
                   std::unique_ptr<ProxyResolutionRequest>* request,
                   const NetLogWithSource& net_log,
                   RequestPriority priority) override;

 private:
  friend class WindowsSystemProxyResolutionRequest;

  WindowsSystemProxyResolutionService(
      std::unique_ptr<WindowsSystemProxyResolver> windows_system_proxy_resolver,
      NetLog* net_log);

  // SystemProxyResolutionService:
  base::DictValue GetProxySettingsForNetLog() override;

  // Called when proxy resolution has completed (either synchronously or
  // asynchronously). Handles logging the result, and cleaning out
  // bad entries from the results list.
  int DidFinishResolvingProxy(
      const GURL& url,
      const std::string& method,
      const NetworkAnonymizationKey& network_anonymization_key,
      ProxyInfo* result,
      WinHttpStatus winhttp_status,
      int windows_error,
      const NetLogWithSource& net_log);

  // This is used to launch cross-process proxy resolution requests. Individual
  // WindowsSystemProxyResolutionRequest will use this to initiate proxy
  // resolution.
  std::unique_ptr<WindowsSystemProxyResolver> windows_system_proxy_resolver_;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_WIN_WINDOWS_SYSTEM_PROXY_RESOLUTION_SERVICE_H_
