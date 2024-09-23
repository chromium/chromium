// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/mp2t/ts_section_pes.h"

#include <memory>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/bit_reader.h"
#include "media/base/byte_queue.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/timestamp_constants.h"
#include "media/formats/mp2t/es_parser.h"
#include "media/formats/mp2t/mp2t_common.h"
#include "media/formats/mp2t/timestamp_unroller.h"

static const int kPesStartCode = 0x000001;

static bool IsTimestampSectionValid(int64_t timestamp_section) {
  // |pts_section| has 40 bits:
  // - starting with either '0010' or '0011' or '0001'
  // - and ending with a marker bit.
  // See ITU H.222 standard - PES section.

  // Verify that all the marker bits are set to one.
  return ((timestamp_section & 0x1) != 0) &&
         ((timestamp_section & 0x10000) != 0) &&
         ((timestamp_section & 0x100000000) != 0);
}

static int64_t ConvertTimestampSectionToTimestamp(int64_t timestamp_section) {
  return (((timestamp_section >> 33) & 0x7) << 30) |
         (((timestamp_section >> 17) & 0x7fff) << 15) |
         (((timestamp_section >> 1) & 0x7fff) << 0);
}

namespace media {
namespace mp2t {

TsSectionPes::TsSectionPes(std::unique_ptr<EsParser> es_parser,
                           TimestampUnroller* timestamp_unroller)
    : es_parser_(es_parser.release()),
      wait_for_pusi_(true),
      timestamp_unroller_(timestamp_unroller) {
  DCHECK(es_parser_);
  DCHECK(timestamp_unroller_);
}

TsSectionPes::~TsSectionPes() {
}

bool TsSectionPes::Parse(bool payload_unit_start_indicator,
                         base::span<const uint8_t> buf) {
  // Ignore partial PES.
  if (wait_for_pusi_ && !payload_unit_start_indicator)
    return true;

  bool parse_result = true;
  if (payload_unit_start_indicator) {
    // Try emitting a packet since we might have a pending PES packet
    // with an undefined size.
    // In this case, a unit is emitted when the next unit is coming.
    int raw_pes_size;
    const uint8_t* raw_pes;
    pes_byte_queue_.Peek(&raw_pes, &raw_pes_size);
    if (raw_pes_size > 0)
      parse_result = Emit(true);

    // Reset the state.
    ResetPesState();

    // Update the state.
    wait_for_pusi_ = false;
  }

  // Add the data to the parser state.
  if (!buf.empty()) {
    RCHECK(pes_byte_queue_.Push(buf));
  }

  // Try emitting the current PES packet.
  return (parse_result && Emit(false));
}

void TsSectionPes::Flush() {
  // Try emitting a packet since we might have a pending PES packet
  // with an undefined size.
  Emit(true);

  // Flush the underlying ES parser.
  es_parser_->Flush();
}

void TsSectionPes::Reset() {
  ResetPesState();
  es_parser_->Reset();
}

bool TsSectionPes::Emit(bool emit_for_unknown_size) {
  int raw_pes_size;
  const uint8_t* raw_pes;
  pes_byte_queue_.Peek(&raw_pes, &raw_pes_size);

  // A PES should be at least 6 bytes.
  // Wait for more data to come if not enough bytes.
  if (raw_pes_size < 6)
    return true;

  // Check whether we have enough data to start parsing.
  int pes_packet_length =
      (static_cast<int>(raw_pes[4]) << 8) |
      (static_cast<int>(raw_pes[5]));
  if ((pes_packet_length == 0 && !emit_for_unknown_size) ||
      (pes_packet_length != 0 && raw_pes_size < pes_packet_length + 6)) {
    // Wait for more data to come either because:
    // - there are not enough bytes,
    // - or the PES size is unknown and the "force emit" flag is not set.
    //   (PES size might be unknown for video PES packet).
    return true;
  }
  DVLOG(LOG_LEVEL_PES) << "pes_packet_length=" << pes_packet_length;

  // Parse the packet.
  bool parse_result = ParseInternal(raw_pes, raw_pes_size);

  // Reset the state.
  ResetPesState();

  return parse_result;
}

bool TsSectionPes::ParseInternal(const uint8_t* raw_pes, int raw_pes_size) {
  BitReader bit_reader(raw_pes, raw_pes_size);

  // Read up to the pes_packet_length (6 bytes).
  int packet_start_code_prefix;
  int stream_id;
  int pes_packet_length;
  RCHECK(bit_reader.ReadBits(24, &packet_start_code_prefix));
  RCHECK(bit_reader.ReadBits(8, &stream_id));
  RCHECK(bit_reader.ReadBits(16, &pes_packet_length));

  RCHECK(packet_start_code_prefix == kPesStartCode);
  DVLOG(LOG_LEVEL_PES) << "stream_id=" << std::hex << stream_id << std::dec;
  if (pes_packet_length == 0)
    pes_packet_length = bit_reader.bits_available() / 8;

  // Ignore the PES for unknown stream IDs.
  // See ITU H.222 Table 2-22 "Stream_id assignments"
  bool is_audio_stream_id = ((stream_id & 0xe0) == 0xc0);
  bool is_video_stream_id = ((stream_id & 0xf0) == 0xe0);
  // According to ETSI DVB standard (ETSI TS 101 154) section 4.1.6.1
  // AC-3 and DTS audio streams may have stream_id 0xbd. These streams
  // have the same syntax as regular audio streams.
  bool is_private_stream_1 = (stream_id == 0xbd);
  if (!is_audio_stream_id && !is_video_stream_id && !is_private_stream_1) {
    DVLOG(LOG_LEVEL_PES) << "Dropped TsPacket for stream_id=0x"
                         << std::hex << stream_id << std::dec;
    return true;
  }

  // Read up to "pes_header_data_length".
  int dummy_2;
  int PES_scrambling_control;
  int PES_priority;
  int data_alignment_indicator;
  int copyright;
  int original_or_copy;
  int pts_dts_flags;
  int escr_flag;
  int es_rate_flag;
  int dsm_trick_mode_flag;
  int additional_copy_info_flag;
  int pes_crc_flag;
  int pes_extension_flag;
  int pes_header_data_length;
  RCHECK(bit_reader.ReadBits(2, &dummy_2));
  RCHECK(dummy_2 == 0x2);
  RCHECK(bit_reader.ReadBits(2, &PES_scrambling_control));
  RCHECK(bit_reader.ReadBits(1, &PES_priority));
  RCHECK(bit_reader.ReadBits(1, &data_alignment_indicator));
  RCHECK(bit_reader.ReadBits(1, &copyright));
  RCHECK(bit_reader.ReadBits(1, &original_or_copy));
  RCHECK(bit_reader.ReadBits(2, &pts_dts_flags));
  RCHECK(bit_reader.ReadBits(1, &escr_flag));
  RCHECK(bit_reader.ReadBits(1, &es_rate_flag));
  RCHECK(bit_reader.ReadBits(1, &dsm_trick_mode_flag));
  RCHECK(bit_reader.ReadBits(1, &additional_copy_info_flag));
  RCHECK(bit_reader.ReadBits(1, &pes_crc_flag));
  RCHECK(bit_reader.ReadBits(1, &pes_extension_flag));
  RCHECK(bit_reader.ReadBits(8, &pes_header_data_length));
  int pes_header_start_size = bit_reader.bits_available() / 8;

  // Compute the size and the offset of the ES payload.
  // "6" for the 6 bytes read before and including |pes_packet_length|.
  // "3" for the 3 bytes read before and including |pes_header_data_length|.
  int es_size = pes_packet_length - 3 - pes_header_data_length;
  int es_offset = 6 + 3 + pes_header_data_length;
  RCHECK(es_size >= 0);
  RCHECK(es_offset + es_size <= raw_pes_size);

  // Read the timing information section.
  bool is_pts_valid = false;
  bool is_dts_valid = false;
  int64_t pts_section = 0;
  int64_t dts_section = 0;
  if (pts_dts_flags == 0x2) {
    RCHECK(bit_reader.ReadBits(40, &pts_section));
    RCHECK((((pts_section >> 36) & 0xf) == 0x2) &&
           IsTimestampSectionValid(pts_section));
    is_pts_valid = true;
  }
  if (pts_dts_flags == 0x3) {
    RCHECK(bit_reader.ReadBits(40, &pts_section));
    RCHECK(bit_reader.ReadBits(40, &dts_section));
    RCHECK((((pts_section >> 36) & 0xf) == 0x3) &&
           IsTimestampSectionValid(pts_section));
    RCHECK((((dts_section >> 36) & 0xf) == 0x1) &&
           IsTimestampSectionValid(dts_section));
    is_pts_valid = true;
    is_dts_valid = true;
  }

  // Convert and unroll the timestamps.
  base::TimeDelta media_pts(kNoTimestamp);
  DecodeTimestamp media_dts(kNoDecodeTimestamp);
  if (is_pts_valid) {
    int64_t pts = timestamp_unroller_->GetUnrolledTimestamp(
        ConvertTimestampSectionToTimestamp(pts_section));
    media_pts = base::Microseconds((1000 * pts) / 90);
  }
  if (is_dts_valid) {
    int64_t dts = timestamp_unroller_->GetUnrolledTimestamp(
        ConvertTimestampSectionToTimestamp(dts_section));
    media_dts = DecodeTimestamp::FromMicroseconds((1000 * dts) / 90);
  }

  // Discard the rest of the PES packet header.
  // TODO(damienv): check if some info of the PES packet header are useful.
  DCHECK_EQ(bit_reader.bits_available() % 8, 0);
  int pes_header_remaining_size = pes_header_data_length -
      (pes_header_start_size - bit_reader.bits_available() / 8);
  RCHECK(pes_header_remaining_size >= 0);

  // Read the PES packet.
  DVLOG(LOG_LEVEL_PES)
      << "Emit a reassembled PES:"
      << " size=" << es_size
      << " pts=" << media_pts.InMilliseconds()
      << " dts=" << media_dts.InMilliseconds()
      << " data_alignment_indicator=" << data_alignment_indicator;
  return es_parser_->Parse(&raw_pes[es_offset], es_size, media_pts, media_dts);
}

void TsSectionPes::ResetPesState() {
  pes_byte_queue_.Reset();
  wait_for_pusi_ = true;
}

}  // namespace mp2t
}  // namespace media
