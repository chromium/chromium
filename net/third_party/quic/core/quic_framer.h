// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_FRAMER_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_FRAMER_H_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "base/macros.h"
#include "net/third_party/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quic/core/crypto/quic_random.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/platform/api/quic_endian.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"

namespace quic {

namespace test {
class QuicFramerPeer;
}  // namespace test

class QuicDataReader;
class QuicDataWriter;
class QuicFramer;
class QuicStreamFrameDataProducer;

// Number of bytes reserved for the frame type preceding each frame.
const size_t kQuicFrameTypeSize = 1;
// Number of bytes reserved for error code.
const size_t kQuicErrorCodeSize = 4;
// Number of bytes reserved to denote the length of error details field.
const size_t kQuicErrorDetailsLengthSize = 2;

// Maximum number of bytes reserved for stream id.
const size_t kQuicMaxStreamIdSize = 4;
// Maximum number of bytes reserved for byte offset in stream frame.
const size_t kQuicMaxStreamOffsetSize = 8;
// Number of bytes reserved to store payload length in stream frame.
const size_t kQuicStreamPayloadLengthSize = 2;
// Number of bytes to reserve for IQ Error codes (for the Connection Close,
// Application Close, and Reset Stream frames).
const size_t kQuicIetfQuicErrorCodeSize = 2;
// Minimum size of the IETF QUIC Error Phrase's length field
const size_t kIetfQuicMinErrorPhraseLengthSize = 1;

// Size in bytes reserved for the delta time of the largest observed
// packet number in ack frames.
const size_t kQuicDeltaTimeLargestObservedSize = 2;
// Size in bytes reserved for the number of received packets with timestamps.
const size_t kQuicNumTimestampsSize = 1;
// Size in bytes reserved for the number of missing packets in ack frames.
const size_t kNumberOfNackRangesSize = 1;
// Size in bytes reserved for the number of ack blocks in ack frames.
const size_t kNumberOfAckBlocksSize = 1;
// Maximum number of missing packet ranges that can fit within an ack frame.
const size_t kMaxNackRanges = (1 << (kNumberOfNackRangesSize * 8)) - 1;
// Maximum number of ack blocks that can fit within an ack frame.
const size_t kMaxAckBlocks = (1 << (kNumberOfAckBlocksSize * 8)) - 1;

// This class receives callbacks from the framer when packets
// are processed.
class QUIC_EXPORT_PRIVATE QuicFramerVisitorInterface {
 public:
  virtual ~QuicFramerVisitorInterface() {}

  // Called if an error is detected in the QUIC protocol.
  virtual void OnError(QuicFramer* framer) = 0;

  // Called only when |perspective_| is IS_SERVER and the framer gets a
  // packet with version flag true and the version on the packet doesn't match
  // |quic_version_|. The visitor should return true after it updates the
  // version of the |framer_| to |received_version| or false to stop processing
  // this packet.
  virtual bool OnProtocolVersionMismatch(
      ParsedQuicVersion received_version) = 0;

  // Called when a new packet has been received, before it
  // has been validated or processed.
  virtual void OnPacket() = 0;

  // Called when a public reset packet has been parsed but has not yet
  // been validated.
  virtual void OnPublicResetPacket(const QuicPublicResetPacket& packet) = 0;

  // Called only when |perspective_| is IS_CLIENT and a version negotiation
  // packet has been parsed.
  virtual void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& packet) = 0;

  // Called when all fields except packet number has been parsed, but has not
  // been authenticated. If it returns false, framing for this packet will
  // cease.
  virtual bool OnUnauthenticatedPublicHeader(
      const QuicPacketHeader& header) = 0;

  // Called when the unauthenticated portion of the header has been parsed.
  // If OnUnauthenticatedHeader returns false, framing for this packet will
  // cease.
  virtual bool OnUnauthenticatedHeader(const QuicPacketHeader& header) = 0;

  // Called when a packet has been decrypted. |level| is the encryption level
  // of the packet.
  virtual void OnDecryptedPacket(EncryptionLevel level) = 0;

  // Called when the complete header of a packet had been parsed.
  // If OnPacketHeader returns false, framing for this packet will cease.
  virtual bool OnPacketHeader(const QuicPacketHeader& header) = 0;

  // Called when a StreamFrame has been parsed.
  virtual bool OnStreamFrame(const QuicStreamFrame& frame) = 0;

  // Called when a CRYPTO frame has been parsed.
  virtual bool OnCryptoFrame(const QuicCryptoFrame& frame) = 0;

  // Called when largest acked of an AckFrame has been parsed.
  virtual bool OnAckFrameStart(QuicPacketNumber largest_acked,
                               QuicTime::Delta ack_delay_time) = 0;

  // Called when ack range [start, end) of an AckFrame has been parsed.
  virtual bool OnAckRange(QuicPacketNumber start, QuicPacketNumber end) = 0;

