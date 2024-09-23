// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_SESSION_OBSERVER_H_
#define REMOTING_PROTOCOL_SESSION_OBSERVER_H_

#include "base/functional/callback_helpers.h"
#include "base/observer_list_types.h"
#include "remoting/protocol/session.h"

namespace remoting::protocol {

// An interface for observing session state changes. It is similar to
// Session::EventHandler. The differences are:
//
// 1. The observer is registered on SessionManager, rather than the Session.
// 2. Implementations will observe state changes from multiple sessions. The
//    |session| parameter will tell you which session's state has changed.
// 3. Unlike Session::EventHandler, the SessionObserver is not supposed to
//    modify the Session. It is only supposed to observe the changes.
//
// An example of the observer is a logger that needs to know the error code and
// the authentication type when the session state has changed.
class SessionObserver : public base::CheckedObserver {
 public:
  using Subscription = base::ScopedClosureRunner;

  // Called after session state has changed. The observer must not destroy the
  // session from within the method.
  virtual void OnSessionStateChange(const Session& session,
                                    Session::State state) = 0;

 protected:
  SessionObserver() = default;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_SESSION_OBSERVER_H_
