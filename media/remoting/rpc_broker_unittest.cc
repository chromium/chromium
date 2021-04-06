// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/rpc_broker.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace media {
namespace remoting {

namespace {

class FakeMessageSender {
 public:
  FakeMessageSender() : received_rpc_(new openscreen::cast::RpcMessage()) {}
  ~FakeMessageSender() = default;

  void OnSendMessageAndQuit(std::unique_ptr<std::vector<uint8_t>> message) {
    EXPECT_TRUE(
        received_rpc_->ParseFromArray(message->data(), message->size()));
    has_sent_message_ = true;
  }

  void OnSendMessage(std::unique_ptr<std::vector<uint8_t>> message) {
    ++send_count_;
  }
  base::WeakPtr<FakeMessageSender> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }
  bool has_sent_message() const { return has_sent_message_; }
  const openscreen::cast::RpcMessage* received_rpc() const {
    return received_rpc_.get();
  }
  int send_count() const { return send_count_; }

 private:
  std::unique_ptr<openscreen::cast::RpcMessage> received_rpc_;
  bool has_sent_message_{false};
  int send_count_{0};
  base::WeakPtrFactory<FakeMessageSender> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeMessageSender);
};

class FakeMessageReceiver {
 public:
  FakeMessageReceiver() = default;
  ~FakeMessageReceiver() = default;

  // RpcBroker::MessageReceiver implementation.
  void OnReceivedRpc(std::unique_ptr<openscreen::cast::RpcMessage> message) {
    received_rpc_ = std::move(message);
    num_received_messages_++;
  }

  void OnSendMessage(std::unique_ptr<std::vector<uint8_t>> message) {}
  base::WeakPtr<FakeMessageReceiver> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }
  int num_received_messages() const { return num_received_messages_; }
  const openscreen::cast::RpcMessage* received_rpc() const {
    return received_rpc_.get();
  }

 private:
  std::unique_ptr<openscreen::cast::RpcMessage> received_rpc_;
  int num_received_messages_{0};
  base::WeakPtrFactory<FakeMessageReceiver> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeMessageReceiver);
};

}  // namespace

class RpcBrokerTest : public testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(RpcBrokerTest, TestProcessMessageFromRemoteRegistered) {
  std::unique_ptr<FakeMessageReceiver> fake_receiver(new FakeMessageReceiver());
  ASSERT_FALSE(fake_receiver->num_received_messages());

  // Creates receiver RpcBroker and registers FakeMessageReceiver.
  std::unique_ptr<RpcBroker> rpc_broker(new RpcBroker(base::BindRepeating(
      &FakeMessageReceiver::OnSendMessage, fake_receiver->GetWeakPtr())));

  int handle = rpc_broker->GetUniqueHandle();
  const RpcBroker::ReceiveMessageCallback receive_callback =
      base::BindRepeating(&FakeMessageReceiver::OnReceivedRpc,
                          fake_receiver->GetWeakPtr());
  rpc_broker->RegisterMessageReceiverCallback(handle, receive_callback);

  std::unique_ptr<openscreen::cast::RpcMessage> rpc(
      new openscreen::cast::RpcMessage());
  rpc->set_handle(handle);
  rpc_broker->ProcessMessageFromRemote(std::move(rpc));
  ASSERT_EQ(fake_receiver->num_received_messages(), 1);
}

TEST_F(RpcBrokerTest, TestProcessMessageFromRemoteUnregistered) {
  std::unique_ptr<FakeMessageReceiver> fake_receiver(new FakeMessageReceiver());
  ASSERT_FALSE(fake_receiver->num_received_messages());

  // Creates receiver RpcBroker and registers FakeMessageReceiver.
  std::unique_ptr<RpcBroker> rpc_broker(new RpcBroker(base::BindRepeating(
      &FakeMessageReceiver::OnSendMessage, fake_receiver->GetWeakPtr())));

  int handle = rpc_broker->GetUniqueHandle();
  const RpcBroker::ReceiveMessageCallback receive_callback =
      base::BindRepeating(&FakeMessageReceiver::OnReceivedRpc,
                          fake_receiver->GetWeakPtr());
  rpc_broker->RegisterMessageReceiverCallback(handle, receive_callback);

  std::unique_ptr<openscreen::cast::RpcMessage> rpc(
      new openscreen::cast::RpcMessage());
  rpc_broker->UnregisterMessageReceiverCallback(handle);
  rpc_broker->ProcessMessageFromRemote(std::move(rpc));
  ASSERT_EQ(fake_receiver->num_received_messages(), 0);
}

TEST_F(RpcBrokerTest, TestSendMessageToRemote) {
  base::test::SingleThreadTaskEnvironment task_environment;

  std::unique_ptr<FakeMessageSender> fake_sender(new FakeMessageSender());
  ASSERT_FALSE(fake_sender->has_sent_message());

  // Creates RpcBroker and set message callback.
  std::unique_ptr<RpcBroker> rpc_broker(new RpcBroker(base::BindRepeating(
      &FakeMessageSender::OnSendMessage, fake_sender->GetWeakPtr())));

  for (int i = 0; i < 10; ++i) {
    std::unique_ptr<openscreen::cast::RpcMessage> rpc(
        new openscreen::cast::RpcMessage());
    rpc_broker->SendMessageToRemote(std::move(rpc));
  }
  EXPECT_EQ(10, fake_sender->send_count());
}

