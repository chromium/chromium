// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/keyboard_event.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"

namespace blink {

TEST(KeyboardEventTest, Unicode) {
  WebKeyboardEvent web_event(WebKeyboardEvent::Type::kChar, 0,
                             base::TimeTicks());
  std::ranges::copy(u"\U0001f44d", web_event.text);
  KeyboardEvent *event = KeyboardEvent::Create(web_event, nullptr);
  ASSERT_EQ(event->charCode(), static_cast<int>(U'\U0001f44d'));
}

}  // namespace blink
