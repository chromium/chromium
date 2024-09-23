// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PAIRING_AUTHENTICATOR_BASE_H_
#define REMOTING_PROTOCOL_PAIRING_AUTHENTICATOR_BASE_H_

#include "base/memory/weak_ptr.h"
#include "remoting/protocol/authenticator.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting::protocol {

// The pairing authenticator builds on top of V2Authenticator to add
// support for PIN-less authentication via device pairing:
//
// * If a client device is already paired, it includes in the initial
//   authentication message a Client Id and the first SPAKE message
//   using the Paired Secret and HMAC_SHA256.
// * If the host recognizes the Client Id, it looks up the corresponding
//   Paired Secret and continue the SPAKE exchange.
// * If it does not recognize the Client Id, it initiates a SPAKE exchange
//   with HMAC_SHA256 using the PIN as the shared secret. The initial
//   message of this exchange includes an an error message, which
//   informs the client that the PIN-less connection failed and causes
//   it to prompt the user for a PIN to use for authentication
//   instead.
// * If, at any point, the SPAKE exchange fails with the Paired Secret,
//   the endpoint that detects the failure initiates a new SPAKE exchange
//   using the PIN, and includes an error message to instruct the peer
//   to do likewise.
//
// If a client device is not already paired, but supports pairing, then
// the V2Authenticator is used instead of this class. Only the method name
// differs, which the client uses to determine that pairing should be offered
// to the user (see NegotiatingHostAuthenticator::CreateAuthenticator and
// NegotiatingClientAuthenticator::CreateAuthenticatorForCurrentMethod).
class PairingAuthenticatorBase : public Authenticator {
 public:
  PairingAuthenticatorBase();

  PairingAuthenticatorBase(const PairingAuthenticatorBase&) = delete;
  PairingAuthenticatorBase& operator=(const PairingAuthenticatorBase&) = delete;

  ~PairingAuthenticatorBase() override;

  // Authenticator interface.
  CredentialsType credentials_type() const override;
  const Authenticator& implementing_authenticator() const override;
  State state() const override;
  bool started() const override;
  RejectionReason rejection_reason() const override;
  void ProcessMessage(const jingle_xmpp::XmlElement* message,
                      base::OnceClosure resume_callback) override;
  std::unique_ptr<jingle_xmpp::XmlElement> GetNextMessage() override;
  const std::string& GetAuthKey() const override;
  const SessionPolicies* GetSessionPolicies() const override;
  std::unique_ptr<ChannelAuthenticator> CreateChannelAuthenticator()
      const override;

 protected:
  // Create a Spake2 authenticator in the specified state, prompting the user
  // for the PIN first if necessary.
  virtual void CreateSpakeAuthenticatorWithPin(
      State initial_state,
      base::OnceClosure resume_callback) = 0;

  // A non-fatal error message that derived classes should set in order to
  // cause the peer to be notified that pairing has failed and that it should
  // fall back on PIN authentication. This string need not be human-readable,
  // nor is it currently used other than being logged.
  std::string error_message_;

  // The underlying SPAKE2 authenticator, created with either the PIN or the
  // Paired Secret by the derived class.
  std::unique_ptr<Authenticator> spake2_authenticator_;

  // Derived classes must set this to True if the underlying authenticator is
  // using the Paired Secret.
  bool using_paired_secret_ = false;

 private:
  // Helper methods for ProcessMessage() and GetNextMessage().
  void MaybeAddErrorMessage(jingle_xmpp::XmlElement* message);
  bool HasErrorMessage(const jingle_xmpp::XmlElement* message) const;
  void CheckForFailedSpakeExchange(base::OnceClosure resume_callback);

  base::WeakPtrFactory<PairingAuthenticatorBase> weak_factory_{this};
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_PAIRING_AUTHENTICATOR_BASE_H_
