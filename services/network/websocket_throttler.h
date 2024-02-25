// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_WEBSOCKET_THROTTLER_H_
#define SERVICES_NETWORK_WEBSOCKET_THROTTLER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace network {

// WebSocketPerProcessThrottler provies a throttling functionality per
// renderer process. See https://goo.gl/tldFNn.
class COMPONENT_EXPORT(NETWORK_SERVICE) WebSocketPerProcessThrottler final {
 public:
  // A PendingConnection represents a connection that has not finished a
  // handshake.
  //
  // Destroying a PendingConnection whose OnCompleteHandshake has not been
  // called represents a handshake failure (including going away during
  // handshake).
  class COMPONENT_EXPORT(NETWORK_SERVICE) PendingConnection final {
   public:
    // |throttler| cannot be null.
    explicit PendingConnection(
        base::WeakPtr<WebSocketPerProcessThrottler> throttler);
    PendingConnection(PendingConnection&& other);

    PendingConnection(const PendingConnection&) = delete;
    PendingConnection& operator=(const PendingConnection&) = delete;

    ~PendingConnection();

    // Called when the hansdhake finishes sucessfully.
    void OnCompleteHandshake();

   private:
    base::WeakPtr<WebSocketPerProcessThrottler> throttler_;
  };

  WebSocketPerProcessThrottler();

  WebSocketPerProcessThrottler(const WebSocketPerProcessThrottler&) = delete;
  WebSocketPerProcessThrottler& operator=(const WebSocketPerProcessThrottler&) =
      delete;

  ~WebSocketPerProcessThrottler();

  // Returns if there are too many pending connections.
  bool HasTooManyPendingConnections() const {
    return num_pending_connections_ >= kMaxPendingWebSocketConnections;
  }

  // Returns the delay which should be used to throttle opening websocket
  // connections.
  base::TimeDelta CalculateDelay() const;

  // Issues an object which represents a pending connection.
  PendingConnection IssuePendingConnectionTracker();

  // Returns true if this throttler is clean, i.e., we can restore the internal
  // state by simply creating a new object.
  bool IsClean() const;

  // Copies the succeeded / failed counters for the current period to the
  // ones for the previous period, and zeroes them.
  void Roll();

  int64_t num_pending_connections() const { return num_pending_connections_; }
  int64_t num_current_succeeded_connections() const {
    return num_current_succeeded_connections_;
  }
  int64_t num_previous_succeeded_connections() const {
    return num_previous_succeeded_connections_;
  }
  int64_t num_current_failed_connections() const {
    return num_current_failed_connections_;
  }
  int64_t num_previous_failed_connections() const {
    return num_previous_failed_connections_;
  }

 private:
  // The current number of pending connections.
  int num_pending_connections_ = 0;

  // The number of handshakes that failed in the clurrent and previous time
  // period.
  int64_t num_current_succeeded_connections_ = 0;
  int64_t num_previous_succeeded_connections_ = 0;

  // The number of handshakes that succeeded in the current and previous time
  // period.
  int64_t num_current_failed_connections_ = 0;
  int64_t num_previous_failed_connections_ = 0;

  static constexpr int kMaxPendingWebSocketConnections = 255;

  base::WeakPtrFactory<WebSocketPerProcessThrottler> weak_factory_{this};
};

// This class is for throttling WebSocket connections. WebSocketThrottler is
// a set of per-renderer throttlers.
class COMPONENT_EXPORT(NETWORK_SERVICE) WebSocketThrottler final {
 public:
  using PendingConnection = WebSocketPerProcessThrottler::PendingConnection;

  WebSocketThrottler();

  WebSocketThrottler(const WebSocketThrottler&) = delete;
  WebSocketThrottler& operator=(const WebSocketThrottler&) = delete;

  ~WebSocketThrottler();

  // Returns true if there are too many pending connections for |process_id|.
  bool HasTooManyPendingConnections(int process_id) const;

  // Calculates connection delay for |process_id|.
  base::TimeDelta CalculateDelay(int process_id) const;

  // Returns a pending connection for |process_id|. This function can be called
  // only when |HasTooManyPendingConnections(process_id)| is false. May return
  // |std::nullopt| if |process_id| is not throttled.
  std::optional<PendingConnection> IssuePendingConnectionTracker(
      int process_id);

  size_t GetSizeForTesting() const { return per_process_throttlers_.size(); }

 private:
  void OnTimer();

  std::map<int, std::unique_ptr<WebSocketPerProcessThrottler>>
      per_process_throttlers_;
  base::RepeatingTimer throttling_period_timer_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_WEBSOCKET_THROTTLER_H_
