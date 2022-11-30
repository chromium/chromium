// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Use this file to assert that *_list.h enums that are meant to do the bridge
// from Blink are valid.

#include "media/base/mime_util.h"
#include "third_party/blink/public/common/input/web_menu_source_type.h"
#include "third_party/blink/public/platform/web_text_input_mode.h"
#include "third_party/blink/public/platform/web_text_input_type.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ui_base_types.h"

namespace content {

#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)

// WebTextInputMode
STATIC_ASSERT_ENUM(blink::kWebTextInputModeDefault,
                   ui::TEXT_INPUT_MODE_DEFAULT);
STATIC_ASSERT_ENUM(blink::kWebTextInputModeNone, ui::TEXT_INPUT_MODE_NONE);
STATIC_ASSERT_ENUM(blink::kWebTextInputModeText, ui::TEXT_INPUT_MODE_TEXT);
STATIC_ASSERT_ENUM(blink::kWebTextInputModeTel, ui::TEXT_INPUT_MODE_TEL);
STATIC_ASSERT_ENUM(blink::kWebTextInputModeUrl, ui::TEXT_INPUT_MODE_URL);
STATIC_ASSERT_ENUM(blink::kWebTextInputModeEmail, ui::TEXT_INPUT_MODE_EMAIL);
STATIC_ASSERT_ENUM(blink::kWebTextInputModeNumeric,
                   ui::TEXT_INPUT_MODE_NUMERIC);
STATIC_ASSERT_ENUM(blink::kWebTextInputModeDecimal,
                   ui::TEXT_INPUT_MODE_DECIMAL);
STATIC_ASSERT_ENUM(blink::kWebTextInputModeSearch, ui::TEXT_INPUT_MODE_SEARCH);

// WebTextInputType
STATIC_ASSERT_ENUM(blink::kWebTextInputTypeNone, ui::TEXT_INPUT_TYPE_NONE);
STATIC_ASSERT_ENUM(blink::kWebTextInputTypeText, ui::TEXT_INPUT_TYPE_TEXT);
STATIC_ASSERT_ENUM(blink::kWebTextInputTypePassword,
                   ui::TEXT_INPUT_TYPE_PASSWORD);
STATIC_ASSERT_ENUM(blink::kWebTextInputTypeSearch, ui::TEXT_INPUT_TYPE_SEARCH);
STATIC_ASSERT_ENUM(blink::kWebTextInputTypeEmail, ui::TEXT_INPUT_TYPE_EMAIL);
STATIC_ASSERT_ENUM(blink::kWebTextInputTypeNumber, ui::TEXT_INPUT_TYPE_NUMBER);
STATIC_ASSERT_ENUM(blink::kWebTextInputTypeTelephone,
                   ui::TEXT_INPUT_TYPE_TELEPHONE);
STATIC_ASSERT_ENUM(blink::kWebTextInputTypeURL, ui::TEXT_INPUT_TYPE_URL);
STATIC_ASSERT_ENUM(blink::kWebTextInputTypeDate, ui::TEXT_INPUT_TYPE_DATE);
STATIC_ASSERT_ENUM(blink::kWebTextInputTypeDateTime,
                   ui::TEXT_INPUT_TYPE_DATE_TIME);
STATIC_ASSERT_ENUM(blink::kWebTextInputTypeDateTimeLocal,
                   ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL);
STATIC_ASSERT_ENUM(blink::kWebTextInputTypeMonth, ui::TEXT_INPUT_TYPE_MONTH);
STATIC_ASSERT_ENUM(blink::kWebTextInputTypeTime, ui::TEXT_INPUT_TYPE_TIME);
STATIC_ASSERT_ENUM(blink::kWebTextInputTypeWeek, ui::TEXT_INPUT_TYPE_WEEK);
STATIC_ASSERT_ENUM(blink::kWebTextInputTypeTextArea,
                   ui::TEXT_INPUT_TYPE_TEXT_AREA);
STATIC_ASSERT_ENUM(blink::kWebTextInputTypeContentEditable,
                   ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE);
STATIC_ASSERT_ENUM(blink::kWebTextInputTypeDateTimeField,
                   ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD);

// WebMenuSourceType
STATIC_ASSERT_ENUM(blink::kMenuSourceNone, ui::MENU_SOURCE_NONE);
STATIC_ASSERT_ENUM(blink::kMenuSourceMouse, ui::MENU_SOURCE_MOUSE);
STATIC_ASSERT_ENUM(blink::kMenuSourceKeyboard, ui::MENU_SOURCE_KEYBOARD);
STATIC_ASSERT_ENUM(blink::kMenuSourceTouch, ui::MENU_SOURCE_TOUCH);
STATIC_ASSERT_ENUM(blink::kMenuSourceTouchEditMenu,
                   ui::MENU_SOURCE_TOUCH_EDIT_MENU);
STATIC_ASSERT_ENUM(blink::kMenuSourceLongPress, ui::MENU_SOURCE_LONG_PRESS);
STATIC_ASSERT_ENUM(blink::kMenuSourceLongTap, ui::MENU_SOURCE_LONG_TAP);
STATIC_ASSERT_ENUM(blink::kMenuSourceTouchHandle, ui::MENU_SOURCE_TOUCH_HANDLE);
STATIC_ASSERT_ENUM(blink::kMenuSourceStylus, ui::MENU_SOURCE_STYLUS);
STATIC_ASSERT_ENUM(blink::kMenuSourceAdjustSelection,
                   ui::MENU_SOURCE_ADJUST_SELECTION);
STATIC_ASSERT_ENUM(blink::kMenuSourceAdjustSelectionReset,
                   ui::MENU_SOURCE_ADJUST_SELECTION_RESET);

} // namespace content
