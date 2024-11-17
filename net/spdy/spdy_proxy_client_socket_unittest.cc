// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_proxy_client_socket.h"

#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_timing_info.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/session_usage.h"
#include "net/base/test_completion_callback.h"
#include "net/base/winsock_init.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_proxy_connect_job.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/connect_job_params.h"
#include "net/socket/connect_job_test_util.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/ssl_connect_job.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket/transport_connect_job.h"
#include "net/spdy/buffered_spdy_framer.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

using net::test::IsError;
using net::test::IsOk;

//-----------------------------------------------------------------------------

namespace net {

namespace {

static const char kRequestUrl[] = "https://www.google.com/";
static const char kOriginHost[] = "www.google.com";
static const int kOriginPort = 443;
static const char kOriginHostPort[] = "www.google.com:443";
static const char kProxyUrl[] = "https://myproxy:6121/";
static const char kProxyHost[] = "myproxy";
static const int kProxyPort = 6121;
static const char kUserAgent[] = "Mozilla/1.0";

static const int kStreamId = 1;

static const char kMsg1[] = "\0hello!\xff";
static const int kLen1 = 8;
static const char kMsg2[] = "\0a2345678\0";
static const int kLen2 = 10;
static const char kMsg3[] = "bye!";
static const int kLen3 = 4;
static const char kMsg33[] = "bye!bye!";
static const int kLen33 = kLen3 + kLen3;
static const char kMsg333[] = "bye!bye!bye!";
static const int kLen333 = kLen3 + kLen3 + kLen3;

static const char kRedirectUrl[] = "https://example.com/";

// Creates a SpdySession with a StreamSocket, instead of a ClientSocketHandle.
base::WeakPtr<SpdySession> CreateSpdyProxySession(
    const url::SchemeHostPort& destination,
    HttpNetworkSession* http_session,
    const SpdySessionKey& key,
    const CommonConnectJobParams* common_connect_job_params) {
  EXPECT_FALSE(http_session->spdy_session_pool()->FindAvailableSession(
      key, true /* enable_ip_based_pooling */, false /* is_websocket */,
      NetLogWithSource()));

  auto transport_params = base::MakeRefCounted<TransportSocketParams>(
      destination, NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
      OnHostResolutionCallback(),
      /*supported_alpns=*/base::flat_set<std::string>{"h2", "http/1.1"});

  SSLConfig ssl_config;
  ssl_config.privacy_mode = key.privacy_mode();
  auto ssl_params = base::MakeRefCounted<SSLSocketParams>(
      ConnectJobParams(transport_params),
      HostPortPair::FromSchemeHostPort(destination), ssl_config,
      key.network_anonymization_key());
  TestConnectJobDelegate connect_job_delegate;
  SSLConnectJob connect_job(MEDIUM, SocketTag(), common_connect_job_params,
                            ssl_params, &connect_job_delegate,
                            nullptr /* net_log */);
  connect_job_delegate.StartJobExpectingResult(&connect_job, OK,
                                               false /* expect_sync_result */);

  base::expected<base::WeakPtr<SpdySession>, int> spdy_session_result =
      http_session->spdy_session_pool()->CreateAvailableSessionFromSocket(
          key, connect_job_delegate.ReleaseSocket(),
          LoadTimingInfo::ConnectTiming(), NetLogWithSource());
  // Failure is reported asynchronously.
  EXPECT_TRUE(spdy_session_result.has_value());
  EXPECT_TRUE(HasSpdySession(http_session->spdy_session_pool(), key));
  return spdy_session_result.value();
}

}  // namespace

class SpdyProxyClientSocketTest : public PlatformTest,
                                  public WithTaskEnvironment,
                                  public ::testing::WithParamInterface<bool> {
 public:
  SpdyProxyClientSocketTest();

  SpdyProxyClientSocketTest(const SpdyProxyClientSocketTest&) = delete;
  SpdyProxyClientSocketTest& operator=(const SpdyProxyClientSocketTest&) =
      delete;

  ~SpdyProxyClientSocketTest() override;

  void TearDown() override;

 protected:
  void Initialize(base::span<const MockRead> reads,
                  base::span<const MockWrite> writes);
  void PopulateConnectRequestIR(quiche::HttpHeaderBlock* syn_ir);
  void PopulateConnectReplyIR(quiche::HttpHeaderBlock* block,
                              const char* status);
  spdy::SpdySerializedFrame ConstructConnectRequestFrame(
      RequestPriority priority = LOWEST);
  spdy::SpdySerializedFrame ConstructConnectAuthRequestFrame();
  spdy::SpdySerializedFrame ConstructConnectReplyFrame();
  spdy::SpdySerializedFrame ConstructConnectAuthReplyFrame();
  spdy::SpdySerializedFrame ConstructConnectRedirectReplyFrame();
  spdy::SpdySerializedFrame ConstructConnectErrorReplyFrame();
  spdy::SpdySerializedFrame ConstructBodyFrame(const char* data,
                                               int length,
                                               bool fin = false);
  scoped_refptr<IOBufferWithSize> CreateBuffer(const char* data, int size);
  void AssertConnectSucceeds();
  void AssertConnectFails(int result);
  void AssertConnectionEstablished();
  void AssertSyncReadEquals(const char* data, int len);
  void AssertSyncReadEOF();
  void AssertAsyncReadEquals(const char* data, int len, bool fin = false);
  void AssertReadStarts(const char* data, int len);
  void AssertReadReturns(const char* data, int len);
  void AssertAsyncWriteSucceeds(const char* data, int len);
  void AssertWriteReturns(const char* data, int len, int rv);
  void AssertWriteLength(int len);

  void AddAuthToCache() {
    const std::u16string kFoo(u"foo");
    const std::u16string kBar(u"bar");
    session_->http_auth_cache()->Add(
        url::SchemeHostPort{GURL(kProxyUrl)}, HttpAuth::AUTH_PROXY, "MyRealm1",
        HttpAuth::AUTH_SCHEME_BASIC, NetworkAnonymizationKey(),
        "Basic realm=MyRealm1", AuthCredentials(kFoo, kBar), "/");
  }

  void ResumeAndRun() {
    // Run until the pause, if the provider isn't paused yet.
    data_->RunUntilPaused();
    data_->Resume();
    base::RunLoop().RunUntilIdle();
  }

  void CloseSpdySession(Error error, const std::string& description) {
    spdy_session_->CloseSessionOnError(error, description);
  }

  // Whether to use net::Socket::ReadIfReady() instead of net::Socket::Read().
  bool use_read_if_ready() const { return GetParam(); }

 protected:
  NetLogWithSource net_log_with_source_{
      NetLogWithSource::Make(NetLogSourceType::NONE)};
  RecordingNetLogObserver net_log_observer_;

  scoped_refptr<IOBuffer> read_buf_;
  SpdySessionDependencies session_deps_;
  std::unique_ptr<HttpNetworkSession> session_;
  MockConnect connect_data_;
  base::WeakPtr<SpdySession> spdy_session_;
  std::string user_agent_;
  GURL url_;
  HostPortPair proxy_host_port_;
  HostPortPair endpoint_host_port_pair_;
  ProxyChain proxy_chain_;
  SpdySessionKey endpoint_spdy_session_key_;
  std::unique_ptr<CommonConnectJobParams> common_connect_job_params_;
  SSLSocketDataProvider ssl_;

  SpdyTestUtil spdy_util_;
  std::unique_ptr<SpdyProxyClientSocket> sock_;
  TestCompletionCallback read_callback_;
  TestCompletionCallback write_callback_;
  std::unique_ptr<SequencedSocketData> data_;
};

SpdyProxyClientSocketTest::SpdyProxyClientSocketTest()
    : connect_data_(SYNCHRONOUS, OK),
      user_agent_(kUserAgent),
      url_(kRequestUrl),
      proxy_host_port_(kProxyHost, kProxyPort),
      endpoint_host_port_pair_(kOriginHost, kOriginPort),
      proxy_chain_(ProxyServer::SCHEME_HTTPS, proxy_host_port_),
      endpoint_spdy_session_key_(
          endpoint_host_port_pair_,
          PRIVACY_MODE_DISABLED,
          proxy_chain_,
          SessionUsage::kDestination,
          SocketTag(),
          NetworkAnonymizationKey(),
          SecureDnsPolicy::kAllow,
          /*disable_cert_verification_network_fetches=*/false),
      ssl_(SYNCHRONOUS, OK) {
  session_deps_.net_log = NetLog::Get();
}

SpdyProxyClientSocketTest::~SpdyProxyClientSocketTest() {
  if (data_) {
    EXPECT_TRUE(data_->AllWriteDataConsumed());
    EXPECT_TRUE(data_->AllReadDataConsumed());
  }
}

void SpdyProxyClientSocketTest::TearDown() {
  if (session_)
    session_->spdy_session_pool()->CloseAllSessions();

  // Empty the current queue.
  base::RunLoop().RunUntilIdle();
  PlatformTest::TearDown();
}

void SpdyProxyClientSocketTest::Initialize(base::span<const MockRead> reads,
                                           base::span<const MockWrite> writes) {
  data_ = std::make_unique<SequencedSocketData>(reads, writes);
  data_->set_connect_data(connect_data_);
  session_deps_.socket_factory->AddSocketDataProvider(data_.get());

  ssl_.ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  ASSERT_TRUE(ssl_.ssl_info.cert);
  ssl_.next_proto = NextProto::kProtoHTTP2;
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl_);

