// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_WEBSOCKET_INTERCEPTOR_H_
#define SERVICES_NETWORK_WEBSOCKET_INTERCEPTOR_H_

#include <optional>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "services/network/throttling/scoped_throttling_token.h"
#include "services/network/throttling/throttling_network_interceptor.h"

namespace network {

// WebSocket interceptor is meant to abstract away the details on
// network emulation from the WebSockets code. At the same time it adapts the
// ThrottlingNetworkInterceptor, which was designed with HTTP in mind, to the
// specifics of WebSockets.
class COMPONENT_EXPORT(NETWORK_SERVICE) WebSocketInterceptor {
 public:
  enum FrameDirection {
    kIncoming,
    kOutgoing,
  };

  WebSocketInterceptor(
      uint32_t net_log_source_id,
      const std::optional<base::UnguessableToken>& throttling_profile_id);

  virtual ~WebSocketInterceptor();

  enum InterceptResult {
    kContinue,
    kShouldWait,
  };

  // This method is meant to be called before WebSocket starts processing each
  // incoming or outgoing frame. 'size' represents frame length in bytes.
  //
  // The return value has the following meaning:
  //   * kContinue: the interceptor does not want to delay frame processing,
  //     `retry_callback` is ignored.
  //   * kShouldWait: frame processing should be paused until `retry_callback`
  //     is invoked.
  //     Calling Intercept again before the callback runs is not allowed.
  InterceptResult Intercept(FrameDirection direction,
                            size_t size,
                            base::OnceClosure retry_callback);

 private:
  void ThrottleCallback(FrameDirection direction, int result, int64_t bytes);

  ThrottlingNetworkInterceptor::ThrottleCallback throttle_callbacks_[2];
  const uint32_t net_log_source_id_;
  const std::unique_ptr<ScopedThrottlingToken> throttling_token_;

  base::OnceClosure pending_callbacks_[2];
};

}  // namespace network

#endif  // SERVICES_NETWORK_WEBSOCKET_INTERCEPTOR_H_
