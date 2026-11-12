// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/message.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "mojo/public/cpp/bindings/lib/unserialized_message_context.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

constexpr int32_t kTestMessageName = 42;
constexpr int32_t kTestMessageFlags = 7;
constexpr uint32_t kTestPayloadSize = 32;

class TestUnserializedMessageContext
    : public internal::UnserializedMessageContext {
 public:
  static const internal::UnserializedMessageContext::Tag kMessageTag;

  TestUnserializedMessageContext(
      uint32_t name,
      uint32_t flags,
      base::OnceClosure destruction_callback = base::OnceClosure())
      : UnserializedMessageContext(&kMessageTag, name, flags),
        destruction_callback_(std::move(destruction_callback)) {}
  ~TestUnserializedMessageContext() override {
    if (destruction_callback_) {
      std::move(destruction_callback_).Run();
    }
  }

  // internal::UnserializedMessageContext:
  void Serialize(Message& message) override {
    message.payload_buffer()->Allocate(kTestPayloadSize);
  }

 private:
  base::OnceClosure destruction_callback_;
};

const internal::UnserializedMessageContext::Tag
    TestUnserializedMessageContext::kMessageTag = {};

void CreateTestMessagePayload(std::vector<uint8_t>* bytes,
                              std::vector<ScopedHandle>* handles) {
  Message message(kTestMessageName, kTestMessageFlags, 0, kTestPayloadSize,
                  nullptr);
  message.header()->trace_nonce = 0;
  if (message.version() >= 3) {
    message.header_v3()->creation_timeticks_us = 0;
  }

  bytes->resize(message.data_num_bytes());
  std::ranges::copy(message.data_as_span(), bytes->begin());

  MessagePipe pipe;
  handles->resize(2);
  handles->at(0) = ScopedHandle(std::move(pipe.handle0));
  handles->at(1) = ScopedHandle(std::move(pipe.handle1));
}

TEST(BindingsMessageTest, ConstructFromPayload) {
  // Verifies that Message objects constructed directly from a raw payload look
  // the same on the wire as raw messages constructed with lower level APIs.
  MessagePipe pipe;

  // First feed the raw message data directly into the pipe.
  std::vector<uint8_t> in_bytes1;
  std::vector<ScopedHandle> in_handles1;
  CreateTestMessagePayload(&in_bytes1, &in_handles1);
  WriteMessageRaw(pipe.handle0.get(), in_bytes1.data(), in_bytes1.size(),
                  reinterpret_cast<const MojoHandle*>(in_handles1.data()),
                  in_handles1.size(), MOJO_WRITE_MESSAGE_FLAG_NONE);
  for (auto& handle : in_handles1) {
    std::ignore = handle.release();
  }

  // Now construct a Message object from the same payload and feed that into the
  // pipe.
  std::vector<uint8_t> in_bytes2;
  std::vector<ScopedHandle> in_handles2;
  CreateTestMessagePayload(&in_bytes2, &in_handles2);
  Message message(in_bytes2, in_handles2);
  WriteMessageNew(pipe.handle0.get(), message.TakeMojoMessage(),
                  MOJO_WRITE_MESSAGE_FLAG_NONE);

  // Now read both messages and ensure that they're identical.
  // NOTE: The handles themselves cannot be identical, but the same number of
  // handles should be attached.
  std::vector<uint8_t> out_bytes1;
  std::vector<ScopedHandle> out_handles1;
  ASSERT_EQ(MOJO_RESULT_OK,
            ReadMessageRaw(pipe.handle1.get(), &out_bytes1, &out_handles1,
                           MOJO_READ_MESSAGE_FLAG_NONE));
  std::vector<uint8_t> out_bytes2;
  std::vector<ScopedHandle> out_handles2;
  ASSERT_EQ(MOJO_RESULT_OK,
            ReadMessageRaw(pipe.handle1.get(), &out_bytes2, &out_handles2,
                           MOJO_READ_MESSAGE_FLAG_NONE));

  EXPECT_EQ(out_bytes1, out_bytes2);
  EXPECT_EQ(out_handles1.size(), out_handles2.size());
}

