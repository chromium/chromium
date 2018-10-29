// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Accumulates frames for the next packet until more frames no longer fit or
// it's time to create a packet from them.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_PACKET_CREATOR_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_PACKET_CREATOR_H_

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "net/third_party/quic/core/quic_connection_close_delegate_interface.h"
#include "net/third_party/quic/core/quic_framer.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/core/quic_pending_retransmission.h"
#include "net/third_party/quic/platform/api/quic_export.h"

namespace quic {
namespace test {
class QuicPacketCreatorPeer;
}

class QUIC_EXPORT_PRIVATE QuicPacketCreator {
 public:
  // A delegate interface for further processing serialized packet.
  class QUIC_EXPORT_PRIVATE DelegateInterface
      : public QuicConnectionCloseDelegateInterface {
   public:
    ~DelegateInterface() override {}
    // Get a buffer of kMaxPacketSize bytes to serialize the next packet.
    // If return nullptr, QuicPacketCreator will serialize on a stack buffer.
    virtual char* GetPacketBuffer() = 0;
    // Called when a packet is serialized. Delegate does not take the ownership
    // of |serialized_packet|, but takes ownership of any frames it removes
    // from |packet.retransmittable_frames|.
    virtual void OnSerializedPacket(SerializedPacket* serialized_packet) = 0;
  };

  // Interface which gets callbacks from the QuicPacketCreator at interesting
  // points.  Implementations must not mutate the state of the creator
  // as a result of these callbacks.
  class QUIC_EXPORT_PRIVATE DebugDelegate {
   public:
    virtual ~DebugDelegate() {}

    // Called when a frame has been added to the current packet.
    virtual void OnFrameAddedToPacket(const QuicFrame& frame) {}
  };

  QuicPacketCreator(QuicConnectionId connection_id,
                    QuicFramer* framer,
                    DelegateInterface* delegate);
  QuicPacketCreator(QuicConnectionId connection_id,
                    QuicFramer* framer,
                    QuicRandom* random,
                    DelegateInterface* delegate);
  QuicPacketCreator(const QuicPacketCreator&) = delete;
  QuicPacketCreator& operator=(const QuicPacketCreator&) = delete;

  ~QuicPacketCreator();

  // Makes the framer not serialize the protocol version in sent packets.
  void StopSendingVersion();

  // SetDiversificationNonce sets the nonce that will be sent in each public
  // header of packets encrypted at the initial encryption level. Should only
  // be called by servers.
  void SetDiversificationNonce(const DiversificationNonce& nonce);

  // Update the packet number length to use in future packets as soon as it
  // can be safely changed.
  // TODO(fayang): Directly set packet number length instead of compute it in
  // creator.
  void UpdatePacketNumberLength(QuicPacketNumber least_packet_awaited_by_peer,
                                QuicPacketCount max_packets_in_flight);

  // The overhead the framing will add for a packet with one frame.
  static size_t StreamFramePacketOverhead(
      QuicTransportVersion version,
      QuicConnectionIdLength destination_connection_id_length,
      QuicConnectionIdLength source_connection_id_length,
      bool include_version,
      bool include_diversification_nonce,
      QuicPacketNumberLength packet_number_length,
      QuicStreamOffset offset);

  // Returns false and flushes all pending frames if current open packet is
  // full.
  // If current packet is not full, creates a stream frame that fits into the
  // open packet and adds it to the packet.
  bool ConsumeData(QuicStreamId id,
                   size_t write_length,
                   size_t iov_offset,
                   QuicStreamOffset offset,
                   bool fin,
                   bool needs_full_padding,
                   QuicFrame* frame);

  // Returns true if current open packet can accommodate more stream frames of
  // stream |id| at |offset| and data length |data_size|, false otherwise.
  bool HasRoomForStreamFrame(QuicStreamId id,
                             QuicStreamOffset offset,
                             size_t data_size);

  // Returns true if current open packet can accomoodate a message frame of
  // |length|.
  bool HasRoomForMessageFrame(QuicByteCount length);

  // Re-serializes frames with the original packet's packet number length.
  // Used for retransmitting packets to ensure they aren't too long.
  void ReserializeAllFrames(const QuicPendingRetransmission& retransmission,
                            char* buffer,
                            size_t buffer_len);

  // Serializes all added frames into a single packet and invokes the delegate_
  // to further process the SerializedPacket.
  void Flush();

