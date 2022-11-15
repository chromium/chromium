// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_WEBSOCKET_API_H_
#define PPAPI_THUNK_PPB_WEBSOCKET_API_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/ppb_websocket.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

// Some arguments and attributes are based on The WebSocket Protocol and The
// WebSocket API. See also following official specifications.
//  - The WebSocket Protocol http://tools.ietf.org/html/rfc6455
//  - The WebSocket API      http://dev.w3.org/html5/websockets/
class PPAPI_THUNK_EXPORT PPB_WebSocket_API {
 public:
  virtual ~PPB_WebSocket_API() {}

  // Connects to the specified WebSocket server with |protocols| argument
  // defined by the WebSocket API. Returns an int32_t error code from
  // pp_errors.h.
  virtual int32_t Connect(const PP_Var& url,
                          const PP_Var protocols[],
                          uint32_t protocol_count,
                          scoped_refptr<TrackedCallback> callback) = 0;

  // Closes the established connection with specified |code| and |reason|.
  // Returns an int32_t error code from pp_errors.h.
  virtual int32_t Close(uint16_t code,
                        const PP_Var& reason,
                        scoped_refptr<TrackedCallback> callback) = 0;

  // Receives a message from the WebSocket server. Caller must keep specified
  // |message| object as valid until completion callback is invoked. Returns an
  // int32_t error code from pp_errors.h.
  virtual int32_t ReceiveMessage(PP_Var* message,
                                 scoped_refptr<TrackedCallback> callback) = 0;

  // Sends a message to the WebSocket server. Returns an int32_t error code
  // from pp_errors.h.
  virtual int32_t SendMessage(const PP_Var& message) = 0;

  // Returns the bufferedAmount attribute of The WebSocket API.
  virtual uint64_t GetBufferedAmount() = 0;

  // Returns the CloseEvent code attribute of The WebSocket API. Returned code
  // is valid if the connection is already closed and wasClean attribute is
  // true.
  virtual uint16_t GetCloseCode() = 0;

  // Returns the CloseEvent reason attribute of The WebSocket API. Returned
  // code is valid if the connection is already closed and wasClean attribute
  // is true.
  virtual PP_Var GetCloseReason() = 0;

  // Returns the CloseEvent wasClean attribute of The WebSocket API. Returned
  // code is valid if the connection is already closed.
  virtual PP_Bool GetCloseWasClean() = 0;

  // Returns the extensions attribute of The WebSocket API.
  virtual PP_Var GetExtensions() = 0;

  // Returns the protocol attribute of The WebSocket API.
  virtual PP_Var GetProtocol() = 0;

  // Returns the readState attribute of The WebSocket API.
  virtual PP_WebSocketReadyState GetReadyState() = 0;

  // Returns the url attribute of The WebSocket API.
  virtual PP_Var GetURL() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_WEBSOCKET_API_H_
