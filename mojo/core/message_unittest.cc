// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_math.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "mojo/core/test/mojo_test_base.h"
#include "mojo/core/user_message_impl.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace mojo {
namespace core {
namespace {

using MessageTest = test::MojoTestBase;

// Helper class which provides a base implementation for an unserialized user
// message context and helpers to go between these objects and opaque message
// handles.
class TestMessageBase {
 public:
  virtual ~TestMessageBase() {}

  static MojoMessageHandle MakeMessageHandle(
      std::unique_ptr<TestMessageBase> message) {
    MojoMessageHandle handle;
    MojoResult rv = MojoCreateMessage(nullptr, &handle);
    DCHECK_EQ(MOJO_RESULT_OK, rv);

    rv = MojoSetMessageContext(
        handle, reinterpret_cast<uintptr_t>(message.release()),
        &TestMessageBase::SerializeMessageContext,
        &TestMessageBase::DestroyMessageContext, nullptr);
    DCHECK_EQ(MOJO_RESULT_OK, rv);

    return handle;
  }

  template <typename T>
  static std::unique_ptr<T> UnwrapMessageHandle(
      MojoMessageHandle* message_handle) {
    MojoMessageHandle handle = MOJO_HANDLE_INVALID;
    std::swap(handle, *message_handle);
    uintptr_t context;
    MojoResult rv = MojoGetMessageContext(handle, nullptr, &context);
    DCHECK_EQ(MOJO_RESULT_OK, rv);
    rv = MojoSetMessageContext(handle, 0, nullptr, nullptr, nullptr);
    DCHECK_EQ(MOJO_RESULT_OK, rv);
    MojoDestroyMessage(handle);
    return base::WrapUnique(reinterpret_cast<T*>(context));
  }

 protected:
  virtual void GetSerializedSize(size_t* num_bytes, size_t* num_handles) = 0;
  virtual void SerializeHandles(MojoHandle* handles) = 0;
  virtual void SerializePayload(void* buffer) = 0;

 private:
  static void SerializeMessageContext(MojoMessageHandle message_handle,
                                      uintptr_t context) {
    auto* message = reinterpret_cast<TestMessageBase*>(context);
    size_t num_bytes = 0;
    size_t num_handles = 0;
    message->GetSerializedSize(&num_bytes, &num_handles);
    std::vector<MojoHandle> handles(num_handles);
    if (num_handles)
      message->SerializeHandles(handles.data());

    MojoAppendMessageDataOptions options;
    options.struct_size = sizeof(options);
    options.flags = MOJO_APPEND_MESSAGE_DATA_FLAG_COMMIT_SIZE;
    void* buffer;
    uint32_t buffer_size;
    MojoResult rv = MojoAppendMessageData(
        message_handle, base::checked_cast<uint32_t>(num_bytes), handles.data(),
        base::checked_cast<uint32_t>(num_handles), &options, &buffer,
        &buffer_size);
    DCHECK_EQ(MOJO_RESULT_OK, rv);
    DCHECK_GE(buffer_size, base::checked_cast<uint32_t>(num_bytes));
    if (num_bytes)
      message->SerializePayload(buffer);
  }

  static void DestroyMessageContext(uintptr_t context) {
    delete reinterpret_cast<TestMessageBase*>(context);
  }
};

class NeverSerializedMessage : public TestMessageBase {
 public:
  NeverSerializedMessage(
      base::OnceClosure destruction_callback = base::OnceClosure())
      : destruction_callback_(std::move(destruction_callback)) {}
  ~NeverSerializedMessage() override {
    if (destruction_callback_)
      std::move(destruction_callback_).Run();
  }

 private:
  // TestMessageBase:
  void GetSerializedSize(size_t* num_bytes, size_t* num_handles) override {
    NOTREACHED();
  }
  void SerializeHandles(MojoHandle* handles) override { NOTREACHED(); }
  void SerializePayload(void* buffer) override { NOTREACHED(); }

  base::OnceClosure destruction_callback_;

  DISALLOW_COPY_AND_ASSIGN(NeverSerializedMessage);
};

class SimpleMessage : public TestMessageBase {
 public:
  SimpleMessage(const std::string& contents,
                base::OnceClosure destruction_callback = base::OnceClosure())
      : contents_(contents),
        destruction_callback_(std::move(destruction_callback)) {}

  ~SimpleMessage() override {
    if (destruction_callback_)
      std::move(destruction_callback_).Run();
  }

  void AddMessagePipe(mojo::ScopedMessagePipeHandle handle) {
    handles_.emplace_back(std::move(handle));
  }

  std::vector<mojo::ScopedMessagePipeHandle>& handles() { return handles_; }

 private:
  // TestMessageBase:
  void GetSerializedSize(size_t* num_bytes, size_t* num_handles) override {
    *num_bytes = contents_.size();
    *num_handles = handles_.size();
  }

  void SerializeHandles(MojoHandle* handles) override {
    ASSERT_TRUE(!handles_.empty());
    for (size_t i = 0; i < handles_.size(); ++i)
      handles[i] = handles_[i].release().value();
    handles_.clear();
  }

