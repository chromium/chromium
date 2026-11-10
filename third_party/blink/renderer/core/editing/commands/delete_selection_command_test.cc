// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/delete_selection_command.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class DeleteSelectionCommandTest : public EditingTestBase {};

// This is a regression test for https://crbug.com/668765
TEST_F(DeleteSelectionCommandTest, deleteListFromTable) {
  SetBodyContent(
      "<div contenteditable=true>"
      "<table><tr><td><ol>"
      "<li><br></li>"
      "<li>foo</li>"
      "</ol></td></tr></table>"
      "</div>");

  Element* div = QuerySelector("div");
  Element* table = QuerySelector("table");
  Element* br = QuerySelector("br");

  LocalFrame* frame = GetDocument().GetFrame();
  frame->Selection().SetSelection(
      SelectionInDomTree::Builder()
          .Collapse(Position(br, PositionAnchorType::kBeforeAnchor))
          .Extend(Position(table, PositionAnchorType::kAfterAnchor))
          .Build(),
      SetSelectionOptions());

  DeleteSelectionCommand* command =
      MakeGarbageCollected<DeleteSelectionCommand>(
          GetDocument(),
          DeleteSelectionOptions::Builder()
              .SetMergeBlocksAfterDelete(true)
              .SetSanitizeMarkup(true)
              .Build(),
          InputEvent::InputType::kDeleteByCut);

  EXPECT_TRUE(command->Apply()) << "the delete command should have succeeded";
  EXPECT_EQ("<div contenteditable=\"true\"><br></div>",
            GetDocument().body()->GetInnerHTMLString());
  EXPECT_TRUE(frame->Selection().GetSelectionInDomTree().IsCaret());
  EXPECT_EQ(Position(div, 0), frame->Selection()
                                  .ComputeVisibleSelectionInDomTree()
                                  .Anchor()
                                  .ToOffsetInAnchor());
}

TEST_F(DeleteSelectionCommandTest, DeleteIntoObjectFallback) {
  SetBodyContent("<div contenteditable=true><object>abc</object>def</div>");
  auto* object = To<HTMLObjectElement>(QuerySelector("object"));
  object->RenderFallbackContent(
      HTMLObjectElement::ErrorEventPolicy::kDoNotDispatch);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  Selection().SetSelection(SelectionInDomTree::Builder()
                               .Collapse(Position(object->firstChild(), 2))
                               .Extend(Position(object->nextSibling(), 0))
                               .Build(),
                           SetSelectionOptions());

  auto* command = MakeGarbageCollected<DeleteSelectionCommand>(
      GetDocument(),
      DeleteSelectionOptions::Builder()
          .SetMergeBlocksAfterDelete(true)
          .SetSanitizeMarkup(true)
          .Build(),
      InputEvent::InputType::kDeleteContentBackward);
  ASSERT_TRUE(command->Apply());
  EXPECT_EQ("<div contenteditable=\"true\"><object>ab</object>def</div>",
            GetDocument().body()->GetInnerHTMLString());
  EXPECT_TRUE(Selection().GetSelectionInDomTree().IsCaret());
}

TEST_F(DeleteSelectionCommandTest, ForwardRangeKeepsUncheckedBehavior) {
  SetBodyContent("<div contenteditable=true>abc<object>def</object></div>");
  auto* object = To<HTMLObjectElement>(QuerySelector("object"));
  object->RenderFallbackContent(
      HTMLObjectElement::ErrorEventPolicy::kDoNotDispatch);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);

  Selection().SetSelection(SelectionInDomTree::Builder()
                               .Collapse(Position(object->previousSibling(), 3))
                               .Extend(Position(object->firstChild(), 1))
                               .Build(),
                           SetSelectionOptions());

  auto* command = MakeGarbageCollected<DeleteSelectionCommand>(
      GetDocument(),
      DeleteSelectionOptions::Builder()
          .SetMergeBlocksAfterDelete(true)
          .SetSanitizeMarkup(true)
          .Build(),
      InputEvent::InputType::kDeleteContentForward);
  // Correcting the boundary assertions must not change this release outcome.
  EXPECT_FALSE(command->Apply());
  EXPECT_EQ("<div contenteditable=\"true\">abc<object>ef</object></div>",
            GetDocument().body()->GetInnerHTMLString());
  EXPECT_TRUE(Selection().GetSelectionInDomTree().IsRange());
}

