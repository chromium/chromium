// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMOTING_HOST_SERVICES_PROVIDER_H_
#define REMOTING_HOST_CHROMOTING_HOST_SERVICES_PROVIDER_H_

#include "base/functional/callback_forward.h"

namespace remoting {

namespace mojom {
class ChromotingSessionServices;
}  // namespace mojom

// Interface that provides ChromotingHostServices APIs.
class ChromotingHostServicesProvider {
 public:
  virtual ~ChromotingHostServicesProvider() = default;

  // Gets the ChromotingHostServices. Returns nullptr if the interface cannot be
  // provided at the moment.
  // Always null-check before using it, as nullptr will be returned if the
  // connection could not be established.
  // Note that when the session is not remoted, you will still get a callable
  // interface, but all outgoing IPCs will be silently dropped, and any pending
  // receivers/remotes/message pipes sent will be closed. In this case, if you
  // have set the disconnect handler, it will be called soon after this function
  // is called.
  virtual mojom::ChromotingSessionServices* GetSessionServices() const = 0;

  // Sets a disconnect handler, which will be run when the receiver of
  // ChromotingSessionServices disconnects, or the IPC channel disconnects.
  virtual void set_disconnect_handler(base::OnceClosure disconnect_handler) = 0;

 protected:
  ChromotingHostServicesProvider() = default;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMOTING_HOST_SERVICES_PROVIDER_H_