  session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);
  common_connect_job_params_ = std::make_unique<CommonConnectJobParams>(
      session_->CreateCommonConnectJobParams());

  // Creates the SPDY session and stream.
  spdy_session_ = CreateSpdyProxySession(
      url::SchemeHostPort(url_), session_.get(), endpoint_spdy_session_key_,
      common_connect_job_params_.get());

  base::WeakPtr<SpdyStream> spdy_stream(
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM, spdy_session_, url_,
                                LOWEST, net_log_with_source_));
  ASSERT_TRUE(spdy_stream.get() != nullptr);

  // Create the SpdyProxyClientSocket.
  sock_ = std::make_unique<SpdyProxyClientSocket>(
      spdy_stream, proxy_chain_, /*proxy_chain_index=*/0, user_agent_,
      endpoint_host_port_pair_, net_log_with_source_,
      base::MakeRefCounted<HttpAuthController>(
          HttpAuth::AUTH_PROXY, GURL("https://" + proxy_host_port_.ToString()),
          NetworkAnonymizationKey(), session_->http_auth_cache(),
          session_->http_auth_handler_factory(), session_->host_resolver()),
      // Testing with the proxy delegate is in HttpProxyConnectJobTest.
      nullptr);
}

scoped_refptr<IOBufferWithSize> SpdyProxyClientSocketTest::CreateBuffer(
    const char* data, int size) {
  scoped_refptr<IOBufferWithSize> buf =
      base::MakeRefCounted<IOBufferWithSize>(size);
  memcpy(buf->data(), data, size);
  return buf;
}

void SpdyProxyClientSocketTest::AssertConnectSucceeds() {
  ASSERT_THAT(sock_->Connect(read_callback_.callback()),
              IsError(ERR_IO_PENDING));
  ASSERT_THAT(read_callback_.WaitForResult(), IsOk());
}

void SpdyProxyClientSocketTest::AssertConnectFails(int result) {
  ASSERT_THAT(sock_->Connect(read_callback_.callback()),
              IsError(ERR_IO_PENDING));
  ASSERT_EQ(result, read_callback_.WaitForResult());
}

void SpdyProxyClientSocketTest::AssertConnectionEstablished() {
  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != nullptr);
  ASSERT_EQ(200, response->headers->response_code());
  // Although the underlying HTTP/2 connection uses TLS and negotiates ALPN, the
  // tunnel itself is a TCP connection to the origin and should not report these
  // values.
  net::SSLInfo ssl_info;
  EXPECT_FALSE(sock_->GetSSLInfo(&ssl_info));
  EXPECT_EQ(sock_->GetNegotiatedProtocol(), NextProto::kProtoUnknown);
}

void SpdyProxyClientSocketTest::AssertSyncReadEquals(const char* data,
                                                     int len) {
  auto buf = base::MakeRefCounted<IOBufferWithSize>(len);
  if (use_read_if_ready()) {
    ASSERT_EQ(len,
              sock_->ReadIfReady(buf.get(), len, CompletionOnceCallback()));
  } else {
    ASSERT_EQ(len, sock_->Read(buf.get(), len, CompletionOnceCallback()));
  }
  ASSERT_EQ(std::string(data, len), std::string(buf->data(), len));
  ASSERT_TRUE(sock_->IsConnected());
}

