// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HEARTBEAT_SERVICE_CLIENT_H_
#define REMOTING_HOST_HEARTBEAT_SERVICE_CLIENT_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace remoting {

class ProtobufHttpStatus;

// HeartbeatServiceClient is an interface which is used by the HeartbeatSender
// to target a specific backend API and hide implementation details of that API
// from the HeartbeatSender class.
class HeartbeatServiceClient {
 public:
  using UpdateRemoteAccessHostResponseCallback =
      base::OnceCallback<void(const ProtobufHttpStatus& status,
                              base::TimeDelta wait_interval,
                              const std::string& primary_user_email,
                              bool require_session_authorization,
                              bool use_lite_heartbeat)>;
  using SendHeartbeatResponseCallback =
      base::OnceCallback<void(const ProtobufHttpStatus& status,
                              base::TimeDelta wait_interval)>;

  HeartbeatServiceClient();

  HeartbeatServiceClient(const HeartbeatServiceClient&) = delete;
  HeartbeatServiceClient& operator=(const HeartbeatServiceClient&) = delete;

  virtual ~HeartbeatServiceClient();

  // Updates a set of attributes for |directory_id_| and the last_seen_time in
  // the Directory service.
  // TODO: joedow - Remove |is_initial_heartbeat| once SendHeartbeat is used for
  // public Me2Me hosts.
  virtual void UpdateRemoteAccessHost(
      bool is_initial_heartbeat,
      std::optional<std::string> ftl_signaling_id,
      std::optional<std::string> offline_reason,
      UpdateRemoteAccessHostResponseCallback callback) = 0;

  // Updates the last_seen_time for |directory_id_| in the Directory service.
  virtual void SendHeartbeat(SendHeartbeatResponseCallback callback) = 0;

  // Cancels any pending service requests.
  virtual void CancelPendingRequests() = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_HEARTBEAT_SERVICE_CLIENT_H_