  // Called when a timestamp in the AckFrame has been parsed.
  virtual bool OnAckTimestamp(QuicPacketNumber packet_number,
                              QuicTime timestamp) = 0;

  // Called after the last ack range in an AckFrame has been parsed.
  // |start| is the starting value of the last ack range.
  virtual bool OnAckFrameEnd(QuicPacketNumber start) = 0;

  // Called when a StopWaitingFrame has been parsed.
  virtual bool OnStopWaitingFrame(const QuicStopWaitingFrame& frame) = 0;

  // Called when a QuicPaddingFrame has been parsed.
  virtual bool OnPaddingFrame(const QuicPaddingFrame& frame) = 0;

  // Called when a PingFrame has been parsed.
  virtual bool OnPingFrame(const QuicPingFrame& frame) = 0;

  // Called when a RstStreamFrame has been parsed.
  virtual bool OnRstStreamFrame(const QuicRstStreamFrame& frame) = 0;

  // Called when a ConnectionCloseFrame has been parsed.
  virtual bool OnConnectionCloseFrame(
      const QuicConnectionCloseFrame& frame) = 0;

  // Called when an IETF ApplicationCloseFrame has been parsed.
  virtual bool OnApplicationCloseFrame(
      const QuicApplicationCloseFrame& frame) = 0;

  // Called when a StopSendingFrame has been parsed.
  virtual bool OnStopSendingFrame(const QuicStopSendingFrame& frame) = 0;

  // Called when a PathChallengeFrame has been parsed.
  virtual bool OnPathChallengeFrame(const QuicPathChallengeFrame& frame) = 0;

  // Called when a PathResponseFrame has been parsed.
  virtual bool OnPathResponseFrame(const QuicPathResponseFrame& frame) = 0;

  // Called when a GoAwayFrame has been parsed.
  virtual bool OnGoAwayFrame(const QuicGoAwayFrame& frame) = 0;

  // Called when a WindowUpdateFrame has been parsed.
  virtual bool OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame) = 0;

  // Called when a BlockedFrame has been parsed.
  virtual bool OnBlockedFrame(const QuicBlockedFrame& frame) = 0;

  // Called when a NewConnectionIdFrame has been parsed.
  virtual bool OnNewConnectionIdFrame(
      const QuicNewConnectionIdFrame& frame) = 0;

  // Called when a RetireConnectionIdFrame has been parsed.
  virtual bool OnRetireConnectionIdFrame(
      const QuicRetireConnectionIdFrame& frame) = 0;

  // Called when a NewTokenFrame has been parsed.
  virtual bool OnNewTokenFrame(const QuicNewTokenFrame& frame) = 0;

  // Called when a message frame has been parsed.
  virtual bool OnMessageFrame(const QuicMessageFrame& frame) = 0;

  // Called when a packet has been completely processed.
  virtual void OnPacketComplete() = 0;

  // Called to check whether |token| is a valid stateless reset token.
  virtual bool IsValidStatelessResetToken(QuicUint128 token) const = 0;

  // Called when an IETF stateless reset packet has been parsed and validated
  // with the stateless reset token.
  virtual void OnAuthenticatedIetfStatelessResetPacket(
      const QuicIetfStatelessResetPacket& packet) = 0;

  // Called when an IETF MaxStreamId frame has been parsed.
  virtual bool OnMaxStreamIdFrame(const QuicMaxStreamIdFrame& frame) = 0;

  // Called when an IETF StreamIdBlocked frame has been parsed.
  virtual bool OnStreamIdBlockedFrame(
      const QuicStreamIdBlockedFrame& frame) = 0;
};

// Class for parsing and constructing QUIC packets.  It has a
// QuicFramerVisitorInterface that is called when packets are parsed.
class QUIC_EXPORT_PRIVATE QuicFramer {
 public:
  // Constructs a new framer that installs a kNULL QuicEncrypter and
  // QuicDecrypter for level ENCRYPTION_NONE. |supported_versions| specifies the
  // list of supported QUIC versions. |quic_version_| is set to the maximum
  // version in |supported_versions|.
  QuicFramer(const ParsedQuicVersionVector& supported_versions,
             QuicTime creation_time,
             Perspective perspective);
  QuicFramer(const QuicFramer&) = delete;
  QuicFramer& operator=(const QuicFramer&) = delete;

  virtual ~QuicFramer();

  // Returns true if |version| is a supported transport version.
  bool IsSupportedTransportVersion(const QuicTransportVersion version) const;

  // Returns true if |version| is a supported protocol version.
  bool IsSupportedVersion(const ParsedQuicVersion version) const;

  // Set callbacks to be called from the framer.  A visitor must be set, or
  // else the framer will likely crash.  It is acceptable for the visitor
  // to do nothing.  If this is called multiple times, only the last visitor
  // will be used.
  void set_visitor(QuicFramerVisitorInterface* visitor) { visitor_ = visitor; }

