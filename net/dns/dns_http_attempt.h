// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_HTTP_ATTEMPT_H_
#define NET_DNS_DNS_HTTP_ATTEMPT_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/isolation_info.h"
#include "net/base/load_timing_internal_info.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/dns/dns_attempt.h"
#include "net/dns/dns_query.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_session.h"
#include "net/dns/public/dns_protocol.h"
#include "net/http/http_connection_info.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_with_source.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"

namespace net {

class ResolveContext;

// An implementation of DnsAttempt over an HTTP transport.
class DnsHTTPAttempt : public DnsAttempt, public URLRequest::Delegate {
 public:
  // Information about an HTTP attempt.
  struct DnsHttpAttemptInfo {
    // Whether the request used an existing H2/H3 session.
    std::optional<SessionSource> session_source;
    // The coarse-grained protocol used to fetch the response.
    HttpConnectionInfoCoarse connection_info;
  };

  DnsHTTPAttempt(base::WeakPtr<ResolveContext> resolve_context,
                 DnsSession* session,
                 size_t doh_server_index,
                 std::unique_ptr<DnsQuery> query,
                 const std::string& server_template,
                 const GURL& gurl_without_parameters,
                 bool use_post,
                 URLRequestContext* url_request_context,
                 const IsolationInfo& isolation_info,
                 RequestPriority request_priority_,
                 bool is_probe);
  ~DnsHTTPAttempt() override;

  DnsHTTPAttempt(const DnsHTTPAttempt&) = delete;
  DnsHTTPAttempt& operator=(const DnsHTTPAttempt&) = delete;

  // DnsAttempt overrides.
  int Start(CompletionOnceCallback callback) override;
  const DnsQuery* GetQuery() const override;
  const DnsResponse* GetResponse() const override;
  base::Value GetRawResponseBufferForLog() const override;
  const NetLogWithSource& GetSocketNetLog() const override;

  // URLRequest::Delegate overrides
  void OnResponseStarted(net::URLRequest* request, int net_error) override;
  void OnReceivedRedirect(URLRequest* request,
                          const RedirectInfo& redirect_info,
                          bool* defer_redirect) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

  bool IsPending() const override;

 private:
  void StartAsync();

  void ResponseCompleted(int net_error);

  int CompleteResponse(int net_error);

  // `resolve_context_` and `session_` are only non-null for non-probe
  // requests. For probes, they are null.
  // ResolveContext is owned by the ContextHostResolver, which can be destroyed
  // while this attempt is still pending (e.g. during a probe). Therefore, we
  // use a WeakPtr to avoid a dangling pointer.
  base::WeakPtr<ResolveContext> resolve_context_;
  scoped_refptr<DnsSession> session_;

  const bool is_probe_;

  scoped_refptr<GrowableIOBuffer> buffer_;
  std::unique_ptr<DnsQuery> query_;
  CompletionOnceCallback callback_;
  std::unique_ptr<DnsResponse> response_;
  std::unique_ptr<URLRequest> request_;
  base::TimeTicks start_time_;
  NetLogWithSource net_log_;

  base::WeakPtrFactory<DnsHTTPAttempt> weak_factory_{this};
};

}  // namespace net

#endif  // NET_DNS_DNS_HTTP_ATTEMPT_H_
