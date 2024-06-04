// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_STREAM_BASE_H_
#define NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_STREAM_BASE_H_

// This file is included from net/http files.
// Since net/http can be built without linking net/websockets code,
// this file must not introduce any link-time dependencies on websockets.

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "net/base/net_export.h"
#include "net/http/http_raw_request_headers.h"
#include "net/http/http_stream.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/websockets/websocket_deflate_parameters.h"
#include "net/websockets/websocket_stream.h"

namespace net {

class ClientSocketHandle;
class SpdySession;
class HttpRequestHeaders;
class HttpResponseHeaders;
class WebSocketEndpointLockManager;
class WebSocketStream;

// WebSocketHandshakeStreamBase is the base class of
// WebSocketBasicHandshakeStream.  net/http code uses this interface to handle
// WebSocketBasicHandshakeStream when it needs to be treated differently from
// HttpStreamBase.
class NET_EXPORT WebSocketHandshakeStreamBase : public HttpStream {
 public:
  // These entries must match histogram Net.WebSocket.HandshakeResult2.
  // Do not change or reuse values.
  enum class HandshakeResult {
    // Handshake not completed via Upgrade over HTTP/1 connection.
    INCOMPLETE = 0,
    // Server responded to Upgrade request with invalid status.
    INVALID_STATUS = 1,
    // Server responded to Upgrade request with empty response.
    EMPTY_RESPONSE = 2,
    // Server responded to Upgrade request with 101 status but there was some
    // other network error.
    FAILED_SWITCHING_PROTOCOLS = 3,
    // Server responded to Upgrade request with invalid Upgrade header.
    FAILED_UPGRADE = 4,
    // Server responded to Upgrade request with invalid Sec-WebSocket-Accept
    // header.
    FAILED_ACCEPT = 5,
    // Server responded to Upgrade request with invalid Connection header.
    FAILED_CONNECTION = 6,
    // Server responded to Upgrade request with invalid Sec-WebSocket-Protocol
    // header.
    FAILED_SUBPROTO = 7,
    // Server responded to Upgrade request with invalid Sec-WebSocket-Extensions
    // header.
    FAILED_EXTENSIONS = 8,
    // Upgrade request failed due to other network error.
    FAILED = 9,
    // Connected via Upgrade over HTTP/1 connection.
    CONNECTED = 10,
    // Handshake not completed over an HTTP/2 connection.
    HTTP2_INCOMPLETE = 11,
    // Server responded to WebSocket request over an HTTP/2 connection with
    // invalid status code.
    HTTP2_INVALID_STATUS = 12,
    // Server responded to WebSocket request over an HTTP/2 connection with
    // invalid sec-websocket-protocol header.
    HTTP2_FAILED_SUBPROTO = 13,
    // Server responded to WebSocket request over an HTTP/2 connection with
    // invalid sec-websocket-extensions header.
    HTTP2_FAILED_EXTENSIONS = 14,
    // WebSocket request over an HTTP/2 connection failed with some other error.
    HTTP2_FAILED = 15,
    // Connected over an HTTP/2 connection.
    HTTP2_CONNECTED = 16,
    // Handshake not completed over an HTTP/3 connection.
    HTTP3_INCOMPLETE = 17,
    // Server responded to WebSocket request over an HTTP/3 connection with
    // invalid status code.
    HTTP3_INVALID_STATUS = 18,
    // Server responded to WebSocket request over an HTTP/3 connection with
    // invalid sec-websocket-protocol header.
    HTTP3_FAILED_SUBPROTO = 19,
    // Server responded to WebSocket request over an HTTP/3 connection with
    // invalid sec-websocket-extensions header.
    HTTP3_FAILED_EXTENSIONS = 20,
    // WebSocket request over an HTTP/3 connection failed with some other error.
    HTTP3_FAILED = 21,
    // Connected over an HTTP/3 connection.
    HTTP3_CONNECTED = 22,
    NUM_HANDSHAKE_RESULT_TYPES = 23
  };

