// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_injector_chromeos.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "remoting/host/chromeos/point_transformer.h"
#include "remoting/host/clipboard.h"
#include "remoting/proto/internal.pb.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/system_input_injector.h"

namespace remoting {

using protocol::ClipboardEvent;
using protocol::KeyEvent;
using protocol::MouseEvent;
using protocol::TextEvent;
using protocol::TouchEvent;

namespace {

ui::EventFlags MouseButtonToUIFlags(MouseEvent::MouseButton button) {
  switch (button) {
    case MouseEvent::BUTTON_LEFT:
      return ui::EF_LEFT_MOUSE_BUTTON;
    case MouseEvent::BUTTON_RIGHT:
      return ui::EF_RIGHT_MOUSE_BUTTON;
    case MouseEvent::BUTTON_MIDDLE:
      return ui::EF_MIDDLE_MOUSE_BUTTON;
    default:
      NOTREACHED();
      return ui::EF_NONE;
  }
}

// Check if the given key could be mapped to caps lock
bool IsLockKey(ui::DomCode dom_code) {
  switch (dom_code) {
    // Ignores all the keys that could possibly be mapped to Caps Lock in event
    // rewriter. Please refer to ui::EventRewriterChromeOS::RewriteModifierKeys.
    case ui::DomCode::F16:
    case ui::DomCode::CAPS_LOCK:
    case ui::DomCode::META_LEFT:
    case ui::DomCode::META_RIGHT:
    case ui::DomCode::CONTROL_LEFT:
    case ui::DomCode::CONTROL_RIGHT:
    case ui::DomCode::ALT_LEFT:
    case ui::DomCode::ALT_RIGHT:
    case ui::DomCode::ESCAPE:
    case ui::DomCode::BACKSPACE:
      return true;
    default:
      return false;
  }
}

// If caps_lock is specified, sets local keyboard state to match.
void SetCapsLockState(bool caps_lock) {
  chromeos::input_method::InputMethodManager* ime =
      chromeos::input_method::InputMethodManager::Get();
  ime->GetImeKeyboard()->SetCapsLockEnabled(caps_lock);
}

}  // namespace

// This class is run exclusively on the UI thread of the browser process.
class InputInjectorChromeos::Core {
 public:
  Core();

  // Mirrors the public InputInjectorChromeos interface.
  void InjectClipboardEvent(const ClipboardEvent& event);
  void InjectKeyEvent(const KeyEvent& event);
  void InjectTextEvent(const TextEvent& event);
  void InjectMouseEvent(const MouseEvent& event);
  void Start(std::unique_ptr<protocol::ClipboardStub> client_clipboard);

 private:
  void SetLockStates(uint32_t states);

  std::unique_ptr<ui::SystemInputInjector> delegate_;
  std::unique_ptr<Clipboard> clipboard_;

  // Used to rotate the input coordinates appropriately based on the current
  // display rotation settings.
  std::unique_ptr<PointTransformer> point_transformer_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

InputInjectorChromeos::Core::Core() = default;

void InputInjectorChromeos::Core::InjectClipboardEvent(
    const ClipboardEvent& event) {
  clipboard_->InjectClipboardEvent(event);
}

void InputInjectorChromeos::Core::InjectKeyEvent(const KeyEvent& event) {
  DCHECK(event.has_pressed());
  DCHECK(event.has_usb_keycode());

  ui::DomCode dom_code =
      ui::KeycodeConverter::UsbKeycodeToDomCode(event.usb_keycode());

  if (event.pressed() && !IsLockKey(dom_code)) {
    if (event.has_caps_lock_state()) {
      SetCapsLockState(event.caps_lock_state());
    } else if (event.has_lock_states()) {
      SetCapsLockState((event.lock_states() &
                        protocol::KeyEvent::LOCK_STATES_CAPSLOCK) != 0);
    }
  }

  // Ignore events which can't be mapped.
  if (dom_code != ui::DomCode::NONE) {
    delegate_->InjectKeyEvent(dom_code, event.pressed(),
                              true /* suppress_auto_repeat */);
  }
}

void InputInjectorChromeos::Core::InjectTextEvent(const TextEvent& event) {
  // Chrome OS only supports It2Me, which is not supported on mobile clients, so
  // we don't need to implement text events.
  NOTIMPLEMENTED();
}

void InputInjectorChromeos::Core::InjectMouseEvent(const MouseEvent& event) {
  if (event.has_button() && event.has_button_down()) {
    delegate_->InjectMouseButton(MouseButtonToUIFlags(event.button()),
                                 event.button_down());
  } else if (event.has_wheel_delta_y() || event.has_wheel_delta_x()) {
    delegate_->InjectMouseWheel(event.wheel_delta_x(), event.wheel_delta_y());
  } else {
    DCHECK(event.has_x() && event.has_y());
    delegate_->MoveCursorTo(point_transformer_->ToScreenCoordinates(
        gfx::PointF(event.x(), event.y())));
  }
}

void InputInjectorChromeos::Core::Start(
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {
  delegate_ = ui::OzonePlatform::GetInstance()->CreateSystemInputInjector();
  DCHECK(delegate_);

  // Implemented by remoting::ClipboardAura.
  clipboard_ = Clipboard::Create();
  clipboard_->Start(std::move(client_clipboard));
  point_transformer_.reset(new PointTransformer());
}

InputInjectorChromeos::InputInjectorChromeos(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : input_task_runner_(task_runner), core_(std::make_unique<Core>()) {}

InputInjectorChromeos::~InputInjectorChromeos() {
  input_task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

void InputInjectorChromeos::InjectClipboardEvent(const ClipboardEvent& event) {
  input_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::InjectClipboardEvent,
                                base::Unretained(core_.get()), event));
}

void InputInjectorChromeos::InjectKeyEvent(const KeyEvent& event) {
  input_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::InjectKeyEvent,
                                base::Unretained(core_.get()), event));
}

void InputInjectorChromeos::InjectTextEvent(const TextEvent& event) {
  input_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::InjectTextEvent,
                                base::Unretained(core_.get()), event));
}

void InputInjectorChromeos::InjectMouseEvent(const MouseEvent& event) {
  input_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::InjectMouseEvent,
                                base::Unretained(core_.get()), event));
}

void InputInjectorChromeos::InjectTouchEvent(const TouchEvent& event) {
  NOTIMPLEMENTED() << "Raw touch event injection not implemented for ChromeOS.";
}

void InputInjectorChromeos::Start(
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {
  input_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::Start, base::Unretained(core_.get()),
                                std::move(client_clipboard)));
}

// static
std::unique_ptr<InputInjector> InputInjector::Create(
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  // The Ozone input injector must be called on the UI task runner of the
  // browser process.
  return base::WrapUnique(new InputInjectorChromeos(ui_task_runner));
}

// static
bool InputInjector::SupportsTouchEvents() {
  return false;
}

}  // namespace remoting
