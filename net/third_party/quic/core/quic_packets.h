// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_PACKETS_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_PACKETS_H_

#include <cstdint>
#include <limits>
#include <list>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "net/third_party/quic/core/frames/quic_frame.h"
#include "net/third_party/quic/core/quic_ack_listener_interface.h"
#include "net/third_party/quic/core/quic_bandwidth.h"
#include "net/third_party/quic/core/quic_constants.h"
#include "net/third_party/quic/core/quic_error_codes.h"
#include "net/third_party/quic/core/quic_time.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/core/quic_versions.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quic/platform/api/quic_uint128.h"

namespace quic {

class QuicPacket;
struct QuicPacketHeader;

// Size in bytes of the data packet header.
QUIC_EXPORT_PRIVATE size_t GetPacketHeaderSize(QuicTransportVersion version,
                                               const QuicPacketHeader& header);

QUIC_EXPORT_PRIVATE size_t
GetPacketHeaderSize(QuicTransportVersion version,
                    QuicConnectionIdLength destination_connection_id_length,
                    QuicConnectionIdLength source_connection_id_length,
                    bool include_version,
                    bool include_diversification_nonce,
                    QuicPacketNumberLength packet_number_length);

// Index of the first byte in a QUIC packet of encrypted data.
QUIC_EXPORT_PRIVATE size_t
GetStartOfEncryptedData(QuicTransportVersion version,
                        const QuicPacketHeader& header);

QUIC_EXPORT_PRIVATE size_t
GetStartOfEncryptedData(QuicTransportVersion version,
                        QuicConnectionIdLength destination_connection_id_length,
                        QuicConnectionIdLength source_connection_id_length,
                        bool include_version,
                        bool include_diversification_nonce,
                        QuicPacketNumberLength packet_number_length);

struct QUIC_EXPORT_PRIVATE QuicPacketHeader {
  QuicPacketHeader();
  QuicPacketHeader(const QuicPacketHeader& other);
  ~QuicPacketHeader();

  QUIC_EXPORT_PRIVATE friend std::ostream& operator<<(
      std::ostream& os,
      const QuicPacketHeader& header);

  // Universal header. All QuicPacket headers will have a connection_id and
  // public flags.
  QuicConnectionId destination_connection_id;
  QuicConnectionIdLength destination_connection_id_length;
  QuicConnectionId source_connection_id;
  QuicConnectionIdLength source_connection_id_length;
  // This is only used for Google QUIC.
  bool reset_flag;
  // For Google QUIC, version flag in packets from the server means version
  // negotiation packet. For IETF QUIC, version flag means long header.
  bool version_flag;
  // Indicates whether |possible_stateless_reset_token| contains a valid value
  // parsed from the packet buffer. IETF QUIC only, always false for GQUIC.
  bool has_possible_stateless_reset_token;
  QuicPacketNumberLength packet_number_length;
  ParsedQuicVersion version;
  // nonce contains an optional, 32-byte nonce value. If not included in the
  // packet, |nonce| will be empty.
  DiversificationNonce* nonce;
  QuicPacketNumber packet_number;
  // Only used if this is an IETF QUIC packet.
  QuicIetfPacketHeaderForm form;
  // Short packet type is reflected in packet_number_length.
  QuicLongHeaderType long_packet_type;
  // Only valid if |has_possible_stateless_reset_token| is true.
  // Stores last 16 bytes of a this packet, used to check whether this packet is
  // a stateless reset packet on decryption failure.
  QuicUint128 possible_stateless_reset_token;
};

struct QUIC_EXPORT_PRIVATE QuicPublicResetPacket {
  QuicPublicResetPacket();
  explicit QuicPublicResetPacket(QuicConnectionId connection_id);

  QuicConnectionId connection_id;
  QuicPublicResetNonceProof nonce_proof;
  QuicSocketAddress client_address;
};

struct QUIC_EXPORT_PRIVATE QuicVersionNegotiationPacket {
  QuicVersionNegotiationPacket();
  explicit QuicVersionNegotiationPacket(QuicConnectionId connection_id);
  QuicVersionNegotiationPacket(const QuicVersionNegotiationPacket& other);
  ~QuicVersionNegotiationPacket();

