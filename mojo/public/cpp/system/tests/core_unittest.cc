// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This file tests the C++ Mojo system core wrappers.
// TODO(vtl): Maybe rename "CoreCppTest" -> "CoreTest" if/when this gets
// compiled into a different binary from the C API tests.

#include <stddef.h>
#include <stdint.h>
#include <map>
#include <utility>

#include "base/containers/contains.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/functions.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/wait.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

const MojoHandleSignals kSignalReadableWritable =
    MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE;

const MojoHandleSignals kSignalAll =
    MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_WRITABLE |
    MOJO_HANDLE_SIGNAL_PEER_CLOSED | MOJO_HANDLE_SIGNAL_PEER_REMOTE |
    MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED;

TEST(CoreCppTest, GetTimeTicksNow) {
  const MojoTimeTicks start = GetTimeTicksNow();
  EXPECT_NE(static_cast<MojoTimeTicks>(0), start)
      << "GetTimeTicksNow should return nonzero value";
}

TEST(CoreCppTest, Basic) {
  // Basic |Handle| implementation:
  {
    EXPECT_EQ(MOJO_HANDLE_INVALID, kInvalidHandleValue);

    Handle h0;
    EXPECT_EQ(kInvalidHandleValue, h0.value());
    EXPECT_EQ(kInvalidHandleValue, *h0.mutable_value());
    EXPECT_FALSE(h0.is_valid());

    Handle h1(static_cast<MojoHandle>(123));
    EXPECT_EQ(static_cast<MojoHandle>(123), h1.value());
    EXPECT_EQ(static_cast<MojoHandle>(123), *h1.mutable_value());
    EXPECT_TRUE(h1.is_valid());
    *h1.mutable_value() = static_cast<MojoHandle>(456);
    EXPECT_EQ(static_cast<MojoHandle>(456), h1.value());
    EXPECT_TRUE(h1.is_valid());

    h1.swap(h0);
    EXPECT_EQ(static_cast<MojoHandle>(456), h0.value());
    EXPECT_TRUE(h0.is_valid());
    EXPECT_FALSE(h1.is_valid());

    h1.set_value(static_cast<MojoHandle>(789));
    h0.swap(h1);
    EXPECT_EQ(static_cast<MojoHandle>(789), h0.value());
    EXPECT_TRUE(h0.is_valid());
    EXPECT_EQ(static_cast<MojoHandle>(456), h1.value());
    EXPECT_TRUE(h1.is_valid());

    // Make sure copy constructor works.
    Handle h2(h0);
    EXPECT_EQ(static_cast<MojoHandle>(789), h2.value());
    // And assignment.
    h2 = h1;
    EXPECT_EQ(static_cast<MojoHandle>(456), h2.value());

    // Make sure that we can put |Handle|s into |std::map|s.
    h0 = Handle(static_cast<MojoHandle>(987));
    h1 = Handle(static_cast<MojoHandle>(654));
    h2 = Handle(static_cast<MojoHandle>(321));
    Handle h3;
    std::map<Handle, int> handle_to_int;
    handle_to_int[h0] = 0;
    handle_to_int[h1] = 1;
    handle_to_int[h2] = 2;
    handle_to_int[h3] = 3;

    EXPECT_EQ(4u, handle_to_int.size());
    EXPECT_TRUE(base::Contains(handle_to_int, h0));
    EXPECT_EQ(0, handle_to_int[h0]);
    EXPECT_TRUE(base::Contains(handle_to_int, h1));
    EXPECT_EQ(1, handle_to_int[h1]);
    EXPECT_TRUE(base::Contains(handle_to_int, h2));
    EXPECT_EQ(2, handle_to_int[h2]);
    EXPECT_TRUE(base::Contains(handle_to_int, h3));
    EXPECT_EQ(3, handle_to_int[h3]);
    EXPECT_FALSE(
        base::Contains(handle_to_int, Handle(static_cast<MojoHandle>(13579))));

    // TODO(vtl): With C++11, support |std::unordered_map|s, etc. (Or figure out
    // how to support the variations of |hash_map|.)
  }

  // |Handle|/|ScopedHandle| functions:
  {
    ScopedHandle h;

    EXPECT_EQ(kInvalidHandleValue, h.get().value());

    // This should be a no-op.
    Close(std::move(h));

    // It should still be invalid.
    EXPECT_EQ(kInvalidHandleValue, h.get().value());

    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              Wait(h.get(), ~MOJO_HANDLE_SIGNAL_NONE));

    std::vector<Handle> wh;
    wh.push_back(h.get());
    std::vector<MojoHandleSignals> sigs;
    sigs.push_back(~MOJO_HANDLE_SIGNAL_NONE);
    size_t result_index;
    MojoResult rv = WaitMany(wh.data(), sigs.data(), wh.size(), &result_index);
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, rv);
  }

  // |MakeScopedHandle| (just compilation tests):
  {
    EXPECT_FALSE(MakeScopedHandle(Handle()).is_valid());
    EXPECT_FALSE(MakeScopedHandle(MessagePipeHandle()).is_valid());
    EXPECT_FALSE(MakeScopedHandle(DataPipeProducerHandle()).is_valid());
    EXPECT_FALSE(MakeScopedHandle(DataPipeConsumerHandle()).is_valid());
    EXPECT_FALSE(MakeScopedHandle(SharedBufferHandle()).is_valid());
  }

  // |MessagePipeHandle|/|ScopedMessagePipeHandle| functions:
  {
    MessagePipeHandle h_invalid;
    EXPECT_FALSE(h_invalid.is_valid());
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              WriteMessageRaw(h_invalid, nullptr, 0, nullptr, 0,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));
    char buffer[10] = {0};
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              WriteMessageRaw(h_invalid, buffer, sizeof(buffer), nullptr, 0,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
              ReadMessageRaw(h_invalid, nullptr, nullptr,
                             MOJO_READ_MESSAGE_FLAG_NONE));

    // Basic tests of waiting and closing.
    {
      ScopedMessagePipeHandle h0;
      ScopedMessagePipeHandle h1;
      EXPECT_FALSE(h0.get().is_valid());
      EXPECT_FALSE(h1.get().is_valid());

      CreateMessagePipe(nullptr, &h0, &h1);
      EXPECT_TRUE(h0.get().is_valid());
      EXPECT_TRUE(h1.get().is_valid());
      EXPECT_NE(h0.get().value(), h1.get().value());
      MojoHandleSignalsState state = h0->QuerySignalsState();

      EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, state.satisfied_signals);
      EXPECT_EQ(kSignalAll, state.satisfiable_signals);

      std::vector<Handle> wh;
      wh.push_back(h0.get());
      wh.push_back(h1.get());
      std::vector<MojoHandleSignals> sigs;
      sigs.push_back(MOJO_HANDLE_SIGNAL_READABLE);
      sigs.push_back(MOJO_HANDLE_SIGNAL_WRITABLE);
      std::vector<MojoHandleSignalsState> states(sigs.size());

      size_t result_index;
      MojoResult rv = WaitMany(wh.data(), sigs.data(), wh.size(), &result_index,
                               states.data());
      EXPECT_EQ(MOJO_RESULT_OK, rv);
      EXPECT_EQ(1u, result_index);
      EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, states[0].satisfied_signals);
      EXPECT_EQ(kSignalAll, states[0].satisfiable_signals);
      EXPECT_EQ(MOJO_HANDLE_SIGNAL_WRITABLE, states[1].satisfied_signals);
      EXPECT_EQ(kSignalAll, states[1].satisfiable_signals);

      // Test closing |h1| explicitly.
      Close(std::move(h1));
      EXPECT_FALSE(h1.get().is_valid());

      EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
                Wait(h0.get(), MOJO_HANDLE_SIGNAL_READABLE, &state));

      EXPECT_EQ(MOJO_HANDLE_SIGNAL_PEER_CLOSED, state.satisfied_signals);
      EXPECT_FALSE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_WRITABLE);
      EXPECT_FALSE(state.satisfiable_signals & MOJO_HANDLE_SIGNAL_READABLE);
    }

    // Actually test writing/reading messages.
    {
      ScopedMessagePipeHandle h0;
      ScopedMessagePipeHandle h1;
      CreateMessagePipe(nullptr, &h0, &h1);

      const char kHello[] = "hello";
      const uint32_t kHelloSize = static_cast<uint32_t>(sizeof(kHello));
      EXPECT_EQ(MOJO_RESULT_OK,
                WriteMessageRaw(h0.get(), kHello, kHelloSize - 1, nullptr, 0,
                                MOJO_WRITE_MESSAGE_FLAG_NONE));

      MojoHandleSignalsState state;
      EXPECT_EQ(MOJO_RESULT_OK,
                Wait(h1.get(), MOJO_HANDLE_SIGNAL_READABLE, &state));
      EXPECT_EQ(kSignalReadableWritable, state.satisfied_signals);
      EXPECT_EQ(kSignalAll, state.satisfiable_signals);

      std::vector<uint8_t> bytes;
      EXPECT_EQ(MOJO_RESULT_OK, ReadMessageRaw(h1.get(), &bytes, nullptr,
                                               MOJO_READ_MESSAGE_FLAG_NONE));
      EXPECT_EQ(kHello, std::string(bytes.begin(), bytes.end()));

      // Send a handle over the previously-establish message pipe. Use the
      // |MessagePipe| wrapper (to test it), which automatically creates a
      // message pipe.
      MessagePipe mp;

      // Write a message to |mp.handle0|, before we send |mp.handle1|.
      const char kWorld[] = "world!";
      const uint32_t kWorldSize = static_cast<uint32_t>(sizeof(kWorld));
      EXPECT_EQ(MOJO_RESULT_OK,
                WriteMessageRaw(mp.handle0.get(), kWorld, kWorldSize - 1,
                                nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE));

      // Send |mp.handle1| over |h1| to |h0|.
      MojoHandle handles[5];
      handles[0] = mp.handle1.release().value();
      EXPECT_NE(kInvalidHandleValue, handles[0]);
      EXPECT_FALSE(mp.handle1.get().is_valid());
      uint32_t handles_count = 1;
      EXPECT_EQ(MOJO_RESULT_OK,
                WriteMessageRaw(h1.get(), kHello, kHelloSize - 1, handles,
                                handles_count, MOJO_WRITE_MESSAGE_FLAG_NONE));

      // Read "hello" and the sent handle.
      EXPECT_EQ(MOJO_RESULT_OK,
                Wait(h0.get(), MOJO_HANDLE_SIGNAL_READABLE, &state));
      EXPECT_EQ(kSignalReadableWritable, state.satisfied_signals);
      EXPECT_EQ(kSignalAll, state.satisfiable_signals);

      std::vector<ScopedHandle> read_handles;
      EXPECT_EQ(MOJO_RESULT_OK, ReadMessageRaw(h0.get(), &bytes, &read_handles,
                                               MOJO_READ_MESSAGE_FLAG_NONE));
      EXPECT_EQ(kHello, std::string(bytes.begin(), bytes.end()));
      EXPECT_EQ(1u, read_handles.size());
      EXPECT_NE(kInvalidHandleValue, read_handles[0]->value());

      // Read from the sent/received handle.
      mp.handle1.reset(MessagePipeHandle(read_handles[0].release().value()));

      EXPECT_EQ(MOJO_RESULT_OK,
                Wait(mp.handle1.get(), MOJO_HANDLE_SIGNAL_READABLE, &state));
      EXPECT_EQ(kSignalReadableWritable, state.satisfied_signals);
      EXPECT_EQ(kSignalAll, state.satisfiable_signals);

      read_handles.clear();
      EXPECT_EQ(MOJO_RESULT_OK,
                ReadMessageRaw(mp.handle1.get(), &bytes, &read_handles,
                               MOJO_READ_MESSAGE_FLAG_NONE));
      EXPECT_EQ(kWorld, std::string(bytes.begin(), bytes.end()));
      EXPECT_TRUE(read_handles.empty());
    }
  }
}

