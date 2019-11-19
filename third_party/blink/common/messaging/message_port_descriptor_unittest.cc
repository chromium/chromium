// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/message_port_descriptor.h"

#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class LenientMockInstrumentationDelegate
    : public MessagePortDescriptor::InstrumentationDelegate {
 public:
  LenientMockInstrumentationDelegate() {
    MessagePortDescriptor::SetInstrumentationDelegate(this);
  }

  ~LenientMockInstrumentationDelegate() override {
    MessagePortDescriptor::SetInstrumentationDelegate(nullptr);
  }

  MOCK_METHOD1(NotifyMessagePortPairCreated,
               void(const MessagePortDescriptorPair& pair));

  MOCK_METHOD3(NotifyMessagePortAttached,
               void(const base::UnguessableToken& port_id,
                    uint64_t sequence_number,
                    const base::UnguessableToken& execution_context_id));

  MOCK_METHOD2(NotifyMessagePortDetached,
               void(const base::UnguessableToken& port_id,
                    uint64_t sequence_number));

  MOCK_METHOD2(NotifyMessagePortDestroyed,
               void(const base::UnguessableToken& port_id,
                    uint64_t sequence_number));
};

using MockInstrumentationDelegate =
    testing::StrictMock<LenientMockInstrumentationDelegate>;

using testing::_;
using testing::Invoke;

}  // namespace

class MessagePortDescriptorTestHelper {
 public:
  static void Init(MessagePortDescriptor* port,
                   mojo::ScopedMessagePipeHandle handle,
                   base::UnguessableToken id,
                   uint64_t sequence_number) {
    return port->Init(std::move(handle), id, sequence_number);
  }

  static mojo::ScopedMessagePipeHandle TakeHandle(MessagePortDescriptor* port) {
    return port->TakeHandle();
  }

  static base::UnguessableToken TakeId(MessagePortDescriptor* port) {
    return port->TakeId();
  }

  static uint64_t TakeSequenceNumber(MessagePortDescriptor* port) {
    return port->TakeSequenceNumber();
  }

  static mojo::ScopedMessagePipeHandle TakeHandleToEntangle(
      MessagePortDescriptor* port,
      const base::UnguessableToken& execution_context_id) {
    return port->TakeHandleToEntangle(execution_context_id);
  }

  static void GiveDisentangledHandle(MessagePortDescriptor* port,
                                     mojo::ScopedMessagePipeHandle handle) {
    return port->GiveDisentangledHandle(std::move(handle));
  }
};

TEST(MessagePortDescriptorTest, InstrumentationAndSerializationWorks) {
  MockInstrumentationDelegate delegate;

  // A small struct for holding information gleaned about ports during their
  // creation event. Allows verifying that other events are appropriately
  // sequenced.
  struct {
    base::UnguessableToken token0;
    base::UnguessableToken token1;
    uint64_t seq0 = 1;
    uint64_t seq1 = 1;
  } created_data;

  // Create a message handle descriptor pair and expect a notification.
  EXPECT_CALL(delegate, NotifyMessagePortPairCreated(_))
      .WillOnce(Invoke([&created_data](const MessagePortDescriptorPair& pair) {
        created_data.token0 = pair.port0().id();
        created_data.token1 = pair.port1().id();
        EXPECT_EQ(1u, pair.port0().sequence_number());
        EXPECT_EQ(1u, pair.port1().sequence_number());
      }));
  MessagePortDescriptorPair pair;

  MessagePortDescriptor port0;
  MessagePortDescriptor port1;
  EXPECT_FALSE(port0.IsValid());
  EXPECT_FALSE(port1.IsValid());
  EXPECT_FALSE(port0.IsEntangled());
  EXPECT_FALSE(port1.IsEntangled());
  EXPECT_TRUE(port0.IsDefault());
  EXPECT_TRUE(port1.IsDefault());
  port0 = pair.TakePort0();
  port1 = pair.TakePort1();
  EXPECT_TRUE(port0.IsValid());
  EXPECT_TRUE(port1.IsValid());
  EXPECT_FALSE(port0.IsEntangled());
  EXPECT_FALSE(port1.IsEntangled());
  EXPECT_FALSE(port0.IsDefault());
  EXPECT_FALSE(port1.IsDefault());

  // Expect that the data received at creation matches the actual ports.
  EXPECT_EQ(created_data.token0, port0.id());
  EXPECT_EQ(created_data.seq0, port0.sequence_number());
  EXPECT_EQ(created_data.token1, port1.id());
  EXPECT_EQ(created_data.seq1, port1.sequence_number());

  // Simulate that a handle is attached by taking the pipe handle.
  base::UnguessableToken dummy_ec = base::UnguessableToken::Create();
  EXPECT_CALL(delegate,
              NotifyMessagePortAttached(created_data.token0,
                                        created_data.seq0++, dummy_ec));
  auto handle0 =
      MessagePortDescriptorTestHelper::TakeHandleToEntangle(&port0, dummy_ec);
  EXPECT_TRUE(port0.IsValid());
  EXPECT_TRUE(port0.IsEntangled());
  EXPECT_FALSE(port0.IsDefault());

  // Simulate that the handle is detached by giving the pipe handle back.
  EXPECT_CALL(delegate, NotifyMessagePortDetached(created_data.token0,
                                                  created_data.seq0++));
  MessagePortDescriptorTestHelper::GiveDisentangledHandle(&port0,
                                                          std::move(handle0));
  EXPECT_TRUE(port0.IsValid());
  EXPECT_FALSE(port0.IsEntangled());
  EXPECT_FALSE(port0.IsDefault());

  // Tear down a handle explicitly.
  EXPECT_CALL(delegate, NotifyMessagePortDestroyed(created_data.token1,
                                                   created_data.seq1++));
  port1.Reset();

  // And leave the other handle to be torn down in the destructor.
  EXPECT_CALL(delegate, NotifyMessagePortDestroyed(created_data.token0,
                                                   created_data.seq0++));
}