void SpdyProxyClientSocketTest::AssertSyncReadEOF() {
  if (use_read_if_ready()) {
    ASSERT_EQ(0, sock_->ReadIfReady(nullptr, 1, read_callback_.callback()));
  } else {
    ASSERT_EQ(0, sock_->Read(nullptr, 1, read_callback_.callback()));
  }
}

void SpdyProxyClientSocketTest::AssertAsyncReadEquals(const char* data,
                                                      int len,
                                                      bool fin) {
  // Issue the read, which will be completed asynchronously
  auto buf = base::MakeRefCounted<IOBufferWithSize>(len);
  if (use_read_if_ready()) {
    ASSERT_EQ(ERR_IO_PENDING,
              sock_->ReadIfReady(buf.get(), len, read_callback_.callback()));
  } else {
    ASSERT_EQ(ERR_IO_PENDING,
              sock_->Read(buf.get(), len, read_callback_.callback()));
  }
  EXPECT_TRUE(sock_->IsConnected());

  ResumeAndRun();

  if (use_read_if_ready()) {
    EXPECT_EQ(OK, read_callback_.WaitForResult());
    ASSERT_EQ(len,
              sock_->ReadIfReady(buf.get(), len, read_callback_.callback()));
  } else {
    EXPECT_EQ(len, read_callback_.WaitForResult());
  }

  if (fin) {
    EXPECT_FALSE(sock_->IsConnected());
  } else {
    EXPECT_TRUE(sock_->IsConnected());
  }

  ASSERT_EQ(std::string(data, len), std::string(buf->data(), len));
}

void SpdyProxyClientSocketTest::AssertReadStarts(const char* data, int len) {
  // Issue the read, which will be completed asynchronously.
  read_buf_ = base::MakeRefCounted<IOBufferWithSize>(len);
  if (use_read_if_ready()) {
    ASSERT_EQ(ERR_IO_PENDING, sock_->ReadIfReady(read_buf_.get(), len,
                                                 read_callback_.callback()));
  } else {
    ASSERT_EQ(ERR_IO_PENDING,
              sock_->Read(read_buf_.get(), len, read_callback_.callback()));
  }
  EXPECT_TRUE(sock_->IsConnected());
}

void SpdyProxyClientSocketTest::AssertReadReturns(const char* data, int len) {
  EXPECT_TRUE(sock_->IsConnected());

  // Now the read will return
  if (use_read_if_ready()) {
    EXPECT_EQ(OK, read_callback_.WaitForResult());
    ASSERT_EQ(len, sock_->ReadIfReady(read_buf_.get(), len,
                                      read_callback_.callback()));
  } else {
    EXPECT_EQ(len, read_callback_.WaitForResult());
  }
  ASSERT_EQ(std::string(data, len), std::string(read_buf_->data(), len));
}

void SpdyProxyClientSocketTest::AssertAsyncWriteSucceeds(const char* data,
                                                              int len) {
  AssertWriteReturns(data, len, ERR_IO_PENDING);
  AssertWriteLength(len);
}

void SpdyProxyClientSocketTest::AssertWriteReturns(const char* data,
                                                   int len,
                                                   int rv) {
  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(data, len));
  EXPECT_EQ(rv, sock_->Write(buf.get(), buf->size(), write_callback_.callback(),
                             TRAFFIC_ANNOTATION_FOR_TESTS));
}

void SpdyProxyClientSocketTest::AssertWriteLength(int len) {
  EXPECT_EQ(len, write_callback_.WaitForResult());
}

void SpdyProxyClientSocketTest::PopulateConnectRequestIR(
    quiche::HttpHeaderBlock* block) {
  (*block)[spdy::kHttp2MethodHeader] = "CONNECT";
  (*block)[spdy::kHttp2AuthorityHeader] = kOriginHostPort;
  (*block)["user-agent"] = kUserAgent;
}

void SpdyProxyClientSocketTest::PopulateConnectReplyIR(
    quiche::HttpHeaderBlock* block,
    const char* status) {
  (*block)[spdy::kHttp2StatusHeader] = status;
}

// Constructs a standard SPDY HEADERS frame for a CONNECT request.
spdy::SpdySerializedFrame
SpdyProxyClientSocketTest::ConstructConnectRequestFrame(
    RequestPriority priority) {
  quiche::HttpHeaderBlock block;
  PopulateConnectRequestIR(&block);
  return spdy_util_.ConstructSpdyHeaders(kStreamId, std::move(block), priority,
                                         false);
}

// Constructs a SPDY HEADERS frame for a CONNECT request which includes
// Proxy-Authorization headers.
spdy::SpdySerializedFrame
SpdyProxyClientSocketTest::ConstructConnectAuthRequestFrame() {
  quiche::HttpHeaderBlock block;
  PopulateConnectRequestIR(&block);
  block["proxy-authorization"] = "Basic Zm9vOmJhcg==";
  return spdy_util_.ConstructSpdyHeaders(kStreamId, std::move(block), LOWEST,
                                         false);
}

// Constructs a standard SPDY HEADERS frame to match the SPDY CONNECT.
spdy::SpdySerializedFrame
SpdyProxyClientSocketTest::ConstructConnectReplyFrame() {
  quiche::HttpHeaderBlock block;
  PopulateConnectReplyIR(&block, "200");
  return spdy_util_.ConstructSpdyReply(kStreamId, std::move(block));
}

// Constructs a standard SPDY HEADERS frame to match the SPDY CONNECT,
// including Proxy-Authenticate headers.
spdy::SpdySerializedFrame
SpdyProxyClientSocketTest::ConstructConnectAuthReplyFrame() {
  quiche::HttpHeaderBlock block;
  PopulateConnectReplyIR(&block, "407");
  block["proxy-authenticate"] = "Basic realm=\"MyRealm1\"";
  return spdy_util_.ConstructSpdyReply(kStreamId, std::move(block));
}

// Constructs a SPDY HEADERS frame with an HTTP 302 redirect.
spdy::SpdySerializedFrame
SpdyProxyClientSocketTest::ConstructConnectRedirectReplyFrame() {
  quiche::HttpHeaderBlock block;
  PopulateConnectReplyIR(&block, "302");
  block["location"] = kRedirectUrl;
  block["set-cookie"] = "foo=bar";
  return spdy_util_.ConstructSpdyReply(kStreamId, std::move(block));
}