  const ParsedQuicVersionVector& supported_versions() const {
    return supported_versions_;
  }

  QuicTransportVersion transport_version() const {
    return version_.transport_version;
  }

  ParsedQuicVersion version() const { return version_; }

  void set_version(const ParsedQuicVersion version);

  // Does not DCHECK for supported version. Used by tests to set unsupported
  // version to trigger version negotiation.
  void set_version_for_tests(const ParsedQuicVersion version) {
    version_ = version;
  }

  QuicErrorCode error() const { return error_; }

  // Allows enabling or disabling of timestamp processing and serialization.
  void set_process_timestamps(bool process_timestamps) {
    process_timestamps_ = process_timestamps;
  }

  // Pass a UDP packet into the framer for parsing.
  // Return true if the packet was processed succesfully. |packet| must be a
  // single, complete UDP packet (not a frame of a packet).  This packet
  // might be null padded past the end of the payload, which will be correctly
  // ignored.
  bool ProcessPacket(const QuicEncryptedPacket& packet);

  // Largest size in bytes of all stream frame fields without the payload.
  static size_t GetMinStreamFrameSize(QuicTransportVersion version,
                                      QuicStreamId stream_id,
                                      QuicStreamOffset offset,
                                      bool last_frame_in_packet,
                                      QuicPacketLength data_length);
  static size_t GetMessageFrameSize(QuicTransportVersion version,
                                    bool last_frame_in_packet,
                                    QuicByteCount length);
  // Size in bytes of all ack frame fields without the missing packets or ack
  // blocks.
  static size_t GetMinAckFrameSize(
      QuicTransportVersion version,
      QuicPacketNumberLength largest_observed_length);
  // Size in bytes of a stop waiting frame.
  static size_t GetStopWaitingFrameSize(
      QuicTransportVersion version,
      QuicPacketNumberLength packet_number_length);
  // Size in bytes of all reset stream frame fields.
  static size_t GetRstStreamFrameSize(QuicTransportVersion version,
                                      const QuicRstStreamFrame& frame);
  // Size in bytes of all connection close frame fields without the error
  // details and the missing packets from the enclosed ack frame.
  static size_t GetMinConnectionCloseFrameSize(
      QuicTransportVersion version,
      const QuicConnectionCloseFrame& frame);
  static size_t GetMinApplicationCloseFrameSize(
      QuicTransportVersion version,
      const QuicApplicationCloseFrame& frame);
  // Size in bytes of all GoAway frame fields without the reason phrase.
  static size_t GetMinGoAwayFrameSize();
  // Size in bytes of all WindowUpdate frame fields.
  // For version 99, determines whether a MAX DATA or MAX STREAM DATA frame will
  // be generated and calculates the appropriate size.
  static size_t GetWindowUpdateFrameSize(QuicTransportVersion version,
                                         const QuicWindowUpdateFrame& frame);
  // Size in bytes of all MaxStreamId frame fields.
  static size_t GetMaxStreamIdFrameSize(QuicTransportVersion version,
                                        const QuicMaxStreamIdFrame& frame);
  // Size in bytes of all StreamIdBlocked frame fields.
  static size_t GetStreamIdBlockedFrameSize(
      QuicTransportVersion version,
      const QuicStreamIdBlockedFrame& frame);
  // Size in bytes of all Blocked frame fields.
  static size_t GetBlockedFrameSize(QuicTransportVersion version,
                                    const QuicBlockedFrame& frame);
  // Size in bytes of PathChallenge frame.
  static size_t GetPathChallengeFrameSize(const QuicPathChallengeFrame& frame);
  // Size in bytes of PathResponse frame.
  static size_t GetPathResponseFrameSize(const QuicPathResponseFrame& frame);
  // Size in bytes required to serialize the stream id.
  static size_t GetStreamIdSize(QuicStreamId stream_id);
  // Size in bytes required to serialize the stream offset.
  static size_t GetStreamOffsetSize(QuicTransportVersion version,
                                    QuicStreamOffset offset);
  // Size in bytes for a serialized new connection id frame
  static size_t GetNewConnectionIdFrameSize(
      const QuicNewConnectionIdFrame& frame);

  // Size in bytes for a serialized retire connection id frame
  static size_t GetRetireConnectionIdFrameSize(
      const QuicRetireConnectionIdFrame& frame);

  // Size in bytes for a serialized new token frame
  static size_t GetNewTokenFrameSize(const QuicNewTokenFrame& frame);

  // Size in bytes required for a serialized version negotiation packet
  static size_t GetVersionNegotiationPacketSize(size_t number_versions);

  // Size in bytes required for a serialized stop sending frame.
  static size_t GetStopSendingFrameSize(const QuicStopSendingFrame& frame);

  // Size in bytes required for a serialized retransmittable control |frame|.
  static size_t GetRetransmittableControlFrameSize(QuicTransportVersion version,
                                                   const QuicFrame& frame);

