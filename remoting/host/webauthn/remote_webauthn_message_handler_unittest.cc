// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webauthn/remote_webauthn_message_handler.h"

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/host/mojom/webauthn_proxy.mojom.h"
#include "remoting/host/webauthn/remote_webauthn_state_change_notifier.h"
#include "remoting/proto/remote_webauthn.pb.h"
#include "remoting/protocol/fake_message_pipe.h"
#include "remoting/protocol/fake_message_pipe_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace {

using testing::Return;

protocol::RemoteWebAuthn ParseMessage(const std::string& data) {
  protocol::RemoteWebAuthn message;
  message.ParseFromString(data);
  return message;
}

decltype(auto) QuitRunLoopOnSecondRun(bool& run_already_once_tracker,
                                      base::RunLoop& run_loop) {
  return [&]() {
    if (!run_already_once_tracker) {
      run_already_once_tracker = true;
      return;
    }
    run_loop.Quit();
  };
}

MATCHER(NullResponse, "") {
  return arg.is_null();
}

MATCHER_P(ResponseDataMatches, response_data, "") {
  return arg->get_response_data() == response_data;
}

MATCHER_P2(ResponseErrorMatches, error_name, error_message, "") {
  return arg->get_error_details()->name == error_name &&
         arg->get_error_details()->message == error_message;
}

MATCHER_P(ResponseErrorNameMatches, error_name, "") {
  return arg->get_error_details()->name == error_name;
}

class MockRemoteWebAuthnStateChangeNotifier
    : public RemoteWebAuthnStateChangeNotifier {
 public:
  MOCK_METHOD(void, NotifyStateChange, (), (override));
};

}  // namespace

class RemoteWebAuthnMessageHandlerTest : public testing::Test {
 public:
  RemoteWebAuthnMessageHandlerTest();
  ~RemoteWebAuthnMessageHandlerTest() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  mojo::Remote<mojom::WebAuthnProxy> AddReceiverAndPassRemote();
  protocol::RemoteWebAuthn GetLatestSentMessage();

  protocol::FakeMessagePipe fake_pipe_{/* asynchronous= */ false};
  raw_ptr<MockRemoteWebAuthnStateChangeNotifier, DanglingUntriaged>
      mock_state_change_notifier_;
  raw_ptr<RemoteWebAuthnMessageHandler, DanglingUntriaged> message_handler_;

 private:
  base::test::TaskEnvironment task_environment_;
};

RemoteWebAuthnMessageHandlerTest::RemoteWebAuthnMessageHandlerTest() {
  auto mock_state_change_notifier =
      std::make_unique<MockRemoteWebAuthnStateChangeNotifier>();
  mock_state_change_notifier_ = mock_state_change_notifier.get();
  // Lifetime of |message_handler_| is controlled by the |fake_pipe_|.
  message_handler_ = new RemoteWebAuthnMessageHandler(
      "fake name", fake_pipe_.Wrap(), std::move(mock_state_change_notifier));
}

RemoteWebAuthnMessageHandlerTest::~RemoteWebAuthnMessageHandlerTest() = default;

void RemoteWebAuthnMessageHandlerTest::SetUp() {
  EXPECT_CALL(*mock_state_change_notifier_, NotifyStateChange())
      .WillOnce(Return());
  fake_pipe_.OpenPipe();
}

void RemoteWebAuthnMessageHandlerTest::TearDown() {
  EXPECT_CALL(*mock_state_change_notifier_, NotifyStateChange())
      .WillOnce(Return());
  fake_pipe_.ClosePipe();
}

mojo::Remote<mojom::WebAuthnProxy>
RemoteWebAuthnMessageHandlerTest::AddReceiverAndPassRemote() {
  mojo::Remote<mojom::WebAuthnProxy> remote;
  message_handler_->AddReceiver(remote.BindNewPipeAndPassReceiver());
  return remote;
}

protocol::RemoteWebAuthn
RemoteWebAuthnMessageHandlerTest::GetLatestSentMessage() {
  EXPECT_GE(fake_pipe_.sent_messages().size(), 1u);
  return ParseMessage(fake_pipe_.sent_messages().back());
}