  void SerializePayload(void* buffer) override {
    std::copy(contents_.begin(), contents_.end(), static_cast<char*>(buffer));
  }

  const std::string contents_;
  base::OnceClosure destruction_callback_;
  std::vector<mojo::ScopedMessagePipeHandle> handles_;

  DISALLOW_COPY_AND_ASSIGN(SimpleMessage);
};

TEST_F(MessageTest, InvalidMessageObjects) {
  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoDestroyMessage(MOJO_MESSAGE_HANDLE_INVALID));

  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoAppendMessageData(MOJO_MESSAGE_HANDLE_INVALID, 0, nullptr, 0,
                                  nullptr, nullptr, nullptr));

  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoGetMessageData(MOJO_MESSAGE_HANDLE_INVALID, nullptr, nullptr,
                               nullptr, nullptr, nullptr));

  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoSerializeMessage(MOJO_MESSAGE_HANDLE_INVALID, nullptr));

  MojoMessageHandle message_handle;
  ASSERT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoCreateMessage(nullptr, nullptr));
  ASSERT_EQ(MOJO_RESULT_OK, MojoCreateMessage(nullptr, &message_handle));
  ASSERT_EQ(MOJO_RESULT_OK, MojoSetMessageContext(message_handle, 0, nullptr,
                                                  nullptr, nullptr));
  ASSERT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message_handle));
}

TEST_F(MessageTest, SendLocalMessageWithContext) {
  // Simple write+read of a message with context. Verifies that such messages
  // are passed through a local pipe without serialization.
  auto message = std::make_unique<NeverSerializedMessage>();
  auto* original_message = message.get();

  MojoHandle a, b;
  CreateMessagePipe(&a, &b);
  EXPECT_EQ(
      MOJO_RESULT_OK,
      MojoWriteMessage(
          a, TestMessageBase::MakeMessageHandle(std::move(message)), nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, WaitForSignals(b, MOJO_HANDLE_SIGNAL_READABLE));

  MojoMessageHandle read_message_handle;
  EXPECT_EQ(MOJO_RESULT_OK, MojoReadMessage(b, nullptr, &read_message_handle));
  message = TestMessageBase::UnwrapMessageHandle<NeverSerializedMessage>(
      &read_message_handle);
  EXPECT_EQ(original_message, message.get());

  MojoClose(a);
  MojoClose(b);
}

TEST_F(MessageTest, DestroyMessageWithContext) {
  // Tests that |MojoDestroyMessage()| destroys any attached context.
  bool was_deleted = false;
  auto message = std::make_unique<NeverSerializedMessage>(base::BindOnce(
      [](bool* was_deleted) { *was_deleted = true; }, &was_deleted));
  MojoMessageHandle handle =
      TestMessageBase::MakeMessageHandle(std::move(message));
  EXPECT_FALSE(was_deleted);
  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(handle));
  EXPECT_TRUE(was_deleted);
}

const char kTestMessageWithContext1[] = "hello laziness";

#if !defined(OS_IOS)

const char kTestMessageWithContext2[] = "my old friend";
const char kTestMessageWithContext3[] = "something something";
const char kTestMessageWithContext4[] = "do moar ipc";
const char kTestQuitMessage[] = "quit";

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(ReceiveMessageNoHandles, MessageTest, h) {
  MojoTestBase::WaitForSignals(h, MOJO_HANDLE_SIGNAL_READABLE);
  auto m = MojoTestBase::ReadMessage(h);
  EXPECT_EQ(kTestMessageWithContext1, m);
}

TEST_F(MessageTest, SerializeSimpleMessageNoHandlesWithContext) {
  RunTestClient("ReceiveMessageNoHandles", [&](MojoHandle h) {
    auto message = std::make_unique<SimpleMessage>(kTestMessageWithContext1);
    MojoWriteMessage(h, TestMessageBase::MakeMessageHandle(std::move(message)),
                     nullptr);
  });
}