  // Returns the number of bytes added to the packet for the specified frame,
  // and 0 if the frame doesn't fit.  Includes the header size for the first
  // frame.
  size_t GetSerializedFrameLength(const QuicFrame& frame,
                                  size_t free_bytes,
                                  bool first_frame_in_packet,
                                  bool last_frame_in_packet,
                                  QuicPacketNumberLength packet_number_length);

  // Returns the associated data from the encrypted packet |encrypted| as a
  // stringpiece.
  static QuicStringPiece GetAssociatedDataFromEncryptedPacket(
      QuicTransportVersion version,
      const QuicEncryptedPacket& encrypted,
      QuicConnectionIdLength destination_connection_id_length,
      QuicConnectionIdLength source_connection_id_length,
      bool includes_version,
      bool includes_diversification_nonce,
      QuicPacketNumberLength packet_number_length);

  // Serializes a packet containing |frames| into |buffer|.
  // Returns the length of the packet, which must not be longer than
  // |packet_length|.  Returns 0 if it fails to serialize.
  size_t BuildDataPacket(const QuicPacketHeader& header,
                         const QuicFrames& frames,
                         char* buffer,
                         size_t packet_length);

  // Serializes a probing packet, which is a padded PING packet. Returns the
  // length of the packet. Returns 0 if it fails to serialize.
  size_t BuildConnectivityProbingPacket(const QuicPacketHeader& header,
                                        char* buffer,
                                        size_t packet_length);

  // Serialize a probing packet that uses IETF QUIC's PATH CHALLENGE frame. Also
  // fills the packet with padding.
  size_t BuildPaddedPathChallengePacket(const QuicPacketHeader& header,
                                        char* buffer,
                                        size_t packet_length,
                                        QuicPathFrameBuffer* payload,
                                        QuicRandom* randomizer);

  // Serialize a probing response packet that uses IETF QUIC's PATH RESPONSE
  // frame. Also fills the packet with padding if |is_padded| is
  // true. |payloads| is always emptied, even if the packet can not be
  // successfully built.
  size_t BuildPathResponsePacket(const QuicPacketHeader& header,
                                 char* buffer,
                                 size_t packet_length,
                                 const QuicDeque<QuicPathFrameBuffer>& payloads,
                                 const bool is_padded);

  // Returns a new public reset packet.
  static std::unique_ptr<QuicEncryptedPacket> BuildPublicResetPacket(
      const QuicPublicResetPacket& packet);

  // Returns a new IETF stateless reset packet.
  static std::unique_ptr<QuicEncryptedPacket> BuildIetfStatelessResetPacket(
      QuicConnectionId connection_id,
      QuicUint128 stateless_reset_token);

  // Returns a new version negotiation packet.
  static std::unique_ptr<QuicEncryptedPacket> BuildVersionNegotiationPacket(
      QuicConnectionId connection_id,
      bool ietf_quic,
      const ParsedQuicVersionVector& versions);

  // Returns a new IETF version negotiation packet.
  static std::unique_ptr<QuicEncryptedPacket> BuildIetfVersionNegotiationPacket(
      QuicConnectionId connection_id,
      const ParsedQuicVersionVector& versions);

  // If header.version_flag is set, the version in the
  // packet will be set -- but it will be set from version_ not
  // header.versions.
  bool AppendPacketHeader(const QuicPacketHeader& header,
                          QuicDataWriter* writer);
  bool AppendIetfPacketHeader(const QuicPacketHeader& header,
                              QuicDataWriter* writer);
  bool AppendTypeByte(const QuicFrame& frame,
                      bool last_frame_in_packet,
                      QuicDataWriter* writer);
  bool AppendIetfTypeByte(const QuicFrame& frame,
                          bool last_frame_in_packet,
                          QuicDataWriter* writer);
  bool AppendStreamFrame(const QuicStreamFrame& frame,
                         bool last_frame_in_packet,
                         QuicDataWriter* writer);
  bool AppendCryptoFrame(const QuicCryptoFrame& frame, QuicDataWriter* writer);

  // SetDecrypter sets the primary decrypter, replacing any that already exists.
  // If an alternative decrypter is in place then the function DCHECKs. This is
  // intended for cases where one knows that future packets will be using the
  // new decrypter and the previous decrypter is now obsolete. |level| indicates
  // the encryption level of the new decrypter.
  void SetDecrypter(EncryptionLevel level,
                    std::unique_ptr<QuicDecrypter> decrypter);

  // SetAlternativeDecrypter sets a decrypter that may be used to decrypt
  // future packets. |level| indicates the encryption level of the decrypter. If
  // |latch_once_used| is true, then the first time that the decrypter is
  // successful it will replace the primary decrypter.  Otherwise both
  // decrypters will remain active and the primary decrypter will be the one
  // last used.
  void SetAlternativeDecrypter(EncryptionLevel level,
                               std::unique_ptr<QuicDecrypter> decrypter,
                               bool latch_once_used);

