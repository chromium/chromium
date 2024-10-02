// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/websockets/websocket_stream.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_samples.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/mock_timer.h"
#include "base/timer/timer.h"
#include "net/base/auth.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/base/url_util.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/ssl/ssl_info.h"
#include "net/storage_access_api/status.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"
#include "net/third_party/quiche/src/quiche/http2/test_tools/spdy_test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "net/websockets/websocket_frame.h"
#include "net/websockets/websocket_handshake_request_info.h"
#include "net/websockets/websocket_handshake_response_info.h"
#include "net/websockets/websocket_handshake_stream_base.h"
#include "net/websockets/websocket_stream_create_test_base.h"
#include "net/websockets/websocket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::net::test::IsError;
using ::net::test::IsOk;
using ::testing::TestWithParam;
using ::testing::Values;

namespace net {
namespace {

enum HandshakeStreamType { BASIC_HANDSHAKE_STREAM, HTTP2_HANDSHAKE_STREAM };

// Simple builder for a SequencedSocketData object to save repetitive code.
// It always sets the connect data to MockConnect(SYNCHRONOUS, OK), so it cannot
// be used in tests where the connect fails. In practice, those tests never have
// any read/write data and so can't benefit from it anyway.  The arrays are not
// copied. It is up to the caller to ensure they stay in scope until the test
// ends.
std::unique_ptr<SequencedSocketData> BuildSocketData(
    base::span<MockRead> reads,
    base::span<MockWrite> writes) {
  auto socket_data = std::make_unique<SequencedSocketData>(reads, writes);
  socket_data->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  return socket_data;
}

// Builder for a SequencedSocketData that expects nothing. This does not
// set the connect data, so the calling code must do that explicitly.
std::unique_ptr<SequencedSocketData> BuildNullSocketData() {
  return std::make_unique<SequencedSocketData>();
}

class MockWeakTimer : public base::MockOneShotTimer {
 public:
  MockWeakTimer() = default;

