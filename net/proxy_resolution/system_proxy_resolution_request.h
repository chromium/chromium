// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_SYSTEM_PROXY_RESOLUTION_REQUEST_H_
#define NET_PROXY_RESOLUTION_SYSTEM_PROXY_RESOLUTION_REQUEST_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_resolution_request.h"
#include "url/gurl.h"

namespace net {

class ProxyInfo;
class SystemProxyResolutionService;

// Intermediate base class for system proxy resolution requests. Holds the
// common request state (URL, method, NAK, callback, results, net log, timing)
// shared across platform-specific implementations (Windows WinHTTP, future
// macOS CFNetwork).
//
// Platform subclasses own the platform-specific resolver handle and implement
// the resolution callback. The destructor handles removing the request from
// the service's pending set and logging cancellation events.
class NET_EXPORT SystemProxyResolutionRequest : public ProxyResolutionRequest {
 public:
  SystemProxyResolutionRequest(const SystemProxyResolutionRequest&) = delete;
  SystemProxyResolutionRequest& operator=(const SystemProxyResolutionRequest&) =
      delete;

  ~SystemProxyResolutionRequest() override;

  // ProxyResolutionRequest:
  LoadState GetLoadState() const override;

 protected:
  SystemProxyResolutionRequest(
      SystemProxyResolutionService* service,
      GURL url,
      std::string method,
      NetworkAnonymizationKey network_anonymization_key,
      ProxyInfo* results,
      CompletionOnceCallback user_callback,
      const NetLogWithSource& net_log);

  // Returns true if the request has been completed (callback already invoked).
  bool was_completed() const { return user_callback_.is_null(); }

  // Marks this request as completed: removes it from the service's pending
  // request set and clears the service back-pointer. Called by platform
  // subclasses when proxy resolution completes successfully (before invoking
  // the user callback). The base destructor handles removal separately for
  // the cancellation path.
  void MarkCompleted();

  // Protected for derived class access. Platform subclasses need direct field
  // access to these members in their completion callbacks (e.g.,
  // ProxyResolutionComplete) where they read url_, method_, results_, etc.
  // and invoke user_callback_.

  // Back-pointer to the owning service. Outstanding requests are cancelled
  // during the service subclass destructor, so this is guaranteed to be valid
  // throughout the lifetime of this object while the request is still pending.
  // Set to nullptr by the derived class after completion.
  raw_ptr<SystemProxyResolutionService> service_;
  CompletionOnceCallback user_callback_;
  raw_ptr<ProxyInfo> results_;
  const GURL url_;
  const std::string method_;
  const NetworkAnonymizationKey network_anonymization_key_;
  NetLogWithSource net_log_;
  // Time when the request was created. Stored here rather than in |results_|
  // because the time in |results_| will be cleared.
  base::TimeTicks creation_time_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_SYSTEM_PROXY_RESOLUTION_REQUEST_H_
