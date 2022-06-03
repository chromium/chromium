// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/websocket_interceptor.h"
#include "net/base/net_errors.h"
#include "services/network/throttling/throttling_controller.h"

namespace network {

WebSocketInterceptor::WebSocketInterceptor(
    uint32_t net_log_source_id,
    const absl::optional<base::UnguessableToken>& throttling_profile_id,
    FrameDirection direction)
    : throttle_callback_(
          base::BindRepeating(&WebSocketInterceptor::ThrottleCallback,
                              base::Unretained(this))),
      net_log_source_id_(net_log_source_id),
      direction_(direction),
      throttling_token_(
          network::ScopedThrottlingToken::MaybeCreate(net_log_source_id_,
                                                      throttling_profile_id)) {}

WebSocketInterceptor::~WebSocketInterceptor() {
  auto* throttling_interceptor =
      ThrottlingController::GetInterceptor(net_log_source_id_);
  // GetInterceptor might return a different instance than what was used to
  // register throttle_callback_ (e.g. when the emulated network conditions has
  // changed). Calling StopThrottle is nevertheless safe in this case and would
  // be no-op.
  if (throttling_interceptor)
    throttling_interceptor->StopThrottle(throttle_callback_);
}

WebSocketInterceptor::InterceptResult WebSocketInterceptor::Intercept(
    size_t size,
    base::OnceClosure retry_callback) {
  DCHECK(!pending_callback_);
  DCHECK(!frame_started_);

  auto* throttling_interceptor =
      ThrottlingController::GetInterceptor(net_log_source_id_);

  if (!throttling_interceptor)
    return kContinue;

  frame_started_ = true;
  throttling_interceptor->SetSuspendWhenOffline(true);
  int start_throttle_result = throttling_interceptor->StartThrottle(
      /*result=*/0, size, /*send_end=*/base::TimeTicks(), /*start=*/false,
      /*is_upload=*/direction_ == kOutgoing, throttle_callback_);
  if (start_throttle_result == net::ERR_IO_PENDING) {
    pending_callback_ = std::move(retry_callback);
    return kShouldWait;
  }
  return kContinue;
}

void WebSocketInterceptor::FinishFrame() {
  DCHECK(frame_started_);
  DCHECK(pending_callback_.is_null());
  frame_started_ = false;
}

void WebSocketInterceptor::ThrottleCallback(int result, int64_t bytes) {
  if (pending_callback_)
    std::move(pending_callback_).Run();
}

}  // namespace network
