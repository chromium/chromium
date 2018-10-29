// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_dispatcher.h"

#include <utility>

#include "base/macros.h"
#include "net/third_party/quic/core/chlo_extractor.h"
#include "net/third_party/quic/core/crypto/crypto_protocol.h"
#include "net/third_party/quic/core/crypto/quic_random.h"
#include "net/third_party/quic/core/quic_time_wait_list_manager.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/core/stateless_rejector.h"
#include "net/third_party/quic/platform/api/quic_bug_tracker.h"
#include "net/third_party/quic/platform/api/quic_flag_utils.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_stack_trace.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"

namespace quic {

typedef QuicBufferedPacketStore::BufferedPacket BufferedPacket;
typedef QuicBufferedPacketStore::BufferedPacketList BufferedPacketList;
typedef QuicBufferedPacketStore::EnqueuePacketResult EnqueuePacketResult;

namespace {

// An alarm that informs the QuicDispatcher to delete old sessions.
class DeleteSessionsAlarm : public QuicAlarm::Delegate {
 public:
  explicit DeleteSessionsAlarm(QuicDispatcher* dispatcher)
      : dispatcher_(dispatcher) {}
  DeleteSessionsAlarm(const DeleteSessionsAlarm&) = delete;
  DeleteSessionsAlarm& operator=(const DeleteSessionsAlarm&) = delete;

  void OnAlarm() override { dispatcher_->DeleteSessions(); }

 private:
  // Not owned.
  QuicDispatcher* dispatcher_;
};

// Collects packets serialized by a QuicPacketCreator in order
// to be handed off to the time wait list manager.
class PacketCollector : public QuicPacketCreator::DelegateInterface,
                        public QuicStreamFrameDataProducer {
 public:
  explicit PacketCollector(QuicBufferAllocator* allocator)
      : send_buffer_(allocator) {}
  ~PacketCollector() override = default;

  // QuicPacketCreator::DelegateInterface methods:
  void OnSerializedPacket(SerializedPacket* serialized_packet) override {
    // Make a copy of the serialized packet to send later.
    packets_.emplace_back(
        new QuicEncryptedPacket(CopyBuffer(*serialized_packet),
                                serialized_packet->encrypted_length, true));
    serialized_packet->encrypted_buffer = nullptr;
    DeleteFrames(&(serialized_packet->retransmittable_frames));
    serialized_packet->retransmittable_frames.clear();
  }

  char* GetPacketBuffer() override {
    // Let QuicPacketCreator to serialize packets on stack buffer.
    return nullptr;
  }

  void OnUnrecoverableError(QuicErrorCode error,
                            const QuicString& error_details,
                            ConnectionCloseSource source) override {}

  void SaveStatelessRejectFrameData(QuicStringPiece reject) {
    struct iovec iovec;
    iovec.iov_base = const_cast<char*>(reject.data());
    iovec.iov_len = reject.length();
    send_buffer_.SaveStreamData(&iovec, 1, 0, iovec.iov_len);
  }

  // QuicStreamFrameDataProducer
  WriteStreamDataResult WriteStreamData(QuicStreamId id,
                                        QuicStreamOffset offset,
                                        QuicByteCount data_length,
                                        QuicDataWriter* writer) override {
    if (send_buffer_.WriteStreamData(offset, data_length, writer)) {
      return WRITE_SUCCESS;
    }
    return WRITE_FAILED;
  }

  std::vector<std::unique_ptr<QuicEncryptedPacket>>* packets() {
    return &packets_;
  }

 private:
  std::vector<std::unique_ptr<QuicEncryptedPacket>> packets_;
  // This is only needed until the packets are encrypted. Once packets are
  // encrypted, the stream data is no longer required.
  QuicStreamSendBuffer send_buffer_;
};

// Helper for statelessly closing connections by generating the
// correct termination packets and adding the connection to the time wait
// list manager.
class StatelessConnectionTerminator {
 public:
  StatelessConnectionTerminator(QuicConnectionId connection_id,
                                QuicFramer* framer,
                                QuicConnectionHelperInterface* helper,
                                QuicTimeWaitListManager* time_wait_list_manager)
      : connection_id_(connection_id),
        framer_(framer),
        collector_(helper->GetStreamSendBufferAllocator()),
        creator_(connection_id, framer, &collector_),
        time_wait_list_manager_(time_wait_list_manager) {
    framer_->set_data_producer(&collector_);
  }

  ~StatelessConnectionTerminator() {
    // Clear framer's producer.
    framer_->set_data_producer(nullptr);
  }

  // Generates a packet containing a CONNECTION_CLOSE frame specifying
  // |error_code| and |error_details| and add the connection to time wait.
  void CloseConnection(QuicErrorCode error_code,
                       const QuicString& error_details) {
    QuicConnectionCloseFrame* frame = new QuicConnectionCloseFrame;
    frame->error_code = error_code;
    frame->error_details = error_details;
    // TODO(fayang): Use the right long header type for conneciton close sent by
    // dispatcher.
    creator_.SetLongHeaderType(RETRY);
    if (!creator_.AddSavedFrame(QuicFrame(frame))) {
      QUIC_BUG << "Unable to add frame to an empty packet";
      delete frame;
      return;
    }
    creator_.Flush();
    DCHECK_EQ(1u, collector_.packets()->size());
    time_wait_list_manager_->AddConnectionIdToTimeWait(
        connection_id_, framer_->last_packet_is_ietf_quic(),
        QuicTimeWaitListManager::SEND_TERMINATION_PACKETS,
        collector_.packets());
  }

