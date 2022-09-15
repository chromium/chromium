// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_UTILITY_WEBSOCKET_WEBSOCKET_API_H_
#define PPAPI_UTILITY_WEBSOCKET_WEBSOCKET_API_H_

#include <stdint.h>

#include "ppapi/c/ppb_websocket.h"

/// @file
/// This file defines the WebSocketAPI interface.

namespace pp {

class Instance;
class Var;

/// The <code>WebSocketAPI</code> class
class WebSocketAPI {
 public:
  /// Constructs a WebSocketAPI object.
  explicit WebSocketAPI(Instance* instance);

  /// Destructs a WebSocketAPI object.
  virtual ~WebSocketAPI();

  /// Connect() connects to the specified WebSocket server. Caller can call
  /// this method at most once.
  ///
  /// @param[in] url A <code>Var</code> of string type representing a WebSocket
  /// server URL.
  /// @param[in] protocols A pointer to an array of string type
  /// <code>Var</code> specifying sub-protocols. Each <code>Var</code>
  /// represents one sub-protocol and its <code>PP_VarType</code> must be
  /// <code>PP_VARTYPE_STRING</code>. This argument can be null only if
  /// <code>protocol_count</code> is 0.
  /// @param[in] protocol_count The number of sub-protocols in
  /// <code>protocols</code>.
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  /// See also <code>pp::WebSocket::Connect</code>.
  int32_t Connect(const Var& url, const Var protocols[],
                  uint32_t protocol_count);

  /// Close() closes the specified WebSocket connection by specifying
  /// <code>code</code> and <code>reason</code>.
  ///
  /// @param[in] code The WebSocket close code. Ignored if it is 0.
  /// @param[in] reason A <code>Var</code> of string type which represents the
  /// WebSocket close reason. Ignored if it is undefined type.
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  /// See also <code>pp::WebSocket::Close</code>.
  int32_t Close(uint16_t code, const Var& reason);

  /// Send() sends a message to the WebSocket server.
  ///
  /// @param[in] data A message to send. The message is copied to internal
  /// buffer. So caller can free <code>data</code> safely after returning
  /// from the function.
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  /// See also <code>pp::WebSocket::SendMessage</code>.
  int32_t Send(const Var& data);

  /// GetBufferedAmount() returns the number of bytes of text and binary
  /// messages that have been queued for the WebSocket connection to send but
  /// have not been transmitted to the network yet.
  ///
  /// @return Returns the number of bytes.
  uint64_t GetBufferedAmount();

  /// GetExtensions() returns the extensions selected by the server for the
  /// specified WebSocket connection.
  ///
  /// @return Returns a <code>Var</code> of string type. If called before the
  /// connection is established, its data is empty string.
  /// Currently its data is always an empty string.
  Var GetExtensions();

  /// GetProtocol() returns the sub-protocol chosen by the server for the
  /// specified WebSocket connection.
  ///
  /// @return Returns a <code>Var</code> of string type. If called before the
  /// connection is established, it contains the empty string.
  Var GetProtocol();

  /// GetReadyState() returns the ready state of the specified WebSocket
  /// connection.
  ///
  /// @return Returns <code>PP_WEBSOCKETREADYSTATE_INVALID</code> if called
  /// before connect() is called.
  PP_WebSocketReadyState GetReadyState();

  /// GetURL() returns the URL associated with specified WebSocket connection.
  ///
  /// @return Returns a <code>Var</code> of string type. If called before the
  /// connection is established, it contains the empty string.
  Var GetURL();

  /// WebSocketDidOpen() is invoked when the connection is established by
  /// Connect().
  virtual void WebSocketDidOpen() = 0;

  /// WebSocketDidClose() is invoked when the connection is closed by errors or
  /// Close().
  virtual void WebSocketDidClose(bool wasClean,
                                 uint16_t code,
                                 const Var& reason) = 0;

  /// HandleWebSocketMessage() is invoked when a message is received.
  virtual void HandleWebSocketMessage(const Var& message) = 0;

  /// HandleWebSocketError() is invoked if the user agent was required to fail
  /// the WebSocket connection or the WebSocket connection is closed with
  /// prejudice. DidClose() always follows HandleError().
  virtual void HandleWebSocketError() = 0;

 private:
  class Implement;
  Implement* impl_;
};

}  // namespace pp

#endif  // PPAPI_UTILITY_WEBSOCKET_WEBSOCKET_API_H_
