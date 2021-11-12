// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_SESSION_CONNECTOR_H_
#define REMOTING_HOST_DESKTOP_SESSION_CONNECTOR_H_

#include "base/process/process.h"
#include "ipc/ipc_channel_handle.h"

namespace remoting {

class DesktopSessionProxy;
class ScreenResolution;

// Provides a way to connect a terminal (i.e. a remote client) with a desktop
// session (i.e. the screen, keyboard and the rest).
class DesktopSessionConnector {
 public:
  DesktopSessionConnector() {}

  DesktopSessionConnector(const DesktopSessionConnector&) = delete;
  DesktopSessionConnector& operator=(const DesktopSessionConnector&) = delete;

  virtual ~DesktopSessionConnector() {}

  // Requests the daemon process to create a desktop session and associates
  // |desktop_session_proxy| with it. |desktop_session_proxy| must be
  // disconnected from the desktop session (see DisconnectTerminal()) before it
  // can be deleted.
  virtual void ConnectTerminal(
      DesktopSessionProxy* desktop_session_proxy,
      const ScreenResolution& resolution,
      bool virtual_terminal) = 0;

  // Requests the daemon process disconnect |desktop_session_proxy| from
  // the associated desktop session.
  virtual void DisconnectTerminal(
      DesktopSessionProxy* desktop_session_proxy) = 0;

  // Changes the screen resolution of the desktop session.
  virtual void SetScreenResolution(
      DesktopSessionProxy* desktop_session_proxy,
      const ScreenResolution& resolution) = 0;

  // Notifies the network process that |terminal_id| is now attached to
  // a desktop integration process. |session_id| is the id of the desktop
  // session being attached. |desktop_pipe| is the client end of the pipe opened
  // by the desktop process.
  virtual void OnDesktopSessionAgentAttached(
      int terminal_id,
      int session_id,
      const IPC::ChannelHandle& desktop_pipe) = 0;

  // Notifies the network process that the daemon has disconnected the desktop
  // session from the associated desktop environment.
  virtual void OnTerminalDisconnected(int terminal_id) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_SESSION_CONNECTOR_H_
