// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_POOL_JOB_CONTROLLER_H_
#define NET_HTTP_HTTP_STREAM_POOL_JOB_CONTROLLER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "net/base/load_states.h"
#include "net/base/load_timing_internal_info.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/request_priority.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/alternative_service.h"
#include "net/http/http_stream_key.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_job.h"
#include "net/http/http_stream_pool_request_info.h"
#include "net/http/http_stream_request.h"
#include "net/log/net_log.h"
#include "net/quic/quic_session_alias_key.h"
#include "net/socket/next_proto.h"
#include "net/ssl/ssl_config.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

class NetLogWithSource;
class SSLCertRequestInfo;
class HttpStream;
struct NetErrorDetails;

// Manages a single HttpStreamRequest or a preconnect. Creates and owns Jobs.
class HttpStreamPool::JobController : public HttpStreamPool::Job::Delegate,
                                      public HttpStreamRequest::Helper {
 public:
  JobController(HttpStreamPool* pool,
                HttpStreamPoolRequestInfo request_info,
                RequestPriority priority,
                std::vector<SSLConfig::CertAndStatus> allowed_bad_certs,
                bool enable_ip_based_pooling_for_h2,
                bool enable_alternative_services);

  JobController(const JobController&) = delete;
  JobController& operator=(const JobController&) = delete;

  ~JobController() override;

  // Takes over the responsibility of processing an already created `request`.
  void HandleStreamRequest(HttpStreamRequest* stream_request,
                           HttpStreamRequest::Delegate* delegate);

  // Requests that enough connections/sessions for `num_streams` be opened.
  // `callback` is only invoked when the return value is `ERR_IO_PENDING`.
  int Preconnect(size_t num_streams, CompletionOnceCallback callback);

  // HttpStreamPool::Job::Delegate implementation:
  RequestPriority priority() const override;
  RespectLimits respect_limits() const override;
  const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs()
      const override;
  bool enable_ip_based_pooling_for_h2() const override;
  bool enable_alternative_services() const override;
  NextProtoSet allowed_alpns() const override;
  const ProxyInfo& proxy_info() const override;
  const NetLogWithSource& net_log() const override;
  const perfetto::Flow& flow() const override;
  void OnStreamReady(Job* job,
                     std::unique_ptr<HttpStream> stream,
                     NextProto negotiated_protocol,
                     std::optional<SessionSource> session_source) override;
  void OnStreamFailed(Job* job,
                      int status,
                      const NetErrorDetails& net_error_details,
                      ResolveErrorInfo resolve_error_info) override;
  void OnCertificateError(Job* job,
                          int status,
                          const SSLInfo& ssl_info) override;
  void OnNeedsClientAuth(Job* job, SSLCertRequestInfo* cert_info) override;
  void OnPreconnectComplete(Job* job, int status) override;

  // HttpStreamRequest::Helper implementation:
  LoadState GetLoadState() const override;
  void OnRequestComplete() override;
  int RestartTunnelWithProxyAuth() override;
  void SetPriority(RequestPriority priority) override;

  base::Value::Dict GetInfoAsValue() const;

 private:
  // Represents an alternative endpoint for the request.
  struct Alternative {
    Alternative(HttpStreamKey stream_key,
                NextProto protocol,
                quic::ParsedQuicVersion quic_version,
                std::optional<QuicSessionAliasKey> quic_key);
    Alternative(Alternative&&);
    ~Alternative();

    Alternative& operator=(Alternative&&);

    Alternative(const Alternative&) = delete;
    Alternative& operator=(const Alternative&) = delete;

    HttpStreamKey stream_key;
    NextProto protocol;

    // Only set when this alternative is QUIC.
    quic::ParsedQuicVersion quic_version;
    std::optional<QuicSessionAliasKey> quic_key;
  };

  // Stream that is ready to be used, along with some associated metadata.
  struct PendingStream {
    PendingStream(std::unique_ptr<HttpStream> stream,
                  NextProto negotiated_protocol,
                  std::optional<SessionSource> session_source);
    PendingStream(PendingStream&&);
    ~PendingStream();

    PendingStream& operator=(PendingStream&&);

    std::unique_ptr<HttpStream> stream;
    NextProto negotiated_protocol;
    std::optional<SessionSource> session_source;
  };

  // Calculate an alternative endpoint for the request.
  static std::optional<Alternative> CalculateAlternative(
      HttpStreamPool* pool,
      const HttpStreamKey& origin_stream_key,
      const HttpStreamPoolRequestInfo& request_info,
      bool enable_alternative_services);

  QuicSessionPool* quic_session_pool();
  SpdySessionPool* spdy_session_pool();

  // Returns an HttpStream and its negotiated protocol if there is an
  // existing session or an idle stream that can serve the request. Otherwise,
  // returns std::nullopt.
  std::optional<PendingStream> MaybeCreateStreamFromExistingSession();

  // When there is a QUIC session that can serve an HttpStream for the request,
  // creates an HttpStream and returns it.
  std::unique_ptr<HttpStream> MaybeCreateStreamFromExistingQuicSession();
  std::unique_ptr<HttpStream> MaybeCreateStreamFromExistingQuicSessionInternal(
      const QuicSessionAliasKey& key);

  // May start an alternative job. Returns true when an alternative job is
  // started.
  bool MaybeStartAlternativeJob();

  // Returns true when a QUIC session can be used for the request.
  bool CanUseExistingQuicSession();

  // Starts a QUIC preconnect job when an alternative service is advertised via
  // Alt-Svc but the current request is not using it.
  void StartAltSvcQuicPreconnect();

  // Calls the request's Complete() and tells the delegate that a stream, now
  // stored in `pending_stream_`, is ready. Used when there is an existing
  // QUIC/SPDY session that can serve the request.
  void CallRequestCompleteAndStreamReady();

  // Calls the request's stream failed callback.
  void CallOnStreamFailed(int status,
                          const NetErrorDetails& net_error_details,
                          ResolveErrorInfo resolve_error_info);

  // Calls the request's certificate error callback.
  void CallOnCertificateError(int status, const SSLInfo& ssl_info);

  // Calls the request's client auth callback.
  void CallOnNeedsClientAuth(SSLCertRequestInfo* cert_info);

  // Resets `job` and invokes the preconnect callback.
  void ResetJobAndInvokePreconnectCallback(Job* job, int status);

  // Sets the result of `job`.
  void SetJobResult(Job* job, int status);

  // Cancels jobs other than `job` to handle a failure that require user
  // interaction such as certificate errors and a client authentication is
  // requested.
  void CancelOtherJob(Job* job);

  // Returns true when all jobs complete.
  bool AllJobsFinished();

  // Called when all jobs complete. Record brokenness of the alternative
  // service if the origin job has no error and the alternative job has an
  // error.
  void MaybeMarkAlternativeServiceBroken();

  const raw_ptr<HttpStreamPool> pool_;
  RequestPriority priority_;
  const std::vector<SSLConfig::CertAndStatus> allowed_bad_certs_;
  const bool enable_ip_based_pooling_for_h2_;
  const bool enable_alternative_services_;
  const RespectLimits respect_limits_;
  NextProtoSet allowed_alpns_;
  const ProxyInfo proxy_info_;
  const AlternativeServiceInfo alternative_service_info_;
  const AdvertisedAltSvcState advertised_alt_svc_state_;

  const HttpStreamKey origin_stream_key_;
  const QuicSessionAliasKey origin_quic_key_;
  quic::ParsedQuicVersion origin_quic_version_ =
      quic::ParsedQuicVersion::Unsupported();

  const std::optional<Alternative> alternative_;

  const NetLogWithSource net_log_;
  const perfetto::Flow flow_;

  const base::TimeTicks created_time_;

  std::optional<base::TimeTicks> stream_ready_time_;

  // Fields specific to stream request.
  raw_ptr<HttpStreamRequest::Delegate> delegate_;
  raw_ptr<HttpStreamRequest> stream_request_;

  // Field specific to preconnect.
  CompletionOnceCallback preconnect_callback_;

  std::unique_ptr<Job> origin_job_;
  std::optional<int> origin_job_result_;

  std::unique_ptr<Job> alternative_job_;
  // Set to `OK` when the alternative job is not needed.
  std::optional<int> alternative_job_result_;

  // Populated when a stream is successfully created. Stored as a field rather
  // than bound to a callback so that on destruction, the stream is destroyed
  // when the controller is. Otherwise, on destruction of the network stack, if
  // the HttpStream has any posted asynchronous tasks, they'll trigger a UAF
  // when they're run.
  std::optional<PendingStream> pending_stream_;

  base::WeakPtrFactory<JobController> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_JOB_CONTROLLER_H_
