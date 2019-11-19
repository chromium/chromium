// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_STREAM_H_
#define NET_WEBSOCKETS_WEBSOCKET_STREAM_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/websockets/websocket_event_interface.h"
#include "net/websockets/websocket_handshake_request_info.h"
#include "net/websockets/websocket_handshake_response_info.h"

class GURL;

namespace base {
class OneShotTimer;
}

namespace url {
class Origin;
}  // namespace url

namespace net {

class AuthChallengeInfo;
class AuthCredentials;
class HttpRequestHeaders;
class HttpResponseHeaders;
class IPEndPoint;
class NetLogWithSource;
class URLRequest;
class URLRequestContext;
struct WebSocketFrame;
class WebSocketBasicHandshakeStream;
class WebSocketHttp2HandshakeStream;

// WebSocketStreamRequest is the caller's handle to the process of creation of a
// WebSocketStream. Deleting the object before the ConnectDelegate OnSuccess or
// OnFailure callbacks are called will cancel the request (and neither callback
// will be called). After OnSuccess or OnFailure have been called, this object
// may be safely deleted without side-effects.
class NET_EXPORT_PRIVATE WebSocketStreamRequest {
 public:
  virtual ~WebSocketStreamRequest();
};

// A subclass of WebSocketStreamRequest that exposes methods that are used as
// part of the handshake.
class NET_EXPORT_PRIVATE WebSocketStreamRequestAPI
    : public WebSocketStreamRequest {
 public:
  virtual void OnBasicHandshakeStreamCreated(
      WebSocketBasicHandshakeStream* handshake_stream) = 0;
  virtual void OnHttp2HandshakeStreamCreated(
      WebSocketHttp2HandshakeStream* handshake_stream) = 0;
  virtual void OnFailure(const std::string& message) = 0;
};

// WebSocketStream is a transport-agnostic interface for reading and writing
// WebSocket frames. This class provides an abstraction for WebSocket streams
// based on various transport layers, such as normal WebSocket connections
// (WebSocket protocol upgraded from HTTP handshake), SPDY transports, or
// WebSocket connections with multiplexing extension. Subtypes of
// WebSocketStream are responsible for managing the underlying transport
// appropriately.
//
// All functions except Close() can be asynchronous. If an operation cannot
// be finished synchronously, the function returns ERR_IO_PENDING, and
// |callback| will be called when the operation is finished. Non-null |callback|
// must be provided to these functions.
//
// Please update the traffic annotations in the websocket_basic_stream.cc and
// websocket_stream.cc if the class is used for any communication with Google.
// In such a case, annotation should be passed from the callers to this class
// and a local annotation can not be used anymore.

class NET_EXPORT_PRIVATE WebSocketStream {
 public:
  // A concrete object derived from ConnectDelegate is supplied by the caller to
  // CreateAndConnectStream() to receive the result of the connection.
  class NET_EXPORT_PRIVATE ConnectDelegate {
   public:
    virtual ~ConnectDelegate();
    // Called when the URLRequest is created.
    virtual void OnCreateRequest(URLRequest* url_request) = 0;

    // Called on successful connection. The parameter is an object derived from
    // WebSocketStream.
    virtual void OnSuccess(std::unique_ptr<WebSocketStream> stream) = 0;

    // Called on failure to connect.
    // |message| contains defails of the failure.
    virtual void OnFailure(const std::string& message) = 0;

    // Called when the WebSocket Opening Handshake starts.
    virtual void OnStartOpeningHandshake(
        std::unique_ptr<WebSocketHandshakeRequestInfo> request) = 0;

    // Called when the WebSocket Opening Handshake ends.
    virtual void OnFinishOpeningHandshake(
        std::unique_ptr<WebSocketHandshakeResponseInfo> response) = 0;

    // Called when there is an SSL certificate error. Should call
    // ssl_error_callbacks->ContinueSSLRequest() or
    // ssl_error_callbacks->CancelSSLRequest().
    virtual void OnSSLCertificateError(
        std::unique_ptr<WebSocketEventInterface::SSLErrorCallbacks>
            ssl_error_callbacks,
        int net_error,
        const SSLInfo& ssl_info,
        bool fatal) = 0;

    // Called when authentication is required. Returns a net error. The opening
    // handshake is blocked when this function returns ERR_IO_PENDING.
    // In that case calling |callback| resumes the handshake. |callback| can be
    // called during the opening handshake. An implementation can rewrite
    // |*credentials| (in the sync case) or provide new credentials (in the
    // async case).
    // Providing null credentials (nullopt in the sync case and nullptr in the
    // async case) cancels authentication. Otherwise the new credentials are set
    // and the opening handshake will be retried with the credentials.
    virtual int OnAuthRequired(
        const AuthChallengeInfo& auth_info,
        scoped_refptr<HttpResponseHeaders> response_headers,
        const IPEndPoint& remote_endpoint,
        base::OnceCallback<void(const AuthCredentials*)> callback,
        base::Optional<AuthCredentials>* credentials) = 0;
  };

  // Create and connect a WebSocketStream of an appropriate type. The actual
  // concrete type returned depends on whether multiplexing or SPDY are being
  // used to communicate with the remote server. If the handshake completed
  // successfully, then connect_delegate->OnSuccess() is called with a
  // WebSocketStream instance. If it failed, then connect_delegate->OnFailure()
  // is called with a WebSocket result code corresponding to the error. Deleting
  // the returned WebSocketStreamRequest object will cancel the connection, in
  // which case the |connect_delegate| object that the caller passed will be
  // deleted without any of its methods being called. Unless cancellation is
  // required, the caller should keep the WebSocketStreamRequest object alive
  // until connect_delegate->OnSuccess() or OnFailure() have been called, then
  // it is safe to delete.
  static std::unique_ptr<WebSocketStreamRequest> CreateAndConnectStream(
      const GURL& socket_url,
      const std::vector<std::string>& requested_subprotocols,
      const url::Origin& origin,
      const GURL& site_for_cookies,
      const net::NetworkIsolationKey& network_isolation_key,
      const HttpRequestHeaders& additional_headers,
      URLRequestContext* url_request_context,
      const NetLogWithSource& net_log,
      std::unique_ptr<ConnectDelegate> connect_delegate);

