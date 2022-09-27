// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/shared_buffer_bytes_consumer.h"

#include <string>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/loader/testing/bytes_consumer_test_reader.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

using PublicState = BytesConsumer::PublicState;

TEST(SharedBufferBytesConsumerTest, Read) {
  const Vector<std::string> kData{"This is a expected data!",
                                  "This is another data!"};
  std::string flatten_expected_data;
  auto shared_buffer = SharedBuffer::Create();
  for (const auto& chunk : kData) {
    shared_buffer->Append(chunk.data(), chunk.size());
    flatten_expected_data += chunk;
  }

  auto* bytes_consumer =
      MakeGarbageCollected<SharedBufferBytesConsumer>(std::move(shared_buffer));
  EXPECT_EQ(PublicState::kReadableOrWaiting, bytes_consumer->GetPublicState());

  auto task_runner = base::MakeRefCounted<scheduler::FakeTaskRunner>();
  auto* test_reader =
      MakeGarbageCollected<BytesConsumerTestReader>(bytes_consumer);
  auto [result, data_from_consumer] = test_reader->Run(task_runner.get());
  EXPECT_EQ(BytesConsumer::Result::kDone, result);
  EXPECT_EQ(PublicState::kClosed, bytes_consumer->GetPublicState());
  EXPECT_EQ(flatten_expected_data,
            std::string(data_from_consumer.data(), data_from_consumer.size()));
}

TEST(SharedBufferBytesConsumerTest, Cancel) {
  const Vector<std::string> kData{"This is a expected data!",
                                  "This is another data!"};
  auto shared_buffer = SharedBuffer::Create();
  for (const auto& chunk : kData) {
    shared_buffer->Append(chunk.data(), chunk.size());
  }

  auto* bytes_consumer =
      MakeGarbageCollected<SharedBufferBytesConsumer>(std::move(shared_buffer));
  EXPECT_EQ(PublicState::kReadableOrWaiting, bytes_consumer->GetPublicState());

  bytes_consumer->Cancel();
  const char* buffer;
  size_t available;
  BytesConsumer::Result result = bytes_consumer->BeginRead(&buffer, &available);
  EXPECT_EQ(0u, available);
  EXPECT_EQ(BytesConsumer::Result::kDone, result);
  EXPECT_EQ(PublicState::kClosed, bytes_consumer->GetPublicState());
}

}  // namespace blink
