// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_TEST_UTIL_H_
#define NET_WEBSOCKETS_WEBSOCKET_TEST_UTIL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "net/http/http_basic_state.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_stream_parser.h"
#include "net/socket/client_socket_handle.h"
#include "net/third_party/quiche/src/spdy/core/spdy_header_block.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "net/websockets/websocket_handshake_stream_create_helper.h"
#include "net/websockets/websocket_stream.h"

namespace url {
class Origin;
}  // namespace url

namespace net {

using WebSocketExtraHeaders = std::vector<std::pair<std::string, std::string>>;

class MockClientSocketFactory;
class WebSocketBasicHandshakeStream;
class ProxyResolutionService;
class SequencedSocketData;
class IPEndPoint;
struct SSLSocketDataProvider;

class LinearCongruentialGenerator {
 public:
  explicit LinearCongruentialGenerator(uint32_t seed);
  uint32_t Generate();

 private:
  uint64_t current_;
};

// Converts a vector of header key-value pairs into a single string.
std::string WebSocketExtraHeadersToString(const WebSocketExtraHeaders& headers);

// Converts a vector of header key-value pairs into an HttpRequestHeaders
HttpRequestHeaders WebSocketExtraHeadersToHttpRequestHeaders(
    const WebSocketExtraHeaders& headers);

// Generates a standard WebSocket handshake request. The challenge key used is
// "dGhlIHNhbXBsZSBub25jZQ==". Each header in |extra_headers| must be terminated
// with "\r\n".
std::string WebSocketStandardRequest(
    const std::string& path,
    const std::string& host,
    const url::Origin& origin,
    const std::string& send_additional_request_headers,
    const std::string& extra_headers);

// Generates a standard WebSocket handshake request. The challenge key used is
// "dGhlIHNhbXBsZSBub25jZQ==". |cookies| must be empty or terminated with
// "\r\n". Each header in |extra_headers| must be terminated with "\r\n".
std::string WebSocketStandardRequestWithCookies(
    const std::string& path,
    const std::string& host,
    const url::Origin& origin,
    const std::string& cookies,
    const std::string& send_additional_request_headers,
    const std::string& extra_headers);

// A response with the appropriate accept header to match the above
// challenge key. Each header in |extra_headers| must be terminated with
// "\r\n".
std::string WebSocketStandardResponse(const std::string& extra_headers);

// WebSocketCommonTestHeaders() generates a common set of request headers
// corresponding to WebSocketStandardRequest("/", "www.example.org",
// url::Origin::Create(GURL("http://origin.example.org")), "", "")
HttpRequestHeaders WebSocketCommonTestHeaders();

// Generates a handshake request header block when using WebSockets over HTTP/2.
spdy::SpdyHeaderBlock WebSocketHttp2Request(
    const std::string& path,
    const std::string& authority,
    const std::string& origin,
    const WebSocketExtraHeaders& extra_headers);

// Generates a handshake response header block when using WebSockets over
// HTTP/2.
spdy::SpdyHeaderBlock WebSocketHttp2Response(
    const WebSocketExtraHeaders& extra_headers);

// This class provides a convenient way to construct a MockClientSocketFactory
// for WebSocket tests.
class WebSocketMockClientSocketFactoryMaker {
 public:
  WebSocketMockClientSocketFactoryMaker();
  ~WebSocketMockClientSocketFactoryMaker();

  // Tell the factory to create a socket which expects |expect_written| to be
  // written, and responds with |return_to_read|. The test will fail if the
  // expected text is not written, or all the bytes are not read. This adds data
  // for a new mock-socket using AddRawExpections(), and so can be called
  // multiple times to queue up multiple mock sockets, but usually in those
  // cases the lower-level AddRawExpections() interface is more appropriate.
  void SetExpectations(const std::string& expect_written,
                       const std::string& return_to_read);

  // A low-level interface to permit arbitrary expectations to be added. The
  // mock sockets will be created in the same order that they were added.
  void AddRawExpectations(std::unique_ptr<SequencedSocketData> socket_data);

  // Allow an SSL socket data provider to be added. You must also supply a mock
  // transport socket for it to use. If the mock SSL handshake fails then the
  // mock transport socket will connect but have nothing read or written. If the
  // mock handshake succeeds then the data from the underlying transport socket
  // will be passed through unchanged (without encryption).
  void AddSSLSocketDataProvider(
      std::unique_ptr<SSLSocketDataProvider> ssl_socket_data);

  // Call to get a pointer to the factory, which remains owned by this object.
  MockClientSocketFactory* factory();

