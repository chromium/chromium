// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_injector_chromeos.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "ash/display/window_tree_host_manager.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/chromeos/point_transformer.h"
#include "remoting/host/clipboard.h"
#include "remoting/proto/internal.pb.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
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
  }
}

// Check if the given key could be mapped to caps lock
bool IsLockKey(ui::DomCode dom_code) {
  switch (dom_code) {
    // Ignores all the keys that could possibly be mapped to Caps Lock in event
    // rewriter. Please refer to ui::EventRewriterAsh::RewriteModifierKeys.
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
  auto* ime = ash::input_method::InputMethodManager::Get();
  ime->GetImeKeyboard()->SetCapsLockEnabled(caps_lock);
}

class SystemInputInjectorStub : public ui::SystemInputInjector {
 public:
  SystemInputInjectorStub() {
    LOG(WARNING)
        << "Using stubbed input injector; All CRD user input will be ignored.";
  }
  SystemInputInjectorStub(const SystemInputInjectorStub&) = delete;
  SystemInputInjectorStub& operator=(const SystemInputInjectorStub&) = delete;
  ~SystemInputInjectorStub() override = default;

  // SystemInputInjector implementation:
  void SetDeviceId(int device_id) override {}
  void MoveCursorTo(const gfx::PointF& location) override {}
  void InjectMouseButton(ui::EventFlags button, bool down) override {}
  void InjectMouseWheel(int delta_x, int delta_y) override {}
  void InjectKeyEvent(ui::DomCode physical_key,
                      bool down,
                      bool suppress_auto_repeat) override {}
};

}  // namespace

// This class is run exclusively on the UI thread of the browser process.
class InputInjectorChromeos::Core {
 public:
  Core();

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  ~Core();

  // Mirrors the public InputInjectorChromeos interface.
  void InjectClipboardEvent(const ClipboardEvent& event);
  void InjectKeyEvent(const KeyEvent& event);
  void InjectTextEvent(const TextEvent& event);
  void InjectMouseEvent(const MouseEvent& event);
  void Start(std::unique_ptr<protocol::ClipboardStub> client_clipboard);
  void StartWithDelegate(
      std::unique_ptr<ui::SystemInputInjector> delegate,
      std::unique_ptr<protocol::ClipboardStub> client_clipboard);

 private:
  void SetLockStates(uint32_t states);

  void InjectMouseMove(const MouseEvent& event);

  bool hide_cursor_on_disconnect_ = false;
  std::unique_ptr<ui::SystemInputInjector> delegate_;
  std::unique_ptr<Clipboard> clipboard_;
};

InputInjectorChromeos::Core::Core() = default;

InputInjectorChromeos::Core::~Core() {
  if (hide_cursor_on_disconnect_) {
    aura::client::CursorClient* cursor_client =
        aura::client::GetCursorClient(ash::Shell::GetPrimaryRootWindow());
    if (cursor_client) {
      cursor_client->HideCursor();
    }
  }
}

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
    VLOG(3) << "Injecting key " << (event.pressed() ? "down" : "up")
            << " event.";
    delegate_->InjectKeyEvent(dom_code, event.pressed(),
                              true /* suppress_auto_repeat */);
  }
}

