// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_SESSION_MANAGER_H_
#define REMOTING_PROTOCOL_JINGLE_SESSION_MANAGER_H_

#include <map>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "remoting/protocol/jingle_messages.h"
#include "remoting/protocol/session_manager.h"
#include "remoting/protocol/session_observer.h"
#include "remoting/signaling/signal_strategy.h"

namespace jingle_xmpp {
class XmlElement;
}  // namespace jingle_xmpp

namespace remoting {

class IqSender;

namespace protocol {

class JingleSession;

// JingleSessionManager and JingleSession implement the subset of the
// Jingle protocol used in Chromoting. JingleSessionManager provides
// the protocol::SessionManager interface for accepting incoming and
// creating outgoing sessions.
class JingleSessionManager : public SessionManager,
                             public SignalStrategy::Listener {
 public:
  explicit JingleSessionManager(SignalStrategy* signal_strategy);

  JingleSessionManager(const JingleSessionManager&) = delete;
  JingleSessionManager& operator=(const JingleSessionManager&) = delete;

  ~JingleSessionManager() override;

  // SessionManager interface.
  void AcceptIncoming(
      const IncomingSessionCallback& incoming_session_callback) override;
  void set_protocol_config(
      std::unique_ptr<CandidateSessionConfig> config) override;
  std::unique_ptr<Session> Connect(
      const SignalingAddress& peer_address,
      std::unique_ptr<Authenticator> authenticator) override;
  void set_authenticator_factory(
      std::unique_ptr<AuthenticatorFactory> authenticator_factory) override;
  [[nodiscard]] SessionObserver::Subscription AddSessionObserver(
      SessionObserver* observer) override;

 private:
  friend class JingleSession;

  void RemoveSessionObserver(SessionObserver* observer);

  // SignalStrategy::Listener interface.
  void OnSignalStrategyStateChange(SignalStrategy::State state) override;
  bool OnSignalStrategyIncomingStanza(
      const jingle_xmpp::XmlElement* stanza) override;

  typedef std::map<std::string, raw_ptr<JingleSession, CtnExperimental>>
      SessionsMap;

  IqSender* iq_sender() { return iq_sender_.get(); }
  void SendReply(std::unique_ptr<jingle_xmpp::XmlElement> original_stanza,
                 JingleMessageReply::ErrorType error);

  // Called by JingleSession when it is being destroyed.
  void SessionDestroyed(JingleSession* session);

  raw_ptr<SignalStrategy> signal_strategy_ = nullptr;
  IncomingSessionCallback incoming_session_callback_;
  std::unique_ptr<CandidateSessionConfig> protocol_config_;

  std::unique_ptr<AuthenticatorFactory> authenticator_factory_;
  std::unique_ptr<IqSender> iq_sender_;

  SessionsMap sessions_;
  base::ObserverList<SessionObserver> observers_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<JingleSessionManager> weak_factory_{this};
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_JINGLE_SESSION_MANAGER_H_
