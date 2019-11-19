// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_REQUEST_H_
#define NET_HTTP_HTTP_STREAM_REQUEST_H_

#include <memory>

#include "base/macros.h"
#include "net/base/load_states.h"
#include "net/base/net_error_details.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_info.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/next_proto.h"
#include "net/spdy/spdy_session_key.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_info.h"
#include "net/websockets/websocket_handshake_stream_base.h"
#include "url/gurl.h"

namespace net {

class BidirectionalStreamImpl;
class HttpAuthController;
class HttpStream;
class SSLCertRequestInfo;

// The HttpStreamRequest is the client's handle to the worker object which
// handles the creation of an HttpStream.  While the HttpStream is being
// created, this object is the creator's handle for interacting with the
// HttpStream creation process.  The request is cancelled by deleting it, after
// which no callbacks will be invoked.
class NET_EXPORT_PRIVATE HttpStreamRequest {
 public:
  // Indicates which type of stream is requested.
  enum StreamType {
    BIDIRECTIONAL_STREAM,
    HTTP_STREAM,
  };

  // The HttpStreamRequest::Delegate is a set of callback methods for a
  // HttpStreamRequestJob.  Generally, only one of these methods will be
  // called as a result of a stream request.
  class NET_EXPORT_PRIVATE Delegate {
   public:
    virtual ~Delegate() {}

    // This is the success case for RequestStream.
    // |stream| is now owned by the delegate.
    // |used_ssl_config| indicates the actual SSL configuration used for this
    // stream, since the HttpStreamRequest may have modified the configuration
    // during stream processing.
    // |used_proxy_info| indicates the actual ProxyInfo used for this stream,
    // since the HttpStreamRequest performs the proxy resolution.
    virtual void OnStreamReady(const SSLConfig& used_ssl_config,
                               const ProxyInfo& used_proxy_info,
                               std::unique_ptr<HttpStream> stream) = 0;

    // This is the success case for RequestWebSocketHandshakeStream.
    // |stream| is now owned by the delegate.
    // |used_ssl_config| indicates the actual SSL configuration used for this
    // stream, since the HttpStreamRequest may have modified the configuration
    // during stream processing.
    // |used_proxy_info| indicates the actual ProxyInfo used for this stream,
    // since the HttpStreamRequest performs the proxy resolution.
    virtual void OnWebSocketHandshakeStreamReady(
        const SSLConfig& used_ssl_config,
        const ProxyInfo& used_proxy_info,
        std::unique_ptr<WebSocketHandshakeStreamBase> stream) = 0;

    virtual void OnBidirectionalStreamImplReady(
        const SSLConfig& used_ssl_config,
        const ProxyInfo& used_proxy_info,
        std::unique_ptr<BidirectionalStreamImpl> stream) = 0;

    // This is the failure to create a stream case.
    // |used_ssl_config| indicates the actual SSL configuration used for this
    // stream, since the HttpStreamRequest may have modified the configuration
    // during stream processing.
    // |used_proxy_info| indicates the actual ProxyInfo used for this stream,
    // since the HttpStreamRequest performs the proxy resolution.
    virtual void OnStreamFailed(int status,
                                const NetErrorDetails& net_error_details,
                                const SSLConfig& used_ssl_config,
                                const ProxyInfo& used_proxy_info) = 0;

    // Called when we have a certificate error for the request.
    // |used_ssl_config| indicates the actual SSL configuration used for this
    // stream, since the HttpStreamRequest may have modified the configuration
    // during stream processing.
    virtual void OnCertificateError(int status,
                                    const SSLConfig& used_ssl_config,
                                    const SSLInfo& ssl_info) = 0;

    // This is the failure case where we need proxy authentication during
    // proxy tunnel establishment.  For the tunnel case, we were unable to
    // create the HttpStream, so the caller provides the auth and then resumes
    // the HttpStreamRequest.
    //
    // For the non-tunnel case, the caller will discover the authentication
    // failure when reading response headers. At that point, it will handle the
    // authentication failure and restart the HttpStreamRequest entirely.
    //
    // Ownership of |auth_controller| and |proxy_response| are owned
    // by the HttpStreamRequest. |proxy_response| is not guaranteed to be usable
    // after the lifetime of this callback.  The delegate may take a reference
    // to |auth_controller| if it is needed beyond the lifetime of this
    // callback.
    //
    // |used_ssl_config| indicates the actual SSL configuration used for this
    // stream, since the HttpStreamRequest may have modified the configuration
    // during stream processing.
    virtual void OnNeedsProxyAuth(const HttpResponseInfo& proxy_response,
                                  const SSLConfig& used_ssl_config,
                                  const ProxyInfo& used_proxy_info,
                                  HttpAuthController* auth_controller) = 0;

