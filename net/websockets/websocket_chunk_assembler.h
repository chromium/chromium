// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_CHUNK_ASSEMBLER_H_
#define NET_WEBSOCKETS_WEBSOCKET_CHUNK_ASSEMBLER_H_

#include <memory>
#include <vector>

#include "base/types/expected.h"
#include "net/base/net_errors.h"
#include "net/websockets/websocket_frame.h"

namespace net {

class NET_EXPORT WebSocketChunkAssembler final {
 public:
  WebSocketChunkAssembler();
  ~WebSocketChunkAssembler();

  // Resets the current state of the assembler.
  void Reset();

  // Processes a WebSocket frame chunk and assembles it into a complete frame.
  // Returns either the assembled frame or an error code.
  base::expected<std::unique_ptr<WebSocketFrame>, net::Error> HandleChunk(
      std::unique_ptr<WebSocketFrameChunk> chunk);

 private:
  // Enum representing the current state of the assembler.
  enum class AssemblyState {
    kMessageFinished,    // Message finished, ready for next frame.
    kInitialFrame,       // Processing the first frame.
    kContinuationFrame,  // Processing continuation frame.
    kControlFrame        // Processing control frame.
  };

  // Current state of the assembler
  AssemblyState state_ = AssemblyState::kMessageFinished;

  std::unique_ptr<WebSocketFrameHeader> current_frame_header_;
  std::vector<char> chunk_buffer_;
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_CHUNK_ASSEMBLER_H_
