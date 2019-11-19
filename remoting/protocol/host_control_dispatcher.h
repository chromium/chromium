// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_HOST_CONTROL_DISPATCHER_H_
#define REMOTING_PROTOCOL_HOST_CONTROL_DISPATCHER_H_

#include <cstddef>

#include "base/macros.h"
#include "remoting/protocol/channel_dispatcher_base.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/cursor_shape_stub.h"

namespace remoting {
namespace protocol {

class HostStub;
class PairingResponse;

// HostControlDispatcher dispatches incoming messages on the control
// channel to HostStub or ClipboardStub, and also implements ClientStub and
// CursorShapeStub for outgoing messages.
class HostControlDispatcher : public ChannelDispatcherBase,
                              public ClientStub {
 public:
  HostControlDispatcher();
  ~HostControlDispatcher() override;

  // ClientStub implementation.
  void SetCapabilities(const Capabilities& capabilities) override;
  void SetPairingResponse(const PairingResponse& pairing_response) override;
  void DeliverHostMessage(const ExtensionMessage& message) override;
  void SetVideoLayout(const VideoLayout& layout) override;

  // ClipboardStub implementation for sending clipboard data to client.
  void InjectClipboardEvent(const ClipboardEvent& event) override;

  // CursorShapeStub implementation for sending cursor shape to client.
  void SetCursorShape(const CursorShapeInfo& cursor_shape) override;

  // Sets the ClipboardStub that will be called for each incoming clipboard
  // message. |clipboard_stub| must outlive this object.
  void set_clipboard_stub(ClipboardStub* clipboard_stub) {
    clipboard_stub_ = clipboard_stub;
  }

  // Sets the HostStub that will be called for each incoming control
  // message. |host_stub| must outlive this object.
  void set_host_stub(HostStub* host_stub) { host_stub_ = host_stub; }

  // Sets the maximum size of outgoing messages, which defaults to 64KiB. This
  // is used to ensure we don't try to send any clipboard messages that the
  // client can't receive. Clipboard updates that are larger than the maximum
  // message size will be dropped.
  void set_max_message_size(std::size_t max_message_size) {
    LOG(INFO) << "Setting maximum message size to " << max_message_size;
    max_message_size_ = max_message_size;
  }

 private:
  void OnIncomingMessage(std::unique_ptr<CompoundBuffer> buffer) override;

  ClipboardStub* clipboard_stub_ = nullptr;
  HostStub* host_stub_ = nullptr;
  // 64 KiB is the default message size expected to be supported in absence of
  // a higher value negotiated via SDP.
  std::size_t max_message_size_ = 64 * 1024;

  DISALLOW_COPY_AND_ASSIGN(HostControlDispatcher);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_HOST_CONTROL_DISPATCHER_H_
