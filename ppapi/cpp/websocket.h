// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_WEBSOCKET_H_
#define PPAPI_CPP_WEBSOCKET_H_

#include <stdint.h>

#include "ppapi/c/ppb_websocket.h"
#include "ppapi/cpp/resource.h"

/// @file
/// This file defines the WebSocket interface providing bi-directional,
/// full-duplex, communications over a single TCP socket.

// Windows headers will redefine SendMessage.
#ifdef SendMessage
#undef SendMessage
#endif

namespace pp {

class CompletionCallback;
class InstanceHandle;
class Var;

/// The <code>WebSocket</code> class providing bi-directional,
/// full-duplex, communications over a single TCP socket.
class WebSocket : public Resource {
 public:
  /// Constructs a WebSocket object.
  ///
  /// @param[in] instance The instance with which this resource will be
  /// associated.
  explicit WebSocket(const InstanceHandle& instance);

  /// Destructs a WebSocket object.
  virtual ~WebSocket();

  /// Connect() connects to the specified WebSocket server. You can call this
  /// function once for an object.
  ///
  /// @param[in] url A <code>Var</code> of string type representing a WebSocket
  /// server URL.
  ///
  /// @param[in] protocols A pointer to an array of <code>Var</code> of string
  /// type specifying sub-protocols. Each <code>Var</code> represents one
  /// sub-protocol. This argument can be null only if
  /// <code>protocol_count</code> is 0.
  ///
  /// @param[in] protocol_count The number of sub-protocols in
  /// <code>protocols</code>.
  ///
  /// @param[in] callback A <code>CompletionCallback</code> called
  /// when a connection is established or an error occurs in establishing
  /// connection.
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  /// Returns <code>PP_ERROR_BADARGUMENT</code> if specified <code>url</code>,
  /// or <code>protocols</code> contains invalid string as defined in
  /// the WebSocket API specification. <code>PP_ERROR_BADARGUMENT</code>
  /// corresponds to a SyntaxError in the WebSocket API specification.
  /// Returns <code>PP_ERROR_NOACCESS</code> if the protocol specified in the
  /// <code>url</code> is not a secure protocol, but the origin of the caller
  /// has a secure scheme. Also returns <code>PP_ERROR_NOACCESS</code> if the
  /// port specified in the <code>url</code> is a port that the user agent is
  /// configured to block access to because it is a well-known port like SMTP.
  /// <code>PP_ERROR_NOACCESS</code> corresponds to a SecurityError of the
  /// specification.
  /// Returns <code>PP_ERROR_INPROGRESS</code> if this is not the first call to
  /// Connect().
  int32_t Connect(const Var& url, const Var protocols[],
                  uint32_t protocol_count, const CompletionCallback& callback);

  /// Close() closes the specified WebSocket connection by specifying
  /// <code>code</code> and <code>reason</code>.
  ///
  /// @param[in] code The WebSocket close code. This is ignored if it is 0.
  /// <code>PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE</code> must be used for the
  /// usual case. To indicate some specific error cases, codes in the range
  /// <code>PP_WEBSOCKETSTATUSCODE_USER_REGISTERED_MIN</code> to
  /// <code>PP_WEBSOCKETSTATUSCODE_USER_REGISTERED_MAX</code>, and in the range
  /// <code>PP_WEBSOCKETSTATUSCODE_USER_PRIVATE_MIN</code> to
  /// <code>PP_WEBSOCKETSTATUSCODE_USER_PRIVATE_MAX</code> are available.
  ///
  /// @param[in] reason A <code>Var</code> of string type representing the
  /// close reason. This is ignored if it is an undefined type.
  ///
  /// @param[in] callback A <code>CompletionCallback</code> called when the
  /// connection is closed or an error occurs in closing the connection.
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  /// Returns <code>PP_ERROR_BADARGUMENT</code> if <code>reason</code> contains
  /// an invalid character as a UTF-8 string, or is longer than 123 bytes.
  /// <code>PP_ERROR_BADARGUMENT</code> corresponds to a JavaScript
  /// SyntaxError in the WebSocket API specification.
  /// Returns <code>PP_ERROR_NOACCESS</code> if the code is not an integer
  /// equal to 1000 or in the range 3000 to 4999.
  /// <code>PP_ERROR_NOACCESS</code> corresponds to an InvalidAccessError in
  /// the WebSocket API specification. Returns <code>PP_ERROR_INPROGRESS</code>
  /// if a previous call to Close() is not finished.
  int32_t Close(uint16_t code, const Var& reason,
                const CompletionCallback& callback);