TEST_F(MessageTest, SerializeDynamicallySizedMessage) {
  RunTestClient("ReceiveMessageNoHandles", [&](MojoHandle h) {
    MojoMessageHandle message;
    EXPECT_EQ(MOJO_RESULT_OK, MojoCreateMessage(nullptr, &message));

    void* buffer;
    uint32_t buffer_size;
    EXPECT_EQ(MOJO_RESULT_OK,
              MojoAppendMessageData(message, 0, nullptr, 0, nullptr, &buffer,
                                    &buffer_size));

    MojoAppendMessageDataOptions options;
    options.struct_size = sizeof(options);
    options.flags = MOJO_APPEND_MESSAGE_DATA_FLAG_COMMIT_SIZE;
    EXPECT_EQ(MOJO_RESULT_OK, MojoAppendMessageData(
                                  message, sizeof(kTestMessageWithContext1) - 1,
                                  nullptr, 0, &options, &buffer, &buffer_size));

    memcpy(buffer, kTestMessageWithContext1,
           sizeof(kTestMessageWithContext1) - 1);
    MojoWriteMessage(h, message, nullptr);
  });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(ReceiveMessageOneHandle, MessageTest, h) {
  MojoTestBase::WaitForSignals(h, MOJO_HANDLE_SIGNAL_READABLE);
  MojoHandle h1;
  auto m = MojoTestBase::ReadMessageWithHandles(h, &h1, 1);
  EXPECT_EQ(kTestMessageWithContext1, m);
  MojoTestBase::WriteMessage(h1, kTestMessageWithContext2);
  EXPECT_EQ(kTestQuitMessage, MojoTestBase::ReadMessage(h));
}

TEST_F(MessageTest, SerializeSimpleMessageOneHandleWithContext) {
  RunTestClient("ReceiveMessageOneHandle", [&](MojoHandle h) {
    auto message = std::make_unique<SimpleMessage>(kTestMessageWithContext1);
    mojo::MessagePipe pipe;
    message->AddMessagePipe(std::move(pipe.handle0));
    MojoWriteMessage(h, TestMessageBase::MakeMessageHandle(std::move(message)),
                     nullptr);
    EXPECT_EQ(kTestMessageWithContext2,
              MojoTestBase::ReadMessage(pipe.handle1.get().value()));
    MojoTestBase::WriteMessage(h, kTestQuitMessage);
  });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(ReceiveMessageWithHandles, MessageTest, h) {
  MojoTestBase::WaitForSignals(h, MOJO_HANDLE_SIGNAL_READABLE);
  MojoHandle handles[4];
  auto m = MojoTestBase::ReadMessageWithHandles(h, handles, 4);
  EXPECT_EQ(kTestMessageWithContext1, m);
  MojoTestBase::WriteMessage(handles[0], kTestMessageWithContext1);
  MojoTestBase::WriteMessage(handles[1], kTestMessageWithContext2);
  MojoTestBase::WriteMessage(handles[2], kTestMessageWithContext3);
  MojoTestBase::WriteMessage(handles[3], kTestMessageWithContext4);

  EXPECT_EQ(kTestQuitMessage, MojoTestBase::ReadMessage(h));
}

TEST_F(MessageTest, SerializeSimpleMessageWithHandlesWithContext) {
  RunTestClient("ReceiveMessageWithHandles", [&](MojoHandle h) {
    auto message = std::make_unique<SimpleMessage>(kTestMessageWithContext1);
    mojo::MessagePipe pipes[4];
    message->AddMessagePipe(std::move(pipes[0].handle0));
    message->AddMessagePipe(std::move(pipes[1].handle0));
    message->AddMessagePipe(std::move(pipes[2].handle0));
    message->AddMessagePipe(std::move(pipes[3].handle0));
    MojoWriteMessage(h, TestMessageBase::MakeMessageHandle(std::move(message)),
                     nullptr);
    EXPECT_EQ(kTestMessageWithContext1,
              MojoTestBase::ReadMessage(pipes[0].handle1.get().value()));
    EXPECT_EQ(kTestMessageWithContext2,
              MojoTestBase::ReadMessage(pipes[1].handle1.get().value()));
    EXPECT_EQ(kTestMessageWithContext3,
              MojoTestBase::ReadMessage(pipes[2].handle1.get().value()));
    EXPECT_EQ(kTestMessageWithContext4,
              MojoTestBase::ReadMessage(pipes[3].handle1.get().value()));

    MojoTestBase::WriteMessage(h, kTestQuitMessage);
  });
}

#endif  // !defined(OS_IOS)

TEST_F(MessageTest, SendLocalSimpleMessageWithHandlesWithContext) {
  auto message = std::make_unique<SimpleMessage>(kTestMessageWithContext1);
  auto* original_message = message.get();
  mojo::MessagePipe pipes[4];
  MojoHandle original_handles[4] = {
      pipes[0].handle0.get().value(), pipes[1].handle0.get().value(),
      pipes[2].handle0.get().value(), pipes[3].handle0.get().value(),
  };
  message->AddMessagePipe(std::move(pipes[0].handle0));
  message->AddMessagePipe(std::move(pipes[1].handle0));
  message->AddMessagePipe(std::move(pipes[2].handle0));
  message->AddMessagePipe(std::move(pipes[3].handle0));

  MojoHandle a, b;
  CreateMessagePipe(&a, &b);
  EXPECT_EQ(
      MOJO_RESULT_OK,
      MojoWriteMessage(
          a, TestMessageBase::MakeMessageHandle(std::move(message)), nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, WaitForSignals(b, MOJO_HANDLE_SIGNAL_READABLE));

  MojoMessageHandle read_message_handle;
  EXPECT_EQ(MOJO_RESULT_OK, MojoReadMessage(b, nullptr, &read_message_handle));
  message =
      TestMessageBase::UnwrapMessageHandle<SimpleMessage>(&read_message_handle);
  EXPECT_EQ(original_message, message.get());
  ASSERT_EQ(4u, message->handles().size());
  EXPECT_EQ(original_handles[0], message->handles()[0].get().value());
  EXPECT_EQ(original_handles[1], message->handles()[1].get().value());
  EXPECT_EQ(original_handles[2], message->handles()[2].get().value());
  EXPECT_EQ(original_handles[3], message->handles()[3].get().value());

  MojoClose(a);
  MojoClose(b);
}

TEST_F(MessageTest, DropUnreadLocalMessageWithContext) {
  // Verifies that if a message is sent with context over a pipe and the
  // receiver closes without reading the message, the context is properly
  // cleaned up.
  bool message_was_destroyed = false;
  auto message = std::make_unique<SimpleMessage>(
      kTestMessageWithContext1,
      base::BindOnce([](bool* was_destroyed) { *was_destroyed = true; },
                     &message_was_destroyed));

  mojo::MessagePipe pipe;
  message->AddMessagePipe(std::move(pipe.handle0));
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);
  EXPECT_EQ(
      MOJO_RESULT_OK,
      MojoWriteMessage(
          a, TestMessageBase::MakeMessageHandle(std::move(message)), nullptr));
  MojoClose(a);
  MojoClose(b);

  EXPECT_TRUE(message_was_destroyed);
  EXPECT_EQ(MOJO_RESULT_OK, WaitForSignals(pipe.handle1.get().value(),
                                           MOJO_HANDLE_SIGNAL_PEER_CLOSED));
}

TEST_F(MessageTest, GetMessageDataWithHandles) {
  MojoHandle h[2];
  CreateMessagePipe(&h[0], &h[1]);

  MojoMessageHandle message_handle;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateMessage(nullptr, &message_handle));

  MojoAppendMessageDataOptions append_data_options;
  append_data_options.struct_size = sizeof(append_data_options);
  append_data_options.flags = MOJO_APPEND_MESSAGE_DATA_FLAG_COMMIT_SIZE;
  const std::string kTestMessage = "hello";
  void* buffer;
  uint32_t buffer_size;
  ASSERT_EQ(MOJO_RESULT_OK,
            MojoAppendMessageData(
                message_handle, static_cast<uint32_t>(kTestMessage.size()), h,
                2, &append_data_options, &buffer, &buffer_size));
  memcpy(buffer, kTestMessage.data(), kTestMessage.size());

  // Ignore handles the first time around. This should mean a subsequent call is
  // allowed to grab the handles.
  MojoGetMessageDataOptions get_data_options;
  get_data_options.struct_size = sizeof(get_data_options);
  get_data_options.flags = MOJO_GET_MESSAGE_DATA_FLAG_IGNORE_HANDLES;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoGetMessageData(message_handle, &get_data_options, &buffer,
                               &buffer_size, nullptr, nullptr));

  // Now grab the handles.
  uint32_t num_handles = 2;
  EXPECT_EQ(MOJO_RESULT_OK, MojoGetMessageData(message_handle, nullptr, &buffer,
                                               &buffer_size, h, &num_handles));
  EXPECT_EQ(2u, num_handles);

  // Should still be callable as long as we ignore handles.
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoGetMessageData(message_handle, &get_data_options, &buffer,
                               &buffer_size, nullptr, nullptr));

  // But not if we don't.
  EXPECT_EQ(MOJO_RESULT_NOT_FOUND,
            MojoGetMessageData(message_handle, nullptr, &buffer, &buffer_size,
                               h, &num_handles));

  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message_handle));
}

