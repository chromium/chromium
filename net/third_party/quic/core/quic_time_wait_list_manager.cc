// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_time_wait_list_manager.h"

#include <errno.h>

#include <memory>

#include "base/macros.h"
#include "net/third_party/quic/core/crypto/crypto_protocol.h"
#include "net/third_party/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quic/core/quic_framer.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/platform/api/quic_clock.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_map_util.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_socket_address.h"

namespace quic {

// A very simple alarm that just informs the QuicTimeWaitListManager to clean
// up old connection_ids. This alarm should be cancelled and deleted before
// the QuicTimeWaitListManager is deleted.
class ConnectionIdCleanUpAlarm : public QuicAlarm::Delegate {
 public:
  explicit ConnectionIdCleanUpAlarm(
      QuicTimeWaitListManager* time_wait_list_manager)
      : time_wait_list_manager_(time_wait_list_manager) {}
  ConnectionIdCleanUpAlarm(const ConnectionIdCleanUpAlarm&) = delete;
  ConnectionIdCleanUpAlarm& operator=(const ConnectionIdCleanUpAlarm&) = delete;

  void OnAlarm() override {
    time_wait_list_manager_->CleanUpOldConnectionIds();
  }

 private:
  // Not owned.
  QuicTimeWaitListManager* time_wait_list_manager_;
};

// This class stores pending public reset packets to be sent to clients.
// server_address - server address on which a packet what was received for
//                  a connection_id in time wait state.
// client_address - address of the client that sent that packet. Needed to send
//                  the public reset packet back to the client.
// packet - the pending public reset packet that is to be sent to the client.
//          created instance takes the ownership of this packet.
class QuicTimeWaitListManager::QueuedPacket {
 public:
  QueuedPacket(const QuicSocketAddress& server_address,
               const QuicSocketAddress& client_address,
               std::unique_ptr<QuicEncryptedPacket> packet)
      : server_address_(server_address),
        client_address_(client_address),
        packet_(std::move(packet)) {}
  QueuedPacket(const QueuedPacket&) = delete;
  QueuedPacket& operator=(const QueuedPacket&) = delete;

  const QuicSocketAddress& server_address() const { return server_address_; }
  const QuicSocketAddress& client_address() const { return client_address_; }
  QuicEncryptedPacket* packet() { return packet_.get(); }

