// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_PROXY_RESOLVER_H_
#define NET_PROXY_RESOLUTION_PROXY_RESOLVER_H_

#include "base/functional/callback_forward.h"
#include "net/base/completion_once_callback.h"
#include "net/base/load_states.h"
#include "net/base/net_export.h"
#include "net/proxy_resolution/pac_file_data.h"
#include "url/gurl.h"

namespace net {

class NetLogWithSource;
class NetworkAnonymizationKey;
class ProxyInfo;

// Interface for "proxy resolvers". A ProxyResolver fills in a list of proxies
// to use for a particular URL. Generally the backend for a ProxyResolver is
// a PAC script, but it doesn't need to be. ProxyResolver can service multiple
// requests at a time.
class NET_EXPORT_PRIVATE ProxyResolver {
 public:
  class Request {
   public:
    virtual ~Request() = default;  // Cancels the request
    virtual LoadState GetLoadState() = 0;
  };

  ProxyResolver() = default;

  ProxyResolver(const ProxyResolver&) = delete;
  ProxyResolver& operator=(const ProxyResolver&) = delete;

  virtual ~ProxyResolver() = default;

  // Gets a list of proxy servers to use for |url|. If the request will
  // complete asynchronously returns ERR_IO_PENDING and notifies the result
  // by running |callback|.  If the result code is OK then
  // the request was successful and |results| contains the proxy
  // resolution information.  In the case of asynchronous completion
  // |*request| is written to. Call request_.reset() to cancel the request.
  //
  // |network_isolation_key| is used for any DNS lookups associated with the
  // request, if net's HostResolver is used. If the underlying platform itself
  // handles proxy resolution, |network_anonymization_key| will be ignored.
  virtual int GetProxyForURL(
      const GURL& url,
      const NetworkAnonymizationKey& network_anonymization_key,
      ProxyInfo* results,
      CompletionOnceCallback callback,
      std::unique_ptr<Request>* request,
      const NetLogWithSource& net_log) = 0;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_PROXY_RESOLVER_H_
