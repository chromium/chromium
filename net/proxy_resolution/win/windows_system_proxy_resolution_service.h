// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_WIN_WINDOWS_SYSTEM_PROXY_RESOLUTION_SERVICE_H_
#define NET_PROXY_RESOLUTION_WIN_WINDOWS_SYSTEM_PROXY_RESOLUTION_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "net/proxy_resolution/proxy_resolution_service.h"

#include <memory>
#include <set>
#include <string>

#include "base/sequence_checker.h"
#include "net/base/net_export.h"
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
    : public ProxyResolutionService {
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
                   const NetLogWithSource& net_log) override;
  void ReportSuccess(const ProxyInfo& proxy_info) override;
  void SetProxyDelegate(ProxyDelegate* delegate) override;
  void OnShutdown() override;
  void ClearBadProxiesCache() override;
  const ProxyRetryInfoMap& proxy_retry_info() const override;
  base::Value::Dict GetProxyNetLogValues() override;
  [[nodiscard]] bool CastToConfiguredProxyResolutionService(
      ConfiguredProxyResolutionService** configured_proxy_resolution_service)
      override;

 private:
  friend class WindowsSystemProxyResolutionRequest;

  WindowsSystemProxyResolutionService(
      std::unique_ptr<WindowsSystemProxyResolver> windows_system_proxy_resolver,
      NetLog* net_log);

  typedef std::set<
      raw_ptr<WindowsSystemProxyResolutionRequest, SetExperimental>>
      PendingRequests;

  [[nodiscard]] bool ContainsPendingRequest(
      WindowsSystemProxyResolutionRequest* req);
  void RemovePendingRequest(WindowsSystemProxyResolutionRequest* req);

  size_t PendingRequestSizeForTesting() const {
    return pending_requests_.size();
  }

  // Called when proxy resolution has completed (either synchronously or
  // asynchronously). Handles logging the result, and cleaning out
  // bad entries from the results list.
  int DidFinishResolvingProxy(const GURL& url,
                              const std::string& method,
                              ProxyInfo* result,
                              WinHttpStatus winhttp_status,
                              const NetLogWithSource& net_log);

  // Map of the known bad proxies and the information about the retry time.
  ProxyRetryInfoMap proxy_retry_info_;

  // Set of pending/in-progress requests.
  PendingRequests pending_requests_;

  // This is used to launch cross-process proxy resolution requests. Individual
  // WindowsSystemProxyResolutionRequest will use this to initiate proxy
  // resolution.
  std::unique_ptr<WindowsSystemProxyResolver> windows_system_proxy_resolver_;

  // This is the log for any generated events.
  raw_ptr<NetLog> net_log_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_WIN_WINDOWS_SYSTEM_PROXY_RESOLUTION_SERVICE_H_
