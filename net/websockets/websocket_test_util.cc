// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/websockets/websocket_test_util.h"

#include <stddef.h>

#include <algorithm>
#include <sstream>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/net_errors.h"
#include "net/http/http_network_session.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/socket/socket_test_util.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/websockets/websocket_basic_handshake_stream.h"
#include "url/origin.h"

namespace net {
class AuthChallengeInfo;
class AuthCredentials;
class HttpResponseHeaders;
class WebSocketHttp2HandshakeStream;
class WebSocketHttp3HandshakeStream;

namespace {

const uint64_t kA = (static_cast<uint64_t>(0x5851f42d) << 32) +
                    static_cast<uint64_t>(0x4c957f2d);
const uint64_t kC = 12345;
const uint64_t kM = static_cast<uint64_t>(1) << 48;

}  // namespace

LinearCongruentialGenerator::LinearCongruentialGenerator(uint32_t seed)
    : current_(seed) {}

uint32_t LinearCongruentialGenerator::Generate() {
  uint64_t result = current_;
  current_ = (current_ * kA + kC) % kM;
  return static_cast<uint32_t>(result >> 16);
}

std::string WebSocketExtraHeadersToString(
    const WebSocketExtraHeaders& headers) {
  std::string answer;
  for (const auto& header : headers) {
    base::StrAppend(&answer, {header.first, ": ", header.second, "\r\n"});
  }
  return answer;
}

HttpRequestHeaders WebSocketExtraHeadersToHttpRequestHeaders(
    const WebSocketExtraHeaders& headers) {
  HttpRequestHeaders headers_to_return;
  for (const auto& header : headers)
    headers_to_return.SetHeader(header.first, header.second);
  return headers_to_return;
}

std::string WebSocketStandardRequest(
    const std::string& path,
    const std::string& host,
    const url::Origin& origin,
    const WebSocketExtraHeaders& send_additional_request_headers,
    const WebSocketExtraHeaders& extra_headers) {
  return WebSocketStandardRequestWithCookies(path, host, origin, /*cookies=*/{},
                                             send_additional_request_headers,
                                             extra_headers);
}

std::string WebSocketStandardRequestWithCookies(
    const std::string& path,
    const std::string& host,
    const url::Origin& origin,
    const WebSocketExtraHeaders& cookies,
    const WebSocketExtraHeaders& send_additional_request_headers,
    const WebSocketExtraHeaders& extra_headers) {
  // Unrelated changes in net/http may change the order and default-values of
  // HTTP headers, causing WebSocket tests to fail. It is safe to update this
  // in that case.
  HttpRequestHeaders headers;
  std::stringstream request_headers;

  request_headers << base::StringPrintf("GET %s HTTP/1.1\r\n", path.c_str());
  headers.SetHeader("Host", host);
  headers.SetHeader("Connection", "Upgrade");
  headers.SetHeader("Pragma", "no-cache");
  headers.SetHeader("Cache-Control", "no-cache");
  for (const auto& [key, value] : send_additional_request_headers)
    headers.SetHeader(key, value);
  headers.SetHeader("Upgrade", "websocket");
  headers.SetHeader("Origin", origin.Serialize());
  headers.SetHeader("Sec-WebSocket-Version", "13");
  if (!headers.HasHeader("User-Agent"))
    headers.SetHeader("User-Agent", "");
  headers.SetHeader("Accept-Encoding", "gzip, deflate");
  headers.SetHeader("Accept-Language", "en-us,fr");
  for (const auto& [key, value] : cookies)
    headers.SetHeader(key, value);
  headers.SetHeader("Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==");
  headers.SetHeader("Sec-WebSocket-Extensions",
                    "permessage-deflate; client_max_window_bits");
  for (const auto& [key, value] : extra_headers)
    headers.SetHeader(key, value);

  request_headers << headers.ToString();
  return request_headers.str();
}

std::string WebSocketStandardResponse(const std::string& extra_headers) {
  return base::StrCat(
      {"HTTP/1.1 101 Switching Protocols\r\n"
       "Upgrade: websocket\r\n"
       "Connection: Upgrade\r\n"
       "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n",
       extra_headers, "\r\n"});
}

HttpRequestHeaders WebSocketCommonTestHeaders() {
  HttpRequestHeaders request_headers;
  request_headers.SetHeader("Host", "www.example.org");
  request_headers.SetHeader("Connection", "Upgrade");
  request_headers.SetHeader("Pragma", "no-cache");
  request_headers.SetHeader("Cache-Control", "no-cache");
  request_headers.SetHeader("Upgrade", "websocket");
  request_headers.SetHeader("Origin", "http://origin.example.org");
  request_headers.SetHeader("Sec-WebSocket-Version", "13");
  request_headers.SetHeader("User-Agent", "");
  request_headers.SetHeader("Accept-Encoding", "gzip, deflate");
  request_headers.SetHeader("Accept-Language", "en-us,fr");
  return request_headers;
}

quiche::HttpHeaderBlock WebSocketHttp2Request(
    const std::string& path,
    const std::string& authority,
    const std::string& origin,
    const WebSocketExtraHeaders& extra_headers) {
  quiche::HttpHeaderBlock request_headers;
  request_headers[spdy::kHttp2MethodHeader] = "CONNECT";
  request_headers[spdy::kHttp2AuthorityHeader] = authority;
  request_headers[spdy::kHttp2SchemeHeader] = "https";
  request_headers[spdy::kHttp2PathHeader] = path;
  request_headers[spdy::kHttp2ProtocolHeader] = "websocket";
  request_headers["pragma"] = "no-cache";
  request_headers["cache-control"] = "no-cache";
  request_headers["origin"] = origin;
  request_headers["sec-websocket-version"] = "13";
  request_headers["user-agent"] = "";
  request_headers["accept-encoding"] = "gzip, deflate";
  request_headers["accept-language"] = "en-us,fr";
  request_headers["sec-websocket-extensions"] =
      "permessage-deflate; client_max_window_bits";
  for (const auto& header : extra_headers) {
    request_headers[base::ToLowerASCII(header.first)] = header.second;
  }
  return request_headers;
}

quiche::HttpHeaderBlock WebSocketHttp2Response(
    const WebSocketExtraHeaders& extra_headers) {
  quiche::HttpHeaderBlock response_headers;
  response_headers[spdy::kHttp2StatusHeader] = "200";
  for (const auto& header : extra_headers) {
    response_headers[base::ToLowerASCII(header.first)] = header.second;
  }
  return response_headers;
}

struct WebSocketMockClientSocketFactoryMaker::Detail {
  std::string expect_written;
  std::string return_to_read;
  std::vector<MockRead> reads;
  MockWrite write;
  std::vector<std::unique_ptr<SequencedSocketData>> socket_data_vector;
  std::vector<std::unique_ptr<SSLSocketDataProvider>> ssl_socket_data_vector;
  MockClientSocketFactory factory;
};

WebSocketMockClientSocketFactoryMaker::WebSocketMockClientSocketFactoryMaker()
    : detail_(std::make_unique<Detail>()) {}

WebSocketMockClientSocketFactoryMaker::
    ~WebSocketMockClientSocketFactoryMaker() = default;

MockClientSocketFactory* WebSocketMockClientSocketFactoryMaker::factory() {
  return &detail_->factory;
}

void WebSocketMockClientSocketFactoryMaker::SetExpectations(
    const std::string& expect_written,
    const std::string& return_to_read) {
  constexpr size_t kHttpStreamParserBufferSize = 4096;
  // We need to extend the lifetime of these strings.
  detail_->expect_written = expect_written;
  detail_->return_to_read = return_to_read;
  int sequence = 0;
  detail_->write = MockWrite(SYNCHRONOUS,
                             detail_->expect_written.data(),
                             detail_->expect_written.size(),
                             sequence++);
  // HttpStreamParser reads 4KB at a time. We need to take this implementation
  // detail into account if |return_to_read| is big enough.
  for (size_t place = 0; place < detail_->return_to_read.size();
       place += kHttpStreamParserBufferSize) {
    detail_->reads.emplace_back(SYNCHRONOUS,
                                detail_->return_to_read.data() + place,
                                std::min(detail_->return_to_read.size() - place,
                                         kHttpStreamParserBufferSize),
                                sequence++);
  }
  auto socket_data = std::make_unique<SequencedSocketData>(
      detail_->reads, base::make_span(&detail_->write, 1u));
  socket_data->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  AddRawExpectations(std::move(socket_data));
}

void WebSocketMockClientSocketFactoryMaker::AddRawExpectations(
    std::unique_ptr<SequencedSocketData> socket_data) {
  detail_->factory.AddSocketDataProvider(socket_data.get());
  detail_->socket_data_vector.push_back(std::move(socket_data));
}

void WebSocketMockClientSocketFactoryMaker::AddSSLSocketDataProvider(
    std::unique_ptr<SSLSocketDataProvider> ssl_socket_data) {
  detail_->factory.AddSSLSocketDataProvider(ssl_socket_data.get());
  detail_->ssl_socket_data_vector.push_back(std::move(ssl_socket_data));
}

WebSocketTestURLRequestContextHost::WebSocketTestURLRequestContextHost()
    : url_request_context_builder_(CreateTestURLRequestContextBuilder()) {
  url_request_context_builder_->set_client_socket_factory_for_testing(
      maker_.factory());
  HttpNetworkSessionParams params;
  params.enable_spdy_ping_based_connection_checking = false;
  params.enable_quic = false;
  params.disable_idle_sockets_close_on_memory_pressure = false;
  url_request_context_builder_->set_http_network_session_params(params);
}

WebSocketTestURLRequestContextHost::~WebSocketTestURLRequestContextHost() =
    default;

void WebSocketTestURLRequestContextHost::AddRawExpectations(
    std::unique_ptr<SequencedSocketData> socket_data) {
  maker_.AddRawExpectations(std::move(socket_data));
}

void WebSocketTestURLRequestContextHost::AddSSLSocketDataProvider(
    std::unique_ptr<SSLSocketDataProvider> ssl_socket_data) {
  maker_.AddSSLSocketDataProvider(std::move(ssl_socket_data));
}

void WebSocketTestURLRequestContextHost::SetProxyConfig(
    const std::string& proxy_rules) {
  DCHECK(!url_request_context_);
  auto proxy_resolution_service =
      ConfiguredProxyResolutionService::CreateFixedForTest(
          proxy_rules, TRAFFIC_ANNOTATION_FOR_TESTS);
  url_request_context_builder_->set_proxy_resolution_service(
      std::move(proxy_resolution_service));
}

void DummyConnectDelegate::OnURLRequestConnected(URLRequest* request,
                                                 const TransportInfo& info) {}

int DummyConnectDelegate::OnAuthRequired(
    const AuthChallengeInfo& auth_info,
    scoped_refptr<HttpResponseHeaders> response_headers,
    const IPEndPoint& host_port_pair,
    base::OnceCallback<void(const AuthCredentials*)> callback,
    std::optional<AuthCredentials>* credentials) {
  return OK;
}

URLRequestContext* WebSocketTestURLRequestContextHost::GetURLRequestContext() {
  if (!url_request_context_) {
    url_request_context_builder_->set_network_delegate(
        std::make_unique<TestNetworkDelegate>());
    url_request_context_ = url_request_context_builder_->Build();
    url_request_context_builder_ = nullptr;
  }
  return url_request_context_.get();
}

void TestWebSocketStreamRequestAPI::OnBasicHandshakeStreamCreated(
    WebSocketBasicHandshakeStream* handshake_stream) {
  handshake_stream->SetWebSocketKeyForTesting("dGhlIHNhbXBsZSBub25jZQ==");
}

void TestWebSocketStreamRequestAPI::OnHttp2HandshakeStreamCreated(
    WebSocketHttp2HandshakeStream* handshake_stream) {}

void TestWebSocketStreamRequestAPI::OnHttp3HandshakeStreamCreated(
    WebSocketHttp3HandshakeStream* handshake_stream) {}
}  // namespace net