TEST(MessagePortDescriptorTest, InvalidUsageDeathTest) {
  static MessagePortDescriptor::InstrumentationDelegate* kDummyDelegate1 =
      reinterpret_cast<MessagePortDescriptor::InstrumentationDelegate*>(
          0xBAADF00D);
  static MessagePortDescriptor::InstrumentationDelegate* kDummyDelegate2 =
      reinterpret_cast<MessagePortDescriptor::InstrumentationDelegate*>(
          0xDEADBEEF);
  EXPECT_DCHECK_DEATH(
      MessagePortDescriptor::SetInstrumentationDelegate(nullptr));
  MessagePortDescriptor::SetInstrumentationDelegate(kDummyDelegate1);
  // Setting the same or another delegate should explode.
  EXPECT_DCHECK_DEATH(
      MessagePortDescriptor::SetInstrumentationDelegate(kDummyDelegate1));
  EXPECT_DCHECK_DEATH(
      MessagePortDescriptor::SetInstrumentationDelegate(kDummyDelegate2));
  // Unset the dummy delegate we installed so we don't receive notifications in
  // the rest of the test.
  MessagePortDescriptor::SetInstrumentationDelegate(nullptr);

  // Trying to take properties of a default port descriptor should explode.
  MessagePortDescriptor port0;
  EXPECT_DCHECK_DEATH(MessagePortDescriptorTestHelper::TakeHandle(&port0));
  EXPECT_DCHECK_DEATH(MessagePortDescriptorTestHelper::TakeId(&port0));
  EXPECT_DCHECK_DEATH(
      MessagePortDescriptorTestHelper::TakeSequenceNumber(&port0));

  MessagePortDescriptorPair pair;
  port0 = pair.TakePort0();
  MessagePortDescriptor port1 = pair.TakePort1();

  {
    // Dismantle the port as if for serialization.
    auto handle = MessagePortDescriptorTestHelper::TakeHandle(&port0);
    auto id = MessagePortDescriptorTestHelper::TakeId(&port0);
    auto sequence_number =
        MessagePortDescriptorTestHelper::TakeSequenceNumber(&port0);

    // Reserializing with inconsistent state should explode.

    // First try with any 1 of the 3 fields being invalid.
    EXPECT_DCHECK_DEATH(MessagePortDescriptorTestHelper::Init(
        &port0, mojo::ScopedMessagePipeHandle(), id, sequence_number));
    EXPECT_DCHECK_DEATH(MessagePortDescriptorTestHelper::Init(
        &port0, std::move(handle), base::UnguessableToken::Null(),
        sequence_number));
    EXPECT_DCHECK_DEATH(MessagePortDescriptorTestHelper::Init(
        &port0, std::move(handle), id, 0));

    // Next try with any 2 of the 3 fields being invalid.
    EXPECT_DCHECK_DEATH(MessagePortDescriptorTestHelper::Init(
        &port0, std::move(handle), base::UnguessableToken::Null(), 0));
    EXPECT_DCHECK_DEATH(MessagePortDescriptorTestHelper::Init(
        &port0, mojo::ScopedMessagePipeHandle(), id, 0));
    EXPECT_DCHECK_DEATH(MessagePortDescriptorTestHelper::Init(
        &port0, mojo::ScopedMessagePipeHandle(), base::UnguessableToken::Null(),
        sequence_number));

    // Restoring the port with default state should work (all 3 fields invalid).
    MessagePortDescriptorTestHelper::Init(&port0,
                                          mojo::ScopedMessagePipeHandle(),
                                          base::UnguessableToken::Null(), 0);
    EXPECT_TRUE(port0.IsDefault());

    // Restoring the port with full state should work (all 3 fields valid).
    MessagePortDescriptorTestHelper::Init(&port0, std::move(handle), id,
                                          sequence_number);
  }

  // Entangle the port.
  base::UnguessableToken dummy_ec = base::UnguessableToken::Create();
  auto handle0 =
      MessagePortDescriptorTestHelper::TakeHandleToEntangle(&port0, dummy_ec);

  // Trying to entangle a second time should explode.
  EXPECT_DCHECK_DEATH(
      MessagePortDescriptorTestHelper::TakeHandleToEntangle(&port0, dummy_ec));

  // Destroying a port descriptor that has been entangled should explode. The
  // handle needs to be given back to the descriptor before its death, ensuring
  // descriptors remain fully accounted for over their entire lifecycle.
  EXPECT_DCHECK_DEATH(port0.Reset());

  // Trying to assign while the handle is entangled should explode, as it
  // amounts to destroying the existing descriptor.
  EXPECT_DCHECK_DEATH(port0 = MessagePortDescriptor());

  // Trying to disentangle with an empty port should explode.
  mojo::ScopedMessagePipeHandle handle1;
  EXPECT_DCHECK_DEATH(MessagePortDescriptorTestHelper::GiveDisentangledHandle(
      &port0, std::move(handle1)));

  // Trying to disentangle with the wrong port should explode.
  handle1 =
      MessagePortDescriptorTestHelper::TakeHandleToEntangle(&port1, dummy_ec);
  EXPECT_DCHECK_DEATH(MessagePortDescriptorTestHelper::GiveDisentangledHandle(
      &port0, std::move(handle1)));

  // Disentangle the ports properly.
  MessagePortDescriptorTestHelper::GiveDisentangledHandle(&port0,
                                                          std::move(handle0));
  MessagePortDescriptorTestHelper::GiveDisentangledHandle(&port1,
                                                          std::move(handle1));
}

}  // namespace blink