  // Generates a series of termination packets containing the crypto handshake
  // message |reject|.  Adds the connection to time wait list with the
  // generated packets.
  void RejectConnection(QuicStringPiece reject) {
    QuicStreamOffset offset = 0;
    collector_.SaveStatelessRejectFrameData(reject);
    while (offset < reject.length()) {
      QuicFrame frame;
      creator_.SetLongHeaderType(RETRY);
      if (!creator_.ConsumeData(
              QuicUtils::GetCryptoStreamId(framer_->transport_version()),
              reject.length(), offset, offset,
              /*fin=*/false,
              /*needs_full_padding=*/true, &frame)) {
        QUIC_BUG << "Unable to consume data into an empty packet.";
        return;
      }
      offset += frame.stream_frame.data_length;
      if (offset < reject.length()) {
        DCHECK(!creator_.HasRoomForStreamFrame(
            QuicUtils::GetCryptoStreamId(framer_->transport_version()), offset,
            frame.stream_frame.data_length));
      }
      creator_.Flush();
    }
    time_wait_list_manager_->AddConnectionIdToTimeWait(
        connection_id_, framer_->last_packet_is_ietf_quic(),
        QuicTimeWaitListManager::SEND_TERMINATION_PACKETS,
        collector_.packets());
    DCHECK(time_wait_list_manager_->IsConnectionIdInTimeWait(connection_id_));
  }

 private:
  QuicConnectionId connection_id_;
  QuicFramer* framer_;  // Unowned.
  // Set as the visitor of |creator_| to collect any generated packets.
  PacketCollector collector_;
  QuicPacketCreator creator_;
  QuicTimeWaitListManager* time_wait_list_manager_;
};

// Class which extracts the ALPN from a CHLO packet.
class ChloAlpnExtractor : public ChloExtractor::Delegate {
 public:
  void OnChlo(QuicTransportVersion version,
              QuicConnectionId connection_id,
              const CryptoHandshakeMessage& chlo) override {
    QuicStringPiece alpn_value;
    if (chlo.GetStringPiece(kALPN, &alpn_value)) {
      alpn_ = QuicString(alpn_value);
    }
  }

  QuicString&& ConsumeAlpn() { return std::move(alpn_); }

 private:
  QuicString alpn_;
};

// Class which sits between the ChloExtractor and the StatelessRejector
// to give the QuicDispatcher a chance to apply policy checks to the CHLO.
class ChloValidator : public ChloAlpnExtractor {
 public:
  ChloValidator(QuicCryptoServerStream::Helper* helper,
                const QuicSocketAddress& client_address,
                const QuicSocketAddress& peer_address,
                const QuicSocketAddress& self_address,
                StatelessRejector* rejector)
      : helper_(helper),
        client_address_(client_address),
        peer_address_(peer_address),
        self_address_(self_address),
        rejector_(rejector),
        can_accept_(false),
        error_details_("CHLO not processed") {}

  // ChloExtractor::Delegate implementation.
  void OnChlo(QuicTransportVersion version,
              QuicConnectionId connection_id,
              const CryptoHandshakeMessage& chlo) override {
    // Extract the ALPN
    ChloAlpnExtractor::OnChlo(version, connection_id, chlo);
    if (helper_->CanAcceptClientHello(chlo, client_address_, peer_address_,
                                      self_address_, &error_details_)) {
      can_accept_ = true;
      rejector_->OnChlo(version, connection_id,
                        helper_->GenerateConnectionIdForReject(connection_id),
                        chlo);
    }
  }

  bool can_accept() const { return can_accept_; }

  const QuicString& error_details() const { return error_details_; }

