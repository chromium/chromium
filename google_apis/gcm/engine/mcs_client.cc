// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/mcs_client.h"

#include <stddef.h>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/base/socket_stream.h"
#include "google_apis/gcm/engine/connection_factory.h"
#include "google_apis/gcm/monitoring/gcm_stats_recorder.h"

namespace gcm {

namespace {

typedef std::unique_ptr<google::protobuf::MessageLite> MCSProto;

// The category of messages intended for the GCM client itself from MCS.
const char kMCSCategory[] = "com.google.android.gsf.gtalkservice";

// The from field for messages originating in the GCM client.
const char kGCMFromField[] = "gcm@android.com";

// MCS status message types.
// TODO(zea): handle these at the GCMClient layer.
const char kIdleNotification[] = "IdleNotification";
// const char kAlwaysShowOnIdle[] = "ShowAwayOnIdle";
// const char kPowerNotification[] = "PowerNotification";
// const char kDataActiveNotification[] = "DataActiveNotification";

// Settings for MCS Login packet.
const char kHeartbeatIntervalSettingName[] = "hbping";
const int kNoCustomHeartbeat = 0;

// The number of unacked messages to allow before sending a stream ack.
// Applies to both incoming and outgoing messages.
// TODO(zea): make this server configurable.
const int kUnackedMessageBeforeStreamAck = 10;

// The global maximum number of pending messages to have in the send queue.
const size_t kMaxSendQueueSize = 10 * 1024;

// The maximum message size that can be sent to the server.
const int kMaxMessageBytes = 4 * 1024;  // 4KB, like the server.

// Helper for converting a proto persistent id list to a vector of strings.
bool BuildPersistentIdListFromProto(const google::protobuf::string& bytes,
                                    std::vector<std::string>* id_list) {
  mcs_proto::SelectiveAck selective_ack;
  if (!selective_ack.ParseFromString(bytes))
    return false;
  std::vector<std::string> new_list;
  for (int i = 0; i < selective_ack.id_size(); ++i) {
    DCHECK(!selective_ack.id(i).empty());
    new_list.push_back(selective_ack.id(i));
  }
  id_list->swap(new_list);
  return true;
}

}  // namespace

class CollapseKey {
 public:
  explicit CollapseKey(const mcs_proto::DataMessageStanza& message);
  ~CollapseKey();

  // Comparison operator for use in maps.
  bool operator<(const CollapseKey& right) const;

  // Whether the message had a valid collapse key.
  bool IsValid() const;

  std::string token() const { return token_; }
  std::string app_id() const { return app_id_; }
  int64_t device_user_id() const { return device_user_id_; }

 private:
  const std::string token_;
  const std::string app_id_;
  const int64_t device_user_id_;
};

CollapseKey::CollapseKey(const mcs_proto::DataMessageStanza& message)
    : token_(message.token()),
      app_id_(message.category()),
      device_user_id_(message.device_user_id()) {}

CollapseKey::~CollapseKey() {}

bool CollapseKey::IsValid() const {
  // Device user id is optional, but the application id and token are not.
  return !token_.empty() && !app_id_.empty();
}

bool CollapseKey::operator<(const CollapseKey& right) const {
  if (device_user_id_ != right.device_user_id())
    return device_user_id_ < right.device_user_id();
  if (app_id_ != right.app_id())
    return app_id_ < right.app_id();
  return token_ < right.token();
}

struct ReliablePacketInfo {
  ReliablePacketInfo();
  ~ReliablePacketInfo();

  // The stream id with which the message was sent.
  uint32_t stream_id;

  // If reliable delivery was requested, the persistent id of the message.
  std::string persistent_id;

  // The type of message itself (for easier lookup).
  uint8_t tag;

