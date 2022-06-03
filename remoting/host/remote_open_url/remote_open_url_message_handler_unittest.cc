// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_open_url/remote_open_url_message_handler.h"

#include <cstdint>
#include <memory>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/mock_callback.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/base/io_buffer.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/host/mojo_ipc/fake_ipc_server.h"
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

std::unique_ptr<CompoundBuffer> MessageToBuffer(
    const protocol::RemoteOpenUrl& message) {
  auto buffer = std::make_unique<CompoundBuffer>();
  std::string data = message.SerializeAsString();
  buffer->Append(base::MakeRefCounted<net::StringIOBuffer>(data.data()),
                 data.size());
  return buffer;
}

}  // namespace

class RemoteOpenUrlMessageHandlerTest : public testing::Test {
 public:
  RemoteOpenUrlMessageHandlerTest();
  ~RemoteOpenUrlMessageHandlerTest() override;

 protected:
  void OpenUrl(mojo::ReceiverId receiver_id,
               const GURL& url,
               mojom::RemoteUrlOpener::OpenUrlCallback callback);

  protocol::FakeMessagePipe fake_pipe_{/* asynchronous= */ false};
  FakeIpcServer::TestState ipc_server_state_;
  RemoteOpenUrlMessageHandler* message_handler_;
};

RemoteOpenUrlMessageHandlerTest::RemoteOpenUrlMessageHandlerTest() {
  // Lifetime of |message_handler_| is controlled by the |fake_pipe_|.
  message_handler_ = new RemoteOpenUrlMessageHandler(
      "fake name", fake_pipe_.Wrap(),
      std::make_unique<FakeIpcServer>(&ipc_server_state_));
  fake_pipe_.OpenPipe();
  EXPECT_TRUE(ipc_server_state_.is_server_started);
}

RemoteOpenUrlMessageHandlerTest::~RemoteOpenUrlMessageHandlerTest() {
  if (fake_pipe_.pipe_opened()) {
    fake_pipe_.ClosePipe();
    EXPECT_FALSE(ipc_server_state_.is_server_started);
  }
}

void RemoteOpenUrlMessageHandlerTest::OpenUrl(
    mojo::ReceiverId receiver_id,
    const GURL& url,
    mojom::RemoteUrlOpener::OpenUrlCallback callback) {
  ipc_server_state_.current_receiver = receiver_id;
  message_handler_->OpenUrl(url, std::move(callback));
}

TEST_F(RemoteOpenUrlMessageHandlerTest, OpenUrl) {
  base::MockCallback<mojom::RemoteUrlOpener::OpenUrlCallback> callback;
  EXPECT_CALL(callback, Run(mojom::OpenUrlResult::SUCCESS)).Times(1);

  const uint64_t receiver_id = 1u;
  OpenUrl(receiver_id, GURL("http://google.com/"), callback.Get());
  protocol::RemoteOpenUrl response;
  response.mutable_open_url_response()->set_id(receiver_id);
  response.mutable_open_url_response()->set_result(
      protocol::RemoteOpenUrl::OpenUrlResponse::SUCCESS);
  fake_pipe_.Receive(MessageToBuffer(response));

  protocol::RemoteOpenUrl request_message =
      ParseMessage(fake_pipe_.sent_messages().front());
  ASSERT_TRUE(request_message.has_open_url_request());
  ASSERT_EQ(receiver_id, request_message.open_url_request().id());
  ASSERT_EQ("http://google.com/", request_message.open_url_request().url());
  ASSERT_EQ(receiver_id, ipc_server_state_.last_closed_receiver);
}

TEST_F(RemoteOpenUrlMessageHandlerTest, OpenInvalidUrl_Failure) {
  base::MockCallback<mojom::RemoteUrlOpener::OpenUrlCallback> callback;
  EXPECT_CALL(callback, Run(mojom::OpenUrlResult::FAILURE)).Times(1);

  OpenUrl(1u, GURL("invalid_url"), callback.Get());

  ASSERT_EQ(0u, fake_pipe_.sent_messages().size());
}