 private:
  const QuicSocketAddress server_address_;
  const QuicSocketAddress client_address_;
  std::unique_ptr<QuicEncryptedPacket> packet_;
};

QuicTimeWaitListManager::QuicTimeWaitListManager(
    QuicPacketWriter* writer,
    Visitor* visitor,
    const QuicClock* clock,
    QuicAlarmFactory* alarm_factory)
    : time_wait_period_(
          QuicTime::Delta::FromSeconds(FLAGS_quic_time_wait_list_seconds)),
      connection_id_clean_up_alarm_(
          alarm_factory->CreateAlarm(new ConnectionIdCleanUpAlarm(this))),
      clock_(clock),
      writer_(writer),
      visitor_(visitor) {
  SetConnectionIdCleanUpAlarm();
}

QuicTimeWaitListManager::~QuicTimeWaitListManager() {
  connection_id_clean_up_alarm_->Cancel();
}

void QuicTimeWaitListManager::AddConnectionIdToTimeWait(
    QuicConnectionId connection_id,
    bool ietf_quic,
    TimeWaitAction action,
    std::vector<std::unique_ptr<QuicEncryptedPacket>>* termination_packets) {
  DCHECK(action != SEND_TERMINATION_PACKETS || termination_packets != nullptr);
  DCHECK(action != DO_NOTHING || ietf_quic);
  int num_packets = 0;
  auto it = connection_id_map_.find(connection_id);
  const bool new_connection_id = it == connection_id_map_.end();
  if (!new_connection_id) {  // Replace record if it is reinserted.
    num_packets = it->second.num_packets;
    connection_id_map_.erase(it);
  }
  TrimTimeWaitListIfNeeded();
  DCHECK_LT(num_connections(),
            static_cast<size_t>(FLAGS_quic_time_wait_list_max_connections));
  ConnectionIdData data(num_packets, ietf_quic, clock_->ApproximateNow(),
                        action);
  if (termination_packets != nullptr) {
    data.termination_packets.swap(*termination_packets);
  }
  connection_id_map_.emplace(std::make_pair(connection_id, std::move(data)));
  if (new_connection_id) {
    visitor_->OnConnectionAddedToTimeWaitList(connection_id);
  }
}

bool QuicTimeWaitListManager::IsConnectionIdInTimeWait(
    QuicConnectionId connection_id) const {
  return QuicContainsKey(connection_id_map_, connection_id);
}

void QuicTimeWaitListManager::OnBlockedWriterCanWrite() {
  while (!pending_packets_queue_.empty()) {
    QueuedPacket* queued_packet = pending_packets_queue_.front().get();
    if (!WriteToWire(queued_packet)) {
      return;
    }
    pending_packets_queue_.pop_front();
  }
}

void QuicTimeWaitListManager::ProcessPacket(
    const QuicSocketAddress& server_address,
    const QuicSocketAddress& client_address,
    QuicConnectionId connection_id) {
  DCHECK(IsConnectionIdInTimeWait(connection_id));
  QUIC_DLOG(INFO) << "Processing " << connection_id << " in time wait state.";
  // TODO(satyamshekhar): Think about handling packets from different client
  // addresses.
  auto it = connection_id_map_.find(connection_id);
  DCHECK(it != connection_id_map_.end());
  // Increment the received packet count.
  ConnectionIdData* connection_data = &it->second;
  ++(connection_data->num_packets);

  if (!ShouldSendResponse(connection_data->num_packets)) {
    return;
  }

  switch (connection_data->action) {
    case SEND_TERMINATION_PACKETS:
      if (connection_data->termination_packets.empty()) {
        QUIC_BUG << "There are no termination packets.";
        return;
      }
      for (const auto& packet : connection_data->termination_packets) {
        SendOrQueuePacket(QuicMakeUnique<QueuedPacket>(
            server_address, client_address, packet->Clone()));
      }
      return;
    case SEND_STATELESS_RESET:
      SendPublicReset(server_address, client_address, connection_id,
                      connection_data->ietf_quic);
      return;
    case DO_NOTHING:
      DCHECK(connection_data->ietf_quic);
  }
}

void QuicTimeWaitListManager::SendVersionNegotiationPacket(
    QuicConnectionId connection_id,
    bool ietf_quic,
    const ParsedQuicVersionVector& supported_versions,
    const QuicSocketAddress& server_address,
    const QuicSocketAddress& client_address) {
  SendOrQueuePacket(QuicMakeUnique<QueuedPacket>(
      server_address, client_address,
      QuicFramer::BuildVersionNegotiationPacket(connection_id, ietf_quic,
                                                supported_versions)));
}

// Returns true if the number of packets received for this connection_id is a
// power of 2 to throttle the number of public reset packets we send to a
// client.
bool QuicTimeWaitListManager::ShouldSendResponse(int received_packet_count) {
  return (received_packet_count & (received_packet_count - 1)) == 0;
}

void QuicTimeWaitListManager::SendPublicReset(
    const QuicSocketAddress& server_address,
    const QuicSocketAddress& client_address,
    QuicConnectionId connection_id,
    bool ietf_quic) {
  if (ietf_quic) {
    SendOrQueuePacket(QuicMakeUnique<QueuedPacket>(
        server_address, client_address,
        BuildIetfStatelessResetPacket(connection_id)));
    return;
  }
  QuicPublicResetPacket packet;
  packet.connection_id = connection_id;
  // TODO(satyamshekhar): generate a valid nonce for this connection_id.
  packet.nonce_proof = 1010101;
  packet.client_address = client_address;
  // Takes ownership of the packet.
  SendOrQueuePacket(QuicMakeUnique<QueuedPacket>(server_address, client_address,
                                                 BuildPublicReset(packet)));
}

std::unique_ptr<QuicEncryptedPacket> QuicTimeWaitListManager::BuildPublicReset(
    const QuicPublicResetPacket& packet) {
  return QuicFramer::BuildPublicResetPacket(packet);
}

std::unique_ptr<QuicEncryptedPacket>
QuicTimeWaitListManager::BuildIetfStatelessResetPacket(
    QuicConnectionId connection_id) {
  return QuicFramer::BuildIetfStatelessResetPacket(
      connection_id, GetStatelessResetToken(connection_id));
}

// Either sends the packet and deletes it or makes pending queue the
// owner of the packet.
void QuicTimeWaitListManager::SendOrQueuePacket(
    std::unique_ptr<QueuedPacket> packet) {
  if (WriteToWire(packet.get())) {
    // Allow the packet to be deleted upon leaving this function.
    return;
  }
  pending_packets_queue_.push_back(std::move(packet));
}

bool QuicTimeWaitListManager::WriteToWire(QueuedPacket* queued_packet) {
  if (writer_->IsWriteBlocked()) {
    visitor_->OnWriteBlocked(this);
    return false;
  }
  WriteResult result = writer_->WritePacket(
      queued_packet->packet()->data(), queued_packet->packet()->length(),
      queued_packet->server_address().host(), queued_packet->client_address(),
      nullptr);

  // If using a batch writer and the packet is buffered, flush it.
  if (writer_->IsBatchMode() && result.status == WRITE_STATUS_OK &&
      result.bytes_written == 0) {
    result = writer_->Flush();
  }

  if (result.status == WRITE_STATUS_BLOCKED) {
    // If blocked and unbuffered, return false to retry sending.
    DCHECK(writer_->IsWriteBlocked());
    visitor_->OnWriteBlocked(this);
    return writer_->IsWriteBlockedDataBuffered();
  } else if (IsWriteError(result.status)) {
    QUIC_LOG_FIRST_N(WARNING, 1)
        << "Received unknown error while sending reset packet to "
        << queued_packet->client_address().ToString() << ": "
        << strerror(result.error_code);
  }
  return true;
}

void QuicTimeWaitListManager::SetConnectionIdCleanUpAlarm() {
  QuicTime::Delta next_alarm_interval = QuicTime::Delta::Zero();
  if (!connection_id_map_.empty()) {
    QuicTime oldest_connection_id =
        connection_id_map_.begin()->second.time_added;
    QuicTime now = clock_->ApproximateNow();
    if (now - oldest_connection_id < time_wait_period_) {
      next_alarm_interval = oldest_connection_id + time_wait_period_ - now;
    } else {
      QUIC_LOG(ERROR)
          << "ConnectionId lingered for longer than time_wait_period_";
    }
  } else {
    // No connection_ids added so none will expire before time_wait_period_.
    next_alarm_interval = time_wait_period_;
  }

  connection_id_clean_up_alarm_->Update(
      clock_->ApproximateNow() + next_alarm_interval, QuicTime::Delta::Zero());
}

bool QuicTimeWaitListManager::MaybeExpireOldestConnection(
    QuicTime expiration_time) {
  if (connection_id_map_.empty()) {
    return false;
  }
  auto it = connection_id_map_.begin();
  QuicTime oldest_connection_id_time = it->second.time_added;
  if (oldest_connection_id_time > expiration_time) {
    // Too recent, don't retire.
    return false;
  }
  // This connection_id has lived its age, retire it now.
  connection_id_map_.erase(it);
  return true;
}

void QuicTimeWaitListManager::CleanUpOldConnectionIds() {
  QuicTime now = clock_->ApproximateNow();
  QuicTime expiration = now - time_wait_period_;

  while (MaybeExpireOldestConnection(expiration)) {
  }

  SetConnectionIdCleanUpAlarm();
}

void QuicTimeWaitListManager::TrimTimeWaitListIfNeeded() {
  if (FLAGS_quic_time_wait_list_max_connections < 0) {
    return;
  }
  while (num_connections() >=
         static_cast<size_t>(FLAGS_quic_time_wait_list_max_connections)) {
    MaybeExpireOldestConnection(QuicTime::Infinite());
  }
}

QuicTimeWaitListManager::ConnectionIdData::ConnectionIdData(
    int num_packets,
    bool ietf_quic,
    QuicTime time_added,
    TimeWaitAction action)
    : num_packets(num_packets),
      ietf_quic(ietf_quic),
      time_added(time_added),
      action(action) {}

QuicTimeWaitListManager::ConnectionIdData::ConnectionIdData(
    ConnectionIdData&& other) = default;

QuicTimeWaitListManager::ConnectionIdData::~ConnectionIdData() = default;

QuicUint128 QuicTimeWaitListManager::GetStatelessResetToken(
    QuicConnectionId connection_id) const {
  return connection_id;
}

}  // namespace quic
