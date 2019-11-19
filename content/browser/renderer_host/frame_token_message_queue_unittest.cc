// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/frame_token_message_queue.h"

#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// Test verision of FrameTokenMessageQueue::Client which tracks the number of
// calls to client methods, and the associated input parameters.
class TestFrameTokenMessageQueueClient : public FrameTokenMessageQueue::Client {
 public:
  TestFrameTokenMessageQueueClient() {}
  ~TestFrameTokenMessageQueueClient() {}

  // Resets all method counters.
  void Reset();

  // All subsequent IPC::Messages received in OnProcessSwapMessage will be
  // marked as having a dispatch error.
  void SetErrorOnMessageProcess();

  // FrameTokenMessageQueue::Client:
  void OnInvalidFrameToken(uint32_t frame_token) override;
  void OnMessageDispatchError(const IPC::Message& message) override;
  void OnProcessSwapMessage(const IPC::Message& message) override;

  bool invalid_frame_token_called() const {
    return invalid_frame_token_called_;
  }
  uint32_t invalid_frame_token() const { return invalid_frame_token_; }
  int on_message_dispatch_error_count() const {
    return on_message_dispatch_error_count_;
  }
  int on_process_swap_message_count() const {
    return on_process_swap_message_count_;
  }

 private:
  // If true the each IPC::Message received will be marked as having a dispatch
  // error.
  bool set_error_on_process_ = false;
  bool invalid_frame_token_called_ = false;
  uint32_t invalid_frame_token_ = 0u;
  int on_message_dispatch_error_count_ = 0;
  int on_process_swap_message_count_ = 0;
  DISALLOW_COPY_AND_ASSIGN(TestFrameTokenMessageQueueClient);
};

void TestFrameTokenMessageQueueClient::Reset() {
  invalid_frame_token_called_ = false;
  invalid_frame_token_ = 0u;
  on_message_dispatch_error_count_ = 0;
  on_process_swap_message_count_ = 0;
}

void TestFrameTokenMessageQueueClient::SetErrorOnMessageProcess() {
  set_error_on_process_ = true;
}

void TestFrameTokenMessageQueueClient::OnInvalidFrameToken(
    uint32_t frame_token) {
  invalid_frame_token_called_ = true;
  invalid_frame_token_ = frame_token;
}

void TestFrameTokenMessageQueueClient::OnMessageDispatchError(
    const IPC::Message& message) {
  ++on_message_dispatch_error_count_;
}

void TestFrameTokenMessageQueueClient::OnProcessSwapMessage(
    const IPC::Message& message) {
  if (set_error_on_process_)
    message.set_dispatch_error();
  ++on_process_swap_message_count_;
}

// Test class which provides FrameTokenCallback() to be used as a closure when
// enqueueing non-IPC callbacks. This only tracks if the callback was called.
class TestNonIPCMessageEnqueuer {
 public:
  TestNonIPCMessageEnqueuer() {}
  ~TestNonIPCMessageEnqueuer() {}

  void FrameTokenCallback();

  bool frame_token_callback_called() const {
    return frame_token_callback_called_;
  }

 private:
  bool frame_token_callback_called_ = false;
  DISALLOW_COPY_AND_ASSIGN(TestNonIPCMessageEnqueuer);
};

void TestNonIPCMessageEnqueuer::FrameTokenCallback() {
  frame_token_callback_called_ = true;
}

}  // namespace

class FrameTokenMessageQueueTest : public testing::Test {
 public:
  FrameTokenMessageQueueTest();
  ~FrameTokenMessageQueueTest() override {}

  TestFrameTokenMessageQueueClient* test_client() { return &test_client_; }
  TestNonIPCMessageEnqueuer* test_non_ipc_enqueuer() {
    return &test_non_ipc_enqueuer_;
  }
  FrameTokenMessageQueue* frame_token_message_queue() {
    return &frame_token_message_queue_;
  }

