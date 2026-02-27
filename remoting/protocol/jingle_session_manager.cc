// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_session_manager.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/jingle_session.h"
#include "remoting/protocol/session_observer.h"
#include "remoting/protocol/transport.h"
#include "remoting/signaling/content_description.h"
#include "remoting/signaling/iq_sender.h"
#include "remoting/signaling/jingle_data_structures.h"
#include "remoting/signaling/jingle_message_xml_converter.h"
#include "remoting/signaling/signal_strategy.h"
#include "remoting/signaling/xmpp_constants.h"
#include "third_party/webrtc/rtc_base/socket_address.h"

namespace remoting::protocol {

JingleSessionManager::JingleSessionManager(SignalStrategy* signal_strategy)
    : signal_strategy_(signal_strategy),
      protocol_config_(CandidateSessionConfig::CreateDefault()),
      iq_sender_(new IqSender(signal_strategy_)) {
  signal_strategy_->AddListener(this);
}

JingleSessionManager::~JingleSessionManager() {
  DCHECK(sessions_.empty());
  signal_strategy_->RemoveListener(this);
}

void JingleSessionManager::AcceptIncoming(
    const IncomingSessionCallback& incoming_session_callback) {
  incoming_session_callback_ = incoming_session_callback;
}

void JingleSessionManager::set_protocol_config(
    std::unique_ptr<CandidateSessionConfig> config) {
  protocol_config_ = std::move(config);
}

std::unique_ptr<Session> JingleSessionManager::Connect(
    const SignalingAddress& peer_address,
    std::unique_ptr<Authenticator> authenticator) {
  std::unique_ptr<JingleSession> session(new JingleSession(this));
  session->StartConnection(peer_address, std::move(authenticator));
  sessions_[session->session_id_] = session.get();
  return std::move(session);
}

void JingleSessionManager::set_authenticator_factory(
    std::unique_ptr<AuthenticatorFactory> authenticator_factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  authenticator_factory_ = std::move(authenticator_factory);
}

SessionObserver::Subscription JingleSessionManager::AddSessionObserver(
    SessionObserver* observer) {
  observers_.AddObserver(observer);
  return SessionObserver::Subscription(
      base::BindOnce(&JingleSessionManager::RemoveSessionObserver,
                     weak_factory_.GetWeakPtr(), observer));
}
void JingleSessionManager::RemoveSessionObserver(SessionObserver* observer) {
  observers_.RemoveObserver(observer);
}

void JingleSessionManager::OnSignalStrategyStateChange(
    SignalStrategy::State state) {}

bool JingleSessionManager::OnSignalStrategyIncomingMessage(
    const SignalingAddress& sender_address,
    const SignalingMessage& signaling_message) {
  const auto* message = std::get_if<JingleMessage>(&signaling_message);
  if (!message) {
    return false;
  }

  // TODO: joedow - Use std::visit(absl::Overload(...), message->payload()) here
  // once the JingleMessage payload is being populated for incoming messages.
  if (message->action() == JingleMessage::ActionType::kSessionInitiate) {
    // Description must be present in session-initiate messages.
    DCHECK(message->description.get());

    SendReply(*message);

    std::unique_ptr<Authenticator> authenticator =
        authenticator_factory_->CreateAuthenticator(
            signal_strategy_->GetLocalAddress().id(), message->from.id());

    JingleSession* session = new JingleSession(this);
    session->InitializeIncomingConnection(*message, std::move(authenticator));
    sessions_[session->session_id_] = session;

    // Destroy the session if it was rejected due to incompatible protocol.
    if (session->state_ != Session::ACCEPTING) {
      delete session;
      DCHECK(sessions_.find(message->sid) == sessions_.end());
      return true;
    }

    IncomingSessionResponse response = SessionManager::DECLINE;
    std::string rejection_reason;
    base::Location rejection_location;
    if (!incoming_session_callback_.is_null()) {
      incoming_session_callback_.Run(session, &response, &rejection_reason,
                                     &rejection_location);
    }

    if (response == SessionManager::ACCEPT) {
      session->AcceptIncomingConnection(*message);
    } else {
      ErrorCode error;
      switch (response) {
        case OVERLOAD:
          error = ErrorCode::HOST_OVERLOAD;
          break;

        case DECLINE:
          error = ErrorCode::SESSION_REJECTED;
          break;

        default:
          NOTREACHED();
      }

      session->Close(error, rejection_reason, rejection_location);
      delete session;
      DCHECK(sessions_.find(message->sid) == sessions_.end());
    }

    return true;
  }

  auto it = sessions_.find(message->sid);
  if (it == sessions_.end()) {
    SendReply(*message, JingleMessageReply::INVALID_SID);
    return true;
  }

  // TODO: joedow - Consider whether OnIncomingMessage can be modified to take a
  // const& instead and move the copy operation there (or eliminate it).
  it->second->OnIncomingMessage(
      std::make_unique<JingleMessage>(*message),
      base::BindOnce(&JingleSessionManager::SendReply, base::Unretained(this)));
  return true;
}

void JingleSessionManager::SendReply(
    const JingleMessage& original_message,
    std::optional<JingleMessageReply::ErrorType> error) {
  JingleMessageReply reply;
  if (error.has_value()) {
    reply = JingleMessageReply(*error);
  }
  reply.message_id = original_message.message_id;
  reply.to = original_message.from;
  // TODO: joedow - Add overload for SendMessage which accepts a JingleMessage
  // or JingleMessageReply since those types contain the to address.
  signal_strategy_->SendMessage(original_message.from,
                                SignalingMessage(std::move(reply)));
}

void JingleSessionManager::SessionDestroyed(JingleSession* session) {
  sessions_.erase(session->session_id_);
}

}  // namespace remoting::protocol
