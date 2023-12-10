// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_TEST_WEBSOCKET_HANDSHAKE_THROTTLE_PROVIDER_H_
#define CONTENT_WEB_TEST_RENDERER_TEST_WEBSOCKET_HANDSHAKE_THROTTLE_PROVIDER_H_

#include <memory>
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle_provider.h"

namespace content {

class TestWebSocketHandshakeThrottleProvider
    : public blink::WebSocketHandshakeThrottleProvider {
 public:
  TestWebSocketHandshakeThrottleProvider() = default;

  TestWebSocketHandshakeThrottleProvider(
      const TestWebSocketHandshakeThrottleProvider&) = delete;
  TestWebSocketHandshakeThrottleProvider& operator=(
      const TestWebSocketHandshakeThrottleProvider&) = delete;

  ~TestWebSocketHandshakeThrottleProvider() override = default;

  std::unique_ptr<blink::WebSocketHandshakeThrottleProvider> Clone(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  std::unique_ptr<blink::WebSocketHandshakeThrottle> CreateThrottle(
      base::optional_ref<const blink::LocalFrameToken> local_frame_token,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_TEST_WEBSOCKET_HANDSHAKE_THROTTLE_PROVIDER_H_