 private:
  TestFrameTokenMessageQueueClient test_client_;
  TestNonIPCMessageEnqueuer test_non_ipc_enqueuer_;
  FrameTokenMessageQueue frame_token_message_queue_;
  DISALLOW_COPY_AND_ASSIGN(FrameTokenMessageQueueTest);
};

FrameTokenMessageQueueTest::FrameTokenMessageQueueTest() {
  frame_token_message_queue_.Init(&test_client_);
}

// Tests that if a valid IPC::Message is enqueued, that it is processed when its
// matching frame token arrives.
TEST_F(FrameTokenMessageQueueTest, OnlyIPCMessageCorrectFrameToken) {
  FrameTokenMessageQueue* queue = frame_token_message_queue();
  TestFrameTokenMessageQueueClient* client = test_client();
  ASSERT_EQ(0u, queue->size());

  const uint32_t frame_token = 42;
  IPC::Message msg(0, 1, IPC::Message::PRIORITY_NORMAL);
  std::vector<IPC::Message> messages;
  messages.push_back(msg);

  // Adding to the queue with a new frame token should not cause processing.
  queue->OnFrameSwapMessagesReceived(frame_token, std::move(messages));
  EXPECT_EQ(1u, queue->size());
  EXPECT_EQ(0, client->on_process_swap_message_count());

  queue->DidProcessFrame(frame_token);
  EXPECT_EQ(0u, queue->size());
  EXPECT_FALSE(client->invalid_frame_token_called());
  EXPECT_EQ(1, client->on_process_swap_message_count());
  EXPECT_EQ(0, client->on_message_dispatch_error_count());
}

// Tests that if a valid IPC::Message is enqueued after its frame token has
// arrived that it is processed immediately.
TEST_F(FrameTokenMessageQueueTest, EnqueueAfterFrameTokenProcesses) {
  FrameTokenMessageQueue* queue = frame_token_message_queue();
  TestFrameTokenMessageQueueClient* client = test_client();
  ASSERT_EQ(0u, queue->size());

  const uint32_t frame_token = 42;
  IPC::Message msg(0, 1, IPC::Message::PRIORITY_NORMAL);
  std::vector<IPC::Message> messages;
  messages.push_back(msg);

  queue->DidProcessFrame(frame_token);
  EXPECT_EQ(0u, queue->size());
  EXPECT_FALSE(client->invalid_frame_token_called());
  EXPECT_EQ(0, client->on_process_swap_message_count());
  EXPECT_EQ(0, client->on_message_dispatch_error_count());

  // Enqueuing after frame token arrival should immediately process.
  queue->OnFrameSwapMessagesReceived(frame_token, std::move(messages));
  EXPECT_EQ(0u, queue->size());
  EXPECT_FALSE(client->invalid_frame_token_called());
  EXPECT_EQ(1, client->on_process_swap_message_count());
  EXPECT_EQ(0, client->on_message_dispatch_error_count());
}

// Tests that if a valid IPC::Message is enqueued and that subsequently a
// non-IPC callback is enqueued, that both get called once the frame token
// arrives.
TEST_F(FrameTokenMessageQueueTest, EnqueueBothIPCMessageAndNonIPCCallback) {
  FrameTokenMessageQueue* queue = frame_token_message_queue();
  TestFrameTokenMessageQueueClient* client = test_client();
  TestNonIPCMessageEnqueuer* enqueuer = test_non_ipc_enqueuer();
  ASSERT_EQ(0u, queue->size());

  const uint32_t frame_token = 42;
  IPC::Message msg(0, 1, IPC::Message::PRIORITY_NORMAL);
  std::vector<IPC::Message> messages;
  messages.push_back(msg);

  // Adding to the queue with a new frame token should not cause processing.
  queue->OnFrameSwapMessagesReceived(frame_token, std::move(messages));
  EXPECT_EQ(1u, queue->size());
  EXPECT_EQ(0, client->on_process_swap_message_count());

  queue->EnqueueOrRunFrameTokenCallback(
      frame_token,
      base::BindOnce(&TestNonIPCMessageEnqueuer::FrameTokenCallback,
                     base::Unretained(enqueuer)));
  EXPECT_FALSE(enqueuer->frame_token_callback_called());
  EXPECT_FALSE(client->invalid_frame_token_called());

  queue->DidProcessFrame(frame_token);
  EXPECT_EQ(0u, queue->size());
  EXPECT_FALSE(client->invalid_frame_token_called());
  EXPECT_EQ(1, client->on_process_swap_message_count());
  EXPECT_EQ(0, client->on_message_dispatch_error_count());
  EXPECT_TRUE(enqueuer->frame_token_callback_called());
}

