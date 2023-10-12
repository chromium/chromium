// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SERVER_WEB_SOCKET_ENCODER_H_
#define NET_SERVER_WEB_SOCKET_ENCODER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "net/server/web_socket.h"
#include "net/websockets/websocket_deflater.h"
#include "net/websockets/websocket_inflater.h"

namespace net {

class WebSocketDeflateParameters;

class WebSocketEncoder final {
 public:
  static const char kClientExtensions[];

  WebSocketEncoder(const WebSocketEncoder&) = delete;
  WebSocketEncoder& operator=(const WebSocketEncoder&) = delete;

  ~WebSocketEncoder();

  // Creates and returns an encoder for a server without extensions.
  static std::unique_ptr<WebSocketEncoder> CreateServer();
  // Creates and returns an encoder.
  // |extensions| is the value of a Sec-WebSocket-Extensions header.
  // Returns nullptr when there is an error.
  static std::unique_ptr<WebSocketEncoder> CreateServer(
      const std::string& extensions,
      WebSocketDeflateParameters* params);
  static std::unique_ptr<WebSocketEncoder> CreateClient(
      const std::string& response_extensions);

  WebSocket::ParseResult DecodeFrame(base::StringPiece frame,
                                     int* bytes_consumed,
                                     std::string* output);
  void EncodeTextFrame(base::StringPiece frame,
                       int masking_key,
                       std::string* output);
  void EncodePongFrame(base::StringPiece frame,
                       int masking_key,
                       std::string* output);
  void EncodeCloseFrame(base::StringPiece frame,
                        int masking_key,
                        std::string* output);

  bool deflate_enabled() const { return !!deflater_; }

 private:
  enum Type {
    FOR_SERVER,
    FOR_CLIENT,
  };

  WebSocketEncoder(Type type,
                   std::unique_ptr<WebSocketDeflater> deflater,
                   std::unique_ptr<WebSocketInflater> inflater);

  std::vector<std::string> continuation_message_frames_;
  bool is_current_message_compressed_ = false;

  bool Inflate(std::string* message);
  bool Deflate(base::StringPiece message, std::string* output);

  Type type_;
  std::unique_ptr<WebSocketDeflater> deflater_;
  std::unique_ptr<WebSocketInflater> inflater_;
};

}  // namespace net

#endif  // NET_SERVER_WEB_SOCKET_ENCODER_H_