TEST_F(MessageTest, ReadMessageWithContextAsSerializedMessage) {
  bool message_was_destroyed = false;
  std::unique_ptr<TestMessageBase> message =
      std::make_unique<NeverSerializedMessage>(
          base::BindOnce([](bool* was_destroyed) { *was_destroyed = true; },
                         &message_was_destroyed));

  MojoHandle a, b;
  CreateMessagePipe(&a, &b);
  EXPECT_EQ(
      MOJO_RESULT_OK,
      MojoWriteMessage(
          a, TestMessageBase::MakeMessageHandle(std::move(message)), nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, WaitForSignals(b, MOJO_HANDLE_SIGNAL_READABLE));

  MojoMessageHandle message_handle;
  EXPECT_EQ(MOJO_RESULT_OK, MojoReadMessage(b, nullptr, &message_handle));
  EXPECT_FALSE(message_was_destroyed);

  // Not a serialized message, so we can't get serialized contents.
  uint32_t num_bytes = 0;
  void* buffer;
  uint32_t num_handles = 0;
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoGetMessageData(message_handle, nullptr, &buffer, &num_bytes,
                               nullptr, &num_handles));
  EXPECT_FALSE(message_was_destroyed);

  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message_handle));
  EXPECT_TRUE(message_was_destroyed);

  MojoClose(a);
  MojoClose(b);
}

TEST_F(MessageTest, ReadSerializedMessageAsMessageWithContext) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);
  MojoTestBase::WriteMessage(a, "hello there");
  EXPECT_EQ(MOJO_RESULT_OK, WaitForSignals(b, MOJO_HANDLE_SIGNAL_READABLE));

  MojoMessageHandle message_handle;
  EXPECT_EQ(MOJO_RESULT_OK, MojoReadMessage(b, nullptr, &message_handle));
  uintptr_t context;
  EXPECT_EQ(MOJO_RESULT_NOT_FOUND,
            MojoGetMessageContext(message_handle, nullptr, &context));
  MojoClose(a);
  MojoClose(b);
  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message_handle));
}

