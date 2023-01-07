// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_WIN_WINDOWS_SYSTEM_PROXY_RESOLUTION_REQUEST_H_
#define NET_PROXY_RESOLUTION_WIN_WINDOWS_SYSTEM_PROXY_RESOLUTION_REQUEST_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_resolution_request.h"
#include "net/proxy_resolution/win/windows_system_proxy_resolver.h"
#include "net/proxy_resolution/win/winhttp_status.h"
#include "url/gurl.h"

namespace net {

class ProxyInfo;
class ProxyList;
class WindowsSystemProxyResolutionService;

// This is the concrete implementation of ProxyResolutionRequest used by
// WindowsSystemProxyResolutionService. Manages a single asynchronous proxy
// resolution request.
class NET_EXPORT WindowsSystemProxyResolutionRequest
    : public ProxyResolutionRequest {
 public:
  // The |windows_system_proxy_resolver| is not saved by this object. Rather, it
  // is simply used to kick off proxy resolution in a utility process from
  // within the constructor. The |windows_system_proxy_resolver| is not needed
  // after construction. Every other parameter is saved by this object. Details
  // for each one of these saved parameters can be found below.
  WindowsSystemProxyResolutionRequest(
      WindowsSystemProxyResolutionService* service,
      const GURL& url,
      const std::string& method,
      ProxyInfo* results,
      const CompletionOnceCallback user_callback,
      const NetLogWithSource& net_log,
      WindowsSystemProxyResolver* windows_system_proxy_resolver);

  WindowsSystemProxyResolutionRequest(
      const WindowsSystemProxyResolutionRequest&) = delete;
  WindowsSystemProxyResolutionRequest& operator=(
      const WindowsSystemProxyResolutionRequest&) = delete;

  ~WindowsSystemProxyResolutionRequest() override;

  // ProxyResolutionRequest
  LoadState GetLoadState() const override;

  // Callback for when the cross-process proxy resolution has completed. The
  // |proxy_list| is the list of proxies returned by WinHttp translated into
  // Chromium-friendly terms. The |winhttp_status| describes the status of the
  // proxy resolution request. If WinHttp fails for some reason, |windows_error|
  // contains the specific error returned by WinHttp.
  virtual void ProxyResolutionComplete(const ProxyList& proxy_list,
                                       WinHttpStatus winhttp_status,
                                       int windows_error);

  WindowsSystemProxyResolver::Request* GetProxyResolutionRequestForTesting();
  void ResetProxyResolutionRequestForTesting();

 private:
  // Cancels the callback from the resolver for a previously started proxy
  // resolution.
  void CancelResolveRequest();

  // Returns true if the request has been completed.
  bool was_completed() const { return user_callback_.is_null(); }

  // Note that Request holds a bare pointer to the
  // WindowsSystemProxyResolutionService. Outstanding requests are cancelled
  // during ~WindowsSystemProxyResolutionService, so this is guaranteed to be
  // valid throughout the lifetime of this object.
  raw_ptr<WindowsSystemProxyResolutionService> service_;
  CompletionOnceCallback user_callback_;
  raw_ptr<ProxyInfo> results_;
  const GURL url_;
  const std::string method_;
  NetLogWithSource net_log_;
  // Time when the request was created.  Stored here rather than in |results_|
  // because the time in |results_| will be cleared.
  base::TimeTicks creation_time_;

  // Manages the cross-process proxy resolution. Deleting this will cancel a
  // pending proxy resolution. After a callback has been received via
  // ProxyResolutionComplete(), this object will no longer do anything.
  std::unique_ptr<WindowsSystemProxyResolver::Request>
      proxy_resolution_request_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_WIN_WINDOWS_SYSTEM_PROXY_RESOLUTION_REQUEST_H_
