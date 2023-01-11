// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_AUTHENTICATOR_H_
#define REMOTING_PROTOCOL_AUTHENTICATOR_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"

namespace jingle_xmpp {
class XmlElement;
}  // namespace jingle_xmpp

namespace remoting::protocol {

class Authenticator;
class ChannelAuthenticator;

// Authenticator is an abstract interface for authentication protocol
// implementations. Different implementations of this interface may be used on
// each side of the connection depending of type of the auth protocol. Client
// and host will repeatedly call their Authenticators and deliver the messages
// they generate, until successful authentication is reported.
//
// Authenticator may exchange multiple messages before session is authenticated.
// Each message sent/received by an Authenticator is delivered either in a
// session description inside session-initiate and session-accept messages or in
// a session-info message. Session-info messages are used only if authenticators
// need to exchange more than one message.
class Authenticator {
 public:
  // Allowed state transitions:
  // When ProcessMessage() is called:
  //    WAITING_MESSAGE -> MESSAGE_READY
  //    WAITING_MESSAGE -> ACCEPTED
  //    WAITING_MESSAGE -> REJECTED
  //    WAITING_MESSAGE -> PROCESSING_MESSAGE
  // After asynchronous message processing finishes:
  //    PROCESSING_MESSAGE -> MESSAGE_READY
  //    PROCESSING_MESSAGE -> ACCEPTED
  //    PROCESSING_MESSAGE -> REJECTED
  // When GetNextMessage() is called:
  //    MESSAGE_READY -> WAITING_MESSAGE
  //    MESSAGE_READY -> ACCEPTED
  enum State {
    // Waiting for the next message from the peer.
    WAITING_MESSAGE,

    // Next message is ready to be sent to the peer.
    MESSAGE_READY,

    // Session is authenticated successfully.
    ACCEPTED,

    // Session is rejected.
    REJECTED,

    // Asynchronously processing the last message from the peer.
    PROCESSING_MESSAGE,
  };

  enum class RejectionReason {
    // The account credentials were not valid (i.e. incorrect PIN).
    INVALID_CREDENTIALS,

    // The client JID was not valid (i.e. violated a policy or was malformed).
    INVALID_ACCOUNT_ID,

    // Generic error used when something goes wrong establishing a session.
    PROTOCOL_ERROR,

    // Session was rejected by the user (i.e. via the confirmation dialog).
    REJECTED_BY_USER,

    // Multiple, valid connection requests were received for the same session.
    TOO_MANY_CONNECTIONS,

    // The client is not authorized to connect to this device due to a policy
    // defined by the third party auth service. No denial reason was given.
    AUTHZ_POLICY_CHECK_FAILED,

    // The client is not authorized to connect to this device based on their
    // current location due to a policy defined by the third party auth service.
    LOCATION_AUTHZ_POLICY_CHECK_FAILED,

    // The remote user is not authorized to access this machine. This is a
    // generic authz error and is not related to third-party auth.
    UNAUTHORIZED_ACCOUNT,
  };

  // Callback used for layered Authenticator implementations, particularly
  // third-party and pairing authenticators. They use this callback to create
  // base SPAKE2 authenticators.
  typedef base::RepeatingCallback<std::unique_ptr<Authenticator>(
      const std::string& shared_secret,
      Authenticator::State initial_state)>
      CreateBaseAuthenticatorCallback;

  // Returns true if |message| is an Authenticator message.
  static bool IsAuthenticatorMessage(const jingle_xmpp::XmlElement* message);

  // Creates an empty Authenticator message, owned by the caller.
  static std::unique_ptr<jingle_xmpp::XmlElement>
  CreateEmptyAuthenticatorMessage();

  // Finds Authenticator message among child elements of |message|, or
  // returns nullptr otherwise.
  static const jingle_xmpp::XmlElement* FindAuthenticatorMessage(
      const jingle_xmpp::XmlElement* message);

  Authenticator() {}
  virtual ~Authenticator() {}

  // Returns current state of the authenticator.
  virtual State state() const = 0;

  // Returns whether authentication has started. The chromoting host uses this
  // method to start the back off process to prevent malicious clients from
  // guessing the PIN by spamming the host with auth requests.
  virtual bool started() const = 0;

  // Returns rejection reason. Can be called only when in REJECTED state.
  virtual RejectionReason rejection_reason() const = 0;

  // Called in response to incoming message received from the peer.
  // Should only be called when in WAITING_MESSAGE state. Caller retains
  // ownership of |message|. |resume_callback| will be called when processing is
  // finished. The implementation must guarantee that |resume_callback| is not
  // called after the Authenticator is destroyed.
  virtual void ProcessMessage(const jingle_xmpp::XmlElement* message,
                              base::OnceClosure resume_callback) = 0;

  // Must be called when in MESSAGE_READY state. Returns next
  // authentication message that needs to be sent to the peer.
  virtual std::unique_ptr<jingle_xmpp::XmlElement> GetNextMessage() = 0;

  // Returns the auth key received as result of the authentication handshake.
  virtual const std::string& GetAuthKey() const = 0;

  // Creates new authenticator for a channel. Can be called only in
  // the ACCEPTED state.
  virtual std::unique_ptr<ChannelAuthenticator> CreateChannelAuthenticator()
      const = 0;
};

// Factory for Authenticator instances.
class AuthenticatorFactory {
 public:
  AuthenticatorFactory() {}
  virtual ~AuthenticatorFactory() {}

  // Called when session-initiate stanza is received to create
  // authenticator for the new session. |first_message| specifies
  // authentication part of the session-initiate stanza so that
  // appropriate type of Authenticator can be chosen for the session
  // (useful when multiple authenticators are supported). Returns nullptr
  // if the |first_message| is invalid and the session should be
  // rejected. ProcessMessage() should be called with |first_message|
  // for the result of this method.
  virtual std::unique_ptr<Authenticator> CreateAuthenticator(
      const std::string& local_jid,
      const std::string& remote_jid) = 0;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_AUTHENTICATOR_H_