TEST(BindingsMessageTest, AllocateExceedingCapacityReallocatesPayloadBuffer) {
  Message message(kTestMessageName, kTestMessageFlags, 0, 0, nullptr);
  ASSERT_TRUE(message.payload_buffer()->is_valid());

  // Allocating `initial_capacity` bytes on top of the already-written header
  // (`cursor() > 0`) exceeds the initial capacity and forces `Buffer::Allocate`
  // to reallocate the backing storage.
  const size_t initial_capacity = message.payload_buffer()->size();
  ASSERT_GT(message.payload_buffer()->cursor(), 0u);
  message.payload_buffer()->Allocate(initial_capacity);
  EXPECT_GT(message.payload_buffer()->size(), initial_capacity);

  ScopedMessageHandle handle = message.TakeMojoMessage();
  EXPECT_TRUE(handle.is_valid());
  EXPECT_TRUE(message.IsNull());
  EXPECT_EQ(nullptr, message.payload_buffer()->data());
}

TEST(BindingsMessageTest, MoveAssignAndResetReleasePayloadBuffer) {
  Message message1(kTestMessageName, kTestMessageFlags, kTestPayloadSize, 0,
                   nullptr);
  Message message2(kTestMessageName + 1, kTestMessageFlags,
                   kTestPayloadSize * 2, 0, nullptr);
  EXPECT_FALSE(message1.IsNull());
  EXPECT_FALSE(message2.IsNull());

  // Move-assigning over a non-null Message should cleanly release the
  // overwritten message's buffer and handle.
  message1 = std::move(message2);
  EXPECT_FALSE(message1.IsNull());
  EXPECT_EQ(static_cast<uint32_t>(kTestMessageName + 1), message1.name());

  // Explicitly resetting a non-null Message should cleanly release its buffer
  // and handle.
  message1.Reset();
  EXPECT_TRUE(message1.IsNull());
  EXPECT_EQ(nullptr, message1.payload_buffer()->data());
}

TEST(BindingsMessageTest, LazySerializationDestroysContext) {
  bool context_destroyed = false;
  Message message(
      std::make_unique<TestUnserializedMessageContext>(
          kTestMessageName, kTestMessageFlags,
          base::BindLambdaForTesting([&] { context_destroyed = true; })),
      MOJO_CREATE_MESSAGE_FLAG_NONE);
  EXPECT_FALSE(message.is_serialized());
  EXPECT_FALSE(context_destroyed);

  message.SerializeIfNecessary();
  EXPECT_TRUE(context_destroyed);
  EXPECT_TRUE(message.is_serialized());
  EXPECT_EQ(kTestPayloadSize, message.payload_num_bytes());

  // Calling SerializeIfNecessary() again on an already-serialized message is a
  // no-op and preserves the payload buffer.
  message.SerializeIfNecessary();
  EXPECT_TRUE(message.is_serialized());
  EXPECT_EQ(kTestPayloadSize, message.payload_num_bytes());
}

TEST(BindingsMessageTest, DestroyTakenUnserializedContextBeforeMessage) {
  bool context_destroyed = false;
  Message message(
      std::make_unique<TestUnserializedMessageContext>(
          kTestMessageName, kTestMessageFlags,
          base::BindLambdaForTesting([&] { context_destroyed = true; })),
      MOJO_CREATE_MESSAGE_FLAG_NONE);
  EXPECT_FALSE(message.is_serialized());

  auto context =
      message.TakeUnserializedContext<TestUnserializedMessageContext>();
  ASSERT_TRUE(context);
  EXPECT_EQ(static_cast<uint32_t>(kTestMessageName), context->message_name());
  EXPECT_EQ(static_cast<uint32_t>(kTestMessageFlags), context->message_flags());
  EXPECT_EQ(nullptr, message.payload_buffer()->data());

  // Destroy the extracted context before `message` goes out of scope to ensure
  // `message` no longer holds any dangling reference to it.
  context.reset();
  EXPECT_TRUE(context_destroyed);
}

}  // namespace
}  // namespace test
}  // namespace mojo