  // The protobuf of the message itself.
  MCSProto protobuf;
};

ReliablePacketInfo::ReliablePacketInfo()
  : stream_id(0), tag(0) {
}
ReliablePacketInfo::~ReliablePacketInfo() {}

int MCSClient::GetSendQueueSize() const {
  return to_send_.size();
}

int MCSClient::GetResendQueueSize() const {
  return to_resend_.size();
}

std::string MCSClient::GetStateString() const {
  switch(state_) {
    case UNINITIALIZED:
      return "UNINITIALIZED";
    case LOADED:
      return "LOADED";
    case CONNECTING:
      return "CONNECTING";
    case CONNECTED:
      return "CONNECTED";
  }
  NOTREACHED();
  return std::string();
}

MCSClient::MCSClient(const std::string& version_string,
                     base::Clock* clock,
                     ConnectionFactory* connection_factory,
                     GCMStore* gcm_store,
                     scoped_refptr<base::SequencedTaskRunner> io_task_runner,
                     GCMStatsRecorder* recorder)
    : version_string_(version_string),
      clock_(clock),
      state_(UNINITIALIZED),
      android_id_(0),
      security_token_(0),
      connection_factory_(connection_factory),
      connection_handler_(nullptr),
      last_device_to_server_stream_id_received_(0),
      last_server_to_device_stream_id_received_(0),
      stream_id_out_(0),
      stream_id_in_(0),
      gcm_store_(gcm_store),
      io_task_runner_(io_task_runner),
      heartbeat_manager_(std::move(base::ThreadTaskRunnerHandle::Get()),
                         std::move(io_task_runner)),
      recorder_(recorder) {
  DCHECK(io_task_runner_);
}

MCSClient::~MCSClient() {
}

void MCSClient::Initialize(
    const ErrorCallback& error_callback,
    const OnMessageReceivedCallback& message_received_callback,
    const OnMessageSentCallback& message_sent_callback,
    std::unique_ptr<GCMStore::LoadResult> load_result) {
  DCHECK_EQ(state_, UNINITIALIZED);

  state_ = LOADED;
  mcs_error_callback_ = error_callback;
  message_received_callback_ = message_received_callback;
  message_sent_callback_ = message_sent_callback;

  connection_factory_->Initialize(
      base::BindRepeating(&MCSClient::ResetStateAndBuildLoginRequest,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&MCSClient::HandlePacketFromWire,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&MCSClient::MaybeSendMessage,
                          weak_ptr_factory_.GetWeakPtr()));

  stream_id_out_ = 1;  // Login request is hardcoded to id 1.

  android_id_ = load_result->device_android_id;
  security_token_ = load_result->device_security_token;

  if (android_id_ == 0) {
    DVLOG(1) << "No device credentials found, assuming new client.";
    // No need to try and load RMQ data in that case.
    return;
  }

  // |android_id_| is non-zero, so should |security_token_|.
  DCHECK_NE(0u, security_token_) << "Security token invalid, while android id"
                                 << " is non-zero.";

  DVLOG(1) << "RMQ Load finished with " << load_result->incoming_messages.size()
           << " incoming acks pending and "
           << load_result->outgoing_messages.size()
           << " outgoing messages pending.";

  restored_unackeds_server_ids_ = load_result->incoming_messages;

  // First go through and order the outgoing messages by recency.
  std::map<uint64_t, google::protobuf::MessageLite*> ordered_messages;
  std::vector<PersistentId> expired_ttl_ids;
  for (GCMStore::OutgoingMessageMap::iterator iter =
           load_result->outgoing_messages.begin();
       iter != load_result->outgoing_messages.end(); ++iter) {
    uint64_t timestamp = 0;
    if (!base::StringToUint64(iter->first, &timestamp)) {
      LOG(ERROR) << "Invalid restored message.";
      // TODO(fgorski): Error: data unreadable
      mcs_error_callback_.Run();
      return;
    }

    // Check if the TTL has expired for this message.
    if (HasTTLExpired(*iter->second, clock_)) {
      expired_ttl_ids.push_back(iter->first);
      NotifyMessageSendStatus(*iter->second, TTL_EXCEEDED);
      continue;
    }

    ordered_messages[timestamp] = iter->second.release();
  }

  if (!expired_ttl_ids.empty()) {
    gcm_store_->RemoveOutgoingMessages(
        expired_ttl_ids, base::BindOnce(&MCSClient::OnGCMUpdateFinished,
                                        weak_ptr_factory_.GetWeakPtr()));
  }

  // Now go through and add the outgoing messages to the send queue in their
  // appropriate order (oldest at front, most recent at back).
  for (std::map<uint64_t, google::protobuf::MessageLite*>::iterator iter =
           ordered_messages.begin();
       iter != ordered_messages.end(); ++iter) {
    auto packet_info = std::make_unique<ReliablePacketInfo>();
    auto* packet_info_ptr = packet_info.get();
    packet_info->protobuf.reset(iter->second);
    packet_info->tag = GetMCSProtoTag(*iter->second);
    packet_info->persistent_id = base::NumberToString(iter->first);
    to_send_.push_back(std::move(packet_info));

    if (packet_info_ptr->tag == kDataMessageStanzaTag) {
      mcs_proto::DataMessageStanza* data_message =
          reinterpret_cast<mcs_proto::DataMessageStanza*>(
              packet_info_ptr->protobuf.get());
      CollapseKey collapse_key(*data_message);
      if (collapse_key.IsValid())
        collapse_key_map_[collapse_key] = packet_info_ptr;
    }
  }

  // Establish if there is any custom client interval persisted from the last
  // run and set it on the heartbeat manager.
  custom_heartbeat_intervals_.swap(load_result->heartbeat_intervals);
  int min_interval_ms = GetMinHeartbeatIntervalMs();
  heartbeat_manager_.SetClientHeartbeatIntervalMs(min_interval_ms);
}

void MCSClient::Login(uint64_t android_id, uint64_t security_token) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(state_, LOADED);
  DCHECK(android_id_ == 0 || android_id_ == android_id);
  DCHECK(security_token_ == 0 || security_token_ == security_token);

