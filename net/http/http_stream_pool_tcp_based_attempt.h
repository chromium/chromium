// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_POOL_TCP_BASED_ATTEMPT_H_
#define NET_HTTP_HTTP_STREAM_POOL_TCP_BASED_ATTEMPT_H_

#include <memory>
#include <optional>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/http/http_stream_pool.h"
#include "net/socket/stream_attempt.h"
#include "net/socket/stream_socket_close_reason.h"
#include "net/socket/tls_stream_attempt.h"

namespace net {

// Represents a TCP based attempt.
class HttpStreamPool::TcpBasedAttempt : public TlsStreamAttempt::Delegate {
 public:
  TcpBasedAttempt(AttemptManager* manager,
                  TcpBasedAttemptSlot* slot,
                  IPEndPoint ip_endpoint);

  TcpBasedAttempt(const TcpBasedAttempt&) = delete;
  TcpBasedAttempt& operator=(const TcpBasedAttempt&) = delete;

  ~TcpBasedAttempt() override;

  void Start();

  void SetCancelReason(StreamSocketCloseReason reason);

  TcpBasedAttemptSlot* slot() const { return slot_; }

  void ResetSlot() { slot_ = nullptr; }

  StreamAttempt* attempt() { return attempt_.get(); }

  base::TimeTicks start_time() const { return start_time_; }

  const IPEndPoint& ip_endpoint() const { return attempt_->ip_endpoint(); }

  bool is_slow() const { return is_slow_; }

  // Set to true when the attempt is aborted. When true, the attempt will fail
  // but not be considered as an actual failure.
  bool is_aborted() const { return is_aborted_; }

  // TlsStreamAttempt::Delegate implementation:
  void OnTcpHandshakeComplete() override;
  int WaitForTlsHandshakeReady(CompletionOnceCallback callback) override;
  base::expected<ServiceEndpoint, TlsStreamAttempt::GetServiceEndpointError>
  GetServiceEndpointForTlsHandshake() override;

  bool IsWaitingForServiceEndpointReady() const {
    return !service_endpoint_waiting_callback_.is_null();
  }

  // Transfers `ssl_config_waiting_callback_` when `this` is waiting for
  // SSLConfig.
  std::optional<CompletionOnceCallback> MaybeTakeSSLConfigWaitingCallback();

  base::Value::Dict GetInfoAsValue() const;

 private:
  void OnAttemptSlow();
  void OnAttemptComplete(int rv);

  const raw_ptr<AttemptManager> manager_;
  const perfetto::Track track_;
  const perfetto::Flow flow_;
  raw_ptr<TcpBasedAttemptSlot> slot_;
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

  // Set to the time `attempt_` completes the TCP handshake. Only set when the
  // underlying attempt is TLS. Used for histogram recording.
  base::TimeTicks tcp_handshake_complete_time_for_tls_;

  base::TimeTicks service_endpoint_wait_start_time_;
  base::TimeTicks service_endpoint_wait_end_time_;
  CompletionOnceCallback service_endpoint_waiting_callback_;

  base::WeakPtrFactory<TcpBasedAttempt> weak_ptr_factory_{this};
};

// Groups at most two concurrent TCP-based attempts (one IPv4, one IPv6) into a
// single “slot” counted against pool limits. Used to work around cases where
// both address families are available but one is much slower than the other. In
// such cases, the slow attempt may time out, causing the whole pool to stall,
// even if the fast attempt would have succeeded. By grouping attempts by
// address family, we can ensure that at most one attempt per address family is
// in-flight at any time.
// TODO(crbug.com/383606724): Figure out a better solution by improving endpoint
// selection.
class HttpStreamPool::TcpBasedAttemptSlot {
 public:
  TcpBasedAttemptSlot();
  ~TcpBasedAttemptSlot();

  TcpBasedAttemptSlot(const TcpBasedAttemptSlot&) = delete;
  TcpBasedAttemptSlot& operator=(const TcpBasedAttemptSlot&) = delete;
  TcpBasedAttemptSlot(TcpBasedAttemptSlot&&);
  TcpBasedAttemptSlot& operator=(TcpBasedAttemptSlot&&);

  // Allocates `attempt` to either IPv4 or IPv6 attempt slot based on its IP
  // address.
  void AllocateAttempt(std::unique_ptr<TcpBasedAttempt> attempt);

  // Transfers ownership of the attempt matching `raw_attempt` to the caller.
  std::unique_ptr<TcpBasedAttempt> TakeAttempt(TcpBasedAttempt* raw_attempt);

  TcpBasedAttempt* ipv4_attempt() const { return ipv4_attempt_.get(); }
  TcpBasedAttempt* ipv6_attempt() const { return ipv6_attempt_.get(); }

  // Returns true if this slot has no attempts.
  bool empty() const { return !ipv4_attempt() && !ipv6_attempt(); }

  // Returns the most advanced load state of the attempts in this slot.
  LoadState GetLoadState() const;

  // Transfers SSLConfig waiting callbacks from attempts in this slot to
  // `callbacks`, if attempts are waiting for SSLConfig.
  void MaybeTakeSSLConfigWaitingCallbacks(
      std::vector<CompletionOnceCallback>& callbacks);

  // Returns true when this slot is slow. A slot is considered slow all attempts
  // it owns are slow.
  bool IsSlow() const;

  // Returns true if either IPv4 or IPv6 attempt has the given `ip_endpoint`.
  bool HasIPEndPoint(const IPEndPoint& ip_endpoint) const;

  // Sets the cancel reason of both attempts in this slot.
  void SetCancelReason(StreamSocketCloseReason reason);

  // Updates `is_slow_` based on current state of `ipv4_attempt_` and
  // `ipv6_attempt_`. Called when an attempt is added, removed, or marked as
  // slow.
  void UpdateIsSlow();

  base::Value::Dict GetInfoAsValue() const;

 private:
  // Re-calculates whether this slot is considered slow, without updating
  // `is_slow_`. This is not inlined in UpdateIsSlow() so that it can be used in
  // DCHECKs.
  bool CalculateIsSlow() const;

  std::unique_ptr<TcpBasedAttempt> ipv4_attempt_;
  std::unique_ptr<TcpBasedAttempt> ipv6_attempt_;

  // False if either of `ipv4_attempt_` or `ipv6_attempt_` is non-null and not
  // slow. Cached to reduced pointer dereferencing overhead of IsSlow() calls.
  bool is_slow_ = false;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_TCP_BASED_ATTEMPT_H_
