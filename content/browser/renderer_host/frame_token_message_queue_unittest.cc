// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/frame_token_message_queue.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// Test verision of FrameTokenMessageQueue::Client which tracks the number of
// calls to client methods, and the associated input parameters.
class TestFrameTokenMessageQueueClient : public FrameTokenMessageQueue::Client {
 public:
  TestFrameTokenMessageQueueClient() {}

  TestFrameTokenMessageQueueClient(const TestFrameTokenMessageQueueClient&) =
      delete;
  TestFrameTokenMessageQueueClient& operator=(
      const TestFrameTokenMessageQueueClient&) = delete;

  ~TestFrameTokenMessageQueueClient() {}

  // Resets all method counters.
  void Reset();

  // FrameTokenMessageQueue::Client:
  void OnInvalidFrameToken(uint32_t frame_token) override;

  bool invalid_frame_token_called() const {
    return invalid_frame_token_called_;
  }
  uint32_t invalid_frame_token() const { return invalid_frame_token_; }

 private:
  bool invalid_frame_token_called_ = false;
  uint32_t invalid_frame_token_ = 0u;
};

void TestFrameTokenMessageQueueClient::Reset() {
  invalid_frame_token_called_ = false;
  invalid_frame_token_ = 0u;
}

void TestFrameTokenMessageQueueClient::OnInvalidFrameToken(
    uint32_t frame_token) {
  invalid_frame_token_called_ = true;
  invalid_frame_token_ = frame_token;
}

// Test class which provides FrameTokenCallback() to be used as a closure when
// enqueueing non-IPC callbacks. This only tracks if the callback was called.
class TestNonIPCMessageEnqueuer {
 public:
  TestNonIPCMessageEnqueuer() {}

  TestNonIPCMessageEnqueuer(const TestNonIPCMessageEnqueuer&) = delete;
  TestNonIPCMessageEnqueuer& operator=(const TestNonIPCMessageEnqueuer&) =
      delete;

  ~TestNonIPCMessageEnqueuer() {}

  void FrameTokenCallback(base::TimeTicks activation_time);

  bool frame_token_callback_called() const {
    return frame_token_callback_called_;
  }

 private:
  bool frame_token_callback_called_ = false;
};

void TestNonIPCMessageEnqueuer::FrameTokenCallback(
    base::TimeTicks activation_time) {
  frame_token_callback_called_ = true;
}

}  // namespace

class FrameTokenMessageQueueTest : public testing::Test {
 public:
  FrameTokenMessageQueueTest();

  FrameTokenMessageQueueTest(const FrameTokenMessageQueueTest&) = delete;
  FrameTokenMessageQueueTest& operator=(const FrameTokenMessageQueueTest&) =
      delete;

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
};

FrameTokenMessageQueueTest::FrameTokenMessageQueueTest() {
  frame_token_message_queue_.Init(&test_client_);
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
  EXPECT_EQ(1u, queue->size());
  EXPECT_FALSE(enqueuer->frame_token_callback_called());
  EXPECT_FALSE(client->invalid_frame_token_called());

  queue->DidProcessFrame(frame_token, base::TimeTicks::Now());
  EXPECT_EQ(0u, queue->size());
  EXPECT_FALSE(client->invalid_frame_token_called());
  EXPECT_TRUE(enqueuer->frame_token_callback_called());
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

  queue->DidProcessFrame(frame_token, base::TimeTicks::Now());
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
  queue->DidProcessFrame(frame_token, base::TimeTicks::Now());
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

  queue->DidProcessFrame(frame_token_1, base::TimeTicks::Now());
  EXPECT_EQ(1u, queue->size());
  EXPECT_FALSE(client->invalid_frame_token_called());
  EXPECT_TRUE(enqueuer->frame_token_callback_called());
  EXPECT_FALSE(second_enqueuer.frame_token_callback_called());

  queue->DidProcessFrame(frame_token_2, base::TimeTicks::Now());
  EXPECT_TRUE(second_enqueuer.frame_token_callback_called());
}

// Tests that if DidProcessFrame is called with an invalid token, that it is
// rejected, and that no callbacks are processed.
TEST_F(FrameTokenMessageQueueTest, InvalidDidProcessFrameTokenNotProcessed) {
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
  EXPECT_FALSE(client->invalid_frame_token_called());

  // Empty token should be invalid even with no process frames processed.
  const uint32_t invalid_frame_token = 0;
  queue->DidProcessFrame(invalid_frame_token, base::TimeTicks::Now());
  EXPECT_EQ(1u, queue->size());
  EXPECT_TRUE(client->invalid_frame_token_called());
  EXPECT_EQ(invalid_frame_token, client->invalid_frame_token());
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
  queue->DidProcessFrame(earlier_frame_token, base::TimeTicks::Now());

  const uint32_t frame_token = 1337;
  queue->EnqueueOrRunFrameTokenCallback(
      frame_token,
      base::BindOnce(&TestNonIPCMessageEnqueuer::FrameTokenCallback,
                     base::Unretained(enqueuer)));
  EXPECT_FALSE(enqueuer->frame_token_callback_called());
  EXPECT_FALSE(client->invalid_frame_token_called());
  EXPECT_EQ(1u, queue->size());

  // Using a frame token that is earlier than the last received should be
  // rejected.
  const uint32_t invalid_frame_token = earlier_frame_token - 1;
  queue->DidProcessFrame(invalid_frame_token, base::TimeTicks::Now());
  EXPECT_EQ(1u, queue->size());
  EXPECT_TRUE(client->invalid_frame_token_called());
  EXPECT_EQ(invalid_frame_token, client->invalid_frame_token());
  EXPECT_FALSE(enqueuer->frame_token_callback_called());
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

  // Process both with the larger frame token arriving.
  queue->DidProcessFrame(larger_frame_token, base::TimeTicks::Now());
  EXPECT_EQ(0u, queue->size());
  EXPECT_FALSE(client->invalid_frame_token_called());
  EXPECT_TRUE(enqueuer->frame_token_callback_called());
}

}  // namespace content
