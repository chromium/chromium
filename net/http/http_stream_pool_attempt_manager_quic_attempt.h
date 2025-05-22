// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_POOL_ATTEMPT_MANAGER_QUIC_ATTEMPT_H_
#define NET_HTTP_HTTP_STREAM_POOL_ATTEMPT_MANAGER_QUIC_ATTEMPT_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "net/base/ip_endpoint.h"
#include "net/base/tracing.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_stream_pool.h"
#include "net/http/http_stream_pool_attempt_manager.h"
#include "net/quic/quic_session_attempt.h"
#include "net/quic/quic_session_pool.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

class HttpStreamKey;
class QuicSessionAliasKey;

// Handles a single QUIC session attempt for HttpStreamPool::AttemptManager.
// Owned by an AttemptManager.
class HttpStreamPool::AttemptManager::QuicAttempt
    : public QuicSessionAttempt::Delegate {
 public:
  // `manager` must outlive `this`.
  QuicAttempt(AttemptManager* manager, QuicEndpoint quic_endpoint);

  QuicAttempt(const QuicAttempt&) = delete;
  QuicAttempt& operator=(const QuicAttempt&) = delete;

  ~QuicAttempt() override;

  void Start();

  // QuicSessionAttempt::Delegate implementation.
  QuicSessionPool* GetQuicSessionPool() override;
  const QuicSessionAliasKey& GetKey() override;
  const NetLogWithSource& GetNetLog() override;

  // Retrieves information on the current state of `this` as a base::Value.
  base::Value::Dict GetInfoAsValue() const;

  base::TimeTicks start_time() const { return start_time_; }

  bool is_slow() const { return is_slow_; }

 private:
  const HttpStreamKey& stream_key() const;

  void OnSessionAttemptSlow();
  void OnSessionAttemptComplete(int rv);

  const raw_ptr<AttemptManager> manager_;
  const QuicEndpoint quic_endpoint_;
  const base::TimeTicks start_time_;
  const NetLogWithSource net_log_;
  const perfetto::Track track_;
  const perfetto::Flow flow_;

  std::unique_ptr<QuicSessionAttempt> session_attempt_;
  base::OneShotTimer slow_timer_;
  bool is_slow_ = false;
  std::optional<int> result_;

  base::WeakPtrFactory<QuicAttempt> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_POOL_ATTEMPT_MANAGER_QUIC_ATTEMPT_H_