// Constructs a SPDY HEADERS frame with an HTTP 500 error.
spdy::SpdySerializedFrame
SpdyProxyClientSocketTest::ConstructConnectErrorReplyFrame() {
  quiche::HttpHeaderBlock block;
  PopulateConnectReplyIR(&block, "500");
  return spdy_util_.ConstructSpdyReply(kStreamId, std::move(block));
}

spdy::SpdySerializedFrame SpdyProxyClientSocketTest::ConstructBodyFrame(
    const char* data,
    int length,
    bool fin) {
  return spdy_util_.ConstructSpdyDataFrame(kStreamId,
                                           std::string_view(data, length), fin);
}

// ----------- Connect

INSTANTIATE_TEST_SUITE_P(All,
                         SpdyProxyClientSocketTest,
                         ::testing::Bool());

TEST_P(SpdyProxyClientSocketTest, ConnectSendsCorrectRequest) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, writes);

  ASSERT_FALSE(sock_->IsConnected());

  AssertConnectSucceeds();

  AssertConnectionEstablished();
}

TEST_P(SpdyProxyClientSocketTest, ConnectWithAuthRequested) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectAuthReplyFrame());
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, writes);

  AssertConnectFails(ERR_PROXY_AUTH_REQUESTED);

  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != nullptr);
  ASSERT_EQ(407, response->headers->response_code());
}

TEST_P(SpdyProxyClientSocketTest, ConnectWithAuthCredentials) {
  spdy::SpdySerializedFrame conn(ConstructConnectAuthRequestFrame());
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, writes);
  AddAuthToCache();

  AssertConnectSucceeds();

  AssertConnectionEstablished();
}

TEST_P(SpdyProxyClientSocketTest, ConnectRedirects) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectRedirectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, writes);

  AssertConnectFails(ERR_TUNNEL_CONNECTION_FAILED);

  const HttpResponseInfo* response = sock_->GetConnectResponseInfo();
  ASSERT_TRUE(response != nullptr);

  const HttpResponseHeaders* headers = response->headers.get();
  ASSERT_EQ(302, headers->response_code());
  ASSERT_TRUE(headers->HasHeader("set-cookie"));

  std::string location;
  ASSERT_TRUE(headers->IsRedirect(&location));
  ASSERT_EQ(location, kRedirectUrl);

  // Let the RST_STREAM write while |rst| is in-scope.
  base::RunLoop().RunUntilIdle();
}

TEST_P(SpdyProxyClientSocketTest, ConnectFails) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
    MockRead(ASYNC, 0, 1),  // EOF
  };

  Initialize(reads, writes);

  ASSERT_FALSE(sock_->IsConnected());

  AssertConnectFails(ERR_CONNECTION_CLOSED);

  ASSERT_FALSE(sock_->IsConnected());
}

TEST_P(SpdyProxyClientSocketTest, SetStreamPriority) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame(LOWEST));
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, writes);

  // Set the stream priority. Since a connection was already established, it's
  // too late to adjust the HTTP2 stream's priority, and the request is ignored.
  sock_->SetStreamPriority(HIGHEST);

  AssertConnectSucceeds();
}

// ----------- WasEverUsed

TEST_P(SpdyProxyClientSocketTest, WasEverUsedReturnsCorrectValues) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS), CreateMockWrite(rst, 3),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, writes);

  EXPECT_FALSE(sock_->WasEverUsed());
  AssertConnectSucceeds();
  EXPECT_TRUE(sock_->WasEverUsed());
  sock_->Disconnect();
  EXPECT_TRUE(sock_->WasEverUsed());

  // Let the RST_STREAM write while |rst| is in-scope.
  base::RunLoop().RunUntilIdle();
}

// ----------- GetPeerAddress

TEST_P(SpdyProxyClientSocketTest, GetPeerAddressReturnsCorrectValues) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      MockRead(ASYNC, 0, 3),  // EOF
  };

  Initialize(reads, writes);

  IPEndPoint addr;
  EXPECT_THAT(sock_->GetPeerAddress(&addr), IsError(ERR_SOCKET_NOT_CONNECTED));

  AssertConnectSucceeds();
  EXPECT_TRUE(sock_->IsConnected());
  EXPECT_THAT(sock_->GetPeerAddress(&addr), IsOk());

  ResumeAndRun();

  EXPECT_FALSE(sock_->IsConnected());
  EXPECT_THAT(sock_->GetPeerAddress(&addr), IsError(ERR_SOCKET_NOT_CONNECTED));

  sock_->Disconnect();

  EXPECT_THAT(sock_->GetPeerAddress(&addr), IsError(ERR_SOCKET_NOT_CONNECTED));
}

// ----------- Write

TEST_P(SpdyProxyClientSocketTest, WriteSendsDataInDataFrame) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  spdy::SpdySerializedFrame msg1(ConstructBodyFrame(kMsg1, kLen1));
  spdy::SpdySerializedFrame msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
      CreateMockWrite(msg1, 3, SYNCHRONOUS),
      CreateMockWrite(msg2, 4, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  AssertAsyncWriteSucceeds(kMsg1, kLen1);
  AssertAsyncWriteSucceeds(kMsg2, kLen2);
}

TEST_P(SpdyProxyClientSocketTest, WriteSplitsLargeDataIntoMultipleFrames) {
  std::string chunk_data(kMaxSpdyFrameChunkSize, 'x');
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  spdy::SpdySerializedFrame chunk(
      ConstructBodyFrame(chunk_data.data(), chunk_data.length()));
  MockWrite writes[] = {CreateMockWrite(conn, 0, SYNCHRONOUS),
                        CreateMockWrite(chunk, 3, SYNCHRONOUS),
                        CreateMockWrite(chunk, 4, SYNCHRONOUS),
                        CreateMockWrite(chunk, 5, SYNCHRONOUS)};

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  std::string big_data(kMaxSpdyFrameChunkSize * 3, 'x');
  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(big_data.data(),
                                                   big_data.length()));

  EXPECT_EQ(ERR_IO_PENDING,
            sock_->Write(buf.get(), buf->size(), write_callback_.callback(),
                         TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_EQ(buf->size(), write_callback_.WaitForResult());
}

// ----------- Read

TEST_P(SpdyProxyClientSocketTest, ReadReadsDataInDataFrame) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  spdy::SpdySerializedFrame msg1(ConstructBodyFrame(kMsg1, kLen1));
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(msg1, 3, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 4),
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  // SpdySession consumes the next read and sends it to sock_ to be buffered.
  ResumeAndRun();
  AssertSyncReadEquals(kMsg1, kLen1);
}