  base::WeakPtr<MockWeakTimer> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockWeakTimer> weak_ptr_factory_{this};
};

constexpr char kOrigin[] = "http://www.example.org";

static url::Origin Origin() {
  return url::Origin::Create(GURL(kOrigin));
}

static net::SiteForCookies SiteForCookies() {
  return net::SiteForCookies::FromOrigin(Origin());
}

static IsolationInfo CreateIsolationInfo() {
  url::Origin origin = Origin();
  return IsolationInfo::Create(IsolationInfo::RequestType::kOther, origin,
                               origin, SiteForCookies::FromOrigin(origin));
}

class WebSocketStreamCreateTest
    : public TestWithParam<std::tuple<HandshakeStreamType, bool>>,
      public WebSocketStreamCreateTestBase {
 protected:
  WebSocketStreamCreateTest()
      : stream_type_(std::get<HandshakeStreamType>(GetParam())),
        spdy_util_(/*use_priority_header=*/true) {
    // Make sure these tests all pass with connection partitioning enabled. The
    // disabled case is less interesting, and is tested more directly at lower
    // layers.
    if (PriorityHeaderEnabled()) {
      feature_list_.InitWithFeatures(
          {features::kPartitionConnectionsByNetworkIsolationKey,
           net::features::kPriorityHeader},
          {});
    } else {
      feature_list_.InitWithFeatures(
          {features::kPartitionConnectionsByNetworkIsolationKey},
          {net::features::kPriorityHeader});
    }
  }

  ~WebSocketStreamCreateTest() override {
    // Permit any endpoint locks to be released.
    stream_request_.reset();
    stream_.reset();
    base::RunLoop().RunUntilIdle();
  }

  // Normally it's easier to use CreateAndConnectRawExpectations() instead. This
  // method is only needed when multiple sockets are involved.
  void AddRawExpectations(std::unique_ptr<SequencedSocketData> socket_data) {
    url_request_context_host_.AddRawExpectations(std::move(socket_data));
  }

  void AddSSLData() {
    auto ssl_data = std::make_unique<SSLSocketDataProvider>(ASYNC, OK);
    ssl_data->ssl_info.cert =
        ImportCertFromFile(GetTestCertsDirectory(), "wildcard.pem");
    if (stream_type_ == HTTP2_HANDSHAKE_STREAM)
      ssl_data->next_proto = kProtoHTTP2;
    ASSERT_TRUE(ssl_data->ssl_info.cert.get());
    url_request_context_host_.AddSSLSocketDataProvider(std::move(ssl_data));
  }

  void SetTimer(std::unique_ptr<base::OneShotTimer> timer) {
    timer_ = std::move(timer);
  }

  void SetAdditionalResponseData(std::string additional_data) {
    additional_data_ = std::move(additional_data);
  }

  void SetHttp2ResponseStatus(const char* const http2_response_status) {
    http2_response_status_ = http2_response_status;
  }

  void SetResetWebSocketHttp2Stream(bool reset_websocket_http2_stream) {
    reset_websocket_http2_stream_ = reset_websocket_http2_stream;
  }

  // Set up mock data and start websockets request, either for WebSocket
  // upgraded from an HTTP/1 connection, or for a WebSocket request over HTTP/2.
  void CreateAndConnectStandard(
      std::string_view url,
      const std::vector<std::string>& sub_protocols,
      const WebSocketExtraHeaders& send_additional_request_headers,
      const WebSocketExtraHeaders& extra_request_headers,
      const WebSocketExtraHeaders& extra_response_headers,
      StorageAccessApiStatus storage_access_api_status =
          StorageAccessApiStatus::kNone) {
    const GURL socket_url(url);
    const std::string socket_host = GetHostAndOptionalPort(socket_url);
    const std::string socket_path = socket_url.path();

    if (stream_type_ == BASIC_HANDSHAKE_STREAM) {
      url_request_context_host_.SetExpectations(
          WebSocketStandardRequest(socket_path, socket_host, Origin(),
                                   send_additional_request_headers,
                                   extra_request_headers),
          WebSocketStandardResponse(
              WebSocketExtraHeadersToString(extra_response_headers)) +
              additional_data_);
      CreateAndConnectStream(socket_url, sub_protocols, Origin(),
                             SiteForCookies(), storage_access_api_status,
                             CreateIsolationInfo(),
                             WebSocketExtraHeadersToHttpRequestHeaders(
                                 send_additional_request_headers),
                             std::move(timer_));
      return;
    }

    DCHECK_EQ(stream_type_, HTTP2_HANDSHAKE_STREAM);

    // TODO(bnc): Find a way to clear
    // spdy_session_pool.enable_sending_initial_data_ to avoid sending
    // connection preface, initial settings, and window update.

    // HTTP/2 connection preface.
    frames_.emplace_back(spdy::test::MakeSerializedFrame(
        const_cast<char*>(spdy::kHttp2ConnectionHeaderPrefix),
        spdy::kHttp2ConnectionHeaderPrefixSize));
    AddWrite(&frames_.back());

    // Server advertises WebSockets over HTTP/2 support.
    spdy::SettingsMap read_settings;
    read_settings[spdy::SETTINGS_ENABLE_CONNECT_PROTOCOL] = 1;
    frames_.push_back(spdy_util_.ConstructSpdySettings(read_settings));
    AddRead(&frames_.back());

    // Initial SETTINGS frame.
    spdy::SettingsMap write_settings;
    write_settings[spdy::SETTINGS_HEADER_TABLE_SIZE] = kSpdyMaxHeaderTableSize;
    write_settings[spdy::SETTINGS_INITIAL_WINDOW_SIZE] = 6 * 1024 * 1024;
    write_settings[spdy::SETTINGS_MAX_HEADER_LIST_SIZE] =
        kSpdyMaxHeaderListSize;
    write_settings[spdy::SETTINGS_ENABLE_PUSH] = 0;
    frames_.push_back(spdy_util_.ConstructSpdySettings(write_settings));
    AddWrite(&frames_.back());

    // Initial window update frame.
    frames_.push_back(spdy_util_.ConstructSpdyWindowUpdate(0, 0x00ef0001));
    AddWrite(&frames_.back());

    // SETTINGS ACK sent as a response to server's SETTINGS frame.
    frames_.push_back(spdy_util_.ConstructSpdySettingsAck());
    AddWrite(&frames_.back());

    // First request.  This is necessary, because a WebSockets request currently
    // does not open a new HTTP/2 connection, it only uses an existing one.
    const char* const kExtraRequestHeaders[] = {
        "user-agent",      "",        "accept-encoding", "gzip, deflate",
        "accept-language", "en-us,fr"};
    frames_.push_back(spdy_util_.ConstructSpdyGet(
        kExtraRequestHeaders, std::size(kExtraRequestHeaders) / 2, 1,
        DEFAULT_PRIORITY));
    AddWrite(&frames_.back());

    // SETTINGS ACK frame sent by the server in response to the client's
    // initial SETTINGS frame.
    frames_.push_back(spdy_util_.ConstructSpdySettingsAck());
    AddRead(&frames_.back());

    // Response headers to first request.
    frames_.push_back(spdy_util_.ConstructSpdyGetReply(nullptr, 0, 1));
    AddRead(&frames_.back());

    // Response body to first request.
    frames_.push_back(spdy_util_.ConstructSpdyDataFrame(1, true));
    AddRead(&frames_.back());

    // First request is closed.
    spdy_util_.UpdateWithStreamDestruction(1);

    // WebSocket request.
    quiche::HttpHeaderBlock request_headers = WebSocketHttp2Request(
        socket_path, socket_host, kOrigin, extra_request_headers);
    frames_.push_back(spdy_util_.ConstructSpdyHeaders(
        3, std::move(request_headers), DEFAULT_PRIORITY, false));
    AddWrite(&frames_.back());

    if (reset_websocket_http2_stream_) {
      frames_.push_back(
          spdy_util_.ConstructSpdyRstStream(3, spdy::ERROR_CODE_CANCEL));
      AddRead(&frames_.back());
    } else {
      // Response to WebSocket request.
      std::vector<std::string> extra_response_header_keys;
      std::vector<const char*> extra_response_headers_vector;
      for (const auto& extra_header : extra_response_headers) {
        // Save a lowercase copy of the header key.
        extra_response_header_keys.push_back(
            base::ToLowerASCII(extra_header.first));
        // Save a pointer to this lowercase copy.
        extra_response_headers_vector.push_back(
            extra_response_header_keys.back().c_str());
        // Save a pointer to the original header value provided by the caller.
        extra_response_headers_vector.push_back(extra_header.second.c_str());
      }
      frames_.push_back(spdy_util_.ConstructSpdyReplyError(
          http2_response_status_, extra_response_headers_vector.data(),
          extra_response_headers_vector.size() / 2, 3));
      AddRead(&frames_.back());

      // WebSocket data received.
      if (!additional_data_.empty()) {
        frames_.push_back(
            spdy_util_.ConstructSpdyDataFrame(3, additional_data_, true));
        AddRead(&frames_.back());
      }

      // Client cancels HTTP/2 stream when request is destroyed.
      frames_.push_back(
          spdy_util_.ConstructSpdyRstStream(3, spdy::ERROR_CODE_CANCEL));
      AddWrite(&frames_.back());
    }

    // EOF.
    reads_.emplace_back(ASYNC, 0, sequence_number_++);

    auto socket_data = std::make_unique<SequencedSocketData>(reads_, writes_);
    socket_data->set_connect_data(MockConnect(SYNCHRONOUS, OK));
    AddRawExpectations(std::move(socket_data));

    // Send first request.  This makes sure server's
    // spdy::SETTINGS_ENABLE_CONNECT_PROTOCOL advertisement is read.
    URLRequestContext* context =
        url_request_context_host_.GetURLRequestContext();
    TestDelegate delegate;
    std::unique_ptr<URLRequest> request = context->CreateRequest(
        GURL("https://www.example.org/"), DEFAULT_PRIORITY, &delegate,
        TRAFFIC_ANNOTATION_FOR_TESTS, /*is_for_websockets=*/false);
    // The IsolationInfo has to match for a socket to be reused.
    request->set_isolation_info(CreateIsolationInfo());
    request->Start();
    EXPECT_TRUE(request->is_pending());
    delegate.RunUntilComplete();
    EXPECT_FALSE(request->is_pending());

    CreateAndConnectStream(socket_url, sub_protocols, Origin(),
                           SiteForCookies(), storage_access_api_status,
                           CreateIsolationInfo(),
                           WebSocketExtraHeadersToHttpRequestHeaders(
                               send_additional_request_headers),
                           std::move(timer_));
  }

  // Like CreateAndConnectStandard(), but allow for arbitrary response body.
  // Only for HTTP/1-based WebSockets.
  void CreateAndConnectCustomResponse(
      std::string_view url,
      const std::vector<std::string>& sub_protocols,
      const WebSocketExtraHeaders& send_additional_request_headers,
      const WebSocketExtraHeaders& extra_request_headers,
      const std::string& response_body,
      StorageAccessApiStatus storage_access_api_status =
          StorageAccessApiStatus::kNone) {
    ASSERT_EQ(BASIC_HANDSHAKE_STREAM, stream_type_);

    const GURL socket_url(url);
    const std::string socket_host = GetHostAndOptionalPort(socket_url);
    const std::string socket_path = socket_url.path();

    url_request_context_host_.SetExpectations(
        WebSocketStandardRequest(socket_path, socket_host, Origin(),
                                 send_additional_request_headers,
                                 extra_request_headers),
        response_body);
    CreateAndConnectStream(socket_url, sub_protocols, Origin(),
                           SiteForCookies(), storage_access_api_status,
                           CreateIsolationInfo(),
                           WebSocketExtraHeadersToHttpRequestHeaders(
                               send_additional_request_headers),
                           nullptr);
  }

  // Like CreateAndConnectStandard(), but take extra response headers as a
  // string.  This can save space in case of a very large response.
  // Only for HTTP/1-based WebSockets.
  void CreateAndConnectStringResponse(
      std::string_view url,
      const std::vector<std::string>& sub_protocols,
      const std::string& extra_response_headers,
      StorageAccessApiStatus storage_access_api_status =
          StorageAccessApiStatus::kNone) {
    ASSERT_EQ(BASIC_HANDSHAKE_STREAM, stream_type_);

    const GURL socket_url(url);
    const std::string socket_host = GetHostAndOptionalPort(socket_url);
    const std::string socket_path = socket_url.path();

    url_request_context_host_.SetExpectations(
        WebSocketStandardRequest(socket_path, socket_host, Origin(),
                                 /*send_additional_request_headers=*/{},
                                 /*extra_headers=*/{}),
        WebSocketStandardResponse(extra_response_headers));
    CreateAndConnectStream(socket_url, sub_protocols, Origin(),
                           SiteForCookies(), storage_access_api_status,
                           CreateIsolationInfo(), HttpRequestHeaders(),
                           nullptr);
  }

  // Like CreateAndConnectStandard(), but take raw mock data.
  void CreateAndConnectRawExpectations(
      std::string_view url,
      const std::vector<std::string>& sub_protocols,
      const HttpRequestHeaders& additional_headers,
      std::unique_ptr<SequencedSocketData> socket_data,
      StorageAccessApiStatus storage_access_api_status =
          StorageAccessApiStatus::kNone) {
    ASSERT_EQ(BASIC_HANDSHAKE_STREAM, stream_type_);

    AddRawExpectations(std::move(socket_data));
    CreateAndConnectStream(GURL(url), sub_protocols, Origin(), SiteForCookies(),
                           storage_access_api_status, CreateIsolationInfo(),
                           additional_headers, std::move(timer_));
  }

  bool PriorityHeaderEnabled() const { return std::get<bool>(GetParam()); }

 private:
  void AddWrite(const spdy::SpdySerializedFrame* frame) {
    writes_.emplace_back(ASYNC, frame->data(), frame->size(),
                         sequence_number_++);
  }

  void AddRead(const spdy::SpdySerializedFrame* frame) {
    reads_.emplace_back(ASYNC, frame->data(), frame->size(),
                        sequence_number_++);
  }

 protected:
  const HandshakeStreamType stream_type_;

 private:
  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<base::OneShotTimer> timer_;
  std::string additional_data_;
  const char* http2_response_status_ = "200";
  bool reset_websocket_http2_stream_ = false;
  SpdyTestUtil spdy_util_;
  NetLogWithSource log_;

  int sequence_number_ = 0;

  // Store mock HTTP/2 data.
  std::vector<spdy::SpdySerializedFrame> frames_;

  // Store MockRead and MockWrite objects that have pointers to above data.
  std::vector<MockRead> reads_;
  std::vector<MockWrite> writes_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         WebSocketStreamCreateTest,
                         testing::Combine(Values(BASIC_HANDSHAKE_STREAM),
                                          testing::Bool()));

using WebSocketMultiProtocolStreamCreateTest = WebSocketStreamCreateTest;

INSTANTIATE_TEST_SUITE_P(All,
                         WebSocketMultiProtocolStreamCreateTest,
                         testing::Combine(Values(BASIC_HANDSHAKE_STREAM,
                                                 HTTP2_HANDSHAKE_STREAM),
                                          testing::Bool()));

// There are enough tests of the Sec-WebSocket-Extensions header that they
// deserve their own test fixture.
class WebSocketStreamCreateExtensionTest
    : public WebSocketMultiProtocolStreamCreateTest {
 protected:
  // Performs a standard connect, with the value of the Sec-WebSocket-Extensions
  // header in the response set to |extensions_header_value|. Runs the event
  // loop to allow the connect to complete.
  void CreateAndConnectWithExtensions(
      const std::string& extensions_header_value) {
    AddSSLData();
    CreateAndConnectStandard(
        "wss://www.example.org/testing_path", NoSubProtocols(), {}, {},
        {{"Sec-WebSocket-Extensions", extensions_header_value}});
    WaitUntilConnectDone();
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         WebSocketStreamCreateExtensionTest,
                         testing::Combine(Values(BASIC_HANDSHAKE_STREAM,
                                                 HTTP2_HANDSHAKE_STREAM),
                                          testing::Bool()));

// Common code to construct expectations for authentication tests that receive
// the auth challenge on one connection and then create a second connection to
// send the authenticated request on.
class CommonAuthTestHelper {
 public:
  CommonAuthTestHelper() : reads_(), writes_() {}

  CommonAuthTestHelper(const CommonAuthTestHelper&) = delete;
  CommonAuthTestHelper& operator=(const CommonAuthTestHelper&) = delete;

  std::unique_ptr<SequencedSocketData> BuildAuthSocketData(
      std::string response1,
      std::string request2,
      std::string response2) {
    request1_ = WebSocketStandardRequest("/", "www.example.org", Origin(),
                                         /*send_additional_request_headers=*/{},
                                         /*extra_headers=*/{});
    response1_ = std::move(response1);
    request2_ = std::move(request2);
    response2_ = std::move(response2);
    writes_[0] = MockWrite(SYNCHRONOUS, 0, request1_.c_str());
    reads_[0] = MockRead(SYNCHRONOUS, 1, response1_.c_str());
    writes_[1] = MockWrite(SYNCHRONOUS, 2, request2_.c_str());
    reads_[1] = MockRead(SYNCHRONOUS, 3, response2_.c_str());
    reads_[2] = MockRead(SYNCHRONOUS, OK, 4);  // Close connection

    return BuildSocketData(reads_, writes_);
  }

 private:
  // These need to be object-scoped since they have to remain valid until all
  // socket operations in the test are complete.
  std::string request1_;
  std::string request2_;
  std::string response1_;
  std::string response2_;
  MockRead reads_[3];
  MockWrite writes_[2];
};

// Data and methods for BasicAuth tests.
class WebSocketStreamCreateBasicAuthTest : public WebSocketStreamCreateTest {
 protected:
  void CreateAndConnectAuthHandshake(std::string_view url,
                                     std::string_view base64_user_pass,
                                     std::string_view response2) {
    CreateAndConnectRawExpectations(
        url, NoSubProtocols(), HttpRequestHeaders(),
        helper_.BuildAuthSocketData(kUnauthorizedResponse,
                                    RequestExpectation(base64_user_pass),
                                    std::string(response2)));
  }

  static std::string RequestExpectation(std::string_view base64_user_pass) {
    // Copy base64_user_pass to a std::string in case it is not nul-terminated.
    std::string base64_user_pass_string(base64_user_pass);
    return base::StringPrintf(
        "GET / HTTP/1.1\r\n"
        "Host: www.example.org\r\n"
        "Connection: Upgrade\r\n"
        "Pragma: no-cache\r\n"
        "Cache-Control: no-cache\r\n"
        "Authorization: Basic %s\r\n"
        "Upgrade: websocket\r\n"
        "Origin: http://www.example.org\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "User-Agent: \r\n"
        "Accept-Encoding: gzip, deflate\r\n"
        "Accept-Language: en-us,fr\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Extensions: permessage-deflate; "
        "client_max_window_bits\r\n"
        "\r\n",
        base64_user_pass_string.c_str());
  }

  static const char kUnauthorizedResponse[];

  CommonAuthTestHelper helper_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         WebSocketStreamCreateBasicAuthTest,
                         testing::Combine(Values(BASIC_HANDSHAKE_STREAM),
                                          testing::Bool()));

class WebSocketStreamCreateDigestAuthTest : public WebSocketStreamCreateTest {
 protected:
  static const char kUnauthorizedResponse[];
  static const char kAuthorizedRequest[];

  CommonAuthTestHelper helper_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         WebSocketStreamCreateDigestAuthTest,
                         testing::Combine(Values(BASIC_HANDSHAKE_STREAM),
                                          testing::Bool()));

const char WebSocketStreamCreateBasicAuthTest::kUnauthorizedResponse[] =
    "HTTP/1.1 401 Unauthorized\r\n"
    "Content-Length: 0\r\n"
    "WWW-Authenticate: Basic realm=\"camelot\"\r\n"
    "\r\n";

// These negotiation values are borrowed from
// http_auth_handler_digest_unittest.cc. Feel free to come up with new ones if
// you are bored. Only the weakest (no qop) variants of Digest authentication
// can be tested by this method, because the others involve random input.
const char WebSocketStreamCreateDigestAuthTest::kUnauthorizedResponse[] =
    "HTTP/1.1 401 Unauthorized\r\n"
    "Content-Length: 0\r\n"
    "WWW-Authenticate: Digest realm=\"Oblivion\", nonce=\"nonce-value\"\r\n"
    "\r\n";

const char WebSocketStreamCreateDigestAuthTest::kAuthorizedRequest[] =
    "GET / HTTP/1.1\r\n"
    "Host: www.example.org\r\n"
    "Connection: Upgrade\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Authorization: Digest username=\"FooBar\", realm=\"Oblivion\", "
    "nonce=\"nonce-value\", uri=\"/\", "
    "response=\"f72ff54ebde2f928860f806ec04acd1b\"\r\n"
    "Upgrade: websocket\r\n"
    "Origin: http://www.example.org\r\n"
    "Sec-WebSocket-Version: 13\r\n"
    "User-Agent: \r\n"
    "Accept-Encoding: gzip, deflate\r\n"
    "Accept-Language: en-us,fr\r\n"
    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
    "Sec-WebSocket-Extensions: permessage-deflate; "
    "client_max_window_bits\r\n"
    "\r\n";

// Confirm that the basic case works as expected.
TEST_P(WebSocketMultiProtocolStreamCreateTest, SimpleSuccess) {
  base::HistogramTester histogram_tester;

  AddSSLData();
  EXPECT_FALSE(url_request_);
  CreateAndConnectStandard("wss://www.example.org/", NoSubProtocols(), {}, {},
                           {});
  EXPECT_FALSE(request_info_);
  EXPECT_FALSE(response_info_);
  EXPECT_TRUE(url_request_);
  WaitUntilConnectDone();
  EXPECT_FALSE(has_failed());
  EXPECT_TRUE(stream_);
  EXPECT_TRUE(request_info_);
  EXPECT_TRUE(response_info_);

  // Histograms are only updated on stream request destruction.
  stream_request_.reset();
  stream_.reset();

  EXPECT_EQ(ERR_WS_UPGRADE,
            url_request_context_host_.network_delegate().last_error());

  auto samples = histogram_tester.GetHistogramSamplesSinceCreation(
      "Net.WebSocket.HandshakeResult2");
  EXPECT_EQ(1, samples->TotalCount());
  if (stream_type_ == BASIC_HANDSHAKE_STREAM) {
    EXPECT_EQ(1,
              samples->GetCount(static_cast<int>(
                  WebSocketHandshakeStreamBase::HandshakeResult::CONNECTED)));
  } else {
    DCHECK_EQ(stream_type_, HTTP2_HANDSHAKE_STREAM);
    EXPECT_EQ(
        1,
        samples->GetCount(static_cast<int>(
            WebSocketHandshakeStreamBase::HandshakeResult::HTTP2_CONNECTED)));
  }
}

TEST_P(WebSocketStreamCreateTest, HandshakeInfo) {
  static constexpr char kResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "foo: bar, baz\r\n"
      "hoge: fuga\r\n"
      "hoge: piyo\r\n"
      "\r\n";

  CreateAndConnectCustomResponse("ws://www.example.org/", NoSubProtocols(), {},
                                 {}, kResponse);
  EXPECT_FALSE(request_info_);
  EXPECT_FALSE(response_info_);
  WaitUntilConnectDone();
  EXPECT_TRUE(stream_);
  ASSERT_TRUE(request_info_);
  ASSERT_TRUE(response_info_);
  std::vector<HeaderKeyValuePair> request_headers =
      RequestHeadersToVector(request_info_->headers);
  // We examine the contents of request_info_ and response_info_
  // mainly only in this test case.
  EXPECT_EQ(GURL("ws://www.example.org/"), request_info_->url);
  EXPECT_EQ(GURL("ws://www.example.org/"), response_info_->url);
  EXPECT_EQ(101, response_info_->headers->response_code());
  EXPECT_EQ("Switching Protocols", response_info_->headers->GetStatusText());
  ASSERT_EQ(12u, request_headers.size());
  EXPECT_EQ(HeaderKeyValuePair("Host", "www.example.org"), request_headers[0]);
  EXPECT_EQ(HeaderKeyValuePair("Connection", "Upgrade"), request_headers[1]);
  EXPECT_EQ(HeaderKeyValuePair("Pragma", "no-cache"), request_headers[2]);
  EXPECT_EQ(HeaderKeyValuePair("Cache-Control", "no-cache"),
            request_headers[3]);
  EXPECT_EQ(HeaderKeyValuePair("Upgrade", "websocket"), request_headers[4]);
  EXPECT_EQ(HeaderKeyValuePair("Origin", "http://www.example.org"),
            request_headers[5]);
  EXPECT_EQ(HeaderKeyValuePair("Sec-WebSocket-Version", "13"),
            request_headers[6]);
  EXPECT_EQ(HeaderKeyValuePair("User-Agent", ""), request_headers[7]);
  EXPECT_EQ(HeaderKeyValuePair("Accept-Encoding", "gzip, deflate"),
            request_headers[8]);
  EXPECT_EQ(HeaderKeyValuePair("Accept-Language", "en-us,fr"),
            request_headers[9]);
  EXPECT_EQ("Sec-WebSocket-Key",  request_headers[10].first);
  EXPECT_EQ(HeaderKeyValuePair("Sec-WebSocket-Extensions",
                               "permessage-deflate; client_max_window_bits"),
            request_headers[11]);

  std::vector<HeaderKeyValuePair> response_headers =
      ResponseHeadersToVector(*response_info_->headers.get());
  ASSERT_EQ(6u, response_headers.size());
  // Sort the headers for ease of verification.
  std::sort(response_headers.begin(), response_headers.end());

  EXPECT_EQ(HeaderKeyValuePair("Connection", "Upgrade"), response_headers[0]);
  EXPECT_EQ("Sec-WebSocket-Accept", response_headers[1].first);
  EXPECT_EQ(HeaderKeyValuePair("Upgrade", "websocket"), response_headers[2]);
  EXPECT_EQ(HeaderKeyValuePair("foo", "bar, baz"), response_headers[3]);
  EXPECT_EQ(HeaderKeyValuePair("hoge", "fuga"), response_headers[4]);
  EXPECT_EQ(HeaderKeyValuePair("hoge", "piyo"), response_headers[5]);
}

// Confirms that request headers are overriden/added after handshake
TEST_P(WebSocketStreamCreateTest, HandshakeOverrideHeaders) {
  WebSocketExtraHeaders additional_headers(
      {{"User-Agent", "OveRrIde"}, {"rAnDomHeader", "foobar"}});
  CreateAndConnectStandard("ws://www.example.org/", NoSubProtocols(),
                           additional_headers, additional_headers, {});
  EXPECT_FALSE(request_info_);
  EXPECT_FALSE(response_info_);
  WaitUntilConnectDone();
  EXPECT_FALSE(has_failed());
  EXPECT_TRUE(stream_);
  EXPECT_TRUE(request_info_);
  EXPECT_TRUE(response_info_);

  std::vector<HeaderKeyValuePair> request_headers =
      RequestHeadersToVector(request_info_->headers);
  EXPECT_EQ(HeaderKeyValuePair("User-Agent", "OveRrIde"), request_headers[4]);
  EXPECT_EQ(HeaderKeyValuePair("rAnDomHeader", "foobar"), request_headers[5]);
}

TEST_P(WebSocketStreamCreateTest, OmitsHasStorageAccess) {
  CreateAndConnectStandard("ws://www.example.org/", NoSubProtocols(), {}, {},
                           {}, StorageAccessApiStatus::kNone);
  WaitUntilConnectDone();

  EXPECT_THAT(
      url_request_context_host_.network_delegate()
          .cookie_setting_overrides_records(),
      testing::ElementsAre(CookieSettingOverrides(), CookieSettingOverrides()));
}

TEST_P(WebSocketStreamCreateTest, PlumbsHasStorageAccess) {
  CreateAndConnectStandard("ws://www.example.org/", NoSubProtocols(), {}, {},
                           {}, StorageAccessApiStatus::kAccessViaAPI);
  WaitUntilConnectDone();

  CookieSettingOverrides expected_overrides;
  expected_overrides.Put(CookieSettingOverride::kStorageAccessGrantEligible);

  EXPECT_THAT(url_request_context_host_.network_delegate()
                  .cookie_setting_overrides_records(),
              testing::ElementsAre(expected_overrides, expected_overrides));
}

// Confirm that the stream isn't established until the message loop runs.
TEST_P(WebSocketStreamCreateTest, NeedsToRunLoop) {
  CreateAndConnectStandard("ws://www.example.org/", NoSubProtocols(), {}, {},
                           {});
  EXPECT_FALSE(has_failed());
  EXPECT_FALSE(stream_);
}

// Check the path is used.
TEST_P(WebSocketMultiProtocolStreamCreateTest, PathIsUsed) {
  AddSSLData();
  CreateAndConnectStandard("wss://www.example.org/testing_path",
                           NoSubProtocols(), {}, {}, {});
  WaitUntilConnectDone();
  EXPECT_FALSE(has_failed());
  EXPECT_TRUE(stream_);
}

// Check that sub-protocols are sent and parsed.
TEST_P(WebSocketMultiProtocolStreamCreateTest, SubProtocolIsUsed) {
  AddSSLData();
  std::vector<std::string> sub_protocols;
  sub_protocols.push_back("chatv11.chromium.org");
  sub_protocols.push_back("chatv20.chromium.org");
  CreateAndConnectStandard(
      "wss://www.example.org/testing_path", sub_protocols, {},
      {{"Sec-WebSocket-Protocol",
        "chatv11.chromium.org, chatv20.chromium.org"}},
      {{"Sec-WebSocket-Protocol", "chatv20.chromium.org"}});
  WaitUntilConnectDone();
  ASSERT_TRUE(stream_);
  EXPECT_FALSE(has_failed());
  EXPECT_EQ("chatv20.chromium.org", stream_->GetSubProtocol());
}

// Unsolicited sub-protocols are rejected.
TEST_P(WebSocketMultiProtocolStreamCreateTest, UnsolicitedSubProtocol) {
  base::HistogramTester histogram_tester;

  AddSSLData();
  CreateAndConnectStandard(
      "wss://www.example.org/testing_path", NoSubProtocols(), {}, {},
      {{"Sec-WebSocket-Protocol", "chatv20.chromium.org"}});
  WaitUntilConnectDone();
  EXPECT_FALSE(stream_);
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: "
            "Response must not include 'Sec-WebSocket-Protocol' header "
            "if not present in request: chatv20.chromium.org",
            failure_message());
  EXPECT_EQ(ERR_INVALID_RESPONSE,
            url_request_context_host_.network_delegate().last_error());

  stream_request_.reset();

  auto samples = histogram_tester.GetHistogramSamplesSinceCreation(
      "Net.WebSocket.HandshakeResult2");
  EXPECT_EQ(1, samples->TotalCount());
  if (stream_type_ == BASIC_HANDSHAKE_STREAM) {
    EXPECT_EQ(
        1,
        samples->GetCount(static_cast<int>(
            WebSocketHandshakeStreamBase::HandshakeResult::FAILED_SUBPROTO)));
  } else {
    DCHECK_EQ(stream_type_, HTTP2_HANDSHAKE_STREAM);
    EXPECT_EQ(1, samples->GetCount(static_cast<int>(
                     WebSocketHandshakeStreamBase::HandshakeResult::
                         HTTP2_FAILED_SUBPROTO)));
  }
}

// Missing sub-protocol response is rejected.
TEST_P(WebSocketMultiProtocolStreamCreateTest, UnacceptedSubProtocol) {
  AddSSLData();
  std::vector<std::string> sub_protocols;
  sub_protocols.push_back("chat.example.com");
  CreateAndConnectStandard("wss://www.example.org/testing_path", sub_protocols,
                           {}, {{"Sec-WebSocket-Protocol", "chat.example.com"}},
                           {});
  WaitUntilConnectDone();
  EXPECT_FALSE(stream_);
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: "
            "Sent non-empty 'Sec-WebSocket-Protocol' header "
            "but no response was received",
            failure_message());
}

// Only one sub-protocol can be accepted.
TEST_P(WebSocketMultiProtocolStreamCreateTest, MultipleSubProtocolsInResponse) {
  AddSSLData();
  std::vector<std::string> sub_protocols;
  sub_protocols.push_back("chatv11.chromium.org");
  sub_protocols.push_back("chatv20.chromium.org");
  CreateAndConnectStandard("wss://www.example.org/testing_path", sub_protocols,
                           {},
                           {{"Sec-WebSocket-Protocol",
                             "chatv11.chromium.org, chatv20.chromium.org"}},
                           {{"Sec-WebSocket-Protocol",
                             "chatv11.chromium.org, chatv20.chromium.org"}});
  WaitUntilConnectDone();
  EXPECT_FALSE(stream_);
  EXPECT_TRUE(has_failed());
  EXPECT_EQ(
      "Error during WebSocket handshake: "
      "'Sec-WebSocket-Protocol' header must not appear "
      "more than once in a response",
      failure_message());
}

// Unmatched sub-protocol should be rejected.
TEST_P(WebSocketMultiProtocolStreamCreateTest, UnmatchedSubProtocolInResponse) {
  AddSSLData();
  std::vector<std::string> sub_protocols;
  sub_protocols.push_back("chatv11.chromium.org");
  sub_protocols.push_back("chatv20.chromium.org");
  CreateAndConnectStandard(
      "wss://www.example.org/testing_path", sub_protocols, {},
      {{"Sec-WebSocket-Protocol",
        "chatv11.chromium.org, chatv20.chromium.org"}},
      {{"Sec-WebSocket-Protocol", "chatv21.chromium.org"}});
  WaitUntilConnectDone();
  EXPECT_FALSE(stream_);
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: "
            "'Sec-WebSocket-Protocol' header value 'chatv21.chromium.org' "
            "in response does not match any of sent values",
            failure_message());
}