  WebSocketHandshakeStreamBase() = default;

  WebSocketHandshakeStreamBase(const WebSocketHandshakeStreamBase&) = delete;
  WebSocketHandshakeStreamBase& operator=(const WebSocketHandshakeStreamBase&) =
      delete;

  ~WebSocketHandshakeStreamBase() override = default;

  // An object that stores data needed for the creation of a
  // WebSocketBasicHandshakeStream object. A new CreateHelper is used for each
  // WebSocket connection.
  class NET_EXPORT_PRIVATE CreateHelper : public base::SupportsUserData::Data {
   public:
    ~CreateHelper() override = default;

    // Create a WebSocketBasicHandshakeStream. This is called after the
    // underlying connection has been established but before any handshake data
    // has been transferred. This can be called more than once in the case that
    // HTTP authentication is needed.
    virtual std::unique_ptr<WebSocketHandshakeStreamBase> CreateBasicStream(
        std::unique_ptr<ClientSocketHandle> connection,
        bool using_proxy,
        WebSocketEndpointLockManager* websocket_endpoint_lock_manager) = 0;

    // Create a WebSocketHttp2HandshakeStream. This is called after the
    // underlying HTTP/2 connection has been established but before the stream
    // has been opened.  This cannot be called more than once.
    virtual std::unique_ptr<WebSocketHandshakeStreamBase> CreateHttp2Stream(
        base::WeakPtr<SpdySession> session,
        std::set<std::string> dns_aliases) = 0;

    // Create a WebSocketHttp3HandshakeStream. This is called after the
    // underlying HTTP/3 connection has been established but before the stream
    // has been opened.  This cannot be called more than once.
    virtual std::unique_ptr<WebSocketHandshakeStreamBase> CreateHttp3Stream(
        std::unique_ptr<QuicChromiumClientSession::Handle> session,
        std::set<std::string> dns_aliases) = 0;
  };

  // After the handshake has completed, this method creates a WebSocketStream
  // (of the appropriate type) from the WebSocketHandshakeStreamBase object.
  // The WebSocketHandshakeStreamBase object is unusable after Upgrade() has
  // been called.
  virtual std::unique_ptr<WebSocketStream> Upgrade() = 0;

  // Returns true if a read from the stream will succeed. This should be true
  // even if the stream is at EOF.
  virtual bool CanReadFromStream() const = 0;

  void SetRequestHeadersCallback(RequestHeadersCallback callback) override {}

  static std::string MultipleHeaderValuesMessage(
      const std::string& header_name);

  // Subclasses need to implement this method so that the resulting weak
  // pointers are invalidated as soon as the derived class is destroyed.
  virtual base::WeakPtr<WebSocketHandshakeStreamBase> GetWeakPtr() = 0;

 protected:
  // TODO(ricea): If more extensions are added, replace this with a more general
  // mechanism.
  struct WebSocketExtensionParams {
    bool deflate_enabled = false;
    WebSocketDeflateParameters deflate_parameters;
  };

  // Add the Sec-WebSocket-Extensions and Sec-WebSocket-Protocol headers to
  // `headers`.
  static void AddVectorHeaders(const std::vector<std::string>& extensions,
                               const std::vector<std::string>& protocols,
                               HttpRequestHeaders* headers);

  static bool ValidateSubProtocol(
      const HttpResponseHeaders* headers,
      const std::vector<std::string>& requested_sub_protocols,
      std::string* sub_protocol,
      std::string* failure_message);

  static bool ValidateExtensions(const HttpResponseHeaders* headers,
                                 std::string* accepted_extensions_descriptor,
                                 std::string* failure_message,
                                 WebSocketExtensionParams* params);

  void RecordHandshakeResult(HandshakeResult result);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_HANDSHAKE_STREAM_BASE_H_
