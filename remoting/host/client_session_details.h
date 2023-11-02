// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CLIENT_SESSION_DETAILS_H_
#define REMOTING_HOST_CLIENT_SESSION_DETAILS_H_

#include <cstdint>

namespace remoting {

class ClientSessionControl;

// Provides ClientSession control and state information to HostExtensions.
class ClientSessionDetails {
 public:
  virtual ~ClientSessionDetails() {}

  // Returns a ClientSessionControl interface pointer used to interact with the
  // current session.
  virtual ClientSessionControl* session_control() = 0;

  // Returns the id of the current desktop session being remoted.  If no session
  // exists, UINT32_MAX is returned.
  // Note: The return value should never be cached as it can change.
  virtual uint32_t desktop_session_id() const = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CLIENT_SESSION_DETAILS_H_
