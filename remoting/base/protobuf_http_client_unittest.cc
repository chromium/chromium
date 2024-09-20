// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/protobuf_http_client.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "remoting/base/protobuf_http_client_messages.pb.h"
#include "remoting/base/protobuf_http_client_test_messages.pb.h"
#include "remoting/base/protobuf_http_request.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/base/protobuf_http_stream_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using protobufhttpclient::Status;
using protobufhttpclient::StreamBody;
using protobufhttpclienttest::EchoRequest;
using protobufhttpclienttest::EchoResponse;

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::InSequence;

using EchoResponseCallback =
    ProtobufHttpRequest::ResponseCallback<EchoResponse>;
using MockEchoResponseCallback = base::MockCallback<EchoResponseCallback>;
using MockEchoMessageCallback = base::MockCallback<
    ProtobufHttpStreamRequest::MessageCallback<EchoResponse>>;
using MockStreamClosedCallback =
    base::MockCallback<ProtobufHttpStreamRequest::StreamClosedCallback>;

constexpr char kTestServerEndpoint[] = "test.com";
constexpr char kTestRpcPath[] = "/v1/echo:echo";
constexpr char kTestFullUrl[] = "https://test.com/v1/echo:echo";
constexpr char kRequestText[] = "This is a request";
constexpr char kResponseText[] = "This is a response";
constexpr char kAuthorizationHeaderKey[] = "Authorization";
constexpr char kFakeAccessToken[] = "fake_access_token";
constexpr char kFakeAccessTokenHeaderValue[] = "Bearer fake_access_token";

MATCHER_P(HasErrorCode, error_code, "") {
  return arg.error_code() == error_code;
}

MATCHER_P(EqualsToStatus, expected_status, "") {
  return arg.error_code() == expected_status.error_code() &&
         arg.error_message() == expected_status.error_message();
}

MATCHER(IsDefaultResponseText, "") {
  return arg->text() == kResponseText;
}

MATCHER_P(IsResponseText, response_text, "") {
  return arg->text() == response_text;
}

MATCHER(IsNullResponse, "") {
  return arg.get() == nullptr;
}

class MockOAuthTokenGetter : public OAuthTokenGetter {
 public:
  MOCK_METHOD1(CallWithToken, void(TokenCallback));
  MOCK_METHOD0(InvalidateCache, void());
};

EchoResponseCallback DoNothingResponse() {
  return base::DoNothing();
}

std::unique_ptr<ProtobufHttpRequestConfig> CreateDefaultRequestConfig() {
  auto request_message = std::make_unique<EchoRequest>();
  request_message->set_text(kRequestText);
  auto request_config =
      std::make_unique<ProtobufHttpRequestConfig>(TRAFFIC_ANNOTATION_FOR_TESTS);
  request_config->request_message = std::move(request_message);
  request_config->path = kTestRpcPath;
  return request_config;
}

std::unique_ptr<ProtobufHttpRequest> CreateDefaultTestRequest() {
  auto request =
      std::make_unique<ProtobufHttpRequest>(CreateDefaultRequestConfig());
  request->SetResponseCallback(DoNothingResponse());
  return request;
}

std::unique_ptr<ProtobufHttpStreamRequest> CreateDefaultTestStreamRequest() {
  auto request =
      std::make_unique<ProtobufHttpStreamRequest>(CreateDefaultRequestConfig());
  request->SetStreamReadyCallback(base::DoNothing());
  request->SetStreamClosedCallback(base::DoNothing());
  request->SetMessageCallback(
      base::BindRepeating([](std::unique_ptr<EchoResponse>) {}));
  return request;
}

std::string CreateSerializedEchoResponse(
    const std::string& text = kResponseText) {
  EchoResponse response;
  response.set_text(text);
  return response.SerializeAsString();
}

std::string CreateSerializedStreamBodyWithText(
    const std::string& text = kResponseText) {
  StreamBody stream_body;
  stream_body.add_messages(CreateSerializedEchoResponse(text));
  return stream_body.SerializeAsString();
}

std::string CreateSerializedStreamBodyWithStatusCode(
    ProtobufHttpStatus::Code status_code) {
  StreamBody stream_body;
  stream_body.mutable_status()->set_code(static_cast<int32_t>(status_code));
  return stream_body.SerializeAsString();
}

}  // namespace