  // Alternate version of CreateAndConnectStream() for testing use only. It
  // takes |timer| as the handshake timeout timer, and for methods on
  // WebSocketStreamRequestAPI calls the |api_delegate| object before the
  // in-built behaviour if non-null.
  static std::unique_ptr<WebSocketStreamRequest>
  CreateAndConnectStreamForTesting(
      const GURL& socket_url,
      const std::vector<std::string>& requested_subprotocols,
      const url::Origin& origin,
      const GURL& site_for_cookies,
      const net::NetworkIsolationKey& network_isolation_key,
      const HttpRequestHeaders& additional_headers,
      URLRequestContext* url_request_context,
      const NetLogWithSource& net_log,
      std::unique_ptr<ConnectDelegate> connect_delegate,
      std::unique_ptr<base::OneShotTimer> timer,
      std::unique_ptr<WebSocketStreamRequestAPI> api_delegate);

  // Derived classes must make sure Close() is called when the stream is not
  // closed on destruction.
  virtual ~WebSocketStream();

  // Reads WebSocket frame data. This operation finishes when new frame data
  // becomes available.
  //
  // |frames| remains owned by the caller and must be valid until the
  // operation completes or Close() is called. |frames| must be empty on
  // calling.
  //
  // This function should not be called while the previous call of ReadFrames()
  // is still pending.
  //
  // Returns net::OK or one of the net::ERR_* codes.
  //
  // frames->size() >= 1 if the result is OK.
  //
  // Only frames with complete header information are inserted into |frames|. If
  // the currently available bytes of a new frame do not form a complete frame
  // header, then the implementation will buffer them until all the fields in
  // the WebSocketFrameHeader object can be filled. If ReadFrames() is freshly
  // called in this situation, it will return ERR_IO_PENDING exactly as if no
  // data was available.
  //
  // Original frame boundaries are not preserved. In particular, if only part of
  // a frame is available, then the frame will be split, and the available data
  // will be returned immediately.
  //
  // When the socket is closed on the remote side, this method will return
  // ERR_CONNECTION_CLOSED. It will not return OK with an empty vector.
  //
  // If the connection is closed in the middle of receiving an incomplete frame,
  // ReadFrames may discard the incomplete frame. Since the renderer will
  // discard any incomplete messages when the connection is closed, this makes
  // no difference to the overall semantics.
  //
  // Implementations of ReadFrames() must be able to handle being deleted during
  // the execution of callback.Run(). In practice this means that the method
  // calling callback.Run() (and any calling methods in the same object) must
  // return immediately without any further method calls or access to member
  // variables. Implementors should write test(s) for this case.
  //
  // Extensions which use reserved header bits should clear them when they are
  // set correctly. If the reserved header bits are set incorrectly, it is okay
  // to leave it to the caller to report the error.
  //
  // Each WebSocketFrame.data is owned by WebSocketStream and must be valid
  // until next ReadFrames() call.
  virtual int ReadFrames(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
                         CompletionOnceCallback callback) = 0;

  // Writes WebSocket frame data.
  //
  // |frames| must be valid until the operation completes or Close() is called.
  //
  // This function must not be called while a previous call of WriteFrames() is
  // still pending.
  //
  // This method will only return OK if all frames were written completely.
  // Otherwise it will return an appropriate net error code.
  //
  // The callback implementation is permitted to delete this
  // object. Implementations of WriteFrames() should be robust against
  // this. This generally means returning to the event loop immediately after
  // calling the callback.
  virtual int WriteFrames(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
                          CompletionOnceCallback callback) = 0;

  // Closes the stream. All pending I/O operations (if any) are cancelled
  // at this point, so |frames| can be freed.
  virtual void Close() = 0;

  // The subprotocol that was negotiated for the stream. If no protocol was
  // negotiated, then the empty string is returned.
  virtual std::string GetSubProtocol() const = 0;

  // The extensions that were negotiated for the stream. Since WebSocketStreams
  // can be layered, this may be different from what this particular
  // WebSocketStream implements. The primary purpose of this accessor is to make
  // the data available to Javascript. The format of the string is identical to
  // the contents of the Sec-WebSocket-Extensions header supplied by the server,
  // with some canonicalisations applied (leading and trailing whitespace
  // removed, multiple headers concatenated into one comma-separated list). See
  // RFC6455 section 9.1 for the exact format specification. If no
  // extensions were negotiated, the empty string is returned.
  virtual std::string GetExtensions() const = 0;

 protected:
  WebSocketStream();

 private:
  DISALLOW_COPY_AND_ASSIGN(WebSocketStream);
};

// A helper function used in the implementation of CreateAndConnectStream() and
// WebSocketBasicHandshakeStream. It creates a WebSocketHandshakeResponseInfo
// object and dispatches it to the OnFinishOpeningHandshake() method of the
// supplied |connect_delegate|.
void WebSocketDispatchOnFinishOpeningHandshake(
    WebSocketStream::ConnectDelegate* connect_delegate,
    const GURL& gurl,
    const scoped_refptr<HttpResponseHeaders>& headers,
    const IPEndPoint& remote_endpoint,
    base::Time response_time);

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_STREAM_H_