// Tests that if a valid non-IPC callback is enqueued before an IPC::Message,
// that both get called once the frame token arrives.
TEST_F(FrameTokenMessageQueueTest, EnqueueNonIPCCallbackFirst) {
  FrameTokenMessageQueue* queue = frame_token_message_queue();
  TestFrameTokenMessageQueueClient* client = test_client();
  TestNonIPCMessageEnqueuer* enqueuer = test_non_ipc_enqueuer();
  ASSERT_EQ(0u, queue->size());

  const uint32_t frame_token = 42;
  IPC::Message msg(0, 1, IPC::Message::PRIORITY_NORMAL);
  std::vector<IPC::Message> messages;
  messages.push_back(msg);

  queue->EnqueueOrRunFrameTokenCallback(
      frame_token,
      base::BindOnce(&TestNonIPCMessageEnqueuer::FrameTokenCallback,
                     base::Unretained(enqueuer)));
  EXPECT_FALSE(enqueuer->frame_token_callback_called());
  EXPECT_EQ(0, client->on_process_swap_message_count());

  // We should be able to enqueue even though it is for the same frame token.
  queue->OnFrameSwapMessagesReceived(frame_token, std::move(messages));
  EXPECT_EQ(2u, queue->size());
  EXPECT_EQ(0, client->on_process_swap_message_count());
  EXPECT_FALSE(client->invalid_frame_token_called());

  queue->DidProcessFrame(frame_token);
  EXPECT_EQ(0u, queue->size());
  EXPECT_FALSE(client->invalid_frame_token_called());
  EXPECT_EQ(1, client->on_process_swap_message_count());
  EXPECT_EQ(0, client->on_message_dispatch_error_count());
  EXPECT_TRUE(enqueuer->frame_token_callback_called());
}

// Tests that if we only have a non-IPC callback enqueued that it is called once
// the frame token arrive.
TEST_F(FrameTokenMessageQueueTest, EnqueueOnlyNonIPC) {
  FrameTokenMessageQueue* queue = frame_token_message_queue();
  TestFrameTokenMessageQueueClient* client = test_client();
  TestNonIPCMessageEnqueuer* enqueuer = test_non_ipc_enqueuer();
  ASSERT_EQ(0u, queue->size());

  const uint32_t frame_token = 42;
  queue->EnqueueOrRunFrameTokenCallback(
      frame_token,
      base::BindOnce(&TestNonIPCMessageEnqueuer::FrameTokenCallback,
                     base::Unretained(enqueuer)));
  EXPECT_FALSE(enqueuer->frame_token_callback_called());
  EXPECT_EQ(0, client->on_process_swap_message_count());
  EXPECT_EQ(1u, queue->size());

  queue->DidProcessFrame(frame_token);
  EXPECT_EQ(0u, queue->size());
  EXPECT_FALSE(client->invalid_frame_token_called());
  EXPECT_EQ(0, client->on_process_swap_message_count());
  EXPECT_EQ(0, client->on_message_dispatch_error_count());
  EXPECT_TRUE(enqueuer->frame_token_callback_called());
}

