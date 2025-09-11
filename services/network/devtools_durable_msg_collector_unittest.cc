// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/devtools_durable_msg_collector.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "net/filter/filter_source_stream_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

class DevtoolsDurableMessageCollectorTest : public testing::Test {
 public:
  void OnAllClientsDisconnectedCallback() {
    CountEvent();
    disconnect_called_ = true;
  }

 protected:
  void WaitForEventCount(int event_count_expected) {
    event_count_expected_ = event_count_expected;
    base::RunLoop run_loop;
    run_loop_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void CountEvent() {
    if (++event_count_ >= event_count_expected_) {
      std::move(run_loop_quit_closure_).Run();
    }
  }

  void AddBytes(base::WeakPtr<DevtoolsDurableMessage> msg,
                base::span<const uint8_t> bytes) {
    ASSERT_NE(msg, nullptr);
    msg->AddBytes(bytes, bytes.size());
  }

  void AddBytes(base::WeakPtr<DevtoolsDurableMessage> msg,
                base::span<const char> bytes) {
    ASSERT_NE(msg, nullptr);
    AddBytes(msg, base::as_byte_span(bytes));
  }

  void MarkComplete(base::WeakPtr<DevtoolsDurableMessage> msg) {
    ASSERT_NE(msg, nullptr);
    msg->MarkComplete();
  }

  void RetrieveEmptyAndCountEvent(
      const mojo::Remote<mojom::DurableMessageCollector>& collector_remote,
      const std::string& request_id) {
    collector_remote->Retrieve(
        request_id,
        base::BindLambdaForTesting(
            [this](std::optional<mojo_base::BigBuffer> bytes_out) -> void {
              EXPECT_FALSE(bytes_out.has_value());
              CountEvent();
            }));
  }

  void RetrieveAndCountEvent(
      const mojo::Remote<mojom::DurableMessageCollector>& collector_remote,
      const std::string& request_id,
      std::string verify_message_str) {
    collector_remote->Retrieve(
        request_id,
        base::BindLambdaForTesting(
            [this, verify_message = std::move(verify_message_str)](
                std::optional<mojo_base::BigBuffer> bytes_out) -> void {
              EXPECT_EQ(base::as_string_view(bytes_out.value()),
                        verify_message);
              CountEvent();
            }));
  }

  bool IsDisconnectCalledback() { return disconnect_called_; }

 private:
  base::test::TaskEnvironment task_environment_;
  base::OnceClosure run_loop_quit_closure_;
  int event_count_ = 0;
  int event_count_expected_ = 0;
  bool disconnect_called_ = false;
};

TEST_F(DevtoolsDurableMessageCollectorTest, CollectsMessageChunksCorrectly) {
  DevtoolsDurableMessageCollector collector(base::DoNothing());
  collector.Configure(network::mojom::NetworkDurableMessageConfig::New(
      /*max_storage_size=*/1000));
  mojo::Remote<mojom::DurableMessageCollector> collector_remote;
  collector.AddReceiver(collector_remote.BindNewPipeAndPassReceiver());

  std::string request_id1 = "req1";
  auto msg1 = collector.CreateDurableMessage(request_id1);
  ASSERT_NE(msg1, nullptr);
  std::string test_message_1 = "abcdefghij";
  auto [first_chunk, second_chunk] =
      base::span(test_message_1).split_at(test_message_1.size() / 2);
  AddBytes(msg1, first_chunk);
  AddBytes(msg1, second_chunk);
  MarkComplete(msg1);
  RetrieveAndCountEvent(collector_remote, request_id1, test_message_1);

  std::string request_id2 = "req2";
  auto msg2 = collector.CreateDurableMessage(request_id2);
  ASSERT_NE(msg2, nullptr);
  std::string test_message_2 = "zyxwvutsrqponm";
  AddBytes(msg2, test_message_2);
  MarkComplete(msg2);
  RetrieveAndCountEvent(collector_remote, request_id2, test_message_2);

  WaitForEventCount(2);
}

TEST_F(DevtoolsDurableMessageCollectorTest, DoesntCollectChunksBeyondLimit) {
  DevtoolsDurableMessageCollector collector(base::DoNothing());
  collector.Configure(network::mojom::NetworkDurableMessageConfig::New(
      /*max_storage_size=*/10));
  mojo::Remote<mojom::DurableMessageCollector> collector_remote;
  collector.AddReceiver(collector_remote.BindNewPipeAndPassReceiver());

  std::string req_id = "req1";
  auto msg1 = collector.CreateDurableMessage(req_id);
  ASSERT_NE(msg1, nullptr);

  std::string test_message = "12345678901";
  auto [first_chunk, second_chunk] =
      base::span(test_message).split_at(test_message.size() / 2);
  AddBytes(msg1, first_chunk);
  AddBytes(msg1, second_chunk);
  // Assert that second chunk evicted the message altogether.
  EXPECT_EQ(msg1, nullptr);

  // Verify that the message isn't retrievable.
  RetrieveEmptyAndCountEvent(collector_remote, req_id);

  WaitForEventCount(1);
}

TEST_F(DevtoolsDurableMessageCollectorTest, DoesntCollectMessageBeyondLimit) {
  DevtoolsDurableMessageCollector collector(base::DoNothing());
  collector.Configure(network::mojom::NetworkDurableMessageConfig::New(
      /*max_storage_size=*/10));
  mojo::Remote<mojom::DurableMessageCollector> collector_remote;
  collector.AddReceiver(collector_remote.BindNewPipeAndPassReceiver());

  std::string req_id = "req1";
  auto msg1 = collector.CreateDurableMessage(req_id);
  ASSERT_NE(msg1, nullptr);

  std::string test_message = "12345678901";
  AddBytes(msg1, test_message);
  // Assert that the message chunk that's larger than set limit wasn't stored.
  EXPECT_EQ(msg1, nullptr);

  // Verify that the message isn't retrievable.
  RetrieveEmptyAndCountEvent(collector_remote, req_id);

  WaitForEventCount(1);
}

TEST_F(DevtoolsDurableMessageCollectorTest, CorrectlyEvictsInOrder) {
  DevtoolsDurableMessageCollector collector(base::BindRepeating(
      &DevtoolsDurableMessageCollectorTest::OnAllClientsDisconnectedCallback,
      base::Unretained(this)));
  collector.Configure(network::mojom::NetworkDurableMessageConfig::New(
      /*max_storage_size=*/10));
  mojo::Remote<mojom::DurableMessageCollector> collector_remote;
  collector.AddReceiver(collector_remote.BindNewPipeAndPassReceiver());

  std::string req_id_1 = "req1";
  std::string str_message_1 = "12345";
  auto msg1 = collector.CreateDurableMessage(req_id_1);
  ASSERT_NE(msg1, nullptr);

  std::string req_id_2 = "req2";
  std::string str_message_2 = "678";
  auto msg2 = collector.CreateDurableMessage(req_id_2);
  ASSERT_NE(msg2, nullptr);

  std::string req_id_3 = "req3";
  std::string str_message_3 = "90123";
  auto msg3 = collector.CreateDurableMessage(req_id_3);
  ASSERT_NE(msg3, nullptr);

  AddBytes(msg1, str_message_1);
  MarkComplete(msg1);
  // Verify that the first message is stored and retrievable.
  RetrieveAndCountEvent(collector_remote, req_id_1, str_message_1);
  WaitForEventCount(1);

  AddBytes(msg2, str_message_2);
  MarkComplete(msg2);

  AddBytes(msg3, str_message_3);
  MarkComplete(msg3);

  // Verify that the first message was evicted.
  RetrieveEmptyAndCountEvent(collector_remote, req_id_1);
  RetrieveAndCountEvent(collector_remote, req_id_2, str_message_2);
  RetrieveAndCountEvent(collector_remote, req_id_3, str_message_3);
  WaitForEventCount(3);
  EXPECT_FALSE(IsDisconnectCalledback());

  // Verify that all clients disconnected triggers the disconnect callback.
  collector_remote.reset();
  WaitForEventCount(1);
  EXPECT_TRUE(IsDisconnectCalledback());
}

TEST_F(DevtoolsDurableMessageCollectorTest,
       CorrectlyHandlesRequestIdOverwrite) {
  DevtoolsDurableMessageCollector collector(base::DoNothing());
  collector.Configure(network::mojom::NetworkDurableMessageConfig::New(
      /*max_storage_size=*/10));
  mojo::Remote<mojom::DurableMessageCollector> collector_remote;
  collector.AddReceiver(collector_remote.BindNewPipeAndPassReceiver());

  std::string req_id_overwrite = "req-overwrite";
  std::string first_message_body = "12345";
  auto msg1 = collector.CreateDurableMessage(req_id_overwrite);
  ASSERT_NE(msg1, nullptr);
  AddBytes(msg1, first_message_body);
  MarkComplete(msg1);

  RetrieveAndCountEvent(collector_remote, req_id_overwrite, first_message_body);
  WaitForEventCount(1);

  // Now, overwrite the same request ID, simulating a redirect.
  // This new message is smaller.
  std::string second_message_body = "abc";
  auto msg2 = collector.CreateDurableMessage(req_id_overwrite);
  ASSERT_NE(msg2, nullptr);
  AddBytes(msg2, second_message_body);
  MarkComplete(msg2);

  // Verify that the new message is now stored.
  RetrieveAndCountEvent(collector_remote, req_id_overwrite,
                        second_message_body);
  WaitForEventCount(1);

  // Create another message that will force an eviction.
  std::string req_id_filler = "req-filler";
  std::string filler_body = "87654321";
  auto msg3 = collector.CreateDurableMessage(req_id_filler);
  AddBytes(msg3, filler_body);
  MarkComplete(msg3);

  // Verify that the overwritten message was evicted and the filler message
  // is now present.
  RetrieveEmptyAndCountEvent(collector_remote, req_id_overwrite);
  RetrieveAndCountEvent(collector_remote, req_id_filler, filler_body);
  WaitForEventCount(2);
}

TEST_F(DevtoolsDurableMessageCollectorTest, RetrieveDecodesGzipBody) {
  DevtoolsDurableMessageCollector collector(base::DoNothing());
  collector.Configure(network::mojom::NetworkDurableMessageConfig::New(
      /*max_storage_size=*/100));
  mojo::Remote<mojom::DurableMessageCollector> collector_remote;
  collector.AddReceiver(collector_remote.BindNewPipeAndPassReceiver());

  const std::string devtools_request_id = "request1";
  auto msg1 = collector.CreateDurableMessage(devtools_request_id);
  ASSERT_NE(msg1, nullptr);
  msg1->set_client_decoding_types({net::SourceStreamType::kGzip});

  const std::string original_body = "Hello, world! This is a test.";
  auto compressed = net::CompressGzip(original_body);
  AddBytes(msg1, compressed);
  MarkComplete(msg1);

  RetrieveAndCountEvent(collector_remote, devtools_request_id,
                        std::move(original_body));
  WaitForEventCount(1);
}

}  // namespace network
