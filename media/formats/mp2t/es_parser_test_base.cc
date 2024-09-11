// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/mp2t/es_parser_test_base.h"

#include "base/check_op.h"
#include "base/files/memory_mapped_file.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/test_data_util.h"
#include "media/base/timestamp_constants.h"
#include "media/formats/mp2t/es_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace mp2t {

EsParserTestBase::Packet::Packet() : offset(0u), size(0u), pts(kNoTimestamp) {}

EsParserTestBase::EsParserTestBase()
    : config_count_(0u),
      buffer_count_(0u) {
}

EsParserTestBase::~EsParserTestBase() {
}

void EsParserTestBase::LoadStream(const char* filename) {
  base::FilePath file_path = GetTestDataFilePath(filename);

  base::MemoryMappedFile stream;
  ASSERT_TRUE(stream.Initialize(file_path))
      << "Couldn't open stream file: " << file_path.MaybeAsASCII();

  stream_.resize(stream.length());
  memcpy(&stream_[0], stream.data(), stream_.size());
}

std::vector<EsParserTestBase::Packet> EsParserTestBase::LoadPacketsFromFiles(
    const char* filename_template,
    size_t file_count) {
  std::vector<Packet> packets;
  for (size_t i = 0; i < file_count; ++i) {
    base::FilePath file_path = GetTestDataFilePath(
        base::StringPrintfNonConstexpr(filename_template, i));
    base::MemoryMappedFile stream;
    EXPECT_TRUE(stream.Initialize(file_path)) << "Couldn't open stream file: "
                                              << file_path.MaybeAsASCII();

    Packet packet;
    packet.offset = stream_.size();
    packet.size = stream.length();
    packet.pts = kNoTimestamp;

    stream_.insert(stream_.end(), stream.data(),
                   stream.data() + stream.length());
    packets.push_back(packet);
  }

  return packets;
}

void EsParserTestBase::NewAudioConfig(const AudioDecoderConfig& config) {
  config_count_++;
}

void EsParserTestBase::NewVideoConfig(const VideoDecoderConfig& config) {
  config_count_++;
}

void EsParserTestBase::EmitBuffer(scoped_refptr<StreamParserBuffer> buffer) {
  buffer_timestamps_stream_ << "("
                            << buffer->timestamp().InMilliseconds()
                            << ") ";
  buffer_count_++;
}

bool EsParserTestBase::ProcessPesPackets(
    EsParser* es_parser,
    const std::vector<Packet>& pes_packets,
    bool force_timing) {
  DCHECK(es_parser);

  buffer_count_ = 0;
  config_count_ = 0;
  buffer_timestamps_stream_.str(std::string());

  for (size_t k = 0; k < pes_packets.size(); k++) {
    size_t cur_pes_offset = pes_packets[k].offset;
    size_t cur_pes_size = pes_packets[k].size;

    base::TimeDelta pts = kNoTimestamp;
    DecodeTimestamp dts = kNoDecodeTimestamp;
    if (pes_packets[k].pts >= base::TimeDelta() || force_timing)
      pts = pes_packets[k].pts;

    DCHECK_LT(cur_pes_offset, stream_.size());
    if (!es_parser->Parse(&stream_[cur_pes_offset], cur_pes_size, pts, dts))
      return false;
  }
  es_parser->Flush();

  buffer_timestamps_ = buffer_timestamps_stream_.str();
  base::TrimWhitespaceASCII(
      buffer_timestamps_, base::TRIM_ALL, &buffer_timestamps_);
  return true;
}

void EsParserTestBase::ComputePacketSize(std::vector<Packet>* packets) {
  DCHECK(packets);
  if (packets->size() == 0u)
    return;

  Packet* cur = &(*packets)[0];
  for (size_t k = 0; k < packets->size() - 1; k++) {
    Packet* next = &(*packets)[k + 1];
    DCHECK_GE(next->offset, cur->offset);
    cur->size = next->offset - cur->offset;
    cur = next;
  }
  DCHECK_GE(stream_.size(), cur->offset);
  cur->size = stream_.size() - cur->offset;
}

std::vector<EsParserTestBase::Packet>
EsParserTestBase::GenerateFixedSizePesPacket(size_t pes_size) {
  DCHECK_GT(stream_.size(), 0u);
  std::vector<Packet> pes_packets;

  Packet cur_pes_packet;
  cur_pes_packet.offset = 0;
  cur_pes_packet.pts = kNoTimestamp;
  while (cur_pes_packet.offset < stream_.size()) {
    pes_packets.push_back(cur_pes_packet);
    cur_pes_packet.offset += pes_size;
  }
  ComputePacketSize(&pes_packets);

  return pes_packets;
}

}  // namespace mp2t
}  // namespace media
