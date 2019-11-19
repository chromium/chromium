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
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "media/remoting/media_remoting_rpc.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace media {
namespace remoting {

namespace {

class FakeMessageSender {
 public:
  FakeMessageSender()
      : received_rpc_(new pb::RpcMessage()),
        has_sent_message_(false),
        send_count_(0) {}
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
  const pb::RpcMessage* received_rpc() const { return received_rpc_.get(); }
  int send_count() const { return send_count_; }

 private:
  std::unique_ptr<pb::RpcMessage> received_rpc_;
  bool has_sent_message_;
  int send_count_;
  base::WeakPtrFactory<FakeMessageSender> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeMessageSender);
};

class FakeMessageReceiver {
 public:
  FakeMessageReceiver() : has_received_message_(false) {}
  ~FakeMessageReceiver() = default;

  // RpcBroker::MessageReceiver implementation.
  void OnReceivedRpc(std::unique_ptr<pb::RpcMessage> message) {
    received_rpc_ = std::move(message);
    has_received_message_++;
  }

  void OnSendMessage(std::unique_ptr<std::vector<uint8_t>> message) {}
  base::WeakPtr<FakeMessageReceiver> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }
  int has_received_message() const { return has_received_message_; }
  const pb::RpcMessage* received_rpc() const { return received_rpc_.get(); }

 private:
  std::unique_ptr<pb::RpcMessage> received_rpc_;
  int has_received_message_;
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
  ASSERT_FALSE(fake_receiver->has_received_message());

  // Creates receiver RpcBroker and registers FakeMessageReceiver.
  std::unique_ptr<RpcBroker> rpc_broker(new RpcBroker(base::Bind(
      &FakeMessageReceiver::OnSendMessage, fake_receiver->GetWeakPtr())));

  int handle = rpc_broker->GetUniqueHandle();
  const RpcBroker::ReceiveMessageCallback receive_callback =
      base::BindRepeating(&FakeMessageReceiver::OnReceivedRpc,
                          fake_receiver->GetWeakPtr());
  rpc_broker->RegisterMessageReceiverCallback(handle, receive_callback);

  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  rpc->set_handle(handle);
  rpc_broker->ProcessMessageFromRemote(std::move(rpc));
  ASSERT_EQ(fake_receiver->has_received_message(), 1);
}

TEST_F(RpcBrokerTest, TestProcessMessageFromRemoteUnregistered) {
  std::unique_ptr<FakeMessageReceiver> fake_receiver(new FakeMessageReceiver());
  ASSERT_FALSE(fake_receiver->has_received_message());

  // Creates receiver RpcBroker and registers FakeMessageReceiver.
  std::unique_ptr<RpcBroker> rpc_broker(new RpcBroker(base::Bind(
      &FakeMessageReceiver::OnSendMessage, fake_receiver->GetWeakPtr())));

  int handle = rpc_broker->GetUniqueHandle();
  const RpcBroker::ReceiveMessageCallback receive_callback =
      base::BindRepeating(&FakeMessageReceiver::OnReceivedRpc,
                          fake_receiver->GetWeakPtr());
  rpc_broker->RegisterMessageReceiverCallback(handle, receive_callback);

  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  rpc_broker->UnregisterMessageReceiverCallback(handle);
  rpc_broker->ProcessMessageFromRemote(std::move(rpc));
  ASSERT_EQ(fake_receiver->has_received_message(), 0);
}

TEST_F(RpcBrokerTest, TestSendMessageToRemote) {
  std::unique_ptr<base::MessageLoop> message_loop(new base::MessageLoop());

  std::unique_ptr<FakeMessageSender> fake_sender(new FakeMessageSender());
  ASSERT_FALSE(fake_sender->has_sent_message());

  // Creates RpcBroker and set message callback.
  std::unique_ptr<RpcBroker> rpc_broker(new RpcBroker(base::Bind(
      &FakeMessageSender::OnSendMessage, fake_sender->GetWeakPtr())));

  for (int i = 0; i < 10; ++i) {
    std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
    rpc_broker->SendMessageToRemote(std::move(rpc));
  }
  EXPECT_EQ(10, fake_sender->send_count());
}