// permessage-deflate extension basic success case.
TEST_P(WebSocketStreamCreateExtensionTest, PerMessageDeflateSuccess) {
  CreateAndConnectWithExtensions("permessage-deflate");
  EXPECT_TRUE(stream_);
  EXPECT_FALSE(has_failed());
}

// permessage-deflate extensions success with all parameters.
TEST_P(WebSocketStreamCreateExtensionTest, PerMessageDeflateParamsSuccess) {
  CreateAndConnectWithExtensions(
      "permessage-deflate; client_no_context_takeover; "
      "server_max_window_bits=11; client_max_window_bits=13; "
      "server_no_context_takeover");
  EXPECT_TRUE(stream_);
  EXPECT_FALSE(has_failed());
}

// Verify that incoming messages are actually decompressed with
// permessage-deflate enabled.
TEST_P(WebSocketStreamCreateExtensionTest, PerMessageDeflateInflates) {
  AddSSLData();
  SetAdditionalResponseData(std::string(
      "\xc1\x07"  // WebSocket header (FIN + RSV1, Text payload 7 bytes)
      "\xf2\x48\xcd\xc9\xc9\x07\x00",  // "Hello" DEFLATE compressed
      9));
  CreateAndConnectStandard(
      "wss://www.example.org/testing_path", NoSubProtocols(), {}, {},
      {{"Sec-WebSocket-Extensions", "permessage-deflate"}});
  WaitUntilConnectDone();

  ASSERT_TRUE(stream_);
  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  TestCompletionCallback callback;
  int rv = stream_->ReadFrames(&frames, callback.callback());
  rv = callback.GetResult(rv);
  ASSERT_THAT(rv, IsOk());
  ASSERT_EQ(1U, frames.size());
  ASSERT_EQ(5U, frames[0]->header.payload_length);
  EXPECT_EQ(std::string("Hello"),
            std::string(frames[0]->payload, frames[0]->header.payload_length));
}

