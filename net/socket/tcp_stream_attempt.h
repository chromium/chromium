// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_STREAM_ATTEMPT_H_
#define NET_SOCKET_TCP_STREAM_ATTEMPT_H_

#include <string_view>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/socket/stream_attempt.h"

namespace net {

class NetLogWithSource;

// Represents a single TCP connection attempt.
class NET_EXPORT_PRIVATE TcpStreamAttempt final : public StreamAttempt {
 public:
  // This timeout is shorter than TransportConnectJob::ConnectionTimeout()
  // because a TcpStreamAttempt only attempts a single TCP connection.
  static constexpr base::TimeDelta kTcpHandshakeTimeout = base::Seconds(60);

  TcpStreamAttempt(const StreamAttemptParams* params,
                   IPEndPoint ip_endpoint,
                   perfetto::Track track,
                   const NetLogWithSource* = nullptr);

  TcpStreamAttempt(const TcpStreamAttempt&) = delete;
  TcpStreamAttempt& operator=(const TcpStreamAttempt&) = delete;

  ~TcpStreamAttempt() override;

  LoadState GetLoadState() const override;

  base::Value::Dict GetInfoAsValue() const override;

 private:
  enum class State {
    kNone,
    kConnecting,
  };

  static std::string_view StateToString(State state);

  // StreamAttempt methods:
  int StartInternal() override;
  base::Value::Dict GetNetLogStartParams() override;

  void HandleCompletion(int rv);

  void OnIOComplete(int rv);

  void OnTimeout();

  void MaybeRecordConnectEnd(int rv);

  State next_state_ = State::kNone;
  base::OneShotTimer timeout_timer_;
};

}  // namespace net

#endif  // NET_SOCKET_TCP_STREAM_ATTEMPT_H_