  // Optimized method to create a QuicStreamFrame and serialize it. Adds the
  // QuicStreamFrame to the returned SerializedPacket.  Sets
  // |num_bytes_consumed| to the number of bytes consumed to create the
  // QuicStreamFrame.
  void CreateAndSerializeStreamFrame(QuicStreamId id,
                                     size_t write_length,
                                     QuicStreamOffset iov_offset,
                                     QuicStreamOffset stream_offset,
                                     bool fin,
                                     size_t* num_bytes_consumed);

  // Returns true if there are frames pending to be serialized.
  bool HasPendingFrames() const;

  // Returns true if there are retransmittable frames pending to be serialized.
  bool HasPendingRetransmittableFrames() const;

  // Returns true if there are stream frames for |id| pending to be serialized.
  bool HasPendingStreamFramesOfStream(QuicStreamId id) const;

  // Returns the number of bytes which are available to be used by additional
  // frames in the packet.  Since stream frames are slightly smaller when they
  // are the last frame in a packet, this method will return a different
  // value than max_packet_size - PacketSize(), in this case.
  size_t BytesFree();

  // Returns the number of bytes that the packet will expand by if a new frame
  // is added to the packet. If the last frame was a stream frame, it will
  // expand slightly when a new frame is added, and this method returns the
  // amount of expected expansion.
  size_t ExpansionOnNewFrame() const;

  // Returns the number of bytes in the current packet, including the header,
  // if serialized with the current frames.  Adding a frame to the packet
  // may change the serialized length of existing frames, as per the comment
  // in BytesFree.
  size_t PacketSize();

  // Tries to add |frame| to the packet creator's list of frames to be
  // serialized. If the frame does not fit into the current packet, flushes the
  // packet and returns false.
  bool AddSavedFrame(const QuicFrame& frame);

  // Identical to AddSavedFrame, but allows the frame to be padded.
  bool AddPaddedSavedFrame(const QuicFrame& frame);

  // Creates a version negotiation packet which supports |supported_versions|.
  std::unique_ptr<QuicEncryptedPacket> SerializeVersionNegotiationPacket(
      bool ietf_quic,
      const ParsedQuicVersionVector& supported_versions);

  // Creates a connectivity probing packet for versions prior to version 99.
  OwningSerializedPacketPointer SerializeConnectivityProbingPacket();

  // Create connectivity probing request and response packets using PATH
  // CHALLENGE and PATH RESPONSE frames, respectively, for version 99/IETF QUIC.
  // SerializePathChallengeConnectivityProbingPacket will pad the packet to be
  // MTU bytes long.
  OwningSerializedPacketPointer SerializePathChallengeConnectivityProbingPacket(
      QuicPathFrameBuffer* payload);

  // If |is_padded| is true then SerializePathResponseConnectivityProbingPacket
  // will pad the packet to be MTU bytes long, else it will not pad the packet.
  // |payloads| is cleared.
  OwningSerializedPacketPointer SerializePathResponseConnectivityProbingPacket(
      const QuicDeque<QuicPathFrameBuffer>& payloads,
      const bool is_padded);

  // Returns a dummy packet that is valid but contains no useful information.
  static SerializedPacket NoPacket();

  // Returns length of destination connection ID to send over the wire.
  QuicConnectionIdLength GetDestinationConnectionIdLength() const;

  // Returns length of source connection ID to send over the wire.
  QuicConnectionIdLength GetSourceConnectionIdLength() const;

  // Sets the encryption level that will be applied to new packets.
  void set_encryption_level(EncryptionLevel level) {
    packet_.encryption_level = level;
  }

  // packet number of the last created packet, or 0 if no packets have been
  // created.
  QuicPacketNumber packet_number() const { return packet_.packet_number; }

  void SetConnectionIdLength(QuicConnectionIdLength length);

  QuicByteCount max_packet_length() const { return max_packet_length_; }

  bool has_ack() const { return packet_.has_ack; }

  bool has_stop_waiting() const { return packet_.has_stop_waiting; }

  // Sets the encrypter to use for the encryption level and updates the max
  // plaintext size.
  void SetEncrypter(EncryptionLevel level,
                    std::unique_ptr<QuicEncrypter> encrypter);

  // Indicates whether the packet creator is in a state where it can change
  // current maximum packet length.
  bool CanSetMaxPacketLength() const;

  // Sets the maximum packet length.
  void SetMaxPacketLength(QuicByteCount length);

  // Increases pending_padding_bytes by |size|. Pending padding will be sent by
  // MaybeAddPadding().
  void AddPendingPadding(QuicByteCount size);

  // Sets transmission type of next constructed packets.
  void SetTransmissionType(TransmissionType type);

  // Sets long header type of next constructed packets.
  void SetLongHeaderType(QuicLongHeaderType type);

