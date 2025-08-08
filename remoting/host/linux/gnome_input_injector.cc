// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_input_injector.h"

#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "remoting/base/logging.h"
#include "remoting/host/linux/ei_sender_session.h"
#include "remoting/host/linux/pipewire_capture_stream.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace remoting {

GnomeInputInjector::GnomeInputInjector(
    std::unique_ptr<EiSenderSession> session,
    base::WeakPtr<const PipewireCaptureStreamManager> stream_manager,
    GDBusConnectionRef dbus_connection,
    gvariant::ObjectPath session_path)
    : ei_session_(std::move(session)),
      stream_manager_(stream_manager),
      clipboard_(std::move(dbus_connection), std::move(session_path)) {}

GnomeInputInjector::~GnomeInputInjector() = default;

void GnomeInputInjector::Start(
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {
  clipboard_.Start(std::move(client_clipboard));
}

void GnomeInputInjector::InjectKeyEvent(const protocol::KeyEvent& event) {
  if (!event.has_usb_keycode() || !event.has_pressed()) {
    LOG(WARNING) << "Key event with no key info";
    return;
  }
  ei_session_->InjectKeyEvent(event.usb_keycode(), event.pressed());
}

void GnomeInputInjector::InjectTextEvent(const protocol::TextEvent& event) {
  NOTIMPLEMENTED();
}

void GnomeInputInjector::InjectMouseEvent(const protocol::MouseEvent& event) {
  bool event_sent = false;
  if (event.has_fractional_coordinate() &&
      event.fractional_coordinate().has_x() &&
      event.fractional_coordinate().has_y()) {
    if (!stream_manager_) {
      LOG(WARNING) << "PipewireCaptureStreamManager no longer exists.";
    } else {
      webrtc::ScreenId screen_id = event.fractional_coordinate().screen_id();
      const base::WeakPtr<PipewireCaptureStream> stream =
          stream_manager_->GetStream(screen_id);
      if (!stream) {
        LOG(ERROR) << "Unexpected screen ID: " << screen_id;
      } else {
        ei_session_->InjectAbsolutePointerMove(
            stream->mapping_id(), event.fractional_coordinate().x(),
            event.fractional_coordinate().y());
        event_sent = true;
      }
    }
  } else if (event.has_delta_x() || event.has_delta_y()) {
    ei_session_->InjectRelativePointerMove(
        event.has_delta_x() ? event.delta_x() : 0,
        event.has_delta_y() ? event.delta_y() : 0);
    event_sent = true;
  }

  if (event.has_button() && event.has_button_down()) {
    ei_session_->InjectButton(event.button(), event.button_down());
    event_sent = true;
  }

  if (event.has_wheel_delta_x() || event.has_wheel_delta_y()) {
    ei_session_->InjectScrollDelta(
        event.has_wheel_delta_x() ? event.wheel_delta_x() : 0,
        event.has_wheel_delta_y() ? event.wheel_delta_y() : 0);
    event_sent = true;
  } else if (event.has_wheel_ticks_x() || event.has_wheel_ticks_y()) {
    ei_session_->InjectScrollDiscrete(
        event.has_wheel_ticks_x() ? event.wheel_ticks_x() : 0,
        event.has_wheel_ticks_y() ? event.wheel_ticks_y() : 0);
    event_sent = true;
  }

  if (!event_sent) {
    LOG(WARNING) << "No mouse event is injected.";
  }
}

void GnomeInputInjector::InjectTouchEvent(const protocol::TouchEvent& event) {
  NOTIMPLEMENTED();
}

void GnomeInputInjector::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  clipboard_.InjectClipboardEvent(event);
}

}  // namespace remoting
