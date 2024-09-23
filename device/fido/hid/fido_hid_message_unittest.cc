// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/hid/fido_hid_message.h"

#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "device/fido/fido_constants.h"
#include "device/fido/hid/fido_hid_packet.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

static const size_t kDefaultInitDataSize =
    kHidMaxPacketSize - kHidInitPacketHeaderSize;
static const size_t kDefaultContinuationDataSize =
    kHidMaxPacketSize - kHidContinuationPacketHeaderSize;

/*
 * U2f Init Packets are of the format:
 * Byte 0:    0
 * Byte 1-4:  Channel ID
 * Byte 5:    Command byte
 * Byte 6-7:  Big Endian size of data
 * Byte 8-n:  Data block
 *
 * Remaining buffer is padded with 0
 */
TEST(FidoHidMessageTest, TestPacketData) {
  uint32_t channel_id = 0xF5060708;
  std::vector<uint8_t> data{10, 11};
  FidoHidDeviceCommand cmd = FidoHidDeviceCommand::kWink;
  auto init_packet =
      std::make_unique<FidoHidInitPacket>(channel_id, cmd, data, data.size());
  size_t index = 0;

  std::vector<uint8_t> serialized = init_packet->GetSerializedData();
  EXPECT_EQ((channel_id >> 24) & 0xff, serialized[index++]);
  EXPECT_EQ((channel_id >> 16) & 0xff, serialized[index++]);
  EXPECT_EQ((channel_id >> 8) & 0xff, serialized[index++]);
  EXPECT_EQ(channel_id & 0xff, serialized[index++]);
  EXPECT_EQ(base::strict_cast<uint8_t>(cmd), serialized[index++] & 0x7f);

  EXPECT_EQ(data.size() >> 8, serialized[index++]);
  EXPECT_EQ(data.size() & 0xff, serialized[index++]);
  EXPECT_EQ(data[0], serialized[index++]);
  EXPECT_EQ(data[1], serialized[index++]);
  for (; index < serialized.size(); index++)
    EXPECT_EQ(0, serialized[index]) << "mismatch at index " << index;
}

TEST(FidoHidMessageTest, TestPacketConstructors) {
  uint32_t channel_id = 0x05060708;
  std::vector<uint8_t> data{10, 11};
  FidoHidDeviceCommand cmd = FidoHidDeviceCommand::kWink;
  auto orig_packet =
      std::make_unique<FidoHidInitPacket>(channel_id, cmd, data, data.size());

  size_t payload_length = static_cast<size_t>(orig_packet->payload_length());
  std::vector<uint8_t> orig_data = orig_packet->GetSerializedData();

  auto reconstructed_packet =
      FidoHidInitPacket::CreateFromSerializedData(orig_data, &payload_length);
  EXPECT_EQ(orig_packet->command(), reconstructed_packet->command());
  EXPECT_EQ(orig_packet->payload_length(),
            reconstructed_packet->payload_length());
  EXPECT_THAT(orig_packet->GetPacketPayload(),
              ::testing::ContainerEq(reconstructed_packet->GetPacketPayload()));

  EXPECT_EQ(channel_id, reconstructed_packet->channel_id());

  ASSERT_EQ(orig_packet->GetSerializedData().size(),
            reconstructed_packet->GetSerializedData().size());
  for (size_t index = 0; index < orig_packet->GetSerializedData().size();
       ++index) {
    EXPECT_EQ(orig_packet->GetSerializedData()[index],
              reconstructed_packet->GetSerializedData()[index])
        << "mismatch at index " << index;
  }
}

TEST(FidoHidMessageTest, TestMaxLengthPacketConstructors) {
  uint32_t channel_id = 0xAAABACAD;
  std::vector<uint8_t> data;
  for (size_t i = 0; i < kHidMaxMessageSize; ++i)
    data.push_back(static_cast<uint8_t>(i % 0xff));

  for (size_t report_size = kHidInitPacketHeaderSize + 1;
       report_size <= kHidMaxPacketSize; report_size++) {
    auto orig_msg = FidoHidMessage::Create(
        channel_id, FidoHidDeviceCommand::kMsg, report_size, data);
    ASSERT_TRUE(orig_msg);

    const auto& original_msg_packets = orig_msg->GetPacketsForTesting();
    auto it = original_msg_packets.begin();
    auto msg_data = (*it)->GetSerializedData();
    auto new_msg = FidoHidMessage::CreateFromSerializedData(msg_data);
    it++;

    for (; it != original_msg_packets.end(); ++it) {
      msg_data = (*it)->GetSerializedData();
      new_msg->AddContinuationPacket(msg_data);
    }

    EXPECT_EQ(new_msg->NumPackets(), orig_msg->NumPackets());

    auto orig_it = original_msg_packets.begin();
    const auto& new_msg_packets = new_msg->GetPacketsForTesting();
    auto new_msg_it = new_msg_packets.begin();

    for (; orig_it != original_msg_packets.end() &&
           new_msg_it != new_msg_packets.end();
         ++orig_it, ++new_msg_it) {
      EXPECT_THAT((*orig_it)->GetPacketPayload(),
                  ::testing::ContainerEq((*new_msg_it)->GetPacketPayload()));

      EXPECT_EQ((*orig_it)->channel_id(), (*new_msg_it)->channel_id());

      ASSERT_EQ((*orig_it)->GetSerializedData().size(),
                (*new_msg_it)->GetSerializedData().size());
      for (size_t index = 0; index < (*orig_it)->GetSerializedData().size();
           ++index) {
        EXPECT_EQ((*orig_it)->GetSerializedData()[index],
                  (*new_msg_it)->GetSerializedData()[index])
            << "mismatch at index " << index;
      }
    }

    EXPECT_TRUE(orig_it == original_msg_packets.end());
    EXPECT_TRUE(new_msg_it == new_msg_packets.end());
  }
}

