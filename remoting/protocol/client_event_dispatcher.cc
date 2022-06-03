// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/client_event_dispatcher.h"

#include "base/time/time.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/base/constants.h"
#include "remoting/proto/event.pb.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/message_pipe.h"

namespace remoting {
namespace protocol {

ClientEventDispatcher::ClientEventDispatcher()
    : ChannelDispatcherBase(kEventChannelName) {}
ClientEventDispatcher::~ClientEventDispatcher() = default;

void ClientEventDispatcher::InjectKeyEvent(const KeyEvent& event) {
  DCHECK(event.has_usb_keycode());
  DCHECK(event.has_pressed());
  EventMessage message;
  message.set_timestamp(base::TimeTicks::Now().ToInternalValue());
  message.mutable_key_event()->CopyFrom(event);
  message_pipe()->Send(&message, {});
}

void ClientEventDispatcher::InjectTextEvent(const TextEvent& event) {
  DCHECK(event.has_text());
  EventMessage message;
  message.set_timestamp(base::TimeTicks::Now().ToInternalValue());
  message.mutable_text_event()->CopyFrom(event);
  message_pipe()->Send(&message, {});
}

void ClientEventDispatcher::InjectMouseEvent(const MouseEvent& event) {
  EventMessage message;
  message.set_timestamp(base::TimeTicks::Now().ToInternalValue());
  message.mutable_mouse_event()->CopyFrom(event);
  message_pipe()->Send(&message, {});
}

void ClientEventDispatcher::InjectTouchEvent(const TouchEvent& event) {
  EventMessage message;
  message.set_timestamp(base::TimeTicks::Now().ToInternalValue());
  message.mutable_touch_event()->CopyFrom(event);
  message_pipe()->Send(&message, {});
}

void ClientEventDispatcher::OnIncomingMessage(
    std::unique_ptr<CompoundBuffer> message) {
  LOG(ERROR) << "Received unexpected message on the event channel.";
}

}  // namespace protocol
}  // namespace remoting
