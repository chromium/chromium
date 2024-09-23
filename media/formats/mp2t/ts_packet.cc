// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/mp2t/ts_packet.h"

#include <memory>

#include "base/logging.h"
#include "media/base/bit_reader.h"
#include "media/formats/mp2t/mp2t_common.h"

namespace media {
namespace mp2t {

static const uint8_t kTsHeaderSyncword = 0x47;

// static
int TsPacket::Sync(const uint8_t* buf, int size) {
  int k = 0;
  for (; k < size; k++) {
    // Verify that we have 4 syncwords in a row when possible,
    // this should improve synchronization robustness.
    // TODO(damienv): Consider the case where there is garbage
    // between TS packets.
    bool is_header = true;
    for (int i = 0; i < 4; i++) {
      int idx = k + i * kPacketSize;
      if (idx >= size)
        break;
      if (buf[idx] != kTsHeaderSyncword) {
        DVLOG(LOG_LEVEL_TS)
            << "ByteSync" << idx << ": "
            << std::hex << static_cast<int>(buf[idx]) << std::dec;
        is_header = false;
        break;
      }
    }
    if (is_header)
      break;
  }

  DVLOG_IF(1, k != 0) << "SYNC: nbytes_skipped=" << k;
  return k;
}

// static
TsPacket* TsPacket::Parse(const uint8_t* buf, size_t size) {
  if (size < kPacketSize) {
    DVLOG(1) << "Buffer does not hold one full TS packet:"
             << " buffer_size=" << size;
    return NULL;
  }

  DCHECK_EQ(buf[0], kTsHeaderSyncword);
  if (buf[0] != kTsHeaderSyncword) {
    DVLOG(1) << "Not on a TS syncword:"
             << " buf[0]="
             << std::hex << static_cast<int>(buf[0]) << std::dec;
    return NULL;
  }

  std::unique_ptr<TsPacket> ts_packet(new TsPacket());
  bool status = ts_packet->ParseHeader(buf);
  if (!status) {
    DVLOG(1) << "Parsing header failed";
    return NULL;
  }
  return ts_packet.release();
}

TsPacket::TsPacket() {
}

TsPacket::~TsPacket() {
}

bool TsPacket::ParseHeader(const uint8_t* buf) {
  BitReader bit_reader(buf, kPacketSize);
  payload_ = {buf, kPacketSize};

  // Read the TS header: 4 bytes.
  int syncword;
  int transport_error_indicator;
  int payload_unit_start_indicator;
  int transport_priority;
  int transport_scrambling_control;
  int adaptation_field_control;
  RCHECK(bit_reader.ReadBits(8, &syncword));
  RCHECK(bit_reader.ReadBits(1, &transport_error_indicator));
  RCHECK(bit_reader.ReadBits(1, &payload_unit_start_indicator));
  RCHECK(bit_reader.ReadBits(1, &transport_priority));
  RCHECK(bit_reader.ReadBits(13, &pid_));
  RCHECK(bit_reader.ReadBits(2, &transport_scrambling_control));
  RCHECK(bit_reader.ReadBits(2, &adaptation_field_control));
  RCHECK(bit_reader.ReadBits(4, &continuity_counter_));
  payload_unit_start_indicator_ = (payload_unit_start_indicator != 0);
  payload_ = payload_.subspan(4);

  // Default values when no adaptation field.
  discontinuity_indicator_ = false;
  random_access_indicator_ = false;

  // Done since no adaptation field.
  if ((adaptation_field_control & 0x2) == 0)
    return true;

  // Read the adaptation field if needed.
  int adaptation_field_length;
  RCHECK(bit_reader.ReadBits(8, &adaptation_field_length));
  DVLOG(LOG_LEVEL_TS) << "adaptation_field_length=" << adaptation_field_length;
  payload_ = payload_.subspan(1);
  if ((adaptation_field_control & 0x1) == 0 &&
       adaptation_field_length != 183) {
    DVLOG(1) << "adaptation_field_length=" << adaptation_field_length;
    return false;
  }
  if ((adaptation_field_control & 0x1) == 1 &&
       adaptation_field_length > 182) {
    DVLOG(1) << "adaptation_field_length=" << adaptation_field_length;
    // This is not allowed by the spec.
    // However, some badly encoded streams are using
    // adaptation_field_length = 183
    return false;
  }

  // adaptation_field_length = '0' is used to insert a single stuffing byte
  // in the adaptation field of a transport stream packet.
  if (adaptation_field_length == 0)
    return true;

  bool status = ParseAdaptationField(&bit_reader, adaptation_field_length);
  payload_ = payload_.subspan(adaptation_field_length);
  return status;
}

bool TsPacket::ParseAdaptationField(BitReader* bit_reader,
                                    int adaptation_field_length) {
  DCHECK_GT(adaptation_field_length, 0);
  int adaptation_field_start_marker = bit_reader->bits_available() / 8;

  int discontinuity_indicator;
  int random_access_indicator;
  int elementary_stream_priority_indicator;
  int pcr_flag;
  int opcr_flag;
  int splicing_point_flag;
  int transport_private_data_flag;
  int adaptation_field_extension_flag;
  RCHECK(bit_reader->ReadBits(1, &discontinuity_indicator));
  RCHECK(bit_reader->ReadBits(1, &random_access_indicator));
  RCHECK(bit_reader->ReadBits(1, &elementary_stream_priority_indicator));
  RCHECK(bit_reader->ReadBits(1, &pcr_flag));
  RCHECK(bit_reader->ReadBits(1, &opcr_flag));
  RCHECK(bit_reader->ReadBits(1, &splicing_point_flag));
  RCHECK(bit_reader->ReadBits(1, &transport_private_data_flag));
  RCHECK(bit_reader->ReadBits(1, &adaptation_field_extension_flag));
  discontinuity_indicator_ = (discontinuity_indicator != 0);
  random_access_indicator_ = (random_access_indicator != 0);

  if (pcr_flag) {
    int64_t program_clock_reference_base;
    int reserved;
    int program_clock_reference_extension;
    RCHECK(bit_reader->ReadBits(33, &program_clock_reference_base));
    RCHECK(bit_reader->ReadBits(6, &reserved));
    RCHECK(bit_reader->ReadBits(9, &program_clock_reference_extension));
  }

  if (opcr_flag) {
    int64_t original_program_clock_reference_base;
    int reserved;
    int original_program_clock_reference_extension;
    RCHECK(bit_reader->ReadBits(33, &original_program_clock_reference_base));
    RCHECK(bit_reader->ReadBits(6, &reserved));
    RCHECK(
        bit_reader->ReadBits(9, &original_program_clock_reference_extension));
  }

  if (splicing_point_flag) {
    int splice_countdown;
    RCHECK(bit_reader->ReadBits(8, &splice_countdown));
  }

  if (transport_private_data_flag) {
    int transport_private_data_length;
    RCHECK(bit_reader->ReadBits(8, &transport_private_data_length));
    RCHECK(bit_reader->SkipBits(8 * transport_private_data_length));
  }

  if (adaptation_field_extension_flag) {
    int adaptation_field_extension_length;
    RCHECK(bit_reader->ReadBits(8, &adaptation_field_extension_length));
    RCHECK(bit_reader->SkipBits(8 * adaptation_field_extension_length));
  }

  // The rest of the adaptation field should be stuffing bytes.
  int adaptation_field_remaining_size = adaptation_field_length -
      (adaptation_field_start_marker - bit_reader->bits_available() / 8);
  RCHECK(adaptation_field_remaining_size >= 0);
  for (int k = 0; k < adaptation_field_remaining_size; k++) {
    int stuffing_byte;
    RCHECK(bit_reader->ReadBits(8, &stuffing_byte));
    // Unfortunately, a lot of streams exist in the field that do not fill
    // the remaining of the adaptation field with the expected stuffing value:
    // do not fail if that's the case.
    DVLOG_IF(1, stuffing_byte != 0xff)
        << "Stream not compliant: invalid stuffing byte "
        << std::hex << stuffing_byte;
  }

  DVLOG(LOG_LEVEL_TS) << "random_access_indicator=" << random_access_indicator_;
  return true;
}

}  // namespace mp2t
}  // namespace media