// http://crbug.com/1273266
TEST_F(DeleteSelectionCommandTest, FixupWhitespace) {
  // Note: To make |RendersInDifferentPosition()| works correctly, font size
  // should not be 1px.
  InsertStyleElement("body { font-size: 10px; }");
  Selection().SetSelection(
      SetSelectionTextToBody("<p contenteditable>a<b>&#32;^X|</b>&#32;Y</p>"),
      SetSelectionOptions());

  DeleteSelectionCommand& command =
      *MakeGarbageCollected<DeleteSelectionCommand>(
          GetDocument(), DeleteSelectionOptions::Builder()
                             .SetMergeBlocksAfterDelete(true)
                             .SetSanitizeMarkup(true)
                             .Build());
  EXPECT_TRUE(command.Apply()) << "the delete command should have succeeded";
  EXPECT_EQ("<p contenteditable>a<b> |</b>\u00A0Y</p>",
            GetSelectionTextFromBody());
}

TEST_F(DeleteSelectionCommandTest, ForwardDeleteWithFirstLetter) {
  InsertStyleElement("p::first-letter {font-size:200%;}");
  Selection().SetSelection(
      SetSelectionTextToBody("<p contenteditable>a^b|c</p>"),
      SetSelectionOptions());

  DeleteSelectionCommand& command =
      *MakeGarbageCollected<DeleteSelectionCommand>(
          GetDocument(), DeleteSelectionOptions::Builder()
                             .SetMergeBlocksAfterDelete(true)
                             .SetSanitizeMarkup(true)
                             .Build());
  EXPECT_TRUE(command.Apply()) << "the delete command should have succeeded";
  EXPECT_EQ("<p contenteditable>a|c</p>", GetSelectionTextFromBody());
}

// http://crbug.com/1299189
TEST_F(DeleteSelectionCommandTest, DeleteOptionElement) {
  Selection().SetSelection(
      SetSelectionTextToBody("<p contenteditable>"
                             "^<option></option>|"
                             "<select><option>A</option></select>"
                             "</p>"),
      SetSelectionOptions());

  DeleteSelectionCommand& command =
      *MakeGarbageCollected<DeleteSelectionCommand>(
          GetDocument(), DeleteSelectionOptions::Builder()
                             .SetMergeBlocksAfterDelete(true)
                             .SetSanitizeMarkup(true)
                             .Build());
  EXPECT_TRUE(command.Apply()) << "the delete command should have succeeded";
  EXPECT_EQ(
      "<p contenteditable>"
      "^<option><select><option>A</option></select><br></option>|"
      "</p>",
      GetSelectionTextFromBody())
      << "Not sure why we get this.";
}

// This is a regression test for https://crbug.com/1172439
TEST_F(DeleteSelectionCommandTest, DeleteWithEditabilityChange) {
  Selection().SetSelection(
      SetSelectionTextToBody(
          "^<style>body{-webkit-user-modify:read-write}</style>x|"),
      SetSelectionOptions());
  EXPECT_TRUE(IsEditable(*GetDocument().body()));

  DeleteSelectionCommand& command =
      *MakeGarbageCollected<DeleteSelectionCommand>(
          GetDocument(), DeleteSelectionOptions::Builder()
                             .SetMergeBlocksAfterDelete(true)
                             .SetSanitizeMarkup(true)
                             .Build());
  // Should not crash.
  // Editing state is aborted after the body stops being editable.
  EXPECT_FALSE(command.Apply());

  // The command removes the <style>, so the <body> stops being editable,
  // and then "x" is not removed.
  EXPECT_FALSE(IsEditable(*GetDocument().body()));
  EXPECT_EQ("^x|", GetSelectionTextFromBody());
}

// This is a regression test for https://crbug.com/1307391
TEST_F(DeleteSelectionCommandTest, FloatingInputsWithTrailingSpace) {
  GetDocument().setDesignMode("on");
  InsertStyleElement("input { float: left; }");
  Selection().SetSelection(SetSelectionTextToBody("<input>^<input><input>| "),
                           SetSelectionOptions());

  DeleteSelectionCommand& command =
      *MakeGarbageCollected<DeleteSelectionCommand>(
          GetDocument(), DeleteSelectionOptions::NormalDelete());
  // Should not crash.
  EXPECT_TRUE(command.Apply());
  EXPECT_EQ("<input>| ", GetSelectionTextFromBody());
}

}  // namespace blink