TEST_F(RemoteWebAuthnMessageHandlerTest, NotifyWebAuthnStateChange) {
  EXPECT_CALL(*mock_state_change_notifier_, NotifyStateChange())
      .WillOnce(Return());

  message_handler_->NotifyWebAuthnStateChange();
}

TEST_F(RemoteWebAuthnMessageHandlerTest, IsUvpaa) {
  base::RunLoop run_loop;
  base::MockOnceCallback<void(bool)> is_uvpaa_callback;
  EXPECT_CALL(is_uvpaa_callback, Run(true))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));

  auto remote = AddReceiverAndPassRemote();
  remote->IsUserVerifyingPlatformAuthenticatorAvailable(
      is_uvpaa_callback.Get());
  remote.FlushForTesting();

  protocol::RemoteWebAuthn request = GetLatestSentMessage();
  ASSERT_EQ(request.message_case(), protocol::RemoteWebAuthn::kIsUvpaaRequest);
  uint64_t id = request.id();

  protocol::RemoteWebAuthn response;
  response.set_id(id);
  response.mutable_is_uvpaa_response()->set_is_available(true);
  fake_pipe_.ReceiveProtobufMessage(response);

  run_loop.Run();
}

TEST_F(RemoteWebAuthnMessageHandlerTest, Create_Success) {
  base::RunLoop run_loop;
  base::MockOnceCallback<void(mojom::WebAuthnCreateResponsePtr)>
      create_callback;
  EXPECT_CALL(create_callback, Run(ResponseDataMatches("fake response")))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));

  auto remote = AddReceiverAndPassRemote();
  mojo::Remote<mojom::WebAuthnRequestCanceller> request_canceller;
  remote->Create("fake request", request_canceller.BindNewPipeAndPassReceiver(),
                 create_callback.Get());
  remote.FlushForTesting();

  protocol::RemoteWebAuthn request = GetLatestSentMessage();
  ASSERT_EQ(request.message_case(), protocol::RemoteWebAuthn::kCreateRequest);
  uint64_t id = request.id();
  ASSERT_EQ(request.create_request().request_details_json(), "fake request");

  protocol::RemoteWebAuthn response;
  response.set_id(id);
  response.mutable_create_response()->set_response_json("fake response");
  fake_pipe_.ReceiveProtobufMessage(response);

  run_loop.Run();
}

TEST_F(RemoteWebAuthnMessageHandlerTest, Create_Failure) {
  base::RunLoop run_loop;
  base::MockOnceCallback<void(mojom::WebAuthnCreateResponsePtr)>
      create_callback;
  EXPECT_CALL(create_callback,
              Run(ResponseErrorMatches("fake error", "fake error message")))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));

  auto remote = AddReceiverAndPassRemote();
  mojo::Remote<mojom::WebAuthnRequestCanceller> request_canceller;
  remote->Create("fake request", request_canceller.BindNewPipeAndPassReceiver(),
                 create_callback.Get());
  remote.FlushForTesting();

  protocol::RemoteWebAuthn request = GetLatestSentMessage();
  ASSERT_EQ(request.message_case(), protocol::RemoteWebAuthn::kCreateRequest);
  uint64_t id = request.id();
  ASSERT_EQ(request.create_request().request_details_json(), "fake request");

  protocol::RemoteWebAuthn response;
  response.set_id(id);
  response.mutable_create_response()->mutable_error()->set_name("fake error");
  response.mutable_create_response()->mutable_error()->set_message(
      "fake error message");
  fake_pipe_.ReceiveProtobufMessage(response);

  run_loop.Run();
}

