// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SERVER_WEB_SOCKET_PARSE_RESULT_H_
#define NET_SERVER_WEB_SOCKET_PARSE_RESULT_H_

#include "net/base/net_export.h"

namespace net {

enum class WebSocketParseResult {
  // Final frame of a text message or compressed frame.
  FRAME_OK_FINAL,
  // Other frame of a text message.
  FRAME_OK_MIDDLE,
  FRAME_PING,
  FRAME_PONG,
  FRAME_INCOMPLETE,
  FRAME_CLOSE,
  FRAME_ERROR
};

}  // namespace net

#endif  // NET_SERVER_WEB_SOCKET_PARSE_RESULT_H_
