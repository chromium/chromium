// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_open_url/remote_open_url_message_handler.h"

#include <cstdint>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/host/mojom/remote_url_opener.mojom.h"
#include "remoting/proto/remote_open_url.pb.h"
#include "remoting/protocol/fake_message_pipe.h"
#include "remoting/protocol/fake_message_pipe_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace remoting {

namespace {

protocol::RemoteOpenUrl ParseMessage(const std::string& data) {
  protocol::RemoteOpenUrl message;
  message.ParseFromString(data);
  return message;
}

inline auto QuitRunLoop(base::RunLoop& run_loop) {
  return base::test::RunOnceClosure(run_loop.QuitClosure());
}

}  // namespace

class RemoteOpenUrlMessageHandlerTest : public testing::Test {
 public:
  RemoteOpenUrlMessageHandlerTest();
  ~RemoteOpenUrlMessageHandlerTest() override;

 protected:
  mojo::ReceiverId OpenUrl(mojo::Remote<mojom::RemoteUrlOpener>& remote,
                           const GURL& url,
                           mojom::RemoteUrlOpener::OpenUrlCallback callback);

  bool HasPendingCallbacks();
  void FlushReceivers();

  base::test::TaskEnvironment task_environment_;
  protocol::FakeMessagePipe fake_pipe_{/* asynchronous= */ false};
  raw_ptr<RemoteOpenUrlMessageHandler, DanglingUntriaged> message_handler_;
};

RemoteOpenUrlMessageHandlerTest::RemoteOpenUrlMessageHandlerTest() {
  // Lifetime of |message_handler_| is controlled by the |fake_pipe_|.
  message_handler_ =
      new RemoteOpenUrlMessageHandler("fake name", fake_pipe_.Wrap());
  fake_pipe_.OpenPipe();
}

RemoteOpenUrlMessageHandlerTest::~RemoteOpenUrlMessageHandlerTest() {
  if (fake_pipe_.pipe_opened()) {
    // Make sure there is no lingering receiver or callback after OpenUrl
    // responses are sent.
    EXPECT_TRUE(message_handler_->receivers_.empty());
    EXPECT_TRUE(message_handler_->callbacks_.empty());
    fake_pipe_.ClosePipe();
  }
}

mojo::ReceiverId RemoteOpenUrlMessageHandlerTest::OpenUrl(
    mojo::Remote<mojom::RemoteUrlOpener>& remote,
    const GURL& url,
    mojom::RemoteUrlOpener::OpenUrlCallback callback) {
  EXPECT_FALSE(remote.is_bound());
  mojo::ReceiverId receiver_id = message_handler_->AddReceiverAndGetReceiverId(
      remote.BindNewPipeAndPassReceiver());
  remote->OpenUrl(url, std::move(callback));
  remote.FlushForTesting();
  return receiver_id;
}

bool RemoteOpenUrlMessageHandlerTest::HasPendingCallbacks() {
  return !message_handler_->callbacks_.empty();
}

void RemoteOpenUrlMessageHandlerTest::FlushReceivers() {
  message_handler_->receivers_.FlushForTesting();
}

TEST_F(RemoteOpenUrlMessageHandlerTest, OpenUrl) {
  base::RunLoop run_loop;
  base::MockCallback<mojom::RemoteUrlOpener::OpenUrlCallback> callback;
  EXPECT_CALL(callback, Run(mojom::OpenUrlResult::SUCCESS))
      .WillOnce(QuitRunLoop(run_loop));

  mojo::Remote<mojom::RemoteUrlOpener> remote;
  mojo::ReceiverId receiver_id =
      OpenUrl(remote, GURL("http://google.com/"), callback.Get());
  protocol::RemoteOpenUrl response;
  response.mutable_open_url_response()->set_id(receiver_id);
  response.mutable_open_url_response()->set_result(
      protocol::RemoteOpenUrl::OpenUrlResponse::SUCCESS);
  fake_pipe_.ReceiveProtobufMessage(response);
  run_loop.Run();

  protocol::RemoteOpenUrl request_message =
      ParseMessage(fake_pipe_.sent_messages().front());
  ASSERT_TRUE(request_message.has_open_url_request());
  ASSERT_EQ(receiver_id, request_message.open_url_request().id());
  ASSERT_EQ("http://google.com/", request_message.open_url_request().url());
}

