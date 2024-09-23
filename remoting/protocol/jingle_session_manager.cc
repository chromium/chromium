// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_session_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/content_description.h"
#include "remoting/protocol/jingle_messages.h"
#include "remoting/protocol/jingle_session.h"
#include "remoting/protocol/session_observer.h"
#include "remoting/protocol/transport.h"
#include "remoting/signaling/iq_sender.h"
#include "remoting/signaling/signal_strategy.h"
#include "remoting/signaling/xmpp_constants.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"
#include "third_party/webrtc/rtc_base/socket_address.h"

using jingle_xmpp::QName;

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

bool JingleSessionManager::OnSignalStrategyIncomingStanza(
    const jingle_xmpp::XmlElement* stanza) {
  if (!JingleMessage::IsJingleMessage(stanza)) {
    return false;
  }

  std::unique_ptr<jingle_xmpp::XmlElement> stanza_copy(
      new jingle_xmpp::XmlElement(*stanza));
  std::unique_ptr<JingleMessage> message(new JingleMessage());
  std::string error_msg;
  if (!message->ParseXml(stanza, &error_msg)) {
    SendReply(std::move(stanza_copy), JingleMessageReply::BAD_REQUEST);
    return true;
  }

  if (message->action == JingleMessage::SESSION_INITIATE) {
    // Description must be present in session-initiate messages.
    DCHECK(message->description.get());

    SendReply(std::move(stanza_copy), JingleMessageReply::NONE);

    std::unique_ptr<Authenticator> authenticator =
        authenticator_factory_->CreateAuthenticator(
            signal_strategy_->GetLocalAddress().id(), message->from.id());

    JingleSession* session = new JingleSession(this);
    session->InitializeIncomingConnection(stanza->Attr(kQNameId), *message,
                                          std::move(authenticator));
    sessions_[session->session_id_] = session;

    // Destroy the session if it was rejected due to incompatible protocol.
    if (session->state_ != Session::ACCEPTING) {
      delete session;
      DCHECK(sessions_.find(message->sid) == sessions_.end());
      return true;
    }

    IncomingSessionResponse response = SessionManager::DECLINE;
    if (!incoming_session_callback_.is_null()) {
      incoming_session_callback_.Run(session, &response);
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

      session->Close(error);
      delete session;
      DCHECK(sessions_.find(message->sid) == sessions_.end());
    }

    return true;
  }

  auto it = sessions_.find(message->sid);
  if (it == sessions_.end()) {
    SendReply(std::move(stanza_copy), JingleMessageReply::INVALID_SID);
    return true;
  }

  it->second->OnIncomingMessage(
      stanza->Attr(kQNameId), std::move(message),
      base::BindOnce(&JingleSessionManager::SendReply, base::Unretained(this),
                     std::move(stanza_copy)));
  return true;
}

void JingleSessionManager::SendReply(
    std::unique_ptr<jingle_xmpp::XmlElement> original_stanza,
    JingleMessageReply::ErrorType error) {
  signal_strategy_->SendStanza(
      JingleMessageReply(error).ToXml(original_stanza.get()));
}

void JingleSessionManager::SessionDestroyed(JingleSession* session) {
  sessions_.erase(session->session_id_);
}

}  // namespace remoting::protocol
