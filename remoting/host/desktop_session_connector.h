// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_SESSION_CONNECTOR_H_
#define REMOTING_HOST_DESKTOP_SESSION_CONNECTOR_H_

#include <string_view>

#include "base/process/process.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/base/errors.h"
#include "remoting/base/source_location.h"

namespace remoting {

class DesktopSessionProxy;
class ScreenResolution;

// Provides a way to connect a terminal (i.e. a remote client) with a desktop
// session (i.e. the screen, keyboard, and the rest).
class DesktopSessionConnector {
 public:
  DesktopSessionConnector() = default;

  DesktopSessionConnector(const DesktopSessionConnector&) = delete;
  DesktopSessionConnector& operator=(const DesktopSessionConnector&) = delete;

  virtual ~DesktopSessionConnector() = default;

  // Requests the daemon process to create a desktop session and associates
  // |desktop_session_proxy| with it. |desktop_session_proxy| must be
  // disconnected from the desktop session (see DisconnectTerminal()) before it
  // can be deleted.
  virtual void ConnectTerminal(DesktopSessionProxy* desktop_session_proxy,
                               const ScreenResolution& resolution,
                               bool is_curtained) = 0;

  // Requests the daemon process disconnect |desktop_session_proxy| from
  // the associated desktop session.
  virtual void DisconnectTerminal(
      DesktopSessionProxy* desktop_session_proxy) = 0;

  // Changes the screen resolution of the desktop session.
  virtual void SetScreenResolution(DesktopSessionProxy* desktop_session_proxy,
                                   const ScreenResolution& resolution) = 0;


  // If set to a non-empty value, the login user of the desktop session must
  // match `username`. This can only be set when there are no active
  // connections.
  virtual void SetRequiredUsername(std::string_view username) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_SESSION_CONNECTOR_H_
