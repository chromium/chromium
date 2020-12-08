// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <array>
#include <list>

#include "base/big_endian.h"
#include "base/bind.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_elementary_stream_descriptor.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_elementary_stream_info.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_elementary_stream_packetizer.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_media_packetizer.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_transport_stream_packetizer.h"
#include "testing/gtest/include/gtest/gtest.h"

using PacketPart = extensions::WiFiDisplayStreamPacketPart;

namespace extensions {

std::ostream& operator<<(std::ostream& os, const PacketPart& part) {
  const auto flags = os.flags();
  os << "{" << std::hex << std::noshowbase;
  for (const auto& item : part) {
    if (&item != &*part.begin())
      os << ", ";
    os << "0x" << static_cast<unsigned>(item);
  }
  os.setf(flags, std::ios::basefield | std::ios::showbase);
  return os << "}";
}

bool operator==(const PacketPart& a, const PacketPart& b) {
  if (a.size() != b.size())
    return false;
  return std::equal(a.begin(), a.end(), b.begin());
}

namespace {

namespace pes {
const unsigned kDtsFlag = 0x0040u;
const unsigned kMarkerFlag = 0x8000u;
const unsigned kPtsFlag = 0x0080u;
const size_t kUnitDataAlignment = sizeof(uint32_t);
}

namespace rtp {
const unsigned kVersionMask = 0xC000u;
const unsigned kVersion2 = 0x8000u;
const unsigned kPaddingFlag = 0x2000u;
const unsigned kExtensionFlag = 0x1000u;
const unsigned kContributingSourceCountMask = 0x0F00u;
const unsigned kMarkerFlag = 0x0010u;
const unsigned kPayloadTypeMask = 0x007Fu;
const unsigned kPayloadTypeMP2T = 0x0021u;
}  // namespace rtp

namespace ts {
const uint64_t kTimeStampMask = (static_cast<uint64_t>(1u) << 33) - 1u;
const uint64_t kTimeStampSecond = 90000u;  // 90 kHz
const uint64_t kProgramClockReferenceSecond =
    300u * kTimeStampSecond;  // 27 MHz

// Packet header:
const size_t kPacketHeaderSize = 4u;
const unsigned kSyncByte = 0x47u;
const uint32_t kSyncByteMask = 0xFF000000u;
const uint32_t kTransportErrorIndicator = 0x00800000u;
const uint32_t kPayloadUnitStartIndicator = 0x00400000u;
const uint32_t kTransportPriority = 0x00200000u;
const uint32_t kScramblingControlMask = 0x000000C0u;
const uint32_t kAdaptationFieldFlag = 0x00000020u;
const uint32_t kPayloadFlag = 0x00000010u;

// Adaptation field:
const unsigned kRandomAccessFlag = 0x40u;
const unsigned kPcrFlag = 0x10u;
}  // namespace ts

namespace widi {
const unsigned kProgramAssociationTablePacketId = 0x0000u;
const unsigned kProgramMapTablePacketId = 0x0100u;
const unsigned kProgramClockReferencePacketId = 0x1000u;
const unsigned kVideoStreamPacketId = 0x1011u;
const unsigned kFirstAudioStreamPacketId = 0x1100u;
const size_t kMaxTransportStreamPacketCountPerDatagramPacket = 7u;
}  // namespace widi

template <typename PacketContainer>
class PacketCollector {
 public:
  PacketContainer FetchPackets() {
    PacketContainer container;
    container.swap(packets_);
    return container;
  }