// Tests that if we have messages enqueued, and receive a frame token that is
// larger, that we still process the messages.
TEST_F(FrameTokenMessageQueueTest, MessagesWhereFrameTokenSkipped) {
  FrameTokenMessageQueue* queue = frame_token_message_queue();
  TestFrameTokenMessageQueueClient* client = test_client();
  TestNonIPCMessageEnqueuer* enqueuer = test_non_ipc_enqueuer();
  ASSERT_EQ(0u, queue->size());

  const uint32_t frame_token = 42;
  IPC::Message msg(0, 1, IPC::Message::PRIORITY_NORMAL);
  std::vector<IPC::Message> messages;
  messages.push_back(msg);

  queue->EnqueueOrRunFrameTokenCallback(
      frame_token,
      base::BindOnce(&TestNonIPCMessageEnqueuer::FrameTokenCallback,
                     base::Unretained(enqueuer)));
  EXPECT_FALSE(enqueuer->frame_token_callback_called());
  EXPECT_EQ(0, client->on_process_swap_message_count());

  // We should be able to enqueue even though it is for the same frame token.
  queue->OnFrameSwapMessagesReceived(frame_token, std::move(messages));
  EXPECT_EQ(2u, queue->size());
  EXPECT_EQ(0, client->on_process_swap_message_count());
  EXPECT_FALSE(client->invalid_frame_token_called());

  const uint32_t larger_frame_token = 1337;
  queue->DidProcessFrame(larger_frame_token);
  EXPECT_EQ(0u, queue->size());
  EXPECT_FALSE(client->invalid_frame_token_called());
  EXPECT_EQ(1, client->on_process_swap_message_count());
  EXPECT_EQ(0, client->on_message_dispatch_error_count());
  EXPECT_TRUE(enqueuer->frame_token_callback_called());
}

// Verifies that if there are multiple IPC::Messages that they are all
// processed.
TEST_F(FrameTokenMessageQueueTest, MultipleIPCMessages) {
  FrameTokenMessageQueue* queue = frame_token_message_queue();
  TestFrameTokenMessageQueueClient* client = test_client();
  ASSERT_EQ(0u, queue->size());

  const uint32_t frame_token = 42;
  IPC::Message msg1(0, 1, IPC::Message::PRIORITY_NORMAL);
  IPC::Message msg2(1, 2, IPC::Message::PRIORITY_LOW);
  std::vector<IPC::Message> messages;
  messages.push_back(msg1);
  messages.push_back(msg2);

  // Adding to the queue with a new frame token should not cause processing.
  queue->OnFrameSwapMessagesReceived(frame_token, std::move(messages));
  // All IPCs are enqueued as one.
  EXPECT_EQ(1u, queue->size());
  EXPECT_EQ(0, client->on_process_swap_message_count());

  queue->DidProcessFrame(frame_token);
  EXPECT_EQ(0u, queue->size());
  EXPECT_FALSE(client->invalid_frame_token_called());
  EXPECT_EQ(2, client->on_process_swap_message_count());
  EXPECT_EQ(0, client->on_message_dispatch_error_count());
}

// Verifies that if there are multiple non-IPC messages enqueued that they are
// all called.
TEST_F(FrameTokenMessageQueueTest, MultipleNonIPCMessages) {
  FrameTokenMessageQueue* queue = frame_token_message_queue();
  TestFrameTokenMessageQueueClient* client = test_client();
  TestNonIPCMessageEnqueuer* enqueuer = test_non_ipc_enqueuer();
  ASSERT_EQ(0u, queue->size());

  const uint32_t frame_token = 42;
  queue->EnqueueOrRunFrameTokenCallback(
      frame_token,
      base::BindOnce(&TestNonIPCMessageEnqueuer::FrameTokenCallback,
                     base::Unretained(enqueuer)));
  EXPECT_FALSE(enqueuer->frame_token_callback_called());
  EXPECT_EQ(1u, queue->size());

  // Create a second callback
  TestNonIPCMessageEnqueuer second_enqueuer;
  queue->EnqueueOrRunFrameTokenCallback(
      frame_token,
      base::BindOnce(&TestNonIPCMessageEnqueuer::FrameTokenCallback,
                     base::Unretained(&second_enqueuer)));
  EXPECT_FALSE(second_enqueuer.frame_token_callback_called());
  EXPECT_EQ(2u, queue->size());

  queue->DidProcessFrame(frame_token);
  EXPECT_EQ(0u, queue->size());
  EXPECT_FALSE(client->invalid_frame_token_called());
  EXPECT_TRUE(enqueuer->frame_token_callback_called());
  EXPECT_TRUE(second_enqueuer.frame_token_callback_called());
}

