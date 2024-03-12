// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The purpose of SessionManager is to facilitate creation of chromotocol
// sessions. Both host and client use it to establish chromotocol
// sessions. JingleChromotocolServer implements this inteface using
// libjingle.
//
// OUTGOING SESSIONS
// Connect() must be used to create new session to a remote host. The
// returned session is initially in INITIALIZING state. Later state is
// changed to CONNECTED if the session is accepted by the host or
// CLOSED if the session is rejected.
//
// INCOMING SESSIONS
// The IncomingSessionCallback is called when a client attempts to connect.
// The callback function decides whether the session should be accepted or
// rejected.
//
// AUTHENTICATION
// Implementations of the Session and SessionManager interfaces
// delegate authentication to an Authenticator implementation. For
// incoming connections authenticators are created using an
// AuthenticatorFactory set via the set_authenticator_factory()
// method. For outgoing sessions authenticator must be passed to the
// Connect() method. The Session's state changes to AUTHENTICATED once
// authentication succeeds.
//
// SESSION OWNERSHIP AND SHUTDOWN
// The SessionManager must not be closed or destroyed before all sessions
// created by that SessionManager are destroyed. Caller owns Sessions
// created by a SessionManager (except rejected
// sessions). The SignalStrategy must outlive the SessionManager.
//
// PROTOCOL VERSION NEGOTIATION
// When client connects to a host it sends a session-initiate stanza with list
// of supported configurations for each channel. If the host decides to accept
// session, then it selects configuration that is supported by both sides
// and then replies with the session-accept stanza that contans selected
// configuration. The configuration specified in the session-accept is used
// for the session.
//
// The CandidateSessionConfig class represents list of configurations
// supported by an endpoint. The |candidate_config| argument in the Connect()
// specifies configuration supported on the client side. When the host receives
// session-initiate stanza, the IncomingSessionCallback is called. The
// configuration sent in the session-intiate staza is available via
// ChromotocolConnection::candidate_config(). If an incoming session is
// being accepted then the IncomingSessionCallback callback function must
// select session configuration and then set it with Session::set_config().

#ifndef REMOTING_PROTOCOL_SESSION_MANAGER_H_
#define REMOTING_PROTOCOL_SESSION_MANAGER_H_

#include <string>

#include "base/functional/callback.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/session_observer.h"

namespace remoting {

class SignalingAddress;
class SignalStrategy;

namespace protocol {

class Authenticator;
class AuthenticatorFactory;

// Generic interface for Chromoting session manager.
class SessionManager {
 public:
  enum IncomingSessionResponse {
    // Accept the session.
    ACCEPT,

    // Reject the session because the host is currently disabled due
    // to previous login attempts.
    OVERLOAD,

    // Reject the session because the client is not allowed to connect
    // to the host.
    DECLINE,
  };

  // Callback used to accept incoming connections. If the host decides to accept
  // the session it should set the |response| to ACCEPT. Otherwise it should set
  // it to DECLINE, or INCOMPATIBLE. INCOMPATIBLE indicates that the session has
  // incompatible configuration, and cannot be accepted. If the callback accepts
  // the |session| then it must also set configuration for the |session| using
  // Session::set_config(). The callback must take ownership of the |session| if
  // it ACCEPTs it.
  typedef base::RepeatingCallback<void(Session* session,
                                       IncomingSessionResponse* response)>
      IncomingSessionCallback;

  SessionManager() {}

  SessionManager(const SessionManager&) = delete;
  SessionManager& operator=(const SessionManager&) = delete;

  virtual ~SessionManager() {}

  // Starts accepting incoming connections.
  virtual void AcceptIncoming(
      const IncomingSessionCallback& incoming_session_callback) = 0;

  // Sets local protocol configuration to be used when negotiating outgoing and
  // incoming connections.
  virtual void set_protocol_config(
      std::unique_ptr<CandidateSessionConfig> config) = 0;

  // Creates a new outgoing session.
  //
  // |peer_address| - full SignalingAddress to connect to.
  // |authenticator| - client authenticator for the session.
  virtual std::unique_ptr<Session> Connect(
      const SignalingAddress& peer_address,
      std::unique_ptr<Authenticator> authenticator) = 0;

  // Set authenticator factory that should be used to authenticate
  // incoming connection. No connections will be accepted if
  // authenticator factory isn't set. Must not be called more than
  // once per SessionManager because it may not be safe to delete
  // factory before all authenticators it created are deleted.
  virtual void set_authenticator_factory(
      std::unique_ptr<AuthenticatorFactory> authenticator_factory) = 0;

  // Adds a session observer. Discarding the returned subscription will result
  // in the removal of the observer.
  [[nodiscard]] virtual SessionObserver::Subscription AddSessionObserver(
      SessionObserver* observer) = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_SESSION_MANAGER_H_