    // This is the failure for SSL Client Auth
    // Ownership of |cert_info| is retained by the HttpStreamRequest.  The
    // delegate may take a reference if it needs the cert_info beyond the
    // lifetime of this callback.
    virtual void OnNeedsClientAuth(const SSLConfig& used_ssl_config,
                                   SSLCertRequestInfo* cert_info) = 0;

    // Called when finding all QUIC alternative services are marked broken for
    // the origin in this request which advertises supporting QUIC.
    virtual void OnQuicBroken() = 0;
  };

  class NET_EXPORT_PRIVATE Helper {
   public:
    virtual ~Helper() {}

    // Returns the LoadState for Request.
    virtual LoadState GetLoadState() const = 0;

    // Called when Request is destructed.
    virtual void OnRequestComplete() = 0;

    // Called to resume the HttpStream creation process when necessary
    // Proxy authentication credentials are collected.
    virtual int RestartTunnelWithProxyAuth() = 0;

    // Called when the priority of transaction changes.
    virtual void SetPriority(RequestPriority priority) = 0;
  };

  // Request will notify |job_controller| when it's destructed.
  // Thus |job_controller| is valid for the lifetime of the |this| Request.
  HttpStreamRequest(const GURL& url,
                    Helper* helper,
                    HttpStreamRequest::Delegate* delegate,
                    WebSocketHandshakeStreamBase::CreateHelper*
                        websocket_handshake_stream_create_helper,
                    const NetLogWithSource& net_log,
                    StreamType stream_type);

  ~HttpStreamRequest();

  // When a HttpStream creation process is stalled due to necessity
  // of Proxy authentication credentials, the delegate OnNeedsProxyAuth
  // will have been called.  It now becomes the delegate's responsibility
  // to collect the necessary credentials, and then call this method to
  // resume the HttpStream creation process.
  int RestartTunnelWithProxyAuth();

  // Called when the priority of the parent transaction changes.
  void SetPriority(RequestPriority priority);

  // Marks completion of the request. Must be called before OnStreamReady().
  void Complete(bool was_alpn_negotiated,
                NextProto negotiated_protocol,
                bool using_spdy);

  // Called by |helper_| to record connection attempts made by the socket
  // layer in an attached Job for this stream request.
  void AddConnectionAttempts(const ConnectionAttempts& attempts);

  // Returns the LoadState for the request.
  LoadState GetLoadState() const;

  // Returns true if TLS/ALPN was negotiated for this stream.
  bool was_alpn_negotiated() const;

  // Protocol negotiated with the server.
  NextProto negotiated_protocol() const;

  // Returns true if this stream is being fetched over SPDY.
  bool using_spdy() const;

  // Returns socket-layer connection attempts made for this stream request.
  const ConnectionAttempts& connection_attempts() const;

  // Returns the WebSocketHandshakeStreamBase::CreateHelper for this stream
  // request.
  WebSocketHandshakeStreamBase::CreateHelper*
  websocket_handshake_stream_create_helper() const;

  // The GURL from the HttpRequestInfo the started the Request.
  const GURL& url() const { return url_; }

  const NetLogWithSource& net_log() const { return net_log_; }

  StreamType stream_type() const { return stream_type_; }

  bool completed() const { return completed_; }

 private:
  const GURL url_;

  // Unowned. The helper must outlive this request.
  Helper* helper_;

  WebSocketHandshakeStreamBase::CreateHelper* const
      websocket_handshake_stream_create_helper_;
  const NetLogWithSource net_log_;

  bool completed_;
  bool was_alpn_negotiated_;
  // Protocol negotiated with the server.
  NextProto negotiated_protocol_;
  bool using_spdy_;
  ConnectionAttempts connection_attempts_;
  const StreamType stream_type_;

  DISALLOW_COPY_AND_ASSIGN(HttpStreamRequest);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_REQUEST_H_
