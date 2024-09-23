// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_HTTP2_HANDSHAKE_STREAM_H_
#define NET_WEBSOCKETS_WEBSOCKET_HTTP2_HANDSHAKE_STREAM_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/log/net_log_with_source.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/websockets/websocket_basic_stream_adapters.h"
#include "net/websockets/websocket_handshake_stream_base.h"
#include "net/websockets/websocket_stream.h"

namespace net {

class HttpNetworkSession;
class HttpRequestHeaders;
class HttpResponseHeaders;
class HttpResponseInfo;
class HttpStream;
class IOBuffer;
class IPEndPoint;
class SSLInfo;
class SpdySession;
class SpdyStream;
class SpdyStreamRequest;
struct AlternativeService;
struct HttpRequestInfo;
struct LoadTimingInfo;
struct NetErrorDetails;
struct WebSocketExtensionParams;

class NET_EXPORT_PRIVATE WebSocketHttp2HandshakeStream
    : public WebSocketHandshakeStreamBase,
      public WebSocketSpdyStreamAdapter::Delegate {
 public:
  // |connect_delegate| and |request| must out-live this object.
  WebSocketHttp2HandshakeStream(
      base::WeakPtr<SpdySession> session,
      WebSocketStream::ConnectDelegate* connect_delegate,
      std::vector<std::string> requested_sub_protocols,
      std::vector<std::string> requested_extensions,
      WebSocketStreamRequestAPI* request,
      std::set<std::string> dns_aliases);

  WebSocketHttp2HandshakeStream(const WebSocketHttp2HandshakeStream&) = delete;
  WebSocketHttp2HandshakeStream& operator=(
      const WebSocketHttp2HandshakeStream&) = delete;

  ~WebSocketHttp2HandshakeStream() override;

  // HttpStream methods.
  void RegisterRequest(const HttpRequestInfo* request_info) override;
  int InitializeStream(bool can_send_early,
                       RequestPriority priority,
                       const NetLogWithSource& net_log,
                       CompletionOnceCallback callback) override;
  int SendRequest(const HttpRequestHeaders& request_headers,
                  HttpResponseInfo* response,
                  CompletionOnceCallback callback) override;
  int ReadResponseHeaders(CompletionOnceCallback callback) override;
  int ReadResponseBody(IOBuffer* buf,
                       int buf_len,
                       CompletionOnceCallback callback) override;
  void Close(bool not_reusable) override;
  bool IsResponseBodyComplete() const override;
  bool IsConnectionReused() const override;
  void SetConnectionReused() override;
  bool CanReuseConnection() const override;
  int64_t GetTotalReceivedBytes() const override;
  int64_t GetTotalSentBytes() const override;
  bool GetAlternativeService(
      AlternativeService* alternative_service) const override;
  bool GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const override;
  void GetSSLInfo(SSLInfo* ssl_info) override;
  int GetRemoteEndpoint(IPEndPoint* endpoint) override;
  void Drain(HttpNetworkSession* session) override;
  void SetPriority(RequestPriority priority) override;
  void PopulateNetErrorDetails(NetErrorDetails* details) override;
  std::unique_ptr<HttpStream> RenewStreamForAuth() override;
  const std::set<std::string>& GetDnsAliases() const override;
  std::string_view GetAcceptChViaAlps() const override;

  // WebSocketHandshakeStreamBase methods.

  // This is called from the top level once correct handshake response headers
  // have been received. It creates an appropriate subclass of WebSocketStream
  // depending on what extensions were negotiated. This object is unusable after
  // Upgrade() has been called and should be disposed of as soon as possible.
  std::unique_ptr<WebSocketStream> Upgrade() override;

  bool CanReadFromStream() const override;

  base::WeakPtr<WebSocketHandshakeStreamBase> GetWeakPtr() override;

  // WebSocketSpdyStreamAdapter::Delegate methods.
  void OnHeadersSent() override;
  void OnHeadersReceived(
      const quiche::HttpHeaderBlock& response_headers) override;
  void OnClose(int status) override;

  // Called by |spdy_stream_request_| when requested stream is ready.
  void StartRequestCallback(int rv);

 private:
  // Validates the response and sends the finished handshake event.
  int ValidateResponse();

  // Check that the headers are well-formed and have a 200 status code,
  // in which case returns OK, otherwise returns ERR_INVALID_RESPONSE.
  int ValidateUpgradeResponse(const HttpResponseHeaders* headers);

  void OnFailure(const std::string& message,
                 int net_error,
                 std::optional<int> response_code);

  HandshakeResult result_ = HandshakeResult::HTTP2_INCOMPLETE;

  // The connection to open the Websocket stream on.
  base::WeakPtr<SpdySession> session_;

  // Owned by another object.
  // |connect_delegate| will live during the lifetime of this object.
  const raw_ptr<WebSocketStream::ConnectDelegate> connect_delegate_;

  raw_ptr<HttpResponseInfo> http_response_info_ = nullptr;

  quiche::HttpHeaderBlock http2_request_headers_;

  // The sub-protocols we requested.
  std::vector<std::string> requested_sub_protocols_;

  // The extensions we requested.
  std::vector<std::string> requested_extensions_;

  const raw_ptr<WebSocketStreamRequestAPI> stream_request_;

  raw_ptr<const HttpRequestInfo> request_info_ = nullptr;

  RequestPriority priority_;

  NetLogWithSource net_log_;

  // SpdyStreamRequest that will create the stream.
  std::unique_ptr<SpdyStreamRequest> spdy_stream_request_;

  // SpdyStream corresponding to the request.
  base::WeakPtr<SpdyStream> stream_;

  // WebSocketSpdyStreamAdapter holding a WeakPtr to |stream_|.
  // This can be passed on to WebSocketBasicStream when created.
  std::unique_ptr<WebSocketSpdyStreamAdapter> stream_adapter_;

  // True if |stream_| has been created then closed.
  bool stream_closed_ = false;

  // The error code corresponding to the reason for closing the stream.
  // Only meaningful if |stream_closed_| is true.
  int stream_error_ = OK;

  // True if complete response headers have been received.
  bool response_headers_complete_ = false;

  // Save callback provided in asynchronous HttpStream methods.
  CompletionOnceCallback callback_;

  // The sub-protocol selected by the server.
  std::string sub_protocol_;

  // The extension(s) selected by the server.
  std::string extensions_;

  // The extension parameters. The class is defined in the implementation file
  // to avoid including extension-related header files here.
  std::unique_ptr<WebSocketExtensionParams> extension_params_;

  // Stores any DNS aliases for the remote endpoint. Includes all known aliases,
  // e.g. from A, AAAA, or HTTPS, not just from the address used for the
  // connection, in no particular order. These are stored in the stream instead
  // of the session due to complications related to IP-pooling.
  std::set<std::string> dns_aliases_;

  base::WeakPtrFactory<WebSocketHttp2HandshakeStream> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_HTTP2_HANDSHAKE_STREAM_H_