  if (android_id != android_id_ && security_token != security_token_) {
    DCHECK(android_id);
    DCHECK(security_token);
    android_id_ = android_id;
    security_token_ = security_token;
  }

  DCHECK(android_id_ != 0 || restored_unackeds_server_ids_.empty());

  state_ = CONNECTING;
  connection_factory_->Connect();
  connection_handler_ = connection_factory_->GetConnectionHandler();
}

void MCSClient::SendMessage(const MCSMessage& message) {
  int ttl = GetTTL(message.GetProtobuf());
  DCHECK_GE(ttl, 0);
  if (to_send_.size() > kMaxSendQueueSize) {
    NotifyMessageSendStatus(message.GetProtobuf(), QUEUE_SIZE_LIMIT_REACHED);
    return;
  }
  if (message.size() > kMaxMessageBytes) {
    NotifyMessageSendStatus(message.GetProtobuf(), MESSAGE_TOO_LARGE);
    return;
  }

  std::unique_ptr<ReliablePacketInfo> packet_info(new ReliablePacketInfo());
  packet_info->tag = message.tag();
  packet_info->protobuf = message.CloneProtobuf();

  if (ttl > 0) {
    DCHECK_EQ(message.tag(), kDataMessageStanzaTag);

    // First check if this message should replace a pending message with the
    // same collapse key.
    mcs_proto::DataMessageStanza* data_message =
        reinterpret_cast<mcs_proto::DataMessageStanza*>(
            packet_info->protobuf.get());
    CollapseKey collapse_key(*data_message);
    if (collapse_key.IsValid() && collapse_key_map_.count(collapse_key) > 0) {
      ReliablePacketInfo* original_packet = collapse_key_map_[collapse_key];
      DVLOG(1) << "Found matching collapse key, Reusing persistent id of "
               << original_packet->persistent_id;
      original_packet->protobuf = std::move(packet_info->protobuf);
      SetPersistentId(original_packet->persistent_id,
                      original_packet->protobuf.get());
      gcm_store_->OverwriteOutgoingMessage(
          original_packet->persistent_id, message,
          base::BindOnce(&MCSClient::OnGCMUpdateFinished,
                         weak_ptr_factory_.GetWeakPtr()));

      // The message is already queued, return.
      return;
    } else {
      PersistentId persistent_id = GetNextPersistentId();
      DVLOG(1) << "Setting persistent id to " << persistent_id;
      packet_info->persistent_id = persistent_id;
      SetPersistentId(persistent_id, packet_info->protobuf.get());
      if (!gcm_store_->AddOutgoingMessage(
              persistent_id,
              MCSMessage(message.tag(), *(packet_info->protobuf)),
              base::BindOnce(&MCSClient::OnGCMUpdateFinished,
                             weak_ptr_factory_.GetWeakPtr()))) {
        NotifyMessageSendStatus(message.GetProtobuf(),
                                APP_QUEUE_SIZE_LIMIT_REACHED);
        return;
      }
    }

    if (collapse_key.IsValid())
      collapse_key_map_[collapse_key] = packet_info.get();
  } else if (!connection_factory_->IsEndpointReachable()) {
    DVLOG(1) << "No active connection, dropping message.";
    NotifyMessageSendStatus(message.GetProtobuf(), NO_CONNECTION_ON_ZERO_TTL);
    return;
  }

  to_send_.push_back(std::move(packet_info));

  // Notify that the messages has been succsfully queued for sending.
  // TODO(jianli): We should report QUEUED after writing to GCM store succeeds.
  NotifyMessageSendStatus(message.GetProtobuf(), QUEUED);

  MaybeSendMessage();
}

void MCSClient::UpdateHeartbeatTimer(
    std::unique_ptr<base::RetainingOneShotTimer> timer) {
  heartbeat_manager_.UpdateHeartbeatTimer(std::move(timer));
}

void MCSClient::AddHeartbeatInterval(const std::string& scope,
                                     int interval_ms) {
  if (!heartbeat_manager_.IsValidClientHeartbeatInterval(interval_ms))
    return;

  custom_heartbeat_intervals_[scope] = interval_ms;
  gcm_store_->AddHeartbeatInterval(
      scope, interval_ms,
      base::BindOnce(&MCSClient::OnGCMUpdateFinished,
                     weak_ptr_factory_.GetWeakPtr()));

  int min_interval_ms = GetMinHeartbeatIntervalMs();
  heartbeat_manager_.SetClientHeartbeatIntervalMs(min_interval_ms);
}

