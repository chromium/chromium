// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_MCS_CLIENT_H_
#define GOOGLE_APIS_GCM_ENGINE_MCS_CLIENT_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "google_apis/gcm/base/gcm_export.h"
#include "google_apis/gcm/base/mcs_message.h"
#include "google_apis/gcm/engine/connection_factory.h"
#include "google_apis/gcm/engine/connection_handler.h"
#include "google_apis/gcm/engine/gcm_store.h"
#include "google_apis/gcm/engine/heartbeat_manager.h"

namespace base {
class Clock;
class RetainingOneShotTimer;
}  // namespace base

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace mcs_proto {
class LoginRequest;
}

namespace gcm {

class CollapseKey;
class ConnectionFactory;
class GCMStatsRecorder;
struct ReliablePacketInfo;

// An MCS client. This client is in charge of all communications with an
// MCS endpoint, and is capable of reliably sending/receiving GCM messages.
// NOTE: Not thread safe. This class should live on the same thread as that
// network requests are performed on.
class GCM_EXPORT MCSClient {
 public:
  // Any change made to this enum should have corresponding change in the
  // GetStateString(...) function.
  enum State {
    UNINITIALIZED,  // Uninitialized.
    LOADED,         // GCM Load finished, waiting to connect.
    CONNECTING,     // Connection in progress.
    CONNECTED,      // Connected and running.
  };

  // Any change made to this enum should have corresponding change in the
  // GetMessageSendStatusString(...) function in mcs_client.cc.
  enum MessageSendStatus {
    // Message was queued successfully.
    QUEUED,
    // Message was sent to the server and the ACK was received.
    SENT,
    // Message not saved, because total queue size limit reached.
    QUEUE_SIZE_LIMIT_REACHED,
    // Message not saved, because app queue size limit reached.
    APP_QUEUE_SIZE_LIMIT_REACHED,
    // Message too large to send.
    MESSAGE_TOO_LARGE,
    // Message not send because of TTL = 0 and no working connection.
    NO_CONNECTION_ON_ZERO_TTL,
    // Message exceeded TTL.
    TTL_EXCEEDED,

    // NOTE: always keep this entry at the end. Add new status types only
    // immediately above this line. Make sure to update the corresponding
    // histogram enum accordingly.
    SEND_STATUS_COUNT
  };

  // Callback for MCSClient's error conditions. A repeating callback is used
  // because occasionally multiple errors are reported, see crbug.com/1039598
  // for more context.
  // TODO(fgorski): Keeping it as a callback with intention to add meaningful
  // error information.
  using ErrorCallback = base::RepeatingClosure;
  // Callback when a message is received.
  using OnMessageReceivedCallback =
      base::RepeatingCallback<void(const MCSMessage& message)>;
  // Callback when a message is sent (and receipt has been acknowledged by
  // the MCS endpoint).
  using OnMessageSentCallback =
      base::RepeatingCallback<void(int64_t user_serial_number,
                                   const std::string& app_id,
                                   const std::string& message_id,
                                   MessageSendStatus status)>;

  MCSClient(const std::string& version_string,
            base::Clock* clock,
            ConnectionFactory* connection_factory,
            GCMStore* gcm_store,
            scoped_refptr<base::SequencedTaskRunner> io_task_runner,
            GCMStatsRecorder* recorder);

  MCSClient(const MCSClient&) = delete;
  MCSClient& operator=(const MCSClient&) = delete;

  virtual ~MCSClient();

  // Initialize the client. Will load any previous id/token information as well
  // as unacknowledged message information from the GCM storage, if it exists,
  // passing the id/token information back via |initialization_callback| along
  // with a |success == true| result. If no GCM information is present (and
  // this is therefore a fresh client), a clean GCM store will be created and
  // values of 0 will be returned via |initialization_callback| with
  // |success == true|.
  // If an error loading the GCM store is encountered,
  // |initialization_callback| will be invoked with |success == false|.
  void Initialize(const ErrorCallback& initialization_callback,
                  const OnMessageReceivedCallback& message_received_callback,
                  const OnMessageSentCallback& message_sent_callback,
                  std::unique_ptr<GCMStore::LoadResult> load_result);

