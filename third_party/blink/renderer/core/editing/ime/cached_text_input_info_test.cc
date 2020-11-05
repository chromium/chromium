// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/cached_text_input_info.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

class CachedTextInputInfoTest : public EditingTestBase {
 protected:
  CachedTextInputInfo& GetCachedTextInputInfo() {
    return GetInputMethodController().GetCachedTextInputInfoForTesting();
  }

  InputMethodController& GetInputMethodController() {
    return GetFrame().GetInputMethodController();
  }
};

TEST_F(CachedTextInputInfoTest, Basic) {
  GetFrame().Selection().SetSelectionAndEndTyping(
      SetSelectionTextToBody("<div contenteditable id=\"sample\">a|b</div>"));
  const Element& sample = *GetElementById("sample");

  EXPECT_EQ(PlainTextRange(1, 1),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("ab", GetCachedTextInputInfo().GetText());

  To<Text>(sample.firstChild())->appendData("X");
  EXPECT_EQ(PlainTextRange(1, 1),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("abX", GetCachedTextInputInfo().GetText());
}

TEST_F(CachedTextInputInfoTest, RelayoutBoundary) {
  InsertStyleElement(
      "#sample { contain: strict; width: 100px; height: 100px; }");
  GetFrame().Selection().SetSelectionAndEndTyping(SetSelectionTextToBody(
      "<div contenteditable><div id=\"sample\">^a|b</div>"));
  const Element& sample = *GetElementById("sample");
  ASSERT_TRUE(sample.GetLayoutObject()->IsRelayoutBoundary());

  EXPECT_EQ(PlainTextRange(0, 1),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("ab", GetCachedTextInputInfo().GetText());

  To<Text>(sample.firstChild())->appendData("X");
  EXPECT_EQ(PlainTextRange(0, 1),
            GetInputMethodController().GetSelectionOffsets());
  EXPECT_EQ("abX", GetCachedTextInputInfo().GetText());
}

}  // namespace blink
