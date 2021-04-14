// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface of a client that receives commands from a Chromoting host.
//
// This interface is responsible for a subset of control messages sent to
// the Chromoting client.

#ifndef REMOTING_PROTOCOL_CLIENT_STUB_H_
#define REMOTING_PROTOCOL_CLIENT_STUB_H_

#include "base/macros.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/cursor_shape_stub.h"
#include "remoting/protocol/keyboard_layout_stub.h"

namespace remoting {
namespace protocol {

class Capabilities;
class ExtensionMessage;
class PairingResponse;
class TransportInfo;
class VideoLayout;

class ClientStub : public ClipboardStub,
                   public CursorShapeStub,
                   public KeyboardLayoutStub {
 public:
  ClientStub() {}
  ~ClientStub() override {}

  // Passes the set of capabilities supported by the host to the client.
  virtual void SetCapabilities(const Capabilities& capabilities) = 0;

  // Passes a pairing response message to the client.
  virtual void SetPairingResponse(const PairingResponse& pairing_response) = 0;

  // Deliver an extension message from the host to the client.
  virtual void DeliverHostMessage(const ExtensionMessage& message) = 0;

  // Sets video layout.
  virtual void SetVideoLayout(const VideoLayout& video_layout) = 0;

  // Passes the host's transport info to the client.
  virtual void SetTransportInfo(const TransportInfo& transport_info) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientStub);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CLIENT_STUB_H_