void MCSClient::RemoveHeartbeatInterval(const std::string& scope) {
  custom_heartbeat_intervals_.erase(scope);
  gcm_store_->RemoveHeartbeatInterval(
      scope, base::BindOnce(&MCSClient::OnGCMUpdateFinished,
                            weak_ptr_factory_.GetWeakPtr()));

  int min_interval = GetMinHeartbeatIntervalMs();
  heartbeat_manager_.SetClientHeartbeatIntervalMs(min_interval);
}

int MCSClient::GetMinHeartbeatIntervalMs() {
  if (custom_heartbeat_intervals_.empty())
    return kNoCustomHeartbeat;

  int min_interval = custom_heartbeat_intervals_.begin()->second;
  for (std::map<std::string, int>::const_iterator it =
           custom_heartbeat_intervals_.begin();
       it != custom_heartbeat_intervals_.end();
       ++it) {
    if (it->second < min_interval)
      min_interval = it->second;
  }
  return min_interval;
}

void MCSClient::ResetStateAndBuildLoginRequest(
    mcs_proto::LoginRequest* request) {
  DCHECK(android_id_);
  DCHECK(security_token_);
  stream_id_in_ = 0;
  stream_id_out_ = 1;
  last_device_to_server_stream_id_received_ = 0;
  last_server_to_device_stream_id_received_ = 0;

  heartbeat_manager_.Stop();

  // Add any pending acknowledgments to the list of ids.
  for (StreamIdToPersistentIdMap::const_iterator iter =
           unacked_server_ids_.begin();
       iter != unacked_server_ids_.end(); ++iter) {
    restored_unackeds_server_ids_.push_back(iter->second);
  }
  unacked_server_ids_.clear();

  // Any acknowledged server ids which have not been confirmed by the server
  // are treated like unacknowledged ids.
  for (std::map<StreamId, PersistentIdList>::const_iterator iter =
           acked_server_ids_.begin();
       iter != acked_server_ids_.end(); ++iter) {
    restored_unackeds_server_ids_.insert(restored_unackeds_server_ids_.end(),
                                         iter->second.begin(),
                                         iter->second.end());
  }
  acked_server_ids_.clear();

  // Then build the request, consuming all pending acknowledgments.
  request->Swap(BuildLoginRequest(android_id_,
                                  security_token_,
                                  version_string_).get());

  // Set custom heartbeat interval if specified.
  if (heartbeat_manager_.HasClientHeartbeatInterval()) {
    // Ensure that the custom heartbeat interval is communicated to the server.
    mcs_proto::Setting* setting = request->add_setting();
    setting->set_name(kHeartbeatIntervalSettingName);
    setting->set_value(base::NumberToString(
        heartbeat_manager_.GetClientHeartbeatIntervalMs()));
  }

  for (PersistentIdList::const_iterator iter =
           restored_unackeds_server_ids_.begin();
       iter != restored_unackeds_server_ids_.end(); ++iter) {
    request->add_received_persistent_id(*iter);
  }
  acked_server_ids_[stream_id_out_] = restored_unackeds_server_ids_;
  restored_unackeds_server_ids_.clear();

  // Push all unacknowledged messages to front of send queue. No need to save
  // to RMQ, as all messages that reach this point should already have been
  // saved as necessary.
  while (!to_resend_.empty()) {
    to_send_.push_front(std::move(to_resend_.back()));
    to_resend_.pop_back();
  }

  // Drop all TTL == 0 or expired TTL messages from the queue.
  base::circular_deque<MCSPacketInternal> new_to_send;
  std::vector<PersistentId> expired_ttl_ids;
  while (!to_send_.empty()) {
    MCSPacketInternal packet = PopMessageForSend();
    if (GetTTL(*packet->protobuf) > 0 &&
        !HasTTLExpired(*packet->protobuf, clock_)) {
      new_to_send.push_back(std::move(packet));
    } else {
      // If the TTL was 0 there is no persistent id, so no need to remove the
      // message from the persistent store.
      if (!packet->persistent_id.empty())
        expired_ttl_ids.push_back(packet->persistent_id);
      NotifyMessageSendStatus(*packet->protobuf, TTL_EXCEEDED);
    }
  }

  if (!expired_ttl_ids.empty()) {
    DVLOG(1) << "Connection reset, " << expired_ttl_ids.size()
             << " messages expired.";
    gcm_store_->RemoveOutgoingMessages(
        expired_ttl_ids, base::BindOnce(&MCSClient::OnGCMUpdateFinished,
                                        weak_ptr_factory_.GetWeakPtr()));
  }

  to_send_.swap(new_to_send);

  DVLOG(1) << "Resetting state, with " << request->received_persistent_id_size()
           << " incoming acks pending, and " << to_send_.size()
           << " pending outgoing messages.";

  state_ = CONNECTING;
}

