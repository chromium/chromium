// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_ERRORS_H_
#define NET_WEBSOCKETS_WEBSOCKET_ERRORS_H_

#include "net/base/net_errors.h"
#include "net/base/net_export.h"

namespace net {

// Reason codes used with close messages. NoStatusReceived,
// AbnormalClosure and TlsHandshake are special in that they
// should never be sent on the wire; they are only used within the
// implementation.
enum WebSocketError {
  // Status codes in the range 0 to 999 are not used.

  // The following are defined by RFC6455.
  kWebSocketNormalClosure = 1000,
  kWebSocketErrorGoingAway = 1001,
  kWebSocketErrorProtocolError = 1002,
  kWebSocketErrorUnsupportedData = 1003,
  kWebSocketErrorNoStatusReceived = 1005,
  kWebSocketErrorAbnormalClosure = 1006,
  kWebSocketErrorInvalidFramePayloadData = 1007,
  kWebSocketErrorPolicyViolation = 1008,
  kWebSocketErrorMessageTooBig = 1009,
  kWebSocketErrorMandatoryExtension = 1010,
  kWebSocketErrorInternalServerError = 1011,
  kWebSocketErrorTlsHandshake = 1015,

  // The range 1000-2999 is reserved by RFC6455 for use by the WebSocket
  // protocol and public extensions.
  kWebSocketErrorProtocolReservedMax = 2999,

  // The range 3000-3999 is reserved by RFC6455 for registered use by libraries,
  // frameworks and applications.
  kWebSocketErrorRegisteredReservedMin = 3000,
  kWebSocketErrorRegisteredReservedMax = 3999,

  // The range 4000-4999 is reserved by RFC6455 for private use by prior
  // agreement of the endpoints.
  kWebSocketErrorPrivateReservedMin = 4000,
  kWebSocketErrorPrivateReservedMax = 4999,
};

// Convert WebSocketError to net::Error defined in net/base/net_errors.h.
NET_EXPORT_PRIVATE Error WebSocketErrorToNetError(WebSocketError error);

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_ERRORS_H_
