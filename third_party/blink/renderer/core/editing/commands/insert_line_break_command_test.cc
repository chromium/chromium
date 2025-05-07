// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/insert_line_break_command.h"

#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"

namespace blink {

class InsertLineBreakCommandTest : public EditingTestBase {};

TEST_F(InsertLineBreakCommandTest, InsertToTextArea) {
  SetBodyContent("<textarea>foobar</textarea>");
  auto* field = To<TextControlElement>(QuerySelector("textarea"));
  field->setSelectionEnd(3);
  field->setSelectionStart(3);
  field->Focus();

  auto& command = *MakeGarbageCollected<InsertLineBreakCommand>(GetDocument());

  EXPECT_TRUE(command.Apply());
  if (RuntimeEnabledFeatures::TextareaLineEndingsAsBrEnabled()) {
    EXPECT_EQ(
        "<textarea><div>foo<br>|bar</div></textarea>",
        GetSelectionTextInFlatTreeFromBody(
            Selection().ComputeVisibleSelectionInFlatTree().AsSelection()));
  } else {
    EXPECT_EQ(
        "<textarea><div>foo\n|bar</div></textarea>",
        GetSelectionTextInFlatTreeFromBody(
            Selection().ComputeVisibleSelectionInFlatTree().AsSelection()));
  }
}

}  // namespace blink