// Tests that if a non-IPC callback is enqueued, after its frame token as been
// received, that it is immediately processed.
TEST_F(FrameTokenMessageQueueTest, EnqueuedAfterFrameTokenImmediatelyRuns) {
  FrameTokenMessageQueue* queue = frame_token_message_queue();
  TestFrameTokenMessageQueueClient* client = test_client();
  TestNonIPCMessageEnqueuer* enqueuer = test_non_ipc_enqueuer();
  ASSERT_EQ(0u, queue->size());

  const uint32_t frame_token = 42;
  queue->DidProcessFrame(frame_token);
  EXPECT_EQ(0u, queue->size());
  EXPECT_FALSE(client->invalid_frame_token_called());
  EXPECT_FALSE(enqueuer->frame_token_callback_called());

  queue->EnqueueOrRunFrameTokenCallback(
      frame_token,
      base::BindOnce(&TestNonIPCMessageEnqueuer::FrameTokenCallback,
                     base::Unretained(enqueuer)));
  EXPECT_EQ(0u, queue->size());
  EXPECT_FALSE(client->invalid_frame_token_called());
  EXPECT_TRUE(enqueuer->frame_token_callback_called());
}

// Tests that if IPC::Messages are enqueued for different frame tokens, that
// we only process the messages associated with the arriving token, and keep the
// others enqueued.
TEST_F(FrameTokenMessageQueueTest, DifferentFrameTokensEnqueuedIPC) {
  FrameTokenMessageQueue* queue = frame_token_message_queue();
  TestFrameTokenMessageQueueClient* client = test_client();
  ASSERT_EQ(0u, queue->size());

  const uint32_t frame_token_1 = 42;
  IPC::Message msg1(0, 1, IPC::Message::PRIORITY_NORMAL);
  std::vector<IPC::Message> messages_1;
  messages_1.push_back(msg1);

  queue->OnFrameSwapMessagesReceived(frame_token_1, std::move(messages_1));
  EXPECT_EQ(1u, queue->size());
  EXPECT_EQ(0, client->on_process_swap_message_count());

  const uint32_t frame_token_2 = 1337;
  IPC::Message msg2(1, 2, IPC::Message::PRIORITY_LOW);
  std::vector<IPC::Message> messages_2;
  messages_2.push_back(msg2);

  queue->OnFrameSwapMessagesReceived(frame_token_2, std::move(messages_2));
  // With no frame token yet the second set of IPC::Messages should be enqueud
  // separately.
  EXPECT_EQ(2u, queue->size());
  EXPECT_EQ(0, client->on_process_swap_message_count());

  // We should only process the first IPC::Message.
  queue->DidProcessFrame(frame_token_1);
  EXPECT_EQ(1u, queue->size());
  EXPECT_FALSE(client->invalid_frame_token_called());
  EXPECT_EQ(1, client->on_process_swap_message_count());
  EXPECT_EQ(0, client->on_message_dispatch_error_count());

  // Clear the counts from the first token.
  client->Reset();

  // The second IPC::Message should be processed.
  queue->DidProcessFrame(frame_token_2);
  EXPECT_EQ(0u, queue->size());
  EXPECT_FALSE(client->invalid_frame_token_called());
  EXPECT_EQ(1, client->on_process_swap_message_count());
  EXPECT_EQ(0, client->on_message_dispatch_error_count());
}

