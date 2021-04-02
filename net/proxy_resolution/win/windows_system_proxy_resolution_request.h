// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_WIN_WINDOWS_SYSTEM_PROXY_RESOLUTION_REQUEST_H_
#define NET_PROXY_RESOLUTION_WIN_WINDOWS_SYSTEM_PROXY_RESOLUTION_REQUEST_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_resolution_request.h"
#include "url/gurl.h"

namespace net {

class ProxyInfo;
class ProxyList;
class WindowsSystemProxyResolutionService;
class WindowsSystemProxyResolver;

// This is the concrete implementation of ProxyResolutionRequest used by
// WindowsSystemProxyResolutionService. Manages a single asynchronous proxy
// resolution request.
class NET_EXPORT WindowsSystemProxyResolutionRequest
    : public ProxyResolutionRequest {
 public:
  WindowsSystemProxyResolutionRequest(
      WindowsSystemProxyResolutionService* service,
      const GURL& url,
      const std::string& method,
      ProxyInfo* results,
      const CompletionOnceCallback user_callback,
      const NetLogWithSource& net_log,
      scoped_refptr<WindowsSystemProxyResolver> windows_system_proxy_resolver);

  WindowsSystemProxyResolutionRequest(
      const WindowsSystemProxyResolutionRequest&) = delete;
  WindowsSystemProxyResolutionRequest& operator=(
      const WindowsSystemProxyResolutionRequest&) = delete;

  ~WindowsSystemProxyResolutionRequest() override;

  // ProxyResolutionRequest
  LoadState GetLoadState() const override;

  // Starts the resolve proxy request.
  int Start();

  // Cancels the callback from the resolver for a previously started proxy
  // resolution.
  void CancelResolveJob();

  bool IsStarted();

  // Returns true if the request has been completed.
  bool was_completed() const { return user_callback_.is_null(); }

  // Helper to call after ProxyResolver completion (both synchronous and
  // asynchronous). Fixes up the result that is to be returned to user.
  int UpdateResultsOnProxyResolutionComplete(const ProxyList& proxy_list,
                                             int net_error);

  // Helper to call if the request completes synchronously, since in that case
  // the request will not be added to |pending_requests_| (in
  // WindowsSystemProxyResolutionService).
  int SynchronousProxyResolutionComplete(int net_error);

  // Callback for when the WinHttp request has completed. This is the main way
  // that proxy resolutions will complete. The |proxy_list| is the list of
  // proxies returned by WinHttp translated into Chromium-friendly terms. The
  // |net_error| describes the status of the proxy resolution request. If
  // WinHttp fails for some reason, |windows_error| contains the specific error
  // returned by WinHttp.
  virtual void AsynchronousProxyResolutionComplete(const ProxyList& proxy_list,
                                                   int net_error,
                                                   int windows_error);

 protected:
  // The resolver will do the work of talking to system APIs and translating the
  // results into something Chromium understands.
  scoped_refptr<WindowsSystemProxyResolver> windows_system_proxy_resolver_;

 private:
  // Note that Request holds a bare pointer to the
  // WindowsSystemProxyResolutionService. Outstanding requests are cancelled
  // during ~WindowsSystemProxyResolutionService, so this is guaranteed to be
  // valid throughout the lifetime of this object.
  WindowsSystemProxyResolutionService* service_;
  CompletionOnceCallback user_callback_;
  ProxyInfo* results_;
  const GURL url_;
  const std::string method_;
  NetLogWithSource net_log_;
  // Time when the request was created.  Stored here rather than in |results_|
  // because the time in |results_| will be cleared.
  base::TimeTicks creation_time_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_WIN_WINDOWS_SYSTEM_PROXY_RESOLUTION_REQUEST_H_
