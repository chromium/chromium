// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_errors.h"


namespace net {

Error WebSocketErrorToNetError(WebSocketError error) {
  switch (error) {
    case kWebSocketNormalClosure:
      return OK;

    case kWebSocketErrorGoingAway:  // TODO(ricea): More specific code?
    case kWebSocketErrorProtocolError:
    case kWebSocketErrorUnsupportedData:
    case kWebSocketErrorInvalidFramePayloadData:
    case kWebSocketErrorPolicyViolation:
    case kWebSocketErrorMandatoryExtension:
    case kWebSocketErrorInternalServerError:
      return ERR_WS_PROTOCOL_ERROR;

    case kWebSocketErrorNoStatusReceived:
    case kWebSocketErrorAbnormalClosure:
      return ERR_CONNECTION_CLOSED;

    case kWebSocketErrorTlsHandshake:
      // This error will probably be reported with more detail at a lower layer;
      // this is the best we can do at this layer.
      return ERR_SSL_PROTOCOL_ERROR;

    case kWebSocketErrorMessageTooBig:
      return ERR_MSG_TOO_BIG;

    default:
      return ERR_UNEXPECTED;
  }
}

}  // namespace net
