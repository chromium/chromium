// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/host_event_dispatcher.h"

#include "base/memory/ref_counted.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/base/constants.h"
#include "remoting/proto/event.pb.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/message_serialization.h"

namespace remoting::protocol {

HostEventDispatcher::HostEventDispatcher()
    : ChannelDispatcherBase(kEventChannelName),
      event_timestamps_source_(new InputEventTimestampsSourceImpl()) {}

HostEventDispatcher::~HostEventDispatcher() = default;

void HostEventDispatcher::OnIncomingMessage(
    std::unique_ptr<CompoundBuffer> buffer) {
  DCHECK(input_stub_);

  std::unique_ptr<EventMessage> message =
      ParseMessage<EventMessage>(buffer.get());
  if (!message) {
    return;
  }

  event_timestamps_source_->OnEventReceived(InputEventTimestamps{
      base::TimeTicks::FromInternalValue(message->timestamp()),
      base::TimeTicks::Now()});

  if (message->has_key_event()) {
    const KeyEvent& event = message->key_event();
    if (event.has_usb_keycode() && event.has_pressed()) {
      input_stub_->InjectKeyEvent(event);
    } else {
      LOG(WARNING) << "Received invalid key event.";
    }
  } else if (message->has_text_event()) {
    const TextEvent& event = message->text_event();
    if (event.has_text()) {
      input_stub_->InjectTextEvent(event);
    } else {
      LOG(WARNING) << "Received invalid text event.";
    }
  } else if (message->has_mouse_event()) {
    input_stub_->InjectMouseEvent(message->mouse_event());
  } else if (message->has_touch_event()) {
    input_stub_->InjectTouchEvent(message->touch_event());
  } else {
    LOG(WARNING) << "Unknown event message received.";
  }
}

}  // namespace remoting::protocol