void MCSClient::SendHeartbeat() {
  SendMessage(MCSMessage(kHeartbeatPingTag, mcs_proto::HeartbeatPing()));
}

void MCSClient::OnGCMUpdateFinished(bool success) {
  LOG_IF(ERROR, !success) << "GCM Update failed!";
  UMA_HISTOGRAM_BOOLEAN("GCM.StoreUpdateSucceeded", success);
  // TODO(zea): Rebuild the store from scratch in case of persistence failure?
}

void MCSClient::MaybeSendMessage() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (to_send_.empty())
    return;

  // If the connection has been reset, do nothing. On reconnection
  // MaybeSendMessage will be automatically invoked again.
  // TODO(zea): consider doing TTL expiration at connection reset time, rather
  // than reconnect time.
  if (!connection_factory_->IsEndpointReachable())
    return;

  MCSPacketInternal packet = PopMessageForSend();
  ReliablePacketInfo* packet_ptr = packet.get();
  if (HasTTLExpired(*packet->protobuf, clock_)) {
    DCHECK(!packet->persistent_id.empty());
    DVLOG(1) << "Dropping expired message " << packet->persistent_id << ".";
    NotifyMessageSendStatus(*packet->protobuf, TTL_EXCEEDED);
    gcm_store_->RemoveOutgoingMessage(
        packet->persistent_id, base::BindOnce(&MCSClient::OnGCMUpdateFinished,
                                              weak_ptr_factory_.GetWeakPtr()));
    io_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(&MCSClient::MaybeSendMessage,
                                             weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  DVLOG(1) << "Pending output message found, sending.";
  if (!packet->persistent_id.empty())
    to_resend_.push_back(std::move(packet));
  SendPacketToWire(packet_ptr);
}

void MCSClient::SendPacketToWire(ReliablePacketInfo* packet_info) {
  packet_info->stream_id = ++stream_id_out_;
  DVLOG(1) << "Sending packet of type " << packet_info->protobuf->GetTypeName();

  // Set the queued time as necessary.
  if (packet_info->tag == kDataMessageStanzaTag) {
    mcs_proto::DataMessageStanza* data_message =
        reinterpret_cast<mcs_proto::DataMessageStanza*>(
            packet_info->protobuf.get());
    uint64_t sent = data_message->sent();
    DCHECK_GT(sent, 0U);
    int queued = (clock_->Now().ToInternalValue() /
        base::Time::kMicrosecondsPerSecond) - sent;
    DVLOG(1) << "Message was queued for " << queued << " seconds.";
    data_message->set_queued(queued);
    recorder_->RecordDataSentToWire(
        data_message->category(),
        data_message->to(),
        data_message->id(),
        queued);
  }

  // Set the proper last received stream id to acknowledge received server
  // packets.
  DVLOG(1) << "Setting last stream id received to "
           << stream_id_in_;
  SetLastStreamIdReceived(stream_id_in_,
                          packet_info->protobuf.get());
  if (stream_id_in_ != last_server_to_device_stream_id_received_) {
    last_server_to_device_stream_id_received_ = stream_id_in_;
    // Mark all acknowledged server messages as such. Note: they're not dropped,
    // as it may be that they'll need to be re-acked if this message doesn't
    // make it.
    PersistentIdList persistent_id_list;
    for (StreamIdToPersistentIdMap::const_iterator iter =
             unacked_server_ids_.begin();
         iter != unacked_server_ids_.end(); ++iter) {
      DCHECK_LE(iter->first, last_server_to_device_stream_id_received_);
      persistent_id_list.push_back(iter->second);
    }
    unacked_server_ids_.clear();
    acked_server_ids_[stream_id_out_] = persistent_id_list;
  }

  connection_handler_->SendMessage(*packet_info->protobuf);
}

void MCSClient::HandleMCSDataMesssage(
    std::unique_ptr<google::protobuf::MessageLite> protobuf) {
  mcs_proto::DataMessageStanza* data_message =
      reinterpret_cast<mcs_proto::DataMessageStanza*>(protobuf.get());
  // TODO(zea): implement a proper status manager rather than hardcoding these
  // values.
  std::unique_ptr<mcs_proto::DataMessageStanza> response(
      new mcs_proto::DataMessageStanza());
  response->set_from(kGCMFromField);
  response->set_sent(clock_->Now().ToInternalValue() /
                         base::Time::kMicrosecondsPerSecond);
  response->set_ttl(0);
  bool send = false;
  for (int i = 0; i < data_message->app_data_size(); ++i) {
    const mcs_proto::AppData& app_data = data_message->app_data(i);
    if (app_data.key() == kIdleNotification) {
      // Tell the MCS server the client is not idle.
      send = true;
      mcs_proto::AppData data;
      data.set_key(kIdleNotification);
      data.set_value("false");
      response->add_app_data()->CopyFrom(data);
      response->set_category(kMCSCategory);
    }
  }

  if (send) {
    SendMessage(MCSMessage(kDataMessageStanzaTag, std::move(response)));
  }
}

void MCSClient::HandlePacketFromWire(
    std::unique_ptr<google::protobuf::MessageLite> protobuf) {
  if (!protobuf.get())
    return;
  uint8_t tag = GetMCSProtoTag(*protobuf);
  PersistentId persistent_id = GetPersistentId(*protobuf);
  StreamId last_stream_id_received = GetLastStreamIdReceived(*protobuf);

  if (last_stream_id_received != 0) {
    last_device_to_server_stream_id_received_ = last_stream_id_received;

    // Process device to server messages that have now been acknowledged by the
    // server. Because messages are stored in order, just pop off all that have
    // a stream id lower than server's last received stream id.
    HandleStreamAck(last_stream_id_received);

    // Process server_to_device_messages that the server now knows were
    // acknowledged. Again, they're in order, so just keep going until the
    // stream id is reached.
    StreamIdList acked_stream_ids_to_remove;
    for (std::map<StreamId, PersistentIdList>::iterator iter =
             acked_server_ids_.begin();
         iter != acked_server_ids_.end() &&
             iter->first <= last_stream_id_received; ++iter) {
      acked_stream_ids_to_remove.push_back(iter->first);
    }
    for (StreamIdList::iterator iter = acked_stream_ids_to_remove.begin();
         iter != acked_stream_ids_to_remove.end(); ++iter) {
      acked_server_ids_.erase(*iter);
    }
  }

  ++stream_id_in_;
  if (!persistent_id.empty()) {
    unacked_server_ids_[stream_id_in_] = persistent_id;
    gcm_store_->AddIncomingMessage(
        persistent_id, base::BindOnce(&MCSClient::OnGCMUpdateFinished,
                                      weak_ptr_factory_.GetWeakPtr()));
  }

  DVLOG(1) << "Received message of type " << protobuf->GetTypeName()
           << " with persistent id "
           << (persistent_id.empty() ? "NULL" : persistent_id)
           << ", stream id " << stream_id_in_ << " and last stream id received "
           << last_stream_id_received;

  if ((unacked_server_ids_.size() > 0 &&
       unacked_server_ids_.size() % kUnackedMessageBeforeStreamAck == 0) ||
      IsImmediateAckRequested(*protobuf)) {
    SendMessage(MCSMessage(kIqStanzaTag, BuildStreamAck()));
  }

  // The connection is alive, treat this message as a heartbeat ack.
  heartbeat_manager_.OnHeartbeatAcked();

  switch (tag) {
    case kLoginResponseTag: {
      DCHECK_EQ(CONNECTING, state_);
      mcs_proto::LoginResponse* login_response =
          reinterpret_cast<mcs_proto::LoginResponse*>(protobuf.get());
      DVLOG(1) << "Received login response:";
      DVLOG(1) << "  Id: " << login_response->id();
      DVLOG(1) << "  Timestamp: " << login_response->server_timestamp();
      if (login_response->has_error() && login_response->error().code() != 0) {
        state_ = UNINITIALIZED;
        LOG(ERROR) << "  Error code: " << login_response->error().code()
                   << "  Error message: " << login_response->error().message();
        LOG(ERROR) << "Failed to log in to GCM, resetting connection.";
        connection_factory_->SignalConnectionReset(
            ConnectionFactory::LOGIN_FAILURE);
        mcs_error_callback_.Run();
        return;
      }

      if (login_response->has_heartbeat_config()) {
        heartbeat_manager_.UpdateHeartbeatConfig(
            login_response->heartbeat_config());
      }

      state_ = CONNECTED;
      stream_id_in_ = 1;  // To account for the login response.
      DCHECK_EQ(1U, stream_id_out_);

      // Pass the login response on up.
      io_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(message_received_callback_,
                                    MCSMessage(tag, std::move(protobuf))));

      // If there are pending messages, attempt to send one.
      if (!to_send_.empty()) {
        io_task_runner_->PostTask(
            FROM_HERE, base::BindOnce(&MCSClient::MaybeSendMessage,
                                      weak_ptr_factory_.GetWeakPtr()));
      }

      heartbeat_manager_.Start(
          base::BindRepeating(&MCSClient::SendHeartbeat,
                              weak_ptr_factory_.GetWeakPtr()),
          base::BindRepeating(&MCSClient::OnConnectionResetByHeartbeat,
                              weak_ptr_factory_.GetWeakPtr()));
      return;
    }
    case kHeartbeatPingTag:
      DCHECK_GE(stream_id_in_, 1U);
      DVLOG(1) << "Received heartbeat ping, sending ack.";
      SendMessage(
          MCSMessage(kHeartbeatAckTag, mcs_proto::HeartbeatAck()));
      return;
    case kHeartbeatAckTag:
      DCHECK_GE(stream_id_in_, 1U);
      DVLOG(1) << "Received heartbeat ack.";
      // Do nothing else, all messages act as heartbeat acks.
      return;
    case kCloseTag:
      LOG(ERROR) << "Received close command, resetting connection.";
      state_ = LOADED;
      connection_factory_->SignalConnectionReset(
          ConnectionFactory::CLOSE_COMMAND);
      return;
    case kIqStanzaTag: {
      DCHECK_GE(stream_id_in_, 1U);
      mcs_proto::IqStanza* iq_stanza =
          reinterpret_cast<mcs_proto::IqStanza*>(protobuf.get());
      const mcs_proto::Extension& iq_extension = iq_stanza->extension();
      switch (iq_extension.id()) {
        case kSelectiveAck: {
          PersistentIdList acked_ids;
          if (BuildPersistentIdListFromProto(iq_extension.data(),
                                             &acked_ids)) {
            HandleSelectiveAck(acked_ids);
          }
          return;
        }
        case kStreamAck:
          // Do nothing. The last received stream id is always processed if it's
          // present.
          return;
        default:
          LOG(WARNING) << "Received invalid iq stanza extension "
                       << iq_extension.id();
          return;
      }
    }
    case kDataMessageStanzaTag: {
      DCHECK_GE(stream_id_in_, 1U);
      mcs_proto::DataMessageStanza* data_message =
          reinterpret_cast<mcs_proto::DataMessageStanza*>(protobuf.get());
      if (data_message->category() == kMCSCategory) {
        HandleMCSDataMesssage(std::move(protobuf));
        return;
      }

      DCHECK(protobuf.get());
      io_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(message_received_callback_,
                                    MCSMessage(tag, std::move(protobuf))));
      return;
    }
    default:
      LOG(ERROR) << "Received unexpected message of type "
                 << static_cast<int>(tag);
      return;
  }
}