// Test that if non-IPC callbacks are enqueued for different frame tokens, that
// we only process the messages associated with the arriving token, and keep the
// others enqueued.
TEST_F(FrameTokenMessageQueueTest, DifferentFrameTokensEnqueuedNonIPC) {
  FrameTokenMessageQueue* queue = frame_token_message_queue();
  TestFrameTokenMessageQueueClient* client = test_client();
  TestNonIPCMessageEnqueuer* enqueuer = test_non_ipc_enqueuer();
  ASSERT_EQ(0u, queue->size());

  const uint32_t frame_token_1 = 42;
  queue->EnqueueOrRunFrameTokenCallback(
      frame_token_1,
      base::BindOnce(&TestNonIPCMessageEnqueuer::FrameTokenCallback,
                     base::Unretained(enqueuer)));
  EXPECT_FALSE(enqueuer->frame_token_callback_called());
  EXPECT_EQ(1u, queue->size());

  // Create a second callback
  const uint32_t frame_token_2 = 1337;
  TestNonIPCMessageEnqueuer second_enqueuer;
  queue->EnqueueOrRunFrameTokenCallback(
      frame_token_2,
      base::BindOnce(&TestNonIPCMessageEnqueuer::FrameTokenCallback,
                     base::Unretained(&second_enqueuer)));
  EXPECT_FALSE(second_enqueuer.frame_token_callback_called());
  EXPECT_EQ(2u, queue->size());

  queue->DidProcessFrame(frame_token_1);
  EXPECT_EQ(1u, queue->size());
  EXPECT_FALSE(client->invalid_frame_token_called());
  EXPECT_TRUE(enqueuer->frame_token_callback_called());
  EXPECT_FALSE(second_enqueuer.frame_token_callback_called());

  queue->DidProcessFrame(frame_token_2);
  EXPECT_TRUE(second_enqueuer.frame_token_callback_called());
}

// An empty frame token is considered invalid, so this tests that attempting to
// enqueue for that is rejected.
TEST_F(FrameTokenMessageQueueTest, EmptyTokenForIPCMessageIsRejected) {
  FrameTokenMessageQueue* queue = frame_token_message_queue();
  TestFrameTokenMessageQueueClient* client = test_client();
  ASSERT_EQ(0u, queue->size());

  const uint32_t invalid_frame_token = 0;
  IPC::Message msg(0, 1, IPC::Message::PRIORITY_NORMAL);
  std::vector<IPC::Message> messages;
  messages.push_back(msg);

  // Adding to the queue with a new frame token should not cause processing.
  queue->OnFrameSwapMessagesReceived(invalid_frame_token, std::move(messages));
  EXPECT_EQ(0u, queue->size());
  EXPECT_TRUE(client->invalid_frame_token_called());
  EXPECT_EQ(invalid_frame_token, client->invalid_frame_token());
  EXPECT_EQ(0, client->on_process_swap_message_count());
}

