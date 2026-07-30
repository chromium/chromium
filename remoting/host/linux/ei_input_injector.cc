// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/ei_input_injector.h"

#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "remoting/base/logging.h"
#include "remoting/host/clipboard.h"
#include "remoting/host/linux/ei_keymap.h"
#include "remoting/host/linux/ei_sender_session.h"
#include "remoting/host/linux/lock_state_tracker.h"
#include "remoting/host/linux/pipewire_capture_stream.h"
#include "remoting/proto/event.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace remoting {

namespace {

constexpr uint32_t kCapsLockUsbKeyCode = 0x070039;
constexpr uint32_t kNumLockUsbKeyCode = 0x070053;

}  // namespace

EiInputInjector::EiInputInjector(
    base::WeakPtr<EiSenderSession> session,
    base::WeakPtr<const CaptureStreamManager> stream_manager,
    std::unique_ptr<Clipboard> clipboard,
    std::unique_ptr<LockStateTracker> lock_state_tracker)
    : ei_session_(session),
      stream_manager_(stream_manager),
      clipboard_(std::move(clipboard)),
      lock_state_tracker_(std::move(lock_state_tracker)) {}

EiInputInjector::~EiInputInjector() = default;

base::WeakPtr<EiInputInjector> EiInputInjector::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void EiInputInjector::SetKeymap(base::WeakPtr<EiKeymap> keymap) {
  keymap_ = keymap;
}

void EiInputInjector::SetEiSession(base::WeakPtr<EiSenderSession> session) {
  ei_session_ = session;
}