TEST_F(RpcBrokerTest, RpcBrokerSendMessageCallback) {
  std::unique_ptr<base::MessageLoop> message_loop(new base::MessageLoop());

  std::unique_ptr<FakeMessageSender> fake_sender(new FakeMessageSender());
  ASSERT_FALSE(fake_sender->has_sent_message());

  // Creates RpcBroker and set message callback.
  std::unique_ptr<RpcBroker> rpc_broker(new RpcBroker(base::Bind(
      &FakeMessageSender::OnSendMessageAndQuit, fake_sender->GetWeakPtr())));

  // Sends RPC message.
  std::unique_ptr<pb::RpcMessage> sent_rpc(new pb::RpcMessage());
  sent_rpc->set_handle(2);
  sent_rpc->set_proc(pb::RpcMessage::RPC_R_SETVOLUME);
  sent_rpc->set_double_value(2.2);
  rpc_broker->SendMessageToRemote(std::move(sent_rpc));

  // Wait for messge callback.
  // message_loop->Run();
  base::RunLoop().RunUntilIdle();

  // Check if received message is identical to the one sent earlier.
  ASSERT_TRUE(fake_sender->has_sent_message());
  const auto* received_rpc = fake_sender->received_rpc();
  ASSERT_EQ(2, received_rpc->handle());
  ASSERT_EQ(pb::RpcMessage::RPC_R_SETVOLUME, received_rpc->proc());
  ASSERT_EQ(2.2, received_rpc->double_value());
}

TEST_F(RpcBrokerTest, RpcBrokerProcessMessageWithRegisteredHandle) {
  std::unique_ptr<FakeMessageReceiver> fake_receiver(new FakeMessageReceiver());
  ASSERT_FALSE(fake_receiver->has_received_message());

  // Creates receiver RpcBroker and registers FakeMessageReceiver.
  std::unique_ptr<RpcBroker> rpc_broker(new RpcBroker(base::Bind(
      &FakeMessageReceiver::OnSendMessage, fake_receiver->GetWeakPtr())));
  int handle = rpc_broker->GetUniqueHandle();
  const RpcBroker::ReceiveMessageCallback receive_callback =
      base::BindRepeating(&FakeMessageReceiver::OnReceivedRpc,
                          fake_receiver->GetWeakPtr());
  rpc_broker->RegisterMessageReceiverCallback(handle, receive_callback);

  // Generates RPC message with handle value |handle| and send it to receover
  // RpcBroker to process.
  std::unique_ptr<pb::RpcMessage> sent_rpc(new pb::RpcMessage());
  sent_rpc->set_handle(handle);
  sent_rpc->set_proc(pb::RpcMessage::RPC_R_SETVOLUME);
  sent_rpc->set_double_value(2.2);
  rpc_broker->ProcessMessageFromRemote(std::move(sent_rpc));

  // Checks if received message is identical to the one sent earlier.
  ASSERT_TRUE(fake_receiver->has_received_message());
  auto* received_rpc = fake_receiver->received_rpc();
  ASSERT_EQ(handle, received_rpc->handle());
  ASSERT_EQ(pb::RpcMessage::RPC_R_SETVOLUME, received_rpc->proc());
  ASSERT_EQ(2.2, received_rpc->double_value());

  // Unregisters FakeMessageReceiver.
  rpc_broker->UnregisterMessageReceiverCallback(handle);
}

TEST_F(RpcBrokerTest, RpcBrokerProcessMessageWithUnregisteredHandle) {
  std::unique_ptr<FakeMessageReceiver> fake_receiver(new FakeMessageReceiver());
  ASSERT_FALSE(fake_receiver->has_received_message());

  // Creates receiver RpcBroker and registers FakeMessageReceiver.
  std::unique_ptr<RpcBroker> rpc_broker(new RpcBroker(base::Bind(
      &FakeMessageReceiver::OnSendMessage, fake_receiver->GetWeakPtr())));
  int handle = rpc_broker->GetUniqueHandle();
  const RpcBroker::ReceiveMessageCallback receive_callback =
      base::BindRepeating(&FakeMessageReceiver::OnReceivedRpc,
                          fake_receiver->GetWeakPtr());
  rpc_broker->RegisterMessageReceiverCallback(handle, receive_callback);

  // Generates RPC message with handle value |handle| and send it to receover
  // RpcBroker to process.
  std::unique_ptr<pb::RpcMessage> sent_rpc(new pb::RpcMessage());
  int different_handle = handle + 1;
  sent_rpc->set_handle(different_handle);
  sent_rpc->set_proc(pb::RpcMessage::RPC_R_SETVOLUME);
  sent_rpc->set_double_value(2.2);
  rpc_broker->ProcessMessageFromRemote(std::move(sent_rpc));

  // Check if received message is identical to the one sent earlier.
  ASSERT_FALSE(fake_receiver->has_received_message());

  // Unregisters FakeMessageReceiver.
  rpc_broker->UnregisterMessageReceiverCallback(handle);
}

}  // namespace remoting
}  // namespace media