TEST_P(SpdyProxyClientSocketTest, ReadDataFromBufferedFrames) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  spdy::SpdySerializedFrame msg1(ConstructBodyFrame(kMsg1, kLen1));
  spdy::SpdySerializedFrame msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(msg1, 3, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 4),
      CreateMockRead(msg2, 5, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 6),
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  // SpdySession consumes the next read and sends it to sock_ to be buffered.
  ResumeAndRun();
  AssertSyncReadEquals(kMsg1, kLen1);
  // SpdySession consumes the next read and sends it to sock_ to be buffered.
  ResumeAndRun();
  AssertSyncReadEquals(kMsg2, kLen2);
}

TEST_P(SpdyProxyClientSocketTest, ReadDataMultipleBufferedFrames) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  spdy::SpdySerializedFrame msg1(ConstructBodyFrame(kMsg1, kLen1));
  spdy::SpdySerializedFrame msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC),
      MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(msg1, 3, ASYNC),
      CreateMockRead(msg2, 4, ASYNC),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 5),
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  // SpdySession consumes the next two reads and sends then to sock_ to be
  // buffered.
  ResumeAndRun();
  AssertSyncReadEquals(kMsg1, kLen1);
  AssertSyncReadEquals(kMsg2, kLen2);
}

TEST_P(SpdyProxyClientSocketTest, LargeReadWillMergeDataFromDifferentFrames) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  spdy::SpdySerializedFrame msg1(ConstructBodyFrame(kMsg1, kLen1));
  spdy::SpdySerializedFrame msg3(ConstructBodyFrame(kMsg3, kLen3));
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC),
      MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(msg3, 3, ASYNC),
      CreateMockRead(msg3, 4, ASYNC),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 5),
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  // SpdySession consumes the next two reads and sends then to sock_ to be
  // buffered.
  ResumeAndRun();
  // The payload from two data frames, each with kMsg3 will be combined
  // together into a single read().
  AssertSyncReadEquals(kMsg33, kLen33);
}

TEST_P(SpdyProxyClientSocketTest, MultipleShortReadsThenMoreRead) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  spdy::SpdySerializedFrame msg1(ConstructBodyFrame(kMsg1, kLen1));
  spdy::SpdySerializedFrame msg3(ConstructBodyFrame(kMsg3, kLen3));
  spdy::SpdySerializedFrame msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC),
      MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(msg1, 3, ASYNC),
      CreateMockRead(msg3, 4, ASYNC),
      CreateMockRead(msg3, 5, ASYNC),
      CreateMockRead(msg2, 6, ASYNC),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 7),
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  // SpdySession consumes the next four reads and sends then to sock_ to be
  // buffered.
  ResumeAndRun();
  AssertSyncReadEquals(kMsg1, kLen1);
  // The payload from two data frames, each with kMsg3 will be combined
  // together into a single read().
  AssertSyncReadEquals(kMsg33, kLen33);
  AssertSyncReadEquals(kMsg2, kLen2);
}

TEST_P(SpdyProxyClientSocketTest, ReadWillSplitDataFromLargeFrame) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  spdy::SpdySerializedFrame msg1(ConstructBodyFrame(kMsg1, kLen1));
  spdy::SpdySerializedFrame msg33(ConstructBodyFrame(kMsg33, kLen33));
  spdy::SpdySerializedFrame msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC),
      MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(msg1, 3, ASYNC),
      CreateMockRead(msg33, 4, ASYNC),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 5),
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  // SpdySession consumes the next two reads and sends then to sock_ to be
  // buffered.
  ResumeAndRun();
  AssertSyncReadEquals(kMsg1, kLen1);
  // The payload from the single large data frame will be read across
  // two different reads.
  AssertSyncReadEquals(kMsg3, kLen3);
  AssertSyncReadEquals(kMsg3, kLen3);
}

TEST_P(SpdyProxyClientSocketTest, MultipleReadsFromSameLargeFrame) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  spdy::SpdySerializedFrame msg333(ConstructBodyFrame(kMsg333, kLen333));
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(msg333, 3, ASYNC),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 4),
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  // SpdySession consumes the next read and sends it to sock_ to be buffered.
  ResumeAndRun();
  // The payload from the single large data frame will be read across
  // two different reads.
  AssertSyncReadEquals(kMsg33, kLen33);

  // Now attempt to do a read of more data than remains buffered
  AssertSyncReadEquals(kMsg3, kLen3);

  ASSERT_TRUE(sock_->IsConnected());
}

TEST_P(SpdyProxyClientSocketTest, ReadAuthResponseBody) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectAuthReplyFrame());
  spdy::SpdySerializedFrame msg1(ConstructBodyFrame(kMsg1, kLen1));
  spdy::SpdySerializedFrame msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC),
      MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(msg1, 3, ASYNC),
      CreateMockRead(msg2, 4, ASYNC),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 5),
  };

  Initialize(reads, writes);

  AssertConnectFails(ERR_PROXY_AUTH_REQUESTED);

  // SpdySession consumes the next two reads and sends then to sock_ to be
  // buffered.
  ResumeAndRun();
  AssertSyncReadEquals(kMsg1, kLen1);
  AssertSyncReadEquals(kMsg2, kLen2);
}

TEST_P(SpdyProxyClientSocketTest, ReadErrorResponseBody) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectErrorReplyFrame());
  spdy::SpdySerializedFrame msg1(ConstructBodyFrame(kMsg1, kLen1));
  spdy::SpdySerializedFrame msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), CreateMockRead(msg1, 2, SYNCHRONOUS),
      CreateMockRead(msg2, 3, SYNCHRONOUS), MockRead(SYNCHRONOUS, 0, 4),  // EOF
  };

  Initialize(reads, writes);

  AssertConnectFails(ERR_TUNNEL_CONNECTION_FAILED);
}