// Unknown extension in the response is rejected
TEST_P(WebSocketStreamCreateExtensionTest, UnknownExtension) {
  CreateAndConnectWithExtensions("x-unknown-extension");
  EXPECT_FALSE(stream_);
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: "
            "Found an unsupported extension 'x-unknown-extension' "
            "in 'Sec-WebSocket-Extensions' header",
            failure_message());
}

// Malformed extensions are rejected (this file does not cover all possible
// parse failures, as the parser is covered thoroughly by its own unit tests).
TEST_P(WebSocketStreamCreateExtensionTest, MalformedExtension) {
  CreateAndConnectWithExtensions(";");
  EXPECT_FALSE(stream_);
  EXPECT_TRUE(has_failed());
  EXPECT_EQ(
      "Error during WebSocket handshake: 'Sec-WebSocket-Extensions' header "
      "value is rejected by the parser: ;",
      failure_message());
}

// The permessage-deflate extension may only be specified once.
TEST_P(WebSocketStreamCreateExtensionTest, OnlyOnePerMessageDeflateAllowed) {
  base::HistogramTester histogram_tester;

  CreateAndConnectWithExtensions(
      "permessage-deflate, permessage-deflate; client_max_window_bits=10");
  EXPECT_FALSE(stream_);
  EXPECT_TRUE(has_failed());
  EXPECT_EQ(
      "Error during WebSocket handshake: "
      "Received duplicate permessage-deflate response",
      failure_message());

  stream_request_.reset();

  auto samples = histogram_tester.GetHistogramSamplesSinceCreation(
      "Net.WebSocket.HandshakeResult2");
  EXPECT_EQ(1, samples->TotalCount());
  if (stream_type_ == BASIC_HANDSHAKE_STREAM) {
    EXPECT_EQ(
        1,
        samples->GetCount(static_cast<int>(
            WebSocketHandshakeStreamBase::HandshakeResult::FAILED_EXTENSIONS)));
  } else {
    DCHECK_EQ(stream_type_, HTTP2_HANDSHAKE_STREAM);
    EXPECT_EQ(1, samples->GetCount(static_cast<int>(
                     WebSocketHandshakeStreamBase::HandshakeResult::
                         HTTP2_FAILED_EXTENSIONS)));
  }
}

