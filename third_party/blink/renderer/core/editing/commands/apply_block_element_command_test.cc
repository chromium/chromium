// Copyright 2016 The Chromium Authors
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
#include "third_party/blink/renderer/core/editing/testing/selection_sample.h"
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
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(GetDocument().documentElement(), 1),
                            Position(GetDocument()
                                         .getElementById(AtomicString("va"))
                                         ->firstChild(),
                                     2))
          .Build(),
      SetSelectionOptions());

  auto* command = MakeGarbageCollected<FormatBlockCommand>(
      GetDocument(), html_names::kFooterTag);
  command->Apply();

  EXPECT_EQ(
      "<head>"
      "<style> .CLASS13 { -webkit-user-modify: read-write; }</style>"
      "</head>foo"
      "<body contenteditable=\"false\">\n"
      "<pre><var id=\"va\" class=\"CLASS13\">\nC\n</var></pre><input></body>",
      GetDocument().documentElement()->innerHTML());
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
          .Collapse(
              Position(GetDocument().QuerySelector(AtomicString("li")), 0))
          .Build(),
      SetSelectionOptions());

  auto* command = MakeGarbageCollected<IndentOutdentCommand>(
      GetDocument(), IndentOutdentCommand::kIndent);
  command->Apply();

  EXPECT_EQ(
      "<head><style>li:first-child { visibility:visible; }</style></head>"
      "<body><ul style=\"visibility:hidden\"><ul></ul><li>xyz</li></ul></body>",
      GetDocument().documentElement()->innerHTML());
}