TEST_P(SpdyProxyClientSocketTest, SocketDestroyedWhenReadIsPending) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {CreateMockWrite(conn, 0, SYNCHRONOUS)};

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  spdy::SpdySerializedFrame msg1(ConstructBodyFrame(kMsg1, kLen1));
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), CreateMockRead(msg1, 2, ASYNC),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 3),
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  // Make Read()/ReadIfReady() pending.
  AssertReadStarts(kMsg1, kLen1);

  // Destroying socket.
  sock_ = nullptr;

  // Read data is not consumed.
  EXPECT_TRUE(data_->AllWriteDataConsumed());
  EXPECT_FALSE(data_->AllReadDataConsumed());

  // Reset |data_| so the test destructor doesn't check it.
  data_ = nullptr;
}

// ----------- Reads and Writes

TEST_P(SpdyProxyClientSocketTest, AsyncReadAroundWrite) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  spdy::SpdySerializedFrame msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
      CreateMockWrite(msg2, 4, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  spdy::SpdySerializedFrame msg1(ConstructBodyFrame(kMsg1, kLen1));
  spdy::SpdySerializedFrame msg3(ConstructBodyFrame(kMsg3, kLen3));
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC),
      MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(msg1, 3, ASYNC),  // sync read
      MockRead(ASYNC, ERR_IO_PENDING, 5),
      CreateMockRead(msg3, 6, ASYNC),  // async read
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 7),
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  ResumeAndRun();
  AssertSyncReadEquals(kMsg1, kLen1);

  AssertReadStarts(kMsg3, kLen3);
  // Read should block until after the write succeeds.

  AssertAsyncWriteSucceeds(kMsg2, kLen2);  // Advances past paused read.

  ASSERT_FALSE(read_callback_.have_result());
  ResumeAndRun();
  // Now the read will return.
  AssertReadReturns(kMsg3, kLen3);
}

TEST_P(SpdyProxyClientSocketTest, AsyncWriteAroundReads) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  spdy::SpdySerializedFrame msg2(ConstructBodyFrame(kMsg2, kLen2));
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
      MockWrite(ASYNC, ERR_IO_PENDING, 7), CreateMockWrite(msg2, 8, ASYNC),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  spdy::SpdySerializedFrame msg1(ConstructBodyFrame(kMsg1, kLen1));
  spdy::SpdySerializedFrame msg3(ConstructBodyFrame(kMsg3, kLen3));
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(msg1, 3, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 4),
      CreateMockRead(msg3, 5, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 6),
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  ResumeAndRun();
  AssertSyncReadEquals(kMsg1, kLen1);
  // Write should block until the read completes
  AssertWriteReturns(kMsg2, kLen2, ERR_IO_PENDING);

  AssertAsyncReadEquals(kMsg3, kLen3);

  ASSERT_FALSE(write_callback_.have_result());

  // Now the write will complete
  ResumeAndRun();
  AssertWriteLength(kLen2);
}

// ----------- Reading/Writing on Closed socket

// Reading from an already closed socket should return 0
TEST_P(SpdyProxyClientSocketTest, ReadOnClosedSocketReturnsZero) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      MockRead(ASYNC, 0, 3),  // EOF
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  ResumeAndRun();

  ASSERT_FALSE(sock_->IsConnected());
  AssertSyncReadEOF();
  AssertSyncReadEOF();
  AssertSyncReadEOF();
  ASSERT_FALSE(sock_->IsConnectedAndIdle());
}

// Read pending when socket is closed should return 0
TEST_P(SpdyProxyClientSocketTest, PendingReadOnCloseReturnsZero) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      MockRead(ASYNC, 0, 3),  // EOF
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  AssertReadStarts(kMsg1, kLen1);

  ResumeAndRun();

  ASSERT_EQ(0, read_callback_.WaitForResult());
}

// Reading from a disconnected socket is an error
TEST_P(SpdyProxyClientSocketTest, ReadOnDisconnectSocketReturnsNotConnected) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS), CreateMockWrite(rst, 3),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  sock_->Disconnect();

  if (use_read_if_ready()) {
    ASSERT_EQ(ERR_SOCKET_NOT_CONNECTED,
              sock_->ReadIfReady(nullptr, 1, CompletionOnceCallback()));
  } else {
    ASSERT_EQ(ERR_SOCKET_NOT_CONNECTED,
              sock_->Read(nullptr, 1, CompletionOnceCallback()));
  }

  // Let the RST_STREAM write while |rst| is in-scope.
  base::RunLoop().RunUntilIdle();
}

// Reading buffered data from an already closed socket should return
// buffered data, then 0.
TEST_P(SpdyProxyClientSocketTest, ReadOnClosedSocketReturnsBufferedData) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  spdy::SpdySerializedFrame msg1(ConstructBodyFrame(kMsg1, kLen1));
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(msg1, 3, ASYNC), MockRead(ASYNC, 0, 4),  // EOF
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  ResumeAndRun();

  ASSERT_FALSE(sock_->IsConnected());
  auto buf = base::MakeRefCounted<IOBufferWithSize>(kLen1);
  ASSERT_EQ(kLen1, sock_->Read(buf.get(), kLen1, CompletionOnceCallback()));
  ASSERT_EQ(std::string(kMsg1, kLen1), std::string(buf->data(), kLen1));

  ASSERT_EQ(0, sock_->Read(nullptr, 1, CompletionOnceCallback()));
  ASSERT_EQ(0, sock_->Read(nullptr, 1, CompletionOnceCallback()));
  sock_->Disconnect();
  ASSERT_EQ(ERR_SOCKET_NOT_CONNECTED,
            sock_->Read(nullptr, 1, CompletionOnceCallback()));
}

// Calling Write() on a closed socket is an error
TEST_P(SpdyProxyClientSocketTest, WriteOnClosedStream) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  spdy::SpdySerializedFrame msg1(ConstructBodyFrame(kMsg1, kLen1));
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      MockRead(ASYNC, 0, 3),  // EOF
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  // Read EOF which will close the stream.
  ResumeAndRun();
  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(kMsg1, kLen1));
  EXPECT_EQ(ERR_SOCKET_NOT_CONNECTED,
            sock_->Write(buf.get(), buf->size(), CompletionOnceCallback(),
                         TRAFFIC_ANNOTATION_FOR_TESTS));
}

