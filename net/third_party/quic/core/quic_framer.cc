// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_framer.h"

#include <cstdint>
#include <memory>

#include "base/compiler_specific.h"
#include "net/third_party/quic/core/crypto/crypto_framer.h"
#include "net/third_party/quic/core/crypto/crypto_handshake_message.h"
#include "net/third_party/quic/core/crypto/crypto_protocol.h"
#include "net/third_party/quic/core/crypto/null_decrypter.h"
#include "net/third_party/quic/core/crypto/null_encrypter.h"
#include "net/third_party/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quic/core/crypto/quic_random.h"
#include "net/third_party/quic/core/quic_data_reader.h"
#include "net/third_party/quic/core/quic_data_writer.h"
#include "net/third_party/quic/core/quic_socket_address_coder.h"
#include "net/third_party/quic/core/quic_stream_frame_data_producer.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/platform/api/quic_aligned.h"
#include "net/third_party/quic/platform/api/quic_bug_tracker.h"
#include "net/third_party/quic/platform/api/quic_fallthrough.h"
#include "net/third_party/quic/platform/api/quic_flag_utils.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_map_util.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_str_cat.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_text_utils.h"

namespace quic {

namespace {

#define ENDPOINT \
  (perspective_ == Perspective::IS_SERVER ? "Server: " : "Client: ")

// How much to shift the timestamp in the IETF Ack frame.
// TODO(fkastenholz) when we get real IETF QUIC, need to get
// the currect shift from the transport parameters.
const int kIetfAckTimestampShift = 3;

// Number of bits the packet number length bits are shifted from the right
// edge of the header.
const uint8_t kPublicHeaderSequenceNumberShift = 4;

// There are two interpretations for the Frame Type byte in the QUIC protocol,
// resulting in two Frame Types: Special Frame Types and Regular Frame Types.
//
// Regular Frame Types use the Frame Type byte simply. Currently defined
// Regular Frame Types are:
// Padding            : 0b 00000000 (0x00)
// ResetStream        : 0b 00000001 (0x01)
// ConnectionClose    : 0b 00000010 (0x02)
// GoAway             : 0b 00000011 (0x03)
// WindowUpdate       : 0b 00000100 (0x04)
// Blocked            : 0b 00000101 (0x05)
//
// Special Frame Types encode both a Frame Type and corresponding flags
// all in the Frame Type byte. Currently defined Special Frame Types
// are:
// Stream             : 0b 1xxxxxxx
// Ack                : 0b 01xxxxxx
//
// Semantics of the flag bits above (the x bits) depends on the frame type.

// Masks to determine if the frame type is a special use
// and for specific special frame types.
const uint8_t kQuicFrameTypeBrokenMask = 0xE0;   // 0b 11100000
const uint8_t kQuicFrameTypeSpecialMask = 0xC0;  // 0b 11000000
const uint8_t kQuicFrameTypeStreamMask = 0x80;
const uint8_t kQuicFrameTypeAckMask = 0x40;
static_assert(kQuicFrameTypeSpecialMask ==
                  (kQuicFrameTypeStreamMask | kQuicFrameTypeAckMask),
              "Invalid kQuicFrameTypeSpecialMask");

// The stream type format is 1FDOOOSS, where
//    F is the fin bit.
//    D is the data length bit (0 or 2 bytes).
//    OO/OOO are the size of the offset.
//    SS is the size of the stream ID.
// Note that the stream encoding can not be determined by inspection. It can
// be determined only by knowing the QUIC Version.
// Stream frame relative shifts and masks for interpreting the stream flags.
// StreamID may be 1, 2, 3, or 4 bytes.
const uint8_t kQuicStreamIdShift = 2;
const uint8_t kQuicStreamIDLengthMask = 0x03;

// Offset may be 0, 2, 4, or 8 bytes.
const uint8_t kQuicStreamShift = 3;
const uint8_t kQuicStreamOffsetMask = 0x07;

// Data length may be 0 or 2 bytes.
const uint8_t kQuicStreamDataLengthShift = 1;
const uint8_t kQuicStreamDataLengthMask = 0x01;

// Fin bit may be set or not.
const uint8_t kQuicStreamFinShift = 1;
const uint8_t kQuicStreamFinMask = 0x01;

// The format is 01M0LLOO, where
//   M if set, there are multiple ack blocks in the frame.
//  LL is the size of the largest ack field.
//  OO is the size of the ack blocks offset field.
// packet number size shift used in AckFrames.
const uint8_t kQuicSequenceNumberLengthNumBits = 2;
const uint8_t kActBlockLengthOffset = 0;
const uint8_t kLargestAckedOffset = 2;

// Acks may have only one ack block.
const uint8_t kQuicHasMultipleAckBlocksOffset = 5;

// Timestamps are 4 bytes followed by 2 bytes.
const uint8_t kQuicNumTimestampsLength = 1;
const uint8_t kQuicFirstTimestampLength = 4;
const uint8_t kQuicTimestampLength = 2;
// Gaps between packet numbers are 1 byte.
const uint8_t kQuicTimestampPacketNumberGapLength = 1;

// Maximum length of encoded error strings.
const int kMaxErrorStringLength = 256;

const uint8_t kQuicLongHeaderTypeMask = 0x7F;
const uint8_t kQuicShortHeaderTypeMask = 0x07;

const uint8_t kConnectionIdLengthAdjustment = 3;
const uint8_t kDestinationConnectionIdLengthMask = 0xF0;
const uint8_t kSourceConnectionIdLengthMask = 0x0F;

// Returns the absolute value of the difference between |a| and |b|.
QuicPacketNumber Delta(QuicPacketNumber a, QuicPacketNumber b) {
  // Since these are unsigned numbers, we can't just return abs(a - b)
  if (a < b) {
    return b - a;
  }
  return a - b;
}

QuicPacketNumber ClosestTo(QuicPacketNumber target,
                           QuicPacketNumber a,
                           QuicPacketNumber b) {
  return (Delta(target, a) < Delta(target, b)) ? a : b;
}

QuicPacketNumberLength ReadSequenceNumberLength(uint8_t flags) {
  switch (flags & PACKET_FLAGS_8BYTE_PACKET) {
    case PACKET_FLAGS_8BYTE_PACKET:
      return PACKET_6BYTE_PACKET_NUMBER;
    case PACKET_FLAGS_4BYTE_PACKET:
      return PACKET_4BYTE_PACKET_NUMBER;
    case PACKET_FLAGS_2BYTE_PACKET:
      return PACKET_2BYTE_PACKET_NUMBER;
    case PACKET_FLAGS_1BYTE_PACKET:
      return PACKET_1BYTE_PACKET_NUMBER;
    default:
      QUIC_BUG << "Unreachable case statement.";
      return PACKET_6BYTE_PACKET_NUMBER;
  }
}

QuicPacketNumberLength ReadAckPacketNumberLength(QuicTransportVersion version,
                                                 uint8_t flags) {
  switch (flags & PACKET_FLAGS_8BYTE_PACKET) {
    case PACKET_FLAGS_8BYTE_PACKET:
      return PACKET_6BYTE_PACKET_NUMBER;
    case PACKET_FLAGS_4BYTE_PACKET:
      return PACKET_4BYTE_PACKET_NUMBER;
    case PACKET_FLAGS_2BYTE_PACKET:
      return PACKET_2BYTE_PACKET_NUMBER;
    case PACKET_FLAGS_1BYTE_PACKET:
      return PACKET_1BYTE_PACKET_NUMBER;
    default:
      QUIC_BUG << "Unreachable case statement.";
      return PACKET_6BYTE_PACKET_NUMBER;
  }
}

QuicShortHeaderType PacketNumberLengthToShortHeaderType(
    QuicPacketNumberLength packet_number_length) {
  switch (packet_number_length) {
    case PACKET_1BYTE_PACKET_NUMBER:
      return SHORT_HEADER_1_BYTE_PACKET_NUMBER;
    case PACKET_2BYTE_PACKET_NUMBER:
      return SHORT_HEADER_2_BYTE_PACKET_NUMBER;
    case PACKET_4BYTE_PACKET_NUMBER:
      return SHORT_HEADER_4_BYTE_PACKET_NUMBER;
    default:
      QUIC_BUG << "Invalid packet number length for short header.";
      return SHORT_HEADER_1_BYTE_PACKET_NUMBER;
  }
}

QuicPacketNumberLength ShortHeaderTypeToPacketNumberLength(
    QuicShortHeaderType short_header_type) {
  switch (short_header_type) {
    case SHORT_HEADER_1_BYTE_PACKET_NUMBER:
      return PACKET_1BYTE_PACKET_NUMBER;
    case SHORT_HEADER_2_BYTE_PACKET_NUMBER:
      return PACKET_2BYTE_PACKET_NUMBER;
    case SHORT_HEADER_4_BYTE_PACKET_NUMBER:
      return PACKET_4BYTE_PACKET_NUMBER;
    default:
      QUIC_BUG << "Unreachable case statement.";
      return PACKET_6BYTE_PACKET_NUMBER;
  }
}

QuicStringPiece TruncateErrorString(QuicStringPiece error) {
  if (error.length() <= kMaxErrorStringLength) {
    return error;
  }
  return QuicStringPiece(error.data(), kMaxErrorStringLength);
}

size_t TruncatedErrorStringSize(const QuicStringPiece& error) {
  if (error.length() < kMaxErrorStringLength) {
    return error.length();
  }
  return kMaxErrorStringLength;
}

uint8_t GetConnectionIdLengthValue(QuicConnectionIdLength length) {
  if (length == 0) {
    return 0;
  }
  return static_cast<uint8_t>(length - kConnectionIdLengthAdjustment);
}

}  // namespace

QuicFramer::QuicFramer(const ParsedQuicVersionVector& supported_versions,
                       QuicTime creation_time,
                       Perspective perspective)
    : visitor_(nullptr),
      error_(QUIC_NO_ERROR),
      largest_packet_number_(0),
      last_serialized_connection_id_(0),
      last_version_label_(0),
      last_packet_is_ietf_quic_(false),
      last_header_form_(LONG_HEADER),
      version_(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
      supported_versions_(supported_versions),
      decrypter_level_(ENCRYPTION_NONE),
      alternative_decrypter_level_(ENCRYPTION_NONE),
      alternative_decrypter_latch_(false),
      perspective_(perspective),
      validate_flags_(true),
      process_timestamps_(false),
      creation_time_(creation_time),
      last_timestamp_(QuicTime::Delta::Zero()),
      data_producer_(nullptr),
      process_stateless_reset_at_client_only_(
          GetQuicReloadableFlag(quic_process_stateless_reset_at_client_only)) {
  DCHECK(!supported_versions.empty());
  version_ = supported_versions_[0];
  decrypter_ = QuicMakeUnique<NullDecrypter>(perspective);
  encrypter_[ENCRYPTION_NONE] = QuicMakeUnique<NullEncrypter>(perspective);
}

QuicFramer::~QuicFramer() {}

// static
size_t QuicFramer::GetMinStreamFrameSize(QuicTransportVersion version,
                                         QuicStreamId stream_id,
                                         QuicStreamOffset offset,
                                         bool last_frame_in_packet,
                                         QuicPacketLength data_length) {
  if (version == QUIC_VERSION_99) {
    return kQuicFrameTypeSize + QuicDataWriter::GetVarInt62Len(stream_id) +
           (last_frame_in_packet
                ? 0
                : QuicDataWriter::GetVarInt62Len(data_length)) +
           (offset != 0 ? QuicDataWriter::GetVarInt62Len(offset) : 0);
  }
  return kQuicFrameTypeSize + GetStreamIdSize(stream_id) +
         GetStreamOffsetSize(version, offset) +
         (last_frame_in_packet ? 0 : kQuicStreamPayloadLengthSize);
}

// static
size_t QuicFramer::GetMessageFrameSize(QuicTransportVersion version,
                                       bool last_frame_in_packet,
                                       QuicByteCount length) {
  QUIC_BUG_IF(version <= QUIC_VERSION_44)
      << "Try to serialize MESSAGE frame in " << version;
  return kQuicFrameTypeSize +
         (last_frame_in_packet ? 0 : QuicDataWriter::GetVarInt62Len(length)) +
         length;
}

// static
size_t QuicFramer::GetMinAckFrameSize(
    QuicTransportVersion version,
    QuicPacketNumberLength largest_observed_length) {
  if (version == QUIC_VERSION_99) {
    // The minimal ack frame consists of the following four fields: Largest
    // Acknowledged, ACK Delay, ACK Block Count, and First ACK Block. Minimum
    // size of each is 1 byte.
    return kQuicFrameTypeSize + 4;
  }
  size_t min_size = kQuicFrameTypeSize + largest_observed_length +
                    kQuicDeltaTimeLargestObservedSize;
  return min_size + kQuicNumTimestampsSize;
}

// static
size_t QuicFramer::GetStopWaitingFrameSize(
    QuicTransportVersion version,
    QuicPacketNumberLength packet_number_length) {
  size_t min_size = kQuicFrameTypeSize + packet_number_length;
  return min_size;
}

// static
size_t QuicFramer::GetRstStreamFrameSize(QuicTransportVersion version,
                                         const QuicRstStreamFrame& frame) {
  if (version == QUIC_VERSION_99) {
    return QuicDataWriter::GetVarInt62Len(frame.stream_id) +
           QuicDataWriter::GetVarInt62Len(frame.byte_offset) +
           kQuicFrameTypeSize + kQuicIetfQuicErrorCodeSize;
  }
  return kQuicFrameTypeSize + kQuicMaxStreamIdSize + kQuicMaxStreamOffsetSize +
         kQuicErrorCodeSize;
}

// static
size_t QuicFramer::GetMinConnectionCloseFrameSize(
    QuicTransportVersion version,
    const QuicConnectionCloseFrame& frame) {
  if (version == QUIC_VERSION_99) {
    return QuicDataWriter::GetVarInt62Len(
               TruncatedErrorStringSize(frame.error_details)) +
           QuicDataWriter::GetVarInt62Len(frame.frame_type) +
           kQuicFrameTypeSize + kQuicIetfQuicErrorCodeSize;
  }
  return kQuicFrameTypeSize + kQuicErrorCodeSize + kQuicErrorDetailsLengthSize;
}

// static
size_t QuicFramer::GetMinApplicationCloseFrameSize(
    QuicTransportVersion version,
    const QuicApplicationCloseFrame& frame) {
  if (version != QUIC_VERSION_99) {
    QUIC_BUG << "In version " << version
             << " - not 99 - and tried to serialize ApplicationClose.";
  }
  return QuicDataWriter::GetVarInt62Len(
             TruncatedErrorStringSize(frame.error_details)) +
         kQuicFrameTypeSize + kQuicIetfQuicErrorCodeSize;
}

// static
size_t QuicFramer::GetMinGoAwayFrameSize() {
  return kQuicFrameTypeSize + kQuicErrorCodeSize + kQuicErrorDetailsLengthSize +
         kQuicMaxStreamIdSize;
}

// static
size_t QuicFramer::GetWindowUpdateFrameSize(
    QuicTransportVersion version,
    const QuicWindowUpdateFrame& frame) {
  if (version != QUIC_VERSION_99) {
    return kQuicFrameTypeSize + kQuicMaxStreamIdSize + kQuicMaxStreamOffsetSize;
  }
  if (frame.stream_id == 0) {
    // Frame would be a MAX DATA frame, which has only a Maximum Data field.
    return kQuicFrameTypeSize +
           QuicDataWriter::GetVarInt62Len(frame.byte_offset);
  }
  // Frame would be MAX STREAM DATA, has Maximum Stream Data and Stream ID
  // fields.
  return kQuicFrameTypeSize +
         QuicDataWriter::GetVarInt62Len(frame.byte_offset) +
         QuicDataWriter::GetVarInt62Len(frame.stream_id);
}

// static
size_t QuicFramer::GetMaxStreamIdFrameSize(QuicTransportVersion version,
                                           const QuicMaxStreamIdFrame& frame) {
  if (version != QUIC_VERSION_99) {
    QUIC_BUG << "In version " << version
             << " - not 99 - and tried to serialize MaxStreamId Frame.";
  }
  return kQuicFrameTypeSize +
         QuicDataWriter::GetVarInt62Len(frame.max_stream_id);
}
// static
size_t QuicFramer::GetStreamIdBlockedFrameSize(
    QuicTransportVersion version,
    const QuicStreamIdBlockedFrame& frame) {
  if (version != QUIC_VERSION_99) {
    QUIC_BUG << "In version " << version
             << " - not 99 - and tried to serialize StreamIdBlocked Frame.";
  }
  return kQuicFrameTypeSize + QuicDataWriter::GetVarInt62Len(frame.stream_id);
}

// static
size_t QuicFramer::GetBlockedFrameSize(QuicTransportVersion version,
                                       const QuicBlockedFrame& frame) {
  if (version != QUIC_VERSION_99) {
    return kQuicFrameTypeSize + kQuicMaxStreamIdSize;
  }
  if (frame.stream_id == 0) {
    // return size of IETF QUIC Blocked frame
    return kQuicFrameTypeSize + QuicDataWriter::GetVarInt62Len(frame.offset);
  }
  // return size of IETF QUIC Stream Blocked frame.
  return kQuicFrameTypeSize + QuicDataWriter::GetVarInt62Len(frame.offset) +
         QuicDataWriter::GetVarInt62Len(frame.stream_id);
}

// static
size_t QuicFramer::GetStopSendingFrameSize(const QuicStopSendingFrame& frame) {
  return kQuicFrameTypeSize + QuicDataWriter::GetVarInt62Len(frame.stream_id) +
         sizeof(QuicApplicationErrorCode);
}

// static
size_t QuicFramer::GetPathChallengeFrameSize(
    const QuicPathChallengeFrame& frame) {
  return kQuicFrameTypeSize + sizeof(frame.data_buffer);
}

// static
size_t QuicFramer::GetPathResponseFrameSize(
    const QuicPathResponseFrame& frame) {
  return kQuicFrameTypeSize + sizeof(frame.data_buffer);
}

// static
size_t QuicFramer::GetRetransmittableControlFrameSize(
    QuicTransportVersion version,
    const QuicFrame& frame) {
  switch (frame.type) {
    case PING_FRAME:
      // Ping has no payload.
      return kQuicFrameTypeSize;
    case RST_STREAM_FRAME:
      return GetRstStreamFrameSize(version, *frame.rst_stream_frame);
    case CONNECTION_CLOSE_FRAME:
      return GetMinConnectionCloseFrameSize(version,
                                            *frame.connection_close_frame) +
             TruncatedErrorStringSize(
                 frame.connection_close_frame->error_details);
    case GOAWAY_FRAME:
      return GetMinGoAwayFrameSize() +
             TruncatedErrorStringSize(frame.goaway_frame->reason_phrase);
    case WINDOW_UPDATE_FRAME:
      // For version 99, this could be either a MAX DATA or MAX STREAM DATA.
      // GetWindowUpdateFrameSize figures this out and returns the correct
      // length.
      return GetWindowUpdateFrameSize(version, *frame.window_update_frame);
    case BLOCKED_FRAME:
      return GetBlockedFrameSize(version, *frame.blocked_frame);
    case APPLICATION_CLOSE_FRAME:
      return GetMinApplicationCloseFrameSize(version,
                                             *frame.application_close_frame) +
             TruncatedErrorStringSize(
                 frame.application_close_frame->error_details);
    case NEW_CONNECTION_ID_FRAME:
      return GetNewConnectionIdFrameSize(*frame.new_connection_id_frame);
    case RETIRE_CONNECTION_ID_FRAME:
      return GetRetireConnectionIdFrameSize(*frame.retire_connection_id_frame);
    case NEW_TOKEN_FRAME:
      return GetNewTokenFrameSize(*frame.new_token_frame);
    case MAX_STREAM_ID_FRAME:
      return GetMaxStreamIdFrameSize(version, frame.max_stream_id_frame);
    case STREAM_ID_BLOCKED_FRAME:
      return GetStreamIdBlockedFrameSize(version,
                                         frame.stream_id_blocked_frame);
    case PATH_RESPONSE_FRAME:
      return GetPathResponseFrameSize(*frame.path_response_frame);
    case PATH_CHALLENGE_FRAME:
      return GetPathChallengeFrameSize(*frame.path_challenge_frame);
    case STOP_SENDING_FRAME:
      return GetStopSendingFrameSize(*frame.stop_sending_frame);

    case STREAM_FRAME:
    case ACK_FRAME:
    case STOP_WAITING_FRAME:
    case MTU_DISCOVERY_FRAME:
    case PADDING_FRAME:
    case MESSAGE_FRAME:
    case CRYPTO_FRAME:
    case NUM_FRAME_TYPES:
      DCHECK(false);
      return 0;
  }

  // Not reachable, but some Chrome compilers can't figure that out.  *sigh*
  DCHECK(false);
  return 0;
}

// static
size_t QuicFramer::GetStreamIdSize(QuicStreamId stream_id) {
  // Sizes are 1 through 4 bytes.
  for (int i = 1; i <= 4; ++i) {
    stream_id >>= 8;
    if (stream_id == 0) {
      return i;
    }
  }
  QUIC_BUG << "Failed to determine StreamIDSize.";
  return 4;
}

// static
size_t QuicFramer::GetStreamOffsetSize(QuicTransportVersion version,
                                       QuicStreamOffset offset) {
  // 0 is a special case.
  if (offset == 0) {
    return 0;
  }
  // 2 through 8 are the remaining sizes.
  offset >>= 8;
  for (int i = 2; i <= 8; ++i) {
    offset >>= 8;
    if (offset == 0) {
      return i;
    }
  }
  QUIC_BUG << "Failed to determine StreamOffsetSize.";
  return 8;
}

// static
size_t QuicFramer::GetNewConnectionIdFrameSize(
    const QuicNewConnectionIdFrame& frame) {
  return kQuicFrameTypeSize +
         QuicDataWriter::GetVarInt62Len(frame.sequence_number) +
         kConnectionIdLengthSize + kQuicConnectionIdLength +
         sizeof(frame.stateless_reset_token);
}

// static
size_t QuicFramer::GetRetireConnectionIdFrameSize(
    const QuicRetireConnectionIdFrame& frame) {
  return kQuicFrameTypeSize +
         QuicDataWriter::GetVarInt62Len(frame.sequence_number);
}

// static
size_t QuicFramer::GetNewTokenFrameSize(const QuicNewTokenFrame& frame) {
  return kQuicFrameTypeSize +
         QuicDataWriter::GetVarInt62Len(frame.token.length()) +
         frame.token.length();
}

// static
size_t QuicFramer::GetVersionNegotiationPacketSize(size_t number_versions) {
  return kPublicFlagsSize + PACKET_8BYTE_CONNECTION_ID +
         number_versions * kQuicVersionSize;
}

// TODO(nharper): Change this method to take a ParsedQuicVersion.
bool QuicFramer::IsSupportedTransportVersion(
    const QuicTransportVersion version) const {
  for (ParsedQuicVersion supported_version : supported_versions_) {
    if (version == supported_version.transport_version) {
      return true;
    }
  }
  return false;
}

bool QuicFramer::IsSupportedVersion(const ParsedQuicVersion version) const {
  for (const ParsedQuicVersion& supported_version : supported_versions_) {
    if (version == supported_version) {
      return true;
    }
  }
  return false;
}

size_t QuicFramer::GetSerializedFrameLength(
    const QuicFrame& frame,
    size_t free_bytes,
    bool first_frame,
    bool last_frame,
    QuicPacketNumberLength packet_number_length) {
  // Prevent a rare crash reported in b/19458523.
  if (frame.type == ACK_FRAME && frame.ack_frame == nullptr) {
    QUIC_BUG << "Cannot compute the length of a null ack frame. free_bytes:"
             << free_bytes << " first_frame:" << first_frame
             << " last_frame:" << last_frame
             << " seq num length:" << packet_number_length;
    set_error(QUIC_INTERNAL_ERROR);
    visitor_->OnError(this);
    return 0;
  }
  if (frame.type == PADDING_FRAME) {
    if (frame.padding_frame.num_padding_bytes == -1) {
      // Full padding to the end of the packet.
      return free_bytes;
    } else {
      // Lite padding.
      return free_bytes <
                     static_cast<size_t>(frame.padding_frame.num_padding_bytes)
                 ? free_bytes
                 : frame.padding_frame.num_padding_bytes;
    }
  }

  size_t frame_len =
      ComputeFrameLength(frame, last_frame, packet_number_length);
  if (frame_len <= free_bytes) {
    // Frame fits within packet. Note that acks may be truncated.
    return frame_len;
  }
  // Only truncate the first frame in a packet, so if subsequent ones go
  // over, stop including more frames.
  if (!first_frame) {
    return 0;
  }
  bool can_truncate =
      frame.type == ACK_FRAME &&
      free_bytes >= GetMinAckFrameSize(version_.transport_version,
                                       PACKET_6BYTE_PACKET_NUMBER);
  if (can_truncate) {
    // Truncate the frame so the packet will not exceed kMaxPacketSize.
    // Note that we may not use every byte of the writer in this case.
    QUIC_DLOG(INFO) << ENDPOINT
                    << "Truncating large frame, free bytes: " << free_bytes;
    return free_bytes;
  }
  return 0;
}

QuicFramer::AckFrameInfo::AckFrameInfo()
    : max_block_length(0), first_block_length(0), num_ack_blocks(0) {}

QuicFramer::AckFrameInfo::AckFrameInfo(const AckFrameInfo& other) = default;

QuicFramer::AckFrameInfo::~AckFrameInfo() {}

size_t QuicFramer::BuildDataPacket(const QuicPacketHeader& header,
                                   const QuicFrames& frames,
                                   char* buffer,
                                   size_t packet_length) {
  if (version_.transport_version == QUIC_VERSION_99) {
    return BuildIetfDataPacket(header, frames, buffer, packet_length);
  }
  QuicDataWriter writer(packet_length, buffer, endianness());
  if (!AppendPacketHeader(header, &writer)) {
    QUIC_BUG << "AppendPacketHeader failed";
    return 0;
  }

  size_t i = 0;
  for (const QuicFrame& frame : frames) {
    // Determine if we should write stream frame length in header.
    const bool last_frame_in_packet = i == frames.size() - 1;
    if (!AppendTypeByte(frame, last_frame_in_packet, &writer)) {
      QUIC_BUG << "AppendTypeByte failed";
      return 0;
    }

    switch (frame.type) {
      case PADDING_FRAME:
        if (!AppendPaddingFrame(frame.padding_frame, &writer)) {
          QUIC_BUG << "AppendPaddingFrame of "
                   << frame.padding_frame.num_padding_bytes << " failed";
          return 0;
        }
        break;
      case STREAM_FRAME:
        if (!AppendStreamFrame(frame.stream_frame, last_frame_in_packet,
                               &writer)) {
          QUIC_BUG << "AppendStreamFrame failed";
          return 0;
        }
        break;
      case ACK_FRAME:
        if (!AppendAckFrameAndTypeByte(*frame.ack_frame, &writer)) {
          QUIC_BUG << "AppendAckFrameAndTypeByte failed: " << detailed_error_;
          return 0;
        }
        break;
      case STOP_WAITING_FRAME:
        if (!AppendStopWaitingFrame(header, *frame.stop_waiting_frame,
                                    &writer)) {
          QUIC_BUG << "AppendStopWaitingFrame failed";
          return 0;
        }
        break;
      case MTU_DISCOVERY_FRAME:
        // MTU discovery frames are serialized as ping frames.
        QUIC_FALLTHROUGH_INTENDED;
      case PING_FRAME:
        // Ping has no payload.
        break;
      case RST_STREAM_FRAME:
        if (!AppendRstStreamFrame(*frame.rst_stream_frame, &writer)) {
          QUIC_BUG << "AppendRstStreamFrame failed";
          return 0;
        }
        break;
      case CONNECTION_CLOSE_FRAME:
        if (!AppendConnectionCloseFrame(*frame.connection_close_frame,
                                        &writer)) {
          QUIC_BUG << "AppendConnectionCloseFrame failed";
          return 0;
        }
        break;
      case GOAWAY_FRAME:
        if (!AppendGoAwayFrame(*frame.goaway_frame, &writer)) {
          QUIC_BUG << "AppendGoAwayFrame failed";
          return 0;
        }
        break;
      case WINDOW_UPDATE_FRAME:
        if (!AppendWindowUpdateFrame(*frame.window_update_frame, &writer)) {
          QUIC_BUG << "AppendWindowUpdateFrame failed";
          return 0;
        }
        break;
      case BLOCKED_FRAME:
        if (!AppendBlockedFrame(*frame.blocked_frame, &writer)) {
          QUIC_BUG << "AppendBlockedFrame failed";
          return 0;
        }
        break;
      case APPLICATION_CLOSE_FRAME:
        set_detailed_error(
            "Attempt to append APPLICATION_CLOSE frame and not in version 99.");
        return RaiseError(QUIC_INTERNAL_ERROR);
      case NEW_CONNECTION_ID_FRAME:
        set_detailed_error(
            "Attempt to append NEW_CONNECTION_ID frame and not in version 99.");
        return RaiseError(QUIC_INTERNAL_ERROR);
      case RETIRE_CONNECTION_ID_FRAME:
        set_detailed_error(
            "Attempt to append RETIRE_CONNECTION_ID frame and not in version "
            "99.");
        return RaiseError(QUIC_INTERNAL_ERROR);
      case NEW_TOKEN_FRAME:
        set_detailed_error(
            "Attempt to append NEW_TOKEN_ID frame and not in version 99.");
        return RaiseError(QUIC_INTERNAL_ERROR);
      case MAX_STREAM_ID_FRAME:
        set_detailed_error(
            "Attempt to append MAX_STREAM_ID frame and not in version 99.");
        return RaiseError(QUIC_INTERNAL_ERROR);
      case STREAM_ID_BLOCKED_FRAME:
        set_detailed_error(
            "Attempt to append STREAM_ID_BLOCKED frame and not in version 99.");
        return RaiseError(QUIC_INTERNAL_ERROR);
      case PATH_RESPONSE_FRAME:
        set_detailed_error(
            "Attempt to append PATH_RESPONSE frame and not in version 99.");
        return RaiseError(QUIC_INTERNAL_ERROR);
      case PATH_CHALLENGE_FRAME:
        set_detailed_error(
            "Attempt to append PATH_CHALLENGE frame and not in version 99.");
        return RaiseError(QUIC_INTERNAL_ERROR);
      case STOP_SENDING_FRAME:
        set_detailed_error(
            "Attempt to append STOP_SENDING frame and not in version 99.");
        return RaiseError(QUIC_INTERNAL_ERROR);
      case MESSAGE_FRAME:
        if (!AppendMessageFrameAndTypeByte(*frame.message_frame,
                                           last_frame_in_packet, &writer)) {
          QUIC_BUG << "AppendMessageFrame failed";
          return 0;
        }
        break;
      case CRYPTO_FRAME:
        set_detailed_error(
            "Attempt to append CRYPTO frame and not in version 99.");
        return RaiseError(QUIC_INTERNAL_ERROR);

      default:
        RaiseError(QUIC_INVALID_FRAME_DATA);
        QUIC_BUG << "QUIC_INVALID_FRAME_DATA";
        return 0;
    }
    ++i;
  }

  return writer.length();
}

size_t QuicFramer::BuildIetfDataPacket(const QuicPacketHeader& header,
                                       const QuicFrames& frames,
                                       char* buffer,
                                       size_t packet_length) {
  QuicDataWriter writer(packet_length, buffer, endianness());
  if (!AppendIetfPacketHeader(header, &writer)) {
    QUIC_BUG << "AppendPacketHeader failed";
    return 0;
  }

  size_t i = 0;
  for (const QuicFrame& frame : frames) {
    // Determine if we should write stream frame length in header.
    const bool last_frame_in_packet = i == frames.size() - 1;
    if (!AppendIetfTypeByte(frame, last_frame_in_packet, &writer)) {
      QUIC_BUG << "AppendIetfTypeByte failed";
      return 0;
    }

    switch (frame.type) {
      case PADDING_FRAME:
        if (!AppendPaddingFrame(frame.padding_frame, &writer)) {
          QUIC_BUG << "AppendPaddingFrame of "
                   << frame.padding_frame.num_padding_bytes << " failed";
          return 0;
        }
        break;
      case STREAM_FRAME:
        if (!AppendStreamFrame(frame.stream_frame, last_frame_in_packet,
                               &writer)) {
          QUIC_BUG << "AppendStreamFrame failed";
          return 0;
        }
        break;
      case ACK_FRAME:
        if (!AppendIetfAckFrameAndTypeByte(*frame.ack_frame, &writer)) {
          QUIC_BUG << "AppendAckFrameAndTypeByte failed";
          return 0;
        }
        break;
      case STOP_WAITING_FRAME:
        set_detailed_error(
            "Attempt to append STOP WAITING frame in version 99.");
        return RaiseError(QUIC_INTERNAL_ERROR);
      case MTU_DISCOVERY_FRAME:
        // MTU discovery frames are serialized as ping frames.
        QUIC_FALLTHROUGH_INTENDED;
      case PING_FRAME:
        // Ping has no payload.
        break;
      case RST_STREAM_FRAME:
        if (!AppendRstStreamFrame(*frame.rst_stream_frame, &writer)) {
          QUIC_BUG << "AppendRstStreamFrame failed";
          return 0;
        }
        break;
      case CONNECTION_CLOSE_FRAME:
        if (!AppendConnectionCloseFrame(*frame.connection_close_frame,
                                        &writer)) {
          QUIC_BUG << "AppendConnectionCloseFrame failed";
          return 0;
        }
        break;
      case GOAWAY_FRAME:
        set_detailed_error("Attempt to append GOAWAY frame in version 99.");
        return RaiseError(QUIC_INTERNAL_ERROR);
      case WINDOW_UPDATE_FRAME:
        // Depending on whether there is a stream ID or not, will be either a
        // MAX STREAM DATA frame or a MAX DATA frame.
        if (frame.window_update_frame->stream_id == 0) {
          if (!AppendMaxDataFrame(*frame.window_update_frame, &writer)) {
            QUIC_BUG << "AppendMaxDataFrame failed";
            return 0;
          }
        } else {
          if (!AppendMaxStreamDataFrame(*frame.window_update_frame, &writer)) {
            QUIC_BUG << "AppendMaxStreamDataFrame failed";
            return 0;
          }
        }
        break;
      case BLOCKED_FRAME:
        if (!AppendBlockedFrame(*frame.blocked_frame, &writer)) {
          QUIC_BUG << "AppendBlockedFrame failed";
          return 0;
        }
        break;
      case APPLICATION_CLOSE_FRAME:
        if (!AppendApplicationCloseFrame(*frame.application_close_frame,
                                         &writer)) {
          QUIC_BUG << "AppendApplicationCloseFrame failed";
          return 0;
        }
        break;
      case MAX_STREAM_ID_FRAME:
        if (!AppendMaxStreamIdFrame(frame.max_stream_id_frame, &writer)) {
          QUIC_BUG << "AppendMaxStreamIdFrame failed";
          return 0;
        }
        break;
      case STREAM_ID_BLOCKED_FRAME:
        if (!AppendStreamIdBlockedFrame(frame.stream_id_blocked_frame,
                                        &writer)) {
          QUIC_BUG << "AppendMaxStreamIdFrame failed";
          return 0;
        }
        break;
      case NEW_CONNECTION_ID_FRAME:
        if (!AppendNewConnectionIdFrame(*frame.new_connection_id_frame,
                                        &writer)) {
          QUIC_BUG << "AppendNewConnectionIdFrame failed";
          return 0;
        }
        break;
      case RETIRE_CONNECTION_ID_FRAME:
        if (!AppendRetireConnectionIdFrame(*frame.retire_connection_id_frame,
                                           &writer)) {
          QUIC_BUG << "AppendRetireConnectionIdFrame failed";
          return 0;
        }
        break;
      case NEW_TOKEN_FRAME:
        if (!AppendNewTokenFrame(*frame.new_token_frame, &writer)) {
          QUIC_BUG << "AppendNewTokenFrame failed";
          return 0;
        }
        break;
      case STOP_SENDING_FRAME:
        if (!AppendStopSendingFrame(*frame.stop_sending_frame, &writer)) {
          QUIC_BUG << "AppendStopSendingFrame failed";
          return 0;
        }
        break;
      case PATH_CHALLENGE_FRAME:
        if (!AppendPathChallengeFrame(*frame.path_challenge_frame, &writer)) {
          QUIC_BUG << "AppendPathChallengeFrame failed";
          return 0;
        }
        break;
      case PATH_RESPONSE_FRAME:
        if (!AppendPathResponseFrame(*frame.path_response_frame, &writer)) {
          QUIC_BUG << "AppendPathResponseFrame failed";
          return 0;
        }
        break;
      case MESSAGE_FRAME:
        if (!AppendMessageFrameAndTypeByte(*frame.message_frame,
                                           last_frame_in_packet, &writer)) {
          QUIC_BUG << "AppendMessageFrame failed";
          return 0;
        }
        break;
      case CRYPTO_FRAME:
        if (!AppendCryptoFrame(*frame.crypto_frame, &writer)) {
          QUIC_BUG << "AppendCryptoFrame failed";
          return 0;
        }
        break;
      default:
        RaiseError(QUIC_INVALID_FRAME_DATA);
        QUIC_BUG << "QUIC_INVALID_FRAME_DATA";
        return 0;
    }
    ++i;
  }

  return writer.length();
}

size_t QuicFramer::BuildConnectivityProbingPacket(
    const QuicPacketHeader& header,
    char* buffer,
    size_t packet_length) {
  QuicDataWriter writer(packet_length, buffer, endianness());

  if (!AppendPacketHeader(header, &writer)) {
    QUIC_BUG << "AppendPacketHeader failed";
    return 0;
  }

  // Write a PING frame, which has no data payload.
  QuicPingFrame ping_frame;
  if (!AppendTypeByte(QuicFrame(ping_frame), false, &writer)) {
    QUIC_BUG << "AppendTypeByte failed for ping frame in probing packet";
    return 0;
  }
  // Add padding to the rest of the packet.
  QuicPaddingFrame padding_frame;
  if (!AppendTypeByte(QuicFrame(padding_frame), true, &writer)) {
    QUIC_BUG << "AppendTypeByte failed for padding frame in probing packet";
    return 0;
  }
  if (!AppendPaddingFrame(padding_frame, &writer)) {
    QUIC_BUG << "AppendPaddingFrame of " << padding_frame.num_padding_bytes
             << " failed";
    return 0;
  }

  return writer.length();
}

size_t QuicFramer::BuildPaddedPathChallengePacket(
    const QuicPacketHeader& header,
    char* buffer,
    size_t packet_length,
    QuicPathFrameBuffer* payload,
    QuicRandom* randomizer) {
  QuicDataWriter writer(packet_length, buffer, endianness());
  DCHECK_EQ(version_.transport_version, QUIC_VERSION_99)
      << "Attempt to build a PATH_CHALLENGE Connectivity Probing packet and "
         "not doing IETF QUIC";

  if (!AppendPacketHeader(header, &writer)) {
    QUIC_BUG << "AppendPacketHeader failed";
    return 0;
  }

  // Write a PATH_CHALLENGE frame, which has a random 8-byte payload
  randomizer->RandBytes(payload->data(), payload->size());

  QuicPathChallengeFrame path_challenge_frame(0, *payload);
  if (!AppendTypeByte(QuicFrame(&path_challenge_frame),
                      /* last_frame_in_packet = */ false, &writer)) {
    QUIC_BUG
        << "AppendTypeByte failed for PATH_CHALLENGE frame in probing packet";
    return 0;
  }

  if (!AppendPathChallengeFrame(path_challenge_frame, &writer)) {
    QUIC_BUG << "AppendPathChallengeFrame failed for PATH_CHALLENGE frame in "
                "probing packet";
    return 0;
  }

  // Add padding to the rest of the packet in order to assess Path MTU
  // characteristics.
  QuicPaddingFrame padding_frame;
  if (!AppendTypeByte(QuicFrame(padding_frame), true, &writer)) {
    QUIC_BUG << "AppendTypeByte failed for padding frame in probing packet";
    return 0;
  }
  if (!AppendPaddingFrame(padding_frame, &writer)) {
    QUIC_BUG << "AppendPaddingFrame of " << padding_frame.num_padding_bytes
             << " failed";
    return 0;
  }

  return writer.length();
}

size_t QuicFramer::BuildPathResponsePacket(
    const QuicPacketHeader& header,
    char* buffer,
    size_t packet_length,
    const QuicDeque<QuicPathFrameBuffer>& payloads,
    const bool is_padded) {
  if (payloads.empty()) {
    QUIC_BUG
        << "Attempt to generate connectivity response with no request payloads";
    return 0;
  }

  QuicDataWriter writer(packet_length, buffer, endianness());

  DCHECK_EQ(version_.transport_version, QUIC_VERSION_99)
      << "Attempt to build a PATH_RESPONSE Connectivity Probing packet and "
         "not doing IETF QUIC";

  if (!AppendPacketHeader(header, &writer)) {
    QUIC_BUG << "AppendPacketHeader failed";
    return 0;
  }

  // Write a set of PATH_RESPONSE frames to a single packet, each with the
  // 8-byte payload of the corresponding PATH_REQUEST frame from a single
  // received packet.
  for (const QuicPathFrameBuffer& payload : payloads) {
    // Note that the control frame ID can be 0 since this is not retransmitted.
    QuicPathResponseFrame path_response_frame(0, payload);

    if (!AppendTypeByte(QuicFrame(&path_response_frame), false, &writer)) {
      QUIC_BUG
          << "AppendTypeByte failed for PATH_RESPONSE frame in probing packet";
      return 0;
    }

    if (!AppendPathResponseFrame(path_response_frame, &writer)) {
      QUIC_BUG << "AppendPathChallengeFrame failed for PATH_CHALLENGE frame in "
                  "probing packet";
      return 0;
    }
  }

  if (is_padded) {
    // Add padding to the rest of the packet in order to assess Path MTU
    // characteristics.
    QuicPaddingFrame padding_frame;
    if (!AppendTypeByte(QuicFrame(padding_frame), true, &writer)) {
      QUIC_BUG << "AppendTypeByte failed for padding frame in probing packet";
      return 0;
    }
    if (!AppendPaddingFrame(padding_frame, &writer)) {
      QUIC_BUG << "AppendPaddingFrame of " << padding_frame.num_padding_bytes
               << " failed";
      return 0;
    }
  }
  return writer.length();
}

// static
std::unique_ptr<QuicEncryptedPacket> QuicFramer::BuildPublicResetPacket(
    const QuicPublicResetPacket& packet) {
  CryptoHandshakeMessage reset;
  reset.set_tag(kPRST);
  reset.SetValue(kRNON, packet.nonce_proof);
  if (packet.client_address.host().address_family() !=
      IpAddressFamily::IP_UNSPEC) {
    // packet.client_address is non-empty.
    QuicSocketAddressCoder address_coder(packet.client_address);
    QuicString serialized_address = address_coder.Encode();
    if (serialized_address.empty()) {
      return nullptr;
    }
    reset.SetStringPiece(kCADR, serialized_address);
  }
  const QuicData& reset_serialized = reset.GetSerialized();

  size_t len =
      kPublicFlagsSize + PACKET_8BYTE_CONNECTION_ID + reset_serialized.length();
  std::unique_ptr<char[]> buffer(new char[len]);
  // Endianness is not a concern here, as writer is not going to write integers
  // or floating numbers.
  QuicDataWriter writer(len, buffer.get(), NETWORK_BYTE_ORDER);

  uint8_t flags = static_cast<uint8_t>(PACKET_PUBLIC_FLAGS_RST |
                                       PACKET_PUBLIC_FLAGS_8BYTE_CONNECTION_ID);
  // This hack makes post-v33 public reset packet look like pre-v33 packets.
  flags |= static_cast<uint8_t>(PACKET_PUBLIC_FLAGS_8BYTE_CONNECTION_ID_OLD);
  if (!writer.WriteUInt8(flags)) {
    return nullptr;
  }

  if (!writer.WriteConnectionId(packet.connection_id)) {
    return nullptr;
  }

  if (!writer.WriteBytes(reset_serialized.data(), reset_serialized.length())) {
    return nullptr;
  }

  return QuicMakeUnique<QuicEncryptedPacket>(buffer.release(), len, true);
}

// static
std::unique_ptr<QuicEncryptedPacket> QuicFramer::BuildIetfStatelessResetPacket(
    QuicConnectionId connection_id,
    QuicUint128 stateless_reset_token) {
  QUIC_DVLOG(1) << "Building IETF stateless reset packet.";
  const bool quic_more_random_bytes =
      GetQuicReloadableFlag(quic_more_random_bytes_in_stateless_reset);
  const size_t random_bytes_length = quic_more_random_bytes
                                         ? kMinRandomBytesLengthInStatelessReset
                                         : PACKET_1BYTE_PACKET_NUMBER;
  size_t len = kPacketHeaderTypeSize + random_bytes_length +
               sizeof(stateless_reset_token);
  std::unique_ptr<char[]> buffer(new char[len]);
  QuicDataWriter writer(len, buffer.get(), NETWORK_BYTE_ORDER);

  uint8_t type = 0;
  type |= FLAGS_SHORT_HEADER_RESERVED_1;
  type |= FLAGS_SHORT_HEADER_RESERVED_2;
  type |= PacketNumberLengthToShortHeaderType(PACKET_1BYTE_PACKET_NUMBER);

  // Append type byte.
  if (!writer.WriteUInt8(type)) {
    return nullptr;
  }
  if (quic_more_random_bytes) {
    // Append random bytes.
    if (!writer.WriteRandomBytes(QuicRandom::GetInstance(),
                                 random_bytes_length)) {
      return nullptr;
    }
  } else {
    // Append an random packet number.
    QuicPacketNumber random_packet_number =
        QuicRandom::GetInstance()->RandUint64() % 255 + 1;
    if (!AppendPacketNumber(PACKET_1BYTE_PACKET_NUMBER, random_packet_number,
                            &writer)) {
      return nullptr;
    }
  }

  // Append stateless reset token.
  if (!writer.WriteBytes(&stateless_reset_token,
                         sizeof(stateless_reset_token))) {
    return nullptr;
  }
  return QuicMakeUnique<QuicEncryptedPacket>(buffer.release(), len, true);
}

// static
std::unique_ptr<QuicEncryptedPacket> QuicFramer::BuildVersionNegotiationPacket(
    QuicConnectionId connection_id,
    bool ietf_quic,
    const ParsedQuicVersionVector& versions) {
  if (ietf_quic) {
    return BuildIetfVersionNegotiationPacket(connection_id, versions);
  }
  DCHECK(!versions.empty());
  size_t len = GetVersionNegotiationPacketSize(versions.size());
  std::unique_ptr<char[]> buffer(new char[len]);
  // Endianness is not a concern here, version negotiation packet does not have
  // integers or floating numbers.
  QuicDataWriter writer(len, buffer.get(), NETWORK_BYTE_ORDER);

  uint8_t flags = static_cast<uint8_t>(
      PACKET_PUBLIC_FLAGS_VERSION | PACKET_PUBLIC_FLAGS_8BYTE_CONNECTION_ID |
      // TODO(rch): Remove this QUIC_VERSION_32 is retired.
      PACKET_PUBLIC_FLAGS_8BYTE_CONNECTION_ID_OLD);
  if (!writer.WriteUInt8(flags)) {
    return nullptr;
  }

  if (!writer.WriteConnectionId(connection_id)) {
    return nullptr;
  }

  for (const ParsedQuicVersion& version : versions) {
    // TODO(rch): Use WriteUInt32() once QUIC_VERSION_35 is removed.
    if (!writer.WriteTag(
            QuicEndian::HostToNet32(CreateQuicVersionLabel(version)))) {
      return nullptr;
    }
  }

  return QuicMakeUnique<QuicEncryptedPacket>(buffer.release(), len, true);
}

// static
std::unique_ptr<QuicEncryptedPacket>
QuicFramer::BuildIetfVersionNegotiationPacket(
    QuicConnectionId connection_id,
    const ParsedQuicVersionVector& versions) {
  QUIC_DVLOG(1) << "Building IETF version negotiation packet.";
  DCHECK(!versions.empty());
  size_t len = kPacketHeaderTypeSize + kConnectionIdLengthSize +
               PACKET_8BYTE_CONNECTION_ID +
               (versions.size() + 1) * kQuicVersionSize;
  std::unique_ptr<char[]> buffer(new char[len]);
  QuicDataWriter writer(len, buffer.get(), NETWORK_BYTE_ORDER);

  // TODO(fayang): Randomly select a value for the type.
  uint8_t type = static_cast<uint8_t>(FLAGS_LONG_HEADER | VERSION_NEGOTIATION);
  if (!writer.WriteUInt8(type)) {
    return nullptr;
  }

  if (!writer.WriteUInt32(0)) {
    return nullptr;
  }

  if (!AppendIetfConnectionId(true, 0, PACKET_0BYTE_CONNECTION_ID,
                              connection_id, PACKET_8BYTE_CONNECTION_ID,
                              &writer)) {
    return nullptr;
  }

  for (const ParsedQuicVersion& version : versions) {
    // TODO(rch): Use WriteUInt32() once QUIC_VERSION_35 is removed.
    if (!writer.WriteTag(
            QuicEndian::HostToNet32(CreateQuicVersionLabel(version)))) {
      return nullptr;
    }
  }

  return QuicMakeUnique<QuicEncryptedPacket>(buffer.release(), len, true);
}

bool QuicFramer::ProcessPacket(const QuicEncryptedPacket& packet) {
  QuicDataReader reader(packet.data(), packet.length(), endianness());

  last_packet_is_ietf_quic_ = false;
  if (perspective_ == Perspective::IS_CLIENT) {
    last_packet_is_ietf_quic_ = version_.transport_version > QUIC_VERSION_43;
  } else if (!reader.IsDoneReading()) {
    uint8_t type = reader.PeekByte();
    last_packet_is_ietf_quic_ = QuicUtils::IsIetfPacketHeader(type);
  }
  if (last_packet_is_ietf_quic_) {
    QUIC_DVLOG(1) << ENDPOINT << "Processing IETF QUIC packet.";
    reader.set_endianness(NETWORK_BYTE_ORDER);
  }

  visitor_->OnPacket();

  QuicPacketHeader header;
  if (!ProcessPublicHeader(&reader, &header)) {
    DCHECK_NE("", detailed_error_);
    QUIC_DVLOG(1) << ENDPOINT << "Unable to process public header. Error: "
                  << detailed_error_;
    DCHECK_NE("", detailed_error_);
    return RaiseError(QUIC_INVALID_PACKET_HEADER);
  }

  if (!visitor_->OnUnauthenticatedPublicHeader(header)) {
    // The visitor suppresses further processing of the packet.
    return true;
  }

  if (perspective_ == Perspective::IS_SERVER && header.version_flag &&
      header.version != version_) {
    if (!visitor_->OnProtocolVersionMismatch(header.version)) {
      return true;
    }
  }

  // framer's version may change, reset reader's endianness.
  reader.set_endianness(endianness());

  bool rv;
  if (IsVersionNegotiation(header)) {
    QUIC_DVLOG(1) << ENDPOINT << "Received version negotiation packet";
    rv = ProcessVersionNegotiationPacket(&reader, header);
  } else if (header.reset_flag) {
    rv = ProcessPublicResetPacket(&reader, header);
  } else if (packet.length() <= kMaxPacketSize) {
    // The optimized decryption algorithm implementations run faster when
    // operating on aligned memory.
    QUIC_CACHELINE_ALIGNED char buffer[kMaxPacketSize];
    if (last_packet_is_ietf_quic_) {
      rv = ProcessIetfDataPacket(&reader, &header, packet, buffer,
                                 kMaxPacketSize);
    } else {
      rv = ProcessDataPacket(&reader, &header, packet, buffer, kMaxPacketSize);
    }
  } else {
    std::unique_ptr<char[]> large_buffer(new char[packet.length()]);
    if (last_packet_is_ietf_quic_) {
      rv = ProcessIetfDataPacket(&reader, &header, packet, large_buffer.get(),
                                 packet.length());
    } else {
      rv = ProcessDataPacket(&reader, &header, packet, large_buffer.get(),
                             packet.length());
    }
    QUIC_BUG_IF(rv) << "QUIC should never successfully process packets larger"
                    << "than kMaxPacketSize. packet size:" << packet.length();
  }
  return rv;
}

bool QuicFramer::ProcessVersionNegotiationPacket(
    QuicDataReader* reader,
    const QuicPacketHeader& header) {
  DCHECK_EQ(Perspective::IS_CLIENT, perspective_);

  QuicVersionNegotiationPacket packet(header.destination_connection_id);
  // Try reading at least once to raise error if the packet is invalid.
  do {
    QuicVersionLabel version_label;
    if (!reader->ReadTag(&version_label)) {
      set_detailed_error("Unable to read supported version in negotiation.");
      return RaiseError(QUIC_INVALID_VERSION_NEGOTIATION_PACKET);
    }
    // TODO(rch): Use ReadUInt32() once QUIC_VERSION_35 is removed.
    version_label = QuicEndian::NetToHost32(version_label);
    packet.versions.push_back(ParseQuicVersionLabel(version_label));
  } while (!reader->IsDoneReading());

  visitor_->OnVersionNegotiationPacket(packet);
  return true;
}

bool QuicFramer::ProcessIetfDataPacket(QuicDataReader* encrypted_reader,
                                       QuicPacketHeader* header,
                                       const QuicEncryptedPacket& packet,
                                       char* decrypted_buffer,
                                       size_t buffer_length) {
  DCHECK(!header->has_possible_stateless_reset_token);
  if (header->form == SHORT_HEADER) {
    if (!process_stateless_reset_at_client_only_) {
      // Peak possible stateless reset token. Will only be used on decryption
      // failure.
      QuicStringPiece remaining = encrypted_reader->PeekRemainingPayload();
      if (remaining.length() >=
          sizeof(header->possible_stateless_reset_token)) {
        remaining.copy(
            reinterpret_cast<char*>(&header->possible_stateless_reset_token),
            sizeof(header->possible_stateless_reset_token),
            remaining.length() -
                sizeof(header->possible_stateless_reset_token));
      }
    } else {
      QUIC_FLAG_COUNT(
          quic_reloadable_flag_quic_process_stateless_reset_at_client_only);
      if (perspective_ == Perspective::IS_CLIENT) {
        // Peek possible stateless reset token. Will only be used on decryption
        // failure.
        QuicStringPiece remaining = encrypted_reader->PeekRemainingPayload();
        if (remaining.length() >=
            sizeof(header->possible_stateless_reset_token)) {
          header->has_possible_stateless_reset_token = true;
          memcpy(
              &header->possible_stateless_reset_token,
              &remaining.data()[remaining.length() -
                                sizeof(header->possible_stateless_reset_token)],
              sizeof(header->possible_stateless_reset_token));
        }
      }
    }
  }

  if (header->form == SHORT_HEADER ||
      header->long_packet_type != VERSION_NEGOTIATION) {
    // Process packet number.
    QuicPacketNumber base_packet_number = largest_packet_number_;

    if (!ProcessAndCalculatePacketNumber(
            encrypted_reader, header->packet_number_length, base_packet_number,
            &header->packet_number)) {
      set_detailed_error("Unable to read packet number.");
      return RaiseError(QUIC_INVALID_PACKET_HEADER);
    }

    if (header->packet_number == 0u) {
      if (IsIetfStatelessResetPacket(*header)) {
        // This is a stateless reset packet.
        QuicIetfStatelessResetPacket packet(
            *header, header->possible_stateless_reset_token);
        visitor_->OnAuthenticatedIetfStatelessResetPacket(packet);
        return true;
      }
      set_detailed_error("packet numbers cannot be 0.");
      return RaiseError(QUIC_INVALID_PACKET_HEADER);
    }
  }

  // A nonce should only present in SHLO from the server to the client when
  // using QUIC crypto.
  if (header->form == LONG_HEADER &&
      header->long_packet_type == ZERO_RTT_PROTECTED &&
      perspective_ == Perspective::IS_CLIENT) {
    if (!encrypted_reader->ReadBytes(
            reinterpret_cast<uint8_t*>(last_nonce_.data()),
            last_nonce_.size())) {
      set_detailed_error("Unable to read nonce.");
      return RaiseError(QUIC_INVALID_PACKET_HEADER);
    }

    header->nonce = &last_nonce_;
  } else {
    header->nonce = nullptr;
  }

  if (!visitor_->OnUnauthenticatedHeader(*header)) {
    set_detailed_error(
        "Visitor asked to stop processing of unauthenticated header.");
    return false;
  }

  size_t decrypted_length = 0;
  if (!DecryptPayload(encrypted_reader, *header, packet, decrypted_buffer,
                      buffer_length, &decrypted_length)) {
    if (IsIetfStatelessResetPacket(*header)) {
      // This is a stateless reset packet.
      QuicIetfStatelessResetPacket packet(
          *header, header->possible_stateless_reset_token);
      visitor_->OnAuthenticatedIetfStatelessResetPacket(packet);
      return true;
    }
    set_detailed_error("Unable to decrypt payload.");
    return RaiseError(QUIC_DECRYPTION_FAILURE);
  }
  QuicDataReader reader(decrypted_buffer, decrypted_length, endianness());

  // Update the largest packet number after we have decrypted the packet
  // so we are confident is not attacker controlled.
  largest_packet_number_ =
      std::max(header->packet_number, largest_packet_number_);

  if (!visitor_->OnPacketHeader(*header)) {
    // The visitor suppresses further processing of the packet.
    return true;
  }

  if (packet.length() > kMaxPacketSize) {
    // If the packet has gotten this far, it should not be too large.
    QUIC_BUG << "Packet too large:" << packet.length();
    return RaiseError(QUIC_PACKET_TOO_LARGE);
  }

  // Handle the payload.
  if (version_.transport_version == QUIC_VERSION_99) {
    if (!ProcessIetfFrameData(&reader, *header)) {
      DCHECK_NE(QUIC_NO_ERROR, error_);  // ProcessIetfFrameData sets the error.
      DCHECK_NE("", detailed_error_);
      QUIC_DLOG(WARNING) << ENDPOINT << "Unable to process frame data. Error: "
                         << detailed_error_;
      return false;
    }
  } else {
    if (!ProcessFrameData(&reader, *header)) {
      DCHECK_NE(QUIC_NO_ERROR, error_);  // ProcessFrameData sets the error.
      DCHECK_NE("", detailed_error_);
      QUIC_DLOG(WARNING) << ENDPOINT << "Unable to process frame data. Error: "
                         << detailed_error_;
      return false;
    }
  }

  visitor_->OnPacketComplete();
  return true;
}

bool QuicFramer::ProcessDataPacket(QuicDataReader* encrypted_reader,
                                   QuicPacketHeader* header,
                                   const QuicEncryptedPacket& packet,
                                   char* decrypted_buffer,
                                   size_t buffer_length) {
  if (!ProcessUnauthenticatedHeader(encrypted_reader, header)) {
    DCHECK_NE("", detailed_error_);
    QUIC_DVLOG(1)
        << ENDPOINT
        << "Unable to process packet header. Stopping parsing. Error: "
        << detailed_error_;
    return false;
  }

  size_t decrypted_length = 0;
  if (!DecryptPayload(encrypted_reader, *header, packet, decrypted_buffer,
                      buffer_length, &decrypted_length)) {
    set_detailed_error("Unable to decrypt payload.");
    return RaiseError(QUIC_DECRYPTION_FAILURE);
  }

  QuicDataReader reader(decrypted_buffer, decrypted_length, endianness());

  // Update the largest packet number after we have decrypted the packet
  // so we are confident is not attacker controlled.
  largest_packet_number_ =
      std::max(header->packet_number, largest_packet_number_);

  if (!visitor_->OnPacketHeader(*header)) {
    // The visitor suppresses further processing of the packet.
    return true;
  }

  if (packet.length() > kMaxPacketSize) {
    // If the packet has gotten this far, it should not be too large.
    QUIC_BUG << "Packet too large:" << packet.length();
    return RaiseError(QUIC_PACKET_TOO_LARGE);
  }

  // Handle the payload.
  if (!ProcessFrameData(&reader, *header)) {
    DCHECK_NE(QUIC_NO_ERROR, error_);  // ProcessFrameData sets the error.
    DCHECK_NE("", detailed_error_);
    QUIC_DLOG(WARNING) << ENDPOINT << "Unable to process frame data. Error: "
                       << detailed_error_;
    return false;
  }

  visitor_->OnPacketComplete();
  return true;
}

bool QuicFramer::ProcessPublicResetPacket(QuicDataReader* reader,
                                          const QuicPacketHeader& header) {
  QuicPublicResetPacket packet(header.destination_connection_id);

  std::unique_ptr<CryptoHandshakeMessage> reset(
      CryptoFramer::ParseMessage(reader->ReadRemainingPayload()));
  if (!reset.get()) {
    set_detailed_error("Unable to read reset message.");
    return RaiseError(QUIC_INVALID_PUBLIC_RST_PACKET);
  }
  if (reset->tag() != kPRST) {
    set_detailed_error("Incorrect message tag.");
    return RaiseError(QUIC_INVALID_PUBLIC_RST_PACKET);
  }

  if (reset->GetUint64(kRNON, &packet.nonce_proof) != QUIC_NO_ERROR) {
    set_detailed_error("Unable to read nonce proof.");
    return RaiseError(QUIC_INVALID_PUBLIC_RST_PACKET);
  }
  // TODO(satyamshekhar): validate nonce to protect against DoS.

  QuicStringPiece address;
  if (reset->GetStringPiece(kCADR, &address)) {
    QuicSocketAddressCoder address_coder;
    if (address_coder.Decode(address.data(), address.length())) {
      packet.client_address =
          QuicSocketAddress(address_coder.ip(), address_coder.port());
    }
  }

  visitor_->OnPublicResetPacket(packet);
  return true;
}

bool QuicFramer::IsIetfStatelessResetPacket(
    const QuicPacketHeader& header) const {
  return perspective_ == Perspective::IS_CLIENT &&
         header.form == SHORT_HEADER &&
         (!process_stateless_reset_at_client_only_ ||
          header.has_possible_stateless_reset_token) &&
         visitor_->IsValidStatelessResetToken(
             header.possible_stateless_reset_token);
}

bool QuicFramer::AppendPacketHeader(const QuicPacketHeader& header,
                                    QuicDataWriter* writer) {
  if (transport_version() > QUIC_VERSION_43) {
    return AppendIetfPacketHeader(header, writer);
  }
  QUIC_DVLOG(1) << ENDPOINT << "Appending header: " << header;
  uint8_t public_flags = 0;
  if (header.reset_flag) {
    public_flags |= PACKET_PUBLIC_FLAGS_RST;
  }
  if (header.version_flag) {
    public_flags |= PACKET_PUBLIC_FLAGS_VERSION;
  }

  public_flags |= GetPacketNumberFlags(header.packet_number_length)
                  << kPublicHeaderSequenceNumberShift;

  if (header.nonce != nullptr) {
    DCHECK_EQ(Perspective::IS_SERVER, perspective_);
    public_flags |= PACKET_PUBLIC_FLAGS_NONCE;
  }
  DCHECK_EQ(PACKET_0BYTE_CONNECTION_ID, header.source_connection_id_length);
  switch (header.destination_connection_id_length) {
    case PACKET_0BYTE_CONNECTION_ID:
      if (!writer->WriteUInt8(public_flags |
                              PACKET_PUBLIC_FLAGS_0BYTE_CONNECTION_ID)) {
        return false;
      }
      break;
    case PACKET_8BYTE_CONNECTION_ID:
      public_flags |= PACKET_PUBLIC_FLAGS_8BYTE_CONNECTION_ID;
      if (perspective_ == Perspective::IS_CLIENT) {
        public_flags |= PACKET_PUBLIC_FLAGS_8BYTE_CONNECTION_ID_OLD;
      }
      if (!writer->WriteUInt8(public_flags) ||
          !writer->WriteConnectionId(header.destination_connection_id)) {
        return false;
      }
      break;
  }
  last_serialized_connection_id_ = header.destination_connection_id;

  if (header.version_flag) {
    DCHECK_EQ(Perspective::IS_CLIENT, perspective_);
    QuicVersionLabel version_label = CreateQuicVersionLabel(version_);
    // TODO(rch): Use WriteUInt32() once QUIC_VERSION_35 is removed.
    if (!writer->WriteTag(QuicEndian::NetToHost32(version_label))) {
      return false;
    }

    QUIC_DVLOG(1) << ENDPOINT << "label = '"
                  << QuicVersionLabelToString(version_label) << "'";
  }

  if (header.nonce != nullptr &&
      !writer->WriteBytes(header.nonce, kDiversificationNonceSize)) {
    return false;
  }

  if (!AppendPacketNumber(header.packet_number_length, header.packet_number,
                          writer)) {
    return false;
  }

  return true;
}

bool QuicFramer::AppendIetfPacketHeader(const QuicPacketHeader& header,
                                        QuicDataWriter* writer) {
  QUIC_DVLOG(1) << ENDPOINT << "Appending IETF header: " << header;
  // Append type byte.
  uint8_t type = 0;
  if (header.version_flag) {
    type = static_cast<uint8_t>(FLAGS_LONG_HEADER | header.long_packet_type);
    DCHECK_EQ(PACKET_4BYTE_PACKET_NUMBER, header.packet_number_length);
  } else {
    type |= FLAGS_SHORT_HEADER_RESERVED_1;
    type |= FLAGS_SHORT_HEADER_RESERVED_2;
    DCHECK_GE(PACKET_4BYTE_PACKET_NUMBER, header.packet_number_length);
    type |= PacketNumberLengthToShortHeaderType(header.packet_number_length);
  }

  if (!writer->WriteUInt8(type)) {
    return false;
  }

  if (header.version_flag) {
    // Append version for long header.
    QuicVersionLabel version_label = CreateQuicVersionLabel(version_);
    // TODO(rch): Use WriteUInt32() once QUIC_VERSION_35 is removed.
    if (!writer->WriteTag(QuicEndian::NetToHost32(version_label))) {
      return false;
    }
  }

  // Append connection ID.
  if (!AppendIetfConnectionId(
          header.version_flag, header.destination_connection_id,
          header.destination_connection_id_length, header.source_connection_id,
          header.source_connection_id_length, writer)) {
    return false;
  }
  last_serialized_connection_id_ = header.destination_connection_id;

  // Append packet number.
  if (!AppendPacketNumber(header.packet_number_length, header.packet_number,
                          writer)) {
    return false;
  }

  if (!header.version_flag) {
    return true;
  }

  if (header.nonce != nullptr) {
    DCHECK(header.version_flag);
    DCHECK_EQ(ZERO_RTT_PROTECTED, header.long_packet_type);
    DCHECK_EQ(Perspective::IS_SERVER, perspective_);
    if (!writer->WriteBytes(header.nonce, kDiversificationNonceSize)) {
      return false;
    }
  }

  return true;
}

const QuicTime::Delta QuicFramer::CalculateTimestampFromWire(
    uint32_t time_delta_us) {
  // The new time_delta might have wrapped to the next epoch, or it
  // might have reverse wrapped to the previous epoch, or it might
  // remain in the same epoch. Select the time closest to the previous
  // time.
  //
  // epoch_delta is the delta between epochs. A delta is 4 bytes of
  // microseconds.
  const uint64_t epoch_delta = UINT64_C(1) << 32;
  uint64_t epoch = last_timestamp_.ToMicroseconds() & ~(epoch_delta - 1);
  // Wrapping is safe here because a wrapped value will not be ClosestTo below.
  uint64_t prev_epoch = epoch - epoch_delta;
  uint64_t next_epoch = epoch + epoch_delta;

  uint64_t time = ClosestTo(
      last_timestamp_.ToMicroseconds(), epoch + time_delta_us,
      ClosestTo(last_timestamp_.ToMicroseconds(), prev_epoch + time_delta_us,
                next_epoch + time_delta_us));

  return QuicTime::Delta::FromMicroseconds(time);
}

QuicPacketNumber QuicFramer::CalculatePacketNumberFromWire(
    QuicPacketNumberLength packet_number_length,
    QuicPacketNumber base_packet_number,
    QuicPacketNumber packet_number) const {
  // The new packet number might have wrapped to the next epoch, or
  // it might have reverse wrapped to the previous epoch, or it might
  // remain in the same epoch.  Select the packet number closest to the
  // next expected packet number, the previous packet number plus 1.

  // epoch_delta is the delta between epochs the packet number was serialized
  // with, so the correct value is likely the same epoch as the last sequence
  // number or an adjacent epoch.
  const QuicPacketNumber epoch_delta = UINT64_C(1)
                                       << (8 * packet_number_length);
  QuicPacketNumber next_packet_number = base_packet_number + 1;
  QuicPacketNumber epoch = base_packet_number & ~(epoch_delta - 1);
  QuicPacketNumber prev_epoch = epoch - epoch_delta;
  QuicPacketNumber next_epoch = epoch + epoch_delta;

  return ClosestTo(next_packet_number, epoch + packet_number,
                   ClosestTo(next_packet_number, prev_epoch + packet_number,
                             next_epoch + packet_number));
}

bool QuicFramer::ProcessPublicHeader(QuicDataReader* reader,
                                     QuicPacketHeader* header) {
  if (last_packet_is_ietf_quic_) {
    return ProcessIetfPacketHeader(reader, header);
  }
  uint8_t public_flags;
  if (!reader->ReadBytes(&public_flags, 1)) {
    set_detailed_error("Unable to read public flags.");
    return false;
  }

  header->reset_flag = (public_flags & PACKET_PUBLIC_FLAGS_RST) != 0;
  header->version_flag = (public_flags & PACKET_PUBLIC_FLAGS_VERSION) != 0;

  if (validate_flags_ && !header->version_flag &&
      public_flags > PACKET_PUBLIC_FLAGS_MAX) {
    set_detailed_error("Illegal public flags value.");
    return false;
  }

  if (header->reset_flag && header->version_flag) {
    set_detailed_error("Got version flag in reset packet");
    return false;
  }

  switch (public_flags & PACKET_PUBLIC_FLAGS_8BYTE_CONNECTION_ID) {
    case PACKET_PUBLIC_FLAGS_8BYTE_CONNECTION_ID:
      if (!reader->ReadConnectionId(&header->destination_connection_id)) {
        set_detailed_error("Unable to read ConnectionId.");
        return false;
      }
      header->destination_connection_id_length = PACKET_8BYTE_CONNECTION_ID;
      break;
    case PACKET_PUBLIC_FLAGS_0BYTE_CONNECTION_ID:
      header->destination_connection_id_length = PACKET_0BYTE_CONNECTION_ID;
      header->destination_connection_id = last_serialized_connection_id_;
      break;
  }

  header->packet_number_length = ReadSequenceNumberLength(
      public_flags >> kPublicHeaderSequenceNumberShift);

  // Read the version only if the packet is from the client.
  // version flag from the server means version negotiation packet.
  if (header->version_flag && perspective_ == Perspective::IS_SERVER) {
    QuicVersionLabel version_label;
    if (!reader->ReadTag(&version_label)) {
      set_detailed_error("Unable to read protocol version.");
      return false;
    }
    // TODO(rch): Use ReadUInt32() once QUIC_VERSION_35 is removed.
    version_label = QuicEndian::NetToHost32(version_label);

    // If the version from the new packet is the same as the version of this
    // framer, then the public flags should be set to something we understand.
    // If not, this raises an error.
    last_version_label_ = version_label;
    ParsedQuicVersion version = ParseQuicVersionLabel(version_label);
    if (version == version_ && public_flags > PACKET_PUBLIC_FLAGS_MAX) {
      set_detailed_error("Illegal public flags value.");
      return false;
    }
    header->version = version;
  }

  // A nonce should only be present in packets from the server to the client,
  // which are neither version negotiation nor public reset packets.
  if (public_flags & PACKET_PUBLIC_FLAGS_NONCE &&
      !(public_flags & PACKET_PUBLIC_FLAGS_VERSION) &&
      !(public_flags & PACKET_PUBLIC_FLAGS_RST) &&
      // The nonce flag from a client is ignored and is assumed to be an older
      // client indicating an eight-byte connection ID.
      perspective_ == Perspective::IS_CLIENT) {
    if (!reader->ReadBytes(reinterpret_cast<uint8_t*>(last_nonce_.data()),
                           last_nonce_.size())) {
      set_detailed_error("Unable to read nonce.");
      return false;
    }
    header->nonce = &last_nonce_;
  } else {
    header->nonce = nullptr;
  }

  return true;
}

// static
QuicPacketNumberLength QuicFramer::GetMinPacketNumberLength(
    QuicTransportVersion version,
    QuicPacketNumber packet_number) {
  if (packet_number < 1 << (PACKET_1BYTE_PACKET_NUMBER * 8)) {
    return PACKET_1BYTE_PACKET_NUMBER;
  } else if (packet_number < 1 << (PACKET_2BYTE_PACKET_NUMBER * 8)) {
    return PACKET_2BYTE_PACKET_NUMBER;
  } else if (packet_number < UINT64_C(1) << (PACKET_4BYTE_PACKET_NUMBER * 8)) {
    return PACKET_4BYTE_PACKET_NUMBER;
  } else {
    return PACKET_6BYTE_PACKET_NUMBER;
  }
}

// static
uint8_t QuicFramer::GetPacketNumberFlags(
    QuicPacketNumberLength packet_number_length) {
  switch (packet_number_length) {
    case PACKET_1BYTE_PACKET_NUMBER:
      return PACKET_FLAGS_1BYTE_PACKET;
    case PACKET_2BYTE_PACKET_NUMBER:
      return PACKET_FLAGS_2BYTE_PACKET;
    case PACKET_4BYTE_PACKET_NUMBER:
      return PACKET_FLAGS_4BYTE_PACKET;
    case PACKET_6BYTE_PACKET_NUMBER:
    case PACKET_8BYTE_PACKET_NUMBER:
      return PACKET_FLAGS_8BYTE_PACKET;
    default:
      QUIC_BUG << "Unreachable case statement.";
      return PACKET_FLAGS_8BYTE_PACKET;
  }
}

// static
QuicFramer::AckFrameInfo QuicFramer::GetAckFrameInfo(
    const QuicAckFrame& frame) {
  AckFrameInfo new_ack_info;
  if (frame.packets.Empty()) {
    return new_ack_info;
  }
  // The first block is the last interval. It isn't encoded with the gap-length
  // encoding, so skip it.
  new_ack_info.first_block_length = frame.packets.LastIntervalLength();
  auto itr = frame.packets.rbegin();
  QuicPacketNumber previous_start = itr->min();
  new_ack_info.max_block_length = itr->Length();
  ++itr;

  // Don't do any more work after getting information for 256 ACK blocks; any
  // more can't be encoded anyway.
  for (; itr != frame.packets.rend() &&
         new_ack_info.num_ack_blocks < std::numeric_limits<uint8_t>::max();
       previous_start = itr->min(), ++itr) {
    const auto& interval = *itr;
    const QuicPacketNumber total_gap = previous_start - interval.max();
    new_ack_info.num_ack_blocks +=
        (total_gap + std::numeric_limits<uint8_t>::max() - 1) /
        std::numeric_limits<uint8_t>::max();
    new_ack_info.max_block_length =
        std::max(new_ack_info.max_block_length, interval.Length());
  }
  return new_ack_info;
}

bool QuicFramer::ProcessUnauthenticatedHeader(QuicDataReader* encrypted_reader,
                                              QuicPacketHeader* header) {
  QuicPacketNumber base_packet_number = largest_packet_number_;

  if (!ProcessAndCalculatePacketNumber(
          encrypted_reader, header->packet_number_length, base_packet_number,
          &header->packet_number)) {
    set_detailed_error("Unable to read packet number.");
    return RaiseError(QUIC_INVALID_PACKET_HEADER);
  }

  if (header->packet_number == 0u) {
    set_detailed_error("packet numbers cannot be 0.");
    return RaiseError(QUIC_INVALID_PACKET_HEADER);
  }

  if (!visitor_->OnUnauthenticatedHeader(*header)) {
    set_detailed_error(
        "Visitor asked to stop processing of unauthenticated header.");
    return false;
  }
  return true;
}

bool QuicFramer::ProcessIetfPacketHeader(QuicDataReader* reader,
                                         QuicPacketHeader* header) {
  uint8_t type;
  if (!reader->ReadBytes(&type, 1)) {
    set_detailed_error("Unable to read type.");
    return false;
  }
  // Determine whether this is a long or short header.
  header->form = type & FLAGS_LONG_HEADER ? LONG_HEADER : SHORT_HEADER;
  last_header_form_ = header->form;
  if (header->form == LONG_HEADER) {
    // Get long packet type.
    header->long_packet_type =
        static_cast<QuicLongHeaderType>(type & kQuicLongHeaderTypeMask);
    if (header->long_packet_type < ZERO_RTT_PROTECTED ||
        header->long_packet_type > INITIAL) {
      header->long_packet_type = VERSION_NEGOTIATION;
    }
    QUIC_DVLOG(1) << ENDPOINT << "Received IETF long header: "
                  << QuicUtils::QuicLongHeaderTypetoString(
                         header->long_packet_type);
    // Version is always present in long headers.
    header->version_flag = true;
    // Packet number is 4 bytes in long headers.
    header->packet_number_length = PACKET_4BYTE_PACKET_NUMBER;
    // Long header packets received by client must include 8-byte source
    // connection ID, and those received by server must include 8-byte
    // destination connection ID.
    header->destination_connection_id_length =
        perspective_ == Perspective::IS_CLIENT ? PACKET_0BYTE_CONNECTION_ID
                                               : PACKET_8BYTE_CONNECTION_ID;
    header->source_connection_id_length = perspective_ == Perspective::IS_CLIENT
                                              ? PACKET_8BYTE_CONNECTION_ID
                                              : PACKET_0BYTE_CONNECTION_ID;
  } else {
    QUIC_DVLOG(1) << ENDPOINT << "Received IETF short header";
    QuicShortHeaderType short_type =
        static_cast<QuicShortHeaderType>(type & kQuicShortHeaderTypeMask);
    QUIC_DVLOG(1) << "short_type = " << short_type;
    if (short_type < SHORT_HEADER_1_BYTE_PACKET_NUMBER ||
        short_type > SHORT_HEADER_4_BYTE_PACKET_NUMBER) {
      set_detailed_error("Illegal short header type value.");
      return false;
    }
    // Version is not present in short headers.
    header->version_flag = false;
    // Connection ID length depends on the perspective. Client does not expect
    // destination connection ID, and server expects destination connection ID.
    header->destination_connection_id_length =
        perspective_ == Perspective::IS_CLIENT ? PACKET_0BYTE_CONNECTION_ID
                                               : PACKET_8BYTE_CONNECTION_ID;
    if (header->destination_connection_id_length ==
        PACKET_0BYTE_CONNECTION_ID) {
      header->destination_connection_id = last_serialized_connection_id_;
    }
    // Packet number length is determined by short header type.
    header->packet_number_length =
        ShortHeaderTypeToPacketNumberLength(short_type);
    QUIC_DVLOG(1) << "packet_number_length = " << header->packet_number_length;
  }

  QuicVersionLabel version_label;
  if (header->form == LONG_HEADER) {
    // Read version tag.
    if (!reader->ReadTag(&version_label)) {
      set_detailed_error("Unable to read protocol version.");
      return false;
    }
    // TODO(rch): Use ReadUInt32() once QUIC_VERSION_35 is removed.
    version_label = QuicEndian::NetToHost32(version_label);
    if (header->long_packet_type == VERSION_NEGOTIATION && version_label) {
      // Version negotiation is identified by the version field.
      set_detailed_error("Illegal long header type value.");
      return false;
    }
    header->version = ParseQuicVersionLabel(version_label);
    if (header->long_packet_type != VERSION_NEGOTIATION) {
      // Do not save version of version negotiation packet.
      last_version_label_ = version_label;
    }

    // Read and validate connection ID length.
    uint8_t connection_id_length;
    if (!reader->ReadBytes(&connection_id_length, 1)) {
      set_detailed_error("Unable to read ConnectionId length.");
      return false;
    }
    uint8_t dcil =
        (connection_id_length & kDestinationConnectionIdLengthMask) >> 4;
    uint8_t scil = connection_id_length & kSourceConnectionIdLengthMask;
    if ((dcil != 0 &&
         dcil != PACKET_8BYTE_CONNECTION_ID - kConnectionIdLengthAdjustment) ||
        (scil != 0 &&
         scil != PACKET_8BYTE_CONNECTION_ID - kConnectionIdLengthAdjustment) ||
        dcil == scil || (perspective_ == Perspective::IS_CLIENT && scil == 0) ||
        (perspective_ == Perspective::IS_SERVER && dcil == 0)) {
      // Long header packets received by client must include 8-byte source
      // connection ID, and those received by server must include 8-byte
      // destination connection ID.
      QUIC_DVLOG(1) << "dcil: " << static_cast<uint32_t>(dcil)
                    << ", scil: " << static_cast<uint32_t>(scil);
      set_detailed_error("Invalid ConnectionId length.");
      return false;
    }
  }

  // Read connection ID.
  if (header->destination_connection_id_length == PACKET_8BYTE_CONNECTION_ID &&
      !reader->ReadConnectionId(&header->destination_connection_id)) {
    set_detailed_error("Unable to read Destination ConnectionId.");
    return false;
  }

  if (header->source_connection_id_length == PACKET_8BYTE_CONNECTION_ID &&
      !reader->ReadConnectionId(&header->source_connection_id)) {
    set_detailed_error("Unable to read Source ConnectionId.");
    return false;
  }

  if (header->source_connection_id_length == PACKET_8BYTE_CONNECTION_ID) {
    // Set destination connection ID to source connection ID.
    DCHECK_EQ(0u, header->destination_connection_id);
    header->destination_connection_id = header->source_connection_id;
  }

  return true;
}

bool QuicFramer::ProcessAndCalculatePacketNumber(
    QuicDataReader* reader,
    QuicPacketNumberLength packet_number_length,
    QuicPacketNumber base_packet_number,
    QuicPacketNumber* packet_number) {
  QuicPacketNumber wire_packet_number;
  if (!reader->ReadBytesToUInt64(packet_number_length, &wire_packet_number)) {
    return false;
  }

  // TODO(ianswett): Explore the usefulness of trying multiple packet numbers
  // in case the first guess is incorrect.
  *packet_number = CalculatePacketNumberFromWire(
      packet_number_length, base_packet_number, wire_packet_number);
  return true;
}

bool QuicFramer::ProcessFrameData(QuicDataReader* reader,
                                  const QuicPacketHeader& header) {
  DCHECK_NE(QUIC_VERSION_99, version_.transport_version)
      << "Version 99 negotiated, but not processing frames as version 99.";
  if (reader->IsDoneReading()) {
    set_detailed_error("Packet has no frames.");
    return RaiseError(QUIC_MISSING_PAYLOAD);
  }
  while (!reader->IsDoneReading()) {
    uint8_t frame_type;
    if (!reader->ReadBytes(&frame_type, 1)) {
      set_detailed_error("Unable to read frame type.");
      return RaiseError(QUIC_INVALID_FRAME_DATA);
    }
    const uint8_t special_mask = transport_version() <= QUIC_VERSION_44
                                     ? kQuicFrameTypeBrokenMask
                                     : kQuicFrameTypeSpecialMask;
    if (frame_type & special_mask) {
      // Stream Frame
      if (frame_type & kQuicFrameTypeStreamMask) {
        QuicStreamFrame frame;
        if (!ProcessStreamFrame(reader, frame_type, &frame)) {
          return RaiseError(QUIC_INVALID_STREAM_DATA);
        }
        if (!visitor_->OnStreamFrame(frame)) {
          QUIC_DVLOG(1) << ENDPOINT
                        << "Visitor asked to stop further processing.";
          // Returning true since there was no parsing error.
          return true;
        }
        continue;
      }

      // Ack Frame
      if (frame_type & kQuicFrameTypeAckMask) {
        if (!ProcessAckFrame(reader, frame_type)) {
          return RaiseError(QUIC_INVALID_ACK_DATA);
        }
        continue;
      }

      // This was a special frame type that did not match any
      // of the known ones. Error.
      set_detailed_error("Illegal frame type.");
      QUIC_DLOG(WARNING) << ENDPOINT << "Illegal frame type: "
                         << static_cast<int>(frame_type);
      return RaiseError(QUIC_INVALID_FRAME_DATA);
    }

    switch (frame_type) {
      case PADDING_FRAME: {
        QuicPaddingFrame frame;
        ProcessPaddingFrame(reader, &frame);
        if (!visitor_->OnPaddingFrame(frame)) {
          QUIC_DVLOG(1) << "Visitor asked to stop further processing.";
          // Returning true since there was no parsing error.
          return true;
        }
        continue;
      }

      case RST_STREAM_FRAME: {
        QuicRstStreamFrame frame;
        if (!ProcessRstStreamFrame(reader, &frame)) {
          return RaiseError(QUIC_INVALID_RST_STREAM_DATA);
        }
        if (!visitor_->OnRstStreamFrame(frame)) {
          QUIC_DVLOG(1) << "Visitor asked to stop further processing.";
          // Returning true since there was no parsing error.
          return true;
        }
        continue;
      }

      case CONNECTION_CLOSE_FRAME: {
        QuicConnectionCloseFrame frame;
        if (!ProcessConnectionCloseFrame(reader, &frame)) {
          return RaiseError(QUIC_INVALID_CONNECTION_CLOSE_DATA);
        }

        if (!visitor_->OnConnectionCloseFrame(frame)) {
          QUIC_DVLOG(1) << ENDPOINT
                        << "Visitor asked to stop further processing.";
          // Returning true since there was no parsing error.
          return true;
        }
        continue;
      }

      case GOAWAY_FRAME: {
        QuicGoAwayFrame goaway_frame;
        if (!ProcessGoAwayFrame(reader, &goaway_frame)) {
          return RaiseError(QUIC_INVALID_GOAWAY_DATA);
        }
        if (!visitor_->OnGoAwayFrame(goaway_frame)) {
          QUIC_DVLOG(1) << ENDPOINT
                        << "Visitor asked to stop further processing.";
          // Returning true since there was no parsing error.
          return true;
        }
        continue;
      }

      case WINDOW_UPDATE_FRAME: {
        QuicWindowUpdateFrame window_update_frame;
        if (!ProcessWindowUpdateFrame(reader, &window_update_frame)) {
          return RaiseError(QUIC_INVALID_WINDOW_UPDATE_DATA);
        }
        if (!visitor_->OnWindowUpdateFrame(window_update_frame)) {
          QUIC_DVLOG(1) << ENDPOINT
                        << "Visitor asked to stop further processing.";
          // Returning true since there was no parsing error.
          return true;
        }
        continue;
      }

      case BLOCKED_FRAME: {
        QuicBlockedFrame blocked_frame;
        if (!ProcessBlockedFrame(reader, &blocked_frame)) {
          return RaiseError(QUIC_INVALID_BLOCKED_DATA);
        }
        if (!visitor_->OnBlockedFrame(blocked_frame)) {
          QUIC_DVLOG(1) << ENDPOINT
                        << "Visitor asked to stop further processing.";
          // Returning true since there was no parsing error.
          return true;
        }
        continue;
      }

      case STOP_WAITING_FRAME: {
        QuicStopWaitingFrame stop_waiting_frame;
        if (!ProcessStopWaitingFrame(reader, header, &stop_waiting_frame)) {
          return RaiseError(QUIC_INVALID_STOP_WAITING_DATA);
        }
        if (!visitor_->OnStopWaitingFrame(stop_waiting_frame)) {
          QUIC_DVLOG(1) << ENDPOINT
                        << "Visitor asked to stop further processing.";
          // Returning true since there was no parsing error.
          return true;
        }
        continue;
      }
      case PING_FRAME: {
        // Ping has no payload.
        QuicPingFrame ping_frame;
        if (!visitor_->OnPingFrame(ping_frame)) {
          QUIC_DVLOG(1) << ENDPOINT
                        << "Visitor asked to stop further processing.";
          // Returning true since there was no parsing error.
          return true;
        }
        continue;
      }
      case IETF_EXTENSION_MESSAGE_NO_LENGTH:
        QUIC_FALLTHROUGH_INTENDED;
      case IETF_EXTENSION_MESSAGE: {
        QuicMessageFrame message_frame;
        if (!ProcessMessageFrame(reader,
                                 frame_type == IETF_EXTENSION_MESSAGE_NO_LENGTH,
                                 &message_frame)) {
          return RaiseError(QUIC_INVALID_MESSAGE_DATA);
        }
        if (!visitor_->OnMessageFrame(message_frame)) {
          QUIC_DVLOG(1) << ENDPOINT
                        << "Visitor asked to stop further processing.";
          // Returning true since there was no parsing error.
          return true;
        }
        break;
      }

      default:
        set_detailed_error("Illegal frame type.");
        QUIC_DLOG(WARNING) << ENDPOINT << "Illegal frame type: "
                           << static_cast<int>(frame_type);
        return RaiseError(QUIC_INVALID_FRAME_DATA);
    }
  }

  return true;
}

bool QuicFramer::ProcessIetfFrameData(QuicDataReader* reader,
                                      const QuicPacketHeader& header) {
  DCHECK_EQ(QUIC_VERSION_99, version_.transport_version)
      << "Attempt to process frames as IETF frames but version is "
      << version_.transport_version << ", not 99.";
  if (reader->IsDoneReading()) {
    set_detailed_error("Packet has no frames.");
    return RaiseError(QUIC_MISSING_PAYLOAD);
  }
  while (!reader->IsDoneReading()) {
    uint64_t frame_type;
    // Will be the number of bytes into which frame_type was encoded.
    size_t encoded_bytes = reader->BytesRemaining();
    if (!reader->ReadVarInt62(&frame_type)) {
      set_detailed_error("Unable to read frame type.");
      return RaiseError(QUIC_INVALID_FRAME_DATA);
    }

    // Is now the number of bytes into which the frame type was encoded.
    encoded_bytes -= reader->BytesRemaining();

    // Check that the frame type is minimally encoded.
    if (encoded_bytes !=
        static_cast<size_t>(QuicDataWriter::GetVarInt62Len(frame_type))) {
      // The frame type was not minimally encoded.
      set_detailed_error("Frame type not minimally encoded.");
      return RaiseError(IETF_QUIC_PROTOCOL_VIOLATION);
    }

    if (IS_IETF_STREAM_FRAME(frame_type)) {
      QuicStreamFrame frame;
      if (!ProcessIetfStreamFrame(reader, frame_type, &frame)) {
        return RaiseError(QUIC_INVALID_STREAM_DATA);
      }
      if (!visitor_->OnStreamFrame(frame)) {
        QUIC_DVLOG(1) << ENDPOINT
                      << "Visitor asked to stop further processing.";
        // Returning true since there was no parsing error.
        return true;
      }
    } else {
      switch (frame_type) {
        case IETF_PADDING: {
          QuicPaddingFrame frame;
          ProcessPaddingFrame(reader, &frame);
          if (!visitor_->OnPaddingFrame(frame)) {
            QUIC_DVLOG(1) << "Visitor asked to stop further processing.";
            // Returning true since there was no parsing error.
            return true;
          }
          break;
        }
        case IETF_RST_STREAM: {
          QuicRstStreamFrame frame;
          if (!ProcessIetfResetStreamFrame(reader, &frame)) {
            return RaiseError(QUIC_INVALID_RST_STREAM_DATA);
          }
          if (!visitor_->OnRstStreamFrame(frame)) {
            QUIC_DVLOG(1) << "Visitor asked to stop further processing.";
            // Returning true since there was no parsing error.
            return true;
          }
          break;
        }
        case IETF_CONNECTION_CLOSE: {
          QuicConnectionCloseFrame frame;
          if (!ProcessIetfConnectionCloseFrame(reader, &frame)) {
            return RaiseError(QUIC_INVALID_CONNECTION_CLOSE_DATA);
          }
          if (!visitor_->OnConnectionCloseFrame(frame)) {
            QUIC_DVLOG(1) << "Visitor asked to stop further processing.";
            // Returning true since there was no parsing error.
            return true;
          }
          break;
        }
        case IETF_APPLICATION_CLOSE: {
          QuicApplicationCloseFrame frame;
          if (!ProcessApplicationCloseFrame(reader, &frame)) {
            return RaiseError(QUIC_INVALID_APPLICATION_CLOSE_DATA);
          }
          if (!visitor_->OnApplicationCloseFrame(frame)) {
            QUIC_DVLOG(1) << "Visitor asked to stop further processing.";
            // Returning true since there was no parsing error.
            return true;
          }
          break;
        }
        case IETF_MAX_DATA: {
          QuicWindowUpdateFrame frame;
          if (!ProcessMaxDataFrame(reader, &frame)) {
            return RaiseError(QUIC_INVALID_MAX_DATA_FRAME_DATA);
          }
          // TODO(fkastenholz): Or should we create a new visitor function,
          // OnMaxDataFrame()?
          if (!visitor_->OnWindowUpdateFrame(frame)) {
            QUIC_DVLOG(1) << "Visitor asked to stop further processing.";
            // Returning true since there was no parsing error.
            return true;
          }
          break;
        }
        case IETF_MAX_STREAM_DATA: {
          QuicWindowUpdateFrame frame;
          if (!ProcessMaxStreamDataFrame(reader, &frame)) {
            return RaiseError(QUIC_INVALID_MAX_STREAM_DATA_FRAME_DATA);
          }
          // TODO(fkastenholz): Or should we create a new visitor function,
          // OnMaxStreamDataFrame()?
          if (!visitor_->OnWindowUpdateFrame(frame)) {
            QUIC_DVLOG(1) << "Visitor asked to stop further processing.";
            // Returning true since there was no parsing error.
            return true;
          }
          break;
        }
        case IETF_MAX_STREAM_ID: {
          QuicMaxStreamIdFrame frame;
          if (!ProcessMaxStreamIdFrame(reader, &frame)) {
            return RaiseError(QUIC_MAX_STREAM_ID_DATA);
          }
          if (!visitor_->OnMaxStreamIdFrame(frame)) {
            QUIC_DVLOG(1) << "Visitor asked to stop further processing.";
            // Returning true since there was no parsing error.
            return true;
          }
          break;
        }
        case IETF_PING: {
          // Ping has no payload.
          QuicPingFrame ping_frame;
          if (!visitor_->OnPingFrame(ping_frame)) {
            QUIC_DVLOG(1) << "Visitor asked to stop further processing.";
            // Returning true since there was no parsing error.
            return true;
          }
          break;
        }
        case IETF_BLOCKED: {
          QuicBlockedFrame frame;
          if (!ProcessIetfBlockedFrame(reader, &frame)) {
            return RaiseError(QUIC_INVALID_BLOCKED_DATA);
          }
          if (!visitor_->OnBlockedFrame(frame)) {
            QUIC_DVLOG(1) << "Visitor asked to stop further processing.";
            // Returning true since there was no parsing error.
            return true;
          }
          break;
        }
        case IETF_STREAM_BLOCKED: {
          QuicBlockedFrame frame;
          if (!ProcessStreamBlockedFrame(reader, &frame)) {
            return RaiseError(QUIC_INVALID_STREAM_BLOCKED_DATA);
          }
          if (!visitor_->OnBlockedFrame(frame)) {
            QUIC_DVLOG(1) << "Visitor asked to stop further processing.";
            // Returning true since there was no parsing error.
            return true;
          }
          break;
        }
        case IETF_STREAM_ID_BLOCKED: {
          QuicStreamIdBlockedFrame frame;
          if (!ProcessStreamIdBlockedFrame(reader, &frame)) {
            return RaiseError(QUIC_STREAM_ID_BLOCKED_DATA);
          }
          if (!visitor_->OnStreamIdBlockedFrame(frame)) {
            QUIC_DVLOG(1) << "Visitor asked to stop further processing.";
            // Returning true since there was no parsing error.
            return true;
          }
          break;
        }
        case IETF_NEW_CONNECTION_ID: {
          QuicNewConnectionIdFrame frame;
          if (!ProcessNewConnectionIdFrame(reader, &frame)) {
            return RaiseError(QUIC_INVALID_NEW_CONNECTION_ID_DATA);
          }
          if (!visitor_->OnNewConnectionIdFrame(frame)) {
            QUIC_DVLOG(1) << "Visitor asked to stop further processing.";
            // Returning true since there was no parsing error.
            return true;
          }
          break;
        }
        case IETF_RETIRE_CONNECTION_ID: {
          QuicRetireConnectionIdFrame frame;
          if (!ProcessRetireConnectionIdFrame(reader, &frame)) {
            return RaiseError(QUIC_INVALID_RETIRE_CONNECTION_ID_DATA);
          }
          if (!visitor_->OnRetireConnectionIdFrame(frame)) {
            QUIC_DVLOG(1) << "Visitor asked to stop further processing.";
            // Returning true since there was no parsing error.
            return true;
          }
          break;
        }
        case IETF_NEW_TOKEN: {
          QuicNewTokenFrame frame;
          if (!ProcessNewTokenFrame(reader, &frame)) {
            return RaiseError(QUIC_INVALID_NEW_TOKEN);
          }
          if (!visitor_->OnNewTokenFrame(frame)) {
            QUIC_DVLOG(1) << "Visitor asked to stop further processing.";
            // Returning true since there was no parsing error.
            return true;
          }
          break;
        }
        case IETF_STOP_SENDING: {
          QuicStopSendingFrame frame;
          if (!ProcessStopSendingFrame(reader, &frame)) {
            return RaiseError(QUIC_INVALID_STOP_SENDING_FRAME_DATA);
          }
          if (!visitor_->OnStopSendingFrame(frame)) {
            QUIC_DVLOG(1) << "Visitor asked to stop further processing.";
            // Returning true since there was no parsing error.
            return true;
          }
          break;
        }
        case IETF_ACK_ECN:
        case IETF_ACK: {
          QuicAckFrame frame;
          if (!ProcessIetfAckFrame(reader, frame_type, &frame)) {
            return RaiseError(QUIC_INVALID_ACK_DATA);
          }
          break;
        }
        case IETF_PATH_CHALLENGE: {
          QuicPathChallengeFrame frame;
          if (!ProcessPathChallengeFrame(reader, &frame)) {
            return RaiseError(QUIC_INVALID_PATH_CHALLENGE_DATA);
          }
          if (!visitor_->OnPathChallengeFrame(frame)) {
            QUIC_DVLOG(1) << "Visitor asked to stop further processing.";
            // Returning true since there was no parsing error.
            return true;
          }
          break;
        }
        case IETF_PATH_RESPONSE: {
          QuicPathResponseFrame frame;
          if (!ProcessPathResponseFrame(reader, &frame)) {
            return RaiseError(QUIC_INVALID_PATH_RESPONSE_DATA);
          }
          if (!visitor_->OnPathResponseFrame(frame)) {
            QUIC_DVLOG(1) << "Visitor asked to stop further processing.";
            // Returning true since there was no parsing error.
            return true;
          }
          break;
        }
        case IETF_EXTENSION_MESSAGE_NO_LENGTH:
          QUIC_FALLTHROUGH_INTENDED;
        case IETF_EXTENSION_MESSAGE: {
          QuicMessageFrame message_frame;
          if (!ProcessMessageFrame(
                  reader, frame_type == IETF_EXTENSION_MESSAGE_NO_LENGTH,
                  &message_frame)) {
            return RaiseError(QUIC_INVALID_MESSAGE_DATA);
          }
          if (!visitor_->OnMessageFrame(message_frame)) {
            QUIC_DVLOG(1) << ENDPOINT
                          << "Visitor asked to stop further processing.";
            // Returning true since there was no parsing error.
            return true;
          }
          break;
        }
        case IETF_CRYPTO: {
          QuicCryptoFrame frame;
          if (!ProcessCryptoFrame(reader, &frame)) {
            return RaiseError(QUIC_INVALID_FRAME_DATA);
          }
          if (!visitor_->OnCryptoFrame(frame)) {
            QUIC_DVLOG(1) << "Visitor asked to stop further processing.";
            // Returning true since there was no parsing error.
            return true;
          }
          break;
        }

        default:
          set_detailed_error("Illegal frame type.");
          QUIC_DLOG(WARNING)
              << ENDPOINT
              << "Illegal frame type: " << static_cast<int>(frame_type);
          return RaiseError(QUIC_INVALID_FRAME_DATA);
      }
    }
  }
  return true;
}

namespace {
// Create a mask that sets the last |num_bits| to 1 and the rest to 0.
inline uint8_t GetMaskFromNumBits(uint8_t num_bits) {
  return (1u << num_bits) - 1;
}

// Extract |num_bits| from |flags| offset by |offset|.
uint8_t ExtractBits(uint8_t flags, uint8_t num_bits, uint8_t offset) {
  return (flags >> offset) & GetMaskFromNumBits(num_bits);
}

// Extract the bit at position |offset| from |flags| as a bool.
bool ExtractBit(uint8_t flags, uint8_t offset) {
  return ((flags >> offset) & GetMaskFromNumBits(1)) != 0;
}

// Set |num_bits|, offset by |offset| to |val| in |flags|.
void SetBits(uint8_t* flags, uint8_t val, uint8_t num_bits, uint8_t offset) {
  DCHECK_LE(val, GetMaskFromNumBits(num_bits));
  *flags |= val << offset;
}

// Set the bit at position |offset| to |val| in |flags|.
void SetBit(uint8_t* flags, bool val, uint8_t offset) {
  SetBits(flags, val ? 1 : 0, 1, offset);
}
}  // namespace

bool QuicFramer::ProcessStreamFrame(QuicDataReader* reader,
                                    uint8_t frame_type,
                                    QuicStreamFrame* frame) {
  uint8_t stream_flags = frame_type;

  uint8_t stream_id_length = 0;
  uint8_t offset_length = 4;
  bool has_data_length = true;
  stream_flags &= ~kQuicFrameTypeStreamMask;

  // Read from right to left: StreamID, Offset, Data Length, Fin.
  stream_id_length = (stream_flags & kQuicStreamIDLengthMask) + 1;
  stream_flags >>= kQuicStreamIdShift;

  offset_length = (stream_flags & kQuicStreamOffsetMask);
  // There is no encoding for 1 byte, only 0 and 2 through 8.
  if (offset_length > 0) {
    offset_length += 1;
  }
  stream_flags >>= kQuicStreamShift;

  has_data_length =
      (stream_flags & kQuicStreamDataLengthMask) == kQuicStreamDataLengthMask;
  stream_flags >>= kQuicStreamDataLengthShift;

  frame->fin = (stream_flags & kQuicStreamFinMask) == kQuicStreamFinShift;

  uint64_t stream_id;
  if (!reader->ReadBytesToUInt64(stream_id_length, &stream_id)) {
    set_detailed_error("Unable to read stream_id.");
    return false;
  }
  frame->stream_id = static_cast<QuicStreamId>(stream_id);

  if (!reader->ReadBytesToUInt64(offset_length, &frame->offset)) {
    set_detailed_error("Unable to read offset.");
    return false;
  }

  // TODO(ianswett): Don't use QuicStringPiece as an intermediary.
  QuicStringPiece data;
  if (has_data_length) {
    if (!reader->ReadStringPiece16(&data)) {
      set_detailed_error("Unable to read frame data.");
      return false;
    }
  } else {
    if (!reader->ReadStringPiece(&data, reader->BytesRemaining())) {
      set_detailed_error("Unable to read frame data.");
      return false;
    }
  }
  frame->data_buffer = data.data();
  frame->data_length = static_cast<uint16_t>(data.length());

  return true;
}

bool QuicFramer::ProcessIetfStreamFrame(QuicDataReader* reader,
                                        uint8_t frame_type,
                                        QuicStreamFrame* frame) {
  // Read stream id from the frame. It's always present.
  if (!reader->ReadVarIntStreamId(&frame->stream_id)) {
    set_detailed_error("Unable to read stream_id.");
    return false;
  }

  // If we have a data offset, read it. If not, set to 0.
  if (frame_type & IETF_STREAM_FRAME_OFF_BIT) {
    if (!reader->ReadVarInt62(&frame->offset)) {
      set_detailed_error("Unable to read stream data offset.");
      return false;
    }
  } else {
    // no offset in the frame, ensure it's 0 in the Frame.
    frame->offset = 0;
  }

  // If we have a data length, read it. If not, set to 0.
  if (frame_type & IETF_STREAM_FRAME_LEN_BIT) {
    QuicIetfStreamDataLength length;
    if (!reader->ReadVarInt62(&length)) {
      set_detailed_error("Unable to read stream data length.");
      return false;
    }
    if (length > 0xffff) {
      set_detailed_error("Stream data length is too large.");
      return false;
    }
    frame->data_length = length;
  } else {
    // no length in the frame, it is the number of bytes remaining in the
    // packet.
    frame->data_length = reader->BytesRemaining();
  }

  if (frame_type & IETF_STREAM_FRAME_FIN_BIT) {
    frame->fin = true;
  } else {
    frame->fin = false;
  }

  // TODO(ianswett): Don't use QuicStringPiece as an intermediary.
  QuicStringPiece data;
  if (!reader->ReadStringPiece(&data, frame->data_length)) {
    set_detailed_error("Unable to read frame data.");
    return false;
  }
  frame->data_buffer = data.data();
  frame->data_length = static_cast<QuicIetfStreamDataLength>(data.length());

  return true;
}

bool QuicFramer::ProcessCryptoFrame(QuicDataReader* reader,
                                    QuicCryptoFrame* frame) {
  if (!reader->ReadVarInt62(&frame->offset)) {
    set_detailed_error("Unable to read crypto data offset.");
    return false;
  }
  uint64_t len;
  if (!reader->ReadVarInt62(&len) ||
      len > std::numeric_limits<QuicPacketLength>::max()) {
    set_detailed_error("Invalid data length.");
    return false;
  }
  frame->data_length = len;

  // TODO(ianswett): Don't use QuicStringPiece as an intermediary.
  QuicStringPiece data;
  if (!reader->ReadStringPiece(&data, frame->data_length)) {
    set_detailed_error("Unable to read frame data.");
    return false;
  }
  frame->data_buffer = data.data();
  return true;
}

bool QuicFramer::ProcessAckFrame(QuicDataReader* reader, uint8_t frame_type) {
  const bool has_ack_blocks =
      ExtractBit(frame_type, kQuicHasMultipleAckBlocksOffset);
  uint8_t num_ack_blocks = 0;
  uint8_t num_received_packets = 0;

  // Determine the two lengths from the frame type: largest acked length,
  // ack block length.
  const QuicPacketNumberLength ack_block_length = ReadAckPacketNumberLength(
      version_.transport_version,
      ExtractBits(frame_type, kQuicSequenceNumberLengthNumBits,
                  kActBlockLengthOffset));
  const QuicPacketNumberLength largest_acked_length = ReadAckPacketNumberLength(
      version_.transport_version,
      ExtractBits(frame_type, kQuicSequenceNumberLengthNumBits,
                  kLargestAckedOffset));

  QuicPacketNumber largest_acked;
  if (!reader->ReadBytesToUInt64(largest_acked_length, &largest_acked)) {
    set_detailed_error("Unable to read largest acked.");
    return false;
  }

  uint64_t ack_delay_time_us;
  if (!reader->ReadUFloat16(&ack_delay_time_us)) {
    set_detailed_error("Unable to read ack delay time.");
    return false;
  }

  if (!visitor_->OnAckFrameStart(
          largest_acked,
          ack_delay_time_us == kUFloat16MaxValue
              ? QuicTime::Delta::Infinite()
              : QuicTime::Delta::FromMicroseconds(ack_delay_time_us))) {
    // The visitor suppresses further processing of the packet. Although this is
    // not a parsing error, returns false as this is in middle of processing an
    // ack frame,
    set_detailed_error("Visitor suppresses further processing of ack frame.");
    return false;
  }

  if (has_ack_blocks && !reader->ReadUInt8(&num_ack_blocks)) {
    set_detailed_error("Unable to read num of ack blocks.");
    return false;
  }

  uint64_t first_block_length;
  if (!reader->ReadBytesToUInt64(ack_block_length, &first_block_length)) {
    set_detailed_error("Unable to read first ack block length.");
    return false;
  }

  if (first_block_length == 0) {
    // For non-empty ACKs, the first block length must be non-zero.
    if (largest_acked != 0 || num_ack_blocks != 0) {
      set_detailed_error(
          QuicStrCat("First block length is zero but ACK is "
                     "not empty. largest acked is ",
                     largest_acked, ", num ack blocks is ",
                     QuicTextUtils::Uint64ToString(num_ack_blocks), ".")
              .c_str());
      return false;
    }
  }

  if (first_block_length > largest_acked + 1) {
    set_detailed_error(QuicStrCat("Underflow with first ack block length ",
                                  first_block_length, " largest acked is ",
                                  largest_acked, ".")
                           .c_str());
    return false;
  }

  QuicPacketNumber first_received = largest_acked + 1 - first_block_length;
  if (!visitor_->OnAckRange(first_received, largest_acked + 1)) {
    // The visitor suppresses further processing of the packet. Although
    // this is not a parsing error, returns false as this is in middle
    // of processing an ack frame,
    set_detailed_error("Visitor suppresses further processing of ack frame.");
    return false;
  }

  if (num_ack_blocks > 0) {
    for (size_t i = 0; i < num_ack_blocks; ++i) {
      uint8_t gap = 0;
      if (!reader->ReadUInt8(&gap)) {
        set_detailed_error("Unable to read gap to next ack block.");
        return false;
      }
      uint64_t current_block_length;
      if (!reader->ReadBytesToUInt64(ack_block_length, &current_block_length)) {
        set_detailed_error("Unable to ack block length.");
        return false;
      }
      if (first_received < gap + current_block_length) {
        set_detailed_error(
            QuicStrCat("Underflow with ack block length ", current_block_length,
                       ", end of block is ", first_received - gap, ".")
                .c_str());
        return false;
      }

      first_received -= (gap + current_block_length);
      if (current_block_length > 0) {
        if (!visitor_->OnAckRange(first_received,
                                  first_received + current_block_length)) {
          // The visitor suppresses further processing of the packet. Although
          // this is not a parsing error, returns false as this is in middle
          // of processing an ack frame,
          set_detailed_error(
              "Visitor suppresses further processing of ack frame.");
          return false;
        }
      }
    }
  }

  if (!reader->ReadUInt8(&num_received_packets)) {
    set_detailed_error("Unable to read num received packets.");
    return false;
  }

  if (!ProcessTimestampsInAckFrame(num_received_packets, largest_acked,
                                   reader)) {
    return false;
  }

  // Done processing the ACK frame.
  return visitor_->OnAckFrameEnd(first_received);
}

bool QuicFramer::ProcessTimestampsInAckFrame(uint8_t num_received_packets,
                                             QuicPacketNumber largest_acked,
                                             QuicDataReader* reader) {
  if (num_received_packets == 0) {
    return true;
  }
  uint8_t delta_from_largest_observed;
  if (!reader->ReadUInt8(&delta_from_largest_observed)) {
    set_detailed_error("Unable to read sequence delta in received packets.");
    return false;
  }

  // Time delta from the framer creation.
  uint32_t time_delta_us;
  if (!reader->ReadUInt32(&time_delta_us)) {
    set_detailed_error("Unable to read time delta in received packets.");
    return false;
  }

  QuicPacketNumber seq_num = largest_acked - delta_from_largest_observed;
  if (process_timestamps_) {
    last_timestamp_ = CalculateTimestampFromWire(time_delta_us);

    visitor_->OnAckTimestamp(seq_num, creation_time_ + last_timestamp_);
  }

  for (uint8_t i = 1; i < num_received_packets; ++i) {
    if (!reader->ReadUInt8(&delta_from_largest_observed)) {
      set_detailed_error("Unable to read sequence delta in received packets.");
      return false;
    }
    seq_num = largest_acked - delta_from_largest_observed;

    // Time delta from the previous timestamp.
    uint64_t incremental_time_delta_us;
    if (!reader->ReadUFloat16(&incremental_time_delta_us)) {
      set_detailed_error(
          "Unable to read incremental time delta in received packets.");
      return false;
    }

    if (process_timestamps_) {
      last_timestamp_ = last_timestamp_ + QuicTime::Delta::FromMicroseconds(
                                              incremental_time_delta_us);
      visitor_->OnAckTimestamp(seq_num, creation_time_ + last_timestamp_);
    }
  }
  return true;
}

bool QuicFramer::ProcessIetfAckFrame(QuicDataReader* reader,
                                     uint64_t frame_type,
                                     QuicAckFrame* ack_frame) {
  QuicPacketNumber largest_acked;
  if (!reader->ReadVarInt62(&largest_acked)) {
    set_detailed_error("Unable to read largest acked.");
    return false;
  }
  ack_frame->largest_acked = static_cast<QuicPacketNumber>(largest_acked);
  uint64_t ack_delay_time_in_us;
  if (!reader->ReadVarInt62(&ack_delay_time_in_us)) {
    set_detailed_error("Unable to read ack delay time.");
    return false;
  }

  // TODO(fkastenholz) when we get real IETF QUIC, need to get
  // the currect shift from the transport parameters.
  if (ack_delay_time_in_us == kVarInt62MaxValue) {
    ack_frame->ack_delay_time = QuicTime::Delta::Infinite();
  } else {
    ack_delay_time_in_us = (ack_delay_time_in_us << kIetfAckTimestampShift);
    ack_frame->ack_delay_time =
        QuicTime::Delta::FromMicroseconds(ack_delay_time_in_us);
  }
  if (frame_type == IETF_ACK_ECN) {
    ack_frame->ecn_counters_populated = true;
    if (!reader->ReadVarInt62(&ack_frame->ect_0_count)) {
      set_detailed_error("Unable to read ack ect_0_count.");
      return false;
    }
    if (!reader->ReadVarInt62(&ack_frame->ect_1_count)) {
      set_detailed_error("Unable to read ack ect_1_count.");
      return false;
    }
    if (!reader->ReadVarInt62(&ack_frame->ecn_ce_count)) {
      set_detailed_error("Unable to read ack ecn_ce_count.");
      return false;
    }
  } else {
    ack_frame->ecn_counters_populated = false;
    ack_frame->ect_0_count = 0;
    ack_frame->ect_1_count = 0;
    ack_frame->ecn_ce_count = 0;
  }
  if (!visitor_->OnAckFrameStart(largest_acked, ack_frame->ack_delay_time)) {
    // The visitor suppresses further processing of the packet. Although this is
    // not a parsing error, returns false as this is in middle of processing an
    // ACK frame.
    set_detailed_error("Visitor suppresses further processing of ACK frame.");
    return false;
  }

  // Get number of ACK blocks from the packet.
  uint64_t ack_block_count;
  if (!reader->ReadVarInt62(&ack_block_count)) {
    set_detailed_error("Unable to read ack block count.");
    return false;
  }
  // There always is a first ACK block, which is the (number of packets being
  // acked)-1, up to and including the packet at largest_acked. Therefore if the
  // value is 0, then only largest is acked. If it is 1, then largest-1,
  // largest] are acked, etc
  uint64_t ack_block_value;
  if (!reader->ReadVarInt62(&ack_block_value)) {
    set_detailed_error("Unable to read first ack block length.");
    return false;
  }
  // Calculate the packets being acked in the first block.
  //  +1 because AddRange implementation requires [low,high)
  QuicPacketNumber block_high = largest_acked + 1;
  QuicPacketNumber block_low = largest_acked - ack_block_value;

  // ack_block_value is the number of packets preceding the
  // largest_acked packet which are in the block being acked. Thus,
  // its maximum value is largest_acked-1. Test this, reporting an
  // error if the value is wrong.
  if (ack_block_value > largest_acked) {
    set_detailed_error(QuicStrCat("Underflow with first ack block length ",
                                  ack_block_value + 1, " largest acked is ",
                                  largest_acked, ".")
                           .c_str());
    return false;
  }

  if (!visitor_->OnAckRange(block_low, block_high)) {
    // The visitor suppresses further processing of the packet. Although
    // this is not a parsing error, returns false as this is in middle
    // of processing an ACK frame.
    set_detailed_error("Visitor suppresses further processing of ACK frame.");
    return false;
  }

  while (ack_block_count != 0) {
    uint64_t gap_block_value;
    // Get the sizes of the gap and ack blocks,
    if (!reader->ReadVarInt62(&gap_block_value)) {
      set_detailed_error("Unable to read gap block value.");
      return false;
    }
    // It's an error if the gap is larger than the space from packet
    // number 0 to the start of the block that's just been acked, PLUS
    // there must be space for at least 1 packet to be acked. For
    // example, if block_low is 10 and gap_block_value is 9, it means
    // the gap block is 10 packets long, leaving no room for a packet
    // to be acked. Thus, gap_block_value+2 can not be larger than
    // block_low.
    // The test is written this way to detect wrap-arounds.
    if ((gap_block_value + 2) > block_low) {
      set_detailed_error(
          QuicStrCat("Underflow with gap block length ", gap_block_value + 1,
                     " previous ack block start is ", block_low, ".")
              .c_str());
      return false;
    }

    // Adjust block_high to be the top of the next ack block.
    // There is a gap of |gap_block_value| packets between the bottom
    // of ack block N and top of block N+1.  Note that gap_block_value
    // is he size of the gap minus 1 (per the QUIC protocol), and
    // block_high is the packet number of the first packet of the gap
    // (per the implementation of OnAckRange/AddAckRange, below).
    block_high = block_low - 1 - gap_block_value;

    if (!reader->ReadVarInt62(&ack_block_value)) {
      set_detailed_error("Unable to read ack block value.");
      return false;
    }
    if (ack_block_value > (block_high - 1)) {
      set_detailed_error(
          QuicStrCat("Underflow with ack block length ", ack_block_value + 1,
                     " latest ack block end is ", block_high - 1, ".")
              .c_str());
      return false;
    }
    // Calculate the low end of the new nth ack block. The +1 is
    // because the encoded value is the blocksize-1.
    block_low = block_high - 1 - ack_block_value;
    if (!visitor_->OnAckRange(block_low, block_high)) {
      // The visitor suppresses further processing of the packet. Although
      // this is not a parsing error, returns false as this is in middle
      // of processing an ACK frame.
      set_detailed_error("Visitor suppresses further processing of ACK frame.");
      return false;
    }

    // Another one done.
    ack_block_count--;
  }

  return visitor_->OnAckFrameEnd(block_low);
}

bool QuicFramer::ProcessStopWaitingFrame(QuicDataReader* reader,
                                         const QuicPacketHeader& header,
                                         QuicStopWaitingFrame* stop_waiting) {
  QuicPacketNumber least_unacked_delta;
  if (!reader->ReadBytesToUInt64(header.packet_number_length,
                                 &least_unacked_delta)) {
    set_detailed_error("Unable to read least unacked delta.");
    return false;
  }
  if (header.packet_number < least_unacked_delta) {
    set_detailed_error("Invalid unacked delta.");
    return false;
  }
  stop_waiting->least_unacked = header.packet_number - least_unacked_delta;

  return true;
}

bool QuicFramer::ProcessRstStreamFrame(QuicDataReader* reader,
                                       QuicRstStreamFrame* frame) {
  if (!reader->ReadUInt32(&frame->stream_id)) {
    set_detailed_error("Unable to read stream_id.");
    return false;
  }

  if (!reader->ReadUInt64(&frame->byte_offset)) {
    set_detailed_error("Unable to read rst stream sent byte offset.");
    return false;
  }

  uint32_t error_code;
  if (!reader->ReadUInt32(&error_code)) {
    set_detailed_error("Unable to read rst stream error code.");
    return false;
  }

  if (error_code >= QUIC_STREAM_LAST_ERROR) {
    // Ignore invalid stream error code if any.
    error_code = QUIC_STREAM_LAST_ERROR;
  }

  frame->error_code = static_cast<QuicRstStreamErrorCode>(error_code);

  return true;
}

bool QuicFramer::ProcessConnectionCloseFrame(QuicDataReader* reader,
                                             QuicConnectionCloseFrame* frame) {
  uint32_t error_code;
  if (!reader->ReadUInt32(&error_code)) {
    set_detailed_error("Unable to read connection close error code.");
    return false;
  }

  if (error_code >= QUIC_LAST_ERROR) {
    // Ignore invalid QUIC error code if any.
    error_code = QUIC_LAST_ERROR;
  }

  frame->error_code = static_cast<QuicErrorCode>(error_code);

  QuicStringPiece error_details;
  if (!reader->ReadStringPiece16(&error_details)) {
    set_detailed_error("Unable to read connection close error details.");
    return false;
  }
  frame->error_details = QuicString(error_details);

  return true;
}

bool QuicFramer::ProcessGoAwayFrame(QuicDataReader* reader,
                                    QuicGoAwayFrame* frame) {
  uint32_t error_code;
  if (!reader->ReadUInt32(&error_code)) {
    set_detailed_error("Unable to read go away error code.");
    return false;
  }

  if (error_code >= QUIC_LAST_ERROR) {
    // Ignore invalid QUIC error code if any.
    error_code = QUIC_LAST_ERROR;
  }
  frame->error_code = static_cast<QuicErrorCode>(error_code);

  uint32_t stream_id;
  if (!reader->ReadUInt32(&stream_id)) {
    set_detailed_error("Unable to read last good stream id.");
    return false;
  }
  frame->last_good_stream_id = static_cast<QuicStreamId>(stream_id);

  QuicStringPiece reason_phrase;
  if (!reader->ReadStringPiece16(&reason_phrase)) {
    set_detailed_error("Unable to read goaway reason.");
    return false;
  }
  frame->reason_phrase = QuicString(reason_phrase);

  return true;
}

bool QuicFramer::ProcessWindowUpdateFrame(QuicDataReader* reader,
                                          QuicWindowUpdateFrame* frame) {
  if (!reader->ReadUInt32(&frame->stream_id)) {
    set_detailed_error("Unable to read stream_id.");
    return false;
  }

  if (!reader->ReadUInt64(&frame->byte_offset)) {
    set_detailed_error("Unable to read window byte_offset.");
    return false;
  }

  return true;
}

bool QuicFramer::ProcessBlockedFrame(QuicDataReader* reader,
                                     QuicBlockedFrame* frame) {
  DCHECK_NE(QUIC_VERSION_99, version_.transport_version)
      << "Attempt to process non-IETF frames but version is 99";

  if (!reader->ReadUInt32(&frame->stream_id)) {
    set_detailed_error("Unable to read stream_id.");
    return false;
  }

  return true;
}

void QuicFramer::ProcessPaddingFrame(QuicDataReader* reader,
                                     QuicPaddingFrame* frame) {
  if (version_.transport_version == QUIC_VERSION_35) {
    frame->num_padding_bytes = reader->BytesRemaining() + 1;
    reader->ReadRemainingPayload();
    return;
  }
  // Type byte has been read.
  frame->num_padding_bytes = 1;
  uint8_t next_byte;
  while (!reader->IsDoneReading() && reader->PeekByte() == 0x00) {
    reader->ReadBytes(&next_byte, 1);
    DCHECK_EQ(0x00, next_byte);
    ++frame->num_padding_bytes;
  }
}

bool QuicFramer::ProcessMessageFrame(QuicDataReader* reader,
                                     bool no_message_length,
                                     QuicMessageFrame* frame) {
  if (no_message_length) {
    frame->message_data = reader->ReadRemainingPayload();
    return true;
  }

  uint64_t message_length;
  if (!reader->ReadVarInt62(&message_length)) {
    set_detailed_error("Unable to read message length");
    return false;
  }
  if (!reader->ReadStringPiece(&frame->message_data, message_length)) {
    set_detailed_error("Unable to read message data");
    return false;
  }

  return true;
}

// static
QuicStringPiece QuicFramer::GetAssociatedDataFromEncryptedPacket(
    QuicTransportVersion version,
    const QuicEncryptedPacket& encrypted,
    QuicConnectionIdLength destination_connection_id_length,
    QuicConnectionIdLength source_connection_id_length,
    bool includes_version,
    bool includes_diversification_nonce,
    QuicPacketNumberLength packet_number_length) {
  // TODO(ianswett): This is identical to QuicData::AssociatedData.
  return QuicStringPiece(
      encrypted.data(),
      GetStartOfEncryptedData(version, destination_connection_id_length,
                              source_connection_id_length, includes_version,
                              includes_diversification_nonce,
                              packet_number_length));
}

void QuicFramer::SetDecrypter(EncryptionLevel level,
                              std::unique_ptr<QuicDecrypter> decrypter) {
  DCHECK(alternative_decrypter_ == nullptr);
  DCHECK_GE(level, decrypter_level_);
  decrypter_ = std::move(decrypter);
  decrypter_level_ = level;
}

void QuicFramer::SetAlternativeDecrypter(
    EncryptionLevel level,
    std::unique_ptr<QuicDecrypter> decrypter,
    bool latch_once_used) {
  alternative_decrypter_ = std::move(decrypter);
  alternative_decrypter_level_ = level;
  alternative_decrypter_latch_ = latch_once_used;
}

const QuicDecrypter* QuicFramer::decrypter() const {
  return decrypter_.get();
}

const QuicDecrypter* QuicFramer::alternative_decrypter() const {
  return alternative_decrypter_.get();
}

void QuicFramer::SetEncrypter(EncryptionLevel level,
                              std::unique_ptr<QuicEncrypter> encrypter) {
  DCHECK_GE(level, 0);
  DCHECK_LT(level, NUM_ENCRYPTION_LEVELS);
  encrypter_[level] = std::move(encrypter);
}

size_t QuicFramer::EncryptInPlace(EncryptionLevel level,
                                  QuicPacketNumber packet_number,
                                  size_t ad_len,
                                  size_t total_len,
                                  size_t buffer_len,
                                  char* buffer) {
  size_t output_length = 0;
  if (!encrypter_[level]->EncryptPacket(
          version_.transport_version, packet_number,
          QuicStringPiece(buffer, ad_len),  // Associated data
          QuicStringPiece(buffer + ad_len, total_len - ad_len),  // Plaintext
          buffer + ad_len,  // Destination buffer
          &output_length, buffer_len - ad_len)) {
    RaiseError(QUIC_ENCRYPTION_FAILURE);
    return 0;
  }

  return ad_len + output_length;
}

size_t QuicFramer::EncryptPayload(EncryptionLevel level,
                                  QuicPacketNumber packet_number,
                                  const QuicPacket& packet,
                                  char* buffer,
                                  size_t buffer_len) {
  DCHECK(encrypter_[level] != nullptr);

  QuicStringPiece associated_data =
      packet.AssociatedData(version_.transport_version);
  // Copy in the header, because the encrypter only populates the encrypted
  // plaintext content.
  const size_t ad_len = associated_data.length();
  memmove(buffer, associated_data.data(), ad_len);
  // Encrypt the plaintext into the buffer.
  size_t output_length = 0;
  if (!encrypter_[level]->EncryptPacket(
          version_.transport_version, packet_number, associated_data,
          packet.Plaintext(version_.transport_version), buffer + ad_len,
          &output_length, buffer_len - ad_len)) {
    RaiseError(QUIC_ENCRYPTION_FAILURE);
    return 0;
  }

  return ad_len + output_length;
}

size_t QuicFramer::GetMaxPlaintextSize(size_t ciphertext_size) {
  // In order to keep the code simple, we don't have the current encryption
  // level to hand. Both the NullEncrypter and AES-GCM have a tag length of 12.
  size_t min_plaintext_size = ciphertext_size;

  for (int i = ENCRYPTION_NONE; i < NUM_ENCRYPTION_LEVELS; i++) {
    if (encrypter_[i] != nullptr) {
      size_t size = encrypter_[i]->GetMaxPlaintextSize(ciphertext_size);
      if (size < min_plaintext_size) {
        min_plaintext_size = size;
      }
    }
  }

  return min_plaintext_size;
}

bool QuicFramer::DecryptPayload(QuicDataReader* encrypted_reader,
                                const QuicPacketHeader& header,
                                const QuicEncryptedPacket& packet,
                                char* decrypted_buffer,
                                size_t buffer_length,
                                size_t* decrypted_length) {
  QuicStringPiece encrypted = encrypted_reader->ReadRemainingPayload();
  DCHECK(decrypter_ != nullptr);
  QuicStringPiece associated_data = GetAssociatedDataFromEncryptedPacket(
      version_.transport_version, packet,
      header.destination_connection_id_length,
      header.source_connection_id_length, header.version_flag,
      header.nonce != nullptr, header.packet_number_length);

  bool success = decrypter_->DecryptPacket(
      version_.transport_version, header.packet_number, associated_data,
      encrypted, decrypted_buffer, decrypted_length, buffer_length);
  if (success) {
    visitor_->OnDecryptedPacket(decrypter_level_);
  } else if (alternative_decrypter_ != nullptr) {
    if (header.nonce != nullptr) {
      DCHECK_EQ(perspective_, Perspective::IS_CLIENT);
      alternative_decrypter_->SetDiversificationNonce(*header.nonce);
    }
    bool try_alternative_decryption = true;
    if (alternative_decrypter_level_ == ENCRYPTION_INITIAL) {
      if (perspective_ == Perspective::IS_CLIENT) {
        if (header.nonce == nullptr) {
          // Can not use INITIAL decryption without a diversification nonce.
          try_alternative_decryption = false;
        }
      } else {
        DCHECK(header.nonce == nullptr);
      }
    }

    if (try_alternative_decryption) {
      success = alternative_decrypter_->DecryptPacket(
          version_.transport_version, header.packet_number, associated_data,
          encrypted, decrypted_buffer, decrypted_length, buffer_length);
    }
    if (success) {
      visitor_->OnDecryptedPacket(alternative_decrypter_level_);
      if (alternative_decrypter_latch_) {
        // Switch to the alternative decrypter and latch so that we cannot
        // switch back.
        decrypter_ = std::move(alternative_decrypter_);
        decrypter_level_ = alternative_decrypter_level_;
        alternative_decrypter_level_ = ENCRYPTION_NONE;
      } else {
        // Switch the alternative decrypter so that we use it first next time.
        decrypter_.swap(alternative_decrypter_);
        EncryptionLevel level = alternative_decrypter_level_;
        alternative_decrypter_level_ = decrypter_level_;
        decrypter_level_ = level;
      }
    }
  }

  if (!success) {
    QUIC_DVLOG(1) << ENDPOINT << "DecryptPacket failed for packet_number:"
                  << header.packet_number;
    return false;
  }

  return true;
}

size_t QuicFramer::GetIetfAckFrameSize(const QuicAckFrame& frame) {
  // Type byte, largest_acked, and delay_time are straight-forward.
  size_t ack_frame_size = kQuicFrameTypeSize;
  QuicPacketNumber largest_acked = LargestAcked(frame);
  ack_frame_size += QuicDataWriter::GetVarInt62Len(largest_acked);
  uint64_t ack_delay_time_us;
  ack_delay_time_us = frame.ack_delay_time.ToMicroseconds();
  ack_delay_time_us = ack_delay_time_us >> kIetfAckTimestampShift;
  ack_frame_size += QuicDataWriter::GetVarInt62Len(ack_delay_time_us);

  // If |ecn_counters_populated| is true and any of the ecn counters is non-0
  // then the ecn counters are included...
  if (frame.ecn_counters_populated &&
      (frame.ect_0_count || frame.ect_1_count || frame.ecn_ce_count)) {
    ack_frame_size += QuicDataWriter::GetVarInt62Len(frame.ect_0_count);
    ack_frame_size += QuicDataWriter::GetVarInt62Len(frame.ect_1_count);
    ack_frame_size += QuicDataWriter::GetVarInt62Len(frame.ecn_ce_count);
  }

  // The rest (ack_block_count, first_ack_block, and additional ack
  // blocks, if any) depends:
  uint64_t ack_block_count = frame.packets.NumIntervals();
  if (ack_block_count == 0) {
    // If the QuicAckFrame has no Intervals, then it is interpreted
    // as an ack of a single packet at QuicAckFrame.largest_acked.
    // The resulting ack will consist of only the frame's
    // largest_ack & first_ack_block fields. The first ack block will be 0
    // (indicating a single packet) and the ack block_count will be 0.
    // Each 0 takes 1 byte when VarInt62 encoded.
    ack_frame_size += 2;
    return ack_frame_size;
  }

  auto itr = frame.packets.rbegin();
  QuicPacketNumber ack_block_largest = largest_acked;
  QuicPacketNumber ack_block_smallest;
  if ((itr->max() - 1) == largest_acked) {
    // If largest_acked + 1 is equal to the Max() of the first Interval
    // in the QuicAckFrame then the first Interval is the first ack block of the
    // frame; remaining Intervals are additional ack blocks.  The QuicAckFrame's
    // first Interval is encoded in the frame's largest_acked/first_ack_block,
    // the remaining Intervals are encoded in additional ack blocks in the
    // frame, and the packet's ack_block_count is the number of QuicAckFrame
    // Intervals - 1.
    ack_block_smallest = itr->min();
    itr++;
    ack_block_count--;
  } else {
    // If QuicAckFrame.largest_acked is NOT equal to the Max() of
    // the first Interval then it is interpreted as acking a single
    // packet at QuicAckFrame.largest_acked, with additional
    // Intervals indicating additional ack blocks. The encoding is
    //  a) The packet's largest_acked is the QuicAckFrame's largest
    //     acked,
    //  b) the first ack block size is 0,
    //  c) The packet's ack_block_count is the number of QuicAckFrame
    //     Intervals, and
    //  d) The QuicAckFrame Intervals are encoded in additional ack
    //     blocks in the packet.
    ack_block_smallest = largest_acked;
  }
  size_t ack_block_count_size = QuicDataWriter::GetVarInt62Len(ack_block_count);
  ack_frame_size += ack_block_count_size;

  QuicPacketNumber first_ack_block = ack_block_largest - ack_block_smallest;
  size_t first_ack_block_size = QuicDataWriter::GetVarInt62Len(first_ack_block);
  ack_frame_size += first_ack_block_size;

  // Account for the remaining Intervals, if any.
  while (ack_block_count != 0) {
    QuicPacketNumber gap_size = ack_block_smallest - itr->max();
    // Decrement per the protocol specification
    size_t size_of_gap_size = QuicDataWriter::GetVarInt62Len(gap_size - 1);
    ack_frame_size += size_of_gap_size;

    QuicPacketNumber block_size = itr->max() - itr->min();
    // Decrement per the protocol specification
    size_t size_of_block_size = QuicDataWriter::GetVarInt62Len(block_size - 1);
    ack_frame_size += size_of_block_size;

    ack_block_smallest = itr->min();
    itr++;
    ack_block_count--;
  }

  return ack_frame_size;
}

size_t QuicFramer::GetAckFrameSize(
    const QuicAckFrame& ack,
    QuicPacketNumberLength packet_number_length) {
  size_t ack_size = 0;

  if (version_.transport_version == QUIC_VERSION_99) {
    return GetIetfAckFrameSize(ack);
  }
  AckFrameInfo ack_info = GetAckFrameInfo(ack);
  QuicPacketNumberLength largest_acked_length =
      GetMinPacketNumberLength(version_.transport_version, LargestAcked(ack));
  QuicPacketNumberLength ack_block_length = GetMinPacketNumberLength(
      version_.transport_version, ack_info.max_block_length);

  ack_size =
      GetMinAckFrameSize(version_.transport_version, largest_acked_length);
  // First ack block length.
  ack_size += ack_block_length;
  if (ack_info.num_ack_blocks != 0) {
    ack_size += kNumberOfAckBlocksSize;
    ack_size += std::min(ack_info.num_ack_blocks, kMaxAckBlocks) *
                (ack_block_length + PACKET_1BYTE_PACKET_NUMBER);
  }

  // Include timestamps.
  if (process_timestamps_) {
    ack_size += GetAckFrameTimeStampSize(ack);
  }

  return ack_size;
}

size_t QuicFramer::GetAckFrameTimeStampSize(const QuicAckFrame& ack) {
  if (ack.received_packet_times.empty()) {
    return 0;
  }

  return kQuicNumTimestampsLength + kQuicFirstTimestampLength +
         (kQuicTimestampLength + kQuicTimestampPacketNumberGapLength) *
             (ack.received_packet_times.size() - 1);
}

size_t QuicFramer::ComputeFrameLength(
    const QuicFrame& frame,
    bool last_frame_in_packet,
    QuicPacketNumberLength packet_number_length) {
  switch (frame.type) {
    case STREAM_FRAME:
      return GetMinStreamFrameSize(
                 version_.transport_version, frame.stream_frame.stream_id,
                 frame.stream_frame.offset, last_frame_in_packet,
                 frame.stream_frame.data_length) +
             frame.stream_frame.data_length;
    // TODO(nharper): Add a case for CRYPTO_FRAME here?
    case ACK_FRAME: {
      return GetAckFrameSize(*frame.ack_frame, packet_number_length);
    }
    case STOP_WAITING_FRAME:
      return GetStopWaitingFrameSize(version_.transport_version,
                                     packet_number_length);
    case MTU_DISCOVERY_FRAME:
      // MTU discovery frames are serialized as ping frames.
      return kQuicFrameTypeSize;
    case MESSAGE_FRAME:
      return GetMessageFrameSize(version_.transport_version,
                                 last_frame_in_packet,
                                 frame.message_frame->message_data.length());
    case PADDING_FRAME:
      DCHECK(false);
      return 0;
    default:
      return GetRetransmittableControlFrameSize(version_.transport_version,
                                                frame);
  }
}

bool QuicFramer::AppendTypeByte(const QuicFrame& frame,
                                bool last_frame_in_packet,
                                QuicDataWriter* writer) {
  if (version_.transport_version == QUIC_VERSION_99) {
    return AppendIetfTypeByte(frame, last_frame_in_packet, writer);
  }
  uint8_t type_byte = 0;
  switch (frame.type) {
    case STREAM_FRAME:
      type_byte =
          GetStreamFrameTypeByte(frame.stream_frame, last_frame_in_packet);
      break;
    case ACK_FRAME:
      return true;
    case MTU_DISCOVERY_FRAME:
      type_byte = static_cast<uint8_t>(PING_FRAME);
      break;

    case APPLICATION_CLOSE_FRAME:
      set_detailed_error(
          "Attempt to append APPLICATION_CLOSE frame and not in version 99.");
      return RaiseError(QUIC_INTERNAL_ERROR);
    case NEW_CONNECTION_ID_FRAME:
      set_detailed_error(
          "Attempt to append NEW_CONNECTION_ID frame and not in version 99.");
      return RaiseError(QUIC_INTERNAL_ERROR);
    case RETIRE_CONNECTION_ID_FRAME:
      set_detailed_error(
          "Attempt to append RETIRE_CONNECTION_ID frame and not in version "
          "99.");
      return RaiseError(QUIC_INTERNAL_ERROR);
    case NEW_TOKEN_FRAME:
      set_detailed_error(
          "Attempt to append NEW_TOKEN frame and not in version 99.");
      return RaiseError(QUIC_INTERNAL_ERROR);
    case MAX_STREAM_ID_FRAME:
      set_detailed_error(
          "Attempt to append MAX_STREAM_ID frame and not in version 99.");
      return RaiseError(QUIC_INTERNAL_ERROR);
    case STREAM_ID_BLOCKED_FRAME:
      set_detailed_error(
          "Attempt to append STREAM_ID_BLOCKED frame and not in version 99.");
      return RaiseError(QUIC_INTERNAL_ERROR);
    case PATH_RESPONSE_FRAME:
      set_detailed_error(
          "Attempt to append PATH_RESPONSE frame and not in version 99.");
      return RaiseError(QUIC_INTERNAL_ERROR);
    case PATH_CHALLENGE_FRAME:
      set_detailed_error(
          "Attempt to append PATH_CHALLENGE frame and not in version 99.");
      return RaiseError(QUIC_INTERNAL_ERROR);
    case STOP_SENDING_FRAME:
      set_detailed_error(
          "Attempt to append STOP_SENDING frame and not in version 99.");
      return RaiseError(QUIC_INTERNAL_ERROR);
    case MESSAGE_FRAME:
      return true;

    default:
      type_byte = static_cast<uint8_t>(frame.type);
      break;
  }

  return writer->WriteUInt8(type_byte);
}

bool QuicFramer::AppendIetfTypeByte(const QuicFrame& frame,
                                    bool last_frame_in_packet,
                                    QuicDataWriter* writer) {
  uint8_t type_byte = 0;
  switch (frame.type) {
    case PADDING_FRAME:
      type_byte = IETF_PADDING;
      break;
    case RST_STREAM_FRAME:
      type_byte = IETF_RST_STREAM;
      break;
    case CONNECTION_CLOSE_FRAME:
      type_byte = IETF_CONNECTION_CLOSE;
      break;
    case GOAWAY_FRAME:
      set_detailed_error(
          "Attempt to create non-version-99 GOAWAY frame in version 99.");
      return RaiseError(QUIC_INTERNAL_ERROR);
    case WINDOW_UPDATE_FRAME:
      // Depending on whether there is a stream ID or not, will be either a
      // MAX_STREAM_DATA frame or a MAX_DATA frame.
      if (frame.window_update_frame->stream_id == 0) {
        type_byte = IETF_MAX_DATA;
      } else {
        type_byte = IETF_MAX_STREAM_DATA;
      }
      break;
    case BLOCKED_FRAME:
      if (frame.blocked_frame->stream_id == 0) {
        type_byte = IETF_BLOCKED;
      } else {
        type_byte = IETF_STREAM_BLOCKED;
      }
      break;
    case STOP_WAITING_FRAME:
      set_detailed_error(
          "Attempt to append type byte of STOP WAITING frame in version 99.");
      return RaiseError(QUIC_INTERNAL_ERROR);
    case PING_FRAME:
      type_byte = IETF_PING;
      break;
    case STREAM_FRAME:
      type_byte =
          GetStreamFrameTypeByte(frame.stream_frame, last_frame_in_packet);
      break;
    case ACK_FRAME:
      // Do nothing here, AppendIetfAckFrameAndTypeByte() will put the type byte
      // in the buffer.
      return true;
    case MTU_DISCOVERY_FRAME:
      // The path MTU discovery frame is encoded as a PING frame on the wire.
      type_byte = IETF_PING;
      break;
    case APPLICATION_CLOSE_FRAME:
      type_byte = IETF_APPLICATION_CLOSE;
      break;
    case NEW_CONNECTION_ID_FRAME:
      type_byte = IETF_NEW_CONNECTION_ID;
      break;
    case RETIRE_CONNECTION_ID_FRAME:
      type_byte = IETF_RETIRE_CONNECTION_ID;
      break;
    case NEW_TOKEN_FRAME:
      type_byte = IETF_NEW_TOKEN;
      break;
    case MAX_STREAM_ID_FRAME:
      type_byte = IETF_MAX_STREAM_ID;
      break;
    case STREAM_ID_BLOCKED_FRAME:
      type_byte = IETF_STREAM_ID_BLOCKED;
      break;
    case PATH_RESPONSE_FRAME:
      type_byte = IETF_PATH_RESPONSE;
      break;
    case PATH_CHALLENGE_FRAME:
      type_byte = IETF_PATH_CHALLENGE;
      break;
    case STOP_SENDING_FRAME:
      type_byte = IETF_STOP_SENDING;
      break;
    case MESSAGE_FRAME:
      return true;
    case CRYPTO_FRAME:
      type_byte = IETF_CRYPTO;
      break;
    default:
      QUIC_BUG << "Attempt to generate a frame type for an unsupported value: "
               << frame.type;
      return false;
  }
  return writer->WriteUInt8(type_byte);
}

// static
bool QuicFramer::AppendPacketNumber(QuicPacketNumberLength packet_number_length,
                                    QuicPacketNumber packet_number,
                                    QuicDataWriter* writer) {
  size_t length = packet_number_length;
  if (length != 1 && length != 2 && length != 4 && length != 6 && length != 8) {
    QUIC_BUG << "Invalid packet_number_length: " << length;
    return false;
  }
  return writer->WriteBytesToUInt64(packet_number_length, packet_number);
}

// static
bool QuicFramer::AppendStreamId(size_t stream_id_length,
                                QuicStreamId stream_id,
                                QuicDataWriter* writer) {
  if (stream_id_length == 0 || stream_id_length > 4) {
    QUIC_BUG << "Invalid stream_id_length: " << stream_id_length;
    return false;
  }
  return writer->WriteBytesToUInt64(stream_id_length, stream_id);
}

// static
bool QuicFramer::AppendStreamOffset(size_t offset_length,
                                    QuicStreamOffset offset,
                                    QuicDataWriter* writer) {
  if (offset_length == 1 || offset_length > 8) {
    QUIC_BUG << "Invalid stream_offset_length: " << offset_length;
    return false;
  }

  return writer->WriteBytesToUInt64(offset_length, offset);
}

// static
bool QuicFramer::AppendAckBlock(uint8_t gap,
                                QuicPacketNumberLength length_length,
                                QuicPacketNumber length,
                                QuicDataWriter* writer) {
  return writer->WriteUInt8(gap) &&
         AppendPacketNumber(length_length, length, writer);
}

bool QuicFramer::AppendStreamFrame(const QuicStreamFrame& frame,
                                   bool no_stream_frame_length,
                                   QuicDataWriter* writer) {
  if (version_.transport_version == QUIC_VERSION_99) {
    return AppendIetfStreamFrame(frame, no_stream_frame_length, writer);
  }
  if (!AppendStreamId(GetStreamIdSize(frame.stream_id), frame.stream_id,
                      writer)) {
    QUIC_BUG << "Writing stream id size failed.";
    return false;
  }
  if (!AppendStreamOffset(
          GetStreamOffsetSize(version_.transport_version, frame.offset),
          frame.offset, writer)) {
    QUIC_BUG << "Writing offset size failed.";
    return false;
  }
  if (!no_stream_frame_length) {
    if ((frame.data_length > std::numeric_limits<uint16_t>::max()) ||
        !writer->WriteUInt16(static_cast<uint16_t>(frame.data_length))) {
      QUIC_BUG << "Writing stream frame length failed";
      return false;
    }
  }

  if (data_producer_ != nullptr) {
    DCHECK_EQ(nullptr, frame.data_buffer);
    if (frame.data_length == 0) {
      return true;
    }
    if (data_producer_->WriteStreamData(frame.stream_id, frame.offset,
                                        frame.data_length,
                                        writer) != WRITE_SUCCESS) {
      QUIC_BUG << "Writing frame data failed.";
      return false;
    }
    return true;
  }

  if (!writer->WriteBytes(frame.data_buffer, frame.data_length)) {
    QUIC_BUG << "Writing frame data failed.";
    return false;
  }
  return true;
}

// static
bool QuicFramer::AppendIetfConnectionId(
    bool version_flag,
    QuicConnectionId destination_connection_id,
    QuicConnectionIdLength destination_connection_id_length,
    QuicConnectionId source_connection_id,
    QuicConnectionIdLength source_connection_id_length,
    QuicDataWriter* writer) {
  if (version_flag) {
    // Append connection ID length byte.
    uint8_t dcil = GetConnectionIdLengthValue(destination_connection_id_length);
    uint8_t scil = GetConnectionIdLengthValue(source_connection_id_length);
    uint8_t connection_id_length = dcil << 4 | scil;
    if (!writer->WriteBytes(&connection_id_length, 1)) {
      return false;
    }
  }
  if (destination_connection_id_length == PACKET_8BYTE_CONNECTION_ID &&
      !writer->WriteConnectionId(destination_connection_id)) {
    return false;
  }
  if (source_connection_id_length == PACKET_8BYTE_CONNECTION_ID &&
      !writer->WriteConnectionId(source_connection_id)) {
    return false;
  }
  return true;
}
bool QuicFramer::AppendNewTokenFrame(const QuicNewTokenFrame& frame,
                                     QuicDataWriter* writer) {
  if (!writer->WriteVarInt62(static_cast<uint64_t>(frame.token.length()))) {
    set_detailed_error("Writing token length failed.");
    return false;
  }
  if (!writer->WriteBytes(frame.token.data(), frame.token.length())) {
    set_detailed_error("Writing token buffer failed.");
    return false;
  }
  return true;
}

bool QuicFramer::ProcessNewTokenFrame(QuicDataReader* reader,
                                      QuicNewTokenFrame* frame) {
  uint64_t length;
  if (!reader->ReadVarInt62(&length)) {
    set_detailed_error("Unable to read new token length.");
    return false;
  }
  if (length > kMaxNewTokenTokenLength) {
    set_detailed_error("Token length larger than maximum.");
    return false;
  }

  // TODO(ianswett): Don't use QuicStringPiece as an intermediary.
  QuicStringPiece data;
  if (!reader->ReadStringPiece(&data, length)) {
    set_detailed_error("Unable to read new token data.");
    return false;
  }
  frame->token = QuicString(data);
  return true;
}

// Add a new ietf-format stream frame.
// Bits controlling whether there is a frame-length and frame-offset
// are in the QuicStreamFrame.
bool QuicFramer::AppendIetfStreamFrame(const QuicStreamFrame& frame,
                                       bool last_frame_in_packet,
                                       QuicDataWriter* writer) {
  if (!writer->WriteVarInt62(static_cast<uint64_t>(frame.stream_id))) {
    set_detailed_error("Writing stream id failed.");
    return false;
  }

  if (frame.offset != 0) {
    if (!writer->WriteVarInt62(static_cast<uint64_t>(frame.offset))) {
      set_detailed_error("Writing data offset failed.");
      return false;
    }
  }

  if (!last_frame_in_packet) {
    if (!writer->WriteVarInt62(frame.data_length)) {
      set_detailed_error("Writing data length failed.");
      return false;
    }
  }

  if (frame.data_length == 0) {
    return true;
  }
  if (data_producer_ == nullptr) {
    if (!writer->WriteBytes(frame.data_buffer, frame.data_length)) {
      set_detailed_error("Writing frame data failed.");
      return false;
    }
  } else {
    DCHECK_EQ(nullptr, frame.data_buffer);

    if (data_producer_->WriteStreamData(frame.stream_id, frame.offset,
                                        frame.data_length,
                                        writer) != WRITE_SUCCESS) {
      set_detailed_error("Writing frame data failed.");
      return false;
    }
  }
  return true;
}

bool QuicFramer::AppendCryptoFrame(const QuicCryptoFrame& frame,
                                   QuicDataWriter* writer) {
  if (!writer->WriteVarInt62(static_cast<uint64_t>(frame.offset))) {
    set_detailed_error("Writing data offset failed.");
    return false;
  }
  if (!writer->WriteVarInt62(static_cast<uint64_t>(frame.data_length))) {
    set_detailed_error("Writing data length failed.");
    return false;
  }
  // TODO(nharper): Append stream frame contents.
  return false;
}

void QuicFramer::set_version(const ParsedQuicVersion version) {
  DCHECK(IsSupportedVersion(version)) << ParsedQuicVersionToString(version);
  version_ = version;
}

bool QuicFramer::AppendAckFrameAndTypeByte(const QuicAckFrame& frame,
                                           QuicDataWriter* writer) {
  if (transport_version() == QUIC_VERSION_99) {
    return AppendIetfAckFrameAndTypeByte(frame, writer);
  }

  const AckFrameInfo new_ack_info = GetAckFrameInfo(frame);
  QuicPacketNumber largest_acked = LargestAcked(frame);
  QuicPacketNumberLength largest_acked_length =
      GetMinPacketNumberLength(version_.transport_version, largest_acked);
  QuicPacketNumberLength ack_block_length = GetMinPacketNumberLength(
      version_.transport_version, new_ack_info.max_block_length);
  // Calculate available bytes for timestamps and ack blocks.
  int32_t available_timestamp_and_ack_block_bytes =
      writer->capacity() - writer->length() - ack_block_length -
      GetMinAckFrameSize(version_.transport_version, largest_acked_length) -
      (new_ack_info.num_ack_blocks != 0 ? kNumberOfAckBlocksSize : 0);
  DCHECK_LE(0, available_timestamp_and_ack_block_bytes);

  // Write out the type byte by setting the low order bits and doing shifts
  // to make room for the next bit flags to be set.
  // Whether there are multiple ack blocks.
  uint8_t type_byte = 0;
  SetBit(&type_byte, new_ack_info.num_ack_blocks != 0,
         kQuicHasMultipleAckBlocksOffset);

  SetBits(&type_byte, GetPacketNumberFlags(largest_acked_length),
          kQuicSequenceNumberLengthNumBits, kLargestAckedOffset);

  SetBits(&type_byte, GetPacketNumberFlags(ack_block_length),
          kQuicSequenceNumberLengthNumBits, kActBlockLengthOffset);

  type_byte |= kQuicFrameTypeAckMask;

  if (!writer->WriteUInt8(type_byte)) {
    return false;
  }

  size_t max_num_ack_blocks = available_timestamp_and_ack_block_bytes /
                              (ack_block_length + PACKET_1BYTE_PACKET_NUMBER);

  // Number of ack blocks.
  size_t num_ack_blocks =
      std::min(new_ack_info.num_ack_blocks, max_num_ack_blocks);
  if (num_ack_blocks > std::numeric_limits<uint8_t>::max()) {
    num_ack_blocks = std::numeric_limits<uint8_t>::max();
  }

  // Largest acked.
  if (!AppendPacketNumber(largest_acked_length, largest_acked, writer)) {
    return false;
  }

  // Largest acked delta time.
  uint64_t ack_delay_time_us = kUFloat16MaxValue;
  if (!frame.ack_delay_time.IsInfinite()) {
    DCHECK_LE(0u, frame.ack_delay_time.ToMicroseconds());
    ack_delay_time_us = frame.ack_delay_time.ToMicroseconds();
  }
  if (!writer->WriteUFloat16(ack_delay_time_us)) {
    return false;
  }

  if (num_ack_blocks > 0) {
    if (!writer->WriteBytes(&num_ack_blocks, 1)) {
      return false;
    }
  }

  // First ack block length.
  if (!AppendPacketNumber(ack_block_length, new_ack_info.first_block_length,
                          writer)) {
    return false;
  }

  // Ack blocks.
  if (num_ack_blocks > 0) {
    size_t num_ack_blocks_written = 0;
    // Append, in descending order from the largest ACKed packet, a series of
    // ACK blocks that represents the successfully acknoweldged packets. Each
    // appended gap/block length represents a descending delta from the previous
    // block. i.e.:
    // |--- length ---|--- gap ---|--- length ---|--- gap ---|--- largest ---|
    // For gaps larger than can be represented by a single encoded gap, a 0
    // length gap of the maximum is used, i.e.:
    // |--- length ---|--- gap ---|- 0 -|--- gap ---|--- largest ---|
    auto itr = frame.packets.rbegin();
    QuicPacketNumber previous_start = itr->min();
    ++itr;

    for (;
         itr != frame.packets.rend() && num_ack_blocks_written < num_ack_blocks;
         previous_start = itr->min(), ++itr) {
      const auto& interval = *itr;
      const QuicPacketNumber total_gap = previous_start - interval.max();
      const size_t num_encoded_gaps =
          (total_gap + std::numeric_limits<uint8_t>::max() - 1) /
          std::numeric_limits<uint8_t>::max();
      DCHECK_LE(0u, num_encoded_gaps);

      // Append empty ACK blocks because the gap is longer than a single gap.
      for (size_t i = 1;
           i < num_encoded_gaps && num_ack_blocks_written < num_ack_blocks;
           ++i) {
        if (!AppendAckBlock(std::numeric_limits<uint8_t>::max(),
                            ack_block_length, 0, writer)) {
          return false;
        }
        ++num_ack_blocks_written;
      }
      if (num_ack_blocks_written >= num_ack_blocks) {
        if (QUIC_PREDICT_FALSE(num_ack_blocks_written != num_ack_blocks)) {
          QUIC_BUG << "Wrote " << num_ack_blocks_written
                   << ", expected to write " << num_ack_blocks;
        }
        break;
      }

      const uint8_t last_gap =
          total_gap -
          (num_encoded_gaps - 1) * std::numeric_limits<uint8_t>::max();
      // Append the final ACK block with a non-empty size.
      if (!AppendAckBlock(last_gap, ack_block_length, interval.Length(),
                          writer)) {
        return false;
      }
      ++num_ack_blocks_written;
    }
    DCHECK_EQ(num_ack_blocks, num_ack_blocks_written);
  }
  // Timestamps.
  // If we don't process timestamps or if we don't have enough available space
  // to append all the timestamps, don't append any of them.
  if (process_timestamps_ && writer->capacity() - writer->length() >=
                                 GetAckFrameTimeStampSize(frame)) {
    if (!AppendTimestampsToAckFrame(frame, writer)) {
      return false;
    }
  } else {
    uint8_t num_received_packets = 0;
    if (!writer->WriteBytes(&num_received_packets, 1)) {
      return false;
    }
  }

  return true;
}

bool QuicFramer::AppendTimestampsToAckFrame(const QuicAckFrame& frame,
                                            QuicDataWriter* writer) {
  DCHECK_GE(std::numeric_limits<uint8_t>::max(),
            frame.received_packet_times.size());
  // num_received_packets is only 1 byte.
  if (frame.received_packet_times.size() >
      std::numeric_limits<uint8_t>::max()) {
    return false;
  }

  uint8_t num_received_packets = frame.received_packet_times.size();
  if (!writer->WriteBytes(&num_received_packets, 1)) {
    return false;
  }
  if (num_received_packets == 0) {
    return true;
  }

  auto it = frame.received_packet_times.begin();
  QuicPacketNumber packet_number = it->first;
  QuicPacketNumber delta_from_largest_observed =
      LargestAcked(frame) - packet_number;

  DCHECK_GE(std::numeric_limits<uint8_t>::max(), delta_from_largest_observed);
  if (delta_from_largest_observed > std::numeric_limits<uint8_t>::max()) {
    return false;
  }

  if (!writer->WriteUInt8(delta_from_largest_observed)) {
    return false;
  }

  // Use the lowest 4 bytes of the time delta from the creation_time_.
  const uint64_t time_epoch_delta_us = UINT64_C(1) << 32;
  uint32_t time_delta_us =
      static_cast<uint32_t>((it->second - creation_time_).ToMicroseconds() &
                            (time_epoch_delta_us - 1));
  if (!writer->WriteUInt32(time_delta_us)) {
    return false;
  }

  QuicTime prev_time = it->second;

  for (++it; it != frame.received_packet_times.end(); ++it) {
    packet_number = it->first;
    delta_from_largest_observed = LargestAcked(frame) - packet_number;

    if (delta_from_largest_observed > std::numeric_limits<uint8_t>::max()) {
      return false;
    }

    if (!writer->WriteUInt8(delta_from_largest_observed)) {
      return false;
    }

    uint64_t frame_time_delta_us = (it->second - prev_time).ToMicroseconds();
    prev_time = it->second;
    if (!writer->WriteUFloat16(frame_time_delta_us)) {
      return false;
    }
  }
  return true;
}

bool QuicFramer::AppendStopWaitingFrame(const QuicPacketHeader& header,
                                        const QuicStopWaitingFrame& frame,
                                        QuicDataWriter* writer) {
  DCHECK_GE(QUIC_VERSION_43, version_.transport_version);
  DCHECK_GE(header.packet_number, frame.least_unacked);
  const QuicPacketNumber least_unacked_delta =
      header.packet_number - frame.least_unacked;
  const QuicPacketNumber length_shift = header.packet_number_length * 8;

  if (least_unacked_delta >> length_shift > 0) {
    QUIC_BUG << "packet_number_length " << header.packet_number_length
             << " is too small for least_unacked_delta: " << least_unacked_delta
             << " packet_number:" << header.packet_number
             << " least_unacked:" << frame.least_unacked
             << " version:" << version_.transport_version;
    return false;
  }
  if (!AppendPacketNumber(header.packet_number_length, least_unacked_delta,
                          writer)) {
    QUIC_BUG << " seq failed: " << header.packet_number_length;
    return false;
  }

  return true;
}

int QuicFramer::CalculateIetfAckBlockCount(const QuicAckFrame& frame,
                                           QuicDataWriter* writer,
                                           size_t available_space) {
  // Number of blocks requested in the frame
  uint64_t ack_block_count = frame.packets.NumIntervals();

  auto itr = frame.packets.rbegin();

  int actual_block_count = 1;
  uint64_t block_length = itr->max() - itr->min();
  size_t encoded_size = QuicDataWriter::GetVarInt62Len(block_length);
  if (encoded_size > available_space) {
    return 0;
  }
  available_space -= encoded_size;
  uint64_t previous_ack_end = itr->min();
  ack_block_count--;

  while (ack_block_count) {
    // Each block is a gap followed by another ACK. Calculate each value,
    // determine the encoded lengths, and check against the available space.
    itr++;
    size_t gap = previous_ack_end - itr->max() - 1;
    encoded_size = QuicDataWriter::GetVarInt62Len(gap);

    // Add the ACK block.
    block_length = itr->max() - itr->min();
    encoded_size += QuicDataWriter::GetVarInt62Len(block_length);

    if (encoded_size > available_space) {
      // No room for this block, so what we've
      // done up to now is all that can be done.
      return actual_block_count;
    }
    available_space -= encoded_size;
    actual_block_count++;
    previous_ack_end = itr->min();
    ack_block_count--;
  }
  // Ran through the whole thing! We can do all blocks.
  return actual_block_count;
}

bool QuicFramer::AppendIetfAckFrameAndTypeByte(const QuicAckFrame& frame,
                                               QuicDataWriter* writer) {
  // Assume frame is an IETF_ACK frame. If |ecn_counters_populated| is true and
  // any of the ECN counters is non-0 then turn it into an IETF_ACK+ECN frame.
  uint8_t type = IETF_ACK;
  if (frame.ecn_counters_populated &&
      (frame.ect_0_count || frame.ect_1_count || frame.ecn_ce_count)) {
    type = IETF_ACK_ECN;
  }

  if (!writer->WriteUInt8(type)) {
    set_detailed_error("No room for frame-type");
    return false;
  }

  QuicPacketNumber largest_acked = LargestAcked(frame);
  if (!writer->WriteVarInt62(largest_acked)) {
    set_detailed_error("No room for largest-acked in ack frame");
    return false;
  }

  uint64_t ack_delay_time_us = kVarInt62MaxValue;
  if (!frame.ack_delay_time.IsInfinite()) {
    DCHECK_LE(0u, frame.ack_delay_time.ToMicroseconds());
    ack_delay_time_us = frame.ack_delay_time.ToMicroseconds();
    // TODO(fkastenholz): Use the shift from TLS transport parameters.
    ack_delay_time_us = ack_delay_time_us >> kIetfAckTimestampShift;
  }

  if (!writer->WriteVarInt62(ack_delay_time_us)) {
    set_detailed_error("No room for ack-delay in ack frame");
    return false;
  }
  if (type == IETF_ACK_ECN) {
    // Encode the ACK ECN fields
    if (!writer->WriteVarInt62(frame.ect_0_count)) {
      set_detailed_error("No room for ect_0_count in ack frame");
      return false;
    }
    if (!writer->WriteVarInt62(frame.ect_1_count)) {
      set_detailed_error("No room for ect_1_count in ack frame");
      return false;
    }
    if (!writer->WriteVarInt62(frame.ecn_ce_count)) {
      set_detailed_error("No room for ecn_ce_count in ack frame");
      return false;
    }
  }

  uint64_t ack_block_count = frame.packets.NumIntervals();
  if (ack_block_count == 0) {
    // If the QuicAckFrame has no Intervals, then it is interpreted
    // as an ack of a single packet at QuicAckFrame.largest_acked.
    // The resulting ack will consist of only the frame's
    // largest_ack & first_ack_block fields. The first ack block will be 0
    // (indicating a single packet) and the ack block_count will be 0.
    if (!writer->WriteVarInt62(0)) {
      set_detailed_error("No room for ack block count in ack frame");
      return false;
    }
    // size of the first block is 1 packet
    if (!writer->WriteVarInt62(0)) {
      set_detailed_error("No room for first ack block in ack frame");
      return false;
    }
    return true;
  }
  // Case 2 or 3
  auto itr = frame.packets.rbegin();

  QuicPacketNumber ack_block_largest = largest_acked;
  QuicPacketNumber ack_block_smallest;
  if ((itr->max() - 1) == largest_acked) {
    // If largest_acked + 1 is equal to the Max() of the first Interval
    // in the QuicAckFrame then the first Interval is the first ack block of the
    // frame; remaining Intervals are additional ack blocks.  The QuicAckFrame's
    // first Interval is encoded in the frame's largest_acked/first_ack_block,
    // the remaining Intervals are encoded in additional ack blocks in the
    // frame, and the packet's ack_block_count is the number of QuicAckFrame
    // Intervals - 1.
    ack_block_smallest = itr->min();
    itr++;
    ack_block_count--;
  } else {
    // If QuicAckFrame.largest_acked is NOT equal to the Max() of
    // the first Interval then it is interpreted as acking a single
    // packet at QuicAckFrame.largest_acked, with additional
    // Intervals indicating additional ack blocks. The encoding is
    //  a) The packet's largest_acked is the QuicAckFrame's largest
    //     acked,
    //  b) the first ack block size is 0,
    //  c) The packet's ack_block_count is the number of QuicAckFrame
    //     Intervals, and
    //  d) The QuicAckFrame Intervals are encoded in additional ack
    //     blocks in the packet.
    ack_block_smallest = largest_acked;
  }

  if (!writer->WriteVarInt62(ack_block_count)) {
    set_detailed_error("No room for ack block count in ack frame");
    return false;
  }

  QuicPacketNumber first_ack_block = ack_block_largest - ack_block_smallest;
  if (!writer->WriteVarInt62(first_ack_block)) {
    set_detailed_error("No room for first ack block in ack frame");
    return false;
  }

  // For the remaining QuicAckFrame Intervals, if any
  while (ack_block_count != 0) {
    QuicPacketNumber gap_size = ack_block_smallest - itr->max();
    if (!writer->WriteVarInt62(gap_size - 1)) {
      set_detailed_error("No room for gap block in ack frame");
      return false;
    }

    QuicPacketNumber block_size = itr->max() - itr->min();
    if (!writer->WriteVarInt62(block_size - 1)) {
      set_detailed_error("No room for nth ack block in ack frame");
      return false;
    }

    ack_block_smallest = itr->min();
    itr++;
    ack_block_count--;
  }
  return true;
}

bool QuicFramer::AppendRstStreamFrame(const QuicRstStreamFrame& frame,
                                      QuicDataWriter* writer) {
  if (version_.transport_version == QUIC_VERSION_99) {
    return AppendIetfResetStreamFrame(frame, writer);
  }
  if (!writer->WriteUInt32(frame.stream_id)) {
    return false;
  }

  if (!writer->WriteUInt64(frame.byte_offset)) {
    return false;
  }

  uint32_t error_code = static_cast<uint32_t>(frame.error_code);
  if (!writer->WriteUInt32(error_code)) {
    return false;
  }

  return true;
}

bool QuicFramer::AppendConnectionCloseFrame(
    const QuicConnectionCloseFrame& frame,
    QuicDataWriter* writer) {
  if (version_.transport_version == QUIC_VERSION_99) {
    return AppendIetfConnectionCloseFrame(frame, writer);
  }
  uint32_t error_code = static_cast<uint32_t>(frame.error_code);
  if (!writer->WriteUInt32(error_code)) {
    return false;
  }
  if (!writer->WriteStringPiece16(TruncateErrorString(frame.error_details))) {
    return false;
  }
  return true;
}

bool QuicFramer::AppendGoAwayFrame(const QuicGoAwayFrame& frame,
                                   QuicDataWriter* writer) {
  uint32_t error_code = static_cast<uint32_t>(frame.error_code);
  if (!writer->WriteUInt32(error_code)) {
    return false;
  }
  uint32_t stream_id = static_cast<uint32_t>(frame.last_good_stream_id);
  if (!writer->WriteUInt32(stream_id)) {
    return false;
  }
  if (!writer->WriteStringPiece16(TruncateErrorString(frame.reason_phrase))) {
    return false;
  }
  return true;
}

bool QuicFramer::AppendWindowUpdateFrame(const QuicWindowUpdateFrame& frame,
                                         QuicDataWriter* writer) {
  uint32_t stream_id = static_cast<uint32_t>(frame.stream_id);
  if (!writer->WriteUInt32(stream_id)) {
    return false;
  }
  if (!writer->WriteUInt64(frame.byte_offset)) {
    return false;
  }
  return true;
}

bool QuicFramer::AppendBlockedFrame(const QuicBlockedFrame& frame,
                                    QuicDataWriter* writer) {
  if (version_.transport_version == QUIC_VERSION_99) {
    if (frame.stream_id == 0) {
      return AppendIetfBlockedFrame(frame, writer);
    }
    return AppendStreamBlockedFrame(frame, writer);
  }
  uint32_t stream_id = static_cast<uint32_t>(frame.stream_id);
  if (!writer->WriteUInt32(stream_id)) {
    return false;
  }
  return true;
}

bool QuicFramer::AppendPaddingFrame(const QuicPaddingFrame& frame,
                                    QuicDataWriter* writer) {
  if (version_.transport_version == QUIC_VERSION_35) {
    writer->WritePadding();
    return true;
  }

  if (frame.num_padding_bytes == 0) {
    return false;
  }
  if (frame.num_padding_bytes < 0) {
    QUIC_BUG_IF(frame.num_padding_bytes != -1);
    writer->WritePadding();
    return true;
  }
  // Please note, num_padding_bytes includes type byte which has been written.
  return writer->WritePaddingBytes(frame.num_padding_bytes - 1);
}

bool QuicFramer::AppendMessageFrameAndTypeByte(const QuicMessageFrame& frame,
                                               bool last_frame_in_packet,
                                               QuicDataWriter* writer) {
  uint8_t type_byte = last_frame_in_packet ? IETF_EXTENSION_MESSAGE_NO_LENGTH
                                           : IETF_EXTENSION_MESSAGE;
  if (!writer->WriteUInt8(type_byte)) {
    return false;
  }
  if (!last_frame_in_packet &&
      !writer->WriteVarInt62(frame.message_data.length())) {
    return false;
  }
  return writer->WriteBytes(frame.message_data.data(),
                            frame.message_data.length());
}

bool QuicFramer::RaiseError(QuicErrorCode error) {
  QUIC_DLOG(INFO) << ENDPOINT << "Error: " << QuicErrorCodeToString(error)
                  << " detail: " << detailed_error_;
  set_error(error);
  visitor_->OnError(this);
  return false;
}

bool QuicFramer::IsVersionNegotiation(const QuicPacketHeader& header) const {
  if (perspective_ == Perspective::IS_SERVER) {
    return false;
  }
  if (!last_packet_is_ietf_quic_) {
    return header.version_flag;
  }
  if (header.form == SHORT_HEADER) {
    return false;
  }
  return header.long_packet_type == VERSION_NEGOTIATION;
}

Endianness QuicFramer::endianness() const {
  return version_.transport_version != QUIC_VERSION_35 ? NETWORK_BYTE_ORDER
                                                       : HOST_BYTE_ORDER;
}

bool QuicFramer::StartsWithChlo(QuicStreamId id,
                                QuicStreamOffset offset) const {
  if (data_producer_ == nullptr) {
    QUIC_BUG << "Does not have data producer.";
    return false;
  }
  char buf[sizeof(kCHLO)];
  QuicDataWriter writer(sizeof(kCHLO), buf, endianness());
  if (data_producer_->WriteStreamData(id, offset, sizeof(kCHLO), &writer) !=
      WRITE_SUCCESS) {
    QUIC_BUG << "Failed to write data for stream " << id << " with offset "
             << offset << " data_length = " << sizeof(kCHLO);
    return false;
  }

  return strncmp(buf, reinterpret_cast<const char*>(&kCHLO), sizeof(kCHLO)) ==
         0;
}

PacketHeaderFormat QuicFramer::GetLastPacketFormat() const {
  if (!last_packet_is_ietf_quic_) {
    return GOOGLE_QUIC_PACKET;
  }
  return last_header_form_ == LONG_HEADER ? IETF_QUIC_LONG_HEADER_PACKET
                                          : IETF_QUIC_SHORT_HEADER_PACKET;
}

bool QuicFramer::AppendIetfConnectionCloseFrame(
    const QuicConnectionCloseFrame& frame,
    QuicDataWriter* writer) {
  if (!writer->WriteUInt16(static_cast<const uint16_t>(frame.error_code))) {
    set_detailed_error("Can not write connection close frame error code");
    return false;
  }
  if (!writer->WriteVarInt62(frame.frame_type)) {
    set_detailed_error("Writing frame type failed.");
    return false;
  }

  if (!writer->WriteStringPieceVarInt62(
          TruncateErrorString(frame.error_details))) {
    set_detailed_error("Can not write connection close phrase");
    return false;
  }
  return true;
}

bool QuicFramer::AppendApplicationCloseFrame(
    const QuicApplicationCloseFrame& frame,
    QuicDataWriter* writer) {
  if (!writer->WriteUInt16(static_cast<const uint16_t>(frame.error_code))) {
    set_detailed_error("Can not write application close frame error code");
    return false;
  }

  if (!writer->WriteStringPieceVarInt62(
          TruncateErrorString(frame.error_details))) {
    set_detailed_error("Can not write application close phrase");
    return false;
  }
  return true;
}

bool QuicFramer::ProcessIetfConnectionCloseFrame(
    QuicDataReader* reader,
    QuicConnectionCloseFrame* frame) {
  uint16_t code;
  if (!reader->ReadUInt16(&code)) {
    set_detailed_error("Unable to read connection close error code.");
    return false;
  }
  frame->ietf_error_code = static_cast<QuicIetfTransportErrorCodes>(code);

  if (!reader->ReadVarInt62(&frame->frame_type)) {
    set_detailed_error("Unable to read connection close frame type.");
    return false;
  }

  uint64_t phrase_length;
  if (!reader->ReadVarInt62(&phrase_length)) {
    set_detailed_error("Unable to read connection close error details.");
    return false;
  }
  QuicStringPiece phrase;
  if (!reader->ReadStringPiece(&phrase, static_cast<size_t>(phrase_length))) {
    set_detailed_error("Unable to read connection close error details.");
    return false;
  }
  frame->error_details = QuicString(phrase);

  return true;
}

bool QuicFramer::ProcessApplicationCloseFrame(
    QuicDataReader* reader,
    QuicApplicationCloseFrame* frame) {
  uint16_t code;
  if (!reader->ReadUInt16(&code)) {
    set_detailed_error("Unable to read application close error code.");
    return false;
  }
  frame->error_code = static_cast<QuicErrorCode>(code);

  uint64_t phrase_length;
  if (!reader->ReadVarInt62(&phrase_length)) {
    set_detailed_error("Unable to read application close error details.");
    return false;
  }
  QuicStringPiece phrase;
  if (!reader->ReadStringPiece(&phrase, static_cast<size_t>(phrase_length))) {
    set_detailed_error("Unable to read application close error details.");
    return false;
  }
  frame->error_details = QuicString(phrase);

  return true;
}

// IETF Quic Path Challenge/Response frames.
bool QuicFramer::ProcessPathChallengeFrame(QuicDataReader* reader,
                                           QuicPathChallengeFrame* frame) {
  if (!reader->ReadBytes(frame->data_buffer.data(),
                         frame->data_buffer.size())) {
    set_detailed_error("Can not read path challenge data.");
    return false;
  }
  return true;
}

bool QuicFramer::ProcessPathResponseFrame(QuicDataReader* reader,
                                          QuicPathResponseFrame* frame) {
  if (!reader->ReadBytes(frame->data_buffer.data(),
                         frame->data_buffer.size())) {
    set_detailed_error("Can not read path response data.");
    return false;
  }
  return true;
}

bool QuicFramer::AppendPathChallengeFrame(const QuicPathChallengeFrame& frame,
                                          QuicDataWriter* writer) {
  if (!writer->WriteBytes(frame.data_buffer.data(), frame.data_buffer.size())) {
    set_detailed_error("Writing Path Challenge data failed.");
    return false;
  }
  return true;
}

bool QuicFramer::AppendPathResponseFrame(const QuicPathResponseFrame& frame,
                                         QuicDataWriter* writer) {
  if (!writer->WriteBytes(frame.data_buffer.data(), frame.data_buffer.size())) {
    set_detailed_error("Writing Path Response data failed.");
    return false;
  }
  return true;
}

// Add a new ietf-format stream reset frame.
// General format is
//    stream id
//    application error code
//    final offset
bool QuicFramer::AppendIetfResetStreamFrame(const QuicRstStreamFrame& frame,
                                            QuicDataWriter* writer) {
  if (!writer->WriteVarInt62(static_cast<uint64_t>(frame.stream_id))) {
    set_detailed_error("Writing reset-stream stream id failed.");
    return false;
  }
  if (!writer->WriteUInt16(frame.ietf_error_code)) {
    set_detailed_error("Writing reset-stream error code failed.");
    return false;
  }
  if (!writer->WriteVarInt62(static_cast<uint64_t>(frame.byte_offset))) {
    set_detailed_error("Writing reset-stream final-offset failed.");
    return false;
  }
  return true;
}

bool QuicFramer::ProcessIetfResetStreamFrame(QuicDataReader* reader,
                                             QuicRstStreamFrame* frame) {
  // Get Stream ID from frame. ReadVarIntStreamID returns false
  // if either A) there is a read error or B) the resulting value of
  // the Stream ID is larger than the maximum allowed value.
  if (!reader->ReadVarIntStreamId(&frame->stream_id)) {
    set_detailed_error("Unable to read rst stream stream id.");
    return false;
  }

  if (!reader->ReadUInt16(&frame->ietf_error_code)) {
    set_detailed_error("Unable to read rst stream error code.");
    return false;
  }

  if (!reader->ReadVarInt62(&frame->byte_offset)) {
    set_detailed_error("Unable to read rst stream sent byte offset.");
    return false;
  }
  return true;
}

bool QuicFramer::ProcessStopSendingFrame(
    QuicDataReader* reader,
    QuicStopSendingFrame* stop_sending_frame) {
  if (!reader->ReadVarIntStreamId(&stop_sending_frame->stream_id)) {
    set_detailed_error("Unable to read stop sending stream id.");
    return false;
  }

  if (!reader->ReadUInt16(&stop_sending_frame->application_error_code)) {
    set_detailed_error("Unable to read stop sending application error code.");
    return false;
  }
  return true;
}

bool QuicFramer::AppendStopSendingFrame(
    const QuicStopSendingFrame& stop_sending_frame,
    QuicDataWriter* writer) {
  if (!writer->WriteVarInt62(stop_sending_frame.stream_id)) {
    set_detailed_error("Can not write stop sending stream id");
    return false;
  }
  if (!writer->WriteUInt16(stop_sending_frame.application_error_code)) {
    set_detailed_error("Can not write application error code");
    return false;
  }
  return true;
}

// Append/process IETF-Format MAX_DATA Frame
bool QuicFramer::AppendMaxDataFrame(const QuicWindowUpdateFrame& frame,
                                    QuicDataWriter* writer) {
  if (!writer->WriteVarInt62(frame.byte_offset)) {
    set_detailed_error("Can not write MAX_DATA byte-offset");
    return false;
  }
  return true;
}

bool QuicFramer::ProcessMaxDataFrame(QuicDataReader* reader,
                                     QuicWindowUpdateFrame* frame) {
  frame->stream_id = 0;
  if (!reader->ReadVarInt62(&frame->byte_offset)) {
    set_detailed_error("Can not read MAX_DATA byte-offset");
    return false;
  }
  return true;
}

// Append/process IETF-Format MAX_STREAM_DATA Frame
bool QuicFramer::AppendMaxStreamDataFrame(const QuicWindowUpdateFrame& frame,
                                          QuicDataWriter* writer) {
  if (!writer->WriteVarInt62(frame.stream_id)) {
    set_detailed_error("Can not write MAX_STREAM_DATA stream id");
    return false;
  }
  if (!writer->WriteVarInt62(frame.byte_offset)) {
    set_detailed_error("Can not write MAX_STREAM_DATA byte-offset");
    return false;
  }
  return true;
}

bool QuicFramer::ProcessMaxStreamDataFrame(QuicDataReader* reader,
                                           QuicWindowUpdateFrame* frame) {
  if (!reader->ReadVarIntStreamId(&frame->stream_id)) {
    set_detailed_error("Can not read MAX_STREAM_DATA stream id");
    return false;
  }
  if (!reader->ReadVarInt62(&frame->byte_offset)) {
    set_detailed_error("Can not read MAX_STREAM_DATA byte-count");
    return false;
  }
  return true;
}

bool QuicFramer::AppendMaxStreamIdFrame(const QuicMaxStreamIdFrame& frame,
                                        QuicDataWriter* writer) {
  if (!writer->WriteVarInt62(frame.max_stream_id)) {
    set_detailed_error("Can not write MAX_STREAM_ID stream id");
    return false;
  }
  return true;
}

bool QuicFramer::ProcessMaxStreamIdFrame(QuicDataReader* reader,
                                         QuicMaxStreamIdFrame* frame) {
  if (!reader->ReadVarIntStreamId(&frame->max_stream_id)) {
    set_detailed_error("Can not read MAX_STREAM_ID stream id.");
    return false;
  }
  return true;
}

bool QuicFramer::AppendIetfBlockedFrame(const QuicBlockedFrame& frame,
                                        QuicDataWriter* writer) {
  if (!writer->WriteVarInt62(frame.offset)) {
    set_detailed_error("Can not write blocked offset.");
    return false;
  }
  return true;
}

bool QuicFramer::ProcessIetfBlockedFrame(QuicDataReader* reader,
                                         QuicBlockedFrame* frame) {
  // Indicates that it is a BLOCKED frame (as opposed to STREAM_BLOCKED).
  frame->stream_id = 0;
  if (!reader->ReadVarInt62(&frame->offset)) {
    set_detailed_error("Can not read blocked offset.");
    return false;
  }
  return true;
}

bool QuicFramer::AppendStreamBlockedFrame(const QuicBlockedFrame& frame,
                                          QuicDataWriter* writer) {
  if (!writer->WriteVarInt62(frame.stream_id)) {
    set_detailed_error("Can not write stream blocked stream id.");
    return false;
  }
  if (!writer->WriteVarInt62(frame.offset)) {
    set_detailed_error("Can not write stream blocked offset.");
    return false;
  }
  return true;
}

bool QuicFramer::ProcessStreamBlockedFrame(QuicDataReader* reader,
                                           QuicBlockedFrame* frame) {
  if (!reader->ReadVarIntStreamId(&frame->stream_id)) {
    set_detailed_error("Can not read stream blocked stream id.");
    return false;
  }
  if (!reader->ReadVarInt62(&frame->offset)) {
    set_detailed_error("Can not read stream blocked offset.");
    return false;
  }
  return true;
}

bool QuicFramer::AppendStreamIdBlockedFrame(
    const QuicStreamIdBlockedFrame& frame,
    QuicDataWriter* writer) {
  if (!writer->WriteVarInt62(frame.stream_id)) {
    set_detailed_error("Can not write STREAM_ID_BLOCKED stream id");
    return false;
  }
  return true;
}

bool QuicFramer::ProcessStreamIdBlockedFrame(QuicDataReader* reader,
                                             QuicStreamIdBlockedFrame* frame) {
  if (!reader->ReadVarIntStreamId(&frame->stream_id)) {
    set_detailed_error("Can not read STREAM_ID_BLOCKED stream id.");
    return false;
  }
  return true;
}

bool QuicFramer::AppendNewConnectionIdFrame(
    const QuicNewConnectionIdFrame& frame,
    QuicDataWriter* writer) {
  if (!writer->WriteVarInt62(frame.sequence_number)) {
    set_detailed_error("Can not write New Connection ID sequence number");
    return false;
  }
  if (!writer->WriteUInt8(kQuicConnectionIdLength)) {
    set_detailed_error(
        "Can not write New Connection ID frame connection ID Length");
    return false;
  }
  if (!writer->WriteConnectionId(frame.connection_id)) {
    set_detailed_error("Can not write New Connection ID frame connection ID");
    return false;
  }

  if (!writer->WriteBytes(
          static_cast<const void*>(&frame.stateless_reset_token),
          sizeof(frame.stateless_reset_token))) {
    set_detailed_error("Can not write New Connection ID Reset Token");
    return false;
  }
  return true;
}

bool QuicFramer::ProcessNewConnectionIdFrame(QuicDataReader* reader,
                                             QuicNewConnectionIdFrame* frame) {
  if (!reader->ReadVarInt62(&frame->sequence_number)) {
    set_detailed_error(
        "Unable to read new connection ID frame sequence number.");
    return false;
  }

  uint8_t connection_id_length;
  if (!reader->ReadUInt8(&connection_id_length)) {
    set_detailed_error(
        "Unable to read new connection ID frame connection id length.");
    return false;
  }

  if (connection_id_length != kQuicConnectionIdLength) {
    set_detailed_error("Invalid new connection ID length.");
    return false;
  }

  if (!reader->ReadConnectionId(&frame->connection_id)) {
    set_detailed_error("Unable to read new connection ID frame connection id.");
    return false;
  }

  if (!reader->ReadBytes(&frame->stateless_reset_token,
                         sizeof(frame->stateless_reset_token))) {
    set_detailed_error("Can not read new connection ID frame reset token.");
    return false;
  }
  return true;
}

bool QuicFramer::AppendRetireConnectionIdFrame(
    const QuicRetireConnectionIdFrame& frame,
    QuicDataWriter* writer) {
  if (!writer->WriteVarInt62(frame.sequence_number)) {
    set_detailed_error("Can not write Retire Connection ID sequence number");
    return false;
  }
  return true;
}

bool QuicFramer::ProcessRetireConnectionIdFrame(
    QuicDataReader* reader,
    QuicRetireConnectionIdFrame* frame) {
  if (!reader->ReadVarInt62(&frame->sequence_number)) {
    set_detailed_error(
        "Unable to read retire connection ID frame sequence number.");
    return false;
  }
  return true;
}

uint8_t QuicFramer::GetStreamFrameTypeByte(const QuicStreamFrame& frame,
                                           bool last_frame_in_packet) const {
  if (version_.transport_version == QUIC_VERSION_99) {
    return GetIetfStreamFrameTypeByte(frame, last_frame_in_packet);
  }
  uint8_t type_byte = 0;
  // Fin bit.
  type_byte |= frame.fin ? kQuicStreamFinMask : 0;

  // Data Length bit.
  type_byte <<= kQuicStreamDataLengthShift;
  type_byte |= last_frame_in_packet ? 0 : kQuicStreamDataLengthMask;

  // Offset 3 bits.
  type_byte <<= kQuicStreamShift;
  const size_t offset_len =
      GetStreamOffsetSize(version_.transport_version, frame.offset);
  if (offset_len > 0) {
    type_byte |= offset_len - 1;
  }

  // stream id 2 bits.
  type_byte <<= kQuicStreamIdShift;
  type_byte |= GetStreamIdSize(frame.stream_id) - 1;
  type_byte |= kQuicFrameTypeStreamMask;  // Set Stream Frame Type to 1.

  return type_byte;
}

uint8_t QuicFramer::GetIetfStreamFrameTypeByte(
    const QuicStreamFrame& frame,
    bool last_frame_in_packet) const {
  DCHECK_EQ(QUIC_VERSION_99, version_.transport_version);
  uint8_t type_byte = IETF_STREAM;
  if (!last_frame_in_packet) {
    type_byte |= IETF_STREAM_FRAME_LEN_BIT;
  }
  if (frame.offset != 0) {
    type_byte |= IETF_STREAM_FRAME_OFF_BIT;
  }
  if (frame.fin) {
    type_byte |= IETF_STREAM_FRAME_FIN_BIT;
  }
  return type_byte;
}

#undef ENDPOINT  // undef for jumbo builds
}  // namespace quic