class ProtobufHttpClientTest : public testing::Test {
 protected:
  void ExpectCallWithTokenSuccess();
  void ExpectCallWithTokenAuthError();
  void ExpectCallWithTokenNetworkError();

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockOAuthTokenGetter mock_token_getter_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  ProtobufHttpClient client_{kTestServerEndpoint, &mock_token_getter_,
                             test_shared_loader_factory_};
};

void ProtobufHttpClientTest::ExpectCallWithTokenSuccess() {
  EXPECT_CALL(mock_token_getter_, CallWithToken(_))
      .WillOnce(RunOnceCallback<0>(OAuthTokenGetter::Status::SUCCESS, "",
                                   kFakeAccessToken, ""));
}

void ProtobufHttpClientTest::ExpectCallWithTokenAuthError() {
  EXPECT_CALL(mock_token_getter_, CallWithToken(_))
      .WillOnce(
          RunOnceCallback<0>(OAuthTokenGetter::Status::AUTH_ERROR, "", "", ""));
}

void ProtobufHttpClientTest::ExpectCallWithTokenNetworkError() {
  EXPECT_CALL(mock_token_getter_, CallWithToken(_))
      .WillOnce(RunOnceCallback<0>(OAuthTokenGetter::Status::NETWORK_ERROR, "",
                                   "", ""));
}

// Unary request tests.

TEST_F(ProtobufHttpClientTest, SendRequestAndDecodeResponse) {
  base::RunLoop run_loop;

  ExpectCallWithTokenSuccess();

  MockEchoResponseCallback response_callback;
  EXPECT_CALL(response_callback, Run(HasErrorCode(ProtobufHttpStatus::Code::OK),
                                     IsDefaultResponseText()))
      .WillOnce([&]() { run_loop.Quit(); });

  auto request = CreateDefaultTestRequest();
  request->SetResponseCallback(response_callback.Get());
  client_.ExecuteRequest(std::move(request));

  // Verify request.
  ASSERT_TRUE(test_url_loader_factory_.IsPending(kTestFullUrl));
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_THAT(
      pending_request->request.headers.GetHeader(kAuthorizationHeaderKey),
      testing::Optional(std::string(kFakeAccessTokenHeaderValue)));
  const auto& data_element =
      pending_request->request.request_body->elements()->front();
  ASSERT_EQ(data_element.type(), network::DataElement::Tag::kBytes);
  std::string request_body_data(
      data_element.As<network::DataElementBytes>().AsStringPiece());
  EchoRequest request_message;
  ASSERT_TRUE(request_message.ParseFromString(request_body_data));
  ASSERT_EQ(kRequestText, request_message.text());

  // Respond.
  test_url_loader_factory_.AddResponse(kTestFullUrl,
                                       CreateSerializedEchoResponse());
  run_loop.Run();
  ASSERT_FALSE(client_.HasPendingRequests());
}

TEST_F(ProtobufHttpClientTest,
       SendUnauthenticatedRequest_TokenGetterNotCalled) {
  EXPECT_CALL(mock_token_getter_, CallWithToken(_)).Times(0);

  auto request_config = CreateDefaultRequestConfig();
  request_config->authenticated = false;
  auto request =
      std::make_unique<ProtobufHttpRequest>(std::move(request_config));
  request->SetResponseCallback(DoNothingResponse());
  client_.ExecuteRequest(std::move(request));

  // Verify that the request is sent with no auth header.
  ASSERT_TRUE(test_url_loader_factory_.IsPending(kTestFullUrl));
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_FALSE(
      pending_request->request.headers.HasHeader(kAuthorizationHeaderKey));
}

TEST_F(ProtobufHttpClientTest,
       FailedToFetchAuthToken_RejectsWithUnauthorizedError) {
  base::RunLoop run_loop;

  ExpectCallWithTokenAuthError();

  MockEchoResponseCallback response_callback;
  EXPECT_CALL(response_callback,
              Run(HasErrorCode(ProtobufHttpStatus::Code::UNAUTHENTICATED),
                  IsNullResponse()))
      .WillOnce([&]() { run_loop.Quit(); });

  auto request = CreateDefaultTestRequest();
  request->SetResponseCallback(response_callback.Get());
  client_.ExecuteRequest(std::move(request));

  run_loop.Run();
  ASSERT_FALSE(client_.HasPendingRequests());
}

