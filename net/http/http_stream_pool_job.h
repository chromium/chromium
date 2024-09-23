// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_POOL_JOB_H_
#define NET_HTTP_HTTP_STREAM_POOL_JOB_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_error_details.h"
#include "net/base/net_export.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/http_stream_pool.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/next_proto.h"
#include "net/socket/stream_socket.h"
#include "net/ssl/ssl_info.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

class HttpStream;
class SSLCertRequestInfo;
class NetLogWithSource;
struct NetErrorDetails;

// Used by a `Delegate` to handle a stream request for a destination. The
// destination could be the origin or alternative services.
class HttpStreamPool::Job {
 public:
  // Interface to report Job's results. Only one of these methods will be
  // called.
  class NET_EXPORT_PRIVATE Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when a stream is ready.
    virtual void OnStreamReady(Job* job,
                               std::unique_ptr<HttpStream> stream,
                               NextProto negotiated_protocol) = 0;

    // Called when stream attempts failed.
    virtual void OnStreamFailed(Job* job,
                                int status,
                                const NetErrorDetails& net_error_details,
                                ResolveErrorInfo resolve_error_info) = 0;

    // Called when a stream attempt has failed due to a certificate error.
    virtual void OnCertificateError(Job* job,
                                    int status,
                                    const SSLInfo& ssl_info) = 0;

    // Called when a stream attempt has requested a client certificate.
    virtual void OnNeedsClientAuth(Job* job, SSLCertRequestInfo* cert_info) = 0;
  };

  // `delegate` must outlive `this`.
  Job(Delegate* delegate,
      AttemptManager* attempt_manager,
      NextProto expected_protocol,
      bool is_http1_allowed,
      ProxyInfo proxy_info);

  Job& operator=(const Job&) = delete;

  ~Job();

  // Starts this job.
  void Start(RequestPriority priority,
             const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
             RespectLimits respect_limits,
             bool enable_ip_based_pooling,
             bool enable_alternative_services,
             quic::ParsedQuicVersion quic_version,
             const NetLogWithSource& net_log);

  // Returns the LoadState of this job.
  LoadState GetLoadState() const;

  // Called when the priority of this job changes.
  void SetPriority(RequestPriority priority);

  // Add connection attempts to the job.
  void AddConnectionAttempts(const ConnectionAttempts& attempts);

  // Called by the associated AttemptManager when a stream is ready.
  void OnStreamReady(std::unique_ptr<HttpStream> stream,
                     NextProto negotiated_protocol);

  // Called by the associated AttemptManager when stream attempts failed.
  void OnStreamFailed(int rv,
                      const NetErrorDetails& net_error_details,
                      ResolveErrorInfo resolve_error_info);

  // Called by the associated AttemptManager when an stream attempt has failed
  // due to a certificate error.
  void OnCertificateError(int status, const SSLInfo& ssl_info);

  // Called by the associated AttemptManager when an stream attempt has
  // requested a client certificate.
  void OnNeedsClientAuth(SSLCertRequestInfo* cert_info);

  const ProxyInfo& proxy_info() const { return proxy_info_; }

  const ConnectionAttempts& connection_attempts() const {
    return connection_attempts_;
  }

 private:
  void CallOnStreamReady(std::unique_ptr<HttpStream> stream,
                         NextProto negotiated_protocol);
  void CallOnStreamFailed(int status,
                          const NetErrorDetails& net_error_details,
                          ResolveErrorInfo resolve_error_info);
  void CallOnCertificateError(int status, const SSLInfo& ssl_info);
  void CallOnNeedsClientAuth(SSLCertRequestInfo* cert_info);

  const raw_ptr<Delegate> delegate_;
  raw_ptr<AttemptManager> attempt_manager_;
  const NextProto expected_protocol_;
  const bool is_http1_allowed_;
  const ProxyInfo proxy_info_;

  ConnectionAttempts connection_attempts_;

  base::WeakPtrFactory<Job> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_JOB_H_