// Calling Write() on a disconnected socket is an error.
TEST_P(SpdyProxyClientSocketTest, WriteOnDisconnectedSocket) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS), CreateMockWrite(rst, 3),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  spdy::SpdySerializedFrame msg1(ConstructBodyFrame(kMsg1, kLen1));
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  sock_->Disconnect();

  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(kMsg1, kLen1));
  EXPECT_EQ(ERR_SOCKET_NOT_CONNECTED,
            sock_->Write(buf.get(), buf->size(), CompletionOnceCallback(),
                         TRAFFIC_ANNOTATION_FOR_TESTS));

  // Let the RST_STREAM write while |rst| is in-scope.
  base::RunLoop().RunUntilIdle();
}

// If the socket is closed with a pending Write(), the callback
// should be called with ERR_CONNECTION_CLOSED.
TEST_P(SpdyProxyClientSocketTest, WritePendingOnClose) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
      MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 3),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(kMsg1, kLen1));
  EXPECT_EQ(ERR_IO_PENDING,
            sock_->Write(buf.get(), buf->size(), write_callback_.callback(),
                         TRAFFIC_ANNOTATION_FOR_TESTS));
  // Make sure the write actually starts.
  base::RunLoop().RunUntilIdle();

  CloseSpdySession(ERR_ABORTED, std::string());

  EXPECT_THAT(write_callback_.WaitForResult(), IsError(ERR_CONNECTION_CLOSED));
}

// If the socket is Disconnected with a pending Write(), the callback
// should not be called.
TEST_P(SpdyProxyClientSocketTest, DisconnectWithWritePending) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS), CreateMockWrite(rst, 3),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  scoped_refptr<IOBufferWithSize> buf(CreateBuffer(kMsg1, kLen1));
  EXPECT_EQ(ERR_IO_PENDING,
            sock_->Write(buf.get(), buf->size(), write_callback_.callback(),
                         TRAFFIC_ANNOTATION_FOR_TESTS));

  sock_->Disconnect();

  EXPECT_FALSE(sock_->IsConnected());
  EXPECT_FALSE(write_callback_.have_result());

  // Let the RST_STREAM write while |rst| is in-scope.
  base::RunLoop().RunUntilIdle();
}

// If the socket is Disconnected with a pending Read(), the callback
// should not be called.
TEST_P(SpdyProxyClientSocketTest, DisconnectWithReadPending) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS), CreateMockWrite(rst, 3),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 2),
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  auto buf = base::MakeRefCounted<IOBufferWithSize>(kLen1);
  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Read(buf.get(), kLen1, read_callback_.callback()));

  sock_->Disconnect();

  EXPECT_FALSE(sock_->IsConnected());
  EXPECT_FALSE(read_callback_.have_result());

  // Let the RST_STREAM write while |rst| is in-scope.
  base::RunLoop().RunUntilIdle();
}

// If the socket is Reset when both a read and write are pending,
// both should be called back.
TEST_P(SpdyProxyClientSocketTest, RstWithReadAndWritePending) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(rst, 3, ASYNC), MockRead(ASYNC, 0, 4)  // EOF
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(kLen1);
  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Read(read_buf.get(), kLen1, read_callback_.callback()));

  scoped_refptr<IOBufferWithSize> write_buf(CreateBuffer(kMsg1, kLen1));
  EXPECT_EQ(ERR_IO_PENDING, sock_->Write(write_buf.get(), write_buf->size(),
                                         write_callback_.callback(),
                                         TRAFFIC_ANNOTATION_FOR_TESTS));

  ResumeAndRun();

  EXPECT_TRUE(sock_.get());
  EXPECT_TRUE(read_callback_.have_result());
  EXPECT_TRUE(write_callback_.have_result());

  // Let the RST_STREAM write while |rst| is in-scope.
  base::RunLoop().RunUntilIdle();
}