TEST_F(ProtobufHttpClientTest,
       FailedToFetchAuthToken_RejectsWithUnavailableError) {
  base::RunLoop run_loop;

  ExpectCallWithTokenNetworkError();

  MockEchoResponseCallback response_callback;
  EXPECT_CALL(response_callback,
              Run(HasErrorCode(ProtobufHttpStatus::Code::UNAVAILABLE),
                  IsNullResponse()))
      .WillOnce([&]() { run_loop.Quit(); });

  auto request = CreateDefaultTestRequest();
  request->SetResponseCallback(response_callback.Get());
  client_.ExecuteRequest(std::move(request));

  run_loop.Run();
  ASSERT_FALSE(client_.HasPendingRequests());
}

TEST_F(ProtobufHttpClientTest, FailedToParseResponse_GetsInvalidResponseError) {
  base::RunLoop run_loop;

  ExpectCallWithTokenSuccess();

  MockEchoResponseCallback response_callback;
  EXPECT_CALL(
      response_callback,
      Run(HasErrorCode(ProtobufHttpStatus::Code::INTERNAL), IsNullResponse()))
      .WillOnce([&]() { run_loop.Quit(); });

  auto request = CreateDefaultTestRequest();
  request->SetResponseCallback(response_callback.Get());
  client_.ExecuteRequest(std::move(request));

  // Respond.
  test_url_loader_factory_.AddResponse(kTestFullUrl, "Invalid content");
  run_loop.Run();
  ASSERT_FALSE(client_.HasPendingRequests());
}

TEST_F(ProtobufHttpClientTest, ServerRespondsWithErrorStatusMessage) {
  base::RunLoop run_loop;

  ExpectCallWithTokenSuccess();

  MockEchoResponseCallback response_callback;
  EXPECT_CALL(response_callback,
              Run(EqualsToStatus(ProtobufHttpStatus(
                      ProtobufHttpStatus::Code::FAILED_PRECONDITION,
                      "Unauthenticated error message")),
                  IsNullResponse()))
      .WillOnce([&]() { run_loop.Quit(); });

  auto request = CreateDefaultTestRequest();
  request->SetResponseCallback(response_callback.Get());
  client_.ExecuteRequest(std::move(request));

  Status status_message;
  status_message.set_code(
      static_cast<int>(ProtobufHttpStatus::Code::FAILED_PRECONDITION));
  status_message.set_message("Unauthenticated error message");

  test_url_loader_factory_.AddResponse(
      kTestFullUrl, status_message.SerializeAsString(),
      net::HttpStatusCode::HTTP_INTERNAL_SERVER_ERROR);
  run_loop.Run();
  ASSERT_FALSE(client_.HasPendingRequests());
}

TEST_F(ProtobufHttpClientTest, ServerRespondsWithHttpErrorCode) {
  base::RunLoop run_loop;

  ExpectCallWithTokenSuccess();

  MockEchoResponseCallback response_callback;
  EXPECT_CALL(response_callback,
              Run(HasErrorCode(ProtobufHttpStatus::Code::UNAUTHENTICATED),
                  IsNullResponse()))
      .WillOnce([&]() { run_loop.Quit(); });

  auto request = CreateDefaultTestRequest();
  request->SetResponseCallback(response_callback.Get());
  client_.ExecuteRequest(std::move(request));

  test_url_loader_factory_.AddResponse(kTestFullUrl, "",
                                       net::HttpStatusCode::HTTP_UNAUTHORIZED);
  run_loop.Run();
  ASSERT_FALSE(client_.HasPendingRequests());
}

TEST_F(ProtobufHttpClientTest,
       CancelPendingRequestsBeforeTokenCallback_CallbackNotCalled) {
  base::RunLoop run_loop;

  OAuthTokenGetter::TokenCallback token_callback;
  EXPECT_CALL(mock_token_getter_, CallWithToken(_))
      .WillOnce([&](OAuthTokenGetter::TokenCallback callback) {
        token_callback = std::move(callback);
      });

  MockEchoResponseCallback not_called_response_callback;

  auto request = CreateDefaultTestRequest();
  request->SetResponseCallback(not_called_response_callback.Get());
  client_.ExecuteRequest(std::move(request));
  client_.CancelPendingRequests();
  ASSERT_TRUE(token_callback);
  std::move(token_callback)
      .Run(OAuthTokenGetter::Status::SUCCESS, "", kFakeAccessToken, "");

  // Verify no request.
  ASSERT_FALSE(test_url_loader_factory_.IsPending(kTestFullUrl));
  ASSERT_FALSE(client_.HasPendingRequests());
}

