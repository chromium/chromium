// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_BASIC_HANDSHAKE_STREAM_H_
#define NET_WEBSOCKETS_WEBSOCKET_BASIC_HANDSHAKE_STREAM_H_

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
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/http/http_basic_state.h"
#include "net/log/net_log_with_source.h"
#include "net/websockets/websocket_handshake_stream_base.h"
#include "net/websockets/websocket_stream.h"
#include "url/gurl.h"

namespace net {

class StreamSocketHandle;
class HttpNetworkSession;
class HttpRequestHeaders;
class HttpResponseHeaders;
class HttpResponseInfo;
class HttpStream;
class HttpStreamParser;
class IOBuffer;
class IPEndPoint;
class SSLInfo;
class WebSocketEndpointLockManager;
class WebSocketStreamRequestAPI;
struct AlternativeService;
struct HttpRequestInfo;
struct LoadTimingInfo;
struct NetErrorDetails;
struct WebSocketExtensionParams;

class NET_EXPORT_PRIVATE WebSocketBasicHandshakeStream final
    : public WebSocketHandshakeStreamBase {
 public:
  // |connect_delegate| and |failure_message| must out-live this object.
  WebSocketBasicHandshakeStream(
      std::unique_ptr<StreamSocketHandle> connection,
      WebSocketStream::ConnectDelegate* connect_delegate,
      bool using_proxy,
      std::vector<std::string> requested_sub_protocols,
      std::vector<std::string> requested_extensions,
      WebSocketStreamRequestAPI* request,
      WebSocketEndpointLockManager* websocket_endpoint_lock_manager);

  WebSocketBasicHandshakeStream(const WebSocketBasicHandshakeStream&) = delete;
  WebSocketBasicHandshakeStream& operator=(
      const WebSocketBasicHandshakeStream&) = delete;

  ~WebSocketBasicHandshakeStream() override;

  // HttpStream methods
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

  // This is called from the top level once correct handshake response headers
  // have been received. It creates an appropriate subclass of WebSocketStream
  // depending on what extensions were negotiated. This object is unusable after
  // Upgrade() has been called and should be disposed of as soon as possible.
  std::unique_ptr<WebSocketStream> Upgrade() override;

  bool CanReadFromStream() const override;

  base::WeakPtr<WebSocketHandshakeStreamBase> GetWeakPtr() override;

  // Set the value used for the next Sec-WebSocket-Key header
  // deterministically. The key is only used once, and then discarded.
  // For tests only.
  void SetWebSocketKeyForTesting(const std::string& key);

 private:
  // A wrapper for the ReadResponseHeaders callback that checks whether or not
  // the connection has been accepted.
  void ReadResponseHeadersCallback(CompletionOnceCallback callback, int result);

  // Validates the response and sends the finished handshake event.
  int ValidateResponse(int rv);

  // Check that the headers are well-formed for a 101 response, and returns
  // OK if they are, otherwise returns ERR_INVALID_RESPONSE.
  int ValidateUpgradeResponse(const HttpResponseHeaders* headers);

  void OnFailure(const std::string& message,
                 int net_error,
                 std::optional<int> response_code);

  HttpStreamParser* parser() const { return state_.parser(); }

  HandshakeResult result_ = HandshakeResult::INCOMPLETE;

  // The request URL.
  GURL url_;

  // HttpBasicState holds most of the handshake-related state.
  HttpBasicState state_;

  // Owned by another object.
  // |connect_delegate| will live during the lifetime of this object.
  const raw_ptr<WebSocketStream::ConnectDelegate> connect_delegate_;

  // This is stored in SendRequest() for use by ReadResponseHeaders().
  raw_ptr<HttpResponseInfo> http_response_info_ = nullptr;

  // The key to be sent in the next Sec-WebSocket-Key header. Usually NULL (the
  // key is generated on the fly).
  std::optional<std::string> handshake_challenge_for_testing_;

  // The required value for the Sec-WebSocket-Accept header.
  std::string handshake_challenge_response_;

  // The sub-protocols we requested.
  std::vector<std::string> requested_sub_protocols_;

  // The extensions we requested.
  std::vector<std::string> requested_extensions_;

  // The sub-protocol selected by the server.
  std::string sub_protocol_;

  // The extension(s) selected by the server.
  std::string extensions_;

  // The extension parameters. The class is defined in the implementation file
  // to avoid including extension-related header files here.
  std::unique_ptr<WebSocketExtensionParams> extension_params_;

  const raw_ptr<WebSocketStreamRequestAPI> stream_request_;

  const raw_ptr<WebSocketEndpointLockManager> websocket_endpoint_lock_manager_;

  NetLogWithSource net_log_;

  // The request to send.
  // Set to null before the response body is read. This is to allow |this| to
  // be shared for reading and to possibly outlive request_info_'s owner.
  // Setting to null happens after headers are completely read or upload data
  // stream is uploaded, whichever is later.
  raw_ptr<const HttpRequestInfo> request_info_;

  base::WeakPtrFactory<WebSocketBasicHandshakeStream> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_BASIC_HANDSHAKE_STREAM_H_
