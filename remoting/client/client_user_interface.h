// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CLIENT_USER_INTERFACE_H_
#define REMOTING_CLIENT_CLIENT_USER_INTERFACE_H_

#include <memory>
#include <string>

#include "remoting/protocol/connection_to_host.h"

namespace webrtc {
class DesktopSize;
class DesktopVector;
}  // namespace webrtc

namespace remoting {

namespace protocol {
class ClipboardStub;
class CursorShapeStub;
class ExtensionMessage;
class KeyboardLayoutStub;
class PairingResponse;
}  // namespace protocol

// ClientUserInterface is an interface that must be implemented by
// applications embedding the Chromoting client, to provide client's user
// interface.
//
// TODO(sergeyu): Cleanup this interface, see crbug.com/138108 .
class ClientUserInterface {
 public:
  virtual ~ClientUserInterface() {}

  // Record the update the state of the connection, updating the UI as needed.
  virtual void OnConnectionState(protocol::ConnectionToHost::State state,
                                 protocol::ErrorCode error) = 0;
  virtual void OnConnectionReady(bool ready) = 0;
  virtual void OnRouteChanged(const std::string& channel_name,
                              const protocol::TransportRoute& route) = 0;

  // Passes the final set of capabilities negotiated between the client and host
  // to the application.
  virtual void SetCapabilities(const std::string& capabilities) = 0;

  // Passes a pairing response message to the client.
  virtual void SetPairingResponse(
      const protocol::PairingResponse& pairing_response) = 0;

  // Deliver an extension message from the host to the client.
  virtual void DeliverHostMessage(
      const protocol::ExtensionMessage& message) = 0;

  // Notify the client about screen dimensions. The |size| is in physical
  // pixels.
  virtual void SetDesktopSize(const webrtc::DesktopSize& size,
                              const webrtc::DesktopVector& dpi) = 0;

  // Get the view's ClipboardStub implementation.
  virtual protocol::ClipboardStub* GetClipboardStub() = 0;

  // Get the view's CursorShapeStub implementation.
  virtual protocol::CursorShapeStub* GetCursorShapeStub() = 0;

  // Get the view's KeyboardLayoutStub implementation.
  virtual protocol::KeyboardLayoutStub* GetKeyboardLayoutStub() = 0;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CLIENT_USER_INTERFACE_H_