  // Returns the largest payload that will fit into a single MESSAGE frame.
  QuicPacketLength GetLargestMessagePayload() const;

  void set_debug_delegate(DebugDelegate* debug_delegate) {
    debug_delegate_ = debug_delegate;
  }

  void set_can_set_transmission_type(bool can_set_transmission_type) {
    can_set_transmission_type_ = can_set_transmission_type;
  }

  QuicByteCount pending_padding_bytes() const { return pending_padding_bytes_; }

  QuicTransportVersion transport_version() const {
    return framer_->transport_version();
  }

 private:
  friend class test::QuicPacketCreatorPeer;

  static bool ShouldRetransmit(const QuicFrame& frame);

  // Creates a stream frame which fits into the current open packet. If
  // |write_length| is 0 and fin is true, the expected behavior is to consume
  // the fin but return 0.
  void CreateStreamFrame(QuicStreamId id,
                         size_t write_length,
                         size_t iov_offset,
                         QuicStreamOffset offset,
                         bool fin,
                         QuicFrame* frame);

  void FillPacketHeader(QuicPacketHeader* header);

  // Adds a |frame| if there is space and returns false and flushes all pending
  // frames if there isn't room. If |save_retransmittable_frames| is true,
  // saves the |frame| in the next SerializedPacket.
  bool AddFrame(const QuicFrame& frame, bool save_retransmittable_frames);

  // Adds a padding frame to the current packet (if there is space) when (1)
  // current packet needs full padding or (2) there are pending paddings.
  void MaybeAddPadding();

  // Serializes all frames which have been added and adds any which should be
  // retransmitted to packet_.retransmittable_frames. All frames must fit into
  // a single packet.
  // Fails if |buffer_len| isn't long enough for the encrypted packet.
  void SerializePacket(char* encrypted_buffer, size_t buffer_len);

  // Called after a new SerialiedPacket is created to call the delegate's
  // OnSerializedPacket and reset state.
  void OnSerializedPacket();

  // Clears all fields of packet_ that should be cleared between serializations.
  void ClearPacket();

  // Returns true if a diversification nonce should be included in the current
  // packet's header.
  bool IncludeNonceInPublicHeader() const;

  // Returns true if version should be included in current packet's header.
  bool IncludeVersionInHeader() const;

  // Returns length of packet number to send over the wire.
  // packet_.packet_number_length should never be read directly, use this
  // function instead.
  QuicPacketNumberLength GetPacketNumberLength() const;

  // Returns true if |frame| starts with CHLO.
  bool StreamFrameStartsWithChlo(const QuicStreamFrame& frame) const;

  // Returns true if packet under construction has IETF long header.
  bool HasIetfLongHeader() const;

  // Does not own these delegates or the framer.
  DelegateInterface* delegate_;
  DebugDelegate* debug_delegate_;
  QuicFramer* framer_;
  QuicRandom* random_;

  // Controls whether version should be included while serializing the packet.
  // send_version_in_packet_ should never be read directly, use
  // IncludeVersionInHeader() instead.
  bool send_version_in_packet_;
  // If true, then |diversification_nonce_| will be included in the header of
  // all packets created at the initial encryption level.
  bool have_diversification_nonce_;
  DiversificationNonce diversification_nonce_;
  // Maximum length including headers and encryption (UDP payload length.)
  QuicByteCount max_packet_length_;
  size_t max_plaintext_size_;
  // Length of connection_id to send over the wire. connection_id_length_ should
  // never be read directly, use GetConnectionIdLength() instead.
  QuicConnectionIdLength connection_id_length_;

  // Frames to be added to the next SerializedPacket
  QuicFrames queued_frames_;

  // packet_size should never be read directly, use PacketSize() instead.
  // TODO(ianswett): Move packet_size_ into SerializedPacket once
  // QuicEncryptedPacket has been flattened into SerializedPacket.
  size_t packet_size_;
  QuicConnectionId connection_id_;

  // Packet used to invoke OnSerializedPacket.
  SerializedPacket packet_;

  // Long header type of next constructed packets.
  QuicLongHeaderType long_header_type_;

  // Pending padding bytes to send. Pending padding bytes will be sent in next
  // packet(s) (after all other frames) if current constructed packet does not
  // have room to send all of them.
  QuicByteCount pending_padding_bytes_;

  // Indicates whether current constructed packet needs full padding to max
  // packet size. Please note, full padding does not consume pending padding
  // bytes.
  bool needs_full_padding_;

  // If true, packet_'s transmission type is only set by
  // SetPacketTransmissionType and does not get cleared in ClearPacket.
  bool can_set_transmission_type_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_PACKET_CREATOR_H_
