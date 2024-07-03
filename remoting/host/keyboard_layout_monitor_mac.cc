// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/keyboard_layout_monitor.h"

#include <Carbon/Carbon.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/apple/scoped_cftyperef.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/proto/control.pb.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace remoting {

namespace {

class KeyboardLayoutMonitorMac : public KeyboardLayoutMonitor {
 public:
  explicit KeyboardLayoutMonitorMac(
      base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback);

  ~KeyboardLayoutMonitorMac() override;

  void Start() override;

 private:
  struct CallbackContext {
    scoped_refptr<base::SequencedTaskRunner> task_runner;
    base::WeakPtr<KeyboardLayoutMonitorMac> weak_ptr;
  };

  void OnLayoutChanged(const protocol::KeyboardLayout& new_layout);

  static void SelectedKeyboardInputSourceChangedCallback(
      CFNotificationCenterRef center,
      void* observer,
      CFNotificationName name,
      const void* object,
      CFDictionaryRef userInfo);

  static void QueryLayoutOnMainLoop(CallbackContext* callback_context);

  base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback_;
  std::unique_ptr<CallbackContext> callback_context_;
  base::WeakPtrFactory<KeyboardLayoutMonitorMac> weak_ptr_factory_;
};

std::optional<protocol::LayoutKeyFunction> GetFixedKeyFunction(int keycode);
std::optional<protocol::LayoutKeyFunction> GetCharFunction(UniChar char_code,
                                                           int keycode);

KeyboardLayoutMonitorMac::KeyboardLayoutMonitorMac(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback)
    : callback_(std::move(callback)), weak_ptr_factory_(this) {}

KeyboardLayoutMonitorMac::~KeyboardLayoutMonitorMac() {
  if (!callback_context_) {
    return;
  }

  CFNotificationCenterRemoveObserver(
      CFNotificationCenterGetDistributedCenter(), callback_context_.get(),
      kTISNotifySelectedKeyboardInputSourceChanged, nullptr);

  // The distributed notification center posts all notifications from the
  // process's main run loop. Schedule deletion from the same loop to ensure
  // we don't delete the callback context while a notification is being
  // dispatched.
  // Store the callback context pointer in a local variable so the block
  // captures it directly instead of capturing this.
  CallbackContext* callback_context = callback_context_.release();
  CFRunLoopPerformBlock(CFRunLoopGetMain(), kCFRunLoopCommonModes, ^(void) {
    delete callback_context;
  });
  // No need to explicitly wake up the main loop here. It's fine if the
  // deletion is delayed slightly until the next time the main loop wakes up
  // normally.
}

void KeyboardLayoutMonitorMac::Start() {
  DCHECK(!callback_context_);
  callback_context_ = std::make_unique<CallbackContext>(
      CallbackContext{base::SequencedTaskRunner::GetCurrentDefault(),
                      weak_ptr_factory_.GetWeakPtr()});
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetDistributedCenter(), callback_context_.get(),
      SelectedKeyboardInputSourceChangedCallback,
      kTISNotifySelectedKeyboardInputSourceChanged, nullptr,
      CFNotificationSuspensionBehaviorDeliverImmediately);

  // Get the initial layout.
  // Store the callback context pointer in a local variable so the block
  // captures it directly instead of capturing this.
  CallbackContext* callback_context = callback_context_.get();
  base::apple::ScopedCFTypeRef<CFRunLoopRef> main_loop(
      CFRunLoopGetMain(), base::scoped_policy::RETAIN);
  CFRunLoopPerformBlock(main_loop.get(), kCFRunLoopCommonModes, ^(void) {
    QueryLayoutOnMainLoop(callback_context);
  });
  CFRunLoopWakeUp(main_loop.get());
}

void KeyboardLayoutMonitorMac::OnLayoutChanged(
    const protocol::KeyboardLayout& new_layout) {
  callback_.Run(new_layout);
}

// static
void KeyboardLayoutMonitorMac::SelectedKeyboardInputSourceChangedCallback(
    CFNotificationCenterRef center,
    void* observer,
    CFNotificationName name,
    const void* object,
    CFDictionaryRef userInfo) {
  CallbackContext* callback_context = static_cast<CallbackContext*>(observer);
  QueryLayoutOnMainLoop(callback_context);
}

