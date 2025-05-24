// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_POOL_ATTEMPT_MANAGER_TCP_BASED_ATTEMPT_H_
#define NET_HTTP_HTTP_STREAM_POOL_ATTEMPT_MANAGER_TCP_BASED_ATTEMPT_H_

#include <memory>
#include <optional>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/base/tracing.h"
#include "net/http/http_stream_pool_attempt_manager.h"
#include "net/socket/stream_attempt.h"
#include "net/socket/stream_socket_close_reason.h"
#include "net/socket/tls_stream_attempt.h"

namespace net {

// Represents a TCP based attempt.
class HttpStreamPool::AttemptManager::TcpBasedAttempt
    : public TlsStreamAttempt::Delegate {
 public:
  TcpBasedAttempt(AttemptManager* manager,
                  bool using_tls,
                  IPEndPoint ip_endpoint);

  TcpBasedAttempt(const TcpBasedAttempt&) = delete;
  TcpBasedAttempt& operator=(const TcpBasedAttempt&) = delete;

  ~TcpBasedAttempt() override;

  void Start();

  void SetCancelReason(StreamSocketCloseReason reason);

  StreamAttempt* attempt() { return attempt_.get(); }

  base::TimeTicks start_time() const { return start_time_; }
  base::TimeTicks ssl_config_wait_start_time() const {
    return ssl_config_wait_start_time_;
  }

  const IPEndPoint& ip_endpoint() const { return attempt_->ip_endpoint(); }

  bool is_slow() const { return is_slow_; }
  void set_is_slow(bool is_slow) { is_slow_ = is_slow; }

  // Set to true when the attempt is aborted. When true, the attempt will fail
  // but not be considered as an actual failure.
  bool is_aborted() const { return is_aborted_; }

  // TlsStreamAttempt::Delegate implementation:
  void OnTcpHandshakeComplete() override;
  int WaitForSSLConfigReady(CompletionOnceCallback callback) override;
  base::expected<SSLConfig, TlsStreamAttempt::GetSSLConfigError> GetSSLConfig()
      override;

  bool IsWaitingSSLConfig() const {
    return !ssl_config_waiting_callback_.is_null();
  }

  // Transfers `ssl_config_waiting_callback_` when `this` is waiting for
  // SSLConfig.
  std::optional<CompletionOnceCallback> MaybeTakeSSLConfigWaitingCallback();

  base::Value::Dict GetInfoAsValue() const;

 private:
  void OnAttemptComplete(int rv);

  const raw_ptr<AttemptManager> manager_;
  const perfetto::Track track_;
  const perfetto::Flow flow_;
  std::unique_ptr<StreamAttempt> attempt_;
  base::TimeTicks start_time_;
  std::optional<int> result_;
  std::optional<StreamSocketCloseReason> cancel_reason_;
  // Timer to start a next attempt. When fired, `this` is treated as a slow
  // attempt but `this` is not timed out yet.
  base::OneShotTimer slow_timer_;
  // Set to true when `slow_timer_` is fired. See the comment of `slow_timer_`.
  bool is_slow_ = false;
  // Set to true when `this` and `attempt_` should abort. Currently used to
  // handle ECH failure.
  bool is_aborted_ = false;
  base::TimeTicks ssl_config_wait_start_time_;
  CompletionOnceCallback ssl_config_waiting_callback_;

  base::WeakPtrFactory<TcpBasedAttempt> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_ATTEMPT_MANAGER_TCP_BASED_ATTEMPT_H_