TEST_F(RemoteWebAuthnMessageHandlerTest, Create_NullResponse) {
  base::RunLoop run_loop;
  base::MockOnceCallback<void(mojom::WebAuthnCreateResponsePtr)>
      create_callback;
  EXPECT_CALL(create_callback, Run(NullResponse()))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));

  auto remote = AddReceiverAndPassRemote();
  mojo::Remote<mojom::WebAuthnRequestCanceller> request_canceller;
  remote->Create("fake request", request_canceller.BindNewPipeAndPassReceiver(),
                 create_callback.Get());
  remote.FlushForTesting();

  protocol::RemoteWebAuthn request = GetLatestSentMessage();
  ASSERT_EQ(request.message_case(), protocol::RemoteWebAuthn::kCreateRequest);
  uint64_t id = request.id();
  ASSERT_EQ(request.create_request().request_details_json(), "fake request");

  protocol::RemoteWebAuthn response;
  response.set_id(id);
  response.mutable_create_response();
  fake_pipe_.ReceiveProtobufMessage(response);

  run_loop.Run();
}

TEST_F(RemoteWebAuthnMessageHandlerTest, Get_Success) {
  base::RunLoop run_loop;
  base::MockOnceCallback<void(mojom::WebAuthnGetResponsePtr)> get_callback;
  EXPECT_CALL(get_callback, Run(ResponseDataMatches("fake response")))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));

  auto remote = AddReceiverAndPassRemote();
  mojo::Remote<mojom::WebAuthnRequestCanceller> request_canceller;
  remote->Get("fake request", request_canceller.BindNewPipeAndPassReceiver(),
              get_callback.Get());
  remote.FlushForTesting();

  protocol::RemoteWebAuthn request = GetLatestSentMessage();
  ASSERT_EQ(request.message_case(), protocol::RemoteWebAuthn::kGetRequest);
  uint64_t id = request.id();
  ASSERT_EQ(request.get_request().request_details_json(), "fake request");

  protocol::RemoteWebAuthn response;
  response.set_id(id);
  response.mutable_get_response()->set_response_json("fake response");
  fake_pipe_.ReceiveProtobufMessage(response);

  run_loop.Run();
}

TEST_F(RemoteWebAuthnMessageHandlerTest, Get_Failure) {
  base::RunLoop run_loop;
  base::MockOnceCallback<void(mojom::WebAuthnGetResponsePtr)> get_callback;
  EXPECT_CALL(get_callback,
              Run(ResponseErrorMatches("fake error", "fake error message")))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));

  auto remote = AddReceiverAndPassRemote();
  mojo::Remote<mojom::WebAuthnRequestCanceller> request_canceller;
  remote->Get("fake request", request_canceller.BindNewPipeAndPassReceiver(),
              get_callback.Get());
  remote.FlushForTesting();

  protocol::RemoteWebAuthn request = GetLatestSentMessage();
  ASSERT_EQ(request.message_case(), protocol::RemoteWebAuthn::kGetRequest);
  uint64_t id = request.id();
  ASSERT_EQ(request.get_request().request_details_json(), "fake request");

  protocol::RemoteWebAuthn response;
  response.set_id(id);
  response.mutable_get_response()->mutable_error()->set_name("fake error");
  response.mutable_get_response()->mutable_error()->set_message(
      "fake error message");
  fake_pipe_.ReceiveProtobufMessage(response);

  run_loop.Run();
}

TEST_F(RemoteWebAuthnMessageHandlerTest, Get_NullResponse) {
  base::RunLoop run_loop;
  base::MockOnceCallback<void(mojom::WebAuthnGetResponsePtr)> get_callback;
  EXPECT_CALL(get_callback, Run(NullResponse()))
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));

  auto remote = AddReceiverAndPassRemote();
  mojo::Remote<mojom::WebAuthnRequestCanceller> request_canceller;
  remote->Get("fake request", request_canceller.BindNewPipeAndPassReceiver(),
              get_callback.Get());
  remote.FlushForTesting();

  protocol::RemoteWebAuthn request = GetLatestSentMessage();
  ASSERT_EQ(request.message_case(), protocol::RemoteWebAuthn::kGetRequest);
  uint64_t id = request.id();
  ASSERT_EQ(request.get_request().request_details_json(), "fake request");

  protocol::RemoteWebAuthn response;
  response.set_id(id);
  response.mutable_get_response();
  fake_pipe_.ReceiveProtobufMessage(response);

  run_loop.Run();
}

