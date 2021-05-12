// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <set>

#include "base/run_loop.h"
#include "ipc/ipc_channel_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace IPC {
namespace internal {

namespace {

class MockChannelReader : public ChannelReader {
 public:
  MockChannelReader()
      : ChannelReader(nullptr), last_dispatched_message_(nullptr) {}

  ReadState ReadData(char* buffer, int buffer_len, int* bytes_read) override {
    if (data_.empty())
      return READ_PENDING;

    size_t read_len = std::min(static_cast<size_t>(buffer_len), data_.size());
    memcpy(buffer, data_.data(), read_len);
    *bytes_read = static_cast<int>(read_len);
    data_.erase(0, read_len);
    return READ_SUCCEEDED;
  }

  bool ShouldDispatchInputMessage(Message* msg) override { return true; }

  bool GetAttachments(Message* msg) override { return true; }

  bool DidEmptyInputBuffers() override { return true; }

  void HandleInternalMessage(const Message& msg) override {}

  void DispatchMessage(Message* m) override { last_dispatched_message_ = m; }

  Message* get_last_dispatched_message() { return last_dispatched_message_; }

  void AppendData(const void* data, size_t size) {
    data_.append(static_cast<const char*>(data), size);
  }

  void AppendMessageData(const Message& message) {
    AppendData(message.data(), message.size());
  }

