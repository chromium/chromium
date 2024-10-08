// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_quic_task.h"

#include <memory>
#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_error_details.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/http/http_network_session.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_attempt_manager.h"
#include "net/http/http_stream_pool_group.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_session_alias_key.h"
#include "net/quic/quic_session_key.h"
#include "net/quic/quic_session_pool.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

HttpStreamPool::QuicTask::QuicTask(AttemptManager* manager,
                                   quic::ParsedQuicVersion quic_version)
    : manager_(manager),
      quic_session_alias_key_(manager_->group()->stream_key().destination(),
                              quic_session_key()),
      quic_version_(quic_version),
      net_log_(NetLogWithSource::Make(
          manager->net_log().net_log(),
          NetLogSourceType::HTTP_STREAM_POOL_QUIC_TASK)) {
  CHECK(manager_);
  CHECK(service_endpoint_request());
  CHECK(service_endpoint_request()->EndpointsCryptoReady());

  net_log_.BeginEvent(NetLogEventType::HTTP_STREAM_POOL_QUIC_TASK_ALIVE, [&] {
    base::Value::Dict dict;
    dict.Set("quic_version", quic::ParsedQuicVersionToString(quic_version_));
    manager_->net_log().source().AddToEventParameters(dict);
    return dict;
  });
  manager_->net_log().AddEventReferencingSource(
      NetLogEventType::HTTP_STREAM_POOL_ATTEMPT_MANAGER_QUIC_TASK_BOUND,
      net_log_.source());
}

HttpStreamPool::QuicTask::~QuicTask() {
  net_log_.EndEvent(NetLogEventType::HTTP_STREAM_POOL_QUIC_TASK_ALIVE);
}

void HttpStreamPool::QuicTask::MaybeAttempt() {
  CHECK(!quic_session_pool()->CanUseExistingSession(
      quic_session_key(), stream_key().destination()));

  if (session_attempt_) {
    // TODO(crbug.com/346835898): Support multiple attempts.
    return;
  }

  std::optional<QuicEndpoint> quic_endpoint = GetQuicEndpointToAttempt();
  if (!quic_endpoint.has_value()) {
    if (manager_->is_service_endpoint_request_finished()) {
      if (!start_result_.has_value()) {
        start_result_ = ERR_DNS_NO_MATCHING_SUPPORTED_ALPN;
      }
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&QuicTask::OnSessionAttemptComplete,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    ERR_DNS_NO_MATCHING_SUPPORTED_ALPN));
    }
    return;
  }

  SSLConfig ssl_config;
  ssl_config.disable_cert_verification_network_fetches =
      stream_key().disable_cert_network_fetches();
  int cert_verify_flags = ssl_config.GetCertVerifyFlags();

  base::TimeTicks dns_resolution_start_time =
      manager_->dns_resolution_start_time();
  // The DNS resolution end time could be null when the resolution is still
  // ongoing. In that case, use the current time to make sure the connect
  // start time is already greater than the DNS resolution end time.
  base::TimeTicks dns_resolution_end_time =
      manager_->dns_resolution_end_time().is_null()
          ? base::TimeTicks::Now()
          : manager_->dns_resolution_end_time();

  std::set<std::string> dns_aliases =
      service_endpoint_request()->GetDnsAliasResults();

  net_log_.AddEvent(NetLogEventType::HTTP_STREAM_POOL_QUIC_ATTEMPT_START,
                    [&] { return quic_endpoint->ToValue(); });

  session_attempt_ = quic_session_pool()->CreateSessionAttempt(
      this, quic_session_key(), std::move(*quic_endpoint), cert_verify_flags,
      dns_resolution_start_time, dns_resolution_end_time,
      /*use_dns_aliases=*/true, std::move(dns_aliases));

  int rv = session_attempt_->Start(base::BindOnce(
      &QuicTask::OnSessionAttemptComplete, weak_ptr_factory_.GetWeakPtr()));
  if (rv != ERR_IO_PENDING) {
    OnSessionAttemptComplete(rv);
  }
}

