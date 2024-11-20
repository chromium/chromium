// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp2t/es_parser_test_base.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/containers/extend.h"
#include "base/files/memory_mapped_file.h"
#include "base/format_macros.h"
#include "base/strings/cstring_view.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/test_data_util.h"
#include "media/formats/mp2t/es_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace mp2t {

EsParserTestBase::Packet::Packet() = default;

EsParserTestBase::EsParserTestBase() = default;

EsParserTestBase::~EsParserTestBase() = default;

void EsParserTestBase::LoadStream(const char* filename) {
  base::FilePath file_path = GetTestDataFilePath(filename);

  base::MemoryMappedFile stream;
  ASSERT_TRUE(stream.Initialize(file_path))
      << "Couldn't open stream file: " << file_path.MaybeAsASCII();

  stream_.resize(stream.length());
  std::ranges::copy(stream.bytes(), stream_.begin());
}

std::vector<EsParserTestBase::Packet> EsParserTestBase::LoadPacketsFromFiles(
    base::cstring_view prefix,
    size_t file_count) {
  std::vector<Packet> packets;
  for (size_t i = 0; i < file_count; ++i) {
    base::FilePath file_path =
        GetTestDataFilePath(base::StringPrintf("%s%" PRIuS, prefix.data(), i));
    base::MemoryMappedFile stream;
    EXPECT_TRUE(stream.Initialize(file_path))
        << "Couldn't open stream file: " << file_path.MaybeAsASCII();

    Packet packet;
    packet.offset = stream_.size();
    packet.size = stream.length();

    base::Extend(stream_, stream.bytes());
    packets.push_back(packet);
  }

  return packets;
}

void EsParserTestBase::NewAudioConfig(const AudioDecoderConfig& config) {
  ++config_count_;
}

void EsParserTestBase::NewVideoConfig(const VideoDecoderConfig& config) {
  ++config_count_;
}

void EsParserTestBase::EmitBuffer(scoped_refptr<StreamParserBuffer> buffer) {
  buffer_timestamps_stream_ << "(" << buffer->timestamp().InMilliseconds()
                            << ") ";
  ++buffer_count_;
}

bool EsParserTestBase::ProcessPesPackets(EsParser* es_parser,
                                         const std::vector<Packet>& pes_packets,
                                         bool force_timing) {
  DCHECK(es_parser);

  buffer_count_ = 0;
  config_count_ = 0;
  buffer_timestamps_stream_.str(std::string());

  for (const auto& packet : pes_packets) {
    size_t cur_pes_offset = packet.offset;
    size_t cur_pes_size = packet.size;

    base::TimeDelta pts = kNoTimestamp;
    DecodeTimestamp dts = kNoDecodeTimestamp;
    if (!packet.pts.is_negative() || force_timing) {
      pts = packet.pts;
    }

    DCHECK_LT(cur_pes_offset, stream_.size());
    if (!es_parser->Parse(&stream_[cur_pes_offset], cur_pes_size, pts, dts)) {
      return false;
    }
  }
  es_parser->Flush();

  buffer_timestamps_ = buffer_timestamps_stream_.str();
  base::TrimWhitespaceASCII(buffer_timestamps_, base::TRIM_ALL,
                            &buffer_timestamps_);
  return true;
}

void EsParserTestBase::ComputePacketSize(std::vector<Packet>* packets) {
  DCHECK(packets);
  if (packets->empty()) {
    return;
  }

  Packet* cur = &packets->front();
  for (Packet& next : base::span(*packets).subspan<1>()) {
    DCHECK_GE(next.offset, cur->offset);
    cur->size = next.offset - cur->offset;
    cur = &next;
  }
  DCHECK_GE(stream_.size(), cur->offset);
  cur->size = stream_.size() - cur->offset;
}

std::vector<EsParserTestBase::Packet>
EsParserTestBase::GenerateFixedSizePesPacket(size_t pes_size) {
  DCHECK_GT(stream_.size(), 0u);
  std::vector<Packet> pes_packets;
  for (Packet cur_pes_packet; cur_pes_packet.offset < stream_.size();
       cur_pes_packet.offset += pes_size) {
    pes_packets.push_back(cur_pes_packet);
  }

  ComputePacketSize(&pes_packets);
  return pes_packets;
}

}  // namespace mp2t
}  // namespace media