// This is a regression test for https://crbug.com/712510
TEST_F(ApplyBlockElementCommandTest, IndentHeadingIntoBlockquote) {
  SetBodyContent(
      "<div contenteditable=\"true\">"
      "<h6><button><table></table></button></h6>"
      "<object></object>"
      "</div>");
  Element* button = GetDocument().QuerySelector(AtomicString("button"));
  Element* object = GetDocument().QuerySelector(AtomicString("object"));
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
      "<br>"
      "<object></object>"
      "</div>",
      GetDocument().body()->innerHTML());
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
  EXPECT_TRUE(command->Apply());
  EXPECT_EQ(
      "<pre>^<input>|</pre><input class=\"input\" style=\"position:absolute\">",
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
  EXPECT_TRUE(command->Apply());
  EXPECT_EQ(
      "<pre>|<br></pre>"
      "<b style=\"-webkit-user-modify:read-only\"><button></button></b>",
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
  EXPECT_TRUE(command->Apply());
  EXPECT_EQ(
      "<pre><table>|</table></pre>"
      "<kbd style=\"-webkit-user-modify:read-only\"><button></button></kbd>",
      GetSelectionTextFromBody());
}

// https://crbug.com/1172656
TEST_F(ApplyBlockElementCommandTest, FormatBlockWithDirectChildrenOfRoot) {
  GetDocument().setDesignMode("on");
  DocumentFragment* fragment = DocumentFragment::Create(GetDocument());
  Element* root = GetDocument().documentElement();
  fragment->ParseXML("a<div>b</div>c", root);
  root->setTextContent("");
  root->appendChild(fragment);
  UpdateAllLifecyclePhasesForTest();

  Selection().SetSelection(
      SelectionInDOMTree::Builder().SelectAllChildren(*root).Build(),
      SetSelectionOptions());
  auto* command = MakeGarbageCollected<FormatBlockCommand>(GetDocument(),
                                                           html_names::kPreTag);
  // Shouldn't crash here.
  EXPECT_FALSE(command->Apply());
  const SelectionInDOMTree& selection = Selection().GetSelectionInDOMTree();
  EXPECT_EQ("^a<div>b</div>c|",
            SelectionSample::GetSelectionText(*root, selection));
}

// This is a regression test for https://crbug.com/1180699
TEST_F(ApplyBlockElementCommandTest, OutdentEmptyBlockquote) {
  Vector<std::string> selection_texts = {
      "<blockquote style='padding:5px'>|</blockquote>",
      "a<blockquote style='padding:5px'>|</blockquote>",
      "<blockquote style='padding:5px'>|</blockquote>b",
      "a<blockquote style='padding:5px'>|</blockquote>b"};
  Vector<std::string> expectations = {"|", "a|<br>", "|<br>b", "a<br>|b"};

  GetDocument().setDesignMode("on");
  for (unsigned i = 0; i < selection_texts.size(); ++i) {
    Selection().SetSelection(SetSelectionTextToBody(selection_texts[i]),
                             SetSelectionOptions());
    auto* command = MakeGarbageCollected<IndentOutdentCommand>(
        GetDocument(), IndentOutdentCommand::kOutdent);

    // Shouldn't crash here.
    command->Apply();
    EXPECT_EQ(expectations[i], GetSelectionTextFromBody());
  }
}

// This is a regression test for https://crbug.com/1188871
TEST_F(ApplyBlockElementCommandTest, IndentSVGWithTable) {
  GetDocument().setDesignMode("on");
  Selection().SetSelection(SetSelectionTextToBody("<svg><foreignObject>|"
                                                  "<table>&#x20;</table>&#x20;x"
                                                  "</foreignObject></svg>"),
                           SetSelectionOptions());
  auto* command = MakeGarbageCollected<IndentOutdentCommand>(
      GetDocument(), IndentOutdentCommand::kIndent);

  // Shouldn't crash here.
  EXPECT_TRUE(command->Apply());
  EXPECT_EQ(
      "<blockquote style=\"margin: 0 0 0 40px; border: none; padding: 0px;\">"
      "<svg><foreignObject><table>| </table></foreignObject></svg>"
      "</blockquote>"
      "<svg><foreignObject> x</foreignObject></svg>",
      GetSelectionTextFromBody());
}

// This is a regression test for https://crbug.com/673056
TEST_F(ApplyBlockElementCommandTest, IndentOutdentLinesDoubleBr) {
  Selection().SetSelection(SetSelectionTextToBody("<div contenteditable>"
                                                  "|a<br><br>"
                                                  "b"
                                                  "</div>"),
                           SetSelectionOptions());

  auto* indent = MakeGarbageCollected<IndentOutdentCommand>(
      GetDocument(), IndentOutdentCommand::kIndent);
  EXPECT_TRUE(indent->Apply());

  EXPECT_EQ(
      "<div contenteditable>"
      "<blockquote style=\"margin: 0 0 0 40px; border: none; padding: 0px;\">"
      "|a"
      "</blockquote>"
      "<br>"
      "b"
      "</div>",
      GetSelectionTextFromBody());

  auto* outdent = MakeGarbageCollected<IndentOutdentCommand>(
      GetDocument(), IndentOutdentCommand::kOutdent);

  // When moving "a" out of the blockquote, the empty line should be preserved.
  EXPECT_TRUE(outdent->Apply());
  EXPECT_EQ(
      "<div contenteditable>"
      "|a"
      "<br>"
      "<br>"
      "b"
      "</div>",
      GetSelectionTextFromBody());
}

// This is a regression test for https://crbug.com/673056
TEST_F(ApplyBlockElementCommandTest, IndentOutdentLinesCrash) {
  Selection().SetSelection(SetSelectionTextToBody("<div contenteditable>"
                                                  "^a<br>"
                                                  "b|<br><br>"
                                                  "c"
                                                  "</div>"),
                           SetSelectionOptions());

  auto* indent = MakeGarbageCollected<IndentOutdentCommand>(
      GetDocument(), IndentOutdentCommand::kIndent);

  EXPECT_TRUE(indent->Apply());
  EXPECT_EQ(
      "<div contenteditable>"
      "<blockquote style=\"margin: 0 0 0 40px; border: none; padding: 0px;\">"
      "^a<br>"
      "b|"
      "</blockquote>"
      "<br>"
      "c"
      "</div>",
      GetSelectionTextFromBody());

  auto* outdent = MakeGarbageCollected<IndentOutdentCommand>(
      GetDocument(), IndentOutdentCommand::kOutdent);

  // Shouldn't crash, and the empty line between b and c should be preserved.
  EXPECT_TRUE(outdent->Apply());
  EXPECT_EQ(
      "<div contenteditable>"
      "^a<br>"
      "b|<br><br>"
      "c"
      "</div>",
      GetSelectionTextFromBody());
}

// This is a regression test for https://crbug.com/673056
TEST_F(ApplyBlockElementCommandTest, IndentOutdentLinesWithJunkCrash) {
  Selection().SetSelection(SetSelectionTextToBody("<div contenteditable>"
                                                  "^a<br>"
                                                  "b|<br>"
                                                  "<!----><br>"
                                                  "c"
                                                  "</div>"),
                           SetSelectionOptions());

  auto* indent = MakeGarbageCollected<IndentOutdentCommand>(
      GetDocument(), IndentOutdentCommand::kIndent);

  EXPECT_TRUE(indent->Apply());
  EXPECT_EQ(
      "<div contenteditable>"
      "<blockquote style=\"margin: 0 0 0 40px; border: none; padding: 0px;\">"
      "^a<br>"
      "b|"
      "</blockquote>"
      "<!----><br>"
      "c"
      "</div>",
      GetSelectionTextFromBody());

  auto* outdent = MakeGarbageCollected<IndentOutdentCommand>(
      GetDocument(), IndentOutdentCommand::kOutdent);

  // Shouldn't crash.
  EXPECT_TRUE(outdent->Apply());

  // TODO(editing-dev): The result is wrong. We should preserve the empty line
  // between b and c.
  EXPECT_EQ(
      "<div contenteditable>"
      "^a<br>"
      "b|"
      "<!----><br>"
      "c"
      "</div>",
      GetSelectionTextFromBody());
}

// http://crbug.com/1264470
TEST_F(ApplyBlockElementCommandTest, SplitTextNodeWithJustNewline) {
  InsertStyleElement("b {-webkit-text-security: square;}");
  Selection().SetSelection(SetSelectionTextToBody("<pre contenteditable>"
                                                  "<b>|<p>X</p>\n</b>"
                                                  "</pre>"),
                           SetSelectionOptions());

  auto* const format_block = MakeGarbageCollected<FormatBlockCommand>(
      GetDocument(), html_names::kDivTag);

  ASSERT_TRUE(format_block->Apply());
  EXPECT_EQ("<pre contenteditable><b><div>|X</div>\n</b></pre>",
            GetSelectionTextFromBody());
}

}  // namespace blink
