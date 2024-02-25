// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_proxy_datagram_client_socket.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/test_proxy_delegate.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_http_utils.h"
#include "net/quic/quic_proxy_client_socket_test_base.h"
#include "net/quic/test_quic_crypto_client_config_handle.h"
#include "net/quic/test_task_runner.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/qpack/qpack_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace net::test {

class QuicProxyDatagramClientSocketTest : public QuicProxyClientSocketTestBase {
 public:
  void TearDown() override {
    sock_.reset();
    EXPECT_TRUE(mock_quic_data_.AllReadDataConsumed());
    EXPECT_TRUE(mock_quic_data_.AllWriteDataConsumed());
  }

  void InitializeClientSocket() override {
    sock_ = std::make_unique<QuicProxyDatagramClientSocket>(
        destination_endpoint_.GetURL(), user_agent_,
        NetLogWithSource::Make(NetLogSourceType::NONE));
    session_->StartReading();
  }

  void PopulateConnectRequestIR(spdy::Http2HeaderBlock* block) override {
    DCHECK(destination_endpoint_.scheme() == url::kHttpsScheme);

    std::string host = destination_endpoint_.host();
    uint16_t port = destination_endpoint_.port();

    (*block)[":method"] = "CONNECT";
    (*block)[":protocol"] = "connect-udp";
    (*block)[":scheme"] = destination_endpoint_.scheme();
    // Port is removed if 443 since that is the default port number for HTTPS.
    (*block)[":authority"] =
        port != 443 ? base::StrCat({host, ":", base::NumberToString(port)})
                    : host;
    (*block)[":path"] = "/";
    (*block)["capsule-protocol"] = "?1";
  }

  void AssertConnectSucceeds() override {
    TestCompletionCallback callback;
    ASSERT_THAT(
        sock_->ConnectViaStream(local_addr_, peer_addr_,
                                std::move(stream_handle_), callback.callback()),
        IsError(ERR_IO_PENDING));
    ASSERT_THAT(callback.WaitForResult(), IsOk());
  }

  void AssertConnectFails(int result) override {
    TestCompletionCallback callback;
    ASSERT_THAT(
        sock_->ConnectViaStream(local_addr_, peer_addr_,
                                std::move(stream_handle_), callback.callback()),
        IsError(ERR_IO_PENDING));
    ASSERT_EQ(result, callback.WaitForResult());
  }

  void AssertWriteReturns(const char* data, int len, int rv) override {
    CHECK(false);
  }

  void AssertSyncWriteSucceeds(const char* data, int len) override {
    CHECK(false);
  }

  void AssertSyncReadEquals(const char* data, int len) override {
    CHECK(false);
  }

  void AssertAsyncReadEquals(const char* data, int len) override {
    CHECK(false);
  }

  void AssertReadStarts(const char* data, int len) override { CHECK(false); }

  void AssertReadReturns(const char* data, int len) override { CHECK(false); }

 protected:
  std::unique_ptr<QuicProxyDatagramClientSocket> sock_;
};

TEST_P(QuicProxyDatagramClientSocketTest, ConnectSendsCorrectRequest) {
  int packet_number = 1;

  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ConstructServerConnectReplyPacket(1, !kFin));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(packet_number++,
                                            quic::QUIC_STREAM_CANCELLED, 1, 1));

  InitializeSession();
  InitializeClientSocket();

  ASSERT_FALSE(sock_->IsConnected());

  AssertConnectSucceeds();

  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != nullptr);
  ASSERT_EQ(200, response->headers->response_code());
}

TEST_P(QuicProxyDatagramClientSocketTest, ConnectFails) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(ASYNC, ERR_CONNECTION_CLOSED);

  InitializeSession();
  InitializeClientSocket();

  ASSERT_FALSE(sock_->IsConnected());

  AssertConnectFails(ERR_QUIC_PROTOCOL_ERROR);

  ASSERT_FALSE(sock_->IsConnected());
}

INSTANTIATE_TEST_SUITE_P(VersionIncludeStreamDependencySequence,
                         QuicProxyDatagramClientSocketTest,
                         ::testing::ValuesIn(AllSupportedQuicVersions()),
                         ::testing::PrintToStringParamName());

}  // namespace net::test
