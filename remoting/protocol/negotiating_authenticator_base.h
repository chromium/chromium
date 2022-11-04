// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_NEGOTIATING_AUTHENTICATOR_BASE_H_
#define REMOTING_PROTOCOL_NEGOTIATING_AUTHENTICATOR_BASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "remoting/protocol/authenticator.h"

namespace jingle_xmpp {
struct StaticQName;
}  // namespace jingle_xmpp

namespace remoting::protocol {

// This class provides the common base for a meta-authenticator that allows
// clients and hosts that support multiple authentication methods to negotiate a
// method to use.
//
// The typical flow is:
//  * Client sends a message to host with its supported methods.
//      (clients may additionally pick a method and send its first message).
//  * Host picks a method and sends its first message (if any).
//      (if a message for that method was sent by the client, it is processed).
//  * Client creates the authenticator selected by the host. If the method
//      starts with a message from the host, it is processed.
//  * Client and host exchange messages until the authentication is ACCEPTED or
//      REJECTED.
//
// The details:
//  * CreateAuthenticator() may be asynchronous (i.e. require user interaction
//      to determine initial parameters, like PIN). This happens inside
//      ProcessMessage, so to the outside this behaves like any asynchronous
//      message processing. Internally, CreateAuthenticator() receives a
//      callback, that will resume the authentication once the authenticator is
//      created. If there is already a message to be processed by the new
//      authenticator, this callback includes a call to the underlying
//      ProcessMessage().
//  * Some authentication methods may have a specific starting direction (e.g.
//      host always sends the first message), while others are versatile (e.g.
//      SPAKE, where either side can send the first message). When an
//      authenticator is created, it is given a preferred initial state, which
//      the authenticator may ignore.
//  * If the new authenticator state doesn't match the preferred one,
//      the NegotiatingAuthenticator deals with that, by sending an empty
//      <authenticator> stanza if the method has no message to send, and
//      ignoring such empty messages on the receiving end.
//  * The client may optimistically pick a method on its first message (assuming
//      it doesn't require user interaction to start). If the host doesn't
//      support that method, it will just discard that message, and choose
//      another method from the client's supported methods list.
//  * The host never sends its own supported methods back to the client, so once
//      the host picks a method from the client's list, it's final.
//  * Any change in this class must maintain compatibility between any version
//      mix of webapp, client plugin and host, for both Me2Me and IT2Me.
class NegotiatingAuthenticatorBase : public Authenticator {
 public:
  // Method represents an authentication algorithm.
  enum class Method {
    INVALID,

    // SPAKE2 with P224 using access code in plain-text. Used for It2Me.
    // TODO(sergeyu): Remove and use SHARED_SECRET_SPAKE2_CURVE25519 once
    // the population of M50 hosts (which require this for IT2Me) is
    // sufficiently low: crbug.com/607643.
    SHARED_SECRET_PLAIN_SPAKE2_P224,

    // SPAKE2 PIN or access code hashed with host_id using HMAC-SHA256.
    SHARED_SECRET_SPAKE2_P224,
    SHARED_SECRET_SPAKE2_CURVE25519,

    // SPAKE2 using shared pairing secret. Falls back to PIN-based
    // authentication when pairing fails.
    PAIRED_SPAKE2_P224,
    PAIRED_SPAKE2_CURVE25519,

    // Authentication using third-party authentication server.
    // SPAKE2 with P224 using shared pairing secret. Falls back to PIN-based
    // authentication when it fails to authenticate using paired secret.
    THIRD_PARTY_SPAKE2_P224,
    THIRD_PARTY_SPAKE2_CURVE25519,
  };

  NegotiatingAuthenticatorBase(const NegotiatingAuthenticatorBase&) = delete;
  NegotiatingAuthenticatorBase& operator=(const NegotiatingAuthenticatorBase&) =
      delete;

  ~NegotiatingAuthenticatorBase() override;

  // Authenticator interface.
  State state() const override;
  bool started() const override;
  RejectionReason rejection_reason() const override;
  const std::string& GetAuthKey() const override;
  std::unique_ptr<ChannelAuthenticator> CreateChannelAuthenticator()
      const override;

  // Calls |current_authenticator_| to process |message|, passing the supplied
  // |resume_callback|.
  void ProcessMessageInternal(const jingle_xmpp::XmlElement* message,
                              base::OnceClosure resume_callback);

 protected:
  friend class NegotiatingAuthenticatorTest;

  static const jingle_xmpp::StaticQName kMethodAttributeQName;
  static const jingle_xmpp::StaticQName kSupportedMethodsAttributeQName;
  static const char kSupportedMethodsSeparator;

  static const jingle_xmpp::StaticQName kPairingInfoTag;
  static const jingle_xmpp::StaticQName kClientIdAttribute;

  // Parses a string that defines an authentication method. Returns
  // Method::INVALID if the string is invalid.
  static Method ParseMethodString(const std::string& value);

  // Returns string representation of |method|.
  static std::string MethodToString(Method method);

  explicit NegotiatingAuthenticatorBase(Authenticator::State initial_state);

  void AddMethod(Method method);

  // Updates |state_| to reflect the current underlying authenticator state.
  // |resume_callback| is called after the state is updated.
  void UpdateState(base::OnceClosure resume_callback);

  // Gets the next message from |current_authenticator_|, if any, and fills in
  // the 'method' tag with |current_method_|.
  virtual std::unique_ptr<jingle_xmpp::XmlElement> GetNextMessageInternal();

  std::vector<Method> methods_;
  Method current_method_ = Method::INVALID;
  std::unique_ptr<Authenticator> current_authenticator_;
  State state_;
  RejectionReason rejection_reason_ = RejectionReason::INVALID_CREDENTIALS;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_NEGOTIATING_AUTHENTICATOR_BASE_H_