 private:
  Message* last_dispatched_message_;
  std::string data_;
};

class ExposedMessage: public Message {
 public:
  using Message::Header;
  using Message::header;
};

// Payload that makes messages large
const size_t LargePayloadSize = Channel::kMaximumReadBufferSize * 3 / 2;

}  // namespace

// We can determine message size from its header (and hence resize the buffer)
// only when attachment broker is not used, see IPC::Message::FindNext().

TEST(ChannelReaderTest, ResizeOverflowBuffer) {
  MockChannelReader reader;

  ExposedMessage::Header header = {};

  header.payload_size = 128 * 1024;
  EXPECT_LT(reader.input_overflow_buf_.capacity(), header.payload_size);
  EXPECT_TRUE(reader.TranslateInputData(
      reinterpret_cast<const char*>(&header), sizeof(header)));

  // Once message header is available we resize overflow buffer to
  // fit the entire message.
  EXPECT_GE(reader.input_overflow_buf_.capacity(), header.payload_size);
}

TEST(ChannelReaderTest, InvalidMessageSize) {
  MockChannelReader reader;

  ExposedMessage::Header header = {};

  size_t capacity_before = reader.input_overflow_buf_.capacity();

  // Message is slightly larger than maximum allowed size
  header.payload_size = Channel::kMaximumMessageSize + 1;
  EXPECT_FALSE(reader.TranslateInputData(
      reinterpret_cast<const char*>(&header), sizeof(header)));
  EXPECT_LE(reader.input_overflow_buf_.capacity(), capacity_before);

  // Payload size is negative, overflow is detected by Pickle::PeekNext()
  header.payload_size = static_cast<uint32_t>(-1);
  EXPECT_FALSE(reader.TranslateInputData(
      reinterpret_cast<const char*>(&header), sizeof(header)));
  EXPECT_LE(reader.input_overflow_buf_.capacity(), capacity_before);

  // Payload size is maximum int32_t value
  header.payload_size = std::numeric_limits<int32_t>::max();
  EXPECT_FALSE(reader.TranslateInputData(
      reinterpret_cast<const char*>(&header), sizeof(header)));
  EXPECT_LE(reader.input_overflow_buf_.capacity(), capacity_before);
}

TEST(ChannelReaderTest, TrimBuffer) {
  // ChannelReader uses std::string as a buffer, and calls reserve()
  // to trim it to kMaximumReadBufferSize. However, an implementation
  // is free to actually reserve a larger amount.
  size_t trimmed_buffer_size;
  {
    std::string buf;
    buf.reserve(Channel::kMaximumReadBufferSize);
    trimmed_buffer_size = buf.capacity();
  }

  // Buffer is trimmed after message is processed.
  {
    MockChannelReader reader;

    Message message;
    message.WriteString(std::string(LargePayloadSize, 'X'));

    // Sanity check
    EXPECT_TRUE(message.size() > trimmed_buffer_size);

    // Initially buffer is small
    EXPECT_LE(reader.input_overflow_buf_.capacity(), trimmed_buffer_size);

    // Write and process large message
    reader.AppendMessageData(message);
    EXPECT_EQ(ChannelReader::DISPATCH_FINISHED,
              reader.ProcessIncomingMessages());

    // After processing large message buffer is trimmed
    EXPECT_EQ(reader.input_overflow_buf_.capacity(), trimmed_buffer_size);
  }

  // Buffer is trimmed only after entire message is processed.
  {
    MockChannelReader reader;

    ExposedMessage message;
    message.WriteString(std::string(LargePayloadSize, 'X'));

    // Write and process message header
    reader.AppendData(message.header(), sizeof(ExposedMessage::Header));
    EXPECT_EQ(ChannelReader::DISPATCH_FINISHED,
              reader.ProcessIncomingMessages());

    // We determined message size for the message from its header, so
    // we resized the buffer to fit.
    EXPECT_GE(reader.input_overflow_buf_.capacity(), message.size());

    // Write and process payload
    reader.AppendData(message.payload(), message.payload_size());
    EXPECT_EQ(ChannelReader::DISPATCH_FINISHED,
              reader.ProcessIncomingMessages());

    // But once we process the message, we trim the buffer
    EXPECT_EQ(reader.input_overflow_buf_.capacity(), trimmed_buffer_size);
  }

  // Buffer is not trimmed if the next message is also large.
  {
    MockChannelReader reader;

    // Write large message
    Message message1;
    message1.WriteString(std::string(LargePayloadSize * 2, 'X'));
    reader.AppendMessageData(message1);

    // Write header for the next large message
    ExposedMessage message2;
    message2.WriteString(std::string(LargePayloadSize, 'Y'));
    reader.AppendData(message2.header(), sizeof(ExposedMessage::Header));

    // Process messages
    EXPECT_EQ(ChannelReader::DISPATCH_FINISHED,
              reader.ProcessIncomingMessages());

    // We determined message size for the second (partial) message, so
    // we resized the buffer to fit.
    EXPECT_GE(reader.input_overflow_buf_.capacity(), message1.size());
  }

  // Buffer resized appropriately if next message is larger than the first.
  // (Similar to the test above except for the order of messages.)
  {
    MockChannelReader reader;

    // Write large message
    Message message1;
    message1.WriteString(std::string(LargePayloadSize, 'Y'));
    reader.AppendMessageData(message1);

    // Write header for the next even larger message
    ExposedMessage message2;
    message2.WriteString(std::string(LargePayloadSize * 2, 'X'));
    reader.AppendData(message2.header(), sizeof(ExposedMessage::Header));

    // Process messages
    EXPECT_EQ(ChannelReader::DISPATCH_FINISHED,
              reader.ProcessIncomingMessages());

    // We determined message size for the second (partial) message, and
    // resized the buffer to fit it.
    EXPECT_GE(reader.input_overflow_buf_.capacity(), message2.size());
  }

  // Buffer is not trimmed if we've just resized it to accommodate large
  // incoming message.
  {
    MockChannelReader reader;

    // Write small message
    Message message1;
    message1.WriteString(std::string(11, 'X'));
    reader.AppendMessageData(message1);

    // Write header for the next large message
    ExposedMessage message2;
    message2.WriteString(std::string(LargePayloadSize, 'Y'));
    reader.AppendData(message2.header(), sizeof(ExposedMessage::Header));

    EXPECT_EQ(ChannelReader::DISPATCH_FINISHED,
              reader.ProcessIncomingMessages());

    // We determined message size for the second (partial) message, so
    // we resized the buffer to fit.
    EXPECT_GE(reader.input_overflow_buf_.capacity(), message2.size());
  }
}

}  // namespace internal
}  // namespace IPC