TEST(CoreCppTest, TearDownWithMessagesEnqueued) {
  // Tear down a message pipe which still has a message enqueued, with the
  // message also having a valid message pipe handle.
  {
    ScopedMessagePipeHandle h0;
    ScopedMessagePipeHandle h1;
    CreateMessagePipe(nullptr, &h0, &h1);

    // Send a handle over the previously-establish message pipe.
    ScopedMessagePipeHandle h2;
    ScopedMessagePipeHandle h3;
    if (CreateMessagePipe(nullptr, &h2, &h3) != MOJO_RESULT_OK)
      CreateMessagePipe(nullptr, &h2, &h3);  // Must be old EDK.

    // Write a message to |h2|, before we send |h3|.
    const char kWorld[] = "world!";
    const uint32_t kWorldSize = static_cast<uint32_t>(sizeof(kWorld));
    EXPECT_EQ(MOJO_RESULT_OK,
              WriteMessageRaw(h2.get(), kWorld, kWorldSize, nullptr, 0,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));
    // And also a message to |h3|.
    EXPECT_EQ(MOJO_RESULT_OK,
              WriteMessageRaw(h3.get(), kWorld, kWorldSize, nullptr, 0,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));

    // Send |h3| over |h1| to |h0|.
    const char kHello[] = "hello";
    const uint32_t kHelloSize = static_cast<uint32_t>(sizeof(kHello));
    MojoHandle h3_value;
    h3_value = h3.release().value();
    EXPECT_NE(kInvalidHandleValue, h3_value);
    EXPECT_FALSE(h3.get().is_valid());
    EXPECT_EQ(MOJO_RESULT_OK,
              WriteMessageRaw(h1.get(), kHello, kHelloSize, &h3_value, 1,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));

    EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h0.release().value()));
    EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h1.release().value()));
    EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h2.release().value()));
  }

  // Do this in a different order: make the enqueued message pipe handle only
  // half-alive.
  {
    ScopedMessagePipeHandle h0;
    ScopedMessagePipeHandle h1;
    CreateMessagePipe(nullptr, &h0, &h1);

    // Send a handle over the previously-establish message pipe.
    ScopedMessagePipeHandle h2;
    ScopedMessagePipeHandle h3;
    if (CreateMessagePipe(nullptr, &h2, &h3) != MOJO_RESULT_OK)
      CreateMessagePipe(nullptr, &h2, &h3);  // Must be old EDK.

    // Write a message to |h2|, before we send |h3|.
    const char kWorld[] = "world!";
    const uint32_t kWorldSize = static_cast<uint32_t>(sizeof(kWorld));
    EXPECT_EQ(MOJO_RESULT_OK,
              WriteMessageRaw(h2.get(), kWorld, kWorldSize, nullptr, 0,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));
    // And also a message to |h3|.
    EXPECT_EQ(MOJO_RESULT_OK,
              WriteMessageRaw(h3.get(), kWorld, kWorldSize, nullptr, 0,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));

    // Send |h3| over |h1| to |h0|.
    const char kHello[] = "hello";
    const uint32_t kHelloSize = static_cast<uint32_t>(sizeof(kHello));
    MojoHandle h3_value;
    h3_value = h3.release().value();
    EXPECT_NE(kInvalidHandleValue, h3_value);
    EXPECT_FALSE(h3.get().is_valid());
    EXPECT_EQ(MOJO_RESULT_OK,
              WriteMessageRaw(h1.get(), kHello, kHelloSize, &h3_value, 1,
                              MOJO_WRITE_MESSAGE_FLAG_NONE));

    EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h2.release().value()));
    EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h0.release().value()));
    EXPECT_EQ(MOJO_RESULT_OK, MojoClose(h1.release().value()));
  }
}

