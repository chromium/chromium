// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_SYSTEM_PROXY_RESOLUTION_SERVICE_H_
#define NET_PROXY_RESOLUTION_SYSTEM_PROXY_RESOLUTION_SERVICE_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "net/base/net_export.h"
#include "net/proxy_resolution/proxy_resolution_service.h"

namespace net {

class ProxyDelegate;
class SystemProxyResolutionRequest;

// Intermediate base class for system-level proxy resolution services that
// delegate proxy resolution to the operating system's native proxy APIs
// (e.g., WinHTTP on Windows, CFNetwork on macOS). This class manages the
// common state shared across platform implementations: pending requests,
// proxy retry info, and proxy delegate.
//
// Platform subclasses must implement:
//   - ResolveProxy(): Create platform-specific request and start resolution
//   - GetProxySettingsForNetLog(): Provide platform-specific proxy settings
//     description for net log diagnostics
//
// Subclass destructor contract: platform subclasses MUST drain
// pending_requests_ (e.g., by completing or cancelling all in-flight requests)
// before their destructor returns. The base destructor DCHECKs this.
class NET_EXPORT SystemProxyResolutionService : public ProxyResolutionService {
 public:
  SystemProxyResolutionService(const SystemProxyResolutionService&) = delete;
  SystemProxyResolutionService& operator=(const SystemProxyResolutionService&) =
      delete;

  ~SystemProxyResolutionService() override;

  // ProxyResolutionService implementation:
  void ReportSuccess(const ProxyInfo& proxy_info) override;
  void SetProxyDelegate(ProxyDelegate* delegate) override;
  void OnShutdown() override;
  void ClearBadProxiesCache() override;
  const ProxyRetryInfoMap& proxy_retry_info() const override;
  base::DictValue GetProxyNetLogValues() override;
  [[nodiscard]] bool CastToConfiguredProxyResolutionService(
      ConfiguredProxyResolutionService** configured_proxy_resolution_service)
      override;

 protected:
  friend class SystemProxyResolutionRequest;

  // Only constructible by subclasses.
  SystemProxyResolutionService();

  // Pending request management. Platform subclasses use these to track
  // in-flight requests.
  [[nodiscard]] bool ContainsPendingRequest(
      SystemProxyResolutionRequest* req) const;
  void RemovePendingRequest(SystemProxyResolutionRequest* req);
  // Exposed for platform subclass test fixtures (e.g., via friend or public
  // accessor on the concrete service class).
  size_t PendingRequestSizeForTesting() const {
    return pending_requests_.size();
  }

  // Platform subclass must supply the "proxy settings" description for NetLog.
  // This is called by GetProxyNetLogValues() to build the complete net log
  // dictionary. For example, Windows returns a dict describing "Windows system
  // proxy configuration".
  virtual base::DictValue GetProxySettingsForNetLog() = 0;

  // Map of the known bad proxies and the information about the retry time.
  ProxyRetryInfoMap proxy_retry_info_;

  // Set of pending/in-progress requests. Non-owning; callers own requests via
  // the std::unique_ptr<ProxyResolutionRequest> returned from ResolveProxy().
  // Platform subclasses static_cast to their concrete request type only in
  // their destructor cleanup loop.
  std::set<raw_ptr<SystemProxyResolutionRequest, SetExperimental>>
      pending_requests_;

  // Optional delegate for customizing proxy resolution behavior and receiving
  // proxy-related callbacks.
  raw_ptr<ProxyDelegate> proxy_delegate_ = nullptr;

  // Ensures all method calls are made on the same sequence.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_SYSTEM_PROXY_RESOLUTION_SERVICE_H_