void InputInjectorChromeos::Core::InjectTextEvent(const TextEvent& event) {
  DCHECK(event.has_text());

  aura::Window* root_window = ash::Shell::GetPrimaryRootWindow();
  if (!root_window) {
    LOG(ERROR) << "root_window is null, can't inject text.";
    return;
  }
  aura::WindowTreeHost* window_tree_host = root_window->GetHost();
  if (!window_tree_host) {
    LOG(ERROR) << "window_tree_host is null, can't inject text.";
    return;
  }
  ui::InputMethod* input_method = window_tree_host->GetInputMethod();
  if (!input_method) {
    LOG(ERROR) << "input_method is null, can't inject text.";
    return;
  }
  ui::TextInputClient* text_input_client = input_method->GetTextInputClient();
  if (!text_input_client) {
    LOG(ERROR) << "text_input_client is null, can't inject text.";
    return;
  }

  std::string normalized_str;
  base::ConvertToUtf8AndNormalize(event.text(), base::kCodepageUTF8,
                                  &normalized_str);
  std::u16string utf16_string = base::UTF8ToUTF16(normalized_str);

  text_input_client->InsertText(
      utf16_string,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
}

void InputInjectorChromeos::Core::InjectMouseEvent(const MouseEvent& event) {
  if (event.has_button() && event.has_button_down()) {
    if (event.has_x() || event.has_y()) {
      // Ensure the mouse button click happens at the correct position.
      InjectMouseMove(event);
    }
    delegate_->InjectMouseButton(MouseButtonToUIFlags(event.button()),
                                 event.button_down());
  } else if (event.has_wheel_delta_x() || event.has_wheel_delta_y()) {
    delegate_->InjectMouseWheel(event.wheel_delta_x(), event.wheel_delta_y());
  } else if (event.has_x() || event.has_y()) {
    InjectMouseMove(event);
  } else {
    LOG(WARNING) << "Ignoring mouse event of unknown type";
  }
}

void InputInjectorChromeos::Core::InjectMouseMove(const MouseEvent& event) {
  gfx::PointF location_in_screen_in_dip = gfx::PointF(event.x(), event.y());
  gfx::PointF location_in_screen_in_pixels =
      PointTransformer::ConvertScreenInDipToScreenInPixel(
          location_in_screen_in_dip);

  delegate_->MoveCursorTo(location_in_screen_in_pixels);
}

void InputInjectorChromeos::Core::Start(
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {
  auto delegate = ui::OzonePlatform::GetInstance()->CreateSystemInputInjector();
  if (!delegate && !base::SysInfo::IsRunningOnChromeOS()) {
    // This happens when directly running the Chrome binary on linux.
    // We'll simply ignore all input there (instead of crashing).
    // Note: it would be nicer to swap this out with input_injector_x11.cc
    // on linux instead (and properly handle the input), but that runs into
    // dependency issues.
    delegate = std::make_unique<SystemInputInjectorStub>();
  }
  CHECK(delegate);

  StartWithDelegate(std::move(delegate), std::move(client_clipboard));
}

void InputInjectorChromeos::Core::StartWithDelegate(
    std::unique_ptr<ui::SystemInputInjector> delegate,
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {
  delegate_ = std::move(delegate);

  delegate_->SetDeviceId(ui::ED_REMOTE_INPUT_DEVICE);

  // Implemented by remoting::ClipboardAura.
  clipboard_ = Clipboard::Create();
  clipboard_->Start(std::move(client_clipboard));

  // If the cursor was hidden before we start injecting input then we should try
  // to restore its state when the remote user disconnects.  The main scenario
  // where this is important is for devices in non-interactive Kiosk mode.
  // Since no one is interacting with the screen in this mode, we will leave a
  // visible cursor after disconnecting which can't be hidden w/o restarting.
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(ash::Shell::GetPrimaryRootWindow());
  if (cursor_client) {
    hide_cursor_on_disconnect_ = !cursor_client->IsCursorVisible();
  }
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

void InputInjectorChromeos::StartForTesting(
    std::unique_ptr<ui::SystemInputInjector> input_injector,
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {
  input_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::StartWithDelegate, base::Unretained(core_.get()),
                     std::move(input_injector), std::move(client_clipboard)));
}

// static
std::unique_ptr<InputInjector> InputInjector::Create(
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  // The Ozone input injector must be called on the UI task runner of the
  // browser process.
  return std::make_unique<InputInjectorChromeos>(ui_task_runner);
}

// static
bool InputInjector::SupportsTouchEvents() {
  return false;
}

}  // namespace remoting
