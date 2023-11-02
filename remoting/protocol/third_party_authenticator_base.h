// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_THIRD_PARTY_AUTHENTICATOR_BASE_H_
#define REMOTING_PROTOCOL_THIRD_PARTY_AUTHENTICATOR_BASE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "remoting/protocol/authenticator.h"
#include "third_party/libjingle_xmpp/xmllite/qname.h"

namespace jingle_xmpp {

class XmlElement;

}  // namespace jingle_xmpp

namespace remoting::protocol {

// Implements an authentication method that relies on a third party server for
// authentication of both client and host.
// When third party authentication is being used, the client must request both a
// token and a shared secret from a third-party server (which may require the
// user to authenticate themselves). The client then sends only the token to the
// host. The host signs the token, then contacts the third-party server to
// exchange the token for the shared secret. Once both client and host have the
// shared secret, they use an underlying |V2Authenticator| (SPAKE2) to negotiate
// an authentication key, which is used to establish the connection.
class ThirdPartyAuthenticatorBase : public Authenticator {
 public:
  ThirdPartyAuthenticatorBase(const ThirdPartyAuthenticatorBase&) = delete;
  ThirdPartyAuthenticatorBase& operator=(const ThirdPartyAuthenticatorBase&) =
      delete;

  ~ThirdPartyAuthenticatorBase() override;

  // Authenticator interface.
  State state() const override;
  bool started() const override;
  RejectionReason rejection_reason() const override;
  void ProcessMessage(const jingle_xmpp::XmlElement* message,
                      base::OnceClosure resume_callback) override;
  std::unique_ptr<jingle_xmpp::XmlElement> GetNextMessage() override;
  const std::string& GetAuthKey() const override;
  std::unique_ptr<ChannelAuthenticator> CreateChannelAuthenticator()
      const override;

 protected:
  // XML tag names for third party authentication fields.
  static const jingle_xmpp::StaticQName kTokenUrlTag;
  static const jingle_xmpp::StaticQName kTokenScopeTag;
  static const jingle_xmpp::StaticQName kTokenTag;

  explicit ThirdPartyAuthenticatorBase(State initial_state);

  // Gives the message to the underlying authenticator for processing.
  void ProcessUnderlyingMessage(const jingle_xmpp::XmlElement* message,
                                base::OnceClosure resume_callback);

  // Processes the token-related elements of the message.
  virtual void ProcessTokenMessage(const jingle_xmpp::XmlElement* message,
                                   base::OnceClosure resume_callback) = 0;

  // Adds the token related XML elements to the message.
  virtual void AddTokenElements(jingle_xmpp::XmlElement* message) = 0;

  std::unique_ptr<Authenticator> underlying_;
  State token_state_;
  bool started_;
  RejectionReason rejection_reason_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_THIRD_PARTY_AUTHENTICATOR_BASE_H_
