// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_FACTORY_JOB_CONTROLLER_H_
#define NET_HTTP_HTTP_STREAM_FACTORY_JOB_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/http/http_stream_factory_job.h"
#include "net/http/http_stream_request.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/ssl/ssl_config.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

class ProxyResolutionRequest;

namespace test {

class JobControllerPeer;

}  // namespace test

// HttpStreamFactory::JobController manages Request and Job(s).
class HttpStreamFactory::JobController
    : public HttpStreamFactory::Job::Delegate,
      public HttpStreamRequest::Helper {
 public:
  JobController(HttpStreamFactory* factory,
                HttpStreamRequest::Delegate* delegate,
                HttpNetworkSession* session,
                JobFactory* job_factory,
                const HttpRequestInfo& http_request_info,
                bool is_preconnect,
                bool is_websocket,
                bool enable_ip_based_pooling,
                bool enable_alternative_services,
                bool delay_main_job_with_available_spdy_session,
                const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs);

  ~JobController() override;

  // Used in tests only for verification purpose.
  const Job* main_job() const { return main_job_.get(); }
  const Job* alternative_job() const { return alternative_job_.get(); }
  const Job* dns_alpn_h3_job() const { return dns_alpn_h3_job_.get(); }

  // Modifies `url` in-place, applying any applicable HostMappingRules of
  // `session_` to it.
  void RewriteUrlWithHostMappingRules(GURL& url) const;

  // Same as RewriteUrlWithHostMappingRules(), but duplicates `url` instead of
  // modifying it.
  GURL DuplicateUrlWithHostMappingRules(const GURL& url) const;

  // Methods below are called by HttpStreamFactory only.
  // Creates request and hands out to HttpStreamFactory, this will also create
  // Job(s) and start serving the created request.
  std::unique_ptr<HttpStreamRequest> Start(
      HttpStreamRequest::Delegate* delegate,
      WebSocketHandshakeStreamBase::CreateHelper*
          websocket_handshake_stream_create_helper,
      const NetLogWithSource& source_net_log,
      HttpStreamRequest::StreamType stream_type,
      RequestPriority priority);

  void Preconnect(int num_streams);

  // From HttpStreamRequest::Helper.
  // Returns the LoadState for Request.
  LoadState GetLoadState() const override;

  // Called when Request is destructed. Job(s) associated with but not bound to
  // |request_| will be deleted. |request_| and |bound_job_| will be nulled if
  // ever set.
  void OnRequestComplete() override;

  // Called to resume the HttpStream creation process when necessary
  // Proxy authentication credentials are collected.
  int RestartTunnelWithProxyAuth() override;

  // Called when the priority of transaction changes.
  void SetPriority(RequestPriority priority) override;

  // From HttpStreamFactory::Job::Delegate.
  // Invoked when |job| has an HttpStream ready.
  void OnStreamReady(Job* job) override;

  // Invoked when |job| has a BidirectionalStream ready.
  void OnBidirectionalStreamImplReady(
      Job* job,
      const ProxyInfo& used_proxy_info) override;

  // Invoked when |job| has a WebSocketHandshakeStream ready.
  void OnWebSocketHandshakeStreamReady(
      Job* job,
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<WebSocketHandshakeStreamBase> stream) override;

  // Invoked when a QUIC job finished a DNS resolution.
  void OnQuicHostResolution(const url::SchemeHostPort& destination,
                            base::TimeTicks dns_resolution_start_time,
                            base::TimeTicks dns_resolution_end_time) override;

  // Invoked when |job| fails to create a stream.
  void OnStreamFailed(Job* job, int status) override;

  // Invoked when |job| fails on the default network.
  void OnFailedOnDefaultNetwork(Job* job) override;

  // Invoked when |job| has a certificate error for the Request.
  void OnCertificateError(Job* job,
                          int status,
                          const SSLInfo& ssl_info) override;

  // Invoked when |job| raises failure for SSL Client Auth.
  void OnNeedsClientAuth(Job* job, SSLCertRequestInfo* cert_info) override;

  // Invoked when |job| needs proxy authentication.
  void OnNeedsProxyAuth(Job* job,
                        const HttpResponseInfo& proxy_response,
                        const ProxyInfo& used_proxy_info,
                        HttpAuthController* auth_controller) override;

  // Invoked when the |job| finishes pre-connecting sockets.
  void OnPreconnectsComplete(Job* job, int result) override;

  // Invoked to record connection attempts made by the socket layer to
  // Request if |job| is associated with Request.
  void AddConnectionAttemptsToRequest(
      Job* job,
      const ConnectionAttempts& attempts) override;

  // Invoked when |job| finishes initiating a connection.
  // Resume the other job if there's an error raised.
  void OnConnectionInitialized(Job* job, int rv) override;

  // Return false if |job| can advance to the next state. Otherwise, |job|
  // will wait for Job::Resume() to be called before advancing.
  bool ShouldWait(Job* job) override;

  const NetLogWithSource* GetNetLog() const override;

  void MaybeSetWaitTimeForMainJob(const base::TimeDelta& delay) override;

  WebSocketHandshakeStreamBase::CreateHelper*
  websocket_handshake_stream_create_helper() override;

  bool is_preconnect() const { return is_preconnect_; }

  // Returns true if |this| has a pending request that is not completed.
  bool HasPendingRequest() const { return request_ != nullptr; }

  // Returns true if |this| has a pending main job that is not completed.
  bool HasPendingMainJob() const;

  // Returns true if |this| has a pending alternative job that is not completed.
  bool HasPendingAltJob() const;

  base::TimeDelta get_main_job_wait_time_for_tests() {
    return main_job_wait_time_;
  }

 private:
  friend class test::JobControllerPeer;

  enum State {
    STATE_RESOLVE_PROXY,
    STATE_RESOLVE_PROXY_COMPLETE,
    STATE_CREATE_JOBS,
    STATE_NONE
  };

  void OnIOComplete(int result);

  void RunLoop(int result);
  int DoLoop(int result);
  int DoResolveProxy();
  int DoResolveProxyComplete(int result);
  // Creates Job(s) for |request_info_|. Job(s) will be owned by |this|.
  int DoCreateJobs();

  // Called to bind |job| to the |request_| and orphan all other jobs that are
  // still associated with |request_|.
  void BindJob(Job* job);

  // Called after BindJob() to notify the unbound job that its result should be
  // ignored by JobController. The unbound job can be canceled or continue until
  // completion.
  void OrphanUnboundJob();

  // Invoked when the orphaned |job| finishes.
  void OnOrphanedJobComplete(const Job* job);

  // Called when a Job succeeds.
  void OnJobSucceeded(Job* job);

  // Clears inappropriate jobs before starting them.
  void ClearInappropriateJobs();

  // Marks completion of the |request_|.
  void MarkRequestComplete(Job* job);

  // Called when all Jobs complete. Reports alternative service brokenness to
  // HttpServerProperties if apply and resets net errors afterwards:
  // - report broken if the main job has no error and the alternative job has an
  //   error;
  // - report broken until default network change if the main job has no error,
  //   the alternative job has no error, but the alternative job failed on the
  //   default network.
  void MaybeReportBrokenAlternativeService(
      const AlternativeService& alt_service,
      int alt_job_net_error,
      bool alt_job_failed_on_default_network,
      const std::string& histogram_name_for_failure);

  void MaybeNotifyFactoryOfCompletion();

  void NotifyRequestFailed(int rv);

  // Called to resume the main job with delay. Main job is resumed only when
  // |alternative_job_| has failed or |main_job_wait_time_| elapsed.
  void MaybeResumeMainJob(Job* job, const base::TimeDelta& delay);

  // Posts a task to resume the main job after |delay|.
  void ResumeMainJobLater(const base::TimeDelta& delay);

  // Resumes the main job immediately.
  void ResumeMainJob();

  // Reset error status to default value for Jobs:
  // - reset |main_job_net_error_| and |alternative_job_net_error_| and
  //   |dns_alpn_h3_job_net_error_| to OK;
  // - reset |alternative_job_failed_on_default_network_| and
  //   |dns_alpn_h3_job_failed_on_default_network_| to false.
  void ResetErrorStatusForJobs();

  AlternativeServiceInfo GetAlternativeServiceInfoFor(
      const GURL& http_request_info_url,
      const StreamRequestInfo& request_info,
      HttpStreamRequest::Delegate* delegate,
      HttpStreamRequest::StreamType stream_type);

  AlternativeServiceInfo GetAlternativeServiceInfoInternal(
      const GURL& http_request_info_url,
      const StreamRequestInfo& request_info,
      HttpStreamRequest::Delegate* delegate,
      HttpStreamRequest::StreamType stream_type);

  // Returns the first quic::ParsedQuicVersion that has been advertised in
  // |advertised_versions| and is supported, following the order of
  // |advertised_versions|.  If no mutually supported version is found,
  // quic::ParsedQuicVersion::Unsupported() will be returned.
  quic::ParsedQuicVersion SelectQuicVersion(
      const quic::ParsedQuicVersionVector& advertised_versions);

  // Records histogram metrics for the usage of alternative protocol. Must be
  // called when |job| has succeeded and the other job will be orphaned.
  void ReportAlternateProtocolUsage(
      AlternateProtocolUsage alternate_protocol_usage,
      bool is_google_host) const;

  // Returns whether |job| is an orphaned job.
  bool IsJobOrphaned(Job* job) const;

  // Calculates why Chrome uses a specific transport protocol for HTTP semantics
  // and returns it as an enum.
  // This returns ALTERNATE_PROTOCOL_USAGE_UNSPECIFIED_REASON as a default value
  // when the reason is unknown.
  AlternateProtocolUsage CalculateAlternateProtocolUsage(Job* job) const;

  // Called when a Job encountered a network error that could be resolved by
  // trying a new proxy configuration. If there is another proxy configuration
  // to try then this method sets |next_state_| appropriately and returns either
  // OK or ERR_IO_PENDING depending on whether or not the new proxy
  // configuration is available synchronously or asynchronously.  Otherwise, the
  // given error code is simply returned.
  int ReconsiderProxyAfterError(Job* job, int error);

  // Returns true if QUIC is allowed for |host|.
  bool IsQuicAllowedForHost(const std::string& host);

  int GetJobCount() const {
    return (main_job_ ? 1 : 0) + (alternative_job_ ? 1 : 0) +
           (dns_alpn_h3_job_ ? 1 : 0);
  }

  // Called when the request needs to use the HttpStreamPool instead of `this`.
  // Call site of Start() should destroy the current HttpStreamRequest and
  // switch to the HttpStreamPool. `this` will be destroyed when `request_` is
  // destroyed.
  void SwitchToHttpStreamPool(quic::ParsedQuicVersion quic_version);

  // Called when `this` asked the HttpStreamPool to handle a preconnect and
  // the preconnect completed. Used to notify the factory of completion.
  void OnPoolPreconnectsComplete(int rv);

  // Used to call HttpStreamRequest::OnSwitchesToHttpStreamPool() later.
  void CallOnSwitchesToHttpStreamPool(
      HttpStreamKey stream_key,
      AlternativeServiceInfo alternative_service_info,
      quic::ParsedQuicVersion quic_version);

  const raw_ptr<HttpStreamFactory> factory_;
  const raw_ptr<HttpNetworkSession> session_;
  const raw_ptr<JobFactory> job_factory_;

  // Request will be handed out to factory once created. This just keeps an
  // reference and is safe as |request_| will notify |this| JobController
  // when it's destructed by calling OnRequestComplete(), which nulls
  // |request_|.
  raw_ptr<HttpStreamRequest> request_ = nullptr;

  raw_ptr<HttpStreamRequest::Delegate> delegate_;

  // True if this JobController is used to preconnect streams.
  const bool is_preconnect_;

  // True if request is for Websocket.
  const bool is_websocket_;

  // Enable pooling to a SpdySession with matching IP and certificate even if
  // the SpdySessionKey is different.
  const bool enable_ip_based_pooling_;

  // Enable using alternative services for the request. If false, the
  // JobController will only create a |main_job_|.
  const bool enable_alternative_services_;

  // For normal (non-preconnect) job, |main_job_| is a job waiting to see if
  // |alternative_job_| or |dns_alpn_h3_job_| can reuse a connection. If both
  // |alternative_job_| and |dns_alpn_h3_job_| are unable to do so, |this| will
  // notify |main_job_| to proceed and then race the two jobs.
  // For preconnect job, |main_job_| is started first, and if it fails with
  // ERR_DNS_NO_MATCHING_SUPPORTED_ALPN, |preconnect_backup_job_| will be
  // started.
  std::unique_ptr<Job> main_job_;
  std::unique_ptr<Job> alternative_job_;
  std::unique_ptr<Job> dns_alpn_h3_job_;

  std::unique_ptr<Job> preconnect_backup_job_;

  // The alternative service used by |alternative_job_|
  // (or by |main_job_| if |is_preconnect_|.)
  AlternativeServiceInfo alternative_service_info_;

  // Error status used for alternative service brokenness reporting.
  // Net error code of the main job. Set to OK by default.
  int main_job_net_error_ = OK;
  // Net error code of the alternative job. Set to OK by default.
  int alternative_job_net_error_ = OK;
  // Set to true if the alternative job failed on the default network.
  bool alternative_job_failed_on_default_network_ = false;
  // Net error code of the DNS HTTPS ALPN job. Set to OK by default.
  int dns_alpn_h3_job_net_error_ = OK;
  // Set to true if the DNS HTTPS ALPN job failed on the default network.
  bool dns_alpn_h3_job_failed_on_default_network_ = false;

  // True if a Job has ever been bound to the |request_|.
  bool job_bound_ = false;

  // True if the main job has to wait for the alternative job: i.e., the main
  // job must not create a connection until it is resumed.
  bool main_job_is_blocked_ = false;

  // Handle for cancelling any posted delayed ResumeMainJob() task.
  base::CancelableOnceClosure resume_main_job_callback_;
  // True if the main job was blocked and has been resumed in ResumeMainJob().
  bool main_job_is_resumed_ = false;

  // If true, delay main job even the request can be sent immediately on an
  // available SPDY session.
  bool delay_main_job_with_available_spdy_session_;

  // Set to true when `this` asked the request to use HttpStreamPool instead
  // of `this`.
  bool switched_to_http_stream_pool_ = false;

  // Waiting time for the main job before it is resumed.
  base::TimeDelta main_job_wait_time_;

  // At the point where a Job is irrevocably tied to |request_|, we set this.
  // It will be nulled when the |request_| is finished.
  raw_ptr<Job> bound_job_ = nullptr;

  State next_state_ = STATE_RESOLVE_PROXY;
  std::unique_ptr<ProxyResolutionRequest> proxy_resolve_request_;
  // The URL from the input `http_request_info`.
  // TODO(https://crbug.com/332724851): Remove this, and update code to use
  // `origin_url_`.
  const GURL http_request_info_url_;
  // The same as `request_info_url_`, but with any applicable rules in
  // HostMappingRules applied to it.
  // TODO: Make this use SchemeHostPort instead, and rename it.
  const GURL origin_url_;
  const StreamRequestInfo request_info_;
  ProxyInfo proxy_info_;
  const std::vector<SSLConfig::CertAndStatus> allowed_bad_certs_;
  int num_streams_ = 0;
  HttpStreamRequest::StreamType stream_type_;
  RequestPriority priority_ = IDLE;
  const NetLogWithSource net_log_;

  base::WeakPtrFactory<JobController> ptr_factory_{this};
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_FACTORY_JOB_CONTROLLER_H_