void MCSClient::HandleStreamAck(StreamId last_stream_id_received) {
  PersistentIdList acked_outgoing_persistent_ids;
  StreamIdList acked_outgoing_stream_ids;
  while (!to_resend_.empty() &&
         to_resend_.front()->stream_id <= last_stream_id_received) {
    const MCSPacketInternal& outgoing_packet = to_resend_.front();
    acked_outgoing_persistent_ids.push_back(outgoing_packet->persistent_id);
    acked_outgoing_stream_ids.push_back(outgoing_packet->stream_id);
    NotifyMessageSendStatus(*outgoing_packet->protobuf, SENT);
    to_resend_.pop_front();
  }

  DVLOG(1) << "Server acked " << acked_outgoing_persistent_ids.size()
           << " outgoing messages, " << to_resend_.size()
           << " remaining unacked";
  gcm_store_->RemoveOutgoingMessages(
      acked_outgoing_persistent_ids,
      base::BindOnce(&MCSClient::OnGCMUpdateFinished,
                     weak_ptr_factory_.GetWeakPtr()));

  HandleServerConfirmedReceipt(last_stream_id_received);
}

void MCSClient::HandleSelectiveAck(const PersistentIdList& id_list) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  std::set<PersistentId> remaining_ids(id_list.begin(), id_list.end());

  StreamId last_stream_id_received = 0;

  // First check the to_resend_ queue. Acknowledgments are always contiguous,
  // so if there's a pending message that hasn't been acked, all newer messages
  // must also be unacked.
  while(!to_resend_.empty() && !remaining_ids.empty()) {
    const MCSPacketInternal& outgoing_packet = to_resend_.front();
    if (remaining_ids.count(outgoing_packet->persistent_id) == 0)
      break;  // Newer message must be unacked too.
    remaining_ids.erase(outgoing_packet->persistent_id);
    NotifyMessageSendStatus(*outgoing_packet->protobuf, SENT);

    // No need to re-acknowledge any server messages this message already
    // acknowledged.
    StreamId device_stream_id = outgoing_packet->stream_id;
    if (device_stream_id > last_stream_id_received)
      last_stream_id_received = device_stream_id;
    to_resend_.pop_front();
  }

  // If the acknowledged ids aren't all there, they might be in the to_send_
  // queue (typically when a SelectiveAck confirms messages as part of a login
  // response).
  while (!to_send_.empty() && !remaining_ids.empty()) {
    const MCSPacketInternal& outgoing_packet = to_send_.front();
    if (remaining_ids.count(outgoing_packet->persistent_id) == 0)
      break;  // Newer messages must be unacked too.
    remaining_ids.erase(outgoing_packet->persistent_id);
    NotifyMessageSendStatus(*outgoing_packet->protobuf, SENT);

    // No need to re-acknowledge any server messages this message already
    // acknowledged.
    StreamId device_stream_id = outgoing_packet->stream_id;
    if (device_stream_id > last_stream_id_received)
      last_stream_id_received = device_stream_id;
    PopMessageForSend();
  }

  // Only handle the largest stream id value. All other stream ids are
  // implicitly handled.
  if (last_stream_id_received > 0)
    HandleServerConfirmedReceipt(last_stream_id_received);

  // At this point, all remaining acked ids are redundant.
  PersistentIdList acked_ids;
  if (remaining_ids.size() > 0) {
    for (size_t i = 0; i < id_list.size(); ++i) {
      if (remaining_ids.count(id_list[i]) > 0)
        continue;
      acked_ids.push_back(id_list[i]);
    }
  } else {
    acked_ids = id_list;
  }

  DVLOG(1) << "Server acked " << acked_ids.size()
           << " messages, " << to_resend_.size() << " remaining unacked.";
  gcm_store_->RemoveOutgoingMessages(
      acked_ids, base::BindOnce(&MCSClient::OnGCMUpdateFinished,
                                weak_ptr_factory_.GetWeakPtr()));

  // Resend any remaining outgoing messages, as they were not received by the
  // server.
  DVLOG(1) << "Resending " << to_resend_.size() << " messages.";
  while (!to_resend_.empty()) {
    to_send_.push_front(std::move(to_resend_.back()));
    to_resend_.pop_back();
  }
  io_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&MCSClient::MaybeSendMessage,
                                           weak_ptr_factory_.GetWeakPtr()));
}

