// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The QuicHttpProxyBackendStream instance manages an instance of
// net::URLRequest to initiate a single HTTP call to the backend. It also
// implements the callbacks of net::URLRequest to receive the response. It is
// instantiated by a delegate (for instance, the QuicSimpleServerStream class)
// when a complete HTTP request is received within a single QUIC stream.
// However, the instance is owned by QuicHttpProxyBackend, that destroys it
// safely on the quic proxy thread. Upon receiving a response (success or
// failed), the response headers and body are posted back to the main thread. In
// the main thread, the QuicHttpProxyBackendStream instance calls the interface,
// that is implemented by the delegate to return the response headers and body.
// In addition to managing the HTTP request/response to the backend, it
// translates the quic_spdy headers to/from HTTP headers for the backend.
//

#ifndef NET_TOOLS_QUIC_QUIC_HTTP_PROXY_BACKEND_STREAM_H_
#define NET_TOOLS_QUIC_QUIC_HTTP_PROXY_BACKEND_STREAM_H_

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "net/base/request_priority.h"
#include "net/base/upload_data_stream.h"
#include "net/url_request/url_request.h"

#include "net/third_party/quiche/src/quic/tools/quic_backend_response.h"
#include "net/third_party/quiche/src/quic/tools/quic_simple_server_backend.h"
#include "net/third_party/quiche/src/spdy/core/spdy_header_block.h"
#include "net/tools/quic/quic_http_proxy_backend.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}  // namespace base

namespace quic {
class QuicBackendResponse;
class QuicSimpleServerBackend;
}  // namespace quic

namespace net {

class HttpRequestHeaders;
class SSLCertRequestInfo;
class SSLInfo;
class UploadDataStream;

class QuicHttpProxyBackend;

// An adapter for making HTTP requests to net::URLRequest.
//
// TODO(https://crbug.com/937621):  This class does not appear to be thread
// safe, so all its tests are disabled.
class QuicHttpProxyBackendStream : public net::URLRequest::Delegate {
 public:
  explicit QuicHttpProxyBackendStream(QuicHttpProxyBackend* context);
  ~QuicHttpProxyBackendStream() override;

  static const std::set<std::string> kHopHeaders;
  static const int kBufferSize;
  static const int kProxyHttpBackendError;
  static const std::string kDefaultQuicPeerIP;

  // Set callbacks to be called from this to the main (quic) thread.
  // A |delegate| may be NULL.
  // If set_delegate() is called multiple times, only the last delegate will be
  // used.
  void set_delegate(quic::QuicSimpleServerBackend::RequestHandler* delegate);
  void reset_delegate() { delegate_ = nullptr; }

  void Initialize(quic::QuicConnectionId quic_connection_id,
                  quic::QuicStreamId quic_stream_id,
                  std::string quic_peer_ip);

  virtual bool SendRequestToBackend(
      const spdy::SpdyHeaderBlock* incoming_request_headers,
      const std::string& incoming_body);

  quic::QuicConnectionId quic_connection_id() const {
    return quic_connection_id_;
  }
  quic::QuicStreamId quic_stream_id() const { return quic_stream_id_; }

  const net::HttpRequestHeaders& request_headers() const {
    return request_headers_;
  }
  // Releases all resources for the request and deletes the object itself.
  virtual void CancelRequest();

  // net::URLRequest::Delegate implementations:
  void OnReceivedRedirect(net::URLRequest* request,
                          const net::RedirectInfo& redirect_info,
                          bool* defer_redirect) override;
  void OnCertificateRequested(
      net::URLRequest* request,
      net::SSLCertRequestInfo* cert_request_info) override;
  void OnSSLCertificateError(net::URLRequest* request,
                             int net_error,
                             const net::SSLInfo& ssl_info,
                             bool fatal) override;
  void OnResponseStarted(net::URLRequest* request, int net_error) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

  bool ResponseIsCompleted() const { return response_completed_; }
  quic::QuicBackendResponse* GetBackendResponse() const;

 private:
  void StartOnBackendThread();
  void SendRequestOnBackendThread();
  void ReadOnceTask();
  void OnResponseCompleted();
  void CopyHeaders(const spdy::SpdyHeaderBlock* incoming_request_headers);
  bool ValidateHttpMethod(std::string method);
  bool AddRequestHeader(std::string name, std::string value);
  // Adds a request body to the request before it starts.
  void SetUpload(std::unique_ptr<net::UploadDataStream> upload);
  void SendResponseOnDelegateThread();
  void ReleaseRequest();
  spdy::SpdyHeaderBlock getAsQuicHeaders(net::HttpResponseHeaders* resp_headers,
                                         int response_code,
                                         uint64_t response_decoded_body_size);

  // The quic proxy backend context
  QuicHttpProxyBackend* proxy_context_;
  // Send back the response from the backend to |delegate_|
  quic::QuicSimpleServerBackend::RequestHandler* delegate_;
  // Task runner for interacting with the delegate
  scoped_refptr<base::SequencedTaskRunner> delegate_task_runner_;
  // Task runner for the proxy network operations.
  scoped_refptr<base::SingleThreadTaskRunner> quic_proxy_task_runner_;

  // The corresponding QUIC conn/client/stream
  quic::QuicConnectionId quic_connection_id_;
  quic::QuicStreamId quic_stream_id_;
  std::string quic_peer_ip_;

  // Url, method and spec for making a http request to the Backend
  GURL url_;
  std::string method_type_;
  net::HttpRequestHeaders request_headers_;
  std::unique_ptr<net::UploadDataStream> upload_;
  std::unique_ptr<net::URLRequest> url_request_;

  // Buffers that holds the response body
  scoped_refptr<IOBuffer> buf_;
  std::string data_received_;
  bool response_completed_;
  // Response and push resources received from the backend
  bool headers_set_;
  std::unique_ptr<quic::QuicBackendResponse> quic_response_;

  base::WeakPtrFactory<QuicHttpProxyBackendStream> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(QuicHttpProxyBackendStream);
};

}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_HTTP_PROXY_BACKEND_STREAM_H_