  // Logs the client into the server. Client must be initialized.
  // |android_id| and |security_token| are optional if this is not a new
  // client, else they must be non-zero.
  // Successful login will result in |message_received_callback| being invoked
  // with a valid LoginResponse.
  // Login failure (typically invalid id/token) will shut down the client, and
  // |initialization_callback| to be invoked with |success = false|.
  virtual void Login(uint64_t android_id, uint64_t security_token);

  // Sends a message, with or without reliable message queueing (RMQ) support.
  // Will asynchronously invoke the OnMessageSent callback regardless.
  // Whether to use RMQ depends on whether the protobuf has |ttl| set or not.
  // |ttl == 0| denotes the message should only be sent if the connection is
  // open. |ttl > 0| will keep the message saved for |ttl| seconds, after which
  // it will be dropped if it was unable to be sent. When a message is dropped,
  // |message_sent_callback_| is invoked with a TTL expiration error.
  virtual void SendMessage(const MCSMessage& message);

  // Returns the current state of the client.
  State state() const { return state_; }

  // Returns the size of the send message queue.
  int GetSendQueueSize() const;

  // Returns the size of the resend messaage queue.
  int GetResendQueueSize() const;

  // Returns text representation of the state enum.
  std::string GetStateString() const;

  // Updates the timer used by |heartbeat_manager_| for sending heartbeats.
  void UpdateHeartbeatTimer(std::unique_ptr<base::RetainingOneShotTimer> timer);

  // Allows a caller to set a heartbeat interval (in milliseconds) with which
  // the MCS connection will be monitored on both ends, to detect device
  // presence. In case the newly set interval is smaller than the current one,
  // connection will be restarted with new heartbeat interval. Valid values have
  // to be between GetMax/GetMinClientHeartbeatIntervalMs of HeartbeatManager,
  // otherwise the setting won't take effect.
  void AddHeartbeatInterval(const std::string& scope, int interval_ms);
  void RemoveHeartbeatInterval(const std::string& scope);

  HeartbeatManager* GetHeartbeatManagerForTesting() {
    return &heartbeat_manager_;
  }

 private:
  using StreamId = uint32_t;
  using PersistentId = std::string;
  using StreamIdList = std::vector<StreamId>;
  using PersistentIdList = std::vector<PersistentId>;
  using StreamIdToPersistentIdMap = std::map<StreamId, PersistentId>;
  using MCSPacketInternal = std::unique_ptr<ReliablePacketInfo>;

  // Resets the internal state and builds a new login request, acknowledging
  // any pending server-to-device messages and rebuilding the send queue
  // from all unacknowledged device-to-server messages.
  // Should only be called when the connection has been reset.
  void ResetStateAndBuildLoginRequest(mcs_proto::LoginRequest* request);

  // Send a heartbeat to the MCS server.
  void SendHeartbeat();

  // GCM Store callback.
  void OnGCMUpdateFinished(bool success);

  // Attempt to send a message.
  void MaybeSendMessage();

  // Helper for sending a protobuf along with any unacknowledged ids to the
  // wire.
  void SendPacketToWire(ReliablePacketInfo* packet_info);

  // Handle a data message sent to the MCS client system from the MCS server.
  void HandleMCSDataMesssage(
      std::unique_ptr<google::protobuf::MessageLite> protobuf);

  // Handle a packet received over the wire.
  void HandlePacketFromWire(
      std::unique_ptr<google::protobuf::MessageLite> protobuf);

  // ReliableMessageQueue acknowledgment helpers.
  // Handle a StreamAck sent by the server confirming receipt of all
  // messages up to the message with stream id |last_stream_id_received|.
  void HandleStreamAck(StreamId last_stream_id_received_);
  // Handle a SelectiveAck sent by the server confirming all messages
  // in |id_list|.
  void HandleSelectiveAck(const PersistentIdList& id_list);
  // Handle server confirmation of a device message, including device's
  // acknowledgment of receipt of messages.
  void HandleServerConfirmedReceipt(StreamId device_stream_id);