TEST(CoreCppTest, ScopedHandleMoveCtor) {
  ScopedSharedBufferHandle buffer1 = SharedBufferHandle::Create(1024);
  EXPECT_TRUE(buffer1.is_valid());

  ScopedSharedBufferHandle buffer2 = SharedBufferHandle::Create(1024);
  EXPECT_TRUE(buffer2.is_valid());

  // If this fails to close buffer1, ScopedHandleBase::CloseIfNecessary() will
  // assert.
  buffer1 = std::move(buffer2);

  EXPECT_TRUE(buffer1.is_valid());
  EXPECT_FALSE(buffer2.is_valid());
}

TEST(CoreCppTest, BasicSharedBuffer) {
  ScopedSharedBufferHandle h0 = SharedBufferHandle::Create(100);
  ASSERT_TRUE(h0.is_valid());
  EXPECT_GE(h0->GetSize(), 100U);

  // Map everything.
  ScopedSharedBufferMapping mapping = h0->Map(100);
  ASSERT_TRUE(mapping);
  static_cast<char*>(mapping.get())[50] = 'x';

  // Duplicate |h0| to |h1|.
  ScopedSharedBufferHandle h1 =
      h0->Clone(SharedBufferHandle::AccessMode::READ_ONLY);
  ASSERT_TRUE(h1.is_valid());

  // Close |h0|.
  h0.reset();

  // The mapping should still be good.
  static_cast<char*>(mapping.get())[51] = 'y';

  // Unmap it.
  mapping.reset();

  // Map half of |h1|.
  mapping = h1->MapAtOffset(50, 50);
  ASSERT_TRUE(mapping);

  // It should have what we wrote.
  EXPECT_EQ('x', static_cast<char*>(mapping.get())[0]);
  EXPECT_EQ('y', static_cast<char*>(mapping.get())[1]);

  // Unmap it.
  mapping.reset();
  h1.reset();

  // Creating a 1 EB shared buffer should fail without crashing.
  EXPECT_FALSE(SharedBufferHandle::Create(1ULL << 60).is_valid());
}

// TODO(vtl): Write data pipe tests.

}  // namespace
}  // namespace mojo
