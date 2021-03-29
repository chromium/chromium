// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_WIN_WINDOWS_SYSTEM_PROXY_RESOLUTION_SERVICE_H_
#define NET_PROXY_RESOLUTION_WIN_WINDOWS_SYSTEM_PROXY_RESOLUTION_SERVICE_H_

#include "net/proxy_resolution/proxy_resolution_service.h"

#include <memory>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "net/base/net_export.h"

namespace net {

class NetLog;
class WindowsSystemProxyResolutionRequest;
class WindowsSystemProxyResolver;

using CreateWindowsSystemProxyResolverFunctionForTesting =
    scoped_refptr<WindowsSystemProxyResolver> (*)();

// This class decides which proxy server(s) to use for a particular URL request.
// It does NOT support passing in fetched proxy configurations. Instead, it
// relies entirely on WinHttp APIs to determine the proxy that should be used
// for each network request.
class NET_EXPORT WindowsSystemProxyResolutionService
    : public ProxyResolutionService {
 public:
  // The WinHttp functions used in the resolver via the WinHttpAPIWrapper are
  // only supported on Windows 8 and above.
  static bool IsSupported() WARN_UNUSED_RESULT;

  // Creates a WindowsSystemProxyResolutionService or returns nullptr if the
  // runtime dependencies are not satisfied.
  static std::unique_ptr<WindowsSystemProxyResolutionService> Create(
      NetLog* net_log);

  WindowsSystemProxyResolutionService(
      const WindowsSystemProxyResolutionService&) = delete;
  WindowsSystemProxyResolutionService& operator=(
      const WindowsSystemProxyResolutionService&) = delete;

  ~WindowsSystemProxyResolutionService() override;

  // ProxyResolutionService implementation
  int ResolveProxy(const GURL& url,
                   const std::string& method,
                   const NetworkIsolationKey& network_isolation_key,
                   ProxyInfo* results,
                   CompletionOnceCallback callback,
                   std::unique_ptr<ProxyResolutionRequest>* request,
                   const NetLogWithSource& net_log) override;
  void ReportSuccess(const ProxyInfo& proxy_info) override;
  void SetProxyDelegate(ProxyDelegate* delegate) override;
  void OnShutdown() override;
  bool MarkProxiesAsBadUntil(
      const ProxyInfo& results,
      base::TimeDelta retry_delay,
      const std::vector<ProxyServer>& additional_bad_proxies,
      const NetLogWithSource& net_log) override;
  void ClearBadProxiesCache() override;
  const ProxyRetryInfoMap& proxy_retry_info() const override;
  base::Value GetProxyNetLogValues() override;
  bool CastToConfiguredProxyResolutionService(
      ConfiguredProxyResolutionService** configured_proxy_resolution_service)
      override WARN_UNUSED_RESULT;

  // Used in tests to provide a fake |windows_system_proxy_resolver_|.
  void SetCreateWindowsSystemProxyResolverFunctionForTesting(
      CreateWindowsSystemProxyResolverFunctionForTesting function);
  void SetWindowsSystemProxyResolverForTesting(
      scoped_refptr<WindowsSystemProxyResolver> windows_system_proxy_resolver);

 private:
  friend class WindowsSystemProxyResolutionRequest;
  friend class WindowsSystemProxyResolutionServiceTest;

  explicit WindowsSystemProxyResolutionService(NetLog* net_log);

  typedef std::set<WindowsSystemProxyResolutionRequest*> PendingRequests;

  bool ContainsPendingRequest(WindowsSystemProxyResolutionRequest* req)
      WARN_UNUSED_RESULT;
  void RemovePendingRequest(WindowsSystemProxyResolutionRequest* req);

  // Lazily creates |windows_system_proxy_resolver_|.
  bool CreateWindowsSystemProxyResolverIfNeeded() WARN_UNUSED_RESULT;

  size_t PendingRequestSizeForTesting() const {
    return pending_requests_.size();
  }

  // Called when proxy resolution has completed (either synchronously or
  // asynchronously). Handles logging the result, and cleaning out
  // bad entries from the results list.
  int DidFinishResolvingProxy(const GURL& url,
                              const std::string& method,
                              ProxyInfo* result,
                              int result_code,
                              const NetLogWithSource& net_log);

  CreateWindowsSystemProxyResolverFunctionForTesting
      create_proxy_resolver_function_for_testing_;

  // Map of the known bad proxies and the information about the retry time.
  ProxyRetryInfoMap proxy_retry_info_;

  // Set of pending/in-progress requests.
  PendingRequests pending_requests_;

  // This is the log for any generated events.
  NetLog* net_log_;

  // This object encapsulates all WinHttp logic in Chromium-friendly terms. It
  // manages the lifetime of the WinHttp session (which is
  // per-resolution-service). This will get handed off to individual resolution
  // requests so that they can query/cancel proxy resolution as needed.
  scoped_refptr<WindowsSystemProxyResolver> windows_system_proxy_resolver_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_WIN_WINDOWS_SYSTEM_PROXY_RESOLUTION_SERVICE_H_
