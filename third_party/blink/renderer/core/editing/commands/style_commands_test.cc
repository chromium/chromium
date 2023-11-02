// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/style_commands.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

class StyleCommandsTest : public EditingTestBase {};

// http://crbug.com/1348478
TEST_F(StyleCommandsTest, ComputeAndSetTypingStyleWithNullPosition) {
  GetDocument().setDesignMode("on");
  InsertStyleElement(
      "b {"
      "display: inline-block;"
      "overflow-x: scroll;"
      "}");
  Selection().SetSelection(SetSelectionTextToBody("|<b></b>&#32;"),
                           SetSelectionOptions());

  EXPECT_TRUE(StyleCommands::ExecuteToggleBold(GetFrame(), nullptr,
                                               EditorCommandSource::kDOM, ""));

  EXPECT_EQ("|<b></b> ", GetSelectionTextFromBody());
}

}  // namespace blink
