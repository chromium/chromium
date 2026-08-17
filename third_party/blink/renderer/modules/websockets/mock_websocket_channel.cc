// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/websockets/mock_websocket_channel.h"

// Generated constructors and destructors for GMock objects are very large. By
// putting them in a separate file we can speed up compile times.

namespace blink {

using ::testing::_;

MockWebSocketChannel::MockWebSocketChannel() {
  ON_CALL(*this, Connect(_, _, _))
      .WillByDefault([this](const KURL& url, const String& protocol,
                            network::mojom::blink::IPAddressSpace) {
        return Connect(url, protocol);
      });
  ON_CALL(*this, Connect(_, _)).WillByDefault(testing::Return(true));
}
MockWebSocketChannel::~MockWebSocketChannel() = default;

}  // namespace blink
