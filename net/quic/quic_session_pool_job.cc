// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session_pool_job.h"

#include "base/memory/weak_ptr.h"
#include "base/not_fatal_until.h"
#include "net/base/completion_once_callback.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_handle.h"
#include "net/base/proxy_chain.h"
#include "net/base/request_priority.h"
#include "net/base/trace_constants.h"
#include "net/base/tracing.h"
#include "net/dns/host_resolver.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/address_utils.h"
#include "net/quic/quic_crypto_client_config_handle.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_session_pool.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

namespace {

base::Value::Dict NetLogQuicSessionPoolJobParams(
    const QuicSessionAliasKey* key) {
  const ProxyChain& proxy_chain = key->session_key().proxy_chain();
  return base::Value::Dict()
      .Set("host", key->server_id().host())
      .Set("port", key->server_id().port())
      .Set("privacy_mode",
           PrivacyModeToDebugString(key->session_key().privacy_mode()))
      .Set("proxy_chain",
           proxy_chain.IsValid() ? proxy_chain.ToDebugString() : "invalid")
      .Set("network_anonymization_key",
           key->session_key().network_anonymization_key().ToDebugString());
}

}  // namespace

QuicSessionPool::Job::Job(
    QuicSessionPool* pool,
    QuicSessionAliasKey key,
    std::unique_ptr<CryptoClientConfigHandle> client_config_handle,
    RequestPriority priority,
    const NetLogWithSource& net_log)
    : pool_(pool),
      key_(std::move(key)),
      client_config_handle_(std::move(client_config_handle)),
      priority_(priority),
      net_log_(net_log) {
  net_log_.BeginEvent(NetLogEventType::QUIC_SESSION_POOL_JOB,
                      [&] { return NetLogQuicSessionPoolJobParams(&key_); });
}

QuicSessionPool::Job::~Job() {
  net_log_.EndEvent(NetLogEventType::QUIC_SESSION_POOL_JOB);
}

void QuicSessionPool::Job::AddRequest(QuicSessionRequest* request) {
  requests_.insert(request);
  SetRequestExpectations(request);
}

void QuicSessionPool::Job::RemoveRequest(QuicSessionRequest* request) {
  auto request_iter = requests_.find(request);
  CHECK(request_iter != requests_.end(), base::NotFatalUntil::M130);
  requests_.erase(request_iter);
}

void QuicSessionPool::Job::SetPriority(RequestPriority priority) {
  UpdatePriority(priority_, priority);
  priority_ = priority;
}

void QuicSessionPool::Job::AssociateWithNetLogSource(
    const NetLogWithSource& http_stream_job_net_log) const {
  net_log().AddEventReferencingSource(
      NetLogEventType::QUIC_SESSION_POOL_JOB_BOUND_TO,
      http_stream_job_net_log.source());
  http_stream_job_net_log.AddEventReferencingSource(
      NetLogEventType::BOUND_TO_QUIC_SESSION_POOL_JOB, net_log().source());
}

QuicSessionPool* QuicSessionPool::Job::GetQuicSessionPool() {
  return pool();
}

const QuicSessionAliasKey& QuicSessionPool::Job::GetKey() {
  return key();
}

const NetLogWithSource& QuicSessionPool::Job::GetNetLog() {
  return net_log();
}

void QuicSessionPool::Job::OnConnectionFailedOnDefaultNetwork() {
  for (QuicSessionRequest* request : requests()) {
    request->OnConnectionFailedOnDefaultNetwork();
  }
}

void QuicSessionPool::Job::OnQuicSessionCreationComplete(int rv) {
  for (QuicSessionRequest* request : requests()) {
    request->OnQuicSessionCreationComplete(rv);
  }
}

void QuicSessionPool::Job::UpdatePriority(RequestPriority old_priority,
                                          RequestPriority new_priority) {}

}  // namespace net