// Makes sure the proxy client socket's source gets the expected NetLog events
// and only the expected NetLog events (No SpdySession events).
TEST_P(SpdyProxyClientSocketTest, NetLog) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS), CreateMockWrite(rst, 5),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  spdy::SpdySerializedFrame msg1(ConstructBodyFrame(kMsg1, kLen1));
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(msg1, 3, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 4),
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  // SpdySession consumes the next read and sends it to sock_ to be buffered.
  ResumeAndRun();
  AssertSyncReadEquals(kMsg1, kLen1);

  NetLogSource sock_source = sock_->NetLog().source();
  sock_.reset();

  auto entry_list = net_log_observer_.GetEntriesForSource(sock_source);

  ASSERT_EQ(entry_list.size(), 10u);
  EXPECT_TRUE(
      LogContainsBeginEvent(entry_list, 0, NetLogEventType::SOCKET_ALIVE));
  EXPECT_TRUE(LogContainsEvent(entry_list, 1,
                               NetLogEventType::HTTP2_PROXY_CLIENT_SESSION,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsBeginEvent(
      entry_list, 2, NetLogEventType::HTTP_TRANSACTION_TUNNEL_SEND_REQUEST));
  EXPECT_TRUE(LogContainsEvent(
      entry_list, 3, NetLogEventType::HTTP_TRANSACTION_SEND_TUNNEL_HEADERS,
      NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsEndEvent(
      entry_list, 4, NetLogEventType::HTTP_TRANSACTION_TUNNEL_SEND_REQUEST));
  EXPECT_TRUE(LogContainsBeginEvent(
      entry_list, 5, NetLogEventType::HTTP_TRANSACTION_TUNNEL_READ_HEADERS));
  EXPECT_TRUE(LogContainsEvent(
      entry_list, 6,
      NetLogEventType::HTTP_TRANSACTION_READ_TUNNEL_RESPONSE_HEADERS,
      NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsEndEvent(
      entry_list, 7, NetLogEventType::HTTP_TRANSACTION_TUNNEL_READ_HEADERS));
  EXPECT_TRUE(LogContainsEvent(entry_list, 8,
                               NetLogEventType::SOCKET_BYTES_RECEIVED,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(
      LogContainsEndEvent(entry_list, 9, NetLogEventType::SOCKET_ALIVE));

  // Let the RST_STREAM write while |rst| is in-scope.
  base::RunLoop().RunUntilIdle();
}

// A helper class that will delete |sock| when the callback is invoked.
class DeleteSockCallback : public TestCompletionCallbackBase {
 public:
  explicit DeleteSockCallback(std::unique_ptr<SpdyProxyClientSocket>* sock)
      : sock_(sock) {}

  DeleteSockCallback(const DeleteSockCallback&) = delete;
  DeleteSockCallback& operator=(const DeleteSockCallback&) = delete;

  ~DeleteSockCallback() override = default;

  CompletionOnceCallback callback() {
    return base::BindOnce(&DeleteSockCallback::OnComplete,
                          base::Unretained(this));
  }

 private:
  void OnComplete(int result) {
    sock_->reset(nullptr);
    SetResult(result);
  }

  raw_ptr<std::unique_ptr<SpdyProxyClientSocket>> sock_;
};

// If the socket is Reset when both a read and write are pending, and the
// read callback causes the socket to be deleted, the write callback should
// not be called.
TEST_P(SpdyProxyClientSocketTest, RstWithReadAndWritePendingDelete) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  spdy::SpdySerializedFrame rst(
      spdy_util_.ConstructSpdyRstStream(1, spdy::ERROR_CODE_CANCEL));
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(rst, 3, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 4),
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  EXPECT_TRUE(sock_->IsConnected());

  DeleteSockCallback read_callback(&sock_);

  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(kLen1);
  ASSERT_EQ(ERR_IO_PENDING,
            sock_->Read(read_buf.get(), kLen1, read_callback.callback()));

  scoped_refptr<IOBufferWithSize> write_buf(CreateBuffer(kMsg1, kLen1));
  EXPECT_EQ(ERR_IO_PENDING, sock_->Write(write_buf.get(), write_buf->size(),
                                         write_callback_.callback(),
                                         TRAFFIC_ANNOTATION_FOR_TESTS));

  ResumeAndRun();

  EXPECT_FALSE(sock_.get());
  EXPECT_TRUE(read_callback.have_result());
  EXPECT_FALSE(write_callback_.have_result());

  // Let the RST_STREAM write while |rst| is in-scope.
  base::RunLoop().RunUntilIdle();
}

// ----------- Canceling a ReadIfReady
TEST_P(SpdyProxyClientSocketTest, CancelReadIfReady) {
  // Not relevant if not ReadIfReady().
  if (!use_read_if_ready())
    return;

  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {CreateMockWrite(conn, 0, SYNCHRONOUS)};

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  spdy::SpdySerializedFrame msg1(ConstructBodyFrame(kMsg1, kLen1));
  spdy::SpdySerializedFrame msg3(ConstructBodyFrame(kMsg3, kLen3));
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), CreateMockRead(msg1, 2, ASYNC),
      CreateMockRead(msg3, 3, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 4),
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  AssertReadStarts(kMsg1, kLen1);
  EXPECT_EQ(OK, sock_->CancelReadIfReady());

  // Perform ReadIfReady again should succeed after cancelation.
  AssertReadStarts(kMsg1, kLen1);
  AssertReadReturns(kMsg1, kLen1);
  AssertReadStarts(kMsg3, kLen3);
  AssertReadReturns(kMsg3, kLen3);

  // Canceling ReadIfReady() when none is in progress is an no-op.
  EXPECT_EQ(OK, sock_->CancelReadIfReady());
}

// ----------- Handling END_STREAM from the peer

TEST_P(SpdyProxyClientSocketTest, HandleEndStreamAsEOF) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  spdy::SpdySerializedFrame end_stream(
      ConstructBodyFrame(/*data=*/nullptr, /*length=*/0, /*fin=*/true));
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
      CreateMockWrite(end_stream, 7, SYNCHRONOUS),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  spdy::SpdySerializedFrame msg1(ConstructBodyFrame(kMsg1, kLen1));
  spdy::SpdySerializedFrame msg2(
      ConstructBodyFrame(kMsg2, kLen2, /*fin=*/true));
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(msg1, 3, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 4),
      CreateMockRead(msg2, 5, ASYNC), MockRead(SYNCHRONOUS, ERR_IO_PENDING, 6),
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  AssertAsyncReadEquals(kMsg1, kLen1);
  AssertAsyncReadEquals(kMsg2, kLen2, /*fin=*/true);
}

TEST_P(SpdyProxyClientSocketTest, SendEndStreamAfterWrite) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  spdy::SpdySerializedFrame write_msg(ConstructBodyFrame(kMsg1, kLen1));
  spdy::SpdySerializedFrame end_stream(
      ConstructBodyFrame(/*data=*/nullptr, /*length=*/0, /*fin=*/true));
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
      CreateMockWrite(write_msg, 4, ASYNC),
      CreateMockWrite(end_stream, 6, ASYNC),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  spdy::SpdySerializedFrame read_msg(
      ConstructBodyFrame(kMsg2, kLen2, /*fin=*/true));
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC),     MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(read_msg, 3, ASYNC), MockRead(ASYNC, ERR_IO_PENDING, 5),
      MockRead(SYNCHRONOUS, 0, 7),
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  AssertWriteReturns(kMsg1, kLen1, ERR_IO_PENDING);
  AssertAsyncReadEquals(kMsg2, kLen2, /*fin=*/false);
  ResumeAndRun();
  AssertWriteLength(kLen1);
  AssertSyncReadEOF();
}

// Regression test for https://crbug.com/1320256
TEST_P(SpdyProxyClientSocketTest, WriteAfterStreamEndSent) {
  spdy::SpdySerializedFrame conn(ConstructConnectRequestFrame());
  MockWrite writes[] = {
      CreateMockWrite(conn, 0, SYNCHRONOUS),
      // The following mock write blocks SpdyStream::SendData() in
      // SpdyProxyClientSocket::MaybeSendEndStream().
      MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 5),
  };

  spdy::SpdySerializedFrame resp(ConstructConnectReplyFrame());
  spdy::SpdySerializedFrame read_msg(
      ConstructBodyFrame(kMsg1, kLen1, /*fin=*/true));
  MockRead reads[] = {
      CreateMockRead(resp, 1, ASYNC),
      MockRead(ASYNC, ERR_IO_PENDING, 2),
      CreateMockRead(read_msg, 3, ASYNC),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 4),
  };

  Initialize(reads, writes);

  AssertConnectSucceeds();

  AssertAsyncReadEquals(kMsg1, kLen1);
  AssertWriteReturns(kMsg2, kLen2, ERR_CONNECTION_CLOSED);
}

}  // namespace net
