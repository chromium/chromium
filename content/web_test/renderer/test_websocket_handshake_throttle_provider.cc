// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/renderer/test_websocket_handshake_throttle_provider.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/web_url.h"
#include "url/gurl.h"

namespace content {

namespace {

using CompletionCallback = blink::WebSocketHandshakeThrottle::OnCompletion;

// Checks for a valid "content-shell-websocket-delay-ms" parameter and returns
// it as a TimeDelta if it exists. Otherwise returns a zero TimeDelta.
base::TimeDelta ExtractDelayFromUrl(const GURL& url) {
  if (!url.has_query())
    return base::TimeDelta();
  url::Component query = url.parsed_for_possibly_invalid_spec().query;
  url::Component key;
  url::Component value;
  std::string_view spec = url.possibly_invalid_spec();
  while (url::ExtractQueryKeyValue(spec, &query, &key, &value)) {
    std::string_view key_piece = spec.substr(key.begin, key.len);
    if (key_piece != "content-shell-websocket-delay-ms")
      continue;

    std::string_view value_piece = spec.substr(value.begin, value.len);
    int value_int;
    if (!base::StringToInt(value_piece, &value_int) || value_int < 0)
      return base::TimeDelta();

    return base::Milliseconds(value_int);
  }

  // Parameter was not found.
  return base::TimeDelta();
}

// A simple WebSocketHandshakeThrottle that calls callbacks->IsSuccess() after n
// milli-seconds if the URL query contains
// content-shell-websocket-delay-ms=n. Otherwise it calls IsSuccess()
// immediately.
class TestWebSocketHandshakeThrottle
    : public blink::WebSocketHandshakeThrottle {
 public:
  explicit TestWebSocketHandshakeThrottle(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    timer_.SetTaskRunner(std::move(task_runner));
  }

  ~TestWebSocketHandshakeThrottle() override = default;

  void ThrottleHandshake(const blink::WebURL& url,
                         const blink::WebSecurityOrigin& creator_origin,
                         const blink::WebSecurityOrigin& isolated_world_origin,
                         CompletionCallback completion_callback) override {
    DCHECK(completion_callback);

    auto wrapper = base::BindOnce(
        [](CompletionCallback callback) {
          std::move(callback).Run(std::nullopt);
        },
        std::move(completion_callback));

    timer_.Start(FROM_HERE, ExtractDelayFromUrl(url), std::move(wrapper));
  }

 private:
  base::OneShotTimer timer_;
};

}  // namespace

std::unique_ptr<blink::WebSocketHandshakeThrottleProvider>
TestWebSocketHandshakeThrottleProvider::Clone(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return std::make_unique<TestWebSocketHandshakeThrottleProvider>();
}

std::unique_ptr<blink::WebSocketHandshakeThrottle>
TestWebSocketHandshakeThrottleProvider::CreateThrottle(
    base::optional_ref<const blink::LocalFrameToken> local_frame_token,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return std::make_unique<TestWebSocketHandshakeThrottle>(
      std::move(task_runner));
}

}  // namespace content
