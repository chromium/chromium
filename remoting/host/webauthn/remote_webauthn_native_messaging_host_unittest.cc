// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/mojom/webauthn_proxy.mojom.h"
#include "remoting/host/native_messaging/log_message_handler.h"
#include "remoting/host/native_messaging/native_messaging_constants.h"
#include "remoting/host/webauthn/remote_webauthn_constants.h"
#include "remoting/host/webauthn/remote_webauthn_native_messaging_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using testing::_;
using testing::Return;

using IsUvpaaCallback =
    mojom::WebAuthnProxy::IsUserVerifyingPlatformAuthenticatorAvailableCallback;

base::Value::Dict CreateRequestMessage(const std::string& message_type,
                                       int message_id = 1) {
  base::Value::Dict request;
  request.Set(kMessageType, message_type);
  request.Set(kMessageId, message_id);
  return request;
}

void VerifyResponseMessage(const base::Value::Dict& response,
                           const std::string& request_message_type,
                           int message_id = 1) {
  ASSERT_EQ(CHECK_DEREF(response.FindString(kMessageType)),
            request_message_type + "Response");
  ASSERT_EQ(response.FindInt(kMessageId), message_id);
}

void VerifyFakeErrorResponse(const base::Value::Dict& response) {
  const base::Value::Dict* json_error = response.FindDict(kWebAuthnErrorKey);
  ASSERT_NE(json_error, nullptr);
  ASSERT_EQ(CHECK_DEREF(json_error->FindString(kWebAuthnErrorNameKey)),
            "NotSupportedError");
  ASSERT_EQ(CHECK_DEREF(json_error->FindString(kWebAuthnErrorMessageKey)),
            "Test error message");
}

mojom::WebAuthnExceptionDetailsPtr CreateFakeMojoError() {
  auto mojo_error = mojom::WebAuthnExceptionDetails::New();
  mojo_error->name = "NotSupportedError";
  mojo_error->message = "Test error message";
  return mojo_error;
}

class MockWebAuthnProxy : public mojom::WebAuthnProxy {
 public:
  MOCK_METHOD(void,
              IsUserVerifyingPlatformAuthenticatorAvailable,
              (IsUserVerifyingPlatformAuthenticatorAvailableCallback),
              (override));
  MOCK_METHOD(void,
              Create,
              (const std::string&,
               mojo::PendingReceiver<mojom::WebAuthnRequestCanceller>,
               CreateCallback),
              (override));
  MOCK_METHOD(void,
              Get,
              (const std::string&,
               mojo::PendingReceiver<mojom::WebAuthnRequestCanceller>,
               GetCallback),
              (override));
};

class MockWebAuthnRequestCanceller : public mojom::WebAuthnRequestCanceller {
 public:
  MOCK_METHOD(void, Cancel, (CancelCallback), (override));
};

}  // namespace

class RemoteWebAuthnNativeMessagingHostTest
    : public testing::Test,
      public extensions::NativeMessageHost::Client {
 public:
  RemoteWebAuthnNativeMessagingHostTest();
  ~RemoteWebAuthnNativeMessagingHostTest() override;

  void SetUp() override;

  // extensions::NativeMessageHost::Client implementation.
  void PostMessageFromNativeHost(const std::string& message) override;
  void CloseChannel(const std::string& error_message) override;

 protected:
  // nullptr will be returned for the GetSessionServices() call if
  // |should_return_valid_services| is false.
  testing::Expectation ExpectGetSessionServices(
      bool should_return_valid_services = true);

  // Receiver will be immediately discarded if |should_bind_receiver| is false.
  testing::Expectation ExpectBindWebAuthnProxy(
      bool should_bind_receiver = true);

  void SetOnRequestCancellerDisconnectedSpy(
      const base::RepeatingClosure& spy_callback);

  // Sends a message to the native messaging host.
  void SendMessage(const base::Value::Dict& message);

  // Blocks until a new message is received, then returns the message.
  const base::Value::Dict& ReadMessage();

  void ResetReceiver();

  MockWebAuthnProxy webauthn_proxy_;
  raw_ptr<MockChromotingHostServicesProvider, DanglingUntriaged> api_provider_;
  MockChromotingSessionServices api_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  std::unique_ptr<RemoteWebAuthnNativeMessagingHost> host_;
  std::unique_ptr<base::RunLoop> response_run_loop_;
  mojo::Receiver<mojom::WebAuthnProxy> webauthn_proxy_receiver_{
      &webauthn_proxy_};
  base::Value::Dict latest_message_;
};

