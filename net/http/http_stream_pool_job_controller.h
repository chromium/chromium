// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_POOL_JOB_CONTROLLER_H_
#define NET_HTTP_HTTP_STREAM_POOL_JOB_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "net/base/load_states.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/request_priority.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/alternative_service.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_job.h"
#include "net/http/http_stream_pool_switching_info.h"
#include "net/http/http_stream_request.h"
#include "net/socket/next_proto.h"
#include "net/ssl/ssl_config.h"

namespace net {

class NetLogWithSource;
class SSLCertRequestInfo;
struct NetErrorDetails;

// Manages a single HttpStreamRequest and its associated Job(s).
class HttpStreamPool::JobController : public HttpStreamPool::Job::Delegate,
                                      public HttpStreamRequest::Helper {
 public:
  explicit JobController(HttpStreamPool* pool);

  JobController(const JobController&) = delete;
  JobController& operator=(const JobController&) = delete;

  ~JobController() override;

  // Creates an HttpStreamRequest and starts Job(s) to handle it.
  std::unique_ptr<HttpStreamRequest> RequestStream(
      HttpStreamRequest::Delegate* delegate,
      HttpStreamPoolSwitchingInfo switching_info,
      RequestPriority priority,
      const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
      bool enable_ip_based_pooling,
      bool enable_alternative_services,
      const NetLogWithSource& net_log);

  // HttpStreamPool::Job::Delegate implementation:
  void OnStreamReady(Job* job,
                     std::unique_ptr<HttpStream> stream,
                     NextProto negotiated_protocol) override;
  void OnStreamFailed(Job* job,
                      int status,
                      const NetErrorDetails& net_error_details,
                      ResolveErrorInfo resolve_error_info) override;
  void OnCertificateError(Job* job,
                          int status,
                          const SSLInfo& ssl_info) override;
  void OnNeedsClientAuth(Job* job, SSLCertRequestInfo* cert_info) override;

  // HttpStreamRequest::Helper implementation:
  LoadState GetLoadState() const override;
  void OnRequestComplete() override;
  int RestartTunnelWithProxyAuth() override;
  void SetPriority(RequestPriority priority) override;

 private:
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

  AlternativeServiceInfo alternative_service_info_;
  NetworkAnonymizationKey network_anonymization_key_;

  raw_ptr<HttpStreamRequest::Delegate> delegate_;
  raw_ptr<HttpStreamRequest> request_;

  std::unique_ptr<Job> origin_job_;
  std::optional<int> origin_job_result_;

  std::unique_ptr<Job> alternative_job_;
  // Set to `OK` when the alternative job is not needed.
  std::optional<int> alternative_job_result_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_JOB_CONTROLLER_H_
