// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/editing/commands/format_block_command.h"
#include "third_party/blink/renderer/core/editing/commands/indent_outdent_command.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

#include <memory>

namespace blink {

class ApplyBlockElementCommandTest : public EditingTestBase {};

// This is a regression test for https://crbug.com/639534
TEST_F(ApplyBlockElementCommandTest, selectionCrossingOverBody) {
  GetDocument().head()->insertAdjacentHTML(
      "afterbegin",
      "<style> .CLASS13 { -webkit-user-modify: read-write; }</style></head>",
      ASSERT_NO_EXCEPTION);
  GetDocument().body()->insertAdjacentHTML(
      "afterbegin",
      "\n<pre><var id='va' class='CLASS13'>\nC\n</var></pre><input />",
      ASSERT_NO_EXCEPTION);
  GetDocument().body()->insertAdjacentText("beforebegin", "foo",
                                           ASSERT_NO_EXCEPTION);

  GetDocument().body()->setContentEditable("false", ASSERT_NO_EXCEPTION);
  GetDocument().setDesignMode("on");
  GetDocument().UpdateStyleAndLayout();
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(
              Position(GetDocument().documentElement(), 1),
              Position(GetDocument().getElementById("va")->firstChild(), 2))
          .Build(),
      SetSelectionOptions());

  auto* command = MakeGarbageCollected<FormatBlockCommand>(
      GetDocument(), html_names::kFooterTag);
  command->Apply();

  EXPECT_EQ(
      "<body contenteditable=\"false\">\n"
      "<pre><var id=\"va\" class=\"CLASS13\">\nC\n</var></pre><input></body>",
      GetDocument().documentElement()->InnerHTMLAsString());
}

// This is a regression test for https://crbug.com/660801
TEST_F(ApplyBlockElementCommandTest, visibilityChangeDuringCommand) {
  GetDocument().head()->insertAdjacentHTML(
      "afterbegin", "<style>li:first-child { visibility:visible; }</style>",
      ASSERT_NO_EXCEPTION);
  SetBodyContent("<ul style='visibility:hidden'><li>xyz</li></ul>");
  GetDocument().setDesignMode("on");

  UpdateAllLifecyclePhasesForTest();
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position(GetDocument().QuerySelector("li"), 0))
          .Build(),
      SetSelectionOptions());

  auto* command = MakeGarbageCollected<IndentOutdentCommand>(
      GetDocument(), IndentOutdentCommand::kIndent);
  command->Apply();

  EXPECT_EQ(
      "<head><style>li:first-child { visibility:visible; }</style></head>"
      "<body><ul style=\"visibility:hidden\"><ul></ul><li>xyz</li></ul></body>",
      GetDocument().documentElement()->InnerHTMLAsString());
}

// This is a regression test for https://crbug.com/712510
TEST_F(ApplyBlockElementCommandTest, IndentHeadingIntoBlockquote) {
  SetBodyContent(
      "<div contenteditable=\"true\">"
      "<h6><button><table></table></button></h6>"
      "<object></object>"
      "</div>");
  Element* button = GetDocument().QuerySelector("button");
  Element* object = GetDocument().QuerySelector("object");
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(Position(button, 0))
                               .Extend(Position(object, 0))
                               .Build(),
                           SetSelectionOptions());

  auto* command = MakeGarbageCollected<IndentOutdentCommand>(
      GetDocument(), IndentOutdentCommand::kIndent);
  command->Apply();

  // This only records the current behavior, which can be wrong.
  EXPECT_EQ(
      "<div contenteditable=\"true\">"
      "<blockquote style=\"margin: 0 0 0 40px; border: none; padding: 0px;\">"
      "<h6><button></button></h6>"
      "<h6><button><table></table></button></h6>"
      "</blockquote>"
      "<h6><button></button></h6><br>"
      "<object></object>"
      "</div>",
      GetDocument().body()->InnerHTMLAsString());
}

// This is a regression test for https://crbug.com/806525
TEST_F(ApplyBlockElementCommandTest, InsertPlaceHolderAtDisconnectedPosition) {
  GetDocument().setDesignMode("on");
  InsertStyleElement(".input:nth-of-type(2n+1) { visibility:collapse; }");
  Selection().SetSelection(
      SetSelectionTextToBody(
          "^<input><input class=\"input\" style=\"position:absolute\">|"),
      SetSelectionOptions());
  auto* command = MakeGarbageCollected<FormatBlockCommand>(GetDocument(),
                                                           html_names::kPreTag);
  // Crash happens here.
  EXPECT_FALSE(command->Apply());
  EXPECT_EQ(
      "<pre>|<input></pre><input class=\"input\" style=\"position:absolute\">",
      GetSelectionTextFromBody());
}

// https://crbug.com/873084
TEST_F(ApplyBlockElementCommandTest, FormatBlockCrossingUserModifyBoundary) {
  InsertStyleElement("*{-webkit-user-modify:read-write}");
  Selection().SetSelection(
      SetSelectionTextToBody(
          "^<b style=\"-webkit-user-modify:read-only\"><button></button></b>|"),
      SetSelectionOptions());
  auto* command = MakeGarbageCollected<FormatBlockCommand>(GetDocument(),
                                                           html_names::kPreTag);
  // Shouldn't crash here.
  EXPECT_FALSE(command->Apply());
  EXPECT_EQ(
      "^<b style=\"-webkit-user-modify:read-only\"><button>|</button></b>",
      GetSelectionTextFromBody());
}

// https://crbug.com/873084
TEST_F(ApplyBlockElementCommandTest,
       FormatBlockWithTableCrossingUserModifyBoundary) {
  InsertStyleElement("*{-webkit-user-modify:read-write}");
  Selection().SetSelection(
      SetSelectionTextToBody("^<table></table>"
                             "<kbd "
                             "style=\"-webkit-user-modify:read-only\"><button><"
                             "/button></kbd>|"),
      SetSelectionOptions());
  auto* command = MakeGarbageCollected<FormatBlockCommand>(GetDocument(),
                                                           html_names::kPreTag);
  // Shouldn't crash here.
  EXPECT_FALSE(command->Apply());
  EXPECT_EQ(
      "<table>^</table>"
      "<kbd style=\"-webkit-user-modify:read-only\"><button>|</button></kbd>",
      GetSelectionTextFromBody());
}

}  // namespace blink
