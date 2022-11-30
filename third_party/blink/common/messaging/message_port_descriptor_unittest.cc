// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/message_port_descriptor.h"

#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

ExecutionContext* kDummyEc = reinterpret_cast<ExecutionContext*>(0xBAADF00D);

class LenientMockInstrumentationDelegate
    : public MessagePortDescriptor::InstrumentationDelegate {
 public:
  LenientMockInstrumentationDelegate() {
    MessagePortDescriptor::SetInstrumentationDelegate(this);
  }

  ~LenientMockInstrumentationDelegate() override {
    MessagePortDescriptor::SetInstrumentationDelegate(nullptr);
  }

  MOCK_METHOD2(NotifyMessagePortPairCreated,
               void(const base::UnguessableToken& port0_id,
                    const base::UnguessableToken& port1_id));

  MOCK_METHOD3(NotifyMessagePortAttached,
               void(const base::UnguessableToken& port_id,
                    uint64_t sequence_number,
                    ExecutionContext* execution_context));

  MOCK_METHOD2(NotifyMessagePortAttachedToEmbedder,
               void(const base::UnguessableToken& port_id,
                    uint64_t sequence_number));

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
  EXPECT_CALL(delegate, NotifyMessagePortPairCreated(_, _))
      .WillOnce(Invoke([&created_data](const base::UnguessableToken& port0_id,
                                       const base::UnguessableToken& port1_id) {
        created_data.token0 = port0_id;
        created_data.token1 = port1_id;
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
  EXPECT_CALL(delegate,
              NotifyMessagePortAttached(created_data.token0,
                                        created_data.seq0++, kDummyEc));
  auto handle0 = port0.TakeHandleToEntangle(kDummyEc);
  EXPECT_TRUE(port0.IsValid());
  EXPECT_TRUE(port0.IsEntangled());
  EXPECT_FALSE(port0.IsDefault());

  // Simulate that the handle is detached by giving the pipe handle back.
  EXPECT_CALL(delegate, NotifyMessagePortDetached(created_data.token0,
                                                  created_data.seq0++));
  port0.GiveDisentangledHandle(std::move(handle0));
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

TEST(MessagePortDescriptorTestDeathTest, InvalidUsageInstrumentationDelegate) {
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
}

TEST(MessagePortDescriptorTestDeathTest, InvalidUsageForSerialization) {
  // Trying to take properties of a default port descriptor should explode.
  MessagePortDescriptor port0;
  EXPECT_DCHECK_DEATH(port0.TakeHandleForSerialization());
  EXPECT_DCHECK_DEATH(port0.TakeIdForSerialization());
  EXPECT_DCHECK_DEATH(port0.TakeSequenceNumberForSerialization());

  MessagePortDescriptorPair pair;
  port0 = pair.TakePort0();
  MessagePortDescriptor port1 = pair.TakePort1();

  {
    // Dismantle the port as if for serialization. Trying to take fields a
    // second time should explode. A partially serialized object should also
    // explode if
    auto handle = port0.TakeHandleForSerialization();
    EXPECT_DCHECK_DEATH(port0.TakeHandleForSerialization());
    EXPECT_DCHECK_DEATH(port0.Reset());
    auto id = port0.TakeIdForSerialization();
    EXPECT_DCHECK_DEATH(port0.TakeIdForSerialization());
    EXPECT_DCHECK_DEATH(port0.Reset());
    auto sequence_number = port0.TakeSequenceNumberForSerialization();
    EXPECT_DCHECK_DEATH(port0.TakeSequenceNumberForSerialization());

    // This time reset should *not* explode, as the object has been fully taken
    // for serialization.
    port0.Reset();

    // Reserializing with inconsistent state should explode.

    // First try with any 1 of the 3 fields being invalid.
    EXPECT_DCHECK_DEATH(port0.InitializeFromSerializedValues(
        mojo::ScopedMessagePipeHandle(), id, sequence_number));
    EXPECT_DCHECK_DEATH(port0.InitializeFromSerializedValues(
        std::move(handle), base::UnguessableToken::Null(), sequence_number));
    EXPECT_DCHECK_DEATH(
        port0.InitializeFromSerializedValues(std::move(handle), id, 0));

    // Next try with any 2 of the 3 fields being invalid.
    EXPECT_DCHECK_DEATH(port0.InitializeFromSerializedValues(
        std::move(handle), base::UnguessableToken::Null(), 0));
    EXPECT_DCHECK_DEATH(port0.InitializeFromSerializedValues(
        mojo::ScopedMessagePipeHandle(), id, 0));
    EXPECT_DCHECK_DEATH(port0.InitializeFromSerializedValues(
        mojo::ScopedMessagePipeHandle(), base::UnguessableToken::Null(),
        sequence_number));

    // Restoring the port with default state should work (all 3 fields invalid).
    port0.InitializeFromSerializedValues(mojo::ScopedMessagePipeHandle(),
                                         base::UnguessableToken::Null(), 0);
    EXPECT_TRUE(port0.IsDefault());

    // Restoring the port with full state should work (all 3 fields valid).
    port0.InitializeFromSerializedValues(std::move(handle), id,
                                         sequence_number);
  }
}

TEST(MessagePortDescriptorTestDeathTest, InvalidUsageForEntangling) {
  MessagePortDescriptorPair pair;
  MessagePortDescriptor port0 = pair.TakePort0();
  MessagePortDescriptor port1 = pair.TakePort1();

  // Entangle the port.
  auto handle0 = port0.TakeHandleToEntangleWithEmbedder();

  // Trying to entangle a second time should explode.
  EXPECT_DCHECK_DEATH(port0.TakeHandleToEntangleWithEmbedder());
  EXPECT_DCHECK_DEATH(port0.TakeHandleToEntangle(kDummyEc));

  // Destroying a port descriptor that has been entangled should explode. The
  // handle needs to be given back to the descriptor before its death, ensuring
  // descriptors remain fully accounted for over their entire lifecycle.
  EXPECT_DCHECK_DEATH(port0.Reset());

  // Trying to assign while the handle is entangled should explode, as it
  // amounts to destroying the existing descriptor.
  EXPECT_DCHECK_DEATH(port0 = MessagePortDescriptor());

  // Trying to reset an entangled port should explode.
  EXPECT_DCHECK_DEATH(port0.Reset());

  // Trying to serialize an entangled port should explode.
  EXPECT_DCHECK_DEATH(port0.TakeHandleForSerialization());
  EXPECT_DCHECK_DEATH(port0.TakeIdForSerialization());
  EXPECT_DCHECK_DEATH(port0.TakeSequenceNumberForSerialization());

  // Disentangle the port so it doesn't explode at teardown.
  port0.GiveDisentangledHandle(std::move(handle0));
}

}  // namespace blink
