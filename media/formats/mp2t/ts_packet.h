// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_TS_PACKET_H_
#define MEDIA_FORMATS_MP2T_TS_PACKET_H_

#include <stdint.h>

#include <memory>

#include "base/containers/span.h"
#include "base/memory/raw_span.h"

namespace media {

class BitReader;

namespace mp2t {

class TsPacket {
 public:
  static constexpr size_t kPacketSize = 188;

  // Return the number of bytes to discard
  // to be synchronized on a TS syncword.
  static size_t Sync(base::span<const uint8_t> buf);

  // Parse a TS packet.
  // Return a TsPacket only when parsing was successful.
  // Return NULL otherwise.
  static std::unique_ptr<TsPacket> Parse(base::span<const uint8_t> buf);

  TsPacket(const TsPacket&) = delete;
  TsPacket& operator=(const TsPacket&) = delete;

  ~TsPacket();

  // TS header accessors.
  bool payload_unit_start_indicator() const {
    return payload_unit_start_indicator_;
  }
  uint32_t pid() const { return pid_; }
  uint32_t continuity_counter() const { return continuity_counter_; }
  bool discontinuity_indicator() const { return discontinuity_indicator_; }
  bool random_access_indicator() const { return random_access_indicator_; }

  // Return the offset and the size of the payload.
  const base::span<const uint8_t> payload() const { return payload_; }

 private:
  TsPacket();

  // Parse an Mpeg2 TS header.
  // The buffer size should be at least |kPacketSize|
  bool ParseHeader(base::span<const uint8_t> buf);
  bool ParseAdaptationField(BitReader* bit_reader,
                            size_t adaptation_field_length);

  base::raw_span<const uint8_t> payload_;

  // TS header.
  bool payload_unit_start_indicator_;
  uint32_t pid_;
  uint32_t continuity_counter_;

  // Params from the adaptation field.
  bool discontinuity_indicator_;
  bool random_access_indicator_;
};

}  // namespace mp2t
}  // namespace media

#endif  // MEDIA_FORMATS_MP2T_TS_PACKET_H_
