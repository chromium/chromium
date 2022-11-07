// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_ES_PARSER_TEST_BASE_H_
#define MEDIA_FORMATS_MP2T_ES_PARSER_TEST_BASE_H_

#include <stddef.h>
#include <stdint.h>

#include <sstream>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"

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
    size_t offset;

    // Size of the packet.
    size_t size;

    // Timestamp of the packet.
    base::TimeDelta pts;
  };

  EsParserTestBase();

  EsParserTestBase(const EsParserTestBase&) = delete;
  EsParserTestBase& operator=(const EsParserTestBase&) = delete;

  virtual ~EsParserTestBase();

 protected:
  void LoadStream(const char* filename);
  std::vector<Packet> LoadPacketsFromFiles(const char* file_temp, size_t num);

  // ES parser callbacks.
  void NewAudioConfig(const AudioDecoderConfig& config);
  void NewVideoConfig(const VideoDecoderConfig& config);
  void EmitBuffer(scoped_refptr<StreamParserBuffer> buffer);

  // Process the PES packets using the given ES parser.
  // When |force_timing| is true, even the invalid negative timestamps will be
  // given to the ES parser.
  // Return true if successful, false otherwise.
  bool ProcessPesPackets(EsParser* es_parser,
                         const std::vector<Packet>& pes_packets,
                         bool force_timing);

  // Assume the offsets are known, compute the size of each packet.
  // The last packet is assumed to cover the end of the stream.
  // Packets are assumed to be in stream order.
  void ComputePacketSize(std::vector<Packet>* packets);

  // Generate some fixed size PES packets of |stream_|.
  std::vector<Packet> GenerateFixedSizePesPacket(size_t pes_size);

  // ES stream.
  std::vector<uint8_t> stream_;

  // Number of decoder configs received from the ES parser.
  size_t config_count_;

  // Number of buffers generated while parsing the ES stream.
  size_t buffer_count_;

  // Timestamps of buffers generated while parsing the ES stream.
  std::string buffer_timestamps_;

 private:
  // Timestamps of buffers generated while parsing the ES stream.
  std::stringstream buffer_timestamps_stream_;
};

}  // namespace mp2t
}  // namespace media

#endif  // MEDIA_FORMATS_MP2T_ES_PARSER_TEST_BASE_H_