TEST_F(RemoteOpenUrlMessageHandlerTest, OpenMultipleUrls) {
  base::MockCallback<mojom::RemoteUrlOpener::OpenUrlCallback> url_1_callback;
  EXPECT_CALL(url_1_callback, Run(mojom::OpenUrlResult::SUCCESS)).Times(1);

  base::MockCallback<mojom::RemoteUrlOpener::OpenUrlCallback> url_2_callback;
  EXPECT_CALL(url_2_callback, Run(mojom::OpenUrlResult::FAILURE)).Times(1);

  OpenUrl(1u, GURL("http://google.com/url1"), url_1_callback.Get());
  OpenUrl(2u, GURL("http://google.com/url2"), url_2_callback.Get());

  protocol::RemoteOpenUrl response;
  response.mutable_open_url_response()->set_id(1u);
  response.mutable_open_url_response()->set_result(
      protocol::RemoteOpenUrl::OpenUrlResponse::SUCCESS);
  fake_pipe_.Receive(MessageToBuffer(response));
  ASSERT_EQ(1u, ipc_server_state_.last_closed_receiver);

  response.mutable_open_url_response()->set_id(2u);
  response.mutable_open_url_response()->set_result(
      protocol::RemoteOpenUrl::OpenUrlResponse::FAILURE);
  fake_pipe_.Receive(MessageToBuffer(response));
  ASSERT_EQ(2u, ipc_server_state_.last_closed_receiver);

  base::queue<std::string> sent_messages = fake_pipe_.sent_messages();
  ASSERT_EQ(2u, sent_messages.size());

  protocol::RemoteOpenUrl request_message_1 =
      ParseMessage(sent_messages.front());
  ASSERT_TRUE(request_message_1.has_open_url_request());
  ASSERT_EQ(1u, request_message_1.open_url_request().id());
  ASSERT_EQ("http://google.com/url1",
            request_message_1.open_url_request().url());
  sent_messages.pop();

  protocol::RemoteOpenUrl request_message_2 =
      ParseMessage(sent_messages.front());
  ASSERT_TRUE(request_message_2.has_open_url_request());
  ASSERT_EQ(2u, request_message_2.open_url_request().id());
  ASSERT_EQ("http://google.com/url2",
            request_message_2.open_url_request().url());
}

TEST_F(RemoteOpenUrlMessageHandlerTest,
       DisconnectReceiver_CallbackSilentlyDropped) {
  // This test is to make sure that callbacks will be dropped immediately after
  // the the corresponding IPC is disconnected, which is to prevent holding the
  // callback until the message handler is destroyed in cases like the server
  // never responds and the client quits due to timeout.
  class DestructionDetector {
   public:
    explicit DestructionDetector(bool* out_is_destructor_called)
        : out_is_destructor_called_(out_is_destructor_called) {}

    ~DestructionDetector() { *out_is_destructor_called_ = true; }

   private:
    bool* out_is_destructor_called_;
  };

  bool is_callback_destroyed = false;
  // The ownership of |DestructionDetector| is passed to the callback, so it is
  // used here to detect whether the callback itself has been destroyed.
  auto destruction_detecting_callback = base::BindOnce(
      [](std::unique_ptr<DestructionDetector> destruction_detector,
         mojom::OpenUrlResult unused) {
        FAIL() << "Callback should be silently dropped.";
      },
      std::make_unique<DestructionDetector>(&is_callback_destroyed));

  OpenUrl(1u, GURL("http://google.com/url1"),
          std::move(destruction_detecting_callback));
  ipc_server_state_.current_receiver = 1u;
  ipc_server_state_.disconnect_handler.Run();

  ASSERT_TRUE(is_callback_destroyed);
}

TEST_F(RemoteOpenUrlMessageHandlerTest,
       DisconnectMessagePipe_PendingCallbacksRunWithLocalFallback) {
  base::MockCallback<mojom::RemoteUrlOpener::OpenUrlCallback> callback;
  EXPECT_CALL(callback, Run(mojom::OpenUrlResult::LOCAL_FALLBACK)).Times(1);

  OpenUrl(1u, GURL("http://google.com/url1"), callback.Get());
  fake_pipe_.ClosePipe();
}

}  // namespace remoting