TEST_F(RemoteWebAuthnMessageHandlerTest, CancelCreate_Success) {
  base::RunLoop run_loop;
  base::MockOnceCallback<void(mojom::WebAuthnCreateResponsePtr)>
      create_callback;
  base::MockOnceCallback<void(bool)> cancel_callback;
  bool cb_run_once = false;
  auto quit_run_loop_on_second_run =
      QuitRunLoopOnSecondRun(cb_run_once, run_loop);
  EXPECT_CALL(create_callback, Run(ResponseErrorNameMatches("AbortError")))
      .WillOnce(quit_run_loop_on_second_run);
  EXPECT_CALL(cancel_callback, Run(true)).WillOnce(quit_run_loop_on_second_run);

  auto remote = AddReceiverAndPassRemote();
  mojo::Remote<mojom::WebAuthnRequestCanceller> request_canceller;
  remote->Create("fake request", request_canceller.BindNewPipeAndPassReceiver(),
                 create_callback.Get());
  remote.FlushForTesting();

  protocol::RemoteWebAuthn create_request = GetLatestSentMessage();
  ASSERT_EQ(create_request.message_case(),
            protocol::RemoteWebAuthn::kCreateRequest);
  uint64_t create_id = create_request.id();
  request_canceller->Cancel(cancel_callback.Get());
  remote.FlushForTesting();

  protocol::RemoteWebAuthn cancel_request = GetLatestSentMessage();
  ASSERT_EQ(cancel_request.message_case(),
            protocol::RemoteWebAuthn::kCancelRequest);
  uint64_t cancel_id = cancel_request.id();
  ASSERT_EQ(cancel_id, create_id);

  protocol::RemoteWebAuthn cancel_response;
  cancel_response.set_id(cancel_id);
  cancel_response.mutable_cancel_response()->set_was_canceled(true);
  fake_pipe_.ReceiveProtobufMessage(cancel_response);

  run_loop.Run();
}

TEST_F(RemoteWebAuthnMessageHandlerTest, CancelCreate_Failure) {
  base::RunLoop cancel_response_run_loop;
  base::RunLoop create_response_run_loop;
  base::MockOnceCallback<void(mojom::WebAuthnCreateResponsePtr)>
      create_callback;
  base::MockOnceCallback<void(bool)> cancel_callback;
  EXPECT_CALL(cancel_callback, Run(false))
      .WillOnce(
          base::test::RunOnceClosure(cancel_response_run_loop.QuitClosure()));
  EXPECT_CALL(create_callback, Run(ResponseDataMatches("fake response")))
      .WillOnce(
          base::test::RunOnceClosure(create_response_run_loop.QuitClosure()));

  auto remote = AddReceiverAndPassRemote();
  mojo::Remote<mojom::WebAuthnRequestCanceller> request_canceller;
  remote->Create("fake request", request_canceller.BindNewPipeAndPassReceiver(),
                 create_callback.Get());
  remote.FlushForTesting();

  protocol::RemoteWebAuthn create_request = GetLatestSentMessage();
  ASSERT_EQ(create_request.message_case(),
            protocol::RemoteWebAuthn::kCreateRequest);
  uint64_t create_id = create_request.id();
  request_canceller->Cancel(cancel_callback.Get());
  remote.FlushForTesting();

  protocol::RemoteWebAuthn cancel_request = GetLatestSentMessage();
  ASSERT_EQ(cancel_request.message_case(),
            protocol::RemoteWebAuthn::kCancelRequest);
  uint64_t cancel_id = cancel_request.id();
  ASSERT_EQ(cancel_id, create_id);

  protocol::RemoteWebAuthn cancel_response;
  cancel_response.set_id(cancel_id);
  cancel_response.mutable_cancel_response()->set_was_canceled(false);
  fake_pipe_.ReceiveProtobufMessage(cancel_response);

  cancel_response_run_loop.Run();

  protocol::RemoteWebAuthn create_response;
  create_response.set_id(create_id);
  create_response.mutable_create_response()->set_response_json("fake response");
  fake_pipe_.ReceiveProtobufMessage(create_response);

  create_response_run_loop.Run();
}

