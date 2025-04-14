// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_stream_pool_attempt_manager_tcp_based_attempt.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/http/http_stream_key.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_attempt_manager.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/stream_attempt.h"
#include "net/socket/stream_socket_close_reason.h"
#include "net/socket/tcp_stream_attempt.h"
#include "net/socket/tls_stream_attempt.h"

namespace net {

namespace {

std::string_view GetResultHistogramSuffix(std::optional<int> result) {
  if (result.has_value()) {
    return *result == OK ? "Success" : "Failure";
  }
  return "Canceled";
}

std::string_view GetHistogramSuffixForTcpBasedAttemptCancel(
    StreamSocketCloseReason reason) {
  switch (reason) {
    case StreamSocketCloseReason::kSpdySessionCreated:
      return "NewSpdySession";
    case StreamSocketCloseReason::kQuicSessionCreated:
      return "NewQuicSession";
    case StreamSocketCloseReason::kUsingExistingSpdySession:
      return "ExistingSpdySession";
    case StreamSocketCloseReason::kUsingExistingQuicSession:
      return "ExistingQuicSession";
    case StreamSocketCloseReason::kUnspecified:
    case StreamSocketCloseReason::kCloseAllConnections:
    case StreamSocketCloseReason::kIpAddressChanged:
    case StreamSocketCloseReason::kSslConfigChanged:
    case StreamSocketCloseReason::kCannotUseTcpBasedProtocols:
    case StreamSocketCloseReason::kAbort:
      return "Other";
  }
}

}  // namespace

HttpStreamPool::AttemptManager::TcpBasedAttempt::TcpBasedAttempt(
    AttemptManager* manager,
    bool using_tls,
    IPEndPoint ip_endpoint)
    : manager_(manager), using_tls_(using_tls) {
  if (using_tls_) {
    attempt_ = std::make_unique<TlsStreamAttempt>(
        manager_->pool()->stream_attempt_params(), std::move(ip_endpoint),
        HostPortPair::FromSchemeHostPort(manager_->stream_key().destination()),
        /*ssl_config_provider=*/this);
  } else {
    attempt_ = std::make_unique<TcpStreamAttempt>(
        manager_->pool()->stream_attempt_params(), std::move(ip_endpoint));
  }
}

HttpStreamPool::AttemptManager::TcpBasedAttempt::~TcpBasedAttempt() {
  base::TimeDelta elapsed = base::TimeTicks::Now() - start_time_;
  // TODO(bashi): Rename following histograms to use TcpBased*.
  base::UmaHistogramTimes(base::StrCat({"Net.HttpStreamPool.StreamAttemptTime.",
                                        GetResultHistogramSuffix(result_)}),
                          elapsed);

  if (cancel_reason_.has_value()) {
    base::UmaHistogramEnumeration(
        "Net.HttpStreamPool.StreamAttemptCancelReason", *cancel_reason_);

    std::string_view suffix =
        GetHistogramSuffixForTcpBasedAttemptCancel(*cancel_reason_);
    CHECK(manager_->initial_attempt_state_.has_value());
    base::UmaHistogramEnumeration(
        base::StrCat(
            {"Net.HttpStreamPool.StreamAttemptCanceledInitialAttemptState.",
             suffix}),
        *manager_->initial_attempt_state_);
    base::UmaHistogramTimes(
        base::StrCat({"Net.HttpStreamPool.StreamAttemptCanceledTime.", suffix}),
        elapsed);
  }
}

void HttpStreamPool::AttemptManager::TcpBasedAttempt::Start() {
  CHECK(attempt_);
  TlsStreamAttempt* tls_attempt_ptr =
      using_tls_ ? static_cast<TlsStreamAttempt*>(attempt_.get()) : nullptr;
  start_time_ = base::TimeTicks::Now();
  int rv = attempt_->Start(base::BindOnce(&TcpBasedAttempt::OnAttemptComplete,
                                          weak_ptr_factory_.GetWeakPtr()));
  manager_->net_log().AddEvent(
      NetLogEventType::HTTP_STREAM_POOL_ATTEMPT_MANAGER_TCP_BASED_ATTEMPT_START,
      [&] {
        base::Value::Dict dict = manager_->GetStatesAsNetLogParams();
        attempt()->net_log().source().AddToEventParameters(dict);
        return dict;
      });
  // Add NetLog dependency after Start() so that the first event of the
  // attempt can have meaningful description in the NetLog viewer.
  attempt()->net_log().AddEventReferencingSource(
      NetLogEventType::TCP_BASED_ATTEMPT_BOUND_TO_POOL,
      manager_->net_log().source());

  if (rv == ERR_IO_PENDING) {
    // SAFETY: Unretained `manager_` is fine since `manager_` owns this and
    // `this` owns `slow_timer_`.
    slow_timer_.Start(FROM_HERE, HttpStreamPool::GetConnectionAttemptDelay(),
                      base::BindOnce(&AttemptManager::OnTcpBasedAttemptSlow,
                                     base::Unretained(manager_), this));
    if (tls_attempt_ptr && !tls_attempt_ptr->IsTcpHandshakeCompleted()) {
      // SAFETY: Unretained `manager_` is fine since the passed callback runs
      // is invoked synchronously (without PostTask) when the TCP handshake
      // completes. See TlsStreamAttempt::DoTcpAttemptComplete.
      tls_attempt_ptr->SetTcpHandshakeCompletionCallback(
          base::BindOnce(&AttemptManager::OnTcpBasedAttemptTcpHandshakeComplete,
                         base::Unretained(manager_), this));
    }
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&TcpBasedAttempt::OnAttemptComplete,
                                  weak_ptr_factory_.GetWeakPtr(), rv));
  }
}

