// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_basic_handshake_stream.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/stream_socket.h"
#include "net/socket/websocket_endpoint_lock_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/websockets/websocket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
namespace {

TEST(WebSocketBasicHandshakeStreamTest, ConnectionClosedOnFailure) {
  std::string request = WebSocketStandardRequest(
      "/", "www.example.org",
      url::Origin::Create(GURL("http://origin.example.org")),
      /*send_additional_request_headers=*/{}, /*extra_headers=*/{});
  std::string response =
      "HTTP/1.1 404 Not Found\r\n"
      "Content-Length: 0\r\n"
      "\r\n";
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, 0, request.c_str())};
  MockRead reads[] = {MockRead(SYNCHRONOUS, 1, response.c_str()),
                      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2)};
  IPEndPoint end_point(IPAddress(127, 0, 0, 1), 80);
  SequencedSocketData sequenced_socket_data(
      MockConnect(SYNCHRONOUS, OK, end_point), reads, writes);
  auto socket = std::make_unique<MockTCPClientSocket>(
      AddressList(end_point), nullptr, &sequenced_socket_data);
  const int connect_result = socket->Connect(CompletionOnceCallback());
  EXPECT_EQ(connect_result, OK);
  const MockTCPClientSocket* const socket_ptr = socket.get();
  auto handle = std::make_unique<ClientSocketHandle>();
  handle->SetSocket(std::move(socket));
  DummyConnectDelegate delegate;
  WebSocketEndpointLockManager endpoint_lock_manager;
  TestWebSocketStreamRequestAPI stream_request_api;
  std::vector<std::string> extensions = {
      "permessage-deflate; client_max_window_bits"};
  WebSocketBasicHandshakeStream basic_handshake_stream(
      std::move(handle), &delegate, false, {}, extensions, &stream_request_api,
      &endpoint_lock_manager);
  basic_handshake_stream.SetWebSocketKeyForTesting("dGhlIHNhbXBsZSBub25jZQ==");
  HttpRequestInfo request_info;
  request_info.url = GURL("ws://www.example.com/");
  request_info.method = "GET";
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  TestCompletionCallback callback1;
  NetLogWithSource net_log;
  basic_handshake_stream.RegisterRequest(&request_info);
  const int result1 =
      callback1.GetResult(basic_handshake_stream.InitializeStream(
          true, LOWEST, net_log, callback1.callback()));
  EXPECT_EQ(result1, OK);

  auto request_headers = WebSocketCommonTestHeaders();
  HttpResponseInfo response_info;
  TestCompletionCallback callback2;
  const int result2 = callback2.GetResult(basic_handshake_stream.SendRequest(
      request_headers, &response_info, callback2.callback()));
  EXPECT_EQ(result2, OK);

  TestCompletionCallback callback3;
  const int result3 = callback3.GetResult(
      basic_handshake_stream.ReadResponseHeaders(callback2.callback()));
  EXPECT_EQ(result3, ERR_INVALID_RESPONSE);

  EXPECT_FALSE(socket_ptr->IsConnected());
}

TEST(WebSocketBasicHandshakeStreamTest, DnsAliasesCanBeAccessed) {
  std::string request = WebSocketStandardRequest(
      "/", "www.example.org",
      url::Origin::Create(GURL("http://origin.example.org")),
      /*send_additional_request_headers=*/{}, /*extra_headers=*/{});
  std::string response = WebSocketStandardResponse("");
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, 0, request.c_str())};
  MockRead reads[] = {MockRead(SYNCHRONOUS, 1, response.c_str()),
                      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2)};

  IPEndPoint end_point(IPAddress(127, 0, 0, 1), 80);
  SequencedSocketData sequenced_socket_data(
      MockConnect(SYNCHRONOUS, OK, end_point), reads, writes);
  auto socket = std::make_unique<MockTCPClientSocket>(
      AddressList(end_point), nullptr, &sequenced_socket_data);
  const int connect_result = socket->Connect(CompletionOnceCallback());
  EXPECT_EQ(connect_result, OK);

  std::set<std::string> aliases({"alias1", "alias2", "www.example.org"});
  socket->SetDnsAliases(aliases);
  EXPECT_THAT(
      socket->GetDnsAliases(),
      testing::UnorderedElementsAre("alias1", "alias2", "www.example.org"));

  const MockTCPClientSocket* const socket_ptr = socket.get();
  auto handle = std::make_unique<ClientSocketHandle>();
  handle->SetSocket(std::move(socket));
  EXPECT_THAT(
      handle->socket()->GetDnsAliases(),
      testing::UnorderedElementsAre("alias1", "alias2", "www.example.org"));

  DummyConnectDelegate delegate;
  WebSocketEndpointLockManager endpoint_lock_manager;
  TestWebSocketStreamRequestAPI stream_request_api;
  std::vector<std::string> extensions = {
      "permessage-deflate; client_max_window_bits"};
  WebSocketBasicHandshakeStream basic_handshake_stream(
      std::move(handle), &delegate, false, {}, extensions, &stream_request_api,
      &endpoint_lock_manager);
  basic_handshake_stream.SetWebSocketKeyForTesting("dGhlIHNhbXBsZSBub25jZQ==");
  HttpRequestInfo request_info;
  request_info.url = GURL("ws://www.example.com/");
  request_info.method = "GET";
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  TestCompletionCallback callback1;
  NetLogWithSource net_log;
  basic_handshake_stream.RegisterRequest(&request_info);
  const int result1 =
      callback1.GetResult(basic_handshake_stream.InitializeStream(
          true, LOWEST, net_log, callback1.callback()));
  EXPECT_EQ(result1, OK);

  auto request_headers = WebSocketCommonTestHeaders();
  HttpResponseInfo response_info;
  TestCompletionCallback callback2;
  const int result2 = callback2.GetResult(basic_handshake_stream.SendRequest(
      request_headers, &response_info, callback2.callback()));
  EXPECT_EQ(result2, OK);

  TestCompletionCallback callback3;
  const int result3 = callback3.GetResult(
      basic_handshake_stream.ReadResponseHeaders(callback2.callback()));
  EXPECT_EQ(result3, OK);

  EXPECT_TRUE(socket_ptr->IsConnected());

  EXPECT_THAT(basic_handshake_stream.GetDnsAliases(),
              testing::ElementsAre("alias1", "alias2", "www.example.org"));
}

}  // namespace
}  // namespace net
