// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_SESSION_MANAGER_H_
#define REMOTING_PROTOCOL_JINGLE_SESSION_MANAGER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "remoting/protocol/jingle_messages.h"
#include "remoting/protocol/session_manager.h"
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

 private:
  friend class JingleSession;

  // SignalStrategy::Listener interface.
  void OnSignalStrategyStateChange(SignalStrategy::State state) override;
  bool OnSignalStrategyIncomingStanza(const jingle_xmpp::XmlElement* stanza) override;

  typedef std::map<std::string, JingleSession*> SessionsMap;

  IqSender* iq_sender() { return iq_sender_.get(); }
  void SendReply(std::unique_ptr<jingle_xmpp::XmlElement> original_stanza,
                 JingleMessageReply::ErrorType error);

  // Called by JingleSession when it is being destroyed.
  void SessionDestroyed(JingleSession* session);

  SignalStrategy* signal_strategy_ = nullptr;
  IncomingSessionCallback incoming_session_callback_;
  std::unique_ptr<CandidateSessionConfig> protocol_config_;

  std::unique_ptr<AuthenticatorFactory> authenticator_factory_;
  std::unique_ptr<IqSender> iq_sender_;

  SessionsMap sessions_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(JingleSessionManager);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_JINGLE_SESSION_MANAGER_H_