void HttpStreamPool::AttemptManager::TcpBasedAttempt::SetCancelReason(
    StreamSocketCloseReason reason) {
  cancel_reason_ = reason;
  if (attempt_) {
    attempt_->SetCancelReason(reason);
  }
}

int HttpStreamPool::AttemptManager::TcpBasedAttempt::WaitForSSLConfigReady(
    CompletionOnceCallback callback) {
  int rv = manager_->WaitForSSLConfigReady();
  if (rv == ERR_IO_PENDING) {
    ssl_config_wait_start_time_ = base::TimeTicks::Now();
    ssl_config_waiting_callback_ = std::move(callback);
  }
  return rv;
}

base::expected<SSLConfig, TlsStreamAttempt::GetSSLConfigError>
HttpStreamPool::AttemptManager::TcpBasedAttempt::GetSSLConfig() {
  return manager_->GetSSLConfig(this);
}

CompletionOnceCallback HttpStreamPool::AttemptManager::TcpBasedAttempt::
    TakeSSLConfigWaitingCallback() {
  CHECK(!ssl_config_wait_start_time_.is_null());
  base::UmaHistogramTimes("Net.HttpStreamPool.StreamAttemptSSLConfigWaitTime",
                          base::TimeTicks::Now() - ssl_config_wait_start_time_);

  return std::move(ssl_config_waiting_callback_);
}

base::Value::Dict
HttpStreamPool::AttemptManager::TcpBasedAttempt::GetInfoAsValue() const {
  base::Value::Dict dict;
  if (attempt_) {
    dict.Set("attempt_state", attempt_->GetInfoAsValue());
    dict.Set("ip_endpoint", attempt_->ip_endpoint().ToString());
    if (attempt_->stream_socket()) {
      attempt_->stream_socket()->NetLog().source().AddToEventParameters(dict);
    }
  }
  dict.Set("is_slow", is_slow_);
  dict.Set("is_aborted", is_aborted_);
  dict.Set("started", !start_time_.is_null());
  if (!start_time_.is_null()) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - start_time_;
    dict.Set("elapsed_ms", static_cast<int>(elapsed.InMilliseconds()));
  }
  if (result_.has_value()) {
    dict.Set("result", *result_);
  }
  if (cancel_reason_.has_value()) {
    dict.Set("cancel_reason", static_cast<int>(*cancel_reason_));
  }
  manager_->net_log().source().AddToEventParameters(dict);
  return dict;
}

void HttpStreamPool::AttemptManager::TcpBasedAttempt::OnAttemptComplete(
    int rv) {
  manager_->net_log().AddEvent(
      NetLogEventType::HTTP_STREAM_POOL_ATTEMPT_MANAGER_TCP_BASED_ATTEMPT_END,
      [&] {
        base::Value::Dict dict = manager_->GetStatesAsNetLogParams();
        dict.Set("result", ErrorToString(rv));
        attempt()->net_log().source().AddToEventParameters(dict);
        return dict;
      });

  CHECK(!result_.has_value());
  result_ = rv;
  slow_timer_.Stop();
  manager_->OnTcpBasedAttemptComplete(this, rv);
}

}  // namespace net