// client_max_window_bits must have an argument
TEST_P(WebSocketStreamCreateExtensionTest, NoMaxWindowBitsArgument) {
  CreateAndConnectWithExtensions("permessage-deflate; client_max_window_bits");
  EXPECT_FALSE(stream_);
  EXPECT_TRUE(has_failed());
  EXPECT_EQ(
      "Error during WebSocket handshake: Error in permessage-deflate: "
      "client_max_window_bits must have value",
      failure_message());
}

// Other cases for permessage-deflate parameters are tested in
// websocket_deflate_parameters_test.cc.

// TODO(ricea): Check that WebSocketDeflateStream is initialised with the
// arguments from the server. This is difficult because the data written to the
// socket is randomly masked.

// Additional Sec-WebSocket-Accept headers should be rejected.
TEST_P(WebSocketStreamCreateTest, DoubleAccept) {
  CreateAndConnectStandard(
      "ws://www.example.org/", NoSubProtocols(), {}, {},
      {{"Sec-WebSocket-Accept", "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="}});
  WaitUntilConnectDone();
  EXPECT_FALSE(stream_);
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: "
            "'Sec-WebSocket-Accept' header must not appear "
            "more than once in a response",
            failure_message());
}

// When upgrading an HTTP/1 connection, response code 200 is invalid and must be
// rejected.  Response code 101 means success.  On the other hand, when
// requesting a WebSocket stream over HTTP/2, response code 101 is invalid and
// must be rejected.  Response code 200 means success.
TEST_P(WebSocketMultiProtocolStreamCreateTest, InvalidStatusCode) {
  base::HistogramTester histogram_tester;

  AddSSLData();
  if (stream_type_ == BASIC_HANDSHAKE_STREAM) {
    static constexpr char kInvalidStatusCodeResponse[] =
        "HTTP/1.1 200 OK\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
        "\r\n";
    CreateAndConnectCustomResponse("wss://www.example.org/", NoSubProtocols(),
                                   {}, {}, kInvalidStatusCodeResponse);
  } else {
    DCHECK_EQ(stream_type_, HTTP2_HANDSHAKE_STREAM);
    SetHttp2ResponseStatus("101");
    CreateAndConnectStandard("wss://www.example.org/", NoSubProtocols(), {}, {},
                             {});
  }

  WaitUntilConnectDone();
  stream_request_.reset();
  EXPECT_TRUE(has_failed());
  auto samples = histogram_tester.GetHistogramSamplesSinceCreation(
      "Net.WebSocket.HandshakeResult2");
  EXPECT_EQ(1, samples->TotalCount());

  if (stream_type_ == BASIC_HANDSHAKE_STREAM) {
    EXPECT_EQ("Error during WebSocket handshake: Unexpected response code: 200",
              failure_message());
    EXPECT_EQ(failure_response_code(), 200);
    EXPECT_EQ(
        1, samples->GetCount(static_cast<int>(
               WebSocketHandshakeStreamBase::HandshakeResult::INVALID_STATUS)));
  } else {
    DCHECK_EQ(stream_type_, HTTP2_HANDSHAKE_STREAM);
    EXPECT_EQ("Error during WebSocket handshake: Unexpected response code: 101",
              failure_message());
    EXPECT_EQ(failure_response_code(), 101);
    EXPECT_EQ(1, samples->GetCount(static_cast<int>(
                     WebSocketHandshakeStreamBase::HandshakeResult::
                         HTTP2_INVALID_STATUS)));
  }
}

// Redirects are not followed (according to the WHATWG WebSocket API, which
// overrides RFC6455 for browser applications).
TEST_P(WebSocketMultiProtocolStreamCreateTest, RedirectsRejected) {
  AddSSLData();
  if (stream_type_ == BASIC_HANDSHAKE_STREAM) {
    static constexpr char kRedirectResponse[] =
        "HTTP/1.1 302 Moved Temporarily\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: 34\r\n"
        "Connection: keep-alive\r\n"
        "Location: wss://www.example.org/other\r\n"
        "\r\n"
        "<title>Moved</title><h1>Moved</h1>";
    CreateAndConnectCustomResponse("wss://www.example.org/", NoSubProtocols(),
                                   {}, {}, kRedirectResponse);
  } else {
    DCHECK_EQ(stream_type_, HTTP2_HANDSHAKE_STREAM);
    SetHttp2ResponseStatus("302");
    CreateAndConnectStandard("wss://www.example.org/", NoSubProtocols(), {}, {},
                             {});
  }
  WaitUntilConnectDone();

  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: Unexpected response code: 302",
            failure_message());
}

// Malformed responses should be rejected. HttpStreamParser will accept just
// about any garbage in the middle of the headers. To make it give up, the junk
// has to be at the start of the response. Even then, it just gets treated as an
// HTTP/0.9 response.
TEST_P(WebSocketStreamCreateTest, MalformedResponse) {
  static constexpr char kMalformedResponse[] =
      "220 mx.google.com ESMTP\r\n"
      "HTTP/1.1 101 OK\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "\r\n";
  CreateAndConnectCustomResponse("ws://www.example.org/", NoSubProtocols(), {},
                                 {}, kMalformedResponse);
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: Invalid status line",
            failure_message());
}

// Upgrade header must be present.
TEST_P(WebSocketStreamCreateTest, MissingUpgradeHeader) {
  base::HistogramTester histogram_tester;

  static constexpr char kMissingUpgradeResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "\r\n";
  CreateAndConnectCustomResponse("ws://www.example.org/", NoSubProtocols(), {},
                                 {}, kMissingUpgradeResponse);
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: 'Upgrade' header is missing",
            failure_message());

  stream_request_.reset();

  auto samples = histogram_tester.GetHistogramSamplesSinceCreation(
      "Net.WebSocket.HandshakeResult2");
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(
      1, samples->GetCount(static_cast<int>(
             WebSocketHandshakeStreamBase::HandshakeResult::FAILED_UPGRADE)));
}

// There must only be one upgrade header.
TEST_P(WebSocketStreamCreateTest, DoubleUpgradeHeader) {
  CreateAndConnectStandard("ws://www.example.org/", NoSubProtocols(), {}, {},
                           {{"Upgrade", "HTTP/2.0"}});
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: "
            "'Upgrade' header must not appear more than once in a response",
            failure_message());
}

// There must only be one correct upgrade header.
TEST_P(WebSocketStreamCreateTest, IncorrectUpgradeHeader) {
  static constexpr char kMissingUpgradeResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "Upgrade: hogefuga\r\n"
      "\r\n";
  CreateAndConnectCustomResponse("ws://www.example.org/", NoSubProtocols(), {},
                                 {}, kMissingUpgradeResponse);
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: "
            "'Upgrade' header value is not 'WebSocket': hogefuga",
            failure_message());
}

// Connection header must be present.
TEST_P(WebSocketStreamCreateTest, MissingConnectionHeader) {
  base::HistogramTester histogram_tester;

  static constexpr char kMissingConnectionResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "\r\n";
  CreateAndConnectCustomResponse("ws://www.example.org/", NoSubProtocols(), {},
                                 {}, kMissingConnectionResponse);
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: "
            "'Connection' header is missing",
            failure_message());

  stream_request_.reset();

  auto samples = histogram_tester.GetHistogramSamplesSinceCreation(
      "Net.WebSocket.HandshakeResult2");
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(
      1,
      samples->GetCount(static_cast<int>(
          WebSocketHandshakeStreamBase::HandshakeResult::FAILED_CONNECTION)));
}