void EiInputInjector::Start(
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {
  clipboard_->Start(std::move(client_clipboard));
  if (lock_state_tracker_) {
    lock_state_tracker_->Start();
  }
}

void EiInputInjector::InjectKeyEvent(const protocol::KeyEvent& event) {
  if (!ei_session_ || !keymap_) {
    return;
  }
  if (!event.has_usb_keycode() || !event.has_pressed()) {
    LOG(WARNING) << "Key event with no key info";
    return;
  }

  // If a lock state tracker is available, synchronize the lock state on key
  // presses.
  if (lock_state_tracker_ && event.pressed() &&
      event.usb_keycode() != kCapsLockUsbKeyCode &&
      event.usb_keycode() != kNumLockUsbKeyCode) {
    std::optional<bool> caps_lock;
    if (event.has_caps_lock_state()) {
      caps_lock = event.caps_lock_state();
    } else if (event.has_lock_states()) {
      caps_lock =
          (event.lock_states() & protocol::KeyEvent::LOCK_STATES_CAPSLOCK) != 0;
    }

    if (caps_lock.has_value() &&
        *caps_lock != lock_state_tracker_->GetCapsLockState()) {
      ei_session_->InjectKeyEvent(kCapsLockUsbKeyCode, true);
      ei_session_->InjectKeyEvent(kCapsLockUsbKeyCode, false);
      lock_state_tracker_->SetExpectedCapsLockState(*caps_lock);
    }

    // Not all clients have a concept of num lock. Since there's no way to
    // distinguish these clients using the legacy lock_states field, only
    // update if the new num_lock field is specified.
    if (event.has_num_lock_state()) {
      bool num_lock = event.num_lock_state();
      if (num_lock != lock_state_tracker_->GetNumLockState()) {
        ei_session_->InjectKeyEvent(kNumLockUsbKeyCode, true);
        ei_session_->InjectKeyEvent(kNumLockUsbKeyCode, false);
        lock_state_tracker_->SetExpectedNumLockState(num_lock);
      }
    }
  }

  if (!event.pressed()) {
    // If the key isn't pressed, there's nothing to do. This is expected if
    // the key was released immediately after being pressed in order to avoid
    // unwanted auto-repeat.
    if (!pressed_keys_.erase(event.usb_keycode())) {
      return;
    }
  }
  ei_session_->InjectKeyEvent(event.usb_keycode(), event.pressed());
  if (event.pressed()) {
    // Immediately release non-modifier keys to avoid unwanted auto-repeat.
    //
    // TODO: jamiewalch - Remove this workaround once
    // https://gitlab.freedesktop.org/libinput/libei/-/issues/74 is fixed.
    if (keymap_->CanAutoRepeatUsbCode(event.usb_keycode())) {
      pressed_keys_.insert(event.usb_keycode());
    } else {
      ei_session_->InjectKeyEvent(event.usb_keycode(), false);
    }
  }
}

void EiInputInjector::InjectTextEvent(const protocol::TextEvent& event) {
  if (!keymap_) {
    return;
  }
  if (!ei_session_) {
    return;
  }
  // Release all keys before injecting text event. This is necessary to avoid
  // any interference with the currently pressed keys. E.g. if Shift is pressed
  // when TextEvent is received.
  for (const auto key : pressed_keys_) {
    ei_session_->InjectKeyEvent(key, false);
  }
  pressed_keys_.clear();
  std::vector<EiKeymap::Recipe> recipes;
  const std::string& text = event.text();
  for (size_t index = 0; index < text.size(); ++index) {
    base_icu::UChar32 code_point;
    if (!base::ReadUnicodeCharacter(text, &index, &code_point)) {
      LOG(ERROR) << "Invalid encoding at index: " << index
                 << " for text: " << text << ". Not injecting any text.";
      return;
    }
    auto recipe = keymap_->GetRecipeForCodepoint(code_point);
    // Skip unsupported codepoints. Ideally we'd ignore the whole string in this
    // case so that it's obvious to the user that something is wrong, but since
    // the client splits the string into individual characters it's not possible
    // to know when two text events are part of the same entry.
    if (recipe.usb_code == 0) {
      LOG(WARNING) << "Unsupported codepoint: " << code_point
                   << " for text: " << text;
      continue;
    }
    recipes.push_back(recipe);
  }
  std::set<uint32_t> pressed_modifiers;
  for (const auto& recipe : recipes) {
    // Release any modifiers that are no longer needed.
    for (const auto key : pressed_modifiers) {
      if (!recipe.modifiers.contains(key)) {
        ei_session_->InjectKeyEvent(key, false);
      }
    }
    // Press any new modifiers that are now needed.
    for (const auto key : recipe.modifiers) {
      if (!pressed_modifiers.contains(key)) {
        ei_session_->InjectKeyEvent(key, true);
      }
    }
    pressed_modifiers = recipe.modifiers;
    ei_session_->InjectKeyEvent(recipe.usb_code, true);
    ei_session_->InjectKeyEvent(recipe.usb_code, false);
  }
  for (const auto key : pressed_modifiers) {
    ei_session_->InjectKeyEvent(key, false);
  }
}

void EiInputInjector::InjectMouseEvent(const protocol::MouseEvent& event) {
  if (!ei_session_) {
    return;
  }
  bool event_sent = false;
  if (event.has_fractional_coordinate() &&
      event.fractional_coordinate().has_x() &&
      event.fractional_coordinate().has_y()) {
    if (!stream_manager_) {
      LOG(WARNING) << "CaptureStreamManager no longer exists.";
    } else {
      webrtc::ScreenId screen_id = event.fractional_coordinate().screen_id();
      base::WeakPtr<const CaptureStream> stream =
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

  if (event.has_wheel_ticks_x() || event.has_wheel_ticks_y()) {
    ei_session_->InjectScrollDiscrete(
        event.has_wheel_ticks_x() ? event.wheel_ticks_x() : 0,
        event.has_wheel_ticks_y() ? event.wheel_ticks_y() : 0);
    event_sent = true;
  } else if (event.has_wheel_delta_x() || event.has_wheel_delta_y()) {
    ei_session_->InjectScrollDelta(
        event.has_wheel_delta_x() ? event.wheel_delta_x() : 0,
        event.has_wheel_delta_y() ? event.wheel_delta_y() : 0);
    event_sent = true;
  }

  if (!event_sent) {
    LOG(WARNING) << "No mouse event is injected.";
  }
}

void EiInputInjector::InjectTouchEvent(const protocol::TouchEvent& event) {
  NOTIMPLEMENTED();
}

void EiInputInjector::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  clipboard_->InjectClipboardEvent(event);
}

}  // namespace remoting
