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
#include "net/base/proxy_string_util.h"
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
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/qpack/qpack_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_spdy_session_peer.h"
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

namespace {

constexpr char kTestHeaderName[] = "Foo";

}  // anonymous namespace

class EstablishedCryptoStream : public quic::test::MockQuicCryptoStream {
 public:
  using quic::test::MockQuicCryptoStream::MockQuicCryptoStream;

  bool encryption_established() const override { return true; }
};

class QuicProxyDatagramClientSocketTest : public QuicProxyClientSocketTestBase {
 public:
  void TearDown() override {
    sock_.reset();
    EXPECT_TRUE(mock_quic_data_.AllReadDataConsumed());
    EXPECT_TRUE(mock_quic_data_.AllWriteDataConsumed());
  }

  void InitializeClientSocket() override {
    sock_ = std::make_unique<QuicProxyDatagramClientSocket>(
        destination_endpoint_.GetURL(), proxy_chain_, user_agent_,
        NetLogWithSource::Make(NetLogSourceType::NONE), proxy_delegate_.get());
    session_->StartReading();
  }

  void PopulateConnectRequestIR(
      quiche::HttpHeaderBlock* block,
      std::optional<const HttpRequestHeaders> extra_headers) override {
    DCHECK(destination_endpoint_.scheme() == url::kHttpsScheme);

    std::string host = destination_endpoint_.host();
    uint16_t port = destination_endpoint_.port();

    (*block)[":scheme"] = destination_endpoint_.scheme();
    (*block)[":path"] = "/";
    (*block)[":protocol"] = "connect-udp";
    (*block)[":method"] = "CONNECT";
    // Port is removed if 443 since that is the default port number for HTTPS.
    (*block)[":authority"] =
        port != 443 ? base::StrCat({host, ":", base::NumberToString(port)})
                    : host;
    if (extra_headers) {
      HttpRequestHeaders::Iterator it(*extra_headers);
      while (it.GetNext()) {
        std::string name = base::ToLowerASCII(it.name());
        (*block)[name] = it.value();
      }
    }
    (*block)["user-agent"] = kUserAgent;
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
    auto buf = base::MakeRefCounted<IOBufferWithSize>(len);
    memcpy(buf->data(), data, len);
    EXPECT_EQ(rv,
              sock_->Write(buf.get(), buf->size(), write_callback_.callback(),
                           TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  void AssertSyncWriteSucceeds(const char* data, int len) override {
    auto buf = base::MakeRefCounted<IOBufferWithSize>(len);
    memcpy(buf->data(), data, len);
    EXPECT_EQ(len,
              sock_->Write(buf.get(), buf->size(), CompletionOnceCallback(),
                           TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  void AssertSyncReadEquals(const char* data, int len) override {
    auto buf = base::MakeRefCounted<IOBufferWithSize>(len);
    ASSERT_EQ(len, sock_->Read(buf.get(), len, CompletionOnceCallback()));
    ASSERT_EQ(std::string(data, len), std::string(buf->data(), len));
    ASSERT_TRUE(sock_->IsConnected());
  }

  void AssertAsyncReadEquals(const char* data, int len) override {
    CHECK(false);
  }

  void AssertReadStarts(const char* data, int len) override {
    read_buf_ = base::MakeRefCounted<IOBufferWithSize>(len);
    ASSERT_EQ(ERR_IO_PENDING,
              sock_->Read(read_buf_.get(), len, read_callback_.callback()));
    EXPECT_TRUE(sock_->IsConnected());
  }

  void AssertReadReturns(const char* data, int len) override {
    EXPECT_TRUE(sock_->IsConnected());

    // Now the read will return.
    EXPECT_EQ(len, read_callback_.WaitForResult());
    ASSERT_EQ(std::string(data, len), std::string(read_buf_->data(), len));
  }

 protected:
  std::unique_ptr<QuicProxyDatagramClientSocket> sock_;
};

TEST_P(QuicProxyDatagramClientSocketTest, ConnectSendsCorrectRequest) {
  int packet_number = 1;

  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(
      ASYNC, ConstructServerConnectReplyPacket(/*packet_number=*/1, !kFin));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(
                       packet_number++, quic::QUIC_STREAM_CANCELLED,
                       /*largest_received=*/1, /*smallest_received=*/1));

  InitializeSession();
  InitializeClientSocket();

  ASSERT_FALSE(sock_->IsConnected());

  AssertConnectSucceeds();

  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != nullptr);
  ASSERT_EQ(200, response->headers->response_code());
}

TEST_P(QuicProxyDatagramClientSocketTest, ProxyDelegateHeaders) {
  proxy_delegate_ = std::make_unique<TestProxyDelegate>();
  proxy_delegate_->set_extra_header_name(kTestHeaderName);

  // TestProxyDelegate sets the header value to the proxy server URI.
  HttpRequestHeaders extra_expected_headers;
  extra_expected_headers.SetHeader(kTestHeaderName,
                                   ProxyServerToProxyUri(proxy_chain_.Last()));

  // Include a header in the response that the ProxyDelegate should see.
  const char kResponseHeaderName[] = "bar";
  const char kResponseHeaderValue[] = "testing";
  HttpRequestHeaders response_headers;
  response_headers.SetHeader(kResponseHeaderName, kResponseHeaderValue);

  int packet_number = 1;

  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructConnectRequestPacket(packet_number++, extra_expected_headers));
  mock_quic_data_.AddRead(
      ASYNC, ConstructServerConnectReplyPacket(
                 /*packet_number=*/1, !kFin, /*header_length=*/nullptr,
                 response_headers));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(
                       packet_number++, quic::QUIC_STREAM_CANCELLED,
                       /*largest_received=*/1, /*smallest_received=*/1));

  InitializeSession();
  InitializeClientSocket();

  ASSERT_FALSE(sock_->IsConnected());

  AssertConnectSucceeds();

  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != nullptr);
  ASSERT_EQ(200, response->headers->response_code());

  proxy_delegate_->VerifyOnTunnelHeadersReceived(
      proxy_chain_, /*chain_index=*/0, kResponseHeaderName,
      kResponseHeaderValue);
}

TEST_P(QuicProxyDatagramClientSocketTest, ProxyDelegateFails) {
  proxy_delegate_ = std::make_unique<TestProxyDelegate>();
  proxy_delegate_->MakeOnTunnelHeadersReceivedFail(
      ERR_TUNNEL_CONNECTION_FAILED);

  int packet_number = 1;

  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(
      ASYNC, ConstructServerConnectReplyPacket(/*packet_number=*/1, !kFin));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(
                       packet_number++, quic::QUIC_STREAM_CANCELLED,
                       /*largest_received=*/1, /*smallest_received=*/1));

