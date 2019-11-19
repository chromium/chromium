// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/websockets/mock_websocket_channel.h"

// Generated constructors and destructors for GMock objects are very large. By
// putting them in a separate file we can speed up compile times.

namespace blink {

MockWebSocketChannel* MockWebSocketChannel::Create() {
  return MakeGarbageCollected<testing::StrictMock<MockWebSocketChannel>>();
}

MockWebSocketChannel::MockWebSocketChannel() = default;
MockWebSocketChannel::~MockWebSocketChannel() = default;

}  // namespace blink
