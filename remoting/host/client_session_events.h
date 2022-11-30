// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CLIENT_SESSION_EVENTS_H_
#define REMOTING_HOST_CLIENT_SESSION_EVENTS_H_

#include <stdint.h>

namespace remoting {

// Allow the desktop environment to trigger events on the client session.
class ClientSessionEvents {
 public:
  virtual ~ClientSessionEvents() = default;

  // Called when the desktop session agent is attached to the given session ID.
  // This method is only called on platforms that use a multi-process
  // architecture (i.e. ones where the desktop being remoted can change).
  virtual void OnDesktopAttached(uint32_t session_id) = 0;

  // Called when the desktop session agent is detached from the previous desktop
  // session. This method is only called on platforms that use a multi-process
  // architecture (i.e. ones where the desktop being remoted can change).
  virtual void OnDesktopDetached() = 0;

 protected:
  ClientSessionEvents() = default;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CLIENT_SESSION_EVENTS_H_
