// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/insert_paragraph_separator_command.h"

#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/testing/selection_sample.h"

namespace blink {

class InsertParagraphSeparatorCommandTest : public EditingTestBase {};

// http://crbug.com/777378
TEST_F(InsertParagraphSeparatorCommandTest,
       CrashWithAppearanceStyleOnEmptyColgroup) {
  Selection().SetSelection(
      SetSelectionTextToBody(
          "<table contenteditable>"
          "    <colgroup style='-webkit-appearance:radio;'><!--|--></colgroup>"
          "</table>"),
      SetSelectionOptions());

  auto* command =
      MakeGarbageCollected<InsertParagraphSeparatorCommand>(GetDocument());
  // Crash should not be observed here.
  command->Apply();

  EXPECT_EQ(
      "<table contenteditable>"
      "|    <colgroup style=\"-webkit-appearance:radio;\"></colgroup>"
      "</table>",
      GetSelectionTextFromBody());
}

// http://crbug.com/777378
TEST_F(InsertParagraphSeparatorCommandTest,
       CrashWithAppearanceStyleOnEmptyColumn) {
  Selection().SetSelection(
      SetSelectionTextToBody("<table contenteditable>"
                             "    <colgroup style='-webkit-appearance:radio;'>"
                             "        <col><!--|--></col>"
                             "    </colgroup>"
                             "</table>"),
      SetSelectionOptions());

  auto* command =
      MakeGarbageCollected<InsertParagraphSeparatorCommand>(GetDocument());
  // Crash should not be observed here.
  command->Apply();
  EXPECT_EQ(
      "<table contenteditable>"
      "|    <colgroup style=\"-webkit-appearance:radio;\">"
      "        <col>"
      "    </colgroup>"
      "</table>",
      GetSelectionTextFromBody());
}

// https://crbug.com/835020
TEST_F(InsertParagraphSeparatorCommandTest, CrashWithCaptionBeforeBody) {
  // The bug reproduces only with |designMode == 'on'|
  GetDocument().setDesignMode("on");
  InsertStyleElement("");
  SetBodyContent("<style>*{max-width:inherit;display:initial;}</style>");

  // Insert <caption> between head and body
  Element* caption = GetDocument().CreateElementForBinding("caption");
  caption->SetInnerHTMLFromString("AxBxC");
  GetDocument().documentElement()->insertBefore(caption, GetDocument().body());

  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(EphemeralRange::RangeOfContents(*caption))
          .Build(),
      SetSelectionOptions());

  auto* command =
      MakeGarbageCollected<InsertParagraphSeparatorCommand>(GetDocument());
  // Shouldn't crash inside.
  EXPECT_FALSE(command->Apply());
  EXPECT_EQ(
      "<body><style><br>|*{max-width:inherit;display:initial;}</style></body>",
      SelectionSample::GetSelectionText(*GetDocument().documentElement(),
                                        Selection().GetSelectionInDOMTree()));
}

}  // namespace blink