TEST_F(MessageTest, ForceSerializeMessageWithContext) {
  // Basic test - we can serialize a simple message.
  bool message_was_destroyed = false;
  auto message = std::make_unique<SimpleMessage>(
      kTestMessageWithContext1,
      base::BindOnce([](bool* was_destroyed) { *was_destroyed = true; },
                     &message_was_destroyed));
  auto message_handle = TestMessageBase::MakeMessageHandle(std::move(message));
  EXPECT_EQ(MOJO_RESULT_OK, MojoSerializeMessage(message_handle, nullptr));
  EXPECT_TRUE(message_was_destroyed);
  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message_handle));

  // Serialize a message with a single handle. Freeing the message should close
  // the handle.
  message_was_destroyed = false;
  message = std::make_unique<SimpleMessage>(
      kTestMessageWithContext1,
      base::BindOnce([](bool* was_destroyed) { *was_destroyed = true; },
                     &message_was_destroyed));
  MessagePipe pipe1;
  message->AddMessagePipe(std::move(pipe1.handle0));
  message_handle = TestMessageBase::MakeMessageHandle(std::move(message));
  EXPECT_EQ(MOJO_RESULT_OK, MojoSerializeMessage(message_handle, nullptr));
  EXPECT_TRUE(message_was_destroyed);
  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message_handle));
  EXPECT_EQ(MOJO_RESULT_OK, WaitForSignals(pipe1.handle1.get().value(),
                                           MOJO_HANDLE_SIGNAL_PEER_CLOSED));

  // Serialize a message with a handle and extract its serialized contents.
  message_was_destroyed = false;
  message = std::make_unique<SimpleMessage>(
      kTestMessageWithContext1,
      base::BindOnce([](bool* was_destroyed) { *was_destroyed = true; },
                     &message_was_destroyed));
  MessagePipe pipe2;
  message->AddMessagePipe(std::move(pipe2.handle0));
  message_handle = TestMessageBase::MakeMessageHandle(std::move(message));
  EXPECT_EQ(MOJO_RESULT_OK, MojoSerializeMessage(message_handle, nullptr));
  EXPECT_TRUE(message_was_destroyed);
  uint32_t num_bytes = 0;
  void* buffer = nullptr;
  uint32_t num_handles = 0;
  MojoHandle extracted_handle;
  EXPECT_EQ(MOJO_RESULT_RESOURCE_EXHAUSTED,
            MojoGetMessageData(message_handle, nullptr, &buffer, &num_bytes,
                               nullptr, &num_handles));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoGetMessageData(message_handle, nullptr, &buffer, &num_bytes,
                               &extracted_handle, &num_handles));
  EXPECT_EQ(std::string(kTestMessageWithContext1).size(), num_bytes);
  EXPECT_EQ(std::string(kTestMessageWithContext1),
            base::StringPiece(static_cast<char*>(buffer), num_bytes));

  // Confirm that the handle we extracted from the serialized message is still
  // connected to the same peer, despite the fact that its handle value may have
  // changed.
  const char kTestMessage[] = "hey you";
  MojoTestBase::WriteMessage(pipe2.handle1.get().value(), kTestMessage);
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(extracted_handle, MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_EQ(kTestMessage, MojoTestBase::ReadMessage(extracted_handle));

  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message_handle));
}

TEST_F(MessageTest, DoubleSerialize) {
  bool message_was_destroyed = false;
  auto message = std::make_unique<SimpleMessage>(
      kTestMessageWithContext1,
      base::BindOnce([](bool* was_destroyed) { *was_destroyed = true; },
                     &message_was_destroyed));
  auto message_handle = TestMessageBase::MakeMessageHandle(std::move(message));

  // Ensure we can safely call |MojoSerializeMessage()| twice on the same
  // message handle.
  EXPECT_EQ(MOJO_RESULT_OK, MojoSerializeMessage(message_handle, nullptr));
  EXPECT_TRUE(message_was_destroyed);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoSerializeMessage(message_handle, nullptr));

  // And also check that we can call it again after we've written and read the
  // message object from a pipe.
  MessagePipe pipe;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoWriteMessage(pipe.handle0->value(), message_handle, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(pipe.handle1->value(), MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoReadMessage(pipe.handle1->value(), nullptr, &message_handle));
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoSerializeMessage(message_handle, nullptr));

  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message_handle));
}