  /// ReceiveMessage() receives a message from the WebSocket server.
  /// This interface only returns a single message. That is, this interface
  /// must be called at least N times to receive N messages, no matter the size
  /// of each message.
  ///
  /// @param[out] message The received message is copied to provided
  /// <code>message</code>. The <code>message</code> must remain valid until
  /// ReceiveMessage() completes. Its received <code>Var</code> will be of
  /// string or ArrayBuffer type.
  ///
  /// @param[in] callback A <code>CompletionCallback</code> called when
  /// ReceiveMessage() completes. This callback is ignored if ReceiveMessage()
  /// completes synchronously and returns <code>PP_OK</code>.
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  /// If an error is detected or connection is closed, ReceiveMessage() returns
  /// <code>PP_ERROR_FAILED</code> after all buffered messages are received.
  /// Until buffered message become empty, ReceiveMessage() continues to return
  /// <code>PP_OK</code> as if connection is still established without errors.
  int32_t ReceiveMessage(Var* message,
                         const CompletionCallback& callback);

  /// SendMessage() sends a message to the WebSocket server.
  ///
  /// @param[in] message A message to send. The message is copied to an internal
  /// buffer, so the caller can free <code>message</code> safely after returning
  /// from the function. This <code>Var</code> must be of string or
  /// ArrayBuffer types.
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  /// Returns <code>PP_ERROR_FAILED</code> if the ReadyState is
  /// <code>PP_WEBSOCKETREADYSTATE_CONNECTING</code>.
  /// <code>PP_ERROR_FAILED</code> corresponds to a JavaScript
  /// InvalidStateError in the WebSocket API specification.
  /// Returns <code>PP_ERROR_BADARGUMENT</code> if the provided
  /// <code>message</code> contains an invalid character as a
  /// UTF-8 string. <code>PP_ERROR_BADARGUMENT</code> corresponds to a
  /// JavaScript SyntaxError in the WebSocket API specification.
  /// Otherwise, returns <code>PP_OK</code>, but it doesn't necessarily mean
  /// that the server received the message.
  int32_t SendMessage(const Var& message);

  /// GetBufferedAmount() returns the number of bytes of text and binary
  /// messages that have been queued for the WebSocket connection to send, but
  /// have not been transmitted to the network yet.
  ///
  /// @return Returns the number of bytes.
  uint64_t GetBufferedAmount();

  /// GetCloseCode() returns the connection close code for the WebSocket
  /// connection.
  ///
  /// @return Returns 0 if called before the close code is set.
  uint16_t GetCloseCode();

  /// GetCloseReason() returns the connection close reason for the WebSocket
  /// connection.
  ///
  /// @return Returns a <code>Var</code> of string type. If called before the
  /// close reason is set, the return value contains an empty string. Returns a
  /// <code>PP_VARTYPE_UNDEFINED</code> if called on an invalid resource.
  Var GetCloseReason();

  /// GetCloseWasClean() returns if the connection was closed cleanly for the
  /// specified WebSocket connection.
  ///
  /// @return Returns <code>false</code> if called before the connection is
  /// closed, called on an invalid resource, or closed for abnormal reasons.
  /// Otherwise, returns <code>true</code> if the connection was closed
  /// cleanly.
  bool GetCloseWasClean();

  /// GetExtensions() returns the extensions selected by the server for the
  /// specified WebSocket connection.
  ///
  /// @return Returns a <code>Var</code> of string type. If called before the
  /// connection is established, the <code>Var</code>'s data is an empty
  /// string. Returns a <code>PP_VARTYPE_UNDEFINED</code> if called on an
  /// invalid resource. Currently the <code>Var</code>'s data for valid
  /// resources are always an empty string.
  Var GetExtensions();

  /// GetProtocol() returns the sub-protocol chosen by the server for the
  /// specified WebSocket connection.
  ///
  /// @return Returns a <code>Var</code> of string type. If called before the
  /// connection is established, the <code>Var</code> contains the empty
  /// string. Returns a code>PP_VARTYPE_UNDEFINED</code> if called on an
  /// invalid resource.
  Var GetProtocol();

  /// GetReadyState() returns the ready state of the specified WebSocket
  /// connection.
  ///
  /// @return Returns <code>PP_WEBSOCKETREADYSTATE_INVALID</code> if called
  /// before Connect() is called, or if this function is called on an
  /// invalid resource.
  PP_WebSocketReadyState GetReadyState();

  /// GetURL() returns the URL associated with specified WebSocket connection.
  ///
  /// @return Returns a <code>Var</code> of string type. If called before the
  /// connection is established, the <code>Var</code> contains the empty
  /// string. Returns a <code>PP_VARTYPE_UNDEFINED</code> if this function
  /// is called on an invalid resource.
  Var GetURL();
};

}  // namespace pp

#endif  // PPAPI_CPP_WEBSOCKET_H_