TEST_F(RemoteOpenUrlMessageHandlerTest, OpenInvalidUrl_Failure) {
  base::RunLoop run_loop;
  base::MockCallback<mojom::RemoteUrlOpener::OpenUrlCallback> callback;
  EXPECT_CALL(callback, Run(mojom::OpenUrlResult::FAILURE))
      .WillOnce(QuitRunLoop(run_loop));

  mojo::Remote<mojom::RemoteUrlOpener> remote;
  OpenUrl(remote, GURL("invalid_url"), callback.Get());
  run_loop.Run();

  ASSERT_EQ(0u, fake_pipe_.sent_messages().size());
}

TEST_F(RemoteOpenUrlMessageHandlerTest, OpenMultipleUrls) {
  base::RunLoop run_loop_1;
  base::MockCallback<mojom::RemoteUrlOpener::OpenUrlCallback> url_1_callback;
  EXPECT_CALL(url_1_callback, Run(mojom::OpenUrlResult::SUCCESS))
      .WillOnce(QuitRunLoop(run_loop_1));

  base::RunLoop run_loop_2;
  base::MockCallback<mojom::RemoteUrlOpener::OpenUrlCallback> url_2_callback;
  EXPECT_CALL(url_2_callback, Run(mojom::OpenUrlResult::FAILURE))
      .WillOnce(QuitRunLoop(run_loop_2));

  mojo::Remote<mojom::RemoteUrlOpener> remote_1;
  auto receiver_id_1 =
      OpenUrl(remote_1, GURL("http://google.com/url1"), url_1_callback.Get());
  mojo::Remote<mojom::RemoteUrlOpener> remote_2;
  auto receiver_id_2 =
      OpenUrl(remote_2, GURL("http://google.com/url2"), url_2_callback.Get());

  protocol::RemoteOpenUrl response;
  response.mutable_open_url_response()->set_id(receiver_id_1);
  response.mutable_open_url_response()->set_result(
      protocol::RemoteOpenUrl::OpenUrlResponse::SUCCESS);
  fake_pipe_.ReceiveProtobufMessage(response);

  response.mutable_open_url_response()->set_id(receiver_id_2);
  response.mutable_open_url_response()->set_result(
      protocol::RemoteOpenUrl::OpenUrlResponse::FAILURE);
  fake_pipe_.ReceiveProtobufMessage(response);

  run_loop_1.Run();
  run_loop_2.Run();

  base::queue<std::string> sent_messages = fake_pipe_.sent_messages();
  ASSERT_EQ(2u, sent_messages.size());

  protocol::RemoteOpenUrl request_message_1 =
      ParseMessage(sent_messages.front());
  ASSERT_TRUE(request_message_1.has_open_url_request());
  ASSERT_EQ(receiver_id_1, request_message_1.open_url_request().id());
  ASSERT_EQ("http://google.com/url1",
            request_message_1.open_url_request().url());
  sent_messages.pop();

  protocol::RemoteOpenUrl request_message_2 =
      ParseMessage(sent_messages.front());
  ASSERT_TRUE(request_message_2.has_open_url_request());
  ASSERT_EQ(receiver_id_2, request_message_2.open_url_request().id());
  ASSERT_EQ("http://google.com/url2",
            request_message_2.open_url_request().url());
}

TEST_F(RemoteOpenUrlMessageHandlerTest,
       DisconnectRemote_CallbackSilentlyDropped) {
  mojo::Remote<mojom::RemoteUrlOpener> remote;
  auto receiver_id =
      OpenUrl(remote, GURL("http://google.com/"), base::DoNothing());

  protocol::RemoteOpenUrl request_message =
      ParseMessage(fake_pipe_.sent_messages().front());
  ASSERT_TRUE(request_message.has_open_url_request());
  ASSERT_EQ(receiver_id, request_message.open_url_request().id());
  ASSERT_EQ("http://google.com/", request_message.open_url_request().url());
  ASSERT_TRUE(HasPendingCallbacks());

  remote.reset();
  FlushReceivers();
  ASSERT_FALSE(HasPendingCallbacks());
}

TEST_F(RemoteOpenUrlMessageHandlerTest,
       DisconnectMessagePipe_PendingCallbacksRunWithLocalFallback) {
  base::RunLoop run_loop;
  base::MockCallback<mojom::RemoteUrlOpener::OpenUrlCallback> callback;
  EXPECT_CALL(callback, Run(mojom::OpenUrlResult::LOCAL_FALLBACK))
      .WillOnce(QuitRunLoop(run_loop));

  mojo::Remote<mojom::RemoteUrlOpener> remote;
  OpenUrl(remote, GURL("http://google.com/url1"), callback.Get());
  fake_pipe_.ClosePipe();
  run_loop.Run();
}

}  // namespace remoting