// Connection header must contain "Upgrade".
TEST_P(WebSocketStreamCreateTest, IncorrectConnectionHeader) {
  static constexpr char kMissingConnectionResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "Connection: hogefuga\r\n"
      "\r\n";
  CreateAndConnectCustomResponse("ws://www.example.org/", NoSubProtocols(), {},
                                 {}, kMissingConnectionResponse);
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: "
            "'Connection' header value must contain 'Upgrade'",
            failure_message());
}

// Connection header is permitted to contain other tokens.
TEST_P(WebSocketStreamCreateTest, AdditionalTokenInConnectionHeader) {
  static constexpr char kAdditionalConnectionTokenResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade, Keep-Alive\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "\r\n";
  CreateAndConnectCustomResponse("ws://www.example.org/", NoSubProtocols(), {},
                                 {}, kAdditionalConnectionTokenResponse);
  WaitUntilConnectDone();
  EXPECT_FALSE(has_failed());
  EXPECT_TRUE(stream_);
}

// Sec-WebSocket-Accept header must be present.
TEST_P(WebSocketStreamCreateTest, MissingSecWebSocketAccept) {
  base::HistogramTester histogram_tester;

  static constexpr char kMissingAcceptResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "\r\n";
  CreateAndConnectCustomResponse("ws://www.example.org/", NoSubProtocols(), {},
                                 {}, kMissingAcceptResponse);
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: "
            "'Sec-WebSocket-Accept' header is missing",
            failure_message());

  stream_request_.reset();

  auto samples = histogram_tester.GetHistogramSamplesSinceCreation(
      "Net.WebSocket.HandshakeResult2");
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1,
            samples->GetCount(static_cast<int>(
                WebSocketHandshakeStreamBase::HandshakeResult::FAILED_ACCEPT)));
}

// Sec-WebSocket-Accept header must match the key that was sent.
TEST_P(WebSocketStreamCreateTest, WrongSecWebSocketAccept) {
  static constexpr char kIncorrectAcceptResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: x/byyPZ2tOFvJCGkkugcKvqhhPk=\r\n"
      "\r\n";
  CreateAndConnectCustomResponse("ws://www.example.org/", NoSubProtocols(), {},
                                 {}, kIncorrectAcceptResponse);
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error during WebSocket handshake: "
            "Incorrect 'Sec-WebSocket-Accept' header value",
            failure_message());
}

// Cancellation works.
TEST_P(WebSocketStreamCreateTest, Cancellation) {
  CreateAndConnectStandard("ws://www.example.org/", NoSubProtocols(), {}, {},
                           {});
  stream_request_.reset();
  // WaitUntilConnectDone doesn't work in this case.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(has_failed());
  EXPECT_FALSE(stream_);
  EXPECT_FALSE(request_info_);
  EXPECT_FALSE(response_info_);
}

// Connect failure must look just like negotiation failure.
TEST_P(WebSocketStreamCreateTest, ConnectionFailure) {
  std::unique_ptr<SequencedSocketData> socket_data(BuildNullSocketData());
  socket_data->set_connect_data(
      MockConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED));
  CreateAndConnectRawExpectations("ws://www.example.org/", NoSubProtocols(),
                                  HttpRequestHeaders(), std::move(socket_data));
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error in connection establishment: net::ERR_CONNECTION_REFUSED",
            failure_message());
  EXPECT_FALSE(request_info_);
  EXPECT_FALSE(response_info_);
}

// Connect timeout must look just like any other failure.
TEST_P(WebSocketStreamCreateTest, ConnectionTimeout) {
  std::unique_ptr<SequencedSocketData> socket_data(BuildNullSocketData());
  socket_data->set_connect_data(
      MockConnect(ASYNC, ERR_CONNECTION_TIMED_OUT));
  CreateAndConnectRawExpectations("ws://www.example.org/", NoSubProtocols(),
                                  HttpRequestHeaders(), std::move(socket_data));
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error in connection establishment: net::ERR_CONNECTION_TIMED_OUT",
            failure_message());
}

// The server doesn't respond to the opening handshake.
TEST_P(WebSocketStreamCreateTest, HandshakeTimeout) {
  std::unique_ptr<SequencedSocketData> socket_data(BuildNullSocketData());
  socket_data->set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  auto timer = std::make_unique<MockWeakTimer>();
  base::WeakPtr<MockWeakTimer> weak_timer = timer->AsWeakPtr();
  SetTimer(std::move(timer));
  CreateAndConnectRawExpectations("ws://www.example.org/", NoSubProtocols(),
                                  HttpRequestHeaders(), std::move(socket_data));
  EXPECT_FALSE(has_failed());
  ASSERT_TRUE(weak_timer.get());
  EXPECT_TRUE(weak_timer->IsRunning());

  weak_timer->Fire();
  WaitUntilConnectDone();

  EXPECT_TRUE(has_failed());
  EXPECT_EQ("WebSocket opening handshake timed out", failure_message());
  ASSERT_TRUE(weak_timer.get());
  EXPECT_FALSE(weak_timer->IsRunning());
}

// When the connection establishes the timer should be stopped.
TEST_P(WebSocketStreamCreateTest, HandshakeTimerOnSuccess) {
  auto timer = std::make_unique<MockWeakTimer>();
  base::WeakPtr<MockWeakTimer> weak_timer = timer->AsWeakPtr();

  SetTimer(std::move(timer));
  CreateAndConnectStandard("ws://www.example.org/", NoSubProtocols(), {}, {},
                           {});
  ASSERT_TRUE(weak_timer);
  EXPECT_TRUE(weak_timer->IsRunning());

  WaitUntilConnectDone();
  EXPECT_FALSE(has_failed());
  EXPECT_TRUE(stream_);
  ASSERT_TRUE(weak_timer);
  EXPECT_FALSE(weak_timer->IsRunning());
}

// When the connection fails the timer should be stopped.
TEST_P(WebSocketStreamCreateTest, HandshakeTimerOnFailure) {
  std::unique_ptr<SequencedSocketData> socket_data(BuildNullSocketData());
  socket_data->set_connect_data(
      MockConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED));
  auto timer = std::make_unique<MockWeakTimer>();
  base::WeakPtr<MockWeakTimer> weak_timer = timer->AsWeakPtr();
  SetTimer(std::move(timer));
  CreateAndConnectRawExpectations("ws://www.example.org/", NoSubProtocols(),
                                  HttpRequestHeaders(), std::move(socket_data));
  ASSERT_TRUE(weak_timer.get());
  EXPECT_TRUE(weak_timer->IsRunning());

  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Error in connection establishment: net::ERR_CONNECTION_REFUSED",
            failure_message());
  ASSERT_TRUE(weak_timer.get());
  EXPECT_FALSE(weak_timer->IsRunning());
}

// Cancellation during connect works.
TEST_P(WebSocketStreamCreateTest, CancellationDuringConnect) {
  std::unique_ptr<SequencedSocketData> socket_data(BuildNullSocketData());
  socket_data->set_connect_data(MockConnect(SYNCHRONOUS, ERR_IO_PENDING));
  CreateAndConnectRawExpectations("ws://www.example.org/", NoSubProtocols(),
                                  HttpRequestHeaders(), std::move(socket_data));
  stream_request_.reset();
  // WaitUntilConnectDone doesn't work in this case.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(has_failed());
  EXPECT_FALSE(stream_);
}

// Cancellation during write of the request headers works.
TEST_P(WebSocketStreamCreateTest, CancellationDuringWrite) {
  // First write never completes.
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  auto socket_data =
      std::make_unique<SequencedSocketData>(base::span<MockRead>(), writes);
  auto* socket_data_ptr = socket_data.get();
  socket_data->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  CreateAndConnectRawExpectations("ws://www.example.org/", NoSubProtocols(),
                                  HttpRequestHeaders(), std::move(socket_data));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(socket_data_ptr->AllWriteDataConsumed());
  stream_request_.reset();
  // WaitUntilConnectDone doesn't work in this case.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(has_failed());
  EXPECT_FALSE(stream_);
  EXPECT_TRUE(request_info_);
  EXPECT_FALSE(response_info_);
}

// Cancellation during read of the response headers works.
TEST_P(WebSocketStreamCreateTest, CancellationDuringRead) {
  std::string request = WebSocketStandardRequest(
      "/", "www.example.org", Origin(), /*send_additional_request_headers=*/{},
      /*extra_headers=*/{});
  MockWrite writes[] = {MockWrite(ASYNC, 0, request.c_str())};
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 1),
  };
  std::unique_ptr<SequencedSocketData> socket_data(
      BuildSocketData(reads, writes));
  SequencedSocketData* socket_data_raw_ptr = socket_data.get();
  CreateAndConnectRawExpectations("ws://www.example.org/", NoSubProtocols(),
                                  HttpRequestHeaders(), std::move(socket_data));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(socket_data_raw_ptr->AllReadDataConsumed());
  stream_request_.reset();
  // WaitUntilConnectDone doesn't work in this case.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(has_failed());
  EXPECT_FALSE(stream_);
  EXPECT_TRUE(request_info_);
  EXPECT_FALSE(response_info_);
}

