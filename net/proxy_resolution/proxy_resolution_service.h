// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_RESOLUTION_SERVICE_H_
#define NET_PROXY_RESOLUTION_PROXY_RESOLUTION_SERVICE_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/proxy_server.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_info.h"
#include "url/gurl.h"

namespace net {

class ConfiguredProxyResolutionService;
class ProxyDelegate;
class ProxyResolutionRequest;

// This is a generic interface that is used to decide which proxy server(s) to
// use for a particular URL request. The typical consumer of the
// ProxyResolutionService does not need to know how we decide on the right proxy
// for that network request.
class NET_EXPORT ProxyResolutionService {
 public:
  virtual ~ProxyResolutionService() = default;

  // Determines the appropriate proxy for |url| for a |method| request and
  // stores the result in |results|. If |method| is empty, the caller can expect
  // method independent resolution.
  //
  // Returns ERR_IO_PENDING if the proxy information could not be provided
  // synchronously, to indicate that the result will be available when the
  // callback is run.  The callback is run on the thread that calls
  // ResolveProxy.
  //
  // The caller is responsible for ensuring that |results| and |callback|
  // remain valid until the callback is run or until |request| is cancelled,
  // which occurs when the unique pointer to it is deleted (by leaving scope or
  // otherwise).  |request| must not be nullptr.
  //
  // Profiling information for the request is saved to |net_log| if non-nullptr.
  virtual int ResolveProxy(
      const GURL& url,
      const std::string& method,
      const NetworkAnonymizationKey& network_anonymization_key,
      ProxyInfo* results,
      CompletionOnceCallback callback,
      std::unique_ptr<ProxyResolutionRequest>* request,
      const NetLogWithSource& net_log) = 0;

  // Called to report that the last proxy connection succeeded.  If |proxy_info|
  // has a non empty proxy_retry_info map, the proxies that have been tried (and
  // failed) for this request will be marked as bad.
  virtual void ReportSuccess(const ProxyInfo& proxy_info) = 0;

  // Associates a delegate with this ProxyResolutionService. |delegate|
  // must outlive |this|.
  // TODO(eroman): Specify this as a dependency at construction time rather
  //               than making it a mutable property.
  virtual void SetProxyDelegate(ProxyDelegate* delegate) = 0;

  // Cancels all network requests, and prevents the service from creating new
  // ones.  Must be called before the URLRequestContext the
  // ProxyResolutionService was created with is torn down, if it's torn down
  // before the ProxyResolutionService itself.
  virtual void OnShutdown() = 0;

  // Clears the list of bad proxy servers that has been cached.
  virtual void ClearBadProxiesCache() = 0;

  // Returns the map of proxies which have been marked as "bad".
  virtual const ProxyRetryInfoMap& proxy_retry_info() const = 0;

  // Returns proxy related debug information to be included in the NetLog. The
  // data should be appropriate for any capture mode (sensitivity level).
  virtual base::Value::Dict GetProxyNetLogValues() = 0;

  // Returns true if |this| is an instance of ConfiguredProxyResolutionService
  // and assigns |this| to the out parameter. Otherwise returns false and sets
  // |*configured_proxy_resolution_service| to nullptr.
  //
  // In general, consumers of the ProxyResolutionService should exclusively
  // interact with the general ProxyResolutionService. In some isolated
  // instances, a consumer may specifically need to interact with an underlying
  // implementation. For example, one might need to fetch the set of proxy
  // configurations determined by the proxy, something which not all
  // implementations of the ProxyResolutionService would have an answer for.
  [[nodiscard]] virtual bool CastToConfiguredProxyResolutionService(
      ConfiguredProxyResolutionService**
          configured_proxy_resolution_service) = 0;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PROXY_RESOLUTION_SERVICE_H_