TEST_F(RemoteWebAuthnMessageHandlerTest, CancelGet_Success) {
  base::RunLoop run_loop;
  base::MockOnceCallback<void(mojom::WebAuthnGetResponsePtr)> get_callback;
  base::MockOnceCallback<void(bool)> cancel_callback;
  bool cb_run_once = false;
  auto quit_run_loop_on_second_run =
      QuitRunLoopOnSecondRun(cb_run_once, run_loop);
  EXPECT_CALL(get_callback, Run(ResponseErrorNameMatches("AbortError")))
      .WillOnce(quit_run_loop_on_second_run);
  EXPECT_CALL(cancel_callback, Run(true)).WillOnce(quit_run_loop_on_second_run);

  auto remote = AddReceiverAndPassRemote();
  mojo::Remote<mojom::WebAuthnRequestCanceller> request_canceller;
  remote->Get("fake request", request_canceller.BindNewPipeAndPassReceiver(),
              get_callback.Get());
  remote.FlushForTesting();

  protocol::RemoteWebAuthn get_request = GetLatestSentMessage();
  ASSERT_EQ(get_request.message_case(), protocol::RemoteWebAuthn::kGetRequest);
  uint64_t get_id = get_request.id();
  request_canceller->Cancel(cancel_callback.Get());
  remote.FlushForTesting();

  protocol::RemoteWebAuthn cancel_request = GetLatestSentMessage();
  ASSERT_EQ(cancel_request.message_case(),
            protocol::RemoteWebAuthn::kCancelRequest);
  uint64_t cancel_id = cancel_request.id();
  ASSERT_EQ(cancel_id, get_id);

  protocol::RemoteWebAuthn cancel_response;
  cancel_response.set_id(cancel_id);
  cancel_response.mutable_cancel_response()->set_was_canceled(true);
  fake_pipe_.ReceiveProtobufMessage(cancel_response);

  run_loop.Run();
}

TEST_F(RemoteWebAuthnMessageHandlerTest, CancelGet_Failure) {
  base::RunLoop cancel_response_run_loop;
  base::RunLoop get_response_run_loop;
  base::MockOnceCallback<void(mojom::WebAuthnGetResponsePtr)> get_callback;
  base::MockOnceCallback<void(bool)> cancel_callback;
  EXPECT_CALL(cancel_callback, Run(false))
      .WillOnce(
          base::test::RunOnceClosure(cancel_response_run_loop.QuitClosure()));
  EXPECT_CALL(get_callback, Run(ResponseDataMatches("fake response")))
      .WillOnce(
          base::test::RunOnceClosure(get_response_run_loop.QuitClosure()));

  auto remote = AddReceiverAndPassRemote();
  mojo::Remote<mojom::WebAuthnRequestCanceller> request_canceller;
  remote->Get("fake request", request_canceller.BindNewPipeAndPassReceiver(),
              get_callback.Get());
  remote.FlushForTesting();

  protocol::RemoteWebAuthn get_request = GetLatestSentMessage();
  ASSERT_EQ(get_request.message_case(), protocol::RemoteWebAuthn::kGetRequest);
  uint64_t get_id = get_request.id();
  request_canceller->Cancel(cancel_callback.Get());
  remote.FlushForTesting();

  protocol::RemoteWebAuthn cancel_request = GetLatestSentMessage();
  ASSERT_EQ(cancel_request.message_case(),
            protocol::RemoteWebAuthn::kCancelRequest);
  uint64_t cancel_id = cancel_request.id();
  ASSERT_EQ(cancel_id, get_id);

  protocol::RemoteWebAuthn cancel_response;
  cancel_response.set_id(cancel_id);
  cancel_response.mutable_cancel_response()->set_was_canceled(false);
  fake_pipe_.ReceiveProtobufMessage(cancel_response);

  cancel_response_run_loop.Run();

  protocol::RemoteWebAuthn get_response;
  get_response.set_id(get_id);
  get_response.mutable_get_response()->set_response_json("fake response");
  fake_pipe_.ReceiveProtobufMessage(get_response);

  get_response_run_loop.Run();
}

}  // namespace remoting