// Over-size response headers (> 256KB) should not cause a crash.  This is a
// regression test for crbug.com/339456. It is based on the layout test
// "cookie-flood.html".
TEST_P(WebSocketStreamCreateTest, VeryLargeResponseHeaders) {
  base::HistogramTester histogram_tester;

  std::string set_cookie_headers;
  set_cookie_headers.reserve(24 * 20000);
  for (int i = 0; i < 20000; ++i) {
    set_cookie_headers += base::StringPrintf("Set-Cookie: ws-%d=1\r\n", i);
  }
  ASSERT_GT(set_cookie_headers.size(), 256U * 1024U);
  CreateAndConnectStringResponse("ws://www.example.org/", NoSubProtocols(),
                                 set_cookie_headers);
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_FALSE(response_info_);

  stream_request_.reset();

  auto samples = histogram_tester.GetHistogramSamplesSinceCreation(
      "Net.WebSocket.HandshakeResult2");
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(static_cast<int>(
                   WebSocketHandshakeStreamBase::HandshakeResult::FAILED)));
}

// If the remote host closes the connection without sending headers, we should
// log the console message "Connection closed before receiving a handshake
// response".
TEST_P(WebSocketStreamCreateTest, NoResponse) {
  base::HistogramTester histogram_tester;

  std::string request = WebSocketStandardRequest(
      "/", "www.example.org", Origin(), /*send_additional_request_headers=*/{},
      /*extra_headers=*/{});
  MockWrite writes[] = {MockWrite(ASYNC, request.data(), request.size(), 0)};
  MockRead reads[] = {MockRead(ASYNC, 0, 1)};
  std::unique_ptr<SequencedSocketData> socket_data(
      BuildSocketData(reads, writes));
  SequencedSocketData* socket_data_raw_ptr = socket_data.get();
  CreateAndConnectRawExpectations("ws://www.example.org/", NoSubProtocols(),
                                  HttpRequestHeaders(), std::move(socket_data));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(socket_data_raw_ptr->AllReadDataConsumed());
  EXPECT_TRUE(has_failed());
  EXPECT_FALSE(stream_);
  EXPECT_FALSE(response_info_);
  EXPECT_EQ("Connection closed before receiving a handshake response",
            failure_message());

  stream_request_.reset();

  auto samples = histogram_tester.GetHistogramSamplesSinceCreation(
      "Net.WebSocket.HandshakeResult2");
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(
      1, samples->GetCount(static_cast<int>(
             WebSocketHandshakeStreamBase::HandshakeResult::EMPTY_RESPONSE)));
}

TEST_P(WebSocketStreamCreateTest, SelfSignedCertificateFailure) {
  auto ssl_socket_data = std::make_unique<SSLSocketDataProvider>(
      ASYNC, ERR_CERT_AUTHORITY_INVALID);
  ssl_socket_data->ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "unittest.selfsigned.der");
  ASSERT_TRUE(ssl_socket_data->ssl_info.cert.get());
  url_request_context_host_.AddSSLSocketDataProvider(
      std::move(ssl_socket_data));
  std::unique_ptr<SequencedSocketData> raw_socket_data(BuildNullSocketData());
  CreateAndConnectRawExpectations("wss://www.example.org/", NoSubProtocols(),
                                  HttpRequestHeaders(),
                                  std::move(raw_socket_data));
  // WaitUntilConnectDone doesn't work in this case.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(has_failed());
  ASSERT_TRUE(ssl_error_callbacks_);
  ssl_error_callbacks_->CancelSSLRequest(ERR_CERT_AUTHORITY_INVALID,
                                         &ssl_info_);
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
}

TEST_P(WebSocketStreamCreateTest, SelfSignedCertificateSuccess) {
  auto ssl_socket_data = std::make_unique<SSLSocketDataProvider>(
      ASYNC, ERR_CERT_AUTHORITY_INVALID);
  ssl_socket_data->ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "unittest.selfsigned.der");
  ASSERT_TRUE(ssl_socket_data->ssl_info.cert.get());
  url_request_context_host_.AddSSLSocketDataProvider(
      std::move(ssl_socket_data));
  url_request_context_host_.AddSSLSocketDataProvider(
      std::make_unique<SSLSocketDataProvider>(ASYNC, OK));
  AddRawExpectations(BuildNullSocketData());
  CreateAndConnectStandard("wss://www.example.org/", NoSubProtocols(), {}, {},
                           {});
  // WaitUntilConnectDone doesn't work in this case.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(ssl_error_callbacks_);
  ssl_error_callbacks_->ContinueSSLRequest();
  WaitUntilConnectDone();
  EXPECT_FALSE(has_failed());
  EXPECT_TRUE(stream_);
}

// If the server requests authorisation, but we have no credentials, the
// connection should fail cleanly.
TEST_P(WebSocketStreamCreateBasicAuthTest, FailureNoCredentials) {
  CreateAndConnectCustomResponse("ws://www.example.org/", NoSubProtocols(), {},
                                 {}, kUnauthorizedResponse);
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("HTTP Authentication failed; no valid credentials available",
            failure_message());
  EXPECT_FALSE(response_info_);
}

TEST_P(WebSocketStreamCreateBasicAuthTest, SuccessPasswordInUrl) {
  CreateAndConnectAuthHandshake("ws://foo:bar@www.example.org/", "Zm9vOmJhcg==",
                                WebSocketStandardResponse(std::string()));
  WaitUntilConnectDone();
  EXPECT_FALSE(has_failed());
  EXPECT_TRUE(stream_);
  ASSERT_TRUE(response_info_);
  EXPECT_EQ(101, response_info_->headers->response_code());
}

TEST_P(WebSocketStreamCreateBasicAuthTest, FailureIncorrectPasswordInUrl) {
  CreateAndConnectAuthHandshake("ws://foo:baz@www.example.org/",
                                "Zm9vOmJheg==", kUnauthorizedResponse);
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_FALSE(response_info_);
}

TEST_P(WebSocketStreamCreateBasicAuthTest, SuccessfulConnectionReuse) {
  std::string request1 = WebSocketStandardRequest(
      "/", "www.example.org", Origin(), /*send_additional_request_headers=*/{},
      /*extra_headers=*/{});
  std::string response1 = kUnauthorizedResponse;
  std::string request2 = WebSocketStandardRequest(
      "/", "www.example.org", Origin(),
      {{"Authorization", "Basic Zm9vOmJhcg=="}}, /*extra_headers=*/{});
  std::string response2 = WebSocketStandardResponse(std::string());
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, 0, request1.c_str()),
      MockWrite(SYNCHRONOUS, 2, request2.c_str()),
  };
  MockRead reads[3] = {
      MockRead(SYNCHRONOUS, 1, response1.c_str()),
      MockRead(SYNCHRONOUS, 3, response2.c_str()),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING, 4),
  };
  CreateAndConnectRawExpectations("ws://foo:bar@www.example.org/",
                                  NoSubProtocols(), HttpRequestHeaders(),
                                  BuildSocketData(reads, writes));
  WaitUntilConnectDone();
  EXPECT_FALSE(has_failed());
  EXPECT_TRUE(stream_);
  ASSERT_TRUE(response_info_);
  EXPECT_EQ(101, response_info_->headers->response_code());
}

TEST_P(WebSocketStreamCreateBasicAuthTest, OnAuthRequiredCancelAuth) {
  CreateAndConnectCustomResponse("ws://www.example.org/", NoSubProtocols(), {},
                                 {}, kUnauthorizedResponse);

  EXPECT_FALSE(request_info_);
  EXPECT_FALSE(response_info_);
  on_auth_required_rv_ = ERR_IO_PENDING;
  WaitUntilOnAuthRequired();

  EXPECT_FALSE(stream_);
  EXPECT_FALSE(has_failed());

  std::move(on_auth_required_callback_).Run(nullptr);
  WaitUntilConnectDone();
  EXPECT_FALSE(stream_);
  EXPECT_TRUE(has_failed());
}

TEST_P(WebSocketStreamCreateBasicAuthTest, OnAuthRequiredSetAuth) {
  CreateAndConnectRawExpectations(
      "ws://www.example.org/", NoSubProtocols(), HttpRequestHeaders(),
      helper_.BuildAuthSocketData(kUnauthorizedResponse,
                                  RequestExpectation("Zm9vOmJheg=="),
                                  WebSocketStandardResponse(std::string())));

  EXPECT_FALSE(request_info_);
  EXPECT_FALSE(response_info_);
  on_auth_required_rv_ = ERR_IO_PENDING;
  WaitUntilOnAuthRequired();

  EXPECT_FALSE(stream_);
  EXPECT_FALSE(has_failed());

  AuthCredentials credentials(u"foo", u"baz");
  std::move(on_auth_required_callback_).Run(&credentials);

  WaitUntilConnectDone();
  EXPECT_TRUE(stream_);
  EXPECT_FALSE(has_failed());
}

// Digest auth has the same connection semantics as Basic auth, so we can
// generally assume that whatever works for Basic auth will also work for
// Digest. There's just one test here, to confirm that it works at all.
TEST_P(WebSocketStreamCreateDigestAuthTest, DigestPasswordInUrl) {
  CreateAndConnectRawExpectations(
      "ws://FooBar:pass@www.example.org/", NoSubProtocols(),
      HttpRequestHeaders(),
      helper_.BuildAuthSocketData(kUnauthorizedResponse, kAuthorizedRequest,
                                  WebSocketStandardResponse(std::string())));
  WaitUntilConnectDone();
  EXPECT_FALSE(has_failed());
  EXPECT_TRUE(stream_);
  ASSERT_TRUE(response_info_);
  EXPECT_EQ(101, response_info_->headers->response_code());
}