  InitializeSession();
  InitializeClientSocket();

  ASSERT_FALSE(sock_->IsConnected());

  AssertConnectFails(ERR_TUNNEL_CONNECTION_FAILED);
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

TEST_P(QuicProxyDatagramClientSocketTest, WriteSendsData) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(
      ASYNC, ConstructServerConnectReplyPacket(/*packet_number=*/1, !kFin));
  mock_quic_data_.AddReadPauseForever();

  std::string quarter_stream_id(1, '\0');
  std::string context_id(1, '\0');

  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructAckAndDatagramPacket(
          packet_number++, /*largest_received=*/1, /*smallest_received=*/1,
          {quarter_stream_id + context_id + std::string(kMsg1, kLen1)}));
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructDatagramPacket(packet_number++, {quarter_stream_id + context_id +
                                                std::string(kMsg2, kLen2)}));
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructRstPacket(packet_number++, quic::QUIC_STREAM_CANCELLED));

  InitializeSession();

  quic::test::QuicSpdySessionPeer::SetHttpDatagramSupport(
      session_.get(), quic::HttpDatagramSupport::kRfc);

  InitializeClientSocket();

  AssertConnectSucceeds();

  AssertSyncWriteSucceeds(kMsg1, kLen1);
  AssertSyncWriteSucceeds(kMsg2, kLen2);
}

TEST_P(QuicProxyDatagramClientSocketTest, WriteOnClosedSocket) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(
      ASYNC, ConstructServerConnectReplyPacket(/*packet_number=*/1, !kFin));
  mock_quic_data_.AddReadPauseForever();
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(
                       packet_number++, quic::QUIC_STREAM_CANCELLED,
                       /*largest_received=*/1, /*smallest_received=*/1));

  InitializeSession();
  InitializeClientSocket();

  AssertConnectSucceeds();

  sock_->Close();

  AssertWriteReturns(kMsg1, kLen1, ERR_SOCKET_NOT_CONNECTED);
}

TEST_P(QuicProxyDatagramClientSocketTest, OnHttp3DatagramAddsDatagram) {
  int packet_number = 1;

  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(
      ASYNC, ConstructServerConnectReplyPacket(/*packet_number=*/1, !kFin));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(
                       packet_number++, quic::QUIC_STREAM_CANCELLED,
                       /*largest_received=*/1, /*smallest_received=*/1));

  InitializeSession();

  quic::test::QuicSpdySessionPeer::SetHttpDatagramSupport(
      session_.get(), quic::HttpDatagramSupport::kRfc);

  InitializeClientSocket();

  AssertConnectSucceeds();

  sock_->OnHttp3Datagram(0, std::string(1, '\0') /* context_id */ +
                                std::string(kDatagramPayload, kDatagramLen));

  ASSERT_TRUE(!sock_->GetDatagramsForTesting().empty());
  ASSERT_EQ(sock_->GetDatagramsForTesting().front(), "youveGotMail");

  histogram_tester_.ExpectUniqueSample(
      QuicProxyDatagramClientSocket::kMaxQueueSizeHistogram, false, 1);
}

