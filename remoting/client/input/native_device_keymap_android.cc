// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/input/native_device_keymap.h"

#include "base/logging.h"

namespace {

// These must be defined in the same order as the Android keycodes in
// <android/keycodes.h> and
// "ui/events/keycodes/keyboard_code_conversion_android.cc" . Some of these
// mappings assume a US keyboard layout for now.
const uint32_t usb_keycodes[] = {
    0,  // UNKNOWN
    0,  // SOFT_LEFT
    0,  // SOFT_RIGHT
    0,  // HOME
    0,  // BACK
    0,  // CALL
    0,  // ENDCALL

    0x070027,  // 0
    0x07001e,  // 1
    0x07001f,  // 2
    0x070020,  // 3
    0x070021,  // 4
    0x070022,  // 5
    0x070023,  // 6
    0x070024,  // 7
    0x070025,  // 8
    0x070026,  // 9

    0,         // STAR
    0,         // POUND
    0x070052,  // DPAD_UP
    0x070051,  // DPAD_DOWN
    0x070050,  // DPAD_LEFT
    0x07004f,  // DPAD_RIGHT
    0,         // DPAD_CENTER
    0,         // VOLUME_UP
    0,         // VOLUME_DOWN
    0,         // POWER
    0,         // CAMERA
    0,         // CLEAR

    0x070004,  // A
    0x070005,  // B
    0x070006,  // C
    0x070007,  // D
    0x070008,  // E
    0x070009,  // F
    0x07000a,  // G
    0x07000b,  // H
    0x07000c,  // I
    0x07000d,  // J
    0x07000e,  // K
    0x07000f,  // L
    0x070010,  // M
    0x070011,  // N
    0x070012,  // O
    0x070013,  // P
    0x070014,  // Q
    0x070015,  // R
    0x070016,  // S
    0x070017,  // T
    0x070018,  // U
    0x070019,  // V
    0x07001a,  // W
    0x07001b,  // X
    0x07001c,  // Y
    0x07001d,  // Z

    0x070036,  // COMMA
    0x070037,  // PERIOD

    0x0700e2,  // ALT_LEFT
    0x0700e6,  // ALT_RIGHT
    0x0700e1,  // SHIFT_LEFT
    0x0700e5,  // SHIFT_RIGHT

    0x07002b,  // TAB
    0x07002c,  // SPACE

    0,  // SYM
    0,  // EXPLORER
    0,  // ENVELOPE

    0x070028,  // ENTER
    0x07002a,  // DEL (backspace)

    0x070035,  // GRAVE (backtick)
    0x07002d,  // MINUS
    0x07002e,  // EQUALS
    0x07002f,  // LEFT_BRACKET
    0x070030,  // RIGHT_BRACKET
    0x070031,  // BACKSLASH
    0x070033,  // SEMICOLON
    0x070034,  // APOSTROPHE
    0x070038,  // SLASH

    0,  // AT
    0,  // NUM
    0,  // HEADSETHOOK
    0,  // FOCUS
    0,  // PLUS
    0,  // MENU
    0,  // NOTIFICATION
    0,  // SEARCH
    0,  // MEDIA_PLAY_PAUSE
    0,  // MEDIA_STOP
    0,  // MEDIA_NEXT
    0,  // MEDIA_PREVIOUS
    0,  // MEDIA_REWIND
    0,  // MEDIA_FAST_FORWARD
    0,  // MUTE

    0x07004b,  // PAGE_UP
    0x07004e,  // PAGE_DOWN

    0,  // PICTSYMBOLS
    0,  // SWITCH_CHARSET
    0,  // BUTTON_A
    0,  // BUTTON_B
    0,  // BUTTON_C
    0,  // BUTTON_X
    0,  // BUTTON_Y
    0,  // BUTTON_Z
    0,  // BUTTON_L1
    0,  // BUTTON_R1
    0,  // BUTTON_L2
    0,  // BUTTON_R2
    0,  // BUTTON_THUMBL
    0,  // BUTTON_THUMBR
    0,  // BUTTON_START
    0,  // BUTTON_SELECT
    0,  // BUTTON_MODE

    0x070029,  // ESCAPE
    0x07004c,  // FORWARD_DEL

    0x0700e0,  // CTRL_LEFT
    0x0700e4,  // CTRL_RIGHT
    0,         // CAPS_LOCK
    0,         // SCROLL_LOCK
    0x0700e3,  // META_LEFT
    0x0700e7,  // META_RIGHT
    0,         // FUNCTION

    0x070046,  // SYSRQ (printscreen)
    0x070048,  // BREAK (pause)
    0x07004a,  // MOVE_HOME (home)
    0x07004d,  // MOVE_END (end)
    0x070049,  // INSERT

    0,  // FORWARD
    0,  // MEDIA_PLAY
    0,  // MEDIA_PAUSE
    0,  // MEDIA_CLOSE
    0,  // MEDIA_EJECT
    0,  // MEDIA_RECORD

    0x07003a,  // F1
    0x07003b,  // F2
    0x07003c,  // F3
    0x07003d,  // F4
    0x07003e,  // F5
    0x07003f,  // F6
    0x070040,  // F7
    0x070041,  // F8
    0x070042,  // F9
    0x070043,  // F10
    0x070044,  // F11
    0x070045,  // F12

    0,  // NUM_LOCK

    0x070062,  // NUMPAD_0
    0x070059,  // NUMPAD_1
    0x07005a,  // NUMPAD_2
    0x07005b,  // NUMPAD_3
    0x07005c,  // NUMPAD_4
    0x07005d,  // NUMPAD_5
    0x07005e,  // NUMPAD_6
    0x07005f,  // NUMPAD_7
    0x070060,  // NUMPAD_8
    0x070061,  // NUMPAD_9

    0x070054,  // NUMPAD_DIVIDE
    0x070055,  // NUMPAD_MULTIPLY
    0x070056,  // NUMPAD_SUBTRACT
    0x070057,  // NUMPAD_ADD
    0x070063,  // NUMPAD_DOT
    0x070085,  // NUMPAD_COMMA
    0x070058,  // NUMPAD_ENTER
    0x070067,  // NUMPAD_EQUALS
    0x0700b6,  // NUMPAD_LEFT_PAREN
    0x0700b7,  // NUMPAD_RIGHT_PAREN
};

}  // namespace

namespace remoting {

uint32_t NativeDeviceKeycodeToUsbKeycode(size_t device_keycode) {
  if (device_keycode >= sizeof(usb_keycodes) / sizeof(uint32_t)) {
    LOG(WARNING) << "Attempted to decode out-of-range Android keycode";
    return 0;
  }

  return usb_keycodes[device_keycode];
}

}  // namespace remoting