// static
void KeyboardLayoutMonitorMac::QueryLayoutOnMainLoop(
    KeyboardLayoutMonitorMac::CallbackContext* callback_context) {
  base::apple::ScopedCFTypeRef<TISInputSourceRef> input_source(
      TISCopyCurrentKeyboardLayoutInputSource());
  base::apple::ScopedCFTypeRef<CFDataRef> layout_data(
      static_cast<CFDataRef>(TISGetInputSourceProperty(
          input_source.get(), kTISPropertyUnicodeKeyLayoutData)),
      base::scoped_policy::RETAIN);

  if (!layout_data) {
    LOG(WARNING) << "Failed to query keyboard layout.";
    return;
  }

  protocol::KeyboardLayout layout_message;

  std::uint8_t keyboard_type = LMGetKbdType();
  PhysicalKeyboardLayoutType layout_type = KBGetLayoutType(keyboard_type);

  for (ui::DomCode key : KeyboardLayoutMonitor::kSupportedKeys) {
    // Lang1 and Lang2 are only present on JIS-style keyboards.
    if ((key == ui::DomCode::LANG1 || key == ui::DomCode::LANG2) &&
        layout_type != kKeyboardJIS) {
      continue;
    }

    // Skip Caps Lock until we decide how/if we want to handle it.
    if (key == ui::DomCode::CAPS_LOCK) {
      continue;
    }

    std::uint32_t usb_code = ui::KeycodeConverter::DomCodeToUsbKeycode(key);
    int keycode = ui::KeycodeConverter::DomCodeToNativeKeycode(key);

    auto& key_actions =
        *(*layout_message.mutable_keys())[usb_code].mutable_actions();

    for (int shift_level = 0; shift_level < 4; ++shift_level) {
      std::optional<protocol::LayoutKeyFunction> fixed_function =
          GetFixedKeyFunction(keycode);
      if (fixed_function) {
        key_actions[shift_level].set_function(*fixed_function);
        continue;
      }

      std::uint32_t modifier_state = 0;
      if (shift_level & 1) {
        modifier_state |= shiftKey;
      }
      if (shift_level & 2) {
        modifier_state |= optionKey;
      }
      std::uint32_t deadkey_state = 0;
      UniChar result_array[255];
      UniCharCount result_length = 0;
      UCKeyTranslate(reinterpret_cast<const UCKeyboardLayout*>(
                         CFDataGetBytePtr(layout_data.get())),
                     keycode, kUCKeyActionDown, modifier_state >> 8,
                     keyboard_type, kUCKeyTranslateNoDeadKeysMask,
                     &deadkey_state, std::size(result_array), &result_length,
                     result_array);

      if (result_length == 0) {
        continue;
      }

      if (result_length == 1) {
        std::optional<protocol::LayoutKeyFunction> char_function =
            GetCharFunction(result_array[0], keycode);
        if (char_function) {
          key_actions[shift_level].set_function(*char_function);
          continue;
        }
      }

      key_actions[shift_level].set_character(
          base::UTF16ToUTF8(std::u16string_view(
              reinterpret_cast<const char16_t*>(result_array), result_length)));
    }

    if (key_actions.size() == 0) {
      layout_message.mutable_keys()->erase(usb_code);
    }
  }

  callback_context->task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&KeyboardLayoutMonitorMac::OnLayoutChanged,
                     callback_context->weak_ptr, std::move(layout_message)));
}

