// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_SESSION_H_
#define REMOTING_PROTOCOL_JINGLE_SESSION_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "crypto/rsa_private_key.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/datagram_channel_factory.h"
#include "remoting/protocol/jingle_messages.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/session_config.h"
#include "remoting/signaling/iq_sender.h"

namespace remoting::protocol {

class JingleSessionManager;
class Transport;

// JingleSessionManager and JingleSession implement the subset of the Jingle
// protocol used in Chromoting. Instances of this class are created by the
// JingleSessionManager.
class JingleSession : public Session {
 public:
  JingleSession(const JingleSession&) = delete;
  JingleSession& operator=(const JingleSession&) = delete;

  ~JingleSession() override;

  // Session interface.
  void SetEventHandler(Session::EventHandler* event_handler) override;
  ErrorCode error() const override;
  const std::string& jid() override;
  const SessionConfig& config() override;
  const Authenticator& authenticator() const override;
  void SetTransport(Transport* transport) override;
  void Close(protocol::ErrorCode error) override;
  void AddPlugin(SessionPlugin* plugin) override;

 private:
  friend class JingleSessionManager;

  using ReplyCallback = base::OnceCallback<void(JingleMessageReply::ErrorType)>;

  explicit JingleSession(JingleSessionManager* session_manager);

  // Start connection by sending session-initiate message.
  void StartConnection(const SignalingAddress& peer_address,
                       std::unique_ptr<Authenticator> authenticator);

  // Called by JingleSessionManager for incoming connections.
  void InitializeIncomingConnection(
      const std::string& message_id,
      const JingleMessage& initiate_message,
      std::unique_ptr<Authenticator> authenticator);
  void AcceptIncomingConnection(const JingleMessage& initiate_message);

  // Callback for Transport interface to send transport-info messages.
  void SendTransportInfo(
      std::unique_ptr<jingle_xmpp::XmlElement> transport_info);

  // Sends |message| to the peer. The session is closed if the send fails or no
  // response is received within a reasonable time. All other responses are
  // ignored.
  void SendMessage(std::unique_ptr<JingleMessage> message);

  // Iq response handler.
  void OnMessageResponse(JingleMessage::ActionType request_type,
                         IqRequest* request,
                         const jingle_xmpp::XmlElement* response);

  // Response handler for transport-info responses. Transport-info timeouts are
  // ignored and don't terminate connection.
  void OnTransportInfoResponse(IqRequest* request,
                               const jingle_xmpp::XmlElement* response);

  // Called by JingleSessionManager on incoming |message|. Must call
  // |reply_callback| to send reply message before sending any other
  // messages.
  void OnIncomingMessage(const std::string& id,
                         std::unique_ptr<JingleMessage> message,
                         ReplyCallback reply_callback);

  // Called by OnIncomingMessage() to process the incoming Jingle messages
  // in the same order that they are sent.
  void ProcessIncomingMessage(std::unique_ptr<JingleMessage> message,
                              ReplyCallback reply_callback);

  // Message handlers for incoming messages.
  void OnAccept(std::unique_ptr<JingleMessage> message,
                ReplyCallback reply_callback);
  void OnSessionInfo(std::unique_ptr<JingleMessage> message,
                     ReplyCallback reply_callback);
  void OnTransportInfo(std::unique_ptr<JingleMessage> message,
                       ReplyCallback reply_callback);
  void OnTerminate(std::unique_ptr<JingleMessage> message,
                   ReplyCallback reply_callback);
  void OnAuthenticatorStateChangeAfterAccepted();

  // Called from OnAccept() to initialize session config.
  bool InitializeConfigFromDescription(const ContentDescription* description);

  // Called after the initial incoming authenticator message is processed.
  void ContinueAcceptIncomingConnection();

  // Called after subsequent authenticator messages are processed.
  void ProcessAuthenticationStep();

  // Called when authentication is finished.
  void OnAuthenticated();

  // Sets |state_| to |new_state| and calls state change callback.
  void SetState(State new_state);

  // Returns true if the state of the session is not CLOSED or FAILED
  bool is_session_active();

  // Executes all plugins against incoming JingleMessage.
  void ProcessIncomingPluginMessage(const JingleMessage& message);

  // Executes all plugins against outgoing JingleMessage.
  void AddPluginAttachments(JingleMessage* message);

  // Sends session-initiate message.
  void SendSessionInitiateMessage();

  // Returns the value of the ID attribute of the next outgoing set IQ with the
  // sequence ID encoded.
  std::string GetNextOutgoingId();

  raw_ptr<JingleSessionManager> session_manager_;
  SignalingAddress peer_address_;
  raw_ptr<Session::EventHandler> event_handler_;

  std::string session_id_;
  State state_;
  ErrorCode error_;

  std::unique_ptr<SessionConfig> config_;

  std::unique_ptr<Authenticator> authenticator_;

  raw_ptr<Transport> transport_ = nullptr;

  // Pending Iq requests. Used for all messages except transport-info.
  std::vector<std::unique_ptr<IqRequest>> pending_requests_;

  // Pending transport-info requests.
  std::vector<std::unique_ptr<IqRequest>> transport_info_requests_;

  struct PendingMessage {
    PendingMessage();
    PendingMessage(PendingMessage&& moved);
    PendingMessage(std::unique_ptr<JingleMessage> message,
                   ReplyCallback reply_callback);
    ~PendingMessage();
    PendingMessage& operator=(PendingMessage&& moved);
    std::unique_ptr<JingleMessage> message;
    ReplyCallback reply_callback;
  };

  // A message queue to guarantee the incoming messages are processed in order.
  class OrderedMessageQueue;
  std::unique_ptr<OrderedMessageQueue> message_queue_;

  // This prefix is necessary to disambiguate between the ID's sent from the
  // client and the ID's sent from the host.
  std::string outgoing_id_prefix_ = base::NumberToString(base::RandUint64());
  int next_outgoing_id_ = 0;

  // Transport info messages that are received while the session is being
  // authenticated.
  std::vector<PendingMessage> pending_transport_info_;

  // The SessionPlugins attached to this session.
  std::vector<raw_ptr<SessionPlugin>> plugins_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<JingleSession> weak_factory_{this};
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_JINGLE_SESSION_H_