  const QuicDecrypter* decrypter() const;
  const QuicDecrypter* alternative_decrypter() const;

  // Changes the encrypter used for level |level| to |encrypter|.
  void SetEncrypter(EncryptionLevel level,
                    std::unique_ptr<QuicEncrypter> encrypter);

  // Encrypts a payload in |buffer|.  |ad_len| is the length of the associated
  // data. |total_len| is the length of the associated data plus plaintext.
  // |buffer_len| is the full length of the allocated buffer.
  size_t EncryptInPlace(EncryptionLevel level,
                        QuicPacketNumber packet_number,
                        size_t ad_len,
                        size_t total_len,
                        size_t buffer_len,
                        char* buffer);

  // Returns the length of the data encrypted into |buffer| if |buffer_len| is
  // long enough, and otherwise 0.
  size_t EncryptPayload(EncryptionLevel level,
                        QuicPacketNumber packet_number,
                        const QuicPacket& packet,
                        char* buffer,
                        size_t buffer_len);

  // Returns the maximum length of plaintext that can be encrypted
  // to ciphertext no larger than |ciphertext_size|.
  size_t GetMaxPlaintextSize(size_t ciphertext_size);

  const QuicString& detailed_error() { return detailed_error_; }

  // The minimum packet number length required to represent |packet_number|.
  static QuicPacketNumberLength GetMinPacketNumberLength(
      QuicTransportVersion version,
      QuicPacketNumber packet_number);

  void SetSupportedVersions(const ParsedQuicVersionVector& versions) {
    supported_versions_ = versions;
    version_ = versions[0];
  }

  // Returns true if data with |offset| of stream |id| starts with 'CHLO'.
  bool StartsWithChlo(QuicStreamId id, QuicStreamOffset offset) const;

  // Returns byte order to read/write integers and floating numbers.
  Endianness endianness() const;

  // Returns true if |header| is considered as an stateless reset packet.
  bool IsIetfStatelessResetPacket(const QuicPacketHeader& header) const;

  // Returns header wire format of last received packet.
  PacketHeaderFormat GetLastPacketFormat() const;

  void set_validate_flags(bool value) { validate_flags_ = value; }

  Perspective perspective() const { return perspective_; }

  QuicVersionLabel last_version_label() const { return last_version_label_; }

  bool last_packet_is_ietf_quic() const { return last_packet_is_ietf_quic_; }

  void set_last_packet_is_ietf_quic(bool last_packet_is_ietf_quic) {
    last_packet_is_ietf_quic_ = last_packet_is_ietf_quic;
  }

  void set_data_producer(QuicStreamFrameDataProducer* data_producer) {
    data_producer_ = data_producer;
  }

  // Returns true if we are doing IETF-formatted packets.
  // In the future this could encompass a wide variety of
  // versions. Doing the test by name ("ietf format") rather
  // than version number localizes the version/ietf-ness binding
  // to this method.
  bool is_ietf_format() {
    return version_.transport_version == QUIC_VERSION_99;
  }

  QuicTime creation_time() const { return creation_time_; }

 private:
  friend class test::QuicFramerPeer;

  typedef std::map<QuicPacketNumber, uint8_t> NackRangeMap;

  struct AckFrameInfo {
    AckFrameInfo();
    AckFrameInfo(const AckFrameInfo& other);
    ~AckFrameInfo();

    // The maximum ack block length.
    QuicPacketNumber max_block_length;
    // Length of first ack block.
    QuicPacketNumber first_block_length;
    // Number of ACK blocks needed for the ACK frame.
    size_t num_ack_blocks;
  };

  // The same as BuildDataPacket, but it only builds IETF-format packets.
  size_t BuildIetfDataPacket(const QuicPacketHeader& header,
                             const QuicFrames& frames,
                             char* buffer,
                             size_t packet_length);

  bool ProcessDataPacket(QuicDataReader* reader,
                         QuicPacketHeader* header,
                         const QuicEncryptedPacket& packet,
                         char* decrypted_buffer,
                         size_t buffer_length);

  bool ProcessIetfDataPacket(QuicDataReader* encrypted_reader,
                             QuicPacketHeader* header,
                             const QuicEncryptedPacket& packet,
                             char* decrypted_buffer,
                             size_t buffer_length);

  bool ProcessPublicResetPacket(QuicDataReader* reader,
                                const QuicPacketHeader& header);

  bool ProcessVersionNegotiationPacket(QuicDataReader* reader,
                                       const QuicPacketHeader& header);

  bool ProcessPublicHeader(QuicDataReader* reader, QuicPacketHeader* header);

  // Processes the unauthenticated portion of the header into |header| from
  // the current QuicDataReader.  Returns true on success, false on failure.
  bool ProcessUnauthenticatedHeader(QuicDataReader* encrypted_reader,
                                    QuicPacketHeader* header);

