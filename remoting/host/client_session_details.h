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
};

}  // namespace remoting

#endif  // REMOTING_HOST_CLIENT_SESSION_DETAILS_H_