TEST_F(MessageTest, ExtendMessagePayload) {
  MojoMessageHandle message;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateMessage(nullptr, &message));

  const std::string kTestMessagePart1("hello i am message.");
  void* buffer;
  uint32_t buffer_size;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAppendMessageData(
                message, static_cast<uint32_t>(kTestMessagePart1.size()),
                nullptr, 0, nullptr, &buffer, &buffer_size));
  ASSERT_GE(buffer_size, static_cast<uint32_t>(kTestMessagePart1.size()));
  memcpy(buffer, kTestMessagePart1.data(), kTestMessagePart1.size());

  const std::string kTestMessagePart2 = " in ur computer.";
  const std::string kTestMessageCombined1 =
      kTestMessagePart1 + kTestMessagePart2;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAppendMessageData(
                message, static_cast<uint32_t>(kTestMessagePart2.size()),
                nullptr, 0, nullptr, &buffer, &buffer_size));
  memcpy(static_cast<uint8_t*>(buffer) + kTestMessagePart1.size(),
         kTestMessagePart2.data(), kTestMessagePart2.size());

  const std::string kTestMessagePart3 = kTestMessagePart2 + " carry ur bits.";
  const std::string kTestMessageCombined2 =
      kTestMessageCombined1 + kTestMessagePart3;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAppendMessageData(
                message, static_cast<uint32_t>(kTestMessagePart3.size()),
                nullptr, 0, nullptr, &buffer, &buffer_size));
  memcpy(static_cast<uint8_t*>(buffer) + kTestMessageCombined1.size(),
         kTestMessagePart3.data(), kTestMessagePart3.size());

  void* payload;
  uint32_t payload_size;
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoGetMessageData(message, nullptr, &payload, &payload_size,
                               nullptr, nullptr));

  MojoAppendMessageDataOptions options;
  options.struct_size = sizeof(options);
  options.flags = MOJO_APPEND_MESSAGE_DATA_FLAG_COMMIT_SIZE;
  EXPECT_EQ(MOJO_RESULT_OK, MojoAppendMessageData(message, 0, nullptr, 0,
                                                  &options, nullptr, nullptr));

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoGetMessageData(message, nullptr, &payload, &payload_size,
                               nullptr, nullptr));
  EXPECT_EQ(kTestMessageCombined2.size(), payload_size);
  EXPECT_EQ(0, memcmp(payload, kTestMessageCombined2.data(),
                      kTestMessageCombined2.size()));

  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message));
}

TEST_F(MessageTest, ExtendMessageWithHandlesPayload) {
  MojoMessageHandle message;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateMessage(nullptr, &message));

  MojoHandle handles[2];
  CreateMessagePipe(&handles[0], &handles[1]);

  const std::string kTestMessagePart1("hello i am message.");
  void* buffer;
  uint32_t buffer_size;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAppendMessageData(
                message, static_cast<uint32_t>(kTestMessagePart1.size()),
                handles, 2, nullptr, &buffer, &buffer_size));
  ASSERT_GE(buffer_size, static_cast<uint32_t>(kTestMessagePart1.size()));
  memcpy(buffer, kTestMessagePart1.data(), kTestMessagePart1.size());

  const std::string kTestMessagePart2 = " in ur computer.";
  const std::string kTestMessageCombined1 =
      kTestMessagePart1 + kTestMessagePart2;
  MojoAppendMessageDataOptions options;
  options.struct_size = sizeof(options);
  options.flags = MOJO_APPEND_MESSAGE_DATA_FLAG_COMMIT_SIZE;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAppendMessageData(
                message, static_cast<uint32_t>(kTestMessagePart2.size()),
                nullptr, 0, &options, &buffer, &buffer_size));
  memcpy(static_cast<uint8_t*>(buffer) + kTestMessagePart1.size(),
         kTestMessagePart2.data(), kTestMessagePart2.size());

  void* payload;
  uint32_t payload_size;
  uint32_t num_handles = 2;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoGetMessageData(message, nullptr, &payload, &payload_size,
                               handles, &num_handles));
  EXPECT_EQ(2u, num_handles);
  EXPECT_EQ(kTestMessageCombined1.size(), payload_size);
  EXPECT_EQ(0, memcmp(payload, kTestMessageCombined1.data(),
                      kTestMessageCombined1.size()));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(handles[0]));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(handles[1]));
  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message));
}