  bool ProcessIetfPacketHeader(QuicDataReader* reader,
                               QuicPacketHeader* header);

  // First processes possibly truncated packet number. Calculates the full
  // packet number from the truncated one and the last seen packet number, and
  // stores it to |packet_number|.
  bool ProcessAndCalculatePacketNumber(
      QuicDataReader* reader,
      QuicPacketNumberLength packet_number_length,
      QuicPacketNumber base_packet_number,
      QuicPacketNumber* packet_number);
  bool ProcessFrameData(QuicDataReader* reader, const QuicPacketHeader& header);
  bool ProcessIetfFrameData(QuicDataReader* reader,
                            const QuicPacketHeader& header);
  bool ProcessStreamFrame(QuicDataReader* reader,
                          uint8_t frame_type,
                          QuicStreamFrame* frame);
  bool ProcessAckFrame(QuicDataReader* reader, uint8_t frame_type);
  bool ProcessTimestampsInAckFrame(uint8_t num_received_packets,
                                   QuicPacketNumber largest_acked,
                                   QuicDataReader* reader);
  bool ProcessIetfAckFrame(QuicDataReader* reader,
                           uint64_t frame_type,
                           QuicAckFrame* ack_frame);
  bool ProcessStopWaitingFrame(QuicDataReader* reader,
                               const QuicPacketHeader& header,
                               QuicStopWaitingFrame* stop_waiting);
  bool ProcessRstStreamFrame(QuicDataReader* reader, QuicRstStreamFrame* frame);
  bool ProcessConnectionCloseFrame(QuicDataReader* reader,
                                   QuicConnectionCloseFrame* frame);
  bool ProcessGoAwayFrame(QuicDataReader* reader, QuicGoAwayFrame* frame);
  bool ProcessWindowUpdateFrame(QuicDataReader* reader,
                                QuicWindowUpdateFrame* frame);
  bool ProcessBlockedFrame(QuicDataReader* reader, QuicBlockedFrame* frame);
  void ProcessPaddingFrame(QuicDataReader* reader, QuicPaddingFrame* frame);
  bool ProcessMessageFrame(QuicDataReader* reader,
                           bool no_message_length,
                           QuicMessageFrame* frame);

  bool DecryptPayload(QuicDataReader* encrypted_reader,
                      const QuicPacketHeader& header,
                      const QuicEncryptedPacket& packet,
                      char* decrypted_buffer,
                      size_t buffer_length,
                      size_t* decrypted_length);

  // Returns the full packet number from the truncated
  // wire format version and the last seen packet number.
  QuicPacketNumber CalculatePacketNumberFromWire(
      QuicPacketNumberLength packet_number_length,
      QuicPacketNumber base_packet_number,
      QuicPacketNumber packet_number) const;

  // Returns the QuicTime::Delta corresponding to the time from when the framer
  // was created.
  const QuicTime::Delta CalculateTimestampFromWire(uint32_t time_delta_us);

  // Computes the wire size in bytes of time stamps in |ack|.
  size_t GetAckFrameTimeStampSize(const QuicAckFrame& ack);

  // Computes the wire size in bytes of the |ack| frame.
  size_t GetAckFrameSize(const QuicAckFrame& ack,
                         QuicPacketNumberLength packet_number_length);
  // Computes the wire-size, in bytes, of the |frame| ack frame, for IETF Quic.
  size_t GetIetfAckFrameSize(const QuicAckFrame& frame);

  // Computes the wire size in bytes of the |ack| frame.
  size_t GetAckFrameSize(const QuicAckFrame& ack);

  // Computes the wire size in bytes of the payload of |frame|.
  size_t ComputeFrameLength(const QuicFrame& frame,
                            bool last_frame_in_packet,
                            QuicPacketNumberLength packet_number_length);

  static bool AppendPacketNumber(QuicPacketNumberLength packet_number_length,
                                 QuicPacketNumber packet_number,
                                 QuicDataWriter* writer);
  static bool AppendStreamId(size_t stream_id_length,
                             QuicStreamId stream_id,
                             QuicDataWriter* writer);
  static bool AppendStreamOffset(size_t offset_length,
                                 QuicStreamOffset offset,
                                 QuicDataWriter* writer);

  // Appends a single ACK block to |writer| and returns true if the block was
  // successfully appended.
  static bool AppendAckBlock(uint8_t gap,
                             QuicPacketNumberLength length_length,
                             QuicPacketNumber length,
                             QuicDataWriter* writer);

  static uint8_t GetPacketNumberFlags(
      QuicPacketNumberLength packet_number_length);

  static AckFrameInfo GetAckFrameInfo(const QuicAckFrame& frame);

  static bool AppendIetfConnectionId(
      bool version_flag,
      QuicConnectionId destination_connection_id,
      QuicConnectionIdLength destination_connection_id_length,
      QuicConnectionId source_connection_id,
      QuicConnectionIdLength source_connection_id_length,
      QuicDataWriter* writer);