 private:
  struct Detail;
  std::unique_ptr<Detail> detail_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketMockClientSocketFactoryMaker);
};

// This class encapsulates the details of creating a
// TestURLRequestContext that returns mock ClientSocketHandles that do what is
// required by the tests.
struct WebSocketTestURLRequestContextHost {
 public:
  WebSocketTestURLRequestContextHost();
  ~WebSocketTestURLRequestContextHost();

  void SetExpectations(const std::string& expect_written,
                       const std::string& return_to_read) {
    maker_.SetExpectations(expect_written, return_to_read);
  }

  void AddRawExpectations(std::unique_ptr<SequencedSocketData> socket_data);

  // Allow an SSL socket data provider to be added.
  void AddSSLSocketDataProvider(
      std::unique_ptr<SSLSocketDataProvider> ssl_socket_data);

  // Allow a proxy to be set. Usage:
  //   SetProxyConfig("proxy1:8000");
  // Any syntax accepted by net::ProxyConfig::ParseFromString() will work.
  // Do not call after GetURLRequestContext() has been called.
  void SetProxyConfig(const std::string& proxy_rules);

  // Call after calling one of SetExpections() or AddRawExpectations(). The
  // returned pointer remains owned by this object.
  TestURLRequestContext* GetURLRequestContext();

  const TestNetworkDelegate& network_delegate() const {
    return network_delegate_;
  }

 private:
  WebSocketMockClientSocketFactoryMaker maker_;
  TestURLRequestContext url_request_context_;
  TestNetworkDelegate network_delegate_;
  std::unique_ptr<ProxyResolutionService> proxy_resolution_service_;
  bool url_request_context_initialized_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketTestURLRequestContextHost);
};

// WebSocketStream::ConnectDelegate implementation that does nothing.
class DummyConnectDelegate : public WebSocketStream::ConnectDelegate {
 public:
  DummyConnectDelegate() = default;
  ~DummyConnectDelegate() override = default;
  void OnCreateRequest(URLRequest* url_request) override {}
  void OnSuccess(std::unique_ptr<WebSocketStream> stream) override {}
  void OnFailure(const std::string& message) override {}
  void OnStartOpeningHandshake(
      std::unique_ptr<WebSocketHandshakeRequestInfo> request) override {}
  void OnFinishOpeningHandshake(
      std::unique_ptr<WebSocketHandshakeResponseInfo> response) override {}
  void OnSSLCertificateError(
      std::unique_ptr<WebSocketEventInterface::SSLErrorCallbacks>
          ssl_error_callbacks,
      int net_error,
      const SSLInfo& ssl_info,
      bool fatal) override {}
  int OnAuthRequired(const AuthChallengeInfo& auth_info,
                     scoped_refptr<HttpResponseHeaders> response_headers,
                     const IPEndPoint& remote_endpoint,
                     base::OnceCallback<void(const AuthCredentials*)> callback,
                     base::Optional<AuthCredentials>* credentials) override;
};

// WebSocketStreamRequestAPI implementation that sets the value of
// Sec-WebSocket-Key to the deterministic key that is used by tests.
class TestWebSocketStreamRequestAPI : public WebSocketStreamRequestAPI {
 public:
  TestWebSocketStreamRequestAPI() = default;
  ~TestWebSocketStreamRequestAPI() override = default;
  void OnBasicHandshakeStreamCreated(
      WebSocketBasicHandshakeStream* handshake_stream) override;
  void OnHttp2HandshakeStreamCreated(
      WebSocketHttp2HandshakeStream* handshake_stream) override;
  void OnFailure(const std::string& message) override {}
};

// A sub-class of WebSocketHandshakeStreamCreateHelper which sets a
// deterministic key to use in the WebSocket handshake, and uses a dummy
// ConnectDelegate and WebSocketStreamRequestAPI.
class TestWebSocketHandshakeStreamCreateHelper
    : public WebSocketHandshakeStreamCreateHelper {
 public:
  // Constructor for using dummy ConnectDelegate and WebSocketStreamRequestAPI.
  TestWebSocketHandshakeStreamCreateHelper()
      : WebSocketHandshakeStreamCreateHelper(&connect_delegate_,
                                             /* requested_subprotocols = */ {},
                                             &request_) {}

  ~TestWebSocketHandshakeStreamCreateHelper() override = default;

 private:
  DummyConnectDelegate connect_delegate_;
  TestWebSocketStreamRequestAPI request_;

  DISALLOW_COPY_AND_ASSIGN(TestWebSocketHandshakeStreamCreateHelper);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_TEST_UTIL_H_