RemoteWebAuthnNativeMessagingHostTest::RemoteWebAuthnNativeMessagingHostTest() {
  auto api_provider = std::make_unique<MockChromotingHostServicesProvider>();
  api_provider_ = api_provider.get();
  host_ = base::WrapUnique(new RemoteWebAuthnNativeMessagingHost(
      std::move(api_provider), task_environment_.GetMainThreadTaskRunner()));
  response_run_loop_ = std::make_unique<base::RunLoop>();
}

RemoteWebAuthnNativeMessagingHostTest::
    ~RemoteWebAuthnNativeMessagingHostTest() = default;

void RemoteWebAuthnNativeMessagingHostTest::SetUp() {
  host_->Start(/* client= */ this);
}

void RemoteWebAuthnNativeMessagingHostTest::PostMessageFromNativeHost(
    const std::string& message) {
  auto message_json = base::JSONReader::Read(message);
  ASSERT_TRUE(message_json.has_value());
  std::string* message_type = message_json->GetDict().FindString(kMessageType);
  if (message_type &&
      *message_type == LogMessageHandler::kDebugMessageTypeName) {
    // Ignore debug message logs.
    return;
  }
  response_run_loop_->Quit();
  latest_message_ = std::move(*message_json).TakeDict();
}

void RemoteWebAuthnNativeMessagingHostTest::CloseChannel(
    const std::string& error_message) {
  NOTREACHED();
}

testing::Expectation
RemoteWebAuthnNativeMessagingHostTest::ExpectGetSessionServices(
    bool should_return_valid_services) {
  return EXPECT_CALL(*api_provider_, GetSessionServices())
      .WillOnce(Return(should_return_valid_services ? &api_ : nullptr));
}

testing::Expectation
RemoteWebAuthnNativeMessagingHostTest::ExpectBindWebAuthnProxy(
    bool should_bind_receiver) {
  if (should_bind_receiver) {
    return EXPECT_CALL(api_, BindWebAuthnProxy(_))
        .WillOnce(
            [&](mojo::PendingReceiver<mojom::WebAuthnProxy> pending_receiver) {
              webauthn_proxy_receiver_.Bind(std::move(pending_receiver));
            });
  }
  // Receiver is immediately discarded.
  return EXPECT_CALL(api_, BindWebAuthnProxy(_)).WillOnce(Return());
}

void RemoteWebAuthnNativeMessagingHostTest::
    SetOnRequestCancellerDisconnectedSpy(
        const base::RepeatingClosure& spy_callback) {
  host_->on_request_canceller_disconnected_for_testing_ = spy_callback;
}

void RemoteWebAuthnNativeMessagingHostTest::SendMessage(
    const base::Value::Dict& message) {
  std::string serialized_message;
  ASSERT_TRUE(base::JSONWriter::Write(message, &serialized_message));
  host_->OnMessage(serialized_message);
}

const base::Value::Dict& RemoteWebAuthnNativeMessagingHostTest::ReadMessage() {
  response_run_loop_->Run();
  response_run_loop_ = std::make_unique<base::RunLoop>();
  return latest_message_;
}

