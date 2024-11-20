// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_ES_PARSER_TEST_BASE_H_
#define MEDIA_FORMATS_MP2T_ES_PARSER_TEST_BASE_H_

#include <stddef.h>
#include <stdint.h>

#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/strings/cstring_view.h"
#include "base/time/time.h"
#include "media/base/timestamp_constants.h"

namespace media {
class AudioDecoderConfig;
class StreamParserBuffer;
class VideoDecoderConfig;

namespace mp2t {
class EsParser;

class EsParserTestBase {
 public:
  struct Packet {
    Packet();

    // Offset in the stream.
    size_t offset = 0;

    // Size of the packet.
    size_t size = 0;

    // Timestamp of the packet.
    base::TimeDelta pts = kNoTimestamp;
  };

  EsParserTestBase();

  EsParserTestBase(const EsParserTestBase&) = delete;
  EsParserTestBase& operator=(const EsParserTestBase&) = delete;

  virtual ~EsParserTestBase();

 protected:
  void LoadStream(const char* filename);
  std::vector<Packet> LoadPacketsFromFiles(base::cstring_view prefix,
                                           size_t num);

  // ES parser callbacks.
  void NewAudioConfig(const AudioDecoderConfig& config);
  void NewVideoConfig(const VideoDecoderConfig& config);
  void EmitBuffer(scoped_refptr<StreamParserBuffer> buffer);

  // Processes the PES packets using the given ES parser. When `force_timing` is
  // true, even invalid (negative) timestamps will be given to the ES parser.
  // Returns whether processing succeeded.
  bool ProcessPesPackets(EsParser* es_parser,
                         const std::vector<Packet>& pes_packets,
                         bool force_timing);

  // Computes the size of each packet, assuming all offsets are known. Packets
  // are assumed to be in stream order and the last packet is assumed to cover
  // the end of the stream.
  void ComputePacketSize(std::vector<Packet>* packets);

  // Generates some fixed size PES packets of `stream_`.
  std::vector<Packet> GenerateFixedSizePesPacket(size_t pes_size);

  // ES stream.
  std::vector<uint8_t> stream_;

  // Number of decoder configs received from the ES parser.
  size_t config_count_ = 0;

  // Number of buffers generated while parsing the ES stream.
  size_t buffer_count_ = 0;

  // Timestamps of buffers generated while parsing the ES stream.
  std::string buffer_timestamps_;

 private:
  // Timestamps of buffers generated while parsing the ES stream.
  std::stringstream buffer_timestamps_stream_;
};

}  // namespace mp2t
}  // namespace media

#endif  // MEDIA_FORMATS_MP2T_ES_PARSER_TEST_BASE_H_