  // Generates a new persistent id for messages.
  // Virtual for testing.
  virtual PersistentId GetNextPersistentId();

  // Helper for the heartbeat manager to signal a connection reset.
  void OnConnectionResetByHeartbeat(
      ConnectionFactory::ConnectionResetReason reason);

  // Runs the message_sent_callback_ with send |status| of the |protobuf|.
  void NotifyMessageSendStatus(const google::protobuf::MessageLite& protobuf,
                               MessageSendStatus status);

  // Pops the next message from the front of the send queue (cleaning up
  // any associated state).
  MCSPacketInternal PopMessageForSend();

  // Gets the minimum interval from the map of scopes to intervals in
  // milliseconds.
  int GetMinHeartbeatIntervalMs();

  // Local version string. Sent on login.
  const std::string version_string_;

  // Clock for enforcing TTL. Passed in for testing.
  const raw_ptr<base::Clock> clock_;

  // Client state.
  State state_;

  // Callbacks for owner.
  ErrorCallback mcs_error_callback_;
  OnMessageReceivedCallback message_received_callback_;
  OnMessageSentCallback message_sent_callback_;

  // The android id and security token in use by this device.
  uint64_t android_id_;
  uint64_t security_token_;

  // Factory for creating new connections and connection handlers.
  raw_ptr<ConnectionFactory, DanglingUntriaged> connection_factory_;

  // Connection handler to handle all over-the-wire protocol communication
  // with the mobile connection server.
  raw_ptr<ConnectionHandler, DanglingUntriaged> connection_handler_;

  // -----  Reliablie Message Queue section -----
  // Note: all queues/maps are ordered from oldest (front/begin) message to
  // most recent (back/end).

  // Send/acknowledge queues.
  base::circular_deque<MCSPacketInternal> to_send_;
  base::circular_deque<MCSPacketInternal> to_resend_;

  // Map of collapse keys to their pending messages.
  std::map<CollapseKey, raw_ptr<ReliablePacketInfo, CtnExperimental>>
      collapse_key_map_;

  // Last device_to_server stream id acknowledged by the server.
  StreamId last_device_to_server_stream_id_received_;
  // Last server_to_device stream id acknowledged by this device.
  StreamId last_server_to_device_stream_id_received_;
  // The stream id for the last sent message. A new message should consume
  // stream_id_out_ + 1.
  StreamId stream_id_out_;
  // The stream id of the last received message. The LoginResponse will always
  // have a stream id of 1, and stream ids increment by 1 for each received
  // message.
  StreamId stream_id_in_;

  // The server messages that have not been acked by the device yet. Keyed by
  // server stream id.
  StreamIdToPersistentIdMap unacked_server_ids_;

  // Those server messages that have been acked. They must remain tracked
  // until the ack message is itself confirmed. The list of all message ids
  // acknowledged are keyed off the device stream id of the message that
  // acknowledged them.
  std::map<StreamId, PersistentIdList> acked_server_ids_;

  // Those server messages from a previous connection that were not fully
  // acknowledged. They do not have associated stream ids, and will be
  // acknowledged on the next login attempt.
  PersistentIdList restored_unackeds_server_ids_;

  // The GCM persistent store. Not owned.
  raw_ptr<GCMStore, DanglingUntriaged> gcm_store_;

  const scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  // Manager to handle triggering/detecting heartbeats.
  HeartbeatManager heartbeat_manager_;

  // Custom heartbeat intervals requested by different components.
  std::map<std::string, int> custom_heartbeat_intervals_;

  // Recorder that records GCM activities for debugging purpose. Not owned.
  raw_ptr<GCMStatsRecorder> recorder_;

  base::WeakPtrFactory<MCSClient> weak_ptr_factory_{this};
};

} // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_MCS_CLIENT_H_