 protected:
  PacketContainer packets_;
};

class FakeMediaPacketizer
    : public WiFiDisplayMediaPacketizer,
      public PacketCollector<std::vector<std::vector<uint8_t>>> {
 public:
  FakeMediaPacketizer(
      const base::TimeDelta& delay_for_unit_time_stamps,
      const std::vector<WiFiDisplayElementaryStreamInfo>& stream_infos)
      : WiFiDisplayMediaPacketizer(
            delay_for_unit_time_stamps,
            stream_infos,
            base::BindRepeating(
                &FakeMediaPacketizer::OnPacketizedMediaDatagramPacket,
                base::Unretained(this))) {}

  // Extend the interface in order to allow to bypass packetization of units to
  // Packetized Elementary Stream (PES) packets and further to Transport Stream
  // (TS) packets and to test only packetization of TS packets to media
  // datagram packets.
  bool EncodeTransportStreamPacket(
      const WiFiDisplayTransportStreamPacket& transport_stream_packet,
      bool flush) {
    return OnPacketizedTransportStreamPacket(transport_stream_packet, flush);
  }

 private:
  bool OnPacketizedMediaDatagramPacket(
      WiFiDisplayMediaDatagramPacket media_datagram_packet) {
    packets_.emplace_back(std::move(media_datagram_packet));
    return true;
  }
};

class FakeTransportStreamPacketizer
    : public WiFiDisplayTransportStreamPacketizer,
      public PacketCollector<std::list<WiFiDisplayTransportStreamPacket>> {
 public:
  FakeTransportStreamPacketizer(
      const base::TimeDelta& delay_for_unit_time_stamps,
      const std::vector<WiFiDisplayElementaryStreamInfo>& stream_infos)
      : WiFiDisplayTransportStreamPacketizer(delay_for_unit_time_stamps,
                                             stream_infos) {}

  using WiFiDisplayTransportStreamPacketizer::NormalizeUnitTimeStamps;

 protected:
  bool OnPacketizedTransportStreamPacket(
      const WiFiDisplayTransportStreamPacket& transport_stream_packet,
      bool flush) override {
    // Make a copy of header bytes as they are in stack.
    headers_.emplace_back(transport_stream_packet.header().begin(),
                          transport_stream_packet.header().end());
    const auto& header = headers_.back();
    const auto& payload = transport_stream_packet.payload();
    packets_.emplace_back(header.data(), header.size(), payload.data(),
                          payload.size());
    EXPECT_EQ(transport_stream_packet.header().size(),
              packets_.back().header().size());
    EXPECT_EQ(transport_stream_packet.payload().size(),
              packets_.back().payload().size());
    EXPECT_EQ(transport_stream_packet.filler().size(),
              packets_.back().filler().size());
    return true;
  }

 private:
  std::vector<std::vector<uint8_t>> headers_;
};

const uint64_t kInvalidProgramClockReferenceBase = ~static_cast<uint64_t>(0u);

struct ProgramClockReference {
  uint64_t base;
  uint16_t extension;
};

ProgramClockReference ParseProgramClockReference(const uint8_t pcr_bytes[6]) {
  const uint8_t reserved_pcr_bits = pcr_bytes[4] & 0x7Eu;
  EXPECT_EQ(0x7Eu, reserved_pcr_bits);
  ProgramClockReference pcr;
  pcr.base = pcr_bytes[0];
  pcr.base = (pcr.base << 8) | pcr_bytes[1];
  pcr.base = (pcr.base << 8) | pcr_bytes[2];
  pcr.base = (pcr.base << 8) | pcr_bytes[3];
  pcr.base = (pcr.base << 1) | ((pcr_bytes[4] & 0x80u) >> 7);
  pcr.extension = pcr_bytes[4] & 0x01u;
  pcr.extension = (pcr.extension << 8) | pcr_bytes[5];
  return pcr;
}

uint64_t ParseTimeStamp(const uint8_t ts_bytes[5], uint8_t pts_dts_indicator) {
  EXPECT_EQ(pts_dts_indicator, (ts_bytes[0] & 0xF0u) >> 4);
  EXPECT_EQ(0x01u, ts_bytes[0] & 0x01u);
  EXPECT_EQ(0x01u, ts_bytes[2] & 0x01u);
  EXPECT_EQ(0x01u, ts_bytes[4] & 0x01u);
  uint64_t ts = 0u;
  ts = (ts_bytes[0] & 0x0Eu) >> 1;
  ts = (ts << 8) | ts_bytes[1];
  ts = (ts << 7) | ((ts_bytes[2] & 0xFEu) >> 1);
  ts = (ts << 8) | ts_bytes[3];
  ts = (ts << 7) | ((ts_bytes[4] & 0xFEu) >> 1);
  return ts;
}

unsigned ParseTransportStreamPacketId(
    const WiFiDisplayTransportStreamPacket& packet) {
  if (packet.header().size() < ts::kPacketHeaderSize)
    return ~0u;
  return (((packet.header().begin()[1] & 0x001Fu) << 8) |
          packet.header().begin()[2]);
}

class WiFiDisplayElementaryStreamUnitPacketizationTest
    : public testing::TestWithParam<
          testing::tuple<unsigned, base::TimeDelta, base::TimeDelta>> {
 protected:
  static base::TimeTicks SumOrNull(const base::TimeTicks& base,
                                   const base::TimeDelta& delta) {
    return delta.is_max() ? base::TimeTicks() : base + delta;
  }

  WiFiDisplayElementaryStreamUnitPacketizationTest()
      : unit_(testing::get<0>(GetParam())),
        now_(base::TimeTicks::Now()),
        dts_(SumOrNull(now_, testing::get<1>(GetParam()))),
        pts_(SumOrNull(now_, testing::get<2>(GetParam()))) {}

  void CheckElementaryStreamPacketHeader(
      const WiFiDisplayElementaryStreamPacket& packet,
      uint8_t stream_id) {
    base::BigEndianReader header_reader(
        reinterpret_cast<const char*>(packet.header().begin()),
        packet.header().size());
    uint8_t parsed_packet_start_code_prefix[3];
    EXPECT_TRUE(
        header_reader.ReadBytes(parsed_packet_start_code_prefix,
                                sizeof(parsed_packet_start_code_prefix)));
    EXPECT_EQ(0x00u, parsed_packet_start_code_prefix[0]);
    EXPECT_EQ(0x00u, parsed_packet_start_code_prefix[1]);
    EXPECT_EQ(0x01u, parsed_packet_start_code_prefix[2]);
    uint8_t parsed_stream_id;
    EXPECT_TRUE(header_reader.ReadU8(&parsed_stream_id));
    EXPECT_EQ(stream_id, parsed_stream_id);
    uint16_t parsed_packet_length;
    EXPECT_TRUE(header_reader.ReadU16(&parsed_packet_length));
    size_t packet_length = static_cast<size_t>(header_reader.remaining()) +
                           packet.unit_header().size() + packet.unit().size();
    if (packet_length >> 16)
      packet_length = 0u;
    EXPECT_EQ(packet_length, parsed_packet_length);
    uint16_t parsed_flags;
    EXPECT_TRUE(header_reader.ReadU16(&parsed_flags));
    EXPECT_EQ(
        0u, parsed_flags & ~(pes::kMarkerFlag | pes::kPtsFlag | pes::kDtsFlag));
    const bool parsed_pts_flag = (parsed_flags & pes::kPtsFlag) != 0u;
    const bool parsed_dts_flag = (parsed_flags & pes::kDtsFlag) != 0u;
    EXPECT_EQ(!pts_.is_null(), parsed_pts_flag);
    EXPECT_EQ(!pts_.is_null() && !dts_.is_null(), parsed_dts_flag);
    uint8_t parsed_header_length;
    EXPECT_TRUE(header_reader.ReadU8(&parsed_header_length));
    EXPECT_EQ(header_reader.remaining(), parsed_header_length);
    if (parsed_pts_flag) {
      uint8_t parsed_pts_bytes[5];
      EXPECT_TRUE(
          header_reader.ReadBytes(parsed_pts_bytes, sizeof(parsed_pts_bytes)));
      const uint64_t parsed_pts =
          ParseTimeStamp(parsed_pts_bytes, parsed_dts_flag ? 0x3u : 0x2u);
      if (parsed_dts_flag) {
        uint8_t parsed_dts_bytes[5];
        EXPECT_TRUE(header_reader.ReadBytes(parsed_dts_bytes,
                                            sizeof(parsed_dts_bytes)));
        const uint64_t parsed_dts = ParseTimeStamp(parsed_dts_bytes, 0x1u);
        EXPECT_EQ(
            static_cast<uint64_t>(90 * (pts_ - dts_).InMicroseconds() / 1000),
            (parsed_pts - parsed_dts) & UINT64_C(0x1FFFFFFFF));
      }
    }
    while (header_reader.remaining() > 0) {
      uint8_t parsed_stuffing_byte;
      EXPECT_TRUE(header_reader.ReadU8(&parsed_stuffing_byte));
      EXPECT_EQ(0xFFu, parsed_stuffing_byte);
    }
    EXPECT_EQ(0, header_reader.remaining());
  }

  void CheckElementaryStreamPacketUnitHeader(
      const WiFiDisplayElementaryStreamPacket& packet,
      const uint8_t* unit_header_data,
      size_t unit_header_size) {
    EXPECT_EQ(unit_header_data, packet.unit_header().begin());
    EXPECT_EQ(unit_header_size, packet.unit_header().size());
  }

  void CheckElementaryStreamPacketUnit(
      const WiFiDisplayElementaryStreamPacket& packet) {
    EXPECT_EQ(0u, (packet.header().size() + packet.unit_header().size()) %
                      pes::kUnitDataAlignment);
    EXPECT_EQ(unit_.data(), packet.unit().begin());
    EXPECT_EQ(unit_.size(), packet.unit().size());
  }

  void CheckTransportStreamPacketHeader(
      base::BigEndianReader* header_reader,
      bool expected_payload_unit_start_indicator,
      unsigned expected_packet_id,
      bool* adaptation_field_flag,
      uint8_t expected_continuity_counter) {
    uint32_t parsed_u32;
    EXPECT_TRUE(header_reader->ReadU32(&parsed_u32));
    EXPECT_EQ(ts::kSyncByte << 24u, parsed_u32 & ts::kSyncByteMask);
    EXPECT_EQ(0x0u, parsed_u32 & ts::kTransportErrorIndicator);
    EXPECT_EQ(expected_payload_unit_start_indicator,
              (parsed_u32 & ts::kPayloadUnitStartIndicator) != 0u);
    EXPECT_EQ(0x0u, parsed_u32 & ts::kTransportPriority);
    EXPECT_EQ(expected_packet_id, (parsed_u32 & 0x001FFF00) >> 8);
    EXPECT_EQ(0x0u, parsed_u32 & ts::kScramblingControlMask);
    if (!adaptation_field_flag) {
      EXPECT_EQ(0x0u, parsed_u32 & ts::kAdaptationFieldFlag);
    } else {
      *adaptation_field_flag = (parsed_u32 & ts::kAdaptationFieldFlag) != 0u;
    }
    EXPECT_EQ(ts::kPayloadFlag, parsed_u32 & ts::kPayloadFlag);
    EXPECT_EQ(expected_continuity_counter & 0xFu, parsed_u32 & 0x0000000Fu);
  }

  void CheckTransportStreamAdaptationField(
      base::BigEndianReader* header_reader,
      const WiFiDisplayTransportStreamPacket& packet,
      uint8_t* adaptation_field_flags) {
    uint8_t parsed_adaptation_field_length;
    EXPECT_TRUE(header_reader->ReadU8(&parsed_adaptation_field_length));
    if (parsed_adaptation_field_length > 0u) {
      const int initial_remaining = header_reader->remaining();
      uint8_t parsed_adaptation_field_flags;
      EXPECT_TRUE(header_reader->ReadU8(&parsed_adaptation_field_flags));
      if (!adaptation_field_flags) {
        EXPECT_EQ(0x0u, parsed_adaptation_field_flags);
      } else {
        *adaptation_field_flags = parsed_adaptation_field_flags;
        if (parsed_adaptation_field_flags & ts::kPcrFlag) {
          uint8_t parsed_pcr_bytes[6];
          EXPECT_TRUE(header_reader->ReadBytes(parsed_pcr_bytes,
                                               sizeof(parsed_pcr_bytes)));
          parsed_pcr_ = ParseProgramClockReference(parsed_pcr_bytes);
        }
      }
      size_t remaining_stuffing_length =
          parsed_adaptation_field_length -
          static_cast<size_t>(initial_remaining - header_reader->remaining());
      while (remaining_stuffing_length > 0u && header_reader->remaining() > 0) {
        // Adaptation field stuffing byte in header_reader.
        uint8_t parsed_stuffing_byte;
        EXPECT_TRUE(header_reader->ReadU8(&parsed_stuffing_byte));
        EXPECT_EQ(0xFFu, parsed_stuffing_byte);
        --remaining_stuffing_length;
      }
      if (packet.payload().empty()) {
        // Adaptation field stuffing bytes in packet.filler().
        EXPECT_EQ(remaining_stuffing_length, packet.filler().size());
        EXPECT_EQ(0xFFu, packet.filler().value());
      } else {
        EXPECT_EQ(0u, remaining_stuffing_length);
      }
    }
  }

  void CheckTransportStreamProgramAssociationTablePacket(
      const WiFiDisplayTransportStreamPacket& packet) {
    static const uint8_t kProgramAssicationTable[4u + 13u] = {
        // Pointer:
        0u,  // Pointer field
        // Table header:
        0x00u,       // Table ID (PAT)
        0x80u |      // Section syntax indicator (0b1 for PAT)
            0x00u |  // Private bit (0b0 for PAT)
            0x30u |  // Reserved bits (0b11)
            0x00u |  // Section length unused bits (0b00)
            0u,      // Section length (10 bits)
        13u,         //
        // Table syntax:
        0x00u,       // Table ID extension (transport stream ID)
        0x01u,       //
        0xC0u |      // Reserved bits (0b11)
            0x00u |  // Version (0b00000)
            0x01u,   // Current indicator (0b1)
        0u,          // Section number
        0u,          // Last section number
        // Program association table specific data:
        0x00u,      // Program number
        0x01u,      //
        0xE0 |      // Reserved bits (0b111)
            0x01u,  // Program map packet ID (13 bits)
        0x00,       //
        // CRC:
        0xE8u,
        0xF9u, 0x5Eu, 0x7Du};

    base::BigEndianReader header_reader(
        reinterpret_cast<const char*>(packet.header().begin()),
        packet.header().size());
    CheckTransportStreamPacketHeader(
        &header_reader, true, widi::kProgramAssociationTablePacketId, nullptr,
        continuity_.program_assication_table++);
    EXPECT_EQ(0, header_reader.remaining());

    EXPECT_EQ(PacketPart(kProgramAssicationTable), packet.payload());
  }

  void CheckTransportStreamProgramMapTablePacket(
      const WiFiDisplayTransportStreamPacket& packet,
      const PacketPart& program_map_table) {
    base::BigEndianReader header_reader(
        reinterpret_cast<const char*>(packet.header().begin()),
        packet.header().size());
    CheckTransportStreamPacketHeader(&header_reader, true,
                                     widi::kProgramMapTablePacketId, nullptr,
                                     continuity_.program_map_table++);
    EXPECT_EQ(0, header_reader.remaining());

    EXPECT_EQ(program_map_table, packet.payload());
  }

  void CheckTransportStreamProgramClockReferencePacket(
      const WiFiDisplayTransportStreamPacket& packet) {
    base::BigEndianReader header_reader(
        reinterpret_cast<const char*>(packet.header().begin()),
        packet.header().size());

    bool parsed_adaptation_field_flag;
    CheckTransportStreamPacketHeader(
        &header_reader, true, widi::kProgramClockReferencePacketId,
        &parsed_adaptation_field_flag, continuity_.program_clock_reference++);
    EXPECT_TRUE(parsed_adaptation_field_flag);

    uint8_t parsed_adaptation_field_flags;
    CheckTransportStreamAdaptationField(&header_reader, packet,
                                        &parsed_adaptation_field_flags);
    EXPECT_EQ(ts::kPcrFlag, parsed_adaptation_field_flags);

    EXPECT_EQ(0, header_reader.remaining());
    EXPECT_EQ(0u, packet.payload().size());
  }

  void CheckTransportStreamElementaryStreamPacket(
      const WiFiDisplayTransportStreamPacket& packet,
      const WiFiDisplayElementaryStreamPacket& elementary_stream_packet,
      unsigned stream_index,
      unsigned expected_packet_id,
      bool expected_random_access,
      const uint8_t** unit_data_pos) {
    const bool first_transport_stream_packet_for_current_unit =
        packet.payload().begin() == unit_.data();
    const bool last_transport_stream_packet_for_current_unit =
        packet.payload().end() == unit_.data() + unit_.size();
    base::BigEndianReader header_reader(
        reinterpret_cast<const char*>(packet.header().begin()),
        packet.header().size());

    bool parsed_adaptation_field_flag;
    CheckTransportStreamPacketHeader(
        &header_reader, first_transport_stream_packet_for_current_unit,
        expected_packet_id, &parsed_adaptation_field_flag,
        continuity_.elementary_streams[stream_index]++);

    if (first_transport_stream_packet_for_current_unit) {
      // Random access can only be signified by adaptation field.
      if (expected_random_access)
        EXPECT_TRUE(parsed_adaptation_field_flag);
      // If there is no need for padding nor for a random access indicator,
      // then there is no need for an adaptation field, either.
      if (!last_transport_stream_packet_for_current_unit &&
          !expected_random_access) {
        EXPECT_FALSE(parsed_adaptation_field_flag);
      }
      if (parsed_adaptation_field_flag) {
        uint8_t parsed_adaptation_field_flags;
        CheckTransportStreamAdaptationField(&header_reader, packet,
                                            &parsed_adaptation_field_flags);
        EXPECT_EQ(expected_random_access ? ts::kRandomAccessFlag : 0u,
                  parsed_adaptation_field_flags);
      }

      // Elementary stream header.
      PacketPart parsed_elementary_stream_packet_header(
          packet.header().end() - header_reader.remaining(),
          std::min(elementary_stream_packet.header().size(),
                   static_cast<size_t>(header_reader.remaining())));
      EXPECT_EQ(elementary_stream_packet.header(),
                parsed_elementary_stream_packet_header);
      EXPECT_TRUE(
          header_reader.Skip(parsed_elementary_stream_packet_header.size()));

      // Elementary stream unit header.
      PacketPart parsed_unit_header(
          packet.header().end() - header_reader.remaining(),
          std::min(elementary_stream_packet.unit_header().size(),
                   static_cast<size_t>(header_reader.remaining())));
      EXPECT_EQ(elementary_stream_packet.unit_header(), parsed_unit_header);
      EXPECT_TRUE(header_reader.Skip(parsed_unit_header.size()));

      // Time stamps.
      if (parsed_elementary_stream_packet_header.size() >= 19u) {
        uint64_t parsed_dts = ParseTimeStamp(
            &parsed_elementary_stream_packet_header.begin()[14], 0x1u);
        // Check that
        //   0 <= 300 * parsed_dts - parsed_pcr_value <=
        //       kProgramClockReferenceSecond
        // where
        //   parsed_pcr_value = 300 * parsed_pcr_.base + parsed_pcr_.extension
        // but allow parsed_pcr_.base and parsed_dts to wrap around in 33 bits.
        EXPECT_NE(kInvalidProgramClockReferenceBase, parsed_pcr_.base);
        EXPECT_LE(
            300u * ((parsed_dts - parsed_pcr_.base) & ts::kTimeStampMask) -
                parsed_pcr_.extension,
            ts::kProgramClockReferenceSecond)
            << " DTS must be not smaller than PCR!";
      }
    } else {
      // If there is no need for padding, then there is no need for
      // an adaptation field, either.
      if (!last_transport_stream_packet_for_current_unit)
        EXPECT_FALSE(parsed_adaptation_field_flag);
      if (parsed_adaptation_field_flag) {
        CheckTransportStreamAdaptationField(&header_reader, packet, nullptr);
      }
    }
    EXPECT_EQ(0, header_reader.remaining());

    // Transport stream packet payload.
    EXPECT_EQ(*unit_data_pos, packet.payload().begin());
    if (*unit_data_pos == packet.payload().begin())
      *unit_data_pos += packet.payload().size();

    // Transport stream packet filler.
    EXPECT_EQ(0u, packet.filler().size());
  }

  enum { kVideoOnlyUnitSize = 0x8000u };  // Not exact. Be on the safe side.

  const std::vector<uint8_t> unit_;
  const base::TimeTicks now_;
  const base::TimeTicks dts_;
  const base::TimeTicks pts_;

  struct {
    size_t program_assication_table;
    size_t program_map_table;
    size_t program_clock_reference;
    size_t elementary_streams[3];
  } continuity_ = {0u, 0u, 0u, {0u, 0u, 0u}};
  ProgramClockReference parsed_pcr_ = {kInvalidProgramClockReferenceBase, 0u};
};

TEST_P(WiFiDisplayElementaryStreamUnitPacketizationTest,
       EncodeToElementaryStreamPacket) {
  const size_t kMaxUnitHeaderSize = 4u;

  const uint8_t stream_id =
      unit_.size() >= kVideoOnlyUnitSize
          ? WiFiDisplayElementaryStreamPacketizer::kFirstVideoStreamId
          : WiFiDisplayElementaryStreamPacketizer::kFirstAudioStreamId;

  uint8_t unit_header_data[kMaxUnitHeaderSize];
  for (size_t unit_header_size = 0u; unit_header_size <= kMaxUnitHeaderSize;
       ++unit_header_size) {
    WiFiDisplayElementaryStreamPacket packet =
        WiFiDisplayElementaryStreamPacketizer::EncodeElementaryStreamUnit(
            stream_id, unit_header_data, unit_header_size, unit_.data(),
            unit_.size(), pts_, dts_);
    CheckElementaryStreamPacketHeader(packet, stream_id);
    CheckElementaryStreamPacketUnitHeader(packet, unit_header_data,
                                          unit_header_size);
    CheckElementaryStreamPacketUnit(packet);
  }
}

TEST_P(WiFiDisplayElementaryStreamUnitPacketizationTest,
       EncodeToTransportStreamPackets) {
  enum { kStreamCount = 3u };
  static const bool kBoolValues[] = {false, true};
  static const unsigned kPacketIds[kStreamCount] = {
      widi::kVideoStreamPacketId, widi::kFirstAudioStreamPacketId + 0u,
      widi::kFirstAudioStreamPacketId + 1u};
  static const uint8_t kProgramMapTable[4u + 42u] = {
      // Pointer:
      0u,  // Pointer field
      // Table header:
      0x02u,       // Table ID (PMT)
      0x80u |      // Section syntax indicator (0b1 for PMT)
          0x00u |  // Private bit (0b0 for PMT)
          0x30u |  // Reserved bits (0b11)
          0x00u |  // Section length unused bits (0b00)
          0u,      // Section length (10 bits)
      42u,         //
      // Table syntax:
      0x00u,       // Table ID extension (program number)
      0x01u,       //
      0xC0u |      // Reserved bits (0b11)
          0x00u |  // Version (0b00000)
          0x01u,   // Current indicator (0b1)
      0u,          // Section number
      0u,          // Last section number
      // Program map table specific data:
      0xE0u |      // Reserved bits (0b111)
          0x10u,   // Program clock reference packet ID (13 bits)
      0x00u,       //
      0xF0u |      // Reserved bits (0b11)
          0x00u |  // Program info length unused bits
          0u,      // Program info length (10 bits)
      0u,          //
      // Elementary stream specific data:
      0x1Bu,       // Stream type (H.264 in a packetized stream)
      0xE0u |      // Reserved bits (0b111)
          0x10u,   // Elementary packet ID (13 bits)
      0x11u,       //
      0xF0u |      // Reserved bits (0b1111)
          0x00u |  // Elementary stream info length unused bits
          0u,      // Elementary stream info length (10 bits)
      10u,         //
      0x28u,       // AVC video descriptor tag
      4u,          // Descriptor length
      0x42u,
      0xF5u, 0x2Au, 0xBFu,
      0x2Au,  // AVC timing and HRD descriptor tag
      2u,     // Descriptor length
      0x7Eu, 0x1Fu,
      // Elementary stream specific data:
      0x83u,       // Stream type (lossless audio in a packetized stream)
      0xE0u |      // Reserved bits (0b111)
          0x11u,   // Elementary packet ID (13 bits)
      0x00u,       //
      0xF0u |      // Reserved bits (0b1111)
          0x00u |  // Elementary stream info length unused bits
          0u,      // Elementary stream info length (10 bits)
      4u,          //
      0x83u,       // LPCM audio stream descriptor tag
      2u,          // Descriptor length
      0x26u,
      0x2Fu,
      // Elementary stream specific data:
      0x0Fu,       // Stream type (AAC in a packetized stream)
      0xE0u |      // Reserved bits (0b111)
          0x11u,   // Elementary packet ID (13 bits)
      0x01u,       //
      0xF0u |      // Reserved bits (0b1111)
          0x00u |  // Elementary stream info length unused bits
          0u,      // Elementary stream info length (10 bits)
      0u,          //
      // CRC:
      0x3Du,
      0xAAu, 0x9Eu, 0x45u};
  static const uint8_t kStreamIds[] = {
      WiFiDisplayElementaryStreamPacketizer::kFirstVideoStreamId,
      WiFiDisplayElementaryStreamPacketizer::kPrivateStream1Id,
      WiFiDisplayElementaryStreamPacketizer::kFirstAudioStreamId};

  using ESDescriptor = WiFiDisplayElementaryStreamDescriptor;
  std::vector<ESDescriptor> lpcm_descriptors;
  lpcm_descriptors.emplace_back(ESDescriptor::LPCMAudioStream::Create(
      ESDescriptor::LPCMAudioStream::SAMPLING_FREQUENCY_44_1K,
      ESDescriptor::LPCMAudioStream::BITS_PER_SAMPLE_16, false,
      ESDescriptor::LPCMAudioStream::NUMBER_OF_CHANNELS_STEREO));
  std::vector<ESDescriptor> video_desciptors;
  video_desciptors.emplace_back(ESDescriptor::AVCVideo::Create(
      ESDescriptor::AVCVideo::PROFILE_BASELINE, true, true, true, 0x15u,
      ESDescriptor::AVCVideo::LEVEL_4_2, true));
  video_desciptors.emplace_back(ESDescriptor::AVCTimingAndHRD::Create());
  std::vector<WiFiDisplayElementaryStreamInfo> stream_infos;
  stream_infos.emplace_back(WiFiDisplayElementaryStreamInfo::VIDEO_H264,
                            std::move(video_desciptors));
  stream_infos.emplace_back(WiFiDisplayElementaryStreamInfo::AUDIO_LPCM,
                            std::move(lpcm_descriptors));
  stream_infos.emplace_back(WiFiDisplayElementaryStreamInfo::AUDIO_AAC);
  FakeTransportStreamPacketizer packetizer(
      base::TimeDelta::FromMilliseconds(200), stream_infos);

  size_t packet_index = 0u;
  for (unsigned stream_index = 0; stream_index < kStreamCount; ++stream_index) {
    const uint8_t* unit_header_data = nullptr;
    size_t unit_header_size = 0u;
    if (stream_index > 0u) {  // Audio stream.
      if (unit_.size() >= kVideoOnlyUnitSize)
        continue;
      if (stream_index == 1u) {  // LPCM
        unit_header_data = reinterpret_cast<const uint8_t*>("\xA0\x06\x00\x09");
        unit_header_size = 4u;
      }
    }
    for (const bool random_access : kBoolValues) {
      EXPECT_TRUE(packetizer.EncodeElementaryStreamUnit(
          stream_index, unit_.data(), unit_.size(), random_access, pts_, dts_,
          true));
      auto normalized_pts = pts_;
      auto normalized_dts = dts_;
      packetizer.NormalizeUnitTimeStamps(&normalized_pts, &normalized_dts);
      WiFiDisplayElementaryStreamPacket elementary_stream_packet =
          WiFiDisplayElementaryStreamPacketizer::EncodeElementaryStreamUnit(
              kStreamIds[stream_index], unit_header_data, unit_header_size,
              unit_.data(), unit_.size(), normalized_pts, normalized_dts);

      const uint8_t* unit_data_pos = unit_.data();
      for (const auto& packet : packetizer.FetchPackets()) {
        switch (ParseTransportStreamPacketId(packet)) {
          case widi::kProgramAssociationTablePacketId:
            if (packet_index < 4u)
              EXPECT_EQ(0u, packet_index);
            CheckTransportStreamProgramAssociationTablePacket(packet);
            break;
          case widi::kProgramMapTablePacketId:
            if (packet_index < 4u)
              EXPECT_EQ(1u, packet_index);
            CheckTransportStreamProgramMapTablePacket(
                packet, PacketPart(kProgramMapTable));
            break;
          case widi::kProgramClockReferencePacketId:
            if (packet_index < 4u)
              EXPECT_EQ(2u, packet_index);
            CheckTransportStreamProgramClockReferencePacket(packet);
            break;
          default:
            if (packet_index < 4u)
              EXPECT_EQ(3u, packet_index);
            CheckTransportStreamElementaryStreamPacket(
                packet, elementary_stream_packet, stream_index,
                kPacketIds[stream_index], random_access, &unit_data_pos);
        }
        ++packet_index;
      }
      EXPECT_EQ(unit_.data() + unit_.size(), unit_data_pos);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    WiFiDisplayElementaryStreamUnitPacketizationTests,
    WiFiDisplayElementaryStreamUnitPacketizationTest,
    testing::Combine(testing::Values(123u, 180u, 0x10000u),
                     testing::Values(base::TimeDelta::Max(),
                                     base::TimeDelta::FromMicroseconds(0)),
                     testing::Values(base::TimeDelta::Max(),
                                     base::TimeDelta::FromMicroseconds(
                                         1000 * INT64_C(0x123456789) / 90))));

TEST(WiFiDisplayTransportStreamPacketizationTest, EncodeToMediaDatagramPacket) {
  const size_t kPacketHeaderSize = 12u;

  // Create fake units.
  const size_t kUnitCount = 12u;
  const size_t kUnitSize =
      WiFiDisplayTransportStreamPacket::kPacketSize - 4u - 12u;
  std::vector<std::array<uint8_t, kUnitSize>> units(kUnitCount);
  for (auto& unit : units)
    unit.fill(static_cast<uint8_t>(&unit - units.data()));

  // Create transport stream packets.
  std::vector<WiFiDisplayElementaryStreamInfo> stream_infos;
  stream_infos.emplace_back(WiFiDisplayElementaryStreamInfo::VIDEO_H264);
  FakeTransportStreamPacketizer transport_stream_packetizer(
      base::TimeDelta::FromMilliseconds(0), stream_infos);
  for (const auto& unit : units) {
    EXPECT_TRUE(transport_stream_packetizer.EncodeElementaryStreamUnit(
        0u, unit.data(), unit.size(), false, base::TimeTicks(),
        base::TimeTicks(), &unit == &units.back()));
  }
  auto transport_stream_packets = transport_stream_packetizer.FetchPackets();
  // There should be exactly one transport stream payload packet for each unit.
  // There should also be some but not too many transport stream meta
  // information packets.
  EXPECT_EQ(1u, transport_stream_packets.size() / kUnitCount);

  // Encode transport stream packets to datagram packets.
  FakeMediaPacketizer packetizer(
      base::TimeDelta::FromMilliseconds(0),
      std::vector<WiFiDisplayElementaryStreamInfo>());
  for (const auto& transport_stream_packet : transport_stream_packets) {
    EXPECT_TRUE(packetizer.EncodeTransportStreamPacket(
        transport_stream_packet,
        &transport_stream_packet == &transport_stream_packets.back()));
  }
  auto packets = packetizer.FetchPackets();

  // Check datagram packets.
  ProgramClockReference pcr = {kInvalidProgramClockReferenceBase, 0u};
  uint16_t sequence_number = 0u;
  uint32_t synchronization_source_identifier;
  auto transport_stream_packet_it = transport_stream_packets.cbegin();
  for (const auto& packet : packets) {
    base::BigEndianReader header_reader(
        reinterpret_cast<const char*>(packet.data()),
        std::min(kPacketHeaderSize, packet.size()));

    // Packet flags.
    uint16_t parsed_u16;
    EXPECT_TRUE(header_reader.ReadU16(&parsed_u16));
    EXPECT_EQ(rtp::kVersion2, parsed_u16 & rtp::kVersionMask);
    EXPECT_FALSE(parsed_u16 & rtp::kPaddingFlag);
    EXPECT_FALSE(parsed_u16 & rtp::kExtensionFlag);
    EXPECT_EQ(0u, parsed_u16 & rtp::kContributingSourceCountMask);
    EXPECT_FALSE(parsed_u16 & rtp::kMarkerFlag);
    EXPECT_EQ(rtp::kPayloadTypeMP2T, parsed_u16 & rtp::kPayloadTypeMask);

    // Packet sequence number.
    uint16_t parsed_sequence_number;
    EXPECT_TRUE(header_reader.ReadU16(&parsed_sequence_number));
    if (&packet == &packets.front())
      sequence_number = parsed_sequence_number;
    EXPECT_EQ(sequence_number++, parsed_sequence_number);

    // Packet time stamp.
    uint32_t parsed_time_stamp;
    EXPECT_TRUE(header_reader.ReadU32(&parsed_time_stamp));
    if (pcr.base == kInvalidProgramClockReferenceBase) {
      // This happens only for the first datagram packet.
      EXPECT_TRUE(&packet == &packets.front());
      // Ensure that the next datagram packet reaches the else branch.
      EXPECT_FALSE(&packet == &packets.back());
    } else {
      // Check that
      //   0 <= parsed_time_stamp - pcr.base <= kTimeStampSecond
      // but allow pcr.base and parsed_time_stamp to wrap around in 32 bits.
      EXPECT_LE((parsed_time_stamp - pcr.base) & 0xFFFFFFFFu,
                ts::kTimeStampSecond)
          << " Time stamp must not be smaller than PCR!";
    }

    // Packet synchronization source identifier.
    uint32_t parsed_synchronization_source_identifier;
    EXPECT_TRUE(
        header_reader.ReadU32(&parsed_synchronization_source_identifier));
    if (&packet == &packets.front()) {
      synchronization_source_identifier =
          parsed_synchronization_source_identifier;
    }
    EXPECT_EQ(synchronization_source_identifier,
              parsed_synchronization_source_identifier);

    EXPECT_EQ(0, header_reader.remaining());

    // Packet payload.
    size_t offset = kPacketHeaderSize;
    while (offset + WiFiDisplayTransportStreamPacket::kPacketSize <=
               packet.size() &&
           transport_stream_packet_it != transport_stream_packets.end()) {
      const auto& transport_stream_packet = *transport_stream_packet_it++;
      const PacketPart parsed_transport_stream_packet_header(
          packet.data() + offset, transport_stream_packet.header().size());
      const PacketPart parsed_transport_stream_packet_payload(
          parsed_transport_stream_packet_header.end(),
          transport_stream_packet.payload().size());
      const PacketPart parsed_transport_stream_packet_filler(
          parsed_transport_stream_packet_payload.end(),
          transport_stream_packet.filler().size());
      offset += WiFiDisplayTransportStreamPacket::kPacketSize;

      // Check bytes.
      EXPECT_EQ(transport_stream_packet.header(),
                parsed_transport_stream_packet_header);
      EXPECT_EQ(transport_stream_packet.payload(),
                parsed_transport_stream_packet_payload);
      EXPECT_EQ(transport_stream_packet.filler().size(),
                std::count(parsed_transport_stream_packet_filler.begin(),
                           parsed_transport_stream_packet_filler.end(),
                           transport_stream_packet.filler().value()));

      if (ParseTransportStreamPacketId(transport_stream_packet) ==
          widi::kProgramClockReferencePacketId) {
        pcr = ParseProgramClockReference(
            &transport_stream_packet.header().begin()[6]);
      }
    }
    EXPECT_EQ(offset, packet.size()) << " Extra packet payload bytes.";

    // Check that the payload contains a correct number of transport stream
    // packets.
    const size_t transport_stream_packet_count_in_datagram_packet =
        packet.size() / WiFiDisplayTransportStreamPacket::kPacketSize;
    if (&packet == &packets.back()) {
      EXPECT_GE(transport_stream_packet_count_in_datagram_packet, 1u);
      EXPECT_LE(transport_stream_packet_count_in_datagram_packet,
                widi::kMaxTransportStreamPacketCountPerDatagramPacket);
    } else {
      EXPECT_EQ(widi::kMaxTransportStreamPacketCountPerDatagramPacket,
                transport_stream_packet_count_in_datagram_packet);
    }
  }
  EXPECT_EQ(transport_stream_packets.end(), transport_stream_packet_it);
}

}  // namespace
}  // namespace extensions