void RemoteWebAuthnNativeMessagingHostTest::ResetReceiver() {
  webauthn_proxy_receiver_.reset();
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest, HelloRequest) {
  SendMessage(CreateRequestMessage(kHelloMessage));

  const base::Value::Dict& response = ReadMessage();
  VerifyResponseMessage(response, kHelloMessage);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest,
       GetRemoteState_FailedToGetSessionServices_NotRemoted) {
  ExpectGetSessionServices(false);
  SendMessage(CreateRequestMessage(kGetRemoteStateMessageType));

  const base::Value::Dict& response = ReadMessage();
  VerifyResponseMessage(response, kGetRemoteStateMessageType);
  ASSERT_EQ(response.FindBool(kGetRemoteStateResponseIsRemotedKey), false);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest,
       GetRemoteState_FailedToBindWebAuthnProxy_NotRemoted) {
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy(false);
  SendMessage(CreateRequestMessage(kGetRemoteStateMessageType));

  const base::Value::Dict& response = ReadMessage();
  VerifyResponseMessage(response, kGetRemoteStateMessageType);
  ASSERT_EQ(response.FindBool(kGetRemoteStateResponseIsRemotedKey), false);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest, GetRemoteState_Remoted) {
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy();
  SendMessage(CreateRequestMessage(kGetRemoteStateMessageType));

  const base::Value::Dict& response = ReadMessage();
  VerifyResponseMessage(response, kGetRemoteStateMessageType);
  ASSERT_EQ(response.FindBool(kGetRemoteStateResponseIsRemotedKey), true);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest, IsUvpaa) {
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy();
  EXPECT_CALL(webauthn_proxy_, IsUserVerifyingPlatformAuthenticatorAvailable(_))
      .WillOnce(base::test::RunOnceCallback<0>(true));
  SendMessage(CreateRequestMessage(kIsUvpaaMessageType));

  const base::Value::Dict& response = ReadMessage();
  VerifyResponseMessage(response, kIsUvpaaMessageType);
  ASSERT_EQ(response.FindBool(kIsUvpaaResponseIsAvailableKey), true);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest,
       ClientDisconnectedWhenRequestIsPending_MessageSent) {
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy();
  EXPECT_CALL(webauthn_proxy_, IsUserVerifyingPlatformAuthenticatorAvailable(_))
      .WillOnce([&](mojom::WebAuthnProxy::
                        IsUserVerifyingPlatformAuthenticatorAvailableCallback
                            callback) { ResetReceiver(); });
  SendMessage(CreateRequestMessage(kIsUvpaaMessageType));

  const base::Value::Dict& response = ReadMessage();
  ASSERT_EQ(CHECK_DEREF(response.FindString(kMessageType)),
            kClientDisconnectedMessageType);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest, ParallelIsUvpaaRequests) {
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy();
  IsUvpaaCallback cb_1;
  IsUvpaaCallback cb_2;
  base::RunLoop both_requests_sent_run_loop;
  EXPECT_CALL(webauthn_proxy_, IsUserVerifyingPlatformAuthenticatorAvailable(_))
      .WillOnce([&](IsUvpaaCallback cb) { cb_1 = std::move(cb); })
      .WillOnce([&](IsUvpaaCallback cb) {
        cb_2 = std::move(cb);
        both_requests_sent_run_loop.Quit();
      });

  SendMessage(CreateRequestMessage(kIsUvpaaMessageType, 1));
  SendMessage(CreateRequestMessage(kIsUvpaaMessageType, 2));
  both_requests_sent_run_loop.Run();
  std::move(cb_2).Run(false);
  base::Value::Dict response_2 = ReadMessage().Clone();
  std::move(cb_1).Run(true);
  base::Value::Dict response_1 = ReadMessage().Clone();

  VerifyResponseMessage(response_1, kIsUvpaaMessageType, 1);
  VerifyResponseMessage(response_2, kIsUvpaaMessageType, 2);
  ASSERT_EQ(response_1.FindBool(kIsUvpaaResponseIsAvailableKey), true);
  ASSERT_EQ(response_2.FindBool(kIsUvpaaResponseIsAvailableKey), false);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest, Create_RequestMissingData_Error) {
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy();

  // Request message missing |requestData| field.
  auto request = CreateRequestMessage(kCreateMessageType);
  SendMessage(std::move(request));

  const base::Value::Dict& response = ReadMessage();

  VerifyResponseMessage(response, kCreateMessageType);
  ASSERT_EQ(response.FindString(kCreateResponseDataKey), nullptr);
  ASSERT_NE(response.Find(kWebAuthnErrorKey), nullptr);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest,
       Create_IpcConnectionFailed_ClientDisconnectMessageSent) {
  ExpectGetSessionServices(false);
  auto request = CreateRequestMessage(kCreateMessageType);
  request.Set(kCreateRequestDataKey, "fake");
  SendMessage(std::move(request));

  const base::Value::Dict& response = ReadMessage();

  ASSERT_EQ(CHECK_DEREF(response.FindString(kMessageType)),
            kClientDisconnectedMessageType);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest, Create_EmptyResponse) {
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy();
  EXPECT_CALL(webauthn_proxy_, Create("fake", _, _))
      .WillOnce(base::test::RunOnceCallback<2>(nullptr));
  auto request = CreateRequestMessage(kCreateMessageType);
  request.Set(kCreateRequestDataKey, "fake");
  SendMessage(std::move(request));

  const base::Value::Dict& response = ReadMessage();

  VerifyResponseMessage(response, kCreateMessageType);
  ASSERT_EQ(response.FindString(kCreateResponseDataKey), nullptr);
  ASSERT_EQ(response.Find(kWebAuthnErrorKey), nullptr);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest, Create_ErrorResponse) {
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy();
  auto mojo_response =
      mojom::WebAuthnCreateResponse::NewErrorDetails(CreateFakeMojoError());
  EXPECT_CALL(webauthn_proxy_, Create("fake", _, _))
      .WillOnce(base::test::RunOnceCallback<2>(std::move(mojo_response)));
  auto request = CreateRequestMessage(kCreateMessageType);
  request.Set(kCreateRequestDataKey, "fake");
  SendMessage(std::move(request));

  const base::Value::Dict& response = ReadMessage();

  VerifyResponseMessage(response, kCreateMessageType);
  VerifyFakeErrorResponse(response);
  ASSERT_EQ(response.FindString(kCreateResponseDataKey), nullptr);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest, Create_DataResponse) {
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy();
  auto mojo_response =
      mojom::WebAuthnCreateResponse::NewResponseData("fake response");
  EXPECT_CALL(webauthn_proxy_, Create("fake", _, _))
      .WillOnce(base::test::RunOnceCallback<2>(std::move(mojo_response)));
  auto request = CreateRequestMessage(kCreateMessageType);
  request.Set(kCreateRequestDataKey, "fake");
  SendMessage(std::move(request));

  const base::Value::Dict& response = ReadMessage();

  VerifyResponseMessage(response, kCreateMessageType);
  ASSERT_EQ(CHECK_DEREF(response.FindString(kCreateResponseDataKey)),
            "fake response");
  ASSERT_EQ(response.Find(kWebAuthnErrorKey), nullptr);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest, Get_RequestMissingData_Error) {
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy();

  // Request message missing |requestData| field.
  auto request = CreateRequestMessage(kGetMessageType);
  SendMessage(std::move(request));

  const base::Value::Dict& response = ReadMessage();

  VerifyResponseMessage(response, kGetMessageType);
  ASSERT_EQ(response.FindString(kGetResponseDataKey), nullptr);
  ASSERT_NE(response.Find(kWebAuthnErrorKey), nullptr);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest,
       Get_IpcConnectionFailed_ClientDisconnectedMessageSent) {
  ExpectGetSessionServices(false);
  auto request = CreateRequestMessage(kGetMessageType);
  request.Set(kGetRequestDataKey, "fake");
  SendMessage(std::move(request));

  const base::Value::Dict& response = ReadMessage();

  ASSERT_EQ(CHECK_DEREF(response.FindString(kMessageType)),
            kClientDisconnectedMessageType);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest, Get_EmptyResponse) {
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy();
  EXPECT_CALL(webauthn_proxy_, Get("fake", _, _))
      .WillOnce(base::test::RunOnceCallback<2>(nullptr));
  auto request = CreateRequestMessage(kGetMessageType);
  request.Set(kGetRequestDataKey, "fake");
  SendMessage(std::move(request));

  const base::Value::Dict& response = ReadMessage();

  VerifyResponseMessage(response, kGetMessageType);
  ASSERT_EQ(response.FindString(kGetResponseDataKey), nullptr);
  ASSERT_EQ(response.Find(kWebAuthnErrorKey), nullptr);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest, Get_ErrorResponse) {
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy();
  auto mojo_response =
      mojom::WebAuthnGetResponse::NewErrorDetails(CreateFakeMojoError());
  EXPECT_CALL(webauthn_proxy_, Get("fake", _, _))
      .WillOnce(base::test::RunOnceCallback<2>(std::move(mojo_response)));
  auto request = CreateRequestMessage(kGetMessageType);
  request.Set(kGetRequestDataKey, "fake");
  SendMessage(std::move(request));

  const base::Value::Dict& response = ReadMessage();

  VerifyResponseMessage(response, kGetMessageType);
  VerifyFakeErrorResponse(response);
  ASSERT_EQ(response.FindString(kGetResponseDataKey), nullptr);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest, Get_DataResponse) {
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy();
  auto mojo_response =
      mojom::WebAuthnGetResponse::NewResponseData("fake response");
  EXPECT_CALL(webauthn_proxy_, Get("fake", _, _))
      .WillOnce(base::test::RunOnceCallback<2>(std::move(mojo_response)));
  auto request = CreateRequestMessage(kGetMessageType);
  request.Set(kGetRequestDataKey, "fake");
  SendMessage(std::move(request));

  const base::Value::Dict& response = ReadMessage();

  VerifyResponseMessage(response, kGetMessageType);
  ASSERT_EQ(CHECK_DEREF(response.FindString(kGetResponseDataKey)),
            "fake response");
  ASSERT_EQ(response.Find(kWebAuthnErrorKey), nullptr);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest,
       Cancel_IpcConnectionFailed_ClientDisconnectedMessageSent) {
  ExpectGetSessionServices(false);

  SendMessage(CreateRequestMessage(kCancelMessageType));

  const base::Value::Dict& response = ReadMessage();

  ASSERT_EQ(CHECK_DEREF(response.FindString(kMessageType)),
            kClientDisconnectedMessageType);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest, Cancel_NonexistentId_Failure) {
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy();

  // No cancelable message with message_id = 1.
  SendMessage(CreateRequestMessage(kCancelMessageType, /* message_id= */ 1));

  const base::Value::Dict& response = ReadMessage();

  VerifyResponseMessage(response, kCancelMessageType);
  ASSERT_EQ(response.FindBool(kCancelResponseWasCanceledKey), false);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest, CancelCreateRequest) {
  MockWebAuthnRequestCanceller mock_canceller_impl;
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy();
  mojo::Receiver<mojom::WebAuthnRequestCanceller> request_canceller{
      &mock_canceller_impl};
  mojom::WebAuthnProxy::CreateCallback create_cb;
  base::RunLoop webauthn_proxy_create_runloop;
  EXPECT_CALL(webauthn_proxy_, Create("fake", _, _))
      .WillOnce([&](const std::string&,
                    mojo::PendingReceiver<mojom::WebAuthnRequestCanceller>
                        pending_receiver,
                    mojom::WebAuthnProxy::CreateCallback cb) {
        request_canceller.Bind(std::move(pending_receiver));
        create_cb = std::move(cb);
        webauthn_proxy_create_runloop.Quit();
      });
  EXPECT_CALL(mock_canceller_impl, Cancel(_))
      .WillOnce(base::test::RunOnceCallback<0>(true));

  auto request = CreateRequestMessage(kCreateMessageType, /* message_id= */ 1);
  request.Set(kCreateRequestDataKey, "fake");
  SendMessage(std::move(request));
  webauthn_proxy_create_runloop.Run();

  request = CreateRequestMessage(kCancelMessageType, /* message_id= */ 1);
  SendMessage(std::move(request));
  const base::Value::Dict& response_1 = ReadMessage();

  VerifyResponseMessage(response_1, kCancelMessageType);
  ASSERT_EQ(response_1.FindBool(kCancelResponseWasCanceledKey), true);

  // Do it again and verify that it should fail this time.
  request = CreateRequestMessage(kCancelMessageType, /* message_id= */ 1);
  SendMessage(std::move(request));
  const base::Value::Dict& response_2 = ReadMessage();

  VerifyResponseMessage(response_2, kCancelMessageType);
  ASSERT_EQ(response_2.FindBool(kCancelResponseWasCanceledKey), false);

  // |create_cb| must be run before it gets disposed.
  std::move(create_cb).Run(nullptr);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest, CancelGetRequest) {
  MockWebAuthnRequestCanceller mock_canceller_impl;
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy();
  mojo::Receiver<mojom::WebAuthnRequestCanceller> request_canceller{
      &mock_canceller_impl};
  mojom::WebAuthnProxy::GetCallback get_cb;
  base::RunLoop webauthn_proxy_get_runloop;
  EXPECT_CALL(webauthn_proxy_, Get("fake", _, _))
      .WillOnce([&](const std::string&,
                    mojo::PendingReceiver<mojom::WebAuthnRequestCanceller>
                        pending_receiver,
                    mojom::WebAuthnProxy::GetCallback cb) {
        request_canceller.Bind(std::move(pending_receiver));
        get_cb = std::move(cb);
        webauthn_proxy_get_runloop.Quit();
      });
  EXPECT_CALL(mock_canceller_impl, Cancel(_))
      .WillOnce(base::test::RunOnceCallback<0>(true));

  auto request = CreateRequestMessage(kGetMessageType, /* message_id= */ 1);
  request.Set(kGetRequestDataKey, "fake");
  SendMessage(std::move(request));
  webauthn_proxy_get_runloop.Run();

  request = CreateRequestMessage(kCancelMessageType, /* message_id= */ 1);
  SendMessage(std::move(request));
  const base::Value::Dict& response_1 = ReadMessage();

  VerifyResponseMessage(response_1, kCancelMessageType);
  ASSERT_EQ(response_1.FindBool(kCancelResponseWasCanceledKey), true);

  // Do it again and verify that it should fail this time.
  request = CreateRequestMessage(kCancelMessageType, /* message_id= */ 1);
  SendMessage(std::move(request));
  const base::Value::Dict& response_2 = ReadMessage();

  VerifyResponseMessage(response_2, kCancelMessageType);
  ASSERT_EQ(response_2.FindBool(kCancelResponseWasCanceledKey), false);

  // |get_cb| must be run before it gets disposed.
  std::move(get_cb).Run(nullptr);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest,
       CancelWithDisconnectedCanceller_Failure) {
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy();
  mojo::PendingReceiver<mojom::WebAuthnRequestCanceller> request_canceller;
  mojom::WebAuthnProxy::CreateCallback create_cb;
  base::RunLoop webauthn_proxy_create_runloop;
  EXPECT_CALL(webauthn_proxy_, Create("fake", _, _))
      .WillOnce([&](const std::string&,
                    mojo::PendingReceiver<mojom::WebAuthnRequestCanceller>
                        pending_receiver,
                    mojom::WebAuthnProxy::CreateCallback cb) {
        request_canceller = std::move(pending_receiver);
        create_cb = std::move(cb);
        webauthn_proxy_create_runloop.Quit();
      });

  auto request = CreateRequestMessage(kCreateMessageType, /* message_id= */ 1);
  request.Set(kCreateRequestDataKey, "fake");
  SendMessage(std::move(request));
  webauthn_proxy_create_runloop.Run();

  base::RunLoop request_canceller_disconnected_run_loop;
  SetOnRequestCancellerDisconnectedSpy(
      request_canceller_disconnected_run_loop.QuitClosure());
  request_canceller.reset();
  request_canceller_disconnected_run_loop.Run();

  request = CreateRequestMessage(kCancelMessageType, /* message_id= */ 1);
  SendMessage(std::move(request));
  const base::Value::Dict& response = ReadMessage();
  // |create_cb| must be run before it gets disposed.
  std::move(create_cb).Run(nullptr);

  VerifyResponseMessage(response, kCancelMessageType);
  ASSERT_EQ(response.FindBool(kCancelResponseWasCanceledKey), false);
}

TEST_F(RemoteWebAuthnNativeMessagingHostTest,
       CancelAlreadyRespondedRequest_Failure) {
  ExpectGetSessionServices();
  ExpectBindWebAuthnProxy();

  EXPECT_CALL(webauthn_proxy_, Create("fake", _, _))
      .WillOnce(base::test::RunOnceCallback<2>(nullptr));

  auto request = CreateRequestMessage(kCreateMessageType, /* message_id= */ 1);
  request.Set(kCreateRequestDataKey, "fake");
  SendMessage(std::move(request));
  const base::Value::Dict& response_1 = ReadMessage();
  VerifyResponseMessage(response_1, kCreateMessageType);

  SendMessage(CreateRequestMessage(kCancelMessageType, /* message_id= */ 1));
  const base::Value::Dict& response_2 = ReadMessage();
  VerifyResponseMessage(response_2, kCancelMessageType);
  ASSERT_EQ(response_2.FindBool(kCancelResponseWasCanceledKey), false);
}

}  // namespace remoting
