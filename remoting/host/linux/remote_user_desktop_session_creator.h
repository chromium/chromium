// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_REMOTE_USER_DESKTOP_SESSION_CREATOR_H_
#define REMOTING_HOST_LINUX_REMOTE_USER_DESKTOP_SESSION_CREATOR_H_

#include <string>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "remoting/base/loggable.h"

namespace remoting {

// Interface for asking the system to create a graphical session for a local
// user without interactive authentication, so that a remote client can connect
// to it. Callers must check that the user is allowed to have such a session
// before calling CreateSession(), and must wait for the session to show up in
// logind, since it can still fail to start after the request is accepted.
class RemoteUserDesktopSessionCreator {
 public:
  using Callback = base::OnceCallback<void(base::expected<void, Loggable>)>;

  virtual ~RemoteUserDesktopSessionCreator() = default;

  // Requests a graphical session for `username`, which must be the canonical
  // passwd name. Runs `callback` with success if the request has been
  // accepted, or with an error otherwise. `callback` may run after the caller
  // has stopped waiting for the session, or be dropped without being run if
  // `this` is destroyed first.
  virtual void CreateSession(const std::string& username,
                             Callback callback) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_REMOTE_USER_DESKTOP_SESSION_CREATOR_H_