TEST_F(MessageTest, ExtendMessagePayloadLarge) {
  // We progressively extend a message payload from small to large using various
  // chunk sizes to test potentially interesting boundary conditions.
  constexpr size_t kTestChunkSizes[] = {1, 2, 3, 64, 509, 4096, 16384, 65535};
  for (const size_t kChunkSize : kTestChunkSizes) {
    MojoMessageHandle message;
    EXPECT_EQ(MOJO_RESULT_OK, MojoCreateMessage(nullptr, &message));

    MojoHandle handles[2];
    CreateMessagePipe(&handles[0], &handles[1]);

    const std::string kTestMessageHeader("hey pretend i'm a header");
    void* buffer;
    uint32_t buffer_size;
    EXPECT_EQ(MOJO_RESULT_OK,
              MojoAppendMessageData(
                  message, static_cast<uint32_t>(kTestMessageHeader.size()),
                  handles, 2, nullptr, &buffer, &buffer_size));
    ASSERT_GE(buffer_size, static_cast<uint32_t>(kTestMessageHeader.size()));
    memcpy(buffer, kTestMessageHeader.data(), kTestMessageHeader.size());

    // 512 kB should be well beyond any reasonable default buffer size for the
    // system implementation to choose, meaning that this test should guarantee
    // several reallocations of the serialized message buffer as we
    // progressively extend the payload to this size.
    constexpr size_t kTestMessagePayloadSize = 512 * 1024;
    std::vector<uint8_t> test_payload(kTestMessagePayloadSize);
    base::RandBytes(test_payload.data(), kTestMessagePayloadSize);

    size_t current_payload_size = 0;
    while (current_payload_size < kTestMessagePayloadSize) {
      const size_t previous_payload_size = current_payload_size;
      current_payload_size =
          std::min(current_payload_size + kChunkSize, kTestMessagePayloadSize);
      const size_t current_chunk_size =
          current_payload_size - previous_payload_size;
      const size_t previous_total_size =
          kTestMessageHeader.size() + previous_payload_size;
      const size_t current_total_size =
          kTestMessageHeader.size() + current_payload_size;
      EXPECT_EQ(MOJO_RESULT_OK,
                MojoAppendMessageData(
                    message, static_cast<uint32_t>(current_chunk_size), nullptr,
                    0, nullptr, &buffer, &buffer_size));
      EXPECT_GE(buffer_size, static_cast<uint32_t>(current_total_size));
      memcpy(static_cast<uint8_t*>(buffer) + previous_total_size,
             &test_payload[previous_payload_size], current_chunk_size);
    }

    MojoAppendMessageDataOptions options;
    options.struct_size = sizeof(options);
    options.flags = MOJO_APPEND_MESSAGE_DATA_FLAG_COMMIT_SIZE;
    EXPECT_EQ(MOJO_RESULT_OK,
              MojoAppendMessageData(message, 0, nullptr, 0, &options, nullptr,
                                    nullptr));

    void* payload;
    uint32_t payload_size;
    uint32_t num_handles = 2;
    EXPECT_EQ(MOJO_RESULT_OK,
              MojoGetMessageData(message, nullptr, &payload, &payload_size,
                                 handles, &num_handles));
    EXPECT_EQ(static_cast<uint32_t>(kTestMessageHeader.size() +
                                    kTestMessagePayloadSize),
              payload_size);
    EXPECT_EQ(0, memcmp(payload, kTestMessageHeader.data(),
                        kTestMessageHeader.size()));
    EXPECT_EQ(0,
              memcmp(static_cast<uint8_t*>(payload) + kTestMessageHeader.size(),
                     test_payload.data(), kTestMessagePayloadSize));

    EXPECT_EQ(MOJO_RESULT_OK, MojoClose(handles[0]));
    EXPECT_EQ(MOJO_RESULT_OK, MojoClose(handles[1]));
    EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message));
  }
}

TEST_F(MessageTest, CorrectPayloadBufferBoundaries) {
  // Exercises writes to the full extent of a message's payload under various
  // circumstances in an effort to catch any potential bugs in internal
  // allocations or reported size from Mojo APIs.

  MojoMessageHandle message;
  void* buffer = nullptr;
  uint32_t buffer_size = 0;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateMessage(nullptr, &message));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAppendMessageData(message, 0, nullptr, 0, nullptr, &buffer,
                                  &buffer_size));
  // Fill the buffer end-to-end.
  memset(buffer, 'x', buffer_size);

  // Continuously grow and fill the message buffer several more times. Should
  // not crash.
  constexpr uint32_t kChunkSize = 4096;
  constexpr size_t kNumIterations = 1000;
  for (size_t i = 0; i < kNumIterations; ++i) {
    EXPECT_EQ(MOJO_RESULT_OK,
              MojoAppendMessageData(message, kChunkSize, nullptr, 0, nullptr,
                                    &buffer, &buffer_size));
    memset(buffer, 'x', buffer_size);
  }

  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message));
}

TEST_F(MessageTest, CommitInvalidMessageContents) {
  // Regression test for https://crbug.com/755127. Ensures that we don't crash
  // if we attempt to commit the contents of an unserialized message.
  MojoMessageHandle message;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateMessage(nullptr, &message));
  EXPECT_EQ(MOJO_RESULT_OK, MojoAppendMessageData(message, 0, nullptr, 0,
                                                  nullptr, nullptr, nullptr));
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);
  EXPECT_EQ(MOJO_RESULT_OK, MojoAppendMessageData(message, 0, &a, 1, nullptr,
                                                  nullptr, nullptr));

  UserMessageImpl::FailHandleSerializationForTesting(true);
  MojoAppendMessageDataOptions options;
  options.struct_size = sizeof(options);
  options.flags = MOJO_APPEND_MESSAGE_DATA_FLAG_COMMIT_SIZE;
  EXPECT_EQ(MOJO_RESULT_OK, MojoAppendMessageData(message, 0, nullptr, 0,
                                                  nullptr, nullptr, nullptr));
  UserMessageImpl::FailHandleSerializationForTesting(false);
  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message));
}

#if !defined(OS_IOS)

