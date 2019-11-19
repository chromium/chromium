// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_FACTORY_JOB_CONTROLLER_H_
#define NET_HTTP_HTTP_STREAM_FACTORY_JOB_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/cancelable_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/privacy_mode.h"
#include "net/http/http_stream_factory_job.h"
#include "net/http/http_stream_request.h"
#include "net/socket/next_proto.h"
#include "net/spdy/spdy_session_pool.h"

namespace net {

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
                const HttpRequestInfo& request_info,
                bool is_preconnect,
                bool is_websocket,
                bool enable_ip_based_pooling,
                bool enable_alternative_services,
                const SSLConfig& server_ssl_config,
                const SSLConfig& proxy_ssl_config);

  ~JobController() override;

  // Used in tests only for verification purpose.
  const Job* main_job() const { return main_job_.get(); }
  const Job* alternative_job() const { return alternative_job_.get(); }

  GURL ApplyHostMappingRules(const GURL& url, HostPortPair* endpoint);

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
  void OnStreamReady(Job* job, const SSLConfig& used_ssl_config) override;

  // Invoked when |job| has a BidirectionalStream ready.
  void OnBidirectionalStreamImplReady(
      Job* job,
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info) override;

  // Invoked when |job| has a WebSocketHandshakeStream ready.
  void OnWebSocketHandshakeStreamReady(
      Job* job,
      const SSLConfig& used_ssl_config,
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<WebSocketHandshakeStreamBase> stream) override;

  // Invoked when |job| fails to create a stream.
  void OnStreamFailed(Job* job,
                      int status,
                      const SSLConfig& used_ssl_config) override;

  // Invoked when |job| fails on the default network.
  void OnFailedOnDefaultNetwork(Job* job) override;

  // Invoked when |job| has a certificate error for the Request.
  void OnCertificateError(Job* job,
                          int status,
                          const SSLConfig& used_ssl_config,
                          const SSLInfo& ssl_info) override;

  // Invoked when |job| raises failure for SSL Client Auth.
  void OnNeedsClientAuth(Job* job,
                         const SSLConfig& used_ssl_config,
                         SSLCertRequestInfo* cert_info) override;

  // Invoked when |job| needs proxy authentication.
  void OnNeedsProxyAuth(Job* job,
                        const HttpResponseInfo& proxy_response,
                        const SSLConfig& used_ssl_config,
                        const ProxyInfo& used_proxy_info,
                        HttpAuthController* auth_controller) override;

  // Invoked when the |job| finishes pre-connecting sockets.
  void OnPreconnectsComplete(Job* job) override;

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

  // Returns the estimated memory usage in bytes.
  size_t EstimateMemoryUsage() const;

 private:
  friend class test::JobControllerPeer;

  enum State {
    STATE_RESOLVE_PROXY,
    STATE_RESOLVE_PROXY_COMPLETE,
    STATE_CREATE_JOBS,
    STATE_NONE
  };

  void OnIOComplete(int result);
  void OnResolveProxyError(int error);
  void RunLoop(int result);
  int DoLoop(int result);
  int DoResolveProxy();
  int DoResolveProxyComplete(int result);
  // Creates Job(s) for |request_info_|. Job(s) will be owned by |this|.
  int DoCreateJobs();

  // Called to bind |job| to the |request_| and orphan all other jobs that are
  // still associated with |request_|.
  void BindJob(Job* job);

  // Called when |request_| is destructed.
  // Job(s) associated with but not bound to |request_| will be deleted.
  void CancelJobs();

  // Called after BindJob() to notify the unbound job that its result should be
  // ignored by JobController. The unbound job can be canceled or continue until
  // completion.
  void OrphanUnboundJob();

  // Invoked when the orphaned |job| finishes.
  void OnOrphanedJobComplete(const Job* job);

  // Called when a Job succeeds.
  void OnJobSucceeded(Job* job);

  // Marks completion of the |request_|.
  void MarkRequestComplete(bool was_alpn_negotiated,
                           NextProto negotiated_protocol,
                           bool using_spdy);

  // Must be called when the alternative service job fails. |net_error| is the
  // net error of the failed alternative service job.
  void OnAlternativeServiceJobFailed(int net_error);

  // Must be called when the alternative proxy job fails. |net_error| is the
  // net error of the failed alternative proxy job.
  void OnAlternativeProxyJobFailed(int net_error);

  // Called when all Jobs complete. Reports alternative service brokenness to
  // HttpServerProperties if apply and resets net errors afterwards:
  // - report broken if the main job has no error and the alternative job has an
  //   error;
  // - report broken until default network change if the main job has no error,
  //   the alternative job has no error, but the alternative job failed on the
  //   default network.
  void MaybeReportBrokenAlternativeService();

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
  // - reset |main_job_net_error_| and |alternative_job_net_error_| to OK;
  // - reset |alternative_job_failed_on_default_network_| to false.
  void ResetErrorStatusForJobs();

  AlternativeServiceInfo GetAlternativeServiceInfoFor(
      const HttpRequestInfo& request_info,
      HttpStreamRequest::Delegate* delegate,
      HttpStreamRequest::StreamType stream_type);

  AlternativeServiceInfo GetAlternativeServiceInfoInternal(
      const HttpRequestInfo& request_info,
      HttpStreamRequest::Delegate* delegate,
      HttpStreamRequest::StreamType stream_type);

  // Returns a quic::ParsedQuicVersion that has been advertised in
  // |advertised_versions| and is supported.  If more than one
  // ParsedQuicVersions are supported, the first matched in the supported
  // versions will be returned.  If no mutually supported version is found,
  // QUIC_VERSION_UNSUPPORTED_VERSION will be returned.
  quic::ParsedQuicVersion SelectQuicVersion(
      const quic::ParsedQuicVersionVector& advertised_versions);

  // Returns true if the |request_| can be fetched via an alternative
  // proxy server, and sets |alternative_proxy_info| to the alternative proxy
  // server configuration. |alternative_proxy_info| should not be null,
  // and is owned by the caller.
  bool ShouldCreateAlternativeProxyServerJob(
      const ProxyInfo& proxy_info_,
      const GURL& url,
      ProxyInfo* alternative_proxy_info) const;

  // Records histogram metrics for the usage of alternative protocol. Must be
  // called when |job| has succeeded and the other job will be orphaned.
  void ReportAlternateProtocolUsage(Job* job) const;

  // Returns whether |job| is an orphaned job.
  bool IsJobOrphaned(Job* job) const;

  // Called when a Job encountered a network error that could be resolved by
  // trying a new proxy configuration. If there is another proxy configuration
  // to try then this method sets |next_state_| appropriately and returns either
  // OK or ERR_IO_PENDING depending on whether or not the new proxy
  // configuration is available synchronously or asynchronously.  Otherwise, the
  // given error code is simply returned.
  int ReconsiderProxyAfterError(Job* job, int error);

  // Returns true if QUIC is allowed for |host|.
  bool IsQuicAllowedForHost(const std::string& host);

  HttpStreamFactory* factory_;
  HttpNetworkSession* session_;
  JobFactory* job_factory_;

  // Request will be handed out to factory once created. This just keeps an
  // reference and is safe as |request_| will notify |this| JobController
  // when it's destructed by calling OnRequestComplete(), which nulls
  // |request_|.
  HttpStreamRequest* request_;

  HttpStreamRequest::Delegate* const delegate_;

  // True if this JobController is used to preconnect streams.
  const bool is_preconnect_;

  // True if request is for Websocket.
  const bool is_websocket_;

  // Enable pooling to a SpdySession with matching IP and certificate even if
  // the SpdySessionKey is different.
  const bool enable_ip_based_pooling_;

  // Enable using alternative services for the request.
  const bool enable_alternative_services_;

  // |main_job_| is a job waiting to see if |alternative_job_| can reuse a
  // connection. If |alternative_job_| is unable to do so, |this| will notify
  // |main_job_| to proceed and then race the two jobs.
  std::unique_ptr<Job> main_job_;
  std::unique_ptr<Job> alternative_job_;
  // The alternative service used by |alternative_job_|
  // (or by |main_job_| if |is_preconnect_|.)
  AlternativeServiceInfo alternative_service_info_;

  // Error status used for alternative service brokenness reporting.
  // Net error code of the main job. Set to OK by default.
  int main_job_net_error_;
  // Net error code of the alternative job. Set to OK by default.
  int alternative_job_net_error_;
  // Set to true if the alternative job failed on the default network.
  bool alternative_job_failed_on_default_network_;

  // True if a Job has ever been bound to the |request_|.
  bool job_bound_;

  // True if the main job has to wait for the alternative job: i.e., the main
  // job must not create a connection until it is resumed.
  bool main_job_is_blocked_;

  // Handle for cancelling any posted delayed ResumeMainJob() task.
  base::CancelableOnceClosure resume_main_job_callback_;
  // True if the main job was blocked and has been resumed in ResumeMainJob().
  bool main_job_is_resumed_;

  // Waiting time for the main job before it is resumed.
  base::TimeDelta main_job_wait_time_;

  // At the point where a Job is irrevocably tied to |request_|, we set this.
  // It will be nulled when the |request_| is finished.
  Job* bound_job_;

  // True if an alternative proxy server job can be started to fetch |request_|.
  bool can_start_alternative_proxy_job_;

  State next_state_;
  std::unique_ptr<ProxyResolutionService::Request> proxy_resolve_request_;
  const HttpRequestInfo request_info_;
  ProxyInfo proxy_info_;
  const SSLConfig server_ssl_config_;
  const SSLConfig proxy_ssl_config_;
  int num_streams_;
  HttpStreamRequest::StreamType stream_type_;
  RequestPriority priority_;
  const NetLogWithSource net_log_;

  base::WeakPtrFactory<JobController> ptr_factory_{this};
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_FACTORY_JOB_CONTROLLER_H_
