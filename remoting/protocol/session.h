// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_SESSION_H_
#define REMOTING_PROTOCOL_SESSION_H_

#include <memory>
#include <string>

#include "remoting/protocol/errors.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/transport.h"

namespace remoting::protocol {

class Authenticator;
class SessionPlugin;
class Transport;

// Session is responsible for initializing and authenticating both incoming and
// outgoing connections. It uses TransportInfoSink interface to pass
// transport-info messages to the transport.
class Session {
 public:
  enum State {
    // Created, but not connecting yet.
    INITIALIZING,

    // Sent session-initiate, but haven't received session-accept.
    CONNECTING,

    // Received session-initiate, but haven't sent session-accept.
    ACCEPTING,

    // Session has been accepted and is pending authentication.
    ACCEPTED,

    // Session has started authenticating.
    AUTHENTICATING,

    // Session has been connected and authenticated.
    AUTHENTICATED,

    // Session has been closed.
    CLOSED,

    // Connection has failed.
    FAILED,
  };

  class EventHandler {
   public:
    EventHandler() = default;
    virtual ~EventHandler() = default;

    // Called after session state has changed. It is safe to destroy
    // the session from within the handler if |state| is AUTHENTICATING
    // or CLOSED or FAILED.
    virtual void OnSessionStateChange(State state) = 0;
  };

  Session() = default;

  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;

  virtual ~Session() = default;

  // Set event handler for this session. |event_handler| must outlive
  // this object.
  virtual void SetEventHandler(EventHandler* event_handler) = 0;

  // Returns error code for a failed session.
  virtual ErrorCode error() const = 0;

  // JID of the other side.
  virtual const std::string& jid() = 0;

  // Protocol configuration. Can be called only after session has been accepted.
  // Returned pointer is valid until connection is closed.
  virtual const SessionConfig& config() = 0;

  virtual const Authenticator& authenticator() const = 0;

  // Sets Transport to be used by the session. Must be called before the
  // session becomes AUTHENTICATED. The transport must outlive the session.
  virtual void SetTransport(Transport* transport) = 0;

  // Closes connection. EventHandler is guaranteed not to be called after this
  // method returns. |error| specifies the error code in case when the session
  // is being closed due to an error.
  virtual void Close(ErrorCode error) = 0;

  // Adds a SessionPlugin to handle attachments. To ensure plugin attachments
  // are processed correctly for session-initiate message, this function must be
  // called immediately after SessionManager::Connect() for outgoing connections
  // or in the IncomingSessionCallback handler for incoming connections.
  virtual void AddPlugin(SessionPlugin* plugin) = 0;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_SESSION_H_