  QuicConnectionId connection_id;
  ParsedQuicVersionVector versions;
};

struct QUIC_EXPORT_PRIVATE QuicIetfStatelessResetPacket {
  QuicIetfStatelessResetPacket();
  QuicIetfStatelessResetPacket(const QuicPacketHeader& header,
                               QuicUint128 token);
  QuicIetfStatelessResetPacket(const QuicIetfStatelessResetPacket& other);
  ~QuicIetfStatelessResetPacket();

  QuicPacketHeader header;
  QuicUint128 stateless_reset_token;
};

class QUIC_EXPORT_PRIVATE QuicData {
 public:
  QuicData(const char* buffer, size_t length);
  QuicData(const char* buffer, size_t length, bool owns_buffer);
  QuicData(const QuicData&) = delete;
  QuicData& operator=(const QuicData&) = delete;
  virtual ~QuicData();

  QuicStringPiece AsStringPiece() const {
    return QuicStringPiece(data(), length());
  }

  const char* data() const { return buffer_; }
  size_t length() const { return length_; }

 private:
  const char* buffer_;
  size_t length_;
  bool owns_buffer_;
};

class QUIC_EXPORT_PRIVATE QuicPacket : public QuicData {
 public:
  // TODO(fayang): 3 fields from header are passed in as arguments.
  // Consider to add a convenience method which directly accepts the entire
  // header.
  QuicPacket(char* buffer,
             size_t length,
             bool owns_buffer,
             QuicConnectionIdLength destination_connection_id_length,
             QuicConnectionIdLength source_connection_id_length,
             bool includes_version,
             bool includes_diversification_nonce,
             QuicPacketNumberLength packet_number_length);
  QuicPacket(const QuicPacket&) = delete;
  QuicPacket& operator=(const QuicPacket&) = delete;

  QuicStringPiece AssociatedData(QuicTransportVersion version) const;
  QuicStringPiece Plaintext(QuicTransportVersion version) const;

  char* mutable_data() { return buffer_; }

 private:
  char* buffer_;
  const QuicConnectionIdLength destination_connection_id_length_;
  const QuicConnectionIdLength source_connection_id_length_;
  const bool includes_version_;
  const bool includes_diversification_nonce_;
  const QuicPacketNumberLength packet_number_length_;
};

class QUIC_EXPORT_PRIVATE QuicEncryptedPacket : public QuicData {
 public:
  QuicEncryptedPacket(const char* buffer, size_t length);
  QuicEncryptedPacket(const char* buffer, size_t length, bool owns_buffer);
  QuicEncryptedPacket(const QuicEncryptedPacket&) = delete;
  QuicEncryptedPacket& operator=(const QuicEncryptedPacket&) = delete;

  // Clones the packet into a new packet which owns the buffer.
  std::unique_ptr<QuicEncryptedPacket> Clone() const;

  // By default, gtest prints the raw bytes of an object. The bool data
  // member (in the base class QuicData) causes this object to have padding
  // bytes, which causes the default gtest object printer to read
  // uninitialize memory. So we need to teach gtest how to print this object.
  QUIC_EXPORT_PRIVATE friend std::ostream& operator<<(
      std::ostream& os,
      const QuicEncryptedPacket& s);
};

// A received encrypted QUIC packet, with a recorded time of receipt.
class QUIC_EXPORT_PRIVATE QuicReceivedPacket : public QuicEncryptedPacket {
 public:
  QuicReceivedPacket(const char* buffer, size_t length, QuicTime receipt_time);
  QuicReceivedPacket(const char* buffer,
                     size_t length,
                     QuicTime receipt_time,
                     bool owns_buffer);
  QuicReceivedPacket(const char* buffer,
                     size_t length,
                     QuicTime receipt_time,
                     bool owns_buffer,
                     int ttl,
                     bool ttl_valid);
  QuicReceivedPacket(const char* buffer,
                     size_t length,
                     QuicTime receipt_time,
                     bool owns_buffer,
                     int ttl,
                     bool ttl_valid,
                     char* packet_headers,
                     size_t headers_length,
                     bool owns_header_buffer);
  ~QuicReceivedPacket();
  QuicReceivedPacket(const QuicReceivedPacket&) = delete;
  QuicReceivedPacket& operator=(const QuicReceivedPacket&) = delete;

