// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/message_decoder.h"

#include <stdint.h>

#include <list>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "remoting/proto/event.pb.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/message_serialization.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::protocol {

static const unsigned int kTestKey = 142;

static void AppendMessage(const EventMessage& msg, std::string* buffer) {
  // Contains one encoded message.
  scoped_refptr<net::IOBufferWithSize> encoded_msg;
  encoded_msg = SerializeAndFrameMessage(msg);
  buffer->append(encoded_msg->data(), encoded_msg->size());
}

// Construct and prepare data in the |output_stream|.
static base::HeapArray<uint8_t> PrepareData(int* size) {
  // Contains all encoded messages.
  std::string encoded_data;

  // Then append 10 update sequences to the data.
  for (int i = 0; i < 10; ++i) {
    EventMessage msg;
    msg.set_timestamp(i);
    msg.mutable_key_event()->set_usb_keycode(kTestKey + i);
    msg.mutable_key_event()->set_pressed((i % 2) != 0);
    AppendMessage(msg, &encoded_data);
  }

  *size = encoded_data.length();
  auto buffer = base::HeapArray<uint8_t>::Uninit(*size);
  UNSAFE_TODO(memcpy(buffer.data(), encoded_data.c_str(), *size));
  return buffer;
}

void SimulateReadSequence(base::span<const int> read_sequence) {
  // Prepare encoded data for testing.
  int size;
  auto test_data = PrepareData(&size);

  // Then simulate using MessageDecoder to decode variable
  // size of encoded data.
  // The first thing to do is to generate a variable size of data. This is done
  // by iterating the following array for read sizes.
  MessageDecoder decoder;

  // Then feed the protocol decoder using the above generated data and the
  // read pattern.
  std::list<std::unique_ptr<EventMessage>> message_list;
  for (int pos = 0; pos < size;) {
    SCOPED_TRACE("Input position: " + base::NumberToString(pos));

    // First generate the amount to feed the decoder.
    int read = std::min(size - pos, read_sequence[pos % read_sequence.size()]);

    // And then prepare an IOBuffer for feeding it.
    auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(read);
    UNSAFE_TODO(memcpy(buffer->data(), test_data.data() + pos, read));
    decoder.AddData(buffer, read);
    while (true) {
      std::unique_ptr<CompoundBuffer> message(decoder.GetNextMessage());
      if (!message.get()) {
        break;
      }

      std::unique_ptr<EventMessage> event = std::make_unique<EventMessage>();
      CompoundBufferInputStream stream(message.get());
      ASSERT_TRUE(event->ParseFromZeroCopyStream(&stream));
      message_list.push_back(std::move(event));
    }
    pos += read;
  }

  // Then verify the decoded messages.
  EXPECT_EQ(10u, message_list.size());

  unsigned int index = 0;
  for (const auto& message : message_list) {
    SCOPED_TRACE("Message " + base::NumberToString(index));

    // Partial update stream.
    EXPECT_TRUE(message->has_key_event());

    // TODO(sergeyu): Don't use index here. Instead store the expected values
    // in an array.
    EXPECT_EQ(kTestKey + index, message->key_event().usb_keycode());
    EXPECT_EQ((index % 2) != 0, message->key_event().pressed());
    ++index;
  }
}

TEST(MessageDecoderTest, SmallReads) {
  const int kReads[] = {1, 2, 3, 1};
  SimulateReadSequence(kReads);
}

TEST(MessageDecoderTest, LargeReads) {
  const int kReads[] = {50, 50, 5};
  SimulateReadSequence(kReads);
}

TEST(MessageDecoderTest, EmptyReads) {
  const int kReads[] = {4, 0, 50, 0};
  SimulateReadSequence(kReads);
}

}  // namespace remoting::protocol