// Tests that when adding an IPC::Message for an earlier frame token, that it is
// enqueued.
TEST_F(FrameTokenMessageQueueTest, EarlierTokenForIPCMessageIsNotRejected) {
  FrameTokenMessageQueue* queue = frame_token_message_queue();
  TestFrameTokenMessageQueueClient* client = test_client();
  ASSERT_EQ(0u, queue->size());

  const uint32_t valid_frame_token = 42;
  IPC::Message msg1(0, 1, IPC::Message::PRIORITY_NORMAL);
  std::vector<IPC::Message> messages_1;
  messages_1.push_back(msg1);

  // Adding to the queue with a new frame token should not cause processing.
  queue->OnFrameSwapMessagesReceived(valid_frame_token, std::move(messages_1));
  EXPECT_EQ(1u, queue->size());
  EXPECT_EQ(0, client->on_process_swap_message_count());

  const uint32_t earlier_frame_token = 1;
  IPC::Message msg2(1, 2, IPC::Message::PRIORITY_NORMAL);
  std::vector<IPC::Message> messages_2;
  messages_2.push_back(msg1);

  // Adding an earlier frame token should be enqueued.
  queue->OnFrameSwapMessagesReceived(earlier_frame_token,
                                     std::move(messages_2));
  EXPECT_EQ(2u, queue->size());
  EXPECT_FALSE(client->invalid_frame_token_called());
  EXPECT_EQ(0, client->on_process_swap_message_count());
}

// Tests that if DidProcessFrame is called with an invalid token, that it is
// rejected, and that no callbacks are processed.
TEST_F(FrameTokenMessageQueueTest, InvalidDidProcessFrameTokenNotProcessed) {
  FrameTokenMessageQueue* queue = frame_token_message_queue();
  TestFrameTokenMessageQueueClient* client = test_client();
  TestNonIPCMessageEnqueuer* enqueuer = test_non_ipc_enqueuer();
  ASSERT_EQ(0u, queue->size());

  const uint32_t frame_token = 42;
  IPC::Message msg(0, 1, IPC::Message::PRIORITY_NORMAL);
  std::vector<IPC::Message> messages;
  messages.push_back(msg);

  queue->OnFrameSwapMessagesReceived(frame_token, std::move(messages));
  EXPECT_EQ(1u, queue->size());
  EXPECT_EQ(0, client->on_process_swap_message_count());

  queue->EnqueueOrRunFrameTokenCallback(
      frame_token,
      base::BindOnce(&TestNonIPCMessageEnqueuer::FrameTokenCallback,
                     base::Unretained(enqueuer)));
  EXPECT_FALSE(enqueuer->frame_token_callback_called());
  EXPECT_FALSE(client->invalid_frame_token_called());

  // Empty token should be invalid even with no process frames processed.
  const uint32_t invalid_frame_token = 0;
  queue->DidProcessFrame(invalid_frame_token);
  EXPECT_EQ(2u, queue->size());
  EXPECT_TRUE(client->invalid_frame_token_called());
  EXPECT_EQ(invalid_frame_token, client->invalid_frame_token());
  EXPECT_EQ(0, client->on_process_swap_message_count());
  EXPECT_EQ(0, client->on_message_dispatch_error_count());
  EXPECT_FALSE(enqueuer->frame_token_callback_called());
}

// Test that if DidProcessFrame is called with an earlier frame token, that it
// is rejected, and that no callbacks are processed.
TEST_F(FrameTokenMessageQueueTest, EarlierTokenForDidProcessFrameRejected) {
  FrameTokenMessageQueue* queue = frame_token_message_queue();
  TestFrameTokenMessageQueueClient* client = test_client();
  TestNonIPCMessageEnqueuer* enqueuer = test_non_ipc_enqueuer();
  ASSERT_EQ(0u, queue->size());

  // Settings a low value frame token will not block enqueueing.
  const uint32_t earlier_frame_token = 42;
  queue->DidProcessFrame(earlier_frame_token);

  const uint32_t frame_token = 1337;
  IPC::Message msg(0, 1, IPC::Message::PRIORITY_NORMAL);
  std::vector<IPC::Message> messages;
  messages.push_back(msg);

  queue->OnFrameSwapMessagesReceived(frame_token, std::move(messages));
  EXPECT_EQ(1u, queue->size());
  EXPECT_EQ(0, client->on_process_swap_message_count());

  queue->EnqueueOrRunFrameTokenCallback(
      frame_token,
      base::BindOnce(&TestNonIPCMessageEnqueuer::FrameTokenCallback,
                     base::Unretained(enqueuer)));
  EXPECT_FALSE(enqueuer->frame_token_callback_called());
  EXPECT_FALSE(client->invalid_frame_token_called());

  // Using a frame token that is earlier than the last received should be
  // rejected.
  const uint32_t invalid_frame_token = earlier_frame_token - 1;
  queue->DidProcessFrame(invalid_frame_token);
  EXPECT_EQ(2u, queue->size());
  EXPECT_TRUE(client->invalid_frame_token_called());
  EXPECT_EQ(invalid_frame_token, client->invalid_frame_token());
  EXPECT_EQ(0, client->on_process_swap_message_count());
  EXPECT_EQ(0, client->on_message_dispatch_error_count());
  EXPECT_FALSE(enqueuer->frame_token_callback_called());
}

