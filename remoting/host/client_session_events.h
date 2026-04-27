// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CLIENT_SESSION_EVENTS_H_
#define REMOTING_HOST_CLIENT_SESSION_EVENTS_H_

#include <stdint.h>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "remoting/host/mojom/chromoting_host_services.mojom-forward.h"
#include "remoting/host/mojom/remote_security_key.mojom.h"

namespace remoting {

// Allow the desktop environment to trigger events on the client session.
class ClientSessionEvents {
 public:
  virtual ~ClientSessionEvents() = default;

  // Called when the desktop session agent is attached.
  // This method is only called on platforms that use a multi-process
  // architecture (i.e. ones where the desktop being remoted can change).
  virtual void OnDesktopAttached() = 0;

  // Called when the desktop session agent is detached from the previous desktop
  // session. This method is only called on platforms that use a multi-process
  // architecture (i.e. ones where the desktop being remoted can change).
  virtual void OnDesktopDetached() = 0;

  // Called when a new security key connection has been established and passes
  // the SecurityKeyForwarder receiver to it.
  virtual void OnSecurityKeyConnection(
      mojo::PendingReceiver<mojom::SecurityKeyForwarder> receiver) = 0;

  // Called when a ChromotingSessionServices client has connected and passes the
  // ChromotingSessionServices receiver to it.
  virtual void OnSessionServicesClientConnected(
      mojo::PendingReceiver<mojom::ChromotingSessionServices> receiver) = 0;

 protected:
  ClientSessionEvents() = default;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CLIENT_SESSION_EVENTS_H_
