// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_POOL_JOB_H_
#define NET_HTTP_HTTP_STREAM_POOL_JOB_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/load_states.h"
#include "net/base/net_error_details.h"
#include "net/base/priority_queue.h"
#include "net/base/request_priority.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_request.h"
#include "net/log/net_log_with_source.h"
#include "url/gurl.h"

namespace net {

class HttpNetworkSession;
class NetLog;
class HttpStreamKey;

// Maintains in-flight HTTP stream requests. Peforms DNS resolution.
class HttpStreamPool::Job
    : public HostResolver::ServiceEndpointRequest::Delegate {
 public:
  // `group` must outlive `this`.
  Job(Group* group, NetLog* net_log);

  Job(const Job&) = delete;
  Job& operator=(const Job&) = delete;

  ~Job() override;

  // Creates an HttpStreamRequest. Will call delegate's methods. See the
  // comments of HttpStreamRequest::Delegate methods for details.
  // TODO(crbug.com/346835898): Support TLS, HTTP/2 and QUIC.
  std::unique_ptr<HttpStreamRequest> RequestStream(
      HttpStreamRequest::Delegate* delegate,
      RequestPriority priority,
      const NetLogWithSource& net_log);

  // HostResolver::ServiceEndpointRequest::Delegate implementation:
  void OnServiceEndpointsUpdated() override;
  void OnServiceEndpointRequestFinished(int rv) override;

 private:
  // A peer of an HttpStreamRequest. Holds the HttpStreamRequest's delegate
  // pointer and implements HttpStreamRequest::Helper.
  class RequestEntry : public HttpStreamRequest::Helper {
   public:
    explicit RequestEntry(Job* job);

    RequestEntry(RequestEntry&) = delete;
    RequestEntry& operator=(const RequestEntry&) = delete;

    ~RequestEntry() override;

    std::unique_ptr<HttpStreamRequest> CreateRequest(
        HttpStreamRequest::Delegate* delegate,
        const NetLogWithSource& net_log);

    HttpStreamRequest* request() const { return request_; }

    HttpStreamRequest::Delegate* delegate() const { return delegate_; }

    // Extract `delegate_` to call a method of the delegate. `delegate_` becomes
    // nullptr.
    HttpStreamRequest::Delegate* ExtractDelegate();

    // HttpStreamRequest::Helper methods:
    LoadState GetLoadState() const override;
    void OnRequestComplete() override;
    int RestartTunnelWithProxyAuth() override;
    void SetPriority(RequestPriority priority) override;

   private:
    const raw_ptr<Job> job_;
    raw_ptr<HttpStreamRequest> request_;
    raw_ptr<HttpStreamRequest::Delegate> delegate_;
  };

  using RequestQueue = PriorityQueue<std::unique_ptr<RequestEntry>>;

  const HttpStreamKey& stream_key() const;

  HttpNetworkSession* http_network_session();

  // Returns the current load state.
  LoadState GetLoadState() const;

  // Returns the highest priority in `requests_`.
  RequestPriority GetPriority() const;

  void ResolveServiceEndpoint(RequestPriority initial_priority);

  void MaybeChangeServiceEndpointRequestPriority();

  void MaybeAttemptConnection();

  void NotifyFailure(int rv);

  // Extracts a delegate pointer from `requests_` of which priority is
  // highest and the delegate is not extracted.
  HttpStreamRequest::Delegate* ExtractFirstRequestDelegate();

  // Called when the priority of `request` is set.
  void SetRequestPriority(HttpStreamRequest* request, RequestPriority priority);

  // Called when `request` is going to be destroyed.
  void OnRequestComplete(HttpStreamRequest* request);

  void MaybeComplete();

  const raw_ptr<Group> group_;
  const NetLogWithSource net_log_;

  RequestQueue requests_;

  std::unique_ptr<HostResolver::ServiceEndpointRequest>
      service_endpoint_request_;
  bool service_endpoint_request_finished_ = false;

  NetErrorDetails net_error_details_;
  ResolveErrorInfo resolve_error_info_;

  base::WeakPtrFactory<Job> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_JOB_H_