void MCSClient::HandleServerConfirmedReceipt(StreamId device_stream_id) {
  PersistentIdList acked_incoming_ids;
  for (std::map<StreamId, PersistentIdList>::iterator iter =
           acked_server_ids_.begin();
       iter != acked_server_ids_.end() &&
           iter->first <= device_stream_id;) {
    acked_incoming_ids.insert(acked_incoming_ids.end(),
                              iter->second.begin(),
                              iter->second.end());
    acked_server_ids_.erase(iter++);
  }

  DVLOG(1) << "Server confirmed receipt of " << acked_incoming_ids.size()
           << " acknowledged server messages.";
  gcm_store_->RemoveIncomingMessages(
      acked_incoming_ids, base::BindOnce(&MCSClient::OnGCMUpdateFinished,
                                         weak_ptr_factory_.GetWeakPtr()));
}

MCSClient::PersistentId MCSClient::GetNextPersistentId() {
  return base::NumberToString(base::TimeTicks::Now().ToInternalValue());
}

void MCSClient::OnConnectionResetByHeartbeat(
    ConnectionFactory::ConnectionResetReason reason) {
  connection_factory_->SignalConnectionReset(reason);
}

void MCSClient::NotifyMessageSendStatus(
    const google::protobuf::MessageLite& protobuf,
    MessageSendStatus status) {
  if (GetMCSProtoTag(protobuf) != kDataMessageStanzaTag)
    return;

  const mcs_proto::DataMessageStanza* data_message_stanza =
      reinterpret_cast<const mcs_proto::DataMessageStanza*>(&protobuf);
  recorder_->RecordNotifySendStatus(
      data_message_stanza->category(),
      data_message_stanza->to(),
      data_message_stanza->id(),
      status,
      protobuf.ByteSize(),
      data_message_stanza->ttl());
  message_sent_callback_.Run(
      data_message_stanza->device_user_id(),
      data_message_stanza->category(),
      data_message_stanza->id(),
      status);
}

MCSClient::MCSPacketInternal MCSClient::PopMessageForSend() {
  MCSPacketInternal packet = std::move(to_send_.front());
  to_send_.pop_front();

  if (packet->tag == kDataMessageStanzaTag) {
    mcs_proto::DataMessageStanza* data_message =
        reinterpret_cast<mcs_proto::DataMessageStanza*>(packet->protobuf.get());
    CollapseKey collapse_key(*data_message);
    if (collapse_key.IsValid())
      collapse_key_map_.erase(collapse_key);
  }

  return packet;
}

} // namespace gcm
