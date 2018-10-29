// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_UTILS_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_UTILS_H_

#include <cstddef>
#include <cstdint>

#include "base/macros.h"
#include "net/third_party/quic/core/quic_error_codes.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/core/quic_versions.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_iovec.h"
#include "net/third_party/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quic/platform/api/quic_uint128.h"

namespace quic {

class QUIC_EXPORT_PRIVATE QuicUtils {
 public:
  QuicUtils() = delete;

  // Returns the 64 bit FNV1a hash of the data.  See
  // http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-param
  static uint64_t FNV1a_64_Hash(QuicStringPiece data);

  // Returns the 128 bit FNV1a hash of the data.  See
  // http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-param
  static QuicUint128 FNV1a_128_Hash(QuicStringPiece data);

  // Returns the 128 bit FNV1a hash of the two sequences of data.  See
  // http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-param
  static QuicUint128 FNV1a_128_Hash_Two(QuicStringPiece data1,
                                        QuicStringPiece data2);

  // Returns the 128 bit FNV1a hash of the three sequences of data.  See
  // http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-param
  static QuicUint128 FNV1a_128_Hash_Three(QuicStringPiece data1,
                                          QuicStringPiece data2,
                                          QuicStringPiece data3);

  // SerializeUint128 writes the first 96 bits of |v| in little-endian form
  // to |out|.
  static void SerializeUint128Short(QuicUint128 v, uint8_t* out);

  // Returns the level of encryption as a char*
  static const char* EncryptionLevelToString(EncryptionLevel level);

  // Returns TransmissionType as a char*
  static const char* TransmissionTypeToString(TransmissionType type);

  // Returns AddressChangeType as a string.
  static QuicString AddressChangeTypeToString(AddressChangeType type);

  // Returns SentPacketState as a char*.
  static const char* SentPacketStateToString(SentPacketState state);

  // Returns QuicLongHeaderType as a char*.
  static const char* QuicLongHeaderTypetoString(QuicLongHeaderType type);

  // Determines and returns change type of address change from |old_address| to
  // |new_address|.
  static AddressChangeType DetermineAddressChangeType(
      const QuicSocketAddress& old_address,
      const QuicSocketAddress& new_address);

  // Copies |buffer_length| bytes from iov starting at offset |iov_offset| into
  // buffer. |iov| must be at least iov_offset+length total length and buffer
  // must be at least |length| long.
  static void CopyToBuffer(const struct iovec* iov,
                           int iov_count,
                           size_t iov_offset,
                           size_t buffer_length,
                           char* buffer);

  // Returns true if a packet is ackable. A packet is unackable if it can never
  // be acked. Occurs when a packet is never sent, after it is acknowledged
  // once, or if it's a crypto packet we never expect to receive an ack for.
  static bool IsAckable(SentPacketState state);

  // Returns packet state corresponding to |retransmission_type|.
  static SentPacketState RetransmissionTypeToPacketState(
      TransmissionType retransmission_type);

  // Returns true if header with |first_byte| is considered as an IETF QUIC
  // packet header.
  static bool IsIetfPacketHeader(uint8_t first_byte);

  // Returns ID to denote an invalid stream of |version|.
  static QuicStreamId GetInvalidStreamId(QuicTransportVersion version);

  // Returns crypto stream ID of |version|.
  static QuicStreamId GetCryptoStreamId(QuicTransportVersion version);

  // Returns headers stream ID of |version|.
  static QuicStreamId GetHeadersStreamId(QuicTransportVersion version);

  // Returns true if |id| is considered as client initiated stream ID.
  static bool IsClientInitiatedStreamId(QuicTransportVersion version,
                                        QuicStreamId id);

  // Returns true if |id| is considered as server initiated stream ID.
  static bool IsServerInitiatedStreamId(QuicTransportVersion version,
                                        QuicStreamId id);
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_UTILS_H_
