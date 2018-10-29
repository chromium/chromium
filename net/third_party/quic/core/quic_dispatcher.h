// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A server side dispatcher which dispatches a given client's data to their
// stream.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_DISPATCHER_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_DISPATCHER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "net/third_party/quic/core/crypto/quic_compressed_certs_cache.h"
#include "net/third_party/quic/core/crypto/quic_random.h"
#include "net/third_party/quic/core/quic_blocked_writer_interface.h"
#include "net/third_party/quic/core/quic_buffered_packet_store.h"
#include "net/third_party/quic/core/quic_connection.h"
#include "net/third_party/quic/core/quic_crypto_server_stream.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/core/quic_process_packet_interface.h"
#include "net/third_party/quic/core/quic_session.h"
#include "net/third_party/quic/core/quic_time_wait_list_manager.h"
#include "net/third_party/quic/core/quic_version_manager.h"
#include "net/third_party/quic/core/stateless_rejector.h"
#include "net/third_party/quic/platform/api/quic_containers.h"
#include "net/third_party/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quic/platform/api/quic_string.h"

namespace quic {
namespace test {
class QuicDispatcherPeer;
}  // namespace test

class QuicConfig;
class QuicCryptoServerConfig;

class QuicDispatcher : public QuicTimeWaitListManager::Visitor,
                       public ProcessPacketInterface,
                       public QuicFramerVisitorInterface,
                       public QuicBufferedPacketStore::VisitorInterface {
 public:
  // Ideally we'd have a linked_hash_set: the  boolean is unused.
  typedef QuicLinkedHashMap<QuicBlockedWriterInterface*, bool> WriteBlockedList;

  QuicDispatcher(const QuicConfig& config,
                 const QuicCryptoServerConfig* crypto_config,
                 QuicVersionManager* version_manager,
                 std::unique_ptr<QuicConnectionHelperInterface> helper,
                 std::unique_ptr<QuicCryptoServerStream::Helper> session_helper,
                 std::unique_ptr<QuicAlarmFactory> alarm_factory);
  QuicDispatcher(const QuicDispatcher&) = delete;
  QuicDispatcher& operator=(const QuicDispatcher&) = delete;

  ~QuicDispatcher() override;

  // Takes ownership of |writer|.
  void InitializeWithWriter(QuicPacketWriter* writer);

  // Process the incoming packet by creating a new session, passing it to
  // an existing session, or passing it to the time wait list.
  void ProcessPacket(const QuicSocketAddress& self_address,
                     const QuicSocketAddress& peer_address,
                     const QuicReceivedPacket& packet) override;

  // Called when the socket becomes writable to allow queued writes to happen.
  virtual void OnCanWrite();

  // Returns true if there's anything in the blocked writer list.
  virtual bool HasPendingWrites() const;

  // Sends ConnectionClose frames to all connected clients.
  void Shutdown();

  // QuicSession::Visitor interface implementation (via inheritance of
  // QuicTimeWaitListManager::Visitor):
  // Ensure that the closed connection is cleaned up asynchronously.
  void OnConnectionClosed(QuicConnectionId connection_id,
                          QuicErrorCode error,
                          const QuicString& error_details) override;

  // QuicSession::Visitor interface implementation (via inheritance of
  // QuicTimeWaitListManager::Visitor):
  // Queues the blocked writer for later resumption.
  void OnWriteBlocked(QuicBlockedWriterInterface* blocked_writer) override;

  // QuicSession::Visitor interface implementation (via inheritance of
  // QuicTimeWaitListManager::Visitor):
  // Collects reset error code received on streams.
  void OnRstStreamReceived(const QuicRstStreamFrame& frame) override;

  // QuicTimeWaitListManager::Visitor interface implementation
  // Called whenever the time wait list manager adds a new connection to the
  // time-wait list.
  void OnConnectionAddedToTimeWaitList(QuicConnectionId connection_id) override;

  using SessionMap =
      QuicUnorderedMap<QuicConnectionId, std::unique_ptr<QuicSession>>;

  const SessionMap& session_map() const { return session_map_; }

  // Deletes all sessions on the closed session list and clears the list.
  virtual void DeleteSessions();

  // The largest packet number we expect to receive with a connection
  // ID for a connection that is not established yet.  The current design will
  // send a handshake and then up to 50 or so data packets, and then it may
  // resend the handshake packet up to 10 times.  (Retransmitted packets are
  // sent with unique packet numbers.)
  static const QuicPacketNumber kMaxReasonableInitialPacketNumber = 100;
  static_assert(kMaxReasonableInitialPacketNumber >=
                    kInitialCongestionWindow + 10,
                "kMaxReasonableInitialPacketNumber is unreasonably small "
                "relative to kInitialCongestionWindow.");

  // QuicFramerVisitorInterface implementation. Not expected to be called
  // outside of this class.
  void OnPacket() override;
  // Called when the public header has been parsed.
  bool OnUnauthenticatedPublicHeader(const QuicPacketHeader& header) override;
  // Called when the private header has been parsed of a data packet that is
  // destined for the time wait manager.
  bool OnUnauthenticatedHeader(const QuicPacketHeader& header) override;
  void OnError(QuicFramer* framer) override;
  bool OnProtocolVersionMismatch(ParsedQuicVersion received_version) override;

  // The following methods should never get called because
  // OnUnauthenticatedPublicHeader() or OnUnauthenticatedHeader() (whichever
  // was called last), will return false and prevent a subsequent invocation
  // of these methods. Thus, the payload of the packet is never processed in
  // the dispatcher.
  void OnPublicResetPacket(const QuicPublicResetPacket& packet) override;
  void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& packet) override;
  void OnDecryptedPacket(EncryptionLevel level) override;
  bool OnPacketHeader(const QuicPacketHeader& header) override;
  bool OnStreamFrame(const QuicStreamFrame& frame) override;
  bool OnCryptoFrame(const QuicCryptoFrame& frame) override;
  bool OnAckFrameStart(QuicPacketNumber largest_acked,
                       QuicTime::Delta ack_delay_time) override;
  bool OnAckRange(QuicPacketNumber start, QuicPacketNumber end) override;
  bool OnAckTimestamp(QuicPacketNumber packet_number,
                      QuicTime timestamp) override;
  bool OnAckFrameEnd(QuicPacketNumber start) override;
  bool OnStopWaitingFrame(const QuicStopWaitingFrame& frame) override;
  bool OnPaddingFrame(const QuicPaddingFrame& frame) override;
  bool OnPingFrame(const QuicPingFrame& frame) override;
  bool OnRstStreamFrame(const QuicRstStreamFrame& frame) override;
  bool OnConnectionCloseFrame(const QuicConnectionCloseFrame& frame) override;
  bool OnApplicationCloseFrame(const QuicApplicationCloseFrame& frame) override;
  bool OnStopSendingFrame(const QuicStopSendingFrame& frame) override;
  bool OnPathChallengeFrame(const QuicPathChallengeFrame& frame) override;
  bool OnPathResponseFrame(const QuicPathResponseFrame& frame) override;
  bool OnGoAwayFrame(const QuicGoAwayFrame& frame) override;
  bool OnMaxStreamIdFrame(const QuicMaxStreamIdFrame& frame) override;
  bool OnStreamIdBlockedFrame(const QuicStreamIdBlockedFrame& frame) override;
  bool OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame) override;
  bool OnBlockedFrame(const QuicBlockedFrame& frame) override;
  bool OnNewConnectionIdFrame(const QuicNewConnectionIdFrame& frame) override;
  bool OnRetireConnectionIdFrame(
      const QuicRetireConnectionIdFrame& frame) override;
  bool OnNewTokenFrame(const QuicNewTokenFrame& frame) override;
  bool OnMessageFrame(const QuicMessageFrame& frame) override;
  void OnPacketComplete() override;
  bool IsValidStatelessResetToken(QuicUint128 token) const override;
  void OnAuthenticatedIetfStatelessResetPacket(
      const QuicIetfStatelessResetPacket& packet) override;

  // QuicBufferedPacketStore::VisitorInterface implementation.
  void OnExpiredPackets(QuicConnectionId connection_id,
                        QuicBufferedPacketStore::BufferedPacketList
                            early_arrived_packets) override;

  // Create connections for previously buffered CHLOs as many as allowed.
  virtual void ProcessBufferedChlos(size_t max_connections_to_create);

  // Return true if there is CHLO buffered.
  virtual bool HasChlosBuffered() const;

  // Used by subclass of QuicDispatcher, to track its per packet context.
  struct PerPacketContext {
    virtual ~PerPacketContext() {}
  };

 protected:
  virtual QuicSession* CreateQuicSession(QuicConnectionId connection_id,
                                         const QuicSocketAddress& peer_address,
                                         QuicStringPiece alpn) = 0;

  // Called when a connection is rejected statelessly.
  virtual void OnConnectionRejectedStatelessly();

  // Called when a connection is closed statelessly.
  virtual void OnConnectionClosedStatelessly(QuicErrorCode error);

  // Returns true if cheap stateless rejection should be attempted.
  virtual bool ShouldAttemptCheapStatelessRejection();

  // Values to be returned by ValidityChecks() to indicate what should be done
  // with a packet.  Fates with greater values are considered to be higher
  // priority, in that if one validity check indicates a lower-valued fate and
  // another validity check indicates a higher-valued fate, the higher-valued
  // fate should be obeyed.
  enum QuicPacketFate {
    // Process the packet normally, which is usually to establish a connection.
    kFateProcess,
    // Put the connection ID into time-wait state and send a public reset.
    kFateTimeWait,
    // Buffer the packet.
    kFateBuffer,
    // Drop the packet (ignore and give no response).
    kFateDrop,
  };

  // This method is called by OnUnauthenticatedHeader on packets not associated
  // with a known connection ID.  It applies validity checks and returns a
  // QuicPacketFate to tell what should be done with the packet.
  virtual QuicPacketFate ValidityChecks(const QuicPacketHeader& header);

  // Create and return the time wait list manager for this dispatcher, which
  // will be owned by the dispatcher as time_wait_list_manager_
  virtual QuicTimeWaitListManager* CreateQuicTimeWaitListManager();

  // Called when |connection_id| doesn't have an open connection yet, to buffer
  // |current_packet_| until it can be delivered to the connection.
  void BufferEarlyPacket(QuicConnectionId connection_id, bool ietf_quic);

  // Called when |current_packet_| is a CHLO packet. Creates a new connection
  // and delivers any buffered packets for that connection id.
  void ProcessChlo();

  // Returns the actual client address of the current packet.
  // This function should only be called once per packet at the very beginning
  // of ProcessPacket(), its result is saved to |current_client_address_|, which
  // is guaranteed to be valid even in the stateless rejector's callback(i.e.
  // OnStatelessRejectorProcessDone).
  // By default, this function returns |current_peer_address_|, subclasses have
  // the option to override this function to return a different address.
  virtual const QuicSocketAddress GetClientAddress() const;

  // Return true if dispatcher wants to destroy session outside of
  // OnConnectionClosed() call stack.
  virtual bool ShouldDestroySessionAsynchronously();

  QuicTimeWaitListManager* time_wait_list_manager() {
    return time_wait_list_manager_.get();
  }

  const QuicTransportVersionVector& GetSupportedTransportVersions();

  const ParsedQuicVersionVector& GetSupportedVersions();

  QuicConnectionId current_connection_id() const {
    return current_connection_id_;
  }
  const QuicSocketAddress& current_self_address() const {
    return current_self_address_;
  }
  const QuicSocketAddress& current_peer_address() const {
    return current_peer_address_;
  }
  const QuicSocketAddress& current_client_address() const {
    return current_client_address_;
  }
  const QuicReceivedPacket& current_packet() const { return *current_packet_; }

  const QuicConfig& config() const { return config_; }

  const QuicCryptoServerConfig* crypto_config() const { return crypto_config_; }

  QuicCompressedCertsCache* compressed_certs_cache() {
    return &compressed_certs_cache_;
  }

  QuicFramer* framer() { return &framer_; }

  QuicConnectionHelperInterface* helper() { return helper_.get(); }

  QuicCryptoServerStream::Helper* session_helper() {
    return session_helper_.get();
  }

  QuicAlarmFactory* alarm_factory() { return alarm_factory_.get(); }

  QuicPacketWriter* writer() { return writer_.get(); }

  // Returns true if a session should be created for a connection with an
  // unknown version identified by |version_label|.
  virtual bool ShouldCreateSessionForUnknownVersion(
      QuicVersionLabel version_label);

  void SetLastError(QuicErrorCode error);

  // Called when the public header has been parsed and the session has been
  // looked up, and the session was not found in the active list of sessions.
  // Returns false if processing should stop after this call.
  virtual bool OnUnauthenticatedUnknownPublicHeader(
      const QuicPacketHeader& header);

  // Called when a new connection starts to be handled by this dispatcher.
  // Either this connection is created or its packets is buffered while waiting
  // for CHLO. Returns true if a new connection should be created or its packets
  // should be buffered, false otherwise.
  virtual bool ShouldCreateOrBufferPacketForConnection(
      QuicConnectionId connection_id);

  bool HasBufferedPackets(QuicConnectionId connection_id);

  // Called when BufferEarlyPacket() fail to buffer the packet.
  virtual void OnBufferPacketFailure(
      QuicBufferedPacketStore::EnqueuePacketResult result,
      QuicConnectionId connection_id);

  // Removes the session from the session map and write blocked list, and adds
  // the ConnectionId to the time-wait list.  If |session_closed_statelessly| is
  // true, any future packets for the ConnectionId will be black-holed.
  virtual void CleanUpSession(SessionMap::iterator it,
                              QuicConnection* connection,
                              bool session_closed_statelessly);

  void StopAcceptingNewConnections();

  // Return true if the blocked writer should be added to blocked list.
  virtual bool ShouldAddToBlockedList();

  // Called to terminate a connection statelessly. Depending on |format|, either
  // 1) send connection close with |error_code| and |error_details| and add
  // connection to time wait list or 2) directly add connection to time wait
  // list with |action|.
  void StatelesslyTerminateConnection(
      QuicConnectionId connection_id,
      PacketHeaderFormat format,
      QuicErrorCode error_code,
      const QuicString& error_details,
      QuicTimeWaitListManager::TimeWaitAction action);

  // Save/Restore per packet context. Used by async stateless rejector.
  virtual std::unique_ptr<PerPacketContext> GetPerPacketContext() const;
  virtual void RestorePerPacketContext(
      std::unique_ptr<PerPacketContext> /*context*/) {}

 private:
  friend class test::QuicDispatcherPeer;
  friend class StatelessRejectorProcessDoneCallback;

  typedef QuicUnorderedSet<QuicConnectionId> QuicConnectionIdSet;

  // Attempts to reject the connection statelessly, if stateless rejects are
  // possible and if the current packet contains a CHLO message.  Determines a
  // fate which describes what subsequent processing should be performed on the
  // packets, like ValidityChecks, and invokes ProcessUnauthenticatedHeaderFate.
  void MaybeRejectStatelessly(QuicConnectionId connection_id,
                              ParsedQuicVersion version);

  // Deliver |packets| to |session| for further processing.
  void DeliverPacketsToSession(
      const std::list<QuicBufferedPacketStore::BufferedPacket>& packets,
      QuicSession* session);

  // Perform the appropriate actions on the current packet based on |fate| -
  // either process, buffer, or drop it.
  void ProcessUnauthenticatedHeaderFate(QuicPacketFate fate,
                                        QuicConnectionId connection_id);

  // Invoked when StatelessRejector::Process completes. |first_version| is the
  // version of the packet which initiated the stateless reject.
  // WARNING: This function can be called when a async proof returns, i.e. not
  // from a stack traceable to ProcessPacket().
  // TODO(fayang): maybe consider not using callback when there is no crypto
  // involved.
  void OnStatelessRejectorProcessDone(
      std::unique_ptr<StatelessRejector> rejector,
      const QuicSocketAddress& current_client_address,
      const QuicSocketAddress& current_peer_address,
      const QuicSocketAddress& current_self_address,
      std::unique_ptr<QuicReceivedPacket> current_packet,
      ParsedQuicVersion first_version,
      bool current_packet_is_ietf_quic);

  // Examine the state of the rejector and decide what to do with the current
  // packet.
  void ProcessStatelessRejectorState(
      std::unique_ptr<StatelessRejector> rejector,
      QuicTransportVersion first_version);

  void set_new_sessions_allowed_per_event_loop(
      int16_t new_sessions_allowed_per_event_loop) {
    new_sessions_allowed_per_event_loop_ = new_sessions_allowed_per_event_loop;
  }

  const QuicConfig& config_;

  const QuicCryptoServerConfig* crypto_config_;

  // The cache for most recently compressed certs.
  QuicCompressedCertsCache compressed_certs_cache_;

  // The list of connections waiting to write.
  WriteBlockedList write_blocked_list_;

  SessionMap session_map_;

  // Entity that manages connection_ids in time wait state.
  std::unique_ptr<QuicTimeWaitListManager> time_wait_list_manager_;

  // The list of closed but not-yet-deleted sessions.
  std::vector<std::unique_ptr<QuicSession>> closed_session_list_;

  // The helper used for all connections.
  std::unique_ptr<QuicConnectionHelperInterface> helper_;

  // The helper used for all sessions.
  std::unique_ptr<QuicCryptoServerStream::Helper> session_helper_;

  // Creates alarms.
  std::unique_ptr<QuicAlarmFactory> alarm_factory_;

  // An alarm which deletes closed sessions.
  std::unique_ptr<QuicAlarm> delete_sessions_alarm_;

  // The writer to write to the socket with.
  std::unique_ptr<QuicPacketWriter> writer_;

  // Packets which are buffered until a connection can be created to handle
  // them.
  QuicBufferedPacketStore buffered_packets_;

  // Set of connection IDs for which asynchronous CHLO processing is in
  // progress, making it necessary to buffer any other packets which arrive on
  // that connection until CHLO processing is complete.
  QuicConnectionIdSet temporarily_buffered_connections_;

  // Information about the packet currently being handled.

  // Used for stateless rejector to generate and validate source address token.
  QuicSocketAddress current_client_address_;
  QuicSocketAddress current_peer_address_;
  QuicSocketAddress current_self_address_;
  const QuicReceivedPacket* current_packet_;
  // If |current_packet_| is a CHLO packet, the extracted alpn.
  QuicString current_alpn_;
  QuicConnectionId current_connection_id_;

  // Used to get the supported versions based on flag. Does not own.
  QuicVersionManager* version_manager_;

  QuicFramer framer_;

  // The last error set by SetLastError(), which is called by
  // framer_visitor_->OnError().
  QuicErrorCode last_error_;

  // A backward counter of how many new sessions can be create within current
  // event loop. When reaches 0, it means can't create sessions for now.
  int16_t new_sessions_allowed_per_event_loop_;

  // True if this dispatcher is not draining.
  bool accept_new_connections_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_DISPATCHER_H_