TEST_F(ProtobufHttpClientTest,
       CancelPendingRequestsAfterTokenCallback_CallbackNotCalled) {
  base::RunLoop run_loop;

  ExpectCallWithTokenSuccess();

  client_.ExecuteRequest(CreateDefaultTestRequest());

  // Respond.
  ASSERT_TRUE(test_url_loader_factory_.IsPending(kTestFullUrl));
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  client_.CancelPendingRequests();
  test_url_loader_factory_.AddResponse(kTestFullUrl,
                                       CreateSerializedEchoResponse());
  run_loop.RunUntilIdle();
  ASSERT_FALSE(client_.HasPendingRequests());
}

TEST_F(ProtobufHttpClientTest, RequestTimeout_ReturnsDeadlineExceeded) {
  base::RunLoop run_loop;

  ExpectCallWithTokenSuccess();

  MockEchoResponseCallback response_callback;
  EXPECT_CALL(response_callback,
              Run(HasErrorCode(ProtobufHttpStatus::Code::DEADLINE_EXCEEDED),
                  IsNullResponse()))
      .WillOnce([&]() { run_loop.Quit(); });

  auto request = CreateDefaultTestRequest();
  request->SetTimeoutDuration(base::Seconds(15));
  request->SetResponseCallback(response_callback.Get());
  client_.ExecuteRequest(std::move(request));

  ASSERT_TRUE(test_url_loader_factory_.IsPending(kTestFullUrl));
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());

  task_environment_.FastForwardBy(base::Seconds(16));

  run_loop.Run();
  ASSERT_FALSE(client_.HasPendingRequests());
}

TEST_F(ProtobufHttpClientTest, DeletesRequestHolderWhenRequestIsCanceled) {
  ExpectCallWithTokenSuccess();

  MockEchoResponseCallback never_called_response_callback;

  auto request = CreateDefaultTestRequest();
  request->SetResponseCallback(never_called_response_callback.Get());
  auto scoped_holder = request->CreateScopedRequest();
  client_.ExecuteRequest(std::move(request));

  // Verify request.
  ASSERT_TRUE(client_.HasPendingRequests());
  ASSERT_TRUE(test_url_loader_factory_.IsPending(kTestFullUrl));
  scoped_holder.reset();
  ASSERT_FALSE(test_url_loader_factory_.IsPending(kTestFullUrl));
  ASSERT_FALSE(client_.HasPendingRequests());

  // Try to respond.
  test_url_loader_factory_.AddResponse(kTestFullUrl,
                                       CreateSerializedEchoResponse());
  // |never_called_response_callback| should not be called.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ProtobufHttpClientTest, DeletesRequestHolderAfterResponseIsReceived) {
  base::RunLoop run_loop;

  ExpectCallWithTokenSuccess();

  MockEchoResponseCallback response_callback;
  EXPECT_CALL(response_callback, Run(HasErrorCode(ProtobufHttpStatus::Code::OK),
                                     IsDefaultResponseText()))
      .WillOnce([&]() { run_loop.Quit(); });

  auto request = CreateDefaultTestRequest();
  request->SetResponseCallback(response_callback.Get());
  auto scoped_holder = request->CreateScopedRequest();
  client_.ExecuteRequest(std::move(request));

  // Verify request.
  ASSERT_TRUE(client_.HasPendingRequests());
  ASSERT_TRUE(test_url_loader_factory_.IsPending(kTestFullUrl));

  // Try to respond.
  test_url_loader_factory_.AddResponse(kTestFullUrl,
                                       CreateSerializedEchoResponse());
  run_loop.Run();

  ASSERT_FALSE(test_url_loader_factory_.IsPending(kTestFullUrl));
  ASSERT_FALSE(client_.HasPendingRequests());
  scoped_holder.reset();
}

// Stream request tests.

