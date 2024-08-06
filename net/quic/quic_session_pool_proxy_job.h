// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SESSION_POOL_PROXY_JOB_H_
#define NET_QUIC_QUIC_SESSION_POOL_PROXY_JOB_H_

#include <memory>

#include "net/base/http_user_agent_settings.h"
#include "net/base/request_priority.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_session_attempt.h"
#include "net/quic/quic_session_pool.h"
#include "net/quic/quic_session_pool_job.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

// A ProxyJob is a QuicSessionPool::Job that handles connections to the
// destination over a (QUIC) proxy.
class QuicSessionPool::ProxyJob : public QuicSessionPool::Job {
 public:
  ProxyJob(QuicSessionPool* pool,
           quic::ParsedQuicVersion target_quic_version,
           QuicSessionAliasKey key,
           NetworkTrafficAnnotationTag proxy_annotation_tag,
           const HttpUserAgentSettings* http_user_agent_settings,
           std::unique_ptr<CryptoClientConfigHandle> client_config_handle,
           RequestPriority priority,
           int cert_verify_flags,
           const NetLogWithSource& net_log);

  ~ProxyJob() override;

  // QuicSessionPool::Job implementation.
  int Run(CompletionOnceCallback callback) override;
  void SetRequestExpectations(QuicSessionRequest* request) override;
  void PopulateNetErrorDetails(NetErrorDetails* details) const override;

 private:
  int DoLoop(int rv);
  int DoCreateProxySession();
  int DoCreateProxySessionComplete(int rv);
  int DoCreateProxyStream();
  int DoCreateProxyStreamComplete(int rv);
  int DoAttemptSession();

  void OnIOComplete(int rv);
  void OnSessionAttemptComplete(int rv);

  base::WeakPtr<ProxyJob> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

  enum IoState {
    STATE_NONE,
    STATE_CREATE_PROXY_SESSION,
    STATE_CREATE_PROXY_SESSION_COMPLETE,
    STATE_CREATE_PROXY_STREAM,
    STATE_CREATE_PROXY_STREAM_COMPLETE,
    STATE_ATTEMPT_SESSION,
  };

  CompletionRepeatingCallback io_callback_;
  IoState io_state_ = STATE_CREATE_PROXY_SESSION;

  std::unique_ptr<QuicSessionRequest> proxy_session_request_;
  std::unique_ptr<QuicChromiumClientSession::Handle> proxy_session_;
  std::unique_ptr<QuicChromiumClientStream::Handle> proxy_stream_;
  NetErrorDetails net_error_details_;

  // The QUIC version for the tunneled session created by this job.
  quic::ParsedQuicVersion target_quic_version_;

  NetworkTrafficAnnotationTag proxy_annotation_tag_;
  const int cert_verify_flags_;
  raw_ptr<const HttpUserAgentSettings> http_user_agent_settings_;
  CompletionOnceCallback callback_;
  std::unique_ptr<QuicSessionAttempt> session_attempt_;
  base::WeakPtrFactory<ProxyJob> weak_factory_{this};
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SESSION_POOL_PROXY_JOB_H_
