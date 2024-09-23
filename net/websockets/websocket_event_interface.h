// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_EVENT_INTERFACE_H_
#define NET_WEBSOCKETS_WEBSOCKET_EVENT_INTERFACE_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/net_export.h"

class GURL;

namespace net {

class AuthChallengeInfo;
class AuthCredentials;
class IPEndPoint;
class HttpResponseHeaders;
class SSLInfo;
class URLRequest;
struct TransportInfo;
struct WebSocketHandshakeRequestInfo;
struct WebSocketHandshakeResponseInfo;

// Interface for events sent from the network layer to the content layer. These
// events will generally be sent as-is to the renderer process.
class NET_EXPORT WebSocketEventInterface {
 public:
  typedef int WebSocketMessageType;

  WebSocketEventInterface(const WebSocketEventInterface&) = delete;
  WebSocketEventInterface& operator=(const WebSocketEventInterface&) = delete;

  virtual ~WebSocketEventInterface() = default;

  // Called when a URLRequest is created for handshaking.
  virtual void OnCreateURLRequest(URLRequest* request) = 0;

  // Called when OnConnected is called on the URLRequest for handshaking.
  virtual void OnURLRequestConnected(URLRequest* request,
                                     const TransportInfo& info) = 0;

  // Called in response to an AddChannelRequest. This means that a response has
  // been received from the remote server.
  virtual void OnAddChannelResponse(
      std::unique_ptr<WebSocketHandshakeResponseInfo> response,
      const std::string& selected_subprotocol,
      const std::string& extensions) = 0;

  // Called when a data frame has been received from the remote host and needs
  // to be forwarded to the renderer process.
  // |payload| stays valid as long as both
  // - the associated WebSocketChannel is valid.
  // - no further ReadFrames() is called on the associated WebSocketChannel.
  virtual void OnDataFrame(bool fin,
                           WebSocketMessageType type,
                           base::span<const char> payload) = 0;

  // Returns true if data pipe is full and waiting the renderer process read
  // out. The network service should not read more from network until that.
  virtual bool HasPendingDataFrames() = 0;

  // Called once for each call to SendFrame() once the frame has been passed to
  // the OS.
  virtual void OnSendDataFrameDone() = 0;

  // Called when the remote server has Started the WebSocket Closing
  // Handshake. The client should not attempt to send any more messages after
  // receiving this message. It will be followed by OnDropChannel() when the
  // closing handshake is complete.
  virtual void OnClosingHandshake() = 0;

  // Called when the channel has been dropped, either due to a network close, a
  // network error, or a protocol error. This may or may not be preceeded by a
  // call to OnClosingHandshake().
  //
  // Warning: Both the |code| and |reason| are passed through to Javascript, so
  // callers must take care not to provide details that could be useful to
  // attackers attempting to use WebSockets to probe networks.
  //
  // |was_clean| should be true if the closing handshake completed successfully.
  //
  // The channel should not be used again after OnDropChannel() has been
  // called.
  //
  // This function deletes the Channel.
  virtual void OnDropChannel(bool was_clean,
                             uint16_t code,
                             const std::string& reason) = 0;

  // Called when the browser fails the channel, as specified in the spec.
  //
  // The channel should not be used again after OnFailChannel() has been
  // called.
  //
  // |message| is a human readable string describing the failure. (It may be
  // empty.) |net_error| contains the network error code for the failure, which
  // may be |OK| if the failure was at a higher level. |response_code| contains
  // the HTTP status code that caused the failure, or |std::nullopt| if the
  // attempt didn't get that far.
  //
  // This function deletes the Channel.
  virtual void OnFailChannel(const std::string& message,
                             int net_error,
                             std::optional<int> response_code) = 0;

  // Called when the browser starts the WebSocket Opening Handshake.
  virtual void OnStartOpeningHandshake(
      std::unique_ptr<WebSocketHandshakeRequestInfo> request) = 0;

  // Callbacks to be used in response to a call to OnSSLCertificateError. Very
  // similar to content::SSLErrorHandler::Delegate (which we can't use directly
  // due to layering constraints).
  class NET_EXPORT SSLErrorCallbacks {
   public:
    virtual ~SSLErrorCallbacks() = default;

    // Cancels the SSL response in response to the error.
    virtual void CancelSSLRequest(int error, const SSLInfo* ssl_info) = 0;

    // Continue with the SSL connection despite the error.
    virtual void ContinueSSLRequest() = 0;
  };

  // Called on SSL Certificate Error during the SSL handshake. Should result in
  // a call to either ssl_error_callbacks->ContinueSSLRequest() or
  // ssl_error_callbacks->CancelSSLRequest(). Normally the implementation of
  // this method will delegate to content::SSLManager::OnSSLCertificateError to
  // make the actual decision. The callbacks must not be called after the
  // WebSocketChannel has been destroyed.
  virtual void OnSSLCertificateError(
      std::unique_ptr<SSLErrorCallbacks> ssl_error_callbacks,
      const GURL& url,
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
      const IPEndPoint& socket_address,
      base::OnceCallback<void(const AuthCredentials*)> callback,
      std::optional<AuthCredentials>* credentials) = 0;

 protected:
  WebSocketEventInterface() = default;
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_EVENT_INTERFACE_H_