QuicSessionPool* HttpStreamPool::QuicTask::GetQuicSessionPool() {
  return manager_->group()->http_network_session()->quic_session_pool();
}

const QuicSessionAliasKey& HttpStreamPool::QuicTask::GetKey() {
  return quic_session_alias_key_;
}

const NetLogWithSource& HttpStreamPool::QuicTask::GetNetLog() {
  return net_log_;
}

const HttpStreamKey& HttpStreamPool::QuicTask::stream_key() const {
  return manager_->group()->stream_key();
}

const QuicSessionKey& HttpStreamPool::QuicTask::quic_session_key() const {
  return manager_->group()->quic_session_key();
}

QuicSessionPool* HttpStreamPool::QuicTask::quic_session_pool() {
  return manager_->group()->http_network_session()->quic_session_pool();
}

HostResolver::ServiceEndpointRequest*
HttpStreamPool::QuicTask::service_endpoint_request() {
  return manager_->service_endpoint_request();
}

std::optional<QuicEndpoint>
HttpStreamPool::QuicTask::GetQuicEndpointToAttempt() {
  for (auto& endpoint : service_endpoint_request()->GetEndpointResults()) {
    std::optional<QuicEndpoint> quic_endpoint =
        GetQuicEndpointFromServiceEndpoint(endpoint);
    if (quic_endpoint.has_value()) {
      return quic_endpoint;
    }
  }

  return std::nullopt;
}

std::optional<QuicEndpoint>
HttpStreamPool::QuicTask::GetQuicEndpointFromServiceEndpoint(
    const ServiceEndpoint& service_endpoint) {
  // TODO(crbug.com/346835898): Support ECH.
  quic::ParsedQuicVersion endpoint_quic_version =
      quic_session_pool()->SelectQuicVersion(
          quic_version_, service_endpoint.metadata, /*svcb_optional=*/true);
  if (!endpoint_quic_version.IsKnown()) {
    return std::nullopt;
  }

  // TODO(crbug.com/346835898): Attempt more than one endpoints.
  std::optional<IPEndPoint> ip_endpoint =
      GetPreferredIPEndPoint(service_endpoint.ipv6_endpoints);
  if (!ip_endpoint.has_value()) {
    ip_endpoint = GetPreferredIPEndPoint(service_endpoint.ipv4_endpoints);
  }

  if (!ip_endpoint.has_value()) {
    return std::nullopt;
  }

  return QuicEndpoint(endpoint_quic_version, *ip_endpoint,
                      service_endpoint.metadata);
}

std::optional<IPEndPoint> HttpStreamPool::QuicTask::GetPreferredIPEndPoint(
    const std::vector<IPEndPoint>& ip_endpoints) {
  // TODO(crbug.com/346835898): Attempt more than one endpoints.
  return ip_endpoints.empty() ? std::nullopt : std::optional(ip_endpoints[0]);
}

void HttpStreamPool::QuicTask::OnSessionAttemptComplete(int rv) {
  if (rv == OK) {
    QuicChromiumClientSession* session =
        quic_session_pool()->FindExistingSession(quic_session_key(),
                                                 stream_key().destination());
    if (!session) {
      // QUIC session is closed before stream can be created.
      rv = ERR_CONNECTION_CLOSED;
    }
  }

  net_log_.AddEventWithNetErrorCode(
      NetLogEventType::HTTP_STREAM_POOL_QUIC_ATTEMPT_END, rv);

  // TODO(crbug.com/346835898): Attempt other endpoints when failed.

  if (rv == OK &&
      !quic_session_pool()->has_quic_ever_worked_on_current_network()) {
    quic_session_pool()->set_has_quic_ever_worked_on_current_network(true);
  }

  NetErrorDetails details;
  if (session_attempt_) {
    session_attempt_->PopulateNetErrorDetails(&details);
  }
  session_attempt_.reset();
  manager_->OnQuicTaskComplete(rv, std::move(details));
  // `this` is deleted.
}

}  // namespace net