// Tests that if an IPC::Message has a dispatch error that the client is
// notified.
TEST_F(FrameTokenMessageQueueTest, DispatchError) {
  FrameTokenMessageQueue* queue = frame_token_message_queue();
  TestFrameTokenMessageQueueClient* client = test_client();
  ASSERT_EQ(0u, queue->size());

  const uint32_t frame_token = 42;
  IPC::Message msg(0, 1, IPC::Message::PRIORITY_NORMAL);
  std::vector<IPC::Message> messages;
  messages.push_back(msg);

  queue->OnFrameSwapMessagesReceived(frame_token, std::move(messages));
  EXPECT_EQ(1u, queue->size());
  EXPECT_EQ(0, client->on_process_swap_message_count());

  // Dispatch error should be notified during processing.
  client->SetErrorOnMessageProcess();
  queue->DidProcessFrame(frame_token);
  EXPECT_EQ(0u, queue->size());
  EXPECT_FALSE(client->invalid_frame_token_called());
  EXPECT_EQ(1, client->on_process_swap_message_count());
  EXPECT_EQ(1, client->on_message_dispatch_error_count());
}

// Tests that if we have already enqueued a callback for a frame token, that if
// a request for an earlier frame token arrives, that it is still enqueued. Then
// once the large frame token arrives, both are processed.
TEST_F(FrameTokenMessageQueueTest, OutOfOrderFrameTokensEnqueue) {
  FrameTokenMessageQueue* queue = frame_token_message_queue();
  TestFrameTokenMessageQueueClient* client = test_client();
  TestNonIPCMessageEnqueuer* enqueuer = test_non_ipc_enqueuer();
  ASSERT_EQ(0u, queue->size());

  const uint32_t larger_frame_token = 1337;
  queue->EnqueueOrRunFrameTokenCallback(
      larger_frame_token,
      base::BindOnce(&TestNonIPCMessageEnqueuer::FrameTokenCallback,
                     base::Unretained(enqueuer)));
  EXPECT_EQ(1u, queue->size());
  EXPECT_FALSE(enqueuer->frame_token_callback_called());
  EXPECT_FALSE(client->invalid_frame_token_called());

  const uint32_t smaller_frame_token = 42;
  IPC::Message msg(0, 1, IPC::Message::PRIORITY_NORMAL);
  std::vector<IPC::Message> messages;
  messages.push_back(msg);

  // Enqueuing for a smaller token, which has not yet arrived, should still
  // enqueue.
  queue->OnFrameSwapMessagesReceived(smaller_frame_token, std::move(messages));
  EXPECT_EQ(2u, queue->size());
  EXPECT_EQ(0, client->on_process_swap_message_count());

  // Process both with the larger frame token arriving.
  queue->DidProcessFrame(larger_frame_token);
  EXPECT_EQ(0u, queue->size());
  EXPECT_FALSE(client->invalid_frame_token_called());
  EXPECT_EQ(1, client->on_process_swap_message_count());
  EXPECT_EQ(0, client->on_message_dispatch_error_count());
  EXPECT_TRUE(enqueuer->frame_token_callback_called());
}

}  // namespace content