TEST_F(RpcBrokerTest, RpcBrokerSendMessageCallback) {
  base::test::SingleThreadTaskEnvironment task_environment;

  std::unique_ptr<FakeMessageSender> fake_sender(new FakeMessageSender());
  ASSERT_FALSE(fake_sender->has_sent_message());

  // Creates RpcBroker and set message callback.
  std::unique_ptr<RpcBroker> rpc_broker(new RpcBroker(base::BindRepeating(
      &FakeMessageSender::OnSendMessageAndQuit, fake_sender->GetWeakPtr())));

  // Sends RPC message.
  std::unique_ptr<openscreen::cast::RpcMessage> sent_rpc(
      new openscreen::cast::RpcMessage());
  sent_rpc->set_handle(2);
  sent_rpc->set_proc(openscreen::cast::RpcMessage::RPC_R_SETVOLUME);
  sent_rpc->set_double_value(2.2);
  rpc_broker->SendMessageToRemote(std::move(sent_rpc));

  // Wait for message callback.
  // message_loop->Run();
  base::RunLoop().RunUntilIdle();

  // Check if received message is identical to the one sent earlier.
  ASSERT_TRUE(fake_sender->has_sent_message());
  const auto* received_rpc = fake_sender->received_rpc();
  ASSERT_EQ(2, received_rpc->handle());
  ASSERT_EQ(openscreen::cast::RpcMessage::RPC_R_SETVOLUME,
            received_rpc->proc());
  ASSERT_EQ(2.2, received_rpc->double_value());
}

TEST_F(RpcBrokerTest, RpcBrokerProcessMessageWithRegisteredHandle) {
  std::unique_ptr<FakeMessageReceiver> fake_receiver(new FakeMessageReceiver());
  ASSERT_FALSE(fake_receiver->num_received_messages());

  // Creates receiver RpcBroker and registers FakeMessageReceiver.
  std::unique_ptr<RpcBroker> rpc_broker(new RpcBroker(base::BindRepeating(
      &FakeMessageReceiver::OnSendMessage, fake_receiver->GetWeakPtr())));
  int handle = rpc_broker->GetUniqueHandle();
  const RpcBroker::ReceiveMessageCallback receive_callback =
      base::BindRepeating(&FakeMessageReceiver::OnReceivedRpc,
                          fake_receiver->GetWeakPtr());
  rpc_broker->RegisterMessageReceiverCallback(handle, receive_callback);

  // Generates RPC message with handle value |handle| and send it to recover
  // RpcBroker to process.
  std::unique_ptr<openscreen::cast::RpcMessage> sent_rpc(
      new openscreen::cast::RpcMessage());
  sent_rpc->set_handle(handle);
  sent_rpc->set_proc(openscreen::cast::RpcMessage::RPC_R_SETVOLUME);
  sent_rpc->set_double_value(2.2);
  rpc_broker->ProcessMessageFromRemote(std::move(sent_rpc));

  // Checks if received message is identical to the one sent earlier.
  ASSERT_TRUE(fake_receiver->num_received_messages());
  auto* received_rpc = fake_receiver->received_rpc();
  ASSERT_EQ(handle, received_rpc->handle());
  ASSERT_EQ(openscreen::cast::RpcMessage::RPC_R_SETVOLUME,
            received_rpc->proc());
  ASSERT_EQ(2.2, received_rpc->double_value());

  // Unregisters FakeMessageReceiver.
  rpc_broker->UnregisterMessageReceiverCallback(handle);
}

TEST_F(RpcBrokerTest, RpcBrokerProcessMessageWithUnregisteredHandle) {
  std::unique_ptr<FakeMessageReceiver> fake_receiver(new FakeMessageReceiver());
  ASSERT_FALSE(fake_receiver->num_received_messages());

  // Creates receiver RpcBroker and registers FakeMessageReceiver.
  std::unique_ptr<RpcBroker> rpc_broker(new RpcBroker(base::BindRepeating(
      &FakeMessageReceiver::OnSendMessage, fake_receiver->GetWeakPtr())));
  int handle = rpc_broker->GetUniqueHandle();
  const RpcBroker::ReceiveMessageCallback receive_callback =
      base::BindRepeating(&FakeMessageReceiver::OnReceivedRpc,
                          fake_receiver->GetWeakPtr());
  rpc_broker->RegisterMessageReceiverCallback(handle, receive_callback);

  // Generates RPC message with handle value |handle| and send it to recover
  // RpcBroker to process.
  std::unique_ptr<openscreen::cast::RpcMessage> sent_rpc(
      new openscreen::cast::RpcMessage());
  int different_handle = handle + 1;
  sent_rpc->set_handle(different_handle);
  sent_rpc->set_proc(openscreen::cast::RpcMessage::RPC_R_SETVOLUME);
  sent_rpc->set_double_value(2.2);
  rpc_broker->ProcessMessageFromRemote(std::move(sent_rpc));

  // Check if received message is identical to the one sent earlier.
  ASSERT_FALSE(fake_receiver->num_received_messages());

  // Unregisters FakeMessageReceiver.
  rpc_broker->UnregisterMessageReceiverCallback(handle);
}

}  // namespace remoting
}  // namespace media
