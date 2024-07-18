// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_HTTP3_HANDSHAKE_STREAM_H_
#define NET_WEBSOCKETS_WEBSOCKET_HTTP3_HANDSHAKE_STREAM_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_session_pool.h"
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
struct AlternativeService;
struct HttpRequestInfo;
struct LoadTimingInfo;
struct NetErrorDetails;

class NET_EXPORT_PRIVATE WebSocketHttp3HandshakeStream final
    : public WebSocketHandshakeStreamBase,
      public WebSocketQuicStreamAdapter::Delegate {
 public:
  WebSocketHttp3HandshakeStream(
      std::unique_ptr<QuicChromiumClientSession::Handle> session,
      WebSocketStream::ConnectDelegate* connect_delegate,
      std::vector<std::string> requested_sub_protocols,
      std::vector<std::string> requested_extensions,
      WebSocketStreamRequestAPI* request,
      std::set<std::string> dns_aliases);

  WebSocketHttp3HandshakeStream(const WebSocketHttp3HandshakeStream&) = delete;
  WebSocketHttp3HandshakeStream& operator=(
      const WebSocketHttp3HandshakeStream&) = delete;

  ~WebSocketHttp3HandshakeStream() override;

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

  // WebSocketQuicStreamAdapter::Delegate methods.
  void OnHeadersSent() override;
  void OnHeadersReceived(
      const quiche::HttpHeaderBlock& response_headers) override;
  void OnClose(int status) override;

 private:
  void ReceiveAdapterAndStartRequest(
      std::unique_ptr<WebSocketQuicStreamAdapter> adapter);

  // Validates the response and sends the finished handshake event.
  int ValidateResponse();

  // Check that the headers are well-formed and have a 200 status code,
  // in which case returns OK, otherwise returns ERR_INVALID_RESPONSE.
  int ValidateUpgradeResponse(const HttpResponseHeaders* headers);

  void OnFailure(const std::string& message,
                 int net_error,
                 std::optional<int> response_code);

  HandshakeResult result_ = HandshakeResult::HTTP3_INCOMPLETE;

  std::unique_ptr<WebSocketSpdyStreamAdapter> adapter_;

  // True if `stream_` has been created then closed.
  bool stream_closed_ = false;

  // The error code corresponding to the reason for closing the stream.
  // Only meaningful if `stream_closed_` is true.
  int stream_error_ = OK;

  // True if complete response headers have been received.
  bool response_headers_complete_ = false;

  // Time the request was issued.
  base::Time request_time_;

  std::unique_ptr<QuicChromiumClientSession::Handle> session_;
  // Owned by another object.
  // `connect_delegate` will live during the lifetime of this object.
  const raw_ptr<WebSocketStream::ConnectDelegate> connect_delegate_;

  raw_ptr<HttpResponseInfo> http_response_info_ = nullptr;

  quiche::HttpHeaderBlock http3_request_headers_;

  // The sub-protocols we requested.
  std::vector<std::string> requested_sub_protocols_;

  // The extensions we requested.
  std::vector<std::string> requested_extensions_;

  const raw_ptr<WebSocketStreamRequestAPI> stream_request_;

  raw_ptr<const HttpRequestInfo> request_info_ = nullptr;

  RequestPriority priority_;

  NetLogWithSource net_log_;

  // WebSocketQuicStreamAdapter holding a WeakPtr to `stream_`.
  // This can be passed on to WebSocketBasicStream when created.
  std::unique_ptr<WebSocketQuicStreamAdapter> stream_adapter_;

  CompletionOnceCallback callback_;

  // The sub-protocol selected by the server.
  std::string sub_protocol_;

  // The extension(s) selected by the server.
  std::string extensions_;

  // The extension parameters. The class is defined in the implementation file
  // to avoid including extension-related header files here.
  std::unique_ptr<WebSocketExtensionParams> extension_params_;

  std::set<std::string> dns_aliases_;

  base::WeakPtrFactory<WebSocketHttp3HandshakeStream> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_HTTP3_HANDSHAKE_STREAM_H_