TEST_F(ProtobufHttpClientTest,
       StreamRequestFailedToFetchAuthToken_RejectsWithUnauthorizedError) {
  base::MockOnceClosure stream_ready_callback;
  MockEchoMessageCallback message_callback;
  MockStreamClosedCallback stream_closed_callback;

  base::RunLoop run_loop;

  ExpectCallWithTokenAuthError();

  MockEchoResponseCallback response_callback;
  EXPECT_CALL(stream_closed_callback,
              Run(HasErrorCode(ProtobufHttpStatus::Code::UNAUTHENTICATED)))
      .WillOnce([&]() { run_loop.Quit(); });

  auto request = CreateDefaultTestStreamRequest();
  request->SetStreamReadyCallback(stream_ready_callback.Get());
  request->SetMessageCallback(message_callback.Get());
  request->SetStreamClosedCallback(stream_closed_callback.Get());
  client_.ExecuteRequest(std::move(request));

  run_loop.Run();
  ASSERT_FALSE(client_.HasPendingRequests());
}

TEST_F(ProtobufHttpClientTest,
       StreamRequestFailedToFetchAuthToken_RejectsWithUnavailableError) {
  base::MockOnceClosure stream_ready_callback;
  MockEchoMessageCallback message_callback;
  MockStreamClosedCallback stream_closed_callback;

  base::RunLoop run_loop;

  ExpectCallWithTokenNetworkError();

  MockEchoResponseCallback response_callback;
  EXPECT_CALL(stream_closed_callback,
              Run(HasErrorCode(ProtobufHttpStatus::Code::UNAVAILABLE)))
      .WillOnce([&]() { run_loop.Quit(); });

  auto request = CreateDefaultTestStreamRequest();
  request->SetStreamReadyCallback(stream_ready_callback.Get());
  request->SetMessageCallback(message_callback.Get());
  request->SetStreamClosedCallback(stream_closed_callback.Get());
  client_.ExecuteRequest(std::move(request));

  run_loop.Run();
  ASSERT_FALSE(client_.HasPendingRequests());
}

TEST_F(ProtobufHttpClientTest, StartStreamRequestAndDecodeMessages) {
  base::MockOnceClosure stream_ready_callback;
  MockEchoMessageCallback message_callback;
  MockStreamClosedCallback stream_closed_callback;

  {
    InSequence s;

    ExpectCallWithTokenSuccess();
    EXPECT_CALL(stream_ready_callback, Run());
    EXPECT_CALL(message_callback, Run(IsResponseText("response text 1")));
    EXPECT_CALL(message_callback, Run(IsResponseText("response text 2")));
    EXPECT_CALL(stream_closed_callback,
                Run(HasErrorCode(ProtobufHttpStatus::Code::CANCELLED)));
  }

  auto request = CreateDefaultTestStreamRequest();
  request->SetStreamReadyCallback(stream_ready_callback.Get());
  request->SetMessageCallback(message_callback.Get());
  request->SetStreamClosedCallback(stream_closed_callback.Get());
  network::SimpleURLLoaderStreamConsumer* stream_consumer = request.get();
  client_.ExecuteRequest(std::move(request));

  ASSERT_TRUE(test_url_loader_factory_.IsPending(kTestFullUrl));
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());

  // TestURLLoaderFactory can't simulate streaming, so we invoke the request
  // directly.
  stream_consumer->OnDataReceived(
      CreateSerializedStreamBodyWithText("response text 1"), base::DoNothing());
  stream_consumer->OnDataReceived(
      CreateSerializedStreamBodyWithText("response text 2"), base::DoNothing());
  stream_consumer->OnDataReceived(CreateSerializedStreamBodyWithStatusCode(
                                      ProtobufHttpStatus::Code::CANCELLED),
                                  base::DoNothing());
  ASSERT_FALSE(client_.HasPendingRequests());
}

TEST_F(ProtobufHttpClientTest, InvalidStreamData_Ignored) {
  base::RunLoop run_loop;
  base::MockOnceClosure stream_ready_callback;
  MockEchoMessageCallback not_called_message_callback;
  MockStreamClosedCallback stream_closed_callback;

  {
    InSequence s;

    ExpectCallWithTokenSuccess();
    EXPECT_CALL(stream_ready_callback, Run());
    EXPECT_CALL(stream_closed_callback,
                Run(HasErrorCode(ProtobufHttpStatus::Code::OK)))
        .WillOnce([&]() { run_loop.Quit(); });
  }

  auto request = CreateDefaultTestStreamRequest();
  request->SetStreamReadyCallback(stream_ready_callback.Get());
  request->SetMessageCallback(not_called_message_callback.Get());
  request->SetStreamClosedCallback(stream_closed_callback.Get());
  client_.ExecuteRequest(std::move(request));

  ASSERT_TRUE(test_url_loader_factory_.IsPending(kTestFullUrl));
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  test_url_loader_factory_.AddResponse(kTestFullUrl, "Invalid stream data",
                                       net::HttpStatusCode::HTTP_OK);
  run_loop.Run();
  ASSERT_FALSE(client_.HasPendingRequests());
}

