// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/keyboard_layout_monitor.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "remoting/proto/control.pb.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"

namespace remoting {

namespace {

using protocol::LayoutKeyFunction;

constexpr int kShiftLevelFlags[] = {0, ui::EF_SHIFT_DOWN, ui::EF_ALTGR_DOWN,
                                    ui::EF_SHIFT_DOWN | ui::EF_ALTGR_DOWN};

class KeyboardLayoutMonitorChromeOs
    : public KeyboardLayoutMonitor,
      public ash::input_method::ImeKeyboard::Observer {
 public:
  explicit KeyboardLayoutMonitorChromeOs(
      base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback);
  ~KeyboardLayoutMonitorChromeOs() override;

  // KeyboardLayoutMonitor implementation.
  void Start() override;

  // ash::input_method::ImeKeyboard::Observer implementation.
  void OnCapsLockChanged(bool enabled) override;
  void OnLayoutChanging(const std::string& layout_name) override;

 private:
  void QueryLayout();

  LayoutKeyFunction GetFunctionFromKeyboardCode(ui::KeyboardCode key_code);

  base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback_;
  base::ScopedObservation<ash::input_method::ImeKeyboard,
                          ash::input_method::ImeKeyboard::Observer>
      observation_{this};
  base::WeakPtrFactory<KeyboardLayoutMonitorChromeOs> weak_ptr_factory_{this};
};

KeyboardLayoutMonitorChromeOs::KeyboardLayoutMonitorChromeOs(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback)
    : callback_(callback) {}

KeyboardLayoutMonitorChromeOs::~KeyboardLayoutMonitorChromeOs() = default;

void KeyboardLayoutMonitorChromeOs::Start() {
  QueryLayout();

  observation_.Observe(
      ash::input_method::InputMethodManager::Get()->GetImeKeyboard());
}

void KeyboardLayoutMonitorChromeOs::OnCapsLockChanged(bool enabled) {}

void KeyboardLayoutMonitorChromeOs::OnLayoutChanging(
    const std::string& layout_name) {
  // OnLayoutChanging() is triggered when the layout is changing but hasn't
  // changed yet. We can post an async task to allow the OS to 'finish' the
  // layout change however on very slow machines (e.g. a development VM), the
  // layout change takes a hundred milliseconds or so. To ensure the layout
  // change has occurred on a normal machine, we delay the async task by a half
  // second which should be enough. Note that we can't rely on checking the
  // current layout as, from observation, the current keyboard layout name and
  // the actual layout map aren't updated atomically meaning the name will
  // change before the layout is actually loaded.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&KeyboardLayoutMonitorChromeOs::QueryLayout,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(500));
}

void KeyboardLayoutMonitorChromeOs::QueryLayout() {
  protocol::KeyboardLayout layout_message;
  ui::KeyboardLayoutEngine* keyboard_layout_engine =
      ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine();

  bool has_alt_gr = ash::input_method::InputMethodManager::Get()
                        ->GetImeKeyboard()
                        ->IsAltGrAvailable();
  int shift_levels = has_alt_gr ? 4 : 2;
  for (auto code : kSupportedKeys) {
    std::uint32_t usb_code = ui::KeycodeConverter::DomCodeToUsbKeycode(code);
    auto& key_actions =
        *(*layout_message.mutable_keys())[usb_code].mutable_actions();
    for (int shift_level = 0; shift_level < shift_levels; shift_level++) {
      ui::DomKey key;
      ui::KeyboardCode key_code;
      int event_flags = kShiftLevelFlags[shift_level];
      if (!keyboard_layout_engine->Lookup(code, event_flags, &key, &key_code)) {
        continue;
      }

      LayoutKeyFunction function = GetFunctionFromKeyboardCode(key_code);
      if (function != LayoutKeyFunction::UNKNOWN) {
        key_actions[shift_level].set_function(function);
        continue;
      }

      std::string utf8;
      if (key.IsCharacter()) {
        base::WriteUnicodeCharacter(key.ToCharacter(), &utf8);
      } else if (key.IsDeadKey()) {
        base::WriteUnicodeCharacter(key.ToDeadKeyCombiningCharacter(), &utf8);
      }

      if (!utf8.empty()) {
        key_actions[shift_level].set_character(utf8);
      }
    }

    if (key_actions.empty()) {
      layout_message.mutable_keys()->erase(usb_code);
    }
  }

  callback_.Run(layout_message);
}

LayoutKeyFunction KeyboardLayoutMonitorChromeOs::GetFunctionFromKeyboardCode(
    ui::KeyboardCode key_code) {
  // CAPS_LOCK and NUM_LOCK are left unmapped as they aren't handled by the
  // protocol or client (meaning lock states are not synchronized across so the
  // soft keyboard might not reflect the local settings accurately).
  switch (key_code) {
    case ui::VKEY_SCROLL:
      return LayoutKeyFunction::SCROLL_LOCK;
    case ui::VKEY_BACK:
      return LayoutKeyFunction::BACKSPACE;
    case ui::VKEY_RETURN:
      return LayoutKeyFunction::ENTER;
    case ui::VKEY_TAB:
      return LayoutKeyFunction::TAB;
    case ui::VKEY_INSERT:
      return LayoutKeyFunction::INSERT;
    case ui::VKEY_DELETE:
      return LayoutKeyFunction::DELETE_;
    case ui::VKEY_HOME:
      return LayoutKeyFunction::HOME;
    case ui::VKEY_END:
      return LayoutKeyFunction::END;
    case ui::VKEY_PRIOR:
      return LayoutKeyFunction::PAGE_UP;
    case ui::VKEY_NEXT:
      return LayoutKeyFunction::PAGE_DOWN;
    case ui::VKEY_CLEAR:
      return LayoutKeyFunction::CLEAR;
    case ui::VKEY_UP:
      return LayoutKeyFunction::ARROW_UP;
    case ui::VKEY_DOWN:
      return LayoutKeyFunction::ARROW_DOWN;
    case ui::VKEY_LEFT:
      return LayoutKeyFunction::ARROW_LEFT;
    case ui::VKEY_RIGHT:
      return LayoutKeyFunction::ARROW_RIGHT;
    case ui::VKEY_F1:
      return LayoutKeyFunction::F1;
    case ui::VKEY_F2:
      return LayoutKeyFunction::F2;
    case ui::VKEY_F3:
      return LayoutKeyFunction::F3;
    case ui::VKEY_F4:
      return LayoutKeyFunction::F4;
    case ui::VKEY_F5:
      return LayoutKeyFunction::F5;
    case ui::VKEY_F6:
      return LayoutKeyFunction::F6;
    case ui::VKEY_F7:
      return LayoutKeyFunction::F7;
    case ui::VKEY_F8:
      return LayoutKeyFunction::F8;
    case ui::VKEY_F9:
      return LayoutKeyFunction::F9;
    case ui::VKEY_F10:
      return LayoutKeyFunction::F10;
    case ui::VKEY_F11:
      return LayoutKeyFunction::F11;
    case ui::VKEY_F12:
      return LayoutKeyFunction::F12;
    case ui::VKEY_ESCAPE:
      return LayoutKeyFunction::ESCAPE;
    case ui::VKEY_APPS:
      return LayoutKeyFunction::CONTEXT_MENU;
    case ui::VKEY_PAUSE:
      return LayoutKeyFunction::PAUSE;
    case ui::VKEY_SNAPSHOT:
      return LayoutKeyFunction::PRINT_SCREEN;
    case ui::VKEY_CONTROL:
    case ui::VKEY_LCONTROL:
    case ui::VKEY_RCONTROL:
      return protocol::LayoutKeyFunction::CONTROL;
    case ui::VKEY_SHIFT:
    case ui::VKEY_LSHIFT:
    case ui::VKEY_RSHIFT:
      return protocol::LayoutKeyFunction::SHIFT;
    case ui::VKEY_ALTGR:
      return protocol::LayoutKeyFunction::ALT_GR;
    case ui::VKEY_MENU:
    case ui::VKEY_RMENU:
    case ui::VKEY_LMENU:
      return protocol::LayoutKeyFunction::ALT;
    case ui::VKEY_LWIN:
    case ui::VKEY_RWIN:
      return protocol::LayoutKeyFunction::SEARCH;
    default:
      return LayoutKeyFunction::UNKNOWN;
  }
}

}  // namespace

std::unique_ptr<KeyboardLayoutMonitor> KeyboardLayoutMonitor::Create(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner) {
  return std::make_unique<KeyboardLayoutMonitorChromeOs>(std::move(callback));
}

}  // namespace remoting
