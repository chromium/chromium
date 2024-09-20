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
  // |status| will be valid for all invocations, the other args must be checked
  // as they may not be populated for all APIs.
  using HeartbeatResponseCallback =
      base::OnceCallback<void(const ProtobufHttpStatus& status,
                              std::optional<base::TimeDelta> wait_interval,
                              const std::string& primary_user_email,
                              std::optional<bool> require_session_authorization,
                              std::optional<bool> use_lite_heartbeat)>;

  HeartbeatServiceClient();

  HeartbeatServiceClient(const HeartbeatServiceClient&) = delete;
  HeartbeatServiceClient& operator=(const HeartbeatServiceClient&) = delete;

  virtual ~HeartbeatServiceClient();

  // Updates a set of attributes for |directory_id_| based on the args provided
  // as well as inherent properties (e.g. host version) when
  // |is_initial_heartbeat| is set.
  // TODO: joedow - Refactor this when we no longer support the LegacyHeartbeat
  // service API.
  virtual void SendFullHeartbeat(bool is_initial_heartbeat,
                                 std::optional<std::string> signaling_id,
                                 std::optional<std::string> offline_reason,
                                 HeartbeatResponseCallback callback) = 0;

  // Updates the last_seen_time for |directory_id_| in the Directory service.
  virtual void SendLiteHeartbeat(HeartbeatResponseCallback callback) = 0;

  // Cancels any pending service requests.
  virtual void CancelPendingRequests() = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_HEARTBEAT_SERVICE_CLIENT_H_