  // Clones the packet into a new packet which owns the buffer.
  std::unique_ptr<QuicReceivedPacket> Clone() const;

  // Returns the time at which the packet was received.
  QuicTime receipt_time() const { return receipt_time_; }

  // This is the TTL of the packet, assuming ttl_vaild_ is true.
  int ttl() const { return ttl_; }

  // Start of packet headers.
  char* packet_headers() const { return packet_headers_; }

  // Length of packet headers.
  int headers_length() const { return headers_length_; }

  // By default, gtest prints the raw bytes of an object. The bool data
  // member (in the base class QuicData) causes this object to have padding
  // bytes, which causes the default gtest object printer to read
  // uninitialize memory. So we need to teach gtest how to print this object.
  QUIC_EXPORT_PRIVATE friend std::ostream& operator<<(
      std::ostream& os,
      const QuicReceivedPacket& s);

 private:
  const QuicTime receipt_time_;
  int ttl_;
  // Points to the start of packet headers.
  char* packet_headers_;
  // Length of packet headers.
  int headers_length_;
  // Whether owns the buffer for packet headers.
  bool owns_header_buffer_;
};

struct QUIC_EXPORT_PRIVATE SerializedPacket {
  SerializedPacket(QuicPacketNumber packet_number,
                   QuicPacketNumberLength packet_number_length,
                   const char* encrypted_buffer,
                   QuicPacketLength encrypted_length,
                   bool has_ack,
                   bool has_stop_waiting);
  SerializedPacket(const SerializedPacket& other);
  SerializedPacket& operator=(const SerializedPacket& other);
  SerializedPacket(SerializedPacket&& other);
  ~SerializedPacket();

  // Not owned.
  const char* encrypted_buffer;
  QuicPacketLength encrypted_length;
  QuicFrames retransmittable_frames;
  IsHandshake has_crypto_handshake;
  // -1: full padding to the end of a max-sized packet
  //  0: no padding
  //  otherwise: only pad up to num_padding_bytes bytes
  int16_t num_padding_bytes;
  QuicPacketNumber packet_number;
  QuicPacketNumberLength packet_number_length;
  EncryptionLevel encryption_level;
  bool has_ack;
  bool has_stop_waiting;
  TransmissionType transmission_type;
  QuicPacketNumber original_packet_number;
  // The largest acked of the AckFrame in this packet if has_ack is true,
  // 0 otherwise.
  QuicPacketNumber largest_acked;
};

// Deletes and clears all the frames and the packet from serialized packet.
QUIC_EXPORT_PRIVATE void ClearSerializedPacket(
    SerializedPacket* serialized_packet);

// Allocates a new char[] of size |packet.encrypted_length| and copies in
// |packet.encrypted_buffer|.
QUIC_EXPORT_PRIVATE char* CopyBuffer(const SerializedPacket& packet);

struct QUIC_EXPORT_PRIVATE SerializedPacketDeleter {
  void operator()(SerializedPacket* packet) {
    if (packet->encrypted_buffer != nullptr) {
      delete[] packet->encrypted_buffer;
    }
    delete packet;
  }
};

// On destruction, OwningSerializedPacketPointer deletes a packet's (on-heap)
// encrypted_buffer before deleting the (also on-heap) packet itself.
// TODO(wub): Maybe delete retransmittable_frames too?
typedef std::unique_ptr<SerializedPacket, SerializedPacketDeleter>
    OwningSerializedPacketPointer;

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_PACKETS_H_