TEST_P(WebSocketMultiProtocolStreamCreateTest, Incomplete) {
  base::HistogramTester histogram_tester;

  AddSSLData();
  if (stream_type_ == BASIC_HANDSHAKE_STREAM) {
    std::string request = WebSocketStandardRequest(
        "/", "www.example.org", Origin(),
        /*send_additional_request_headers=*/{}, /*extra_headers=*/{});
    MockRead reads[] = {MockRead(ASYNC, ERR_IO_PENDING, 0)};
    MockWrite writes[] = {MockWrite(ASYNC, 1, request.c_str())};
    CreateAndConnectRawExpectations("wss://www.example.org/", NoSubProtocols(),
                                    HttpRequestHeaders(),
                                    BuildSocketData(reads, writes));
    base::RunLoop().RunUntilIdle();
    stream_request_.reset();

    auto samples = histogram_tester.GetHistogramSamplesSinceCreation(
        "Net.WebSocket.HandshakeResult2");
    EXPECT_EQ(1, samples->TotalCount());
    EXPECT_EQ(1,
              samples->GetCount(static_cast<int>(
                  WebSocketHandshakeStreamBase::HandshakeResult::INCOMPLETE)));
  } else {
    DCHECK_EQ(stream_type_, HTTP2_HANDSHAKE_STREAM);
    CreateAndConnectStandard("wss://www.example.org/", NoSubProtocols(), {}, {},
                             {});
    stream_request_.reset();

    auto samples = histogram_tester.GetHistogramSamplesSinceCreation(
        "Net.WebSocket.HandshakeResult2");
    EXPECT_EQ(1, samples->TotalCount());
    EXPECT_EQ(
        1,
        samples->GetCount(static_cast<int>(
            WebSocketHandshakeStreamBase::HandshakeResult::HTTP2_INCOMPLETE)));
  }
}

TEST_P(WebSocketMultiProtocolStreamCreateTest, Http2StreamReset) {
  AddSSLData();

  if (stream_type_ == BASIC_HANDSHAKE_STREAM) {
    // This is a dummy transaction to avoid crash in ~URLRequestContext().
    CreateAndConnectStandard("wss://www.example.org/", NoSubProtocols(), {}, {},
                             {});
  } else {
    DCHECK_EQ(stream_type_, HTTP2_HANDSHAKE_STREAM);
    base::HistogramTester histogram_tester;

    SetResetWebSocketHttp2Stream(true);
    CreateAndConnectStandard("wss://www.example.org/", NoSubProtocols(), {}, {},
                             {});
    base::RunLoop().RunUntilIdle();
    stream_request_.reset();

    EXPECT_TRUE(has_failed());
    EXPECT_EQ("Stream closed with error: net::ERR_HTTP2_PROTOCOL_ERROR",
              failure_message());

    auto samples = histogram_tester.GetHistogramSamplesSinceCreation(
        "Net.WebSocket.HandshakeResult2");
    EXPECT_EQ(1, samples->TotalCount());
    EXPECT_EQ(
        1, samples->GetCount(static_cast<int>(
               WebSocketHandshakeStreamBase::HandshakeResult::HTTP2_FAILED)));
  }
}

TEST_P(WebSocketStreamCreateTest, HandleErrConnectionClosed) {
  base::HistogramTester histogram_tester;

  static constexpr char kTruncatedResponse[] =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
      "Cache-Control: no-sto";

  std::string request = WebSocketStandardRequest(
      "/", "www.example.org", Origin(), /*send_additional_request_headers=*/{},
      /*extra_headers=*/{});
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, 1, kTruncatedResponse),
      MockRead(SYNCHRONOUS, ERR_CONNECTION_CLOSED, 2),
  };
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, 0, request.c_str())};
  std::unique_ptr<SequencedSocketData> socket_data(
      BuildSocketData(reads, writes));
  socket_data->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  CreateAndConnectRawExpectations("ws://www.example.org/", NoSubProtocols(),
                                  HttpRequestHeaders(), std::move(socket_data));
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());

  stream_request_.reset();

  auto samples = histogram_tester.GetHistogramSamplesSinceCreation(
      "Net.WebSocket.HandshakeResult2");
  EXPECT_EQ(1, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(static_cast<int>(
                   WebSocketHandshakeStreamBase::HandshakeResult::
                       FAILED_SWITCHING_PROTOCOLS)));
}

TEST_P(WebSocketStreamCreateTest, HandleErrTunnelConnectionFailed) {
  static constexpr char kConnectRequest[] =
      "CONNECT www.example.org:80 HTTP/1.1\r\n"
      "Host: www.example.org:80\r\n"
      "Proxy-Connection: keep-alive\r\n"
      "\r\n";

  static constexpr char kProxyResponse[] =
      "HTTP/1.1 403 Forbidden\r\n"
      "Content-Type: text/html\r\n"
      "Content-Length: 9\r\n"
      "Connection: keep-alive\r\n"
      "\r\n"
      "Forbidden";

  MockRead reads[] = {MockRead(SYNCHRONOUS, 1, kProxyResponse)};
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, 0, kConnectRequest)};
  std::unique_ptr<SequencedSocketData> socket_data(
      BuildSocketData(reads, writes));
  url_request_context_host_.SetProxyConfig("https=proxy:8000");
  CreateAndConnectRawExpectations("ws://www.example.org/", NoSubProtocols(),
                                  HttpRequestHeaders(), std::move(socket_data));
  WaitUntilConnectDone();
  EXPECT_TRUE(has_failed());
  EXPECT_EQ("Establishing a tunnel via proxy server failed.",
            failure_message());
}

TEST_P(WebSocketStreamCreateTest, CancelSSLRequestAfterDelete) {
  auto ssl_socket_data = std::make_unique<SSLSocketDataProvider>(
      ASYNC, ERR_CERT_AUTHORITY_INVALID);
  ssl_socket_data->ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "unittest.selfsigned.der");
  ASSERT_TRUE(ssl_socket_data->ssl_info.cert.get());
  url_request_context_host_.AddSSLSocketDataProvider(
      std::move(ssl_socket_data));

  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_CONNECTION_RESET, 0)};
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_CONNECTION_RESET, 1)};
  std::unique_ptr<SequencedSocketData> raw_socket_data(
      BuildSocketData(reads, writes));
  CreateAndConnectRawExpectations("wss://www.example.org/", NoSubProtocols(),
                                  HttpRequestHeaders(),
                                  std::move(raw_socket_data));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(has_failed());
  ASSERT_TRUE(ssl_error_callbacks_);
  stream_request_.reset();
  ssl_error_callbacks_->CancelSSLRequest(ERR_CERT_AUTHORITY_INVALID,
                                         &ssl_info_);
}

TEST_P(WebSocketStreamCreateTest, ContinueSSLRequestAfterDelete) {
  auto ssl_socket_data = std::make_unique<SSLSocketDataProvider>(
      ASYNC, ERR_CERT_AUTHORITY_INVALID);
  ssl_socket_data->ssl_info.cert =
      ImportCertFromFile(GetTestCertsDirectory(), "unittest.selfsigned.der");
  ASSERT_TRUE(ssl_socket_data->ssl_info.cert.get());
  url_request_context_host_.AddSSLSocketDataProvider(
      std::move(ssl_socket_data));

  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_CONNECTION_RESET, 0)};
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, ERR_CONNECTION_RESET, 1)};
  std::unique_ptr<SequencedSocketData> raw_socket_data(
      BuildSocketData(reads, writes));
  CreateAndConnectRawExpectations("wss://www.example.org/", NoSubProtocols(),
                                  HttpRequestHeaders(),
                                  std::move(raw_socket_data));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(has_failed());
  ASSERT_TRUE(ssl_error_callbacks_);
  stream_request_.reset();
  ssl_error_callbacks_->ContinueSSLRequest();
}

TEST_P(WebSocketStreamCreateTest, HandleConnectionCloseInFirstSegment) {
  std::string request = WebSocketStandardRequest(
      "/", "www.example.org", Origin(), /*send_additional_request_headers=*/{},
      /*extra_headers=*/{});

  // The response headers are immediately followed by a close frame, length 11,
  // code 1013, reason "Try Again".
  std::string close_body = "\x03\xf5Try Again";
  std::string response = WebSocketStandardResponse(std::string()) + "\x88" +
                         static_cast<char>(close_body.size()) + close_body;
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, response.data(), response.size(), 1),
      MockRead(SYNCHRONOUS, ERR_CONNECTION_CLOSED, 2),
  };
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, 0, request.c_str())};
  std::unique_ptr<SequencedSocketData> socket_data(
      BuildSocketData(reads, writes));
  socket_data->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  CreateAndConnectRawExpectations("ws://www.example.org/", NoSubProtocols(),
                                  HttpRequestHeaders(), std::move(socket_data));
  WaitUntilConnectDone();
  ASSERT_TRUE(stream_);

  std::vector<std::unique_ptr<WebSocketFrame>> frames;
  TestCompletionCallback callback1;
  int rv1 = stream_->ReadFrames(&frames, callback1.callback());
  rv1 = callback1.GetResult(rv1);
  ASSERT_THAT(rv1, IsOk());
  ASSERT_EQ(1U, frames.size());
  EXPECT_EQ(frames[0]->header.opcode, WebSocketFrameHeader::kOpCodeClose);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_EQ(close_body,
            std::string(frames[0]->payload, frames[0]->header.payload_length));

  std::vector<std::unique_ptr<WebSocketFrame>> empty_frames;
  TestCompletionCallback callback2;
  int rv2 = stream_->ReadFrames(&empty_frames, callback2.callback());
  rv2 = callback2.GetResult(rv2);
  ASSERT_THAT(rv2, IsError(ERR_CONNECTION_CLOSED));
}

}  // namespace
}  // namespace net