TEST(FidoHidMessageTest, TestMessagePartitoning) {
  uint32_t channel_id = 0x01010203;
  std::vector<uint8_t> data(kDefaultInitDataSize + 1);
  auto two_packet_message = FidoHidMessage::Create(
      channel_id, FidoHidDeviceCommand::kPing, kHidMaxPacketSize, data);
  ASSERT_TRUE(two_packet_message);
  EXPECT_EQ(2U, two_packet_message->NumPackets());

  data.resize(kDefaultInitDataSize);
  auto one_packet_message = FidoHidMessage::Create(
      channel_id, FidoHidDeviceCommand::kPing, kHidMaxPacketSize, data);
  ASSERT_TRUE(one_packet_message);
  EXPECT_EQ(1U, one_packet_message->NumPackets());

  data.resize(kDefaultInitDataSize + kDefaultContinuationDataSize + 1);
  auto three_packet_message = FidoHidMessage::Create(
      channel_id, FidoHidDeviceCommand::kPing, kHidMaxPacketSize, data);
  ASSERT_TRUE(three_packet_message);
  EXPECT_EQ(3U, three_packet_message->NumPackets());

  // With the minimal report size, only a single byte of data will fit in an
  // init message, followed by three bytes in each continuation message.
  data.resize(1 + 3 + 3);
  auto three_small_messages =
      FidoHidMessage::Create(channel_id, FidoHidDeviceCommand::kPing,
                             kHidInitPacketHeaderSize + 1, data);
  ASSERT_TRUE(three_small_messages);
  EXPECT_EQ(3U, three_small_messages->NumPackets());
}

TEST(FidoHidMessageTest, TooLarge) {
  std::vector<uint8_t> data;

  // kHidInitPacketHeaderSize is too small a report size to be valid.
  EXPECT_DEATH_IF_SUPPORTED(
      FidoHidMessage::Create(kHidBroadcastChannel, FidoHidDeviceCommand::kPing,
                             kHidInitPacketHeaderSize, data),
      "");

  // kHidMaxPacketSize + 1 is too large a report size.
  EXPECT_DEATH_IF_SUPPORTED(
      FidoHidMessage::Create(kHidBroadcastChannel, FidoHidDeviceCommand::kPing,
                             kHidMaxPacketSize + 1, data),
      "");
}

TEST(FidoHidMessageTest, TestMaxSize) {
  uint32_t channel_id = 0x00010203;
  std::vector<uint8_t> data(kHidMaxMessageSize + 1);
  auto oversize_message = FidoHidMessage::Create(
      channel_id, FidoHidDeviceCommand::kPing, kHidMaxPacketSize, data);
  EXPECT_FALSE(oversize_message);
}

TEST(FidoHidMessageTest, TestDeconstruct) {
  uint32_t channel_id = 0x0A0B0C0D;
  std::vector<uint8_t> data(kHidMaxMessageSize, 0x7F);
  auto filled_message = FidoHidMessage::Create(
      channel_id, FidoHidDeviceCommand::kPing, kHidMaxPacketSize, data);
  ASSERT_TRUE(filled_message);
  EXPECT_THAT(data,
              ::testing::ContainerEq(filled_message->GetMessagePayload()));
}

TEST(FidoHidMessageTest, TestDeserialize) {
  uint32_t channel_id = 0x0A0B0C0D;
  std::vector<uint8_t> data(kHidMaxMessageSize);

  auto orig_message = FidoHidMessage::Create(
      channel_id, FidoHidDeviceCommand::kPing, kHidMaxPacketSize, data);
  ASSERT_TRUE(orig_message);

  base::circular_deque<std::vector<uint8_t>> orig_list;
  auto buf = orig_message->PopNextPacket();
  orig_list.push_back(buf);

  auto new_message = FidoHidMessage::CreateFromSerializedData(buf);
  while (!new_message->MessageComplete()) {
    buf = orig_message->PopNextPacket();
    orig_list.push_back(buf);
    new_message->AddContinuationPacket(buf);
  }

  while (!(buf = new_message->PopNextPacket()).empty()) {
    EXPECT_EQ(buf, orig_list.front());
    orig_list.pop_front();
  }
}

}  // namespace device
