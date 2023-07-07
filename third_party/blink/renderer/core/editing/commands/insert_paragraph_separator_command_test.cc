// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/insert_paragraph_separator_command.h"

#include "third_party/blink/renderer/core/dom/text.h"
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
      "    <colgroup style=\"-webkit-appearance:radio;\">"
      "        <col>|"
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
  Element* caption =
      GetDocument().CreateElementForBinding(AtomicString("caption"));
  caption->setInnerHTML("AxBxC");
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

// http://crbug.com/1345989
TEST_F(InsertParagraphSeparatorCommandTest, CrashWithObject) {
  GetDocument().setDesignMode("on");
  Selection().SetSelection(
      SetSelectionTextToBody("<object><b>|ABC</b></object>"),
      SetSelectionOptions());
  base::RunLoop().RunUntilIdle();  // prepare <object> fallback content

  auto* command =
      MakeGarbageCollected<InsertParagraphSeparatorCommand>(GetDocument());

  EXPECT_TRUE(command->Apply());
  EXPECT_EQ(
      "<div><object><b><br></b></object></div>"
      "<object><b>|ABC</b></object>",
      GetSelectionTextFromBody());
}

// http://crbug.com/1357082
TEST_F(InsertParagraphSeparatorCommandTest, CrashWithObjectWithFloat) {
  InsertStyleElement("object { float: right; }");
  GetDocument().setDesignMode("on");
  Selection().SetSelection(
      SetSelectionTextToBody("<object><b>|ABC</b></object>"),
      SetSelectionOptions());
  base::RunLoop().RunUntilIdle();  // prepare <object> fallback content

  Element& object_element =
      *GetDocument().QuerySelector(AtomicString("object"));
  object_element.appendChild(Text::Create(GetDocument(), "XYZ"));

  auto* command =
      MakeGarbageCollected<InsertParagraphSeparatorCommand>(GetDocument());

  EXPECT_TRUE(command->Apply());
  EXPECT_EQ(
      "<object><b><br></b></object>"
      "<object><b>|ABC</b>XYZ</object>",
      GetSelectionTextFromBody());
}

// crbug.com/1420675
TEST_F(InsertParagraphSeparatorCommandTest, PhrasingContent) {
  const char* html = R"HTML("
    <span contenteditable>
      <div>
        <span>a|</span>
      </div>
    </span>)HTML";
  const char* expected_html = R"HTML("
    <span contenteditable>
      <div>
        <span>a<br>|<br></span>
      </div>
    </span>)HTML";
  Selection().SetSelection(SetSelectionTextToBody(html), SetSelectionOptions());
  auto* command =
      MakeGarbageCollected<InsertParagraphSeparatorCommand>(GetDocument());
  EXPECT_TRUE(command->Apply());
  EXPECT_EQ(expected_html, GetSelectionTextFromBody());
}

}  // namespace blink
