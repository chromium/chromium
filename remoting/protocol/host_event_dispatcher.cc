// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/host_event_dispatcher.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/base/constants.h"
#include "remoting/proto/event.pb.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/message_serialization.h"

namespace remoting::protocol {

namespace {

// Minimum interval between log messages reporting oversized event messages.
constexpr base::TimeDelta kOversizedMessageLogInterval = base::Seconds(10);

}  // namespace

HostEventDispatcher::HostEventDispatcher()
    : ChannelDispatcherBase(kEventChannelName),
      event_timestamps_source_(new InputEventTimestampsSourceImpl()) {}

HostEventDispatcher::~HostEventDispatcher() = default;

void HostEventDispatcher::OnIncomingMessage(
    std::unique_ptr<CompoundBuffer> buffer) {
  if (buffer->total_bytes() > kMaxEventMessageSizeBytes) {
    OnOversizedMessageDropped(buffer->total_bytes());
    return;
  }

  if (!input_stub_) {
    return;
  }

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
    const MouseEvent& event = message->mouse_event();
    bool has_absolute =
        event.has_x() || event.has_y() || event.has_fractional_coordinate();
    bool has_relative = event.has_delta_x() || event.has_delta_y();
    if (has_absolute && has_relative) {
      LOG(WARNING) << "Received mouse event with both absolute and relative "
                      "coordinates.";
    } else {
      input_stub_->InjectMouseEvent(event);
    }
  } else if (message->has_touch_event()) {
    input_stub_->InjectTouchEvent(message->touch_event());
  } else {
    LOG(WARNING) << "Unknown event message received.";
  }
}

void HostEventDispatcher::OnOversizedMessageDropped(size_t message_size) {
  // TODO: crbug.com/588598319 - Tell the client, with a generic response, when
  // an input event is dropped for exceeding a size or rate limit. Clipboard
  // events that exceed the configured size limit are also handled without
  // telling the client.
  base::TimeTicks now = base::TimeTicks::Now();
  if (!last_oversized_message_log_time_.is_null() &&
      now - last_oversized_message_log_time_ < kOversizedMessageLogInterval) {
    return;
  }
  last_oversized_message_log_time_ = now;
  LOG(WARNING) << "Dropped an event message of " << message_size
               << " bytes (limit is " << kMaxEventMessageSizeBytes
               << " bytes).";
}

}  // namespace remoting::protocol
