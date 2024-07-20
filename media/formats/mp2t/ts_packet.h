// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_TS_PACKET_H_
#define MEDIA_FORMATS_MP2T_TS_PACKET_H_

#include <stdint.h>

#include "base/containers/span.h"

namespace media {

class BitReader;

namespace mp2t {

class TsPacket {
 public:
  static const size_t kPacketSize = 188;

  // Return the number of bytes to discard
  // to be synchronized on a TS syncword.
  static int Sync(const uint8_t* buf, int size);

  // Parse a TS packet.
  // Return a TsPacket only when parsing was successful.
  // Return NULL otherwise.
  static TsPacket* Parse(const uint8_t* buf, size_t size);

  TsPacket(const TsPacket&) = delete;
  TsPacket& operator=(const TsPacket&) = delete;

  ~TsPacket();

  // TS header accessors.
  bool payload_unit_start_indicator() const {
    return payload_unit_start_indicator_;
  }
  int pid() const { return pid_; }
  int continuity_counter() const { return continuity_counter_; }
  bool discontinuity_indicator() const { return discontinuity_indicator_; }
  bool random_access_indicator() const { return random_access_indicator_; }

  // Return the offset and the size of the payload.
  const base::span<const uint8_t> payload() const { return payload_; }

 private:
  TsPacket();

  // Parse an Mpeg2 TS header.
  // The buffer size should be at least |kPacketSize|
  bool ParseHeader(const uint8_t* buf);
  bool ParseAdaptationField(BitReader* bit_reader,
                            int adaptation_field_length);

  base::span<const uint8_t> payload_;

  // TS header.
  bool payload_unit_start_indicator_;
  int pid_;
  int continuity_counter_;

  // Params from the adaptation field.
  bool discontinuity_indicator_;
  bool random_access_indicator_;
};

}  // namespace mp2t
}  // namespace media

#endif  // MEDIA_FORMATS_MP2T_TS_PACKET_H_