TEST_F(MessageTest, ExtendPayloadWithHandlesAttached) {
  // Regression test for https://crbug.com/748996. Verifies that internal
  // message objects do not retain invalid payload pointers across buffer
  // relocations.

  MojoHandle handles[5];
  CreateMessagePipe(&handles[0], &handles[1]);
  PlatformChannel channel;
  handles[2] =
      WrapPlatformHandle(channel.TakeLocalEndpoint().TakePlatformHandle())
          .release()
          .value();
  handles[3] =
      WrapPlatformHandle(channel.TakeRemoteEndpoint().TakePlatformHandle())
          .release()
          .value();
  handles[4] = SharedBufferHandle::Create(64).release().value();

  MojoMessageHandle message;
  void* buffer = nullptr;
  uint32_t buffer_size = 0;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateMessage(nullptr, &message));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAppendMessageData(message, 0, handles, 5, nullptr, &buffer,
                                  &buffer_size));

  // Force buffer reallocation by extending the payload beyond the original
  // buffer size. This should typically result in a relocation of the buffer as
  // well -- at least often enough that breakage will be caught by automated
  // tests.
  MojoAppendMessageDataOptions options;
  options.struct_size = sizeof(options);
  options.flags = MOJO_APPEND_MESSAGE_DATA_FLAG_COMMIT_SIZE;
  uint32_t payload_size = buffer_size * 64;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAppendMessageData(message, payload_size, nullptr, 0, &options,
                                  &buffer, &buffer_size));
  ASSERT_GE(buffer_size, payload_size);
  memset(buffer, 'x', payload_size);

  RunTestClient("ReadAndIgnoreMessage", [&](MojoHandle h) {
    // Send the message out of process to exercise the regression path where
    // internally cached, stale payload pointers may be dereferenced and written
    // into.
    EXPECT_EQ(MOJO_RESULT_OK, MojoWriteMessage(h, message, nullptr));
  });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(ReadAndIgnoreMessage, MessageTest, h) {
  MojoTestBase::WaitForSignals(h, MOJO_HANDLE_SIGNAL_READABLE);

  MojoHandle handles[5];
  MojoTestBase::ReadMessageWithHandles(h, handles, 5);
  for (size_t i = 0; i < 5; ++i)
    EXPECT_EQ(MOJO_RESULT_OK, MojoClose(handles[i]));
}

TEST_F(MessageTest, ExtendPayloadWithHandlesAttachedViaExtension) {
  MojoHandle handles[5];
  CreateMessagePipe(&handles[0], &handles[4]);
  PlatformChannel channel;
  handles[1] =
      WrapPlatformHandle(channel.TakeLocalEndpoint().TakePlatformHandle())
          .release()
          .value();
  handles[2] =
      WrapPlatformHandle(channel.TakeRemoteEndpoint().TakePlatformHandle())
          .release()
          .value();
  handles[3] = SharedBufferHandle::Create(64).release().value();

  MojoMessageHandle message;
  void* buffer = nullptr;
  uint32_t buffer_size = 0;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateMessage(nullptr, &message));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAppendMessageData(message, 0, handles, 1, nullptr, &buffer,
                                  &buffer_size));
  uint32_t payload_size = buffer_size * 64;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAppendMessageData(message, payload_size, nullptr, 0, nullptr,
                                  &buffer, nullptr));

  // Add more handles.
  EXPECT_EQ(MOJO_RESULT_OK, MojoAppendMessageData(message, 0, handles + 1, 1,
                                                  nullptr, &buffer, nullptr));
  MojoAppendMessageDataOptions options;
  options.struct_size = sizeof(options);
  options.flags = MOJO_APPEND_MESSAGE_DATA_FLAG_COMMIT_SIZE;
  EXPECT_EQ(MOJO_RESULT_OK, MojoAppendMessageData(message, 0, handles + 2, 3,
                                                  &options, &buffer, nullptr));
  memset(buffer, 'x', payload_size);

  RunTestClient("ReadMessageAndCheckPipe", [&](MojoHandle h) {
    // Send the message out of process to exercise the regression path where
    // internally cached, stale payload pointers may be dereferenced and written
    // into.
    EXPECT_EQ(MOJO_RESULT_OK, MojoWriteMessage(h, message, nullptr));
  });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(ReadMessageAndCheckPipe, MessageTest, h) {
  MojoTestBase::WaitForSignals(h, MOJO_HANDLE_SIGNAL_READABLE);

  const std::string kTestMessage("hey pipe");
  MojoHandle handles[5];
  MojoTestBase::ReadMessageWithHandles(h, handles, 5);
  MojoTestBase::WriteMessage(handles[0], kTestMessage);
  MojoTestBase::WaitForSignals(handles[4], MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ(kTestMessage, MojoTestBase::ReadMessage(handles[4]));
  for (size_t i = 0; i < 5; ++i)
    EXPECT_EQ(MOJO_RESULT_OK, MojoClose(handles[i]));
}

#endif  // !defined(OS_IOS)

TEST_F(MessageTest, PartiallySerializedMessagesDontLeakHandles) {
  MojoMessageHandle message;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateMessage(nullptr, &message));

  MojoHandle handles[2];
  CreateMessagePipe(&handles[0], &handles[1]);

  const std::string kTestMessagePart1("hello i am message.");
  void* buffer;
  uint32_t buffer_size;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAppendMessageData(
                message, static_cast<uint32_t>(kTestMessagePart1.size()),
                nullptr, 0, nullptr, &buffer, &buffer_size));
  ASSERT_GE(buffer_size, static_cast<uint32_t>(kTestMessagePart1.size()));
  memcpy(buffer, kTestMessagePart1.data(), kTestMessagePart1.size());

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAppendMessageData(message, 0, handles, 1, nullptr, &buffer,
                                  &buffer_size));

  // This must close |handles[0]|, which we can detect by observing the
  // signal state of |handles[1].
  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message));
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(handles[1], MOJO_HANDLE_SIGNAL_PEER_CLOSED));
}

}  // namespace
}  // namespace core
}  // namespace mojo
