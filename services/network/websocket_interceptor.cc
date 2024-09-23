// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/network/websocket_interceptor.h"
#include "base/functional/bind.h"
#include "net/base/net_errors.h"
#include "services/network/throttling/throttling_controller.h"
#include "services/network/throttling/throttling_network_interceptor.h"

namespace network {

WebSocketInterceptor::WebSocketInterceptor(
    uint32_t net_log_source_id,
    const std::optional<base::UnguessableToken>& throttling_profile_id)
    : net_log_source_id_(net_log_source_id),
      throttling_token_(
          network::ScopedThrottlingToken::MaybeCreate(net_log_source_id_,
                                                      throttling_profile_id)) {
  throttle_callbacks_[kIncoming] =
      base::BindRepeating(&WebSocketInterceptor::ThrottleCallback,
                          base::Unretained(this), kIncoming);
  throttle_callbacks_[kOutgoing] =
      base::BindRepeating(&WebSocketInterceptor::ThrottleCallback,
                          base::Unretained(this), kOutgoing);
}

WebSocketInterceptor::~WebSocketInterceptor() {
  auto* throttling_interceptor =
      ThrottlingController::GetInterceptor(net_log_source_id_);
  if (throttling_interceptor) {
    throttling_interceptor->StopThrottle(throttle_callbacks_[kIncoming]);
    throttling_interceptor->StopThrottle(throttle_callbacks_[kOutgoing]);
  }
}

WebSocketInterceptor::InterceptResult WebSocketInterceptor::Intercept(
    FrameDirection direction,
    size_t size,
    base::OnceClosure retry_callback) {
  DCHECK(!pending_callbacks_[direction]);

  auto* throttling_interceptor =
      ThrottlingController::GetInterceptor(net_log_source_id_);

  if (!throttling_interceptor)
    return kContinue;

  throttling_interceptor->SetSuspendWhenOffline(true);
  int start_throttle_result = throttling_interceptor->StartThrottle(
      /*result=*/0, size, /*send_end=*/base::TimeTicks(), /*start=*/false,
      /*is_upload=*/direction == kOutgoing, throttle_callbacks_[direction]);
  if (start_throttle_result == net::ERR_IO_PENDING) {
    pending_callbacks_[direction] = std::move(retry_callback);
    return kShouldWait;
  }
  return kContinue;
}

void WebSocketInterceptor::ThrottleCallback(FrameDirection direction,
                                            int result,
                                            int64_t bytes) {
  if (pending_callbacks_[direction])
    std::move(pending_callbacks_[direction]).Run();
}

}  // namespace network