TEST_P(QuicProxyDatagramClientSocketTest, ReadReadsDataInQueue) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(
      ASYNC, ConstructServerConnectReplyPacket(/*packet_number=*/1, !kFin));
  mock_quic_data_.AddReadPause();

  mock_quic_data_.AddRead(
      ASYNC, ConstructServerDatagramPacket(
                 2, std::string(1, '\0') /* quarter_stream_id */ +
                        std::string(1, '\0') /* context_id */ +
                        std::string(kDatagramPayload,
                                    kDatagramLen)  // Actual message payload
                 ));
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckPacket(packet_number++, /*largest_received=*/2,
                                      /*smallest_received=*/1));
  mock_quic_data_.AddReadPauseForever();
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructRstPacket(packet_number++, quic::QUIC_STREAM_CANCELLED));

  InitializeSession();

  quic::test::QuicSpdySessionPeer::SetHttpDatagramSupport(
      session_.get(), quic::HttpDatagramSupport::kRfc);

  InitializeClientSocket();

  AssertConnectSucceeds();

  ResumeAndRun();
  AssertSyncReadEquals(kDatagramPayload, kDatagramLen);

  histogram_tester_.ExpectUniqueSample(
      QuicProxyDatagramClientSocket::kMaxQueueSizeHistogram, false, 1);
}

TEST_P(QuicProxyDatagramClientSocketTest, AsyncReadWhenQueueIsEmpty) {
  int packet_number = 1;
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(
      ASYNC, ConstructServerConnectReplyPacket(/*packet_number=*/1, !kFin));
  mock_quic_data_.AddReadPause();

  mock_quic_data_.AddRead(
      ASYNC, ConstructServerDatagramPacket(
                 2, std::string(1, '\0') /* quarter_stream_id */ +
                        std::string(1, '\0') /* context_id */ +
                        std::string(kDatagramPayload,
                                    kDatagramLen)  // Actual message payload
                 ));
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckPacket(packet_number++, /*largest_received=*/2,
                                      /*smallest_received=*/1));
  mock_quic_data_.AddReadPauseForever();
  mock_quic_data_.AddWrite(
      SYNCHRONOUS,
      ConstructRstPacket(packet_number++, quic::QUIC_STREAM_CANCELLED));

  InitializeSession();

  quic::test::QuicSpdySessionPeer::SetHttpDatagramSupport(
      session_.get(), quic::HttpDatagramSupport::kRfc);

  InitializeClientSocket();

  AssertConnectSucceeds();

  AssertReadStarts(kDatagramPayload, kDatagramLen);

  ResumeAndRun();

  EXPECT_TRUE(read_callback_.have_result());
  AssertReadReturns(kDatagramPayload, kDatagramLen);
}

TEST_P(QuicProxyDatagramClientSocketTest,
       MaxQueueLimitDiscardsIncomingDatagram) {
  int packet_number = 1;

  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructSettingsPacket(packet_number++));
  mock_quic_data_.AddWrite(SYNCHRONOUS,
                           ConstructConnectRequestPacket(packet_number++));
  mock_quic_data_.AddRead(
      ASYNC, ConstructServerConnectReplyPacket(/*packet_number=*/1, !kFin));
  mock_quic_data_.AddRead(SYNCHRONOUS, ERR_IO_PENDING);
  mock_quic_data_.AddWrite(
      SYNCHRONOUS, ConstructAckAndRstPacket(
                       packet_number++, quic::QUIC_STREAM_CANCELLED,
                       /*largest_received=*/1, /*smallest_received=*/1));

  InitializeSession();

  quic::test::QuicSpdySessionPeer::SetHttpDatagramSupport(
      session_.get(), quic::HttpDatagramSupport::kRfc);

  InitializeClientSocket();

  AssertConnectSucceeds();

  for (size_t i = 0;
       i < QuicProxyDatagramClientSocket::kMaxDatagramQueueSize + 1; i++) {
    sock_->OnHttp3Datagram(0, std::string(1, '\0') /* context_id */ +
                                  std::string(kDatagramPayload, kDatagramLen));
  }

  ASSERT_TRUE(sock_->GetDatagramsForTesting().size() ==
              QuicProxyDatagramClientSocket::kMaxDatagramQueueSize);

  histogram_tester_.ExpectTotalCount(
      QuicProxyDatagramClientSocket::kMaxQueueSizeHistogram,
      QuicProxyDatagramClientSocket::kMaxDatagramQueueSize + 1);
  histogram_tester_.ExpectBucketCount(
      QuicProxyDatagramClientSocket::kMaxQueueSizeHistogram, false,
      QuicProxyDatagramClientSocket::kMaxDatagramQueueSize);
  histogram_tester_.ExpectBucketCount(
      QuicProxyDatagramClientSocket::kMaxQueueSizeHistogram, true, 1);
}

INSTANTIATE_TEST_SUITE_P(VersionIncludeStreamDependencySequence,
                         QuicProxyDatagramClientSocketTest,
                         ::testing::ValuesIn(AllSupportedQuicVersions()),
                         ::testing::PrintToStringParamName());

}  // namespace net::test