TEST_F(ProtobufHttpClientTest, SendHttpStatusOnly_StreamClosesWithHttpStatus) {
  base::RunLoop run_loop;
  base::MockOnceClosure stream_ready_callback;
  MockStreamClosedCallback stream_closed_callback;

  {
    InSequence s;

    ExpectCallWithTokenSuccess();
    EXPECT_CALL(stream_closed_callback,
                Run(HasErrorCode(ProtobufHttpStatus::Code::UNAUTHENTICATED)))
        .WillOnce([&]() { run_loop.Quit(); });
  }

  auto request = CreateDefaultTestStreamRequest();
  request->SetStreamReadyCallback(stream_ready_callback.Get());
  request->SetStreamClosedCallback(stream_closed_callback.Get());
  client_.ExecuteRequest(std::move(request));

  ASSERT_TRUE(test_url_loader_factory_.IsPending(kTestFullUrl));
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  test_url_loader_factory_.AddResponse(kTestFullUrl, /* response_body= */ "",
                                       net::HttpStatusCode::HTTP_UNAUTHORIZED);
  run_loop.Run();
  ASSERT_FALSE(client_.HasPendingRequests());
}

TEST_F(ProtobufHttpClientTest, SendStreamStatusAndHttpStatus_StreamStatusWins) {
  base::RunLoop run_loop;
  base::MockOnceClosure stream_ready_callback;
  MockStreamClosedCallback stream_closed_callback;

  {
    InSequence s;

    ExpectCallWithTokenSuccess();
    EXPECT_CALL(stream_ready_callback, Run());
    EXPECT_CALL(stream_closed_callback,
                Run(HasErrorCode(ProtobufHttpStatus::Code::CANCELLED)))
        .WillOnce([&]() { run_loop.Quit(); });
  }

  auto request = CreateDefaultTestStreamRequest();
  request->SetStreamReadyCallback(stream_ready_callback.Get());
  request->SetStreamClosedCallback(stream_closed_callback.Get());
  client_.ExecuteRequest(std::move(request));

  ASSERT_TRUE(test_url_loader_factory_.IsPending(kTestFullUrl));
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());
  test_url_loader_factory_.AddResponse(kTestFullUrl,
                                       CreateSerializedStreamBodyWithStatusCode(
                                           ProtobufHttpStatus::Code::CANCELLED),
                                       net::HttpStatusCode::HTTP_OK);
  run_loop.Run();
  ASSERT_FALSE(client_.HasPendingRequests());
}

TEST_F(ProtobufHttpClientTest, StreamReadyTimeout) {
  base::MockOnceClosure not_called_stream_ready_callback;
  MockEchoMessageCallback not_called_message_callback;
  MockStreamClosedCallback stream_closed_callback;

  {
    InSequence s;

    ExpectCallWithTokenSuccess();
    EXPECT_CALL(stream_closed_callback,
                Run(HasErrorCode(ProtobufHttpStatus::Code::DEADLINE_EXCEEDED)));
  }

  auto request = CreateDefaultTestStreamRequest();
  request->SetStreamReadyCallback(not_called_stream_ready_callback.Get());
  request->SetMessageCallback(not_called_message_callback.Get());
  request->SetStreamClosedCallback(stream_closed_callback.Get());
  client_.ExecuteRequest(std::move(request));

  ASSERT_TRUE(client_.HasPendingRequests());
  ASSERT_TRUE(test_url_loader_factory_.IsPending(kTestFullUrl));
  ASSERT_EQ(1, test_url_loader_factory_.NumPending());

  task_environment_.FastForwardBy(
      ProtobufHttpStreamRequest::kStreamReadyTimeoutDuration +
      base::Seconds(1));
  ASSERT_FALSE(client_.HasPendingRequests());
}

}  // namespace remoting
