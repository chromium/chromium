// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CLIENT_SESSION_CONTROL_H_
#define REMOTING_HOST_CLIENT_SESSION_CONTROL_H_

#include "remoting/protocol/errors.h"
#include "ui/events/event.h"

namespace webrtc {
class DesktopVector;
}  // namespace webrtc

namespace remoting {

namespace protocol {
class VideoLayout;
}  // namespace protocol

// Allows the desktop environment to disconnect the client session and
// to control the remote input handling (i.e. disable, enable, and pause
// temporarily if the local mouse movements are detected).
class ClientSessionControl {
 public:
  virtual ~ClientSessionControl() = default;

  // Returns the authenticated JID of the client session.
  virtual const std::string& client_jid() const = 0;

  // Disconnects the client session, tears down transport resources and stops
  // scheduler components.
  virtual void DisconnectSession(protocol::ErrorCode error) = 0;

  // Called when local mouse or touch movement is detected.
  virtual void OnLocalPointerMoved(const webrtc::DesktopVector& position,
                                   ui::EventType type) = 0;

  // Called when a local key press or release is detected.
  virtual void OnLocalKeyPressed(uint32_t usb_keycode) = 0;

  // Disables or enables the remote input in the client session.
  virtual void SetDisableInputs(bool disable_inputs) = 0;

  // Called when the host desktop displays are changed.
  // TODO(yuweih): Move this to ClientSessionEvents.
  virtual void OnDesktopDisplayChanged(
      std::unique_ptr<protocol::VideoLayout> layout) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CLIENT_SESSION_CONTROL_H_