  // The Append* methods attempt to write the provided header or frame using the
  // |writer|, and return true if successful.

  bool AppendAckFrameAndTypeByte(const QuicAckFrame& frame,
                                 QuicDataWriter* builder);
  bool AppendTimestampsToAckFrame(const QuicAckFrame& frame,
                                  QuicDataWriter* writer);

  // Append IETF format ACK frame.
  //
  // AppendIetfAckFrameAndTypeByte adds the IETF type byte and the body
  // of the frame.
  bool AppendIetfAckFrameAndTypeByte(const QuicAckFrame& frame,
                                     QuicDataWriter* writer);

  // Used by AppendIetfAckFrameAndTypeByte to figure out how many ack
  // blocks can be included.
  int CalculateIetfAckBlockCount(const QuicAckFrame& frame,
                                 QuicDataWriter* writer,
                                 size_t available_space);
  bool AppendStopWaitingFrame(const QuicPacketHeader& header,
                              const QuicStopWaitingFrame& frame,
                              QuicDataWriter* builder);
  bool AppendRstStreamFrame(const QuicRstStreamFrame& frame,
                            QuicDataWriter* builder);
  bool AppendConnectionCloseFrame(const QuicConnectionCloseFrame& frame,
                                  QuicDataWriter* builder);
  bool AppendGoAwayFrame(const QuicGoAwayFrame& frame, QuicDataWriter* writer);
  bool AppendWindowUpdateFrame(const QuicWindowUpdateFrame& frame,
                               QuicDataWriter* writer);
  bool AppendBlockedFrame(const QuicBlockedFrame& frame,
                          QuicDataWriter* writer);
  bool AppendPaddingFrame(const QuicPaddingFrame& frame,
                          QuicDataWriter* writer);
  bool AppendMessageFrameAndTypeByte(const QuicMessageFrame& frame,
                                     bool last_frame_in_packet,
                                     QuicDataWriter* writer);

  // IETF frame processing methods.
  bool ProcessIetfStreamFrame(QuicDataReader* reader,
                              uint8_t frame_type,
                              QuicStreamFrame* frame);
  bool ProcessIetfConnectionCloseFrame(QuicDataReader* reader,
                                       QuicConnectionCloseFrame* frame);
  bool ProcessApplicationCloseFrame(QuicDataReader* reader,
                                    QuicApplicationCloseFrame* frame);
  bool ProcessPathChallengeFrame(QuicDataReader* reader,
                                 QuicPathChallengeFrame* frame);
  bool ProcessPathResponseFrame(QuicDataReader* reader,
                                QuicPathResponseFrame* frame);
  bool ProcessIetfResetStreamFrame(QuicDataReader* reader,
                                   QuicRstStreamFrame* frame);
  bool ProcessStopSendingFrame(QuicDataReader* reader,
                               QuicStopSendingFrame* stop_sending_frame);
  bool ProcessCryptoFrame(QuicDataReader* reader, QuicCryptoFrame* frame);

  // IETF frame appending methods.  All methods append the type byte as well.
  bool AppendIetfStreamFrame(const QuicStreamFrame& frame,
                             bool last_frame_in_packet,
                             QuicDataWriter* writer);
  bool AppendIetfConnectionCloseFrame(const QuicConnectionCloseFrame& frame,
                                      QuicDataWriter* writer);
  bool AppendApplicationCloseFrame(const QuicApplicationCloseFrame& frame,
                                   QuicDataWriter* writer);
  bool AppendPathChallengeFrame(const QuicPathChallengeFrame& frame,
                                QuicDataWriter* writer);
  bool AppendPathResponseFrame(const QuicPathResponseFrame& frame,
                               QuicDataWriter* writer);
  bool AppendIetfResetStreamFrame(const QuicRstStreamFrame& frame,
                                  QuicDataWriter* writer);
  bool AppendStopSendingFrame(const QuicStopSendingFrame& stop_sending_frame,
                              QuicDataWriter* writer);

  // Append/consume IETF-Format MAX_DATA and MAX_STREAM_DATA frames
  bool AppendMaxDataFrame(const QuicWindowUpdateFrame& frame,
                          QuicDataWriter* writer);
  bool AppendMaxStreamDataFrame(const QuicWindowUpdateFrame& frame,
                                QuicDataWriter* writer);
  bool ProcessMaxDataFrame(QuicDataReader* reader,
                           QuicWindowUpdateFrame* frame);
  bool ProcessMaxStreamDataFrame(QuicDataReader* reader,
                                 QuicWindowUpdateFrame* frame);

  bool AppendMaxStreamIdFrame(const QuicMaxStreamIdFrame& frame,
                              QuicDataWriter* writer);
  bool ProcessMaxStreamIdFrame(QuicDataReader* reader,
                               QuicMaxStreamIdFrame* frame);