 private:
  QuicCryptoServerStream::Helper* helper_;  // Unowned.
  // client_address_ and peer_address_ could be different values for proxy
  // connections.
  QuicSocketAddress client_address_;
  QuicSocketAddress peer_address_;
  QuicSocketAddress self_address_;
  StatelessRejector* rejector_;  // Unowned.
  bool can_accept_;
  QuicString error_details_;
};

}  // namespace

QuicDispatcher::QuicDispatcher(
    const QuicConfig& config,
    const QuicCryptoServerConfig* crypto_config,
    QuicVersionManager* version_manager,
    std::unique_ptr<QuicConnectionHelperInterface> helper,
    std::unique_ptr<QuicCryptoServerStream::Helper> session_helper,
    std::unique_ptr<QuicAlarmFactory> alarm_factory)
    : config_(config),
      crypto_config_(crypto_config),
      compressed_certs_cache_(
          QuicCompressedCertsCache::kQuicCompressedCertsCacheSize),
      helper_(std::move(helper)),
      session_helper_(std::move(session_helper)),
      alarm_factory_(std::move(alarm_factory)),
      delete_sessions_alarm_(
          alarm_factory_->CreateAlarm(new DeleteSessionsAlarm(this))),
      buffered_packets_(this, helper_->GetClock(), alarm_factory_.get()),
      current_packet_(nullptr),
      version_manager_(version_manager),
      framer_(GetSupportedVersions(),
              /*unused*/ QuicTime::Zero(),
              Perspective::IS_SERVER),
      last_error_(QUIC_NO_ERROR),
      new_sessions_allowed_per_event_loop_(0u),
      accept_new_connections_(true) {
  framer_.set_visitor(this);
}

QuicDispatcher::~QuicDispatcher() {
  session_map_.clear();
  closed_session_list_.clear();
}

void QuicDispatcher::InitializeWithWriter(QuicPacketWriter* writer) {
  DCHECK(writer_ == nullptr);
  writer_.reset(writer);
  time_wait_list_manager_.reset(CreateQuicTimeWaitListManager());
}

void QuicDispatcher::ProcessPacket(const QuicSocketAddress& self_address,
                                   const QuicSocketAddress& peer_address,
                                   const QuicReceivedPacket& packet) {
  current_self_address_ = self_address;
  current_peer_address_ = peer_address;
  // GetClientAddress must be called after current_peer_address_ is set.
  current_client_address_ = GetClientAddress();
  current_packet_ = &packet;
  // ProcessPacket will cause the packet to be dispatched in
  // OnUnauthenticatedPublicHeader, or sent to the time wait list manager
  // in OnUnauthenticatedHeader.
  framer_.ProcessPacket(packet);
  // TODO(rjshade): Return a status describing if/why a packet was dropped,
  //                and log somehow.  Maybe expose as a varz.
  // TODO(wub): Consider invalidate the current_* variables so processing of the
  //            next packet does not use them incorrectly.
}

bool QuicDispatcher::OnUnauthenticatedPublicHeader(
    const QuicPacketHeader& header) {
  current_connection_id_ = header.destination_connection_id;

  // Port zero is only allowed for unidirectional UDP, so is disallowed by QUIC.
  // Given that we can't even send a reply rejecting the packet, just drop the
  // packet.
  if (current_peer_address_.port() == 0) {
    return false;
  }

  // Stopgap test: The code does not construct full-length connection IDs
  // correctly from truncated connection ID fields.  Prevent this from causing
  // the connection ID lookup to error by dropping any packet with a short
  // connection ID.
  if (header.destination_connection_id_length != PACKET_8BYTE_CONNECTION_ID) {
    return false;
  }

  // Packets with connection IDs for active connections are processed
  // immediately.
  QuicConnectionId connection_id = header.destination_connection_id;
  auto it = session_map_.find(connection_id);
  if (it != session_map_.end()) {
    DCHECK(!buffered_packets_.HasBufferedPackets(connection_id));
    it->second->ProcessUdpPacket(current_self_address_, current_peer_address_,
                                 *current_packet_);
    return false;
  }

  if (buffered_packets_.HasChloForConnection(connection_id)) {
    BufferEarlyPacket(connection_id, framer_.last_packet_is_ietf_quic());
    return false;
  }

  // Check if we are buffering packets for this connection ID
  if (temporarily_buffered_connections_.find(connection_id) !=
      temporarily_buffered_connections_.end()) {
    // This packet was received while the a CHLO for the same connection ID was
    // being processed.  Buffer it.
    BufferEarlyPacket(connection_id, framer_.last_packet_is_ietf_quic());
    return false;
  }

  if (!OnUnauthenticatedUnknownPublicHeader(header)) {
    return false;
  }

  // If the packet is a public reset for a connection ID that is not active,
  // there is nothing we must do or can do.
  if (header.reset_flag) {
    return false;
  }

  if (time_wait_list_manager_->IsConnectionIdInTimeWait(connection_id)) {
    // This connection ID is already in time-wait state.
    time_wait_list_manager_->ProcessPacket(current_self_address_,
                                           current_peer_address_,
                                           header.destination_connection_id);
    return false;
  }

  // The packet has an unknown connection ID.

  // Unless the packet provides a version, assume that we can continue
  // processing using our preferred version.
  ParsedQuicVersion version = GetSupportedVersions().front();
  if (header.version_flag) {
    ParsedQuicVersion packet_version = header.version;
    if (framer_.supported_versions() != GetSupportedVersions()) {
      // Reset framer's version if version flags change in flight.
      framer_.SetSupportedVersions(GetSupportedVersions());
    }
    if (!framer_.IsSupportedVersion(packet_version)) {
      if (ShouldCreateSessionForUnknownVersion(framer_.last_version_label())) {
        return true;
      }
      // Since the version is not supported, send a version negotiation
      // packet and stop processing the current packet.
      time_wait_list_manager()->SendVersionNegotiationPacket(
          connection_id, framer_.last_packet_is_ietf_quic(),
          GetSupportedVersions(), current_self_address_, current_peer_address_);
      return false;
    }
    version = packet_version;
  }
  // Set the framer's version and continue processing.
  framer_.set_version(version);
  return true;
}

bool QuicDispatcher::OnUnauthenticatedHeader(const QuicPacketHeader& header) {
  QuicConnectionId connection_id = header.destination_connection_id;
  // Packet's connection ID is unknown.  Apply the validity checks.
  QuicPacketFate fate = ValidityChecks(header);
  if (fate == kFateProcess) {
    // Execute stateless rejection logic to determine the packet fate, then
    // invoke ProcessUnauthenticatedHeaderFate.
    MaybeRejectStatelessly(connection_id, header.version);
  } else {
    // If the fate is already known, process it without executing stateless
    // rejection logic.
    ProcessUnauthenticatedHeaderFate(fate, connection_id);
  }

  return false;
}

void QuicDispatcher::ProcessUnauthenticatedHeaderFate(
    QuicPacketFate fate,
    QuicConnectionId connection_id) {
  switch (fate) {
    case kFateProcess: {
      ProcessChlo();
      break;
    }
    case kFateTimeWait:
      // MaybeRejectStatelessly or OnExpiredPackets might have already added the
      // connection to time wait, in which case it should not be added again.
      if (!GetQuicReloadableFlag(quic_use_cheap_stateless_rejects) ||
          !time_wait_list_manager_->IsConnectionIdInTimeWait(connection_id)) {
        // Add this connection_id to the time-wait state, to safely reject
        // future packets.
        QUIC_DLOG(INFO) << "Adding connection ID " << connection_id
                        << "to time-wait list.";
        StatelesslyTerminateConnection(
            connection_id, framer()->GetLastPacketFormat(),
            QUIC_HANDSHAKE_FAILED, "Reject connection",
            quic::QuicTimeWaitListManager::SEND_STATELESS_RESET);
      }
      DCHECK(time_wait_list_manager_->IsConnectionIdInTimeWait(connection_id));
      time_wait_list_manager_->ProcessPacket(
          current_self_address_, current_peer_address_, connection_id);

      // Any packets which were buffered while the stateless rejector logic was
      // running should be discarded.  Do not inform the time wait list manager,
      // which should already have a made a decision about sending a reject
      // based on the CHLO alone.
      buffered_packets_.DiscardPackets(connection_id);
      break;
    case kFateBuffer:
      // This packet is a non-CHLO packet which has arrived before the
      // corresponding CHLO, *or* this packet was received while the
      // corresponding CHLO was being processed.  Buffer it.
      BufferEarlyPacket(connection_id, framer_.last_packet_is_ietf_quic());
      break;
    case kFateDrop:
      // Do nothing with the packet.
      break;
  }
}

QuicDispatcher::QuicPacketFate QuicDispatcher::ValidityChecks(
    const QuicPacketHeader& header) {
  // To have all the checks work properly without tears, insert any new check
  // into the framework of this method in the section for checks that return the
  // check's fate value.  The sections for checks must be ordered with the
  // highest priority fate first.

  // Checks that return kFateDrop.

  // Checks that return kFateTimeWait.

  // All packets within a connection sent by a client before receiving a
  // response from the server are required to have the version negotiation flag
  // set.  Since this may be a client continuing a connection we lost track of
  // via server restart, send a rejection to fast-fail the connection.
  if (!header.version_flag) {
    QUIC_DLOG(INFO)
        << "Packet without version arrived for unknown connection ID "
        << header.destination_connection_id;
    return kFateTimeWait;
  }

  // initial packet number of 0 is always invalid.
  if (header.packet_number == kInvalidPacketNumber) {
    return kFateTimeWait;
  }
  if (GetQuicRestartFlag(quic_enable_accept_random_ipn)) {
    QUIC_FLAG_COUNT_N(quic_restart_flag_quic_enable_accept_random_ipn, 1, 2);
    // Accepting Initial Packet Numbers in 1...((2^31)-1) range... check
    // maximum accordingly.
    if (header.packet_number > kMaxRandomInitialPacketNumber) {
      return kFateTimeWait;
    }
  } else {
    // Count those that would have been accepted if FLAGS..random_ipn
    // were true -- to detect/diagnose potential issues prior to
    // enabling the flag.
    if ((header.packet_number > kMaxReasonableInitialPacketNumber) &&
        (header.packet_number <= kMaxRandomInitialPacketNumber)) {
      QUIC_CODE_COUNT_N(had_possibly_random_ipn, 1, 2);
    }
    // Check that the sequence number is within the range that the client is
    // expected to send before receiving a response from the server.
    if (header.packet_number > kMaxReasonableInitialPacketNumber) {
      return kFateTimeWait;
    }
  }
  return kFateProcess;
}

void QuicDispatcher::CleanUpSession(SessionMap::iterator it,
                                    QuicConnection* connection,
                                    bool should_close_statelessly) {
  write_blocked_list_.erase(connection);
  if (should_close_statelessly) {
    DCHECK(connection->termination_packets() != nullptr &&
           !connection->termination_packets()->empty());
  }
  QuicTimeWaitListManager::TimeWaitAction action =
      QuicTimeWaitListManager::SEND_STATELESS_RESET;
  if (connection->termination_packets() != nullptr &&
      !connection->termination_packets()->empty()) {
    action = QuicTimeWaitListManager::SEND_TERMINATION_PACKETS;
  } else if (connection->transport_version() > QUIC_VERSION_43) {
    action = QuicTimeWaitListManager::DO_NOTHING;
  }
  time_wait_list_manager_->AddConnectionIdToTimeWait(
      it->first, connection->transport_version() > QUIC_VERSION_43, action,
      connection->termination_packets());
  session_map_.erase(it);
}

void QuicDispatcher::StopAcceptingNewConnections() {
  accept_new_connections_ = false;
}

bool QuicDispatcher::ShouldAddToBlockedList() {
  return writer_->IsWriteBlocked();
}

std::unique_ptr<QuicDispatcher::PerPacketContext>
QuicDispatcher::GetPerPacketContext() const {
  return nullptr;
}

void QuicDispatcher::DeleteSessions() {
  closed_session_list_.clear();
}

void QuicDispatcher::OnCanWrite() {
  // The socket is now writable.
  writer_->SetWritable();

  // Give all the blocked writers one chance to write, until we're blocked again
  // or there's no work left.
  while (!write_blocked_list_.empty() && !writer_->IsWriteBlocked()) {
    QuicBlockedWriterInterface* blocked_writer =
        write_blocked_list_.begin()->first;
    write_blocked_list_.erase(write_blocked_list_.begin());
    blocked_writer->OnBlockedWriterCanWrite();
  }
}

bool QuicDispatcher::HasPendingWrites() const {
  return !write_blocked_list_.empty();
}

void QuicDispatcher::Shutdown() {
  while (!session_map_.empty()) {
    QuicSession* session = session_map_.begin()->second.get();
    session->connection()->CloseConnection(
        QUIC_PEER_GOING_AWAY, "Server shutdown imminent",
        ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    // Validate that the session removes itself from the session map on close.
    DCHECK(session_map_.empty() ||
           session_map_.begin()->second.get() != session);
  }
  DeleteSessions();
}

void QuicDispatcher::OnConnectionClosed(QuicConnectionId connection_id,
                                        QuicErrorCode error,
                                        const QuicString& error_details) {
  auto it = session_map_.find(connection_id);
  if (it == session_map_.end()) {
    QUIC_BUG << "ConnectionId " << connection_id
             << " does not exist in the session map.  Error: "
             << QuicErrorCodeToString(error);
    QUIC_BUG << QuicStackTrace();
    return;
  }

  QUIC_DLOG_IF(INFO, error != QUIC_NO_ERROR)
      << "Closing connection (" << connection_id
      << ") due to error: " << QuicErrorCodeToString(error)
      << ", with details: " << error_details;

  QuicConnection* connection = it->second->connection();
  if (ShouldDestroySessionAsynchronously()) {
    // Set up alarm to fire immediately to bring destruction of this session
    // out of current call stack.
    if (closed_session_list_.empty()) {
      delete_sessions_alarm_->Update(helper()->GetClock()->ApproximateNow(),
                                     QuicTime::Delta::Zero());
    }
    closed_session_list_.push_back(std::move(it->second));
  }
  const bool should_close_statelessly =
      (error == QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT);
  CleanUpSession(it, connection, should_close_statelessly);
}

void QuicDispatcher::OnWriteBlocked(
    QuicBlockedWriterInterface* blocked_writer) {
  if (!ShouldAddToBlockedList()) {
    QUIC_BUG
        << "Tried to add writer into blocked list when it shouldn't be added";
    // Return without adding the connection to the blocked list, to avoid
    // infinite loops in OnCanWrite.
    return;
  }
  write_blocked_list_.insert(std::make_pair(blocked_writer, true));
}

void QuicDispatcher::OnRstStreamReceived(const QuicRstStreamFrame& frame) {}

void QuicDispatcher::OnConnectionAddedToTimeWaitList(
    QuicConnectionId connection_id) {
  QUIC_DLOG(INFO) << "Connection " << connection_id
                  << " added to time wait list.";
}

void QuicDispatcher::StatelesslyTerminateConnection(
    QuicConnectionId connection_id,
    PacketHeaderFormat format,
    QuicErrorCode error_code,
    const QuicString& error_details,
    QuicTimeWaitListManager::TimeWaitAction action) {
  if (format == IETF_QUIC_LONG_HEADER_PACKET) {
    // Send connection close for IETF long header packet, and this also adds
    // connection to time wait list.
    StatelessConnectionTerminator terminator(
        connection_id, &framer_, helper_.get(), time_wait_list_manager_.get());
    terminator.CloseConnection(error_code, error_details);
    return;
  }

  time_wait_list_manager_->AddConnectionIdToTimeWait(
      connection_id, format != GOOGLE_QUIC_PACKET, action,
      /*termination_packets=*/nullptr);
}

void QuicDispatcher::OnPacket() {}

void QuicDispatcher::OnError(QuicFramer* framer) {
  QuicErrorCode error = framer->error();
  SetLastError(error);
  QUIC_DLOG(INFO) << QuicErrorCodeToString(error);
}

bool QuicDispatcher::ShouldCreateSessionForUnknownVersion(
    QuicVersionLabel /*version_label*/) {
  return false;
}

bool QuicDispatcher::OnProtocolVersionMismatch(
    ParsedQuicVersion /*received_version*/) {
  QUIC_BUG_IF(
      !time_wait_list_manager_->IsConnectionIdInTimeWait(
          current_connection_id_) &&
      !ShouldCreateSessionForUnknownVersion(framer_.last_version_label()))
      << "Unexpected version mismatch: "
      << QuicVersionLabelToString(framer_.last_version_label());

  // Keep processing after protocol mismatch - this will be dealt with by the
  // time wait list or connection that we will create.
  return true;
}

void QuicDispatcher::OnPublicResetPacket(
    const QuicPublicResetPacket& /*packet*/) {
  DCHECK(false);
}

void QuicDispatcher::OnVersionNegotiationPacket(
    const QuicVersionNegotiationPacket& /*packet*/) {
  DCHECK(false);
}

void QuicDispatcher::OnDecryptedPacket(EncryptionLevel level) {
  DCHECK(false);
}

bool QuicDispatcher::OnPacketHeader(const QuicPacketHeader& /*header*/) {
  DCHECK(false);
  return false;
}

bool QuicDispatcher::OnStreamFrame(const QuicStreamFrame& /*frame*/) {
  DCHECK(false);
  return false;
}

bool QuicDispatcher::OnCryptoFrame(const QuicCryptoFrame& /*frame*/) {
  DCHECK(false);
  return false;
}

bool QuicDispatcher::OnAckFrameStart(QuicPacketNumber /*largest_acked*/,
                                     QuicTime::Delta /*ack_delay_time*/) {
  DCHECK(false);
  return false;
}

bool QuicDispatcher::OnAckRange(QuicPacketNumber /*start*/,
                                QuicPacketNumber /*end*/) {
  DCHECK(false);
  return false;
}

bool QuicDispatcher::OnAckTimestamp(QuicPacketNumber /*packet_number*/,
                                    QuicTime /*timestamp*/) {
  DCHECK(false);
  return false;
}

bool QuicDispatcher::OnAckFrameEnd(QuicPacketNumber /*start*/) {
  DCHECK(false);
  return false;
}

bool QuicDispatcher::OnStopWaitingFrame(const QuicStopWaitingFrame& /*frame*/) {
  DCHECK(false);
  return false;
}

bool QuicDispatcher::OnPaddingFrame(const QuicPaddingFrame& /*frame*/) {
  DCHECK(false);
  return false;
}

bool QuicDispatcher::OnPingFrame(const QuicPingFrame& /*frame*/) {
  DCHECK(false);
  return false;
}

bool QuicDispatcher::OnRstStreamFrame(const QuicRstStreamFrame& /*frame*/) {
  DCHECK(false);
  return false;
}

bool QuicDispatcher::OnConnectionCloseFrame(
    const QuicConnectionCloseFrame& /*frame*/) {
  DCHECK(false);
  return false;
}

bool QuicDispatcher::OnApplicationCloseFrame(
    const QuicApplicationCloseFrame& /*frame*/) {
  DCHECK(false);
  return false;
}

bool QuicDispatcher::OnMaxStreamIdFrame(const QuicMaxStreamIdFrame& frame) {
  return true;
}

bool QuicDispatcher::OnStreamIdBlockedFrame(
    const QuicStreamIdBlockedFrame& frame) {
  return true;
}

bool QuicDispatcher::OnStopSendingFrame(const QuicStopSendingFrame& /*frame*/) {
  DCHECK(false);
  return false;
}

bool QuicDispatcher::OnPathChallengeFrame(
    const QuicPathChallengeFrame& /*frame*/) {
  DCHECK(false);
  return false;
}

bool QuicDispatcher::OnPathResponseFrame(
    const QuicPathResponseFrame& /*frame*/) {
  DCHECK(false);
  return false;
}

bool QuicDispatcher::OnGoAwayFrame(const QuicGoAwayFrame& /*frame*/) {
  DCHECK(false);
  return false;
}

bool QuicDispatcher::OnWindowUpdateFrame(
    const QuicWindowUpdateFrame& /*frame*/) {
  DCHECK(false);
  return false;
}

bool QuicDispatcher::OnBlockedFrame(const QuicBlockedFrame& frame) {
  DCHECK(false);
  return false;
}

bool QuicDispatcher::OnNewConnectionIdFrame(
    const QuicNewConnectionIdFrame& frame) {
  DCHECK(false);
  return false;
}

bool QuicDispatcher::OnRetireConnectionIdFrame(
    const QuicRetireConnectionIdFrame& frame) {
  DCHECK(false);
  return false;
}

bool QuicDispatcher::OnNewTokenFrame(const QuicNewTokenFrame& frame) {
  DCHECK(false);
  return false;
}

bool QuicDispatcher::OnMessageFrame(const QuicMessageFrame& frame) {
  DCHECK(false);
  return false;
}

void QuicDispatcher::OnPacketComplete() {
  DCHECK(false);
}

bool QuicDispatcher::IsValidStatelessResetToken(QuicUint128 token) const {
  DCHECK(false);
  return false;
}

void QuicDispatcher::OnAuthenticatedIetfStatelessResetPacket(
    const QuicIetfStatelessResetPacket& packet) {
  DCHECK(false);
}

void QuicDispatcher::OnExpiredPackets(
    QuicConnectionId connection_id,
    BufferedPacketList early_arrived_packets) {
  StatelesslyTerminateConnection(
      connection_id,
      early_arrived_packets.ietf_quic ? IETF_QUIC_LONG_HEADER_PACKET
                                      : GOOGLE_QUIC_PACKET,
      QUIC_HANDSHAKE_FAILED, "Packets buffered for too long",
      quic::QuicTimeWaitListManager::SEND_STATELESS_RESET);
}

void QuicDispatcher::ProcessBufferedChlos(size_t max_connections_to_create) {
  // Reset the counter before starting creating connections.
  new_sessions_allowed_per_event_loop_ = max_connections_to_create;
  for (; new_sessions_allowed_per_event_loop_ > 0;
       --new_sessions_allowed_per_event_loop_) {
    QuicConnectionId connection_id;
    BufferedPacketList packet_list =
        buffered_packets_.DeliverPacketsForNextConnection(&connection_id);
    const std::list<BufferedPacket>& packets = packet_list.buffered_packets;
    if (packets.empty()) {
      return;
    }
    QuicSession* session = CreateQuicSession(
        connection_id, packets.front().client_address, packet_list.alpn);
    QUIC_DLOG(INFO) << "Created new session for " << connection_id;
    session_map_.insert(std::make_pair(connection_id, QuicWrapUnique(session)));
    DeliverPacketsToSession(packets, session);
  }
}

bool QuicDispatcher::HasChlosBuffered() const {
  return buffered_packets_.HasChlosBuffered();
}

bool QuicDispatcher::ShouldCreateOrBufferPacketForConnection(
    QuicConnectionId connection_id) {
  VLOG(1) << "Received packet from new connection " << connection_id;
  return true;
}

// Return true if there is any packet buffered in the store.
bool QuicDispatcher::HasBufferedPackets(QuicConnectionId connection_id) {
  return buffered_packets_.HasBufferedPackets(connection_id);
}

void QuicDispatcher::OnBufferPacketFailure(EnqueuePacketResult result,
                                           QuicConnectionId connection_id) {
  QUIC_DLOG(INFO) << "Fail to buffer packet on connection " << connection_id
                  << " because of " << result;
}

void QuicDispatcher::OnConnectionRejectedStatelessly() {}

void QuicDispatcher::OnConnectionClosedStatelessly(QuicErrorCode error) {}

bool QuicDispatcher::ShouldAttemptCheapStatelessRejection() {
  return true;
}

QuicTimeWaitListManager* QuicDispatcher::CreateQuicTimeWaitListManager() {
  return new QuicTimeWaitListManager(writer_.get(), this, helper_->GetClock(),
                                     alarm_factory_.get());
}

void QuicDispatcher::BufferEarlyPacket(QuicConnectionId connection_id,
                                       bool ietf_quic) {
  bool is_new_connection = !buffered_packets_.HasBufferedPackets(connection_id);
  if (is_new_connection &&
      !ShouldCreateOrBufferPacketForConnection(connection_id)) {
    return;
  }
  EnqueuePacketResult rs = buffered_packets_.EnqueuePacket(
      connection_id, ietf_quic, *current_packet_, current_self_address_,
      current_peer_address_, /*is_chlo=*/false, /*alpn=*/"");
  if (rs != EnqueuePacketResult::SUCCESS) {
    OnBufferPacketFailure(rs, connection_id);
  }
}

void QuicDispatcher::ProcessChlo() {
  if (!accept_new_connections_) {
    // Don't any create new connection.
    StatelesslyTerminateConnection(
        current_connection_id(), framer()->GetLastPacketFormat(),
        QUIC_HANDSHAKE_FAILED, "Stop accepting new connections",
        quic::QuicTimeWaitListManager::SEND_STATELESS_RESET);
    // Time wait list will reject the packet correspondingly.
    time_wait_list_manager()->ProcessPacket(current_self_address(),
                                            current_peer_address(),
                                            current_connection_id());
    return;
  }
  if (!buffered_packets_.HasBufferedPackets(current_connection_id_) &&
      !ShouldCreateOrBufferPacketForConnection(current_connection_id_)) {
    return;
  }
  if (FLAGS_quic_allow_chlo_buffering &&
      new_sessions_allowed_per_event_loop_ <= 0) {
    // Can't create new session any more. Wait till next event loop.
    QUIC_BUG_IF(buffered_packets_.HasChloForConnection(current_connection_id_));
    EnqueuePacketResult rs = buffered_packets_.EnqueuePacket(
        current_connection_id_, framer_.last_packet_is_ietf_quic(),
        *current_packet_, current_self_address_, current_peer_address_,
        /*is_chlo=*/true, current_alpn_);
    if (rs != EnqueuePacketResult::SUCCESS) {
      OnBufferPacketFailure(rs, current_connection_id_);
    }
    return;
  }
  // Creates a new session and process all buffered packets for this connection.
  QuicSession* session = CreateQuicSession(
      current_connection_id_, current_peer_address_, current_alpn_);
  QUIC_DLOG(INFO) << "Created new session for " << current_connection_id_;
  session_map_.insert(
      std::make_pair(current_connection_id_, QuicWrapUnique(session)));
  std::list<BufferedPacket> packets =
      buffered_packets_.DeliverPackets(current_connection_id_).buffered_packets;
  // Process CHLO at first.
  session->ProcessUdpPacket(current_self_address_, current_peer_address_,
                            *current_packet_);
  // Deliver queued-up packets in the same order as they arrived.
  // Do this even when flag is off because there might be still some packets
  // buffered in the store before flag is turned off.
  DeliverPacketsToSession(packets, session);
  --new_sessions_allowed_per_event_loop_;
}

const QuicSocketAddress QuicDispatcher::GetClientAddress() const {
  return current_peer_address_;
}

bool QuicDispatcher::ShouldDestroySessionAsynchronously() {
  return true;
}

void QuicDispatcher::SetLastError(QuicErrorCode error) {
  last_error_ = error;
}

bool QuicDispatcher::OnUnauthenticatedUnknownPublicHeader(
    const QuicPacketHeader& header) {
  return true;
}

class StatelessRejectorProcessDoneCallback
    : public StatelessRejector::ProcessDoneCallback {
 public:
  StatelessRejectorProcessDoneCallback(QuicDispatcher* dispatcher,
                                       ParsedQuicVersion first_version)
      : dispatcher_(dispatcher),
        current_client_address_(dispatcher->current_client_address_),
        current_peer_address_(dispatcher->current_peer_address_),
        current_self_address_(dispatcher->current_self_address_),
        additional_context_(dispatcher->GetPerPacketContext()),
        current_packet_(
            dispatcher->current_packet_->Clone()),  // Note: copies the packet
        first_version_(first_version),
        current_packet_is_ietf_quic_(
            dispatcher->framer()->last_packet_is_ietf_quic()) {}

  void Run(std::unique_ptr<StatelessRejector> rejector) override {
    if (additional_context_ != nullptr) {
      dispatcher_->RestorePerPacketContext(std::move(additional_context_));
    }
    dispatcher_->OnStatelessRejectorProcessDone(
        std::move(rejector), current_client_address_, current_peer_address_,
        current_self_address_, std::move(current_packet_), first_version_,
        current_packet_is_ietf_quic_);
  }

 private:
  QuicDispatcher* dispatcher_;
  QuicSocketAddress current_client_address_;
  QuicSocketAddress current_peer_address_;
  QuicSocketAddress current_self_address_;
  // TODO(wub): Wrap all current_* variables into PerPacketContext. And rename
  // |additional_context_| to |context_|.
  std::unique_ptr<QuicDispatcher::PerPacketContext> additional_context_;
  std::unique_ptr<QuicReceivedPacket> current_packet_;
  ParsedQuicVersion first_version_;
  const bool current_packet_is_ietf_quic_;
};

void QuicDispatcher::MaybeRejectStatelessly(QuicConnectionId connection_id,
                                            ParsedQuicVersion version) {
  if (version.handshake_protocol == PROTOCOL_TLS1_3) {
    ProcessUnauthenticatedHeaderFate(kFateProcess, connection_id);
    return;
    // TODO(nharper): Support buffering non-ClientHello packets when using TLS.
  }
  // TODO(rch): This logic should probably live completely inside the rejector.
  if (!FLAGS_quic_allow_chlo_buffering ||
      !GetQuicReloadableFlag(quic_use_cheap_stateless_rejects) ||
      !GetQuicReloadableFlag(enable_quic_stateless_reject_support) ||
      !ShouldAttemptCheapStatelessRejection()) {
    // Not use cheap stateless reject.
    ChloAlpnExtractor alpn_extractor;
    if (FLAGS_quic_allow_chlo_buffering &&
        !ChloExtractor::Extract(*current_packet_, GetSupportedVersions(),
                                config_.create_session_tag_indicators(),
                                &alpn_extractor)) {
      // Buffer non-CHLO packets.
      ProcessUnauthenticatedHeaderFate(kFateBuffer, connection_id);
      return;
    }
    current_alpn_ = alpn_extractor.ConsumeAlpn();
    ProcessUnauthenticatedHeaderFate(kFateProcess, connection_id);
    return;
  }

  std::unique_ptr<StatelessRejector> rejector(new StatelessRejector(
      version, GetSupportedVersions(), crypto_config_, &compressed_certs_cache_,
      helper()->GetClock(), helper()->GetRandomGenerator(),
      current_packet_->length(), current_client_address_,
      current_self_address_));
  ChloValidator validator(session_helper_.get(), current_client_address_,
                          current_peer_address_, current_self_address_,
                          rejector.get());
  if (!ChloExtractor::Extract(*current_packet_, GetSupportedVersions(),
                              config_.create_session_tag_indicators(),
                              &validator)) {
    ProcessUnauthenticatedHeaderFate(kFateBuffer, connection_id);
    return;
  }
  current_alpn_ = validator.ConsumeAlpn();

  if (!validator.can_accept()) {
    // This CHLO is prohibited by policy.
    StatelessConnectionTerminator terminator(connection_id, &framer_, helper(),
                                             time_wait_list_manager_.get());
    terminator.CloseConnection(QUIC_HANDSHAKE_FAILED,
                               validator.error_details());
    OnConnectionClosedStatelessly(QUIC_HANDSHAKE_FAILED);
    ProcessUnauthenticatedHeaderFate(kFateTimeWait, connection_id);
    return;
  }

  // If we were able to make a decision about this CHLO based purely on the
  // information available in OnChlo, just invoke the done callback immediately.
  if (rejector->state() != StatelessRejector::UNKNOWN) {
    ProcessStatelessRejectorState(std::move(rejector),
                                  version.transport_version);
    return;
  }

  // Insert into set of connection IDs to buffer
  const bool ok =
      temporarily_buffered_connections_.insert(connection_id).second;
  QUIC_BUG_IF(!ok)
      << "Processing multiple stateless rejections for connection ID "
      << connection_id;

  // Continue stateless rejector processing
  std::unique_ptr<StatelessRejectorProcessDoneCallback> cb(
      new StatelessRejectorProcessDoneCallback(this, version));
  StatelessRejector::Process(std::move(rejector), std::move(cb));
}

void QuicDispatcher::OnStatelessRejectorProcessDone(
    std::unique_ptr<StatelessRejector> rejector,
    const QuicSocketAddress& current_client_address,
    const QuicSocketAddress& current_peer_address,
    const QuicSocketAddress& current_self_address,
    std::unique_ptr<QuicReceivedPacket> current_packet,
    ParsedQuicVersion first_version,
    bool current_packet_is_ietf_quic) {
  // Reset current_* to correspond to the packet which initiated the stateless
  // reject logic.
  current_client_address_ = current_client_address;
  current_peer_address_ = current_peer_address;
  current_self_address_ = current_self_address;
  current_packet_ = current_packet.get();
  current_connection_id_ = rejector->connection_id();
  framer_.set_version(first_version);
  if (GetQuicReloadableFlag(quic_fix_last_packet_is_ietf_quic)) {
    if (framer_.last_packet_is_ietf_quic() != current_packet_is_ietf_quic) {
      QUIC_FLAG_COUNT(quic_reloadable_flag_quic_fix_last_packet_is_ietf_quic);
    }
    framer_.set_last_packet_is_ietf_quic(current_packet_is_ietf_quic);
  }

  // Stop buffering packets on this connection
  const auto num_erased =
      temporarily_buffered_connections_.erase(rejector->connection_id());
  QUIC_BUG_IF(num_erased != 1) << "Completing stateless rejection logic for "
                                  "non-buffered connection ID "
                               << rejector->connection_id();

  // If this connection has gone into time-wait during the async processing,
  // don't proceed.
  if (time_wait_list_manager_->IsConnectionIdInTimeWait(
          rejector->connection_id())) {
    time_wait_list_manager_->ProcessPacket(
        current_self_address, current_peer_address, rejector->connection_id());
    return;
  }

  ProcessStatelessRejectorState(std::move(rejector),
                                first_version.transport_version);
}

void QuicDispatcher::ProcessStatelessRejectorState(
    std::unique_ptr<StatelessRejector> rejector,
    QuicTransportVersion first_version) {
  QuicPacketFate fate;
  switch (rejector->state()) {
    case StatelessRejector::FAILED: {
      // There was an error processing the client hello.
      StatelessConnectionTerminator terminator(rejector->connection_id(),
                                               &framer_, helper(),
                                               time_wait_list_manager_.get());
      terminator.CloseConnection(rejector->error(), rejector->error_details());
      fate = kFateTimeWait;
      break;
    }

    case StatelessRejector::UNSUPPORTED:
      // Cheap stateless rejects are not supported so process the packet.
      fate = kFateProcess;
      break;

    case StatelessRejector::ACCEPTED:
      // Contains a valid CHLO, so process the packet and create a connection.
      fate = kFateProcess;
      break;

    case StatelessRejector::REJECTED: {
      QUIC_BUG_IF(first_version != framer_.transport_version())
          << "SREJ: Client's version: " << QuicVersionToString(first_version)
          << " is different from current dispatcher framer's version: "
          << QuicVersionToString(framer_.transport_version());
      StatelessConnectionTerminator terminator(rejector->connection_id(),
                                               &framer_, helper(),
                                               time_wait_list_manager_.get());
      terminator.RejectConnection(
          rejector->reply().GetSerialized().AsStringPiece());
      OnConnectionRejectedStatelessly();
      fate = kFateTimeWait;
      break;
    }

    default:
      QUIC_BUG << "Rejector has invalid state " << rejector->state();
      fate = kFateDrop;
      break;
  }
  ProcessUnauthenticatedHeaderFate(fate, rejector->connection_id());
}

const QuicTransportVersionVector&
QuicDispatcher::GetSupportedTransportVersions() {
  return version_manager_->GetSupportedTransportVersions();
}

const ParsedQuicVersionVector& QuicDispatcher::GetSupportedVersions() {
  return version_manager_->GetSupportedVersions();
}

void QuicDispatcher::DeliverPacketsToSession(
    const std::list<BufferedPacket>& packets,
    QuicSession* session) {
  for (const BufferedPacket& packet : packets) {
    session->ProcessUdpPacket(packet.server_address, packet.client_address,
                              *(packet.packet));
  }
}

}  // namespace quic
