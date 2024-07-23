// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_FRAME_PARSER_H_
#define NET_WEBSOCKETS_WEBSOCKET_FRAME_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "net/base/net_export.h"
#include "net/websockets/websocket_errors.h"
#include "net/websockets/websocket_frame.h"

namespace net {
struct WebSocketFrameChunk;
struct WebSocketFrameHeader;

// Parses WebSocket frames from byte stream.
//
// Specification of WebSocket frame format is available at
// <http://tools.ietf.org/html/rfc6455#section-5>.
// This class does *NOT* unmask frame payload.
class NET_EXPORT WebSocketFrameParser {
 public:
  WebSocketFrameParser();

  WebSocketFrameParser(const WebSocketFrameParser&) = delete;
  WebSocketFrameParser& operator=(const WebSocketFrameParser&) = delete;

  ~WebSocketFrameParser();

  // Decodes the given byte stream and stores parsed WebSocket frames in
  // |frame_chunks|.
  // Each WebSocketFrameChunk's payload is a subspan of data_span.
  // Thus callers must take care of its lifecycle.
  //
  // If the parser encounters invalid payload length format, Decode() fails
  // and returns false. Once Decode() has failed, the parser refuses to decode
  // any more data and future invocations of Decode() will simply return false.
  //
  // Payload data of parsed WebSocket frames may be incomplete; see comments in
  // websocket_frame.h for more details.
  bool Decode(base::span<const uint8_t> data_span,
              std::vector<std::unique_ptr<WebSocketFrameChunk>>* frame_chunks);

  // Returns kWebSocketNormalClosure if the parser has not failed to decode
  // WebSocket frames. Otherwise returns WebSocketError which is defined in
  // websocket_errors.h. We can convert net::WebSocketError to net::Error by
  // using WebSocketErrorToNetError().
  WebSocketError websocket_error() const { return websocket_error_; }

 private:
  // Tries to decode a frame header from |data|.
  // If successful, this function updates
  // |current_frame_header_|, and |masking_key_| (if available) and returns
  // the number of consumed bytes in |data|.
  // If there is not enough data in the remaining buffer to parse a frame
  // header, this function returns 0 without doing anything.
  // This function may update |websocket_error_| if it observes a corrupt frame.
  size_t DecodeFrameHeader(base::span<const uint8_t> data);

  // Decodes frame payload and creates a WebSocketFrameChunk object.
  // This function updates |frame_offset_| after
  // parsing. This function returns a frame object even if no payload data is
  // available at this moment, so the receiver could make use of frame header
  // information. If the end of frame is reached, this function clears
  // |current_frame_header_|, |frame_offset_| and |masking_key_|.
  std::unique_ptr<WebSocketFrameChunk> DecodeFramePayload(
      bool first_chunk,
      base::span<const uint8_t>* data);

  // Internal buffer to store the data to parse header.
  std::vector<uint8_t> incomplete_header_buffer_;

  // Frame header and masking key of the current frame.
  // |masking_key_| is filled with zeros if the current frame is not masked.
  std::unique_ptr<WebSocketFrameHeader> current_frame_header_;

  // Amount of payload data read so far for the current frame.
  uint64_t frame_offset_ = 0;

  WebSocketError websocket_error_ = kWebSocketNormalClosure;
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_FRAME_PARSER_H_
