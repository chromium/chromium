// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_basic_state.h"

#include "base/memory/ptr_util.h"
#include "net/base/request_priority.h"
#include "net/http/http_request_info.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_handle.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(HttpBasicStateTest, ConstructsProperly) {
  ClientSocketHandle* const handle = new ClientSocketHandle;
  // Ownership of |handle| is passed to |state|.
  const HttpBasicState state(base::WrapUnique(handle), true /* using_proxy */);
  EXPECT_EQ(handle, state.connection());
  EXPECT_TRUE(state.using_proxy());
}

TEST(HttpBasicStateTest, ConstructsProperlyWithDifferentOptions) {
  const HttpBasicState state(std::make_unique<ClientSocketHandle>(),
                             false /* using_proxy */);
  EXPECT_FALSE(state.using_proxy());
}

TEST(HttpBasicStateTest, ReleaseConnectionWorks) {
  ClientSocketHandle* const handle = new ClientSocketHandle;
  // Ownership of |handle| is passed to |state|.
  HttpBasicState state(base::WrapUnique(handle), false);
  const std::unique_ptr<ClientSocketHandle> released_connection(
      state.ReleaseConnection());
  EXPECT_EQ(NULL, state.connection());
  EXPECT_EQ(handle, released_connection.get());
}

TEST(HttpBasicStateTest, InitializeWorks) {
  HttpBasicState state(std::make_unique<ClientSocketHandle>(), false);
  const HttpRequestInfo request_info;
  state.Initialize(&request_info, LOW, NetLogWithSource());
  EXPECT_TRUE(state.parser());
}

TEST(HttpBasicStateTest, TrafficAnnotationStored) {
  HttpBasicState state(std::make_unique<ClientSocketHandle>(), false);
  HttpRequestInfo request_info;
  request_info.traffic_annotation =
      MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS);
  state.Initialize(&request_info, LOW, NetLogWithSource());
  EXPECT_EQ(TRAFFIC_ANNOTATION_FOR_TESTS,
            NetworkTrafficAnnotationTag(state.traffic_annotation()));
}

TEST(HttpBasicStateTest, DeleteParser) {
  HttpBasicState state(std::make_unique<ClientSocketHandle>(), false);
  const HttpRequestInfo request_info;
  state.Initialize(&request_info, LOW, NetLogWithSource());
  EXPECT_TRUE(state.parser());
  state.DeleteParser();
  EXPECT_EQ(NULL, state.parser());
}

TEST(HttpBasicStateTest, GenerateRequestLineNoProxy) {
  const bool use_proxy = false;
  HttpBasicState state(std::make_unique<ClientSocketHandle>(), use_proxy);
  HttpRequestInfo request_info;
  request_info.url = GURL("http://www.example.com/path?foo=bar#hoge");
  request_info.method = "PUT";
  state.Initialize(&request_info, LOW, NetLogWithSource());
  EXPECT_EQ("PUT /path?foo=bar HTTP/1.1\r\n", state.GenerateRequestLine());
}

TEST(HttpBasicStateTest, GenerateRequestLineWithProxy) {
  const bool use_proxy = true;
  HttpBasicState state(std::make_unique<ClientSocketHandle>(), use_proxy);
  HttpRequestInfo request_info;
  request_info.url = GURL("http://www.example.com/path?foo=bar#hoge");
  request_info.method = "PUT";
  state.Initialize(&request_info, LOW, NetLogWithSource());
  EXPECT_EQ("PUT http://www.example.com/path?foo=bar HTTP/1.1\r\n",
            state.GenerateRequestLine());
}

}  // namespace
}  // namespace net
