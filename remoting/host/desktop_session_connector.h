// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_SESSION_CONNECTOR_H_
#define REMOTING_HOST_DESKTOP_SESSION_CONNECTOR_H_

#include "base/process/process.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/host/mojom/remoting_host.mojom.h"

namespace remoting {

class DesktopSessionProxy;
class ScreenResolution;

// Provides a way to connect a terminal (i.e. a remote client) with a desktop
// session (i.e. the screen, keyboard, and the rest).
class DesktopSessionConnector : public mojom::DesktopSessionConnectionEvents {
 public:
  DesktopSessionConnector() = default;

  DesktopSessionConnector(const DesktopSessionConnector&) = delete;
  DesktopSessionConnector& operator=(const DesktopSessionConnector&) = delete;

  ~DesktopSessionConnector() override = default;

  // Requests the daemon process to create a desktop session and associates
  // |desktop_session_proxy| with it. |desktop_session_proxy| must be
  // disconnected from the desktop session (see DisconnectTerminal()) before it
  // can be deleted.
  virtual void ConnectTerminal(DesktopSessionProxy* desktop_session_proxy,
                               const ScreenResolution& resolution,
                               bool virtual_terminal) = 0;

  // Requests the daemon process disconnect |desktop_session_proxy| from
  // the associated desktop session.
  virtual void DisconnectTerminal(
      DesktopSessionProxy* desktop_session_proxy) = 0;

  // Changes the screen resolution of the desktop session.
  virtual void SetScreenResolution(DesktopSessionProxy* desktop_session_proxy,
                                   const ScreenResolution& resolution) = 0;

  // Binds a receiver to allow the DesktopSessionConnector instance to receive
  // events related to changes in the desktop session. Returns True if |handle|
  // was successfully bound, otherwise false.
  virtual bool BindConnectionEventsReceiver(
      mojo::ScopedInterfaceEndpointHandle handle) = 0;

#if !BUILDFLAG(IS_WIN)
  // Notifies the network process that |terminal_id| is now attached to
  // a desktop integration process. |session_id| is the id of the desktop
  // session being attached. |desktop_pipe| is the client end of the pipe opened
  // by the desktop process.
  virtual void OnDesktopSessionAgentAttached(
      int terminal_id,
      int session_id,
      mojo::ScopedMessagePipeHandle desktop_pipe) = 0;

  // Notifies the network process that the daemon has disconnected the desktop
  // session from the associated desktop environment.
  virtual void OnTerminalDisconnected(int terminal_id) = 0;
#endif
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_SESSION_CONNECTOR_H_