std::optional<protocol::LayoutKeyFunction> GetFixedKeyFunction(int keycode) {
  // Some keys are not represented in the layout and always have the same
  // function.
  switch (keycode) {
    case kVK_Command:
    case kVK_RightCommand:
      return protocol::LayoutKeyFunction::COMMAND;
    case kVK_Shift:
    case kVK_RightShift:
      return protocol::LayoutKeyFunction::SHIFT;
    case kVK_CapsLock:
      return protocol::LayoutKeyFunction::CAPS_LOCK;
    case kVK_Option:
    case kVK_RightOption:
      return protocol::LayoutKeyFunction::OPTION;
    case kVK_Control:
    case kVK_RightControl:
      return protocol::LayoutKeyFunction::CONTROL;
    case kVK_F1:
      return protocol::LayoutKeyFunction::F1;
    case kVK_F2:
      return protocol::LayoutKeyFunction::F2;
    case kVK_F3:
      return protocol::LayoutKeyFunction::F3;
    case kVK_F4:
      return protocol::LayoutKeyFunction::F4;
    case kVK_F5:
      return protocol::LayoutKeyFunction::F5;
    case kVK_F6:
      return protocol::LayoutKeyFunction::F6;
    case kVK_F7:
      return protocol::LayoutKeyFunction::F7;
    case kVK_F8:
      return protocol::LayoutKeyFunction::F8;
    case kVK_F9:
      return protocol::LayoutKeyFunction::F9;
    case kVK_F10:
      return protocol::LayoutKeyFunction::F10;
    case kVK_F11:
      return protocol::LayoutKeyFunction::F11;
    case kVK_F12:
      return protocol::LayoutKeyFunction::F12;
    case kVK_F13:
      return protocol::LayoutKeyFunction::F13;
    case kVK_F14:
      return protocol::LayoutKeyFunction::F14;
    case kVK_F15:
      return protocol::LayoutKeyFunction::F15;
    case kVK_F16:
      return protocol::LayoutKeyFunction::F16;
    case kVK_F17:
      return protocol::LayoutKeyFunction::F17;
    case kVK_F18:
      return protocol::LayoutKeyFunction::F18;
    case kVK_F19:
      return protocol::LayoutKeyFunction::F19;
    case kVK_JIS_Kana:
      return protocol::LayoutKeyFunction::KANA;
    case kVK_JIS_Eisu:
      return protocol::LayoutKeyFunction::EISU;
    default:
      return std::nullopt;
  }
}

std::optional<protocol::LayoutKeyFunction> GetCharFunction(UniChar char_code,
                                                           int keycode) {
  switch (char_code) {
    case kHomeCharCode:
      return protocol::LayoutKeyFunction::HOME;
    case kEnterCharCode:
      // Numpad Enter
      return protocol::LayoutKeyFunction::ENTER;
    case kEndCharCode:
      return protocol::LayoutKeyFunction::END;
    case kHelpCharCode:
      // Old Macs called this key "Help". Newer Macs lack it altogether (and
      // have "Fn" in this position).
      // TODO(rkjnsn): See how the latest macOS handles this key, and consider
      // hiding this key or creating a distinct HELP function, as appropriate.
      return protocol::LayoutKeyFunction::INSERT;
    case kBackspaceCharCode:
      return protocol::LayoutKeyFunction::BACKSPACE;
    case kTabCharCode:
      return protocol::LayoutKeyFunction::TAB;
    case kPageUpCharCode:
      return protocol::LayoutKeyFunction::PAGE_UP;
    case kPageDownCharCode:
      return protocol::LayoutKeyFunction::PAGE_DOWN;
    case kReturnCharCode:
      // Writing system return key.
      return protocol::LayoutKeyFunction::ENTER;
    case kFunctionKeyCharCode:
      // The known keys with this char code are handled by GetFixedKeyFunction.
      return protocol::LayoutKeyFunction::UNKNOWN;
    case kEscapeCharCode:
      if (keycode == kVK_ANSI_KeypadClear) {
        return protocol::LayoutKeyFunction::CLEAR;
      }
      return protocol::LayoutKeyFunction::ESCAPE;
    case kLeftArrowCharCode:
      return protocol::LayoutKeyFunction::ARROW_LEFT;
    case kRightArrowCharCode:
      return protocol::LayoutKeyFunction::ARROW_RIGHT;
    case kUpArrowCharCode:
      return protocol::LayoutKeyFunction::ARROW_UP;
    case kDownArrowCharCode:
      return protocol::LayoutKeyFunction::ARROW_DOWN;
    case kDeleteCharCode:
      return protocol::LayoutKeyFunction::DELETE_;
    default:
      return std::nullopt;
  }
}

}  // namespace

std::unique_ptr<KeyboardLayoutMonitor> KeyboardLayoutMonitor::Create(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner) {
  return std::make_unique<KeyboardLayoutMonitorMac>(std::move(callback));
}

}  // namespace remoting