  bool AppendIetfBlockedFrame(const QuicBlockedFrame& frame,
                              QuicDataWriter* writer);
  bool ProcessIetfBlockedFrame(QuicDataReader* reader, QuicBlockedFrame* frame);

  bool AppendStreamBlockedFrame(const QuicBlockedFrame& frame,
                                QuicDataWriter* writer);
  bool ProcessStreamBlockedFrame(QuicDataReader* reader,
                                 QuicBlockedFrame* frame);

  bool AppendStreamIdBlockedFrame(const QuicStreamIdBlockedFrame& frame,
                                  QuicDataWriter* writer);
  bool ProcessStreamIdBlockedFrame(QuicDataReader* reader,
                                   QuicStreamIdBlockedFrame* frame);
  bool AppendNewConnectionIdFrame(const QuicNewConnectionIdFrame& frame,
                                  QuicDataWriter* writer);
  bool ProcessNewConnectionIdFrame(QuicDataReader* reader,
                                   QuicNewConnectionIdFrame* frame);
  bool AppendRetireConnectionIdFrame(const QuicRetireConnectionIdFrame& frame,
                                     QuicDataWriter* writer);
  bool ProcessRetireConnectionIdFrame(QuicDataReader* reader,
                                      QuicRetireConnectionIdFrame* frame);

  bool AppendNewTokenFrame(const QuicNewTokenFrame& frame,
                           QuicDataWriter* writer);
  bool ProcessNewTokenFrame(QuicDataReader* reader, QuicNewTokenFrame* frame);

  bool RaiseError(QuicErrorCode error);

  // Returns true if |header| indicates a version negotiation packet.
  bool IsVersionNegotiation(const QuicPacketHeader& header) const;

  // Calculates and returns type byte of stream frame.
  uint8_t GetStreamFrameTypeByte(const QuicStreamFrame& frame,
                                 bool last_frame_in_packet) const;
  uint8_t GetIetfStreamFrameTypeByte(const QuicStreamFrame& frame,
                                     bool last_frame_in_packet) const;

  void set_error(QuicErrorCode error) { error_ = error; }

  void set_detailed_error(const char* error) { detailed_error_ = error; }

  QuicString detailed_error_;
  QuicFramerVisitorInterface* visitor_;
  QuicErrorCode error_;
  // Updated by ProcessPacketHeader when it succeeds decrypting a larger packet.
  QuicPacketNumber largest_packet_number_;
  // Updated by WritePacketHeader.
  QuicConnectionId last_serialized_connection_id_;
  // The last QUIC version label received.
  QuicVersionLabel last_version_label_;
  // Whether last received packet is IETF QUIC packet.
  bool last_packet_is_ietf_quic_;
  // Whether last received IETF QUIC packet has long or short header. Only used
  // when last_packet_is_ietf_quic_ is true.
  QuicIetfPacketHeaderForm last_header_form_;
  // Version of the protocol being used.
  ParsedQuicVersion version_;
  // This vector contains QUIC versions which we currently support.
  // This should be ordered such that the highest supported version is the first
  // element, with subsequent elements in descending order (versions can be
  // skipped as necessary).
  ParsedQuicVersionVector supported_versions_;
  // Primary decrypter used to decrypt packets during parsing.
  std::unique_ptr<QuicDecrypter> decrypter_;
  // Alternative decrypter that can also be used to decrypt packets.
  std::unique_ptr<QuicDecrypter> alternative_decrypter_;
  // The encryption level of |decrypter_|.
  EncryptionLevel decrypter_level_;
  // The encryption level of |alternative_decrypter_|.
  EncryptionLevel alternative_decrypter_level_;
  // |alternative_decrypter_latch_| is true if, when |alternative_decrypter_|
  // successfully decrypts a packet, we should install it as the only
  // decrypter.
  bool alternative_decrypter_latch_;
  // Encrypters used to encrypt packets via EncryptPayload().
  std::unique_ptr<QuicEncrypter> encrypter_[NUM_ENCRYPTION_LEVELS];
  // Tracks if the framer is being used by the entity that received the
  // connection or the entity that initiated it.
  Perspective perspective_;
  // If false, skip validation that the public flags are set to legal values.
  bool validate_flags_;
  // The diversification nonce from the last received packet.
  DiversificationNonce last_nonce_;
  // If true, send and process timestamps in the ACK frame.
  bool process_timestamps_;
  // The creation time of the connection, used to calculate timestamps.
  QuicTime creation_time_;
  // The last timestamp received if process_timestamps_ is true.
  QuicTime::Delta last_timestamp_;

  // If not null, framer asks data_producer_ to write stream frame data. Not
  // owned. TODO(fayang): Consider add data producer to framer's constructor.
  QuicStreamFrameDataProducer* data_producer_;

  // Latched value of
  // quic_reloadable_flag_quic_process_stateless_reset_at_client_only.
  const bool process_stateless_reset_at_client_only_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_FRAMER_H_
