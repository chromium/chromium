// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_POOL_JOB_H_
#define NET_HTTP_HTTP_STREAM_POOL_JOB_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/load_timing_internal_info.h"
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

// Used by a `Delegate` to handle a stream request or a preconnect for a
// destination. The destination could be the origin or alternative services.
class HttpStreamPool::Job {
 public:
  // Interface to report Job's results. JobController is the only implementation
  // of this interface other than tests. We abstract the interface to avoid a
  // circular dependency.
  class NET_EXPORT_PRIVATE Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns the priority of the job.
    virtual RequestPriority priority() const = 0;

    // Returns whether the limits should be respected.
    virtual RespectLimits respect_limits() const = 0;

    // Returns allowed bad certificates.
    virtual const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs()
        const = 0;

    // True when IP-based pooling is enabled.
    virtual bool enable_ip_based_pooling_for_h2() const = 0;

    // True when alternative services is enabled.
    virtual bool enable_alternative_services() const = 0;

    // Returns the set of ALPNs that are allowed for this job.
    virtual NextProtoSet allowed_alpns() const = 0;

    // Returns the proxy info.
    virtual const ProxyInfo& proxy_info() const = 0;

    virtual const NetLogWithSource& net_log() const = 0;

    virtual const perfetto::Flow& flow() const = 0;

    // Callback methods: Only one of these methods will be called.
    // Called when a stream is ready.
    virtual void OnStreamReady(Job* job,
                               std::unique_ptr<HttpStream> stream,
                               NextProto negotiated_protocol,
                               std::optional<SessionSource> session_source) = 0;
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

    // Called when the preconnect has completed.
    virtual void OnPreconnectComplete(Job* job, int status) = 0;
  };

  // `delegate` must outlive `this`. For a stream request, `num_streams` must
  // not be specified. `group` must not be destroyed until either it has
  // notified the Job of completion, or `this` has informed the Group's
  // AttemptManager of cancellation. For a preconnect, `num_streams` must be
  // specified.
  Job(Delegate* delegate,
      JobType type,
      Group* group,
      quic::ParsedQuicVersion quic_version,
      NextProto expected_protocol,
      const NetLogWithSource& request_net_log,
      size_t num_streams = 0);

  Job& operator=(const Job&) = delete;

  ~Job();

  // Starts this job.
  void Start();

  // Returns the LoadState of this job.
  LoadState GetLoadState() const;

  // Called when the priority of this job changes.
  void SetPriority(RequestPriority priority);

  // Add connection attempts to the job.
  void AddConnectionAttempts(const ConnectionAttempts& attempts);

  // Called by the associated AttemptManager when a stream is ready.
  void OnStreamReady(std::unique_ptr<HttpStream> stream,
                     NextProto negotiated_protocol,
                     std::optional<SessionSource> session_source);

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

  // Called by the associated AttemptManager when the preconnect completed.
  void OnPreconnectComplete(int status);

  // Helper method to call OnPreconnectComplete asynchronously. Used to avoid
  // a dangling pointer since calling `delegate_->OnPreconnectComplete()`
  // deletes `this` synchronously.
  void CallOnPreconnectCompleteLater(int status);

  RequestPriority priority() const { return delegate_->priority(); }

  RespectLimits respect_limits() const { return delegate_->respect_limits(); }

  bool enable_ip_based_pooling_for_h2() const {
    return delegate_->enable_ip_based_pooling_for_h2();
  }

  const ProxyInfo& proxy_info() const { return delegate_->proxy_info(); }

  const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs() const {
    return delegate_->allowed_bad_certs();
  }

  const NetLogWithSource& delegate_net_log() const {
    return delegate_->net_log();
  }

  // TODO(crbug.com/455891789): Remove this once the bug is fixed.
  bool enable_alternative_services() const {
    return delegate_->enable_alternative_services();
  }

  const NetLogWithSource& net_log() const { return job_net_log_; }

  const NetLogWithSource& request_net_log() const { return request_net_log_; }

  const perfetto::Flow& flow() const { return delegate_->flow(); }

  quic::ParsedQuicVersion quic_version() const { return quic_version_; }

  const NextProtoSet& allowed_alpns() const { return allowed_alpns_; }

  size_t num_streams() const { return num_streams_; }

  JobType type() const { return type_; }

  bool is_preconnect() const { return type_ != JobType::kRequest; }

  const ConnectionAttempts& connection_attempts() const {
    return connection_attempts_;
  }

  base::TimeTicks create_time() const { return create_time_; }

 private:
  // Called when job is cancelled or completes. Sets `result_` (which is
  // currently nullopt on cancelletation - should it be ERR_ABORTED instead?).
  // Clears `attempt_manager_`. On cancellation, The AttemptManager must already
  // have been notified of cancellation.
  void OnDone(std::optional<int> result);

  const raw_ptr<Delegate> delegate_;
  const JobType type_;

  // The AttemptManager associated with `this`. Once `this` has been notified of
  // success or failure, replaced with nullptr.
  raw_ptr<AttemptManager> attempt_manager_;

  const quic::ParsedQuicVersion quic_version_;
  const NextProtoSet allowed_alpns_;
  const NetLogWithSource request_net_log_;
  const NetLogWithSource job_net_log_;
  const size_t num_streams_;
  const base::TimeTicks create_time_;

  std::optional<int> result_;
  std::optional<NextProto> negotiated_protocol_;

  ConnectionAttempts connection_attempts_;

  base::WeakPtrFactory<Job> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_JOB_H_
