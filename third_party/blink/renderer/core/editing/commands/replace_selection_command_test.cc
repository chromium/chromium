// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/replace_selection_command.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/parser_content_policy.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

#include <memory>

namespace blink {

class ReplaceSelectionCommandTest : public EditingTestBase {};

// This is a regression test for https://crbug.com/619131
TEST_F(ReplaceSelectionCommandTest, pastingEmptySpan) {
  GetDocument().setDesignMode("on");
  SetBodyContent("foo");

  LocalFrame* frame = GetDocument().GetFrame();
  frame->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position(GetDocument().body(), 0))
          .Build(),
      SetSelectionOptions());

  DocumentFragment* fragment = GetDocument().createDocumentFragment();
  fragment->AppendChild(GetDocument().CreateRawElement(html_names::kSpanTag));

  // |options| are taken from |Editor::replaceSelectionWithFragment()| with
  // |selectReplacement| and |smartReplace|.
  ReplaceSelectionCommand::CommandOptions options =
      ReplaceSelectionCommand::kPreventNesting |
      ReplaceSelectionCommand::kSanitizeFragment |
      ReplaceSelectionCommand::kSelectReplacement |
      ReplaceSelectionCommand::kSmartReplace;
  auto* command = MakeGarbageCollected<ReplaceSelectionCommand>(
      GetDocument(), fragment, options);

  EXPECT_TRUE(command->Apply()) << "the replace command should have succeeded";
  EXPECT_EQ("foo", GetDocument().body()->innerHTML()) << "no DOM tree mutation";
}

// This is a regression test for https://crbug.com/668808
TEST_F(ReplaceSelectionCommandTest, pasteSpanInText) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  GetDocument().setDesignMode("on");
  SetBodyContent("<b>text</b>");

  Element* b_element = GetDocument().QuerySelector(AtomicString("b"));
  LocalFrame* frame = GetDocument().GetFrame();
  frame->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position(b_element->firstChild(), 1))
          .Build(),
      SetSelectionOptions());

  DocumentFragment* fragment = GetDocument().createDocumentFragment();
  fragment->ParseHTML("<span><div>bar</div></span>", b_element);

  ReplaceSelectionCommand::CommandOptions options = 0;
  auto* command = MakeGarbageCollected<ReplaceSelectionCommand>(
      GetDocument(), fragment, options);

  EXPECT_TRUE(command->Apply()) << "the replace command should have succeeded";
  EXPECT_EQ("<b>t</b>bar<b>ext</b>", GetDocument().body()->innerHTML())
      << "'bar' should have been inserted";
}

// Helper function to set autosizing multipliers on a document.
bool SetTextAutosizingMultiplier(Document* document, float multiplier) {
  bool multiplier_set = false;
  for (LayoutObject* layout_object = document->GetLayoutView(); layout_object;
       layout_object = layout_object->NextInPreOrder()) {
    if (layout_object->Style()) {
      ComputedStyleBuilder builder(layout_object->StyleRef());
      builder.SetTextAutosizingMultiplier(multiplier);
      layout_object->SetStyle(builder.TakeStyle(),
                              LayoutObject::ApplyStyleChanges::kNo);
      multiplier_set = true;
    }
  }
  return multiplier_set;
}

// This is a regression test for https://crbug.com/768261
TEST_F(ReplaceSelectionCommandTest, TextAutosizingDoesntInflateText) {
  GetDocument().GetSettings()->SetTextAutosizingEnabled(true);
  GetDocument().setDesignMode("on");
  SetBodyContent("<div><span style='font-size: 12px;'>foo bar</span></div>");
  SetTextAutosizingMultiplier(&GetDocument(), 2.0);

  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Element* span = GetDocument().QuerySelector(AtomicString("span"));

  // Select "bar".
  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position(span->firstChild(), 4))
          .Extend(Position(span->firstChild(), 7))
          .Build(),
      SetSelectionOptions());

  DocumentFragment* fragment = GetDocument().createDocumentFragment();
  fragment->ParseHTML("baz", span);

  ReplaceSelectionCommand::CommandOptions options =
      ReplaceSelectionCommand::kMatchStyle;

  auto* command = MakeGarbageCollected<ReplaceSelectionCommand>(
      GetDocument(), fragment, options);

  EXPECT_TRUE(command->Apply()) << "the replace command should have succeeded";
  // The span element should not have been split to increase the font size.
  EXPECT_EQ(1u, div->CountChildren());
}

// This is a regression test for https://crbug.com/781282
TEST_F(ReplaceSelectionCommandTest, TrailingNonVisibleTextCrash) {
  GetDocument().setDesignMode("on");
  Selection().SetSelection(SetSelectionTextToBody("<div>^foo|</div>"),
                           SetSelectionOptions());

  DocumentFragment* fragment = GetDocument().createDocumentFragment();
  fragment->ParseHTML("<div>bar</div> ",
                      GetDocument().QuerySelector(AtomicString("div")));
  ReplaceSelectionCommand::CommandOptions options = 0;
  auto* command = MakeGarbageCollected<ReplaceSelectionCommand>(
      GetDocument(), fragment, options);

  // Crash should not occur on applying ReplaceSelectionCommand
  EXPECT_FALSE(command->Apply());
  EXPECT_EQ("<div>bar</div>|<br>", GetSelectionTextFromBody());
}

// This is a regression test for https://crbug.com/796840
TEST_F(ReplaceSelectionCommandTest, CrashWithNoSelection) {
  GetDocument().setDesignMode("on");
  SetBodyContent("<div></div>");
  ReplaceSelectionCommand::CommandOptions options = 0;
  auto* command = MakeGarbageCollected<ReplaceSelectionCommand>(
      GetDocument(), nullptr, options);

  // Crash should not occur on applying ReplaceSelectionCommand
  EXPECT_FALSE(command->Apply());
  EXPECT_EQ("<div></div>", GetSelectionTextFromBody());
}

// http://crbug.com/877127
TEST_F(ReplaceSelectionCommandTest, SmartPlainTextPaste) {
  // After typing "abc", Enter, "def".
  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>abc<div>def</div>|</div>"),
      SetSelectionOptions());
  DocumentFragment& fragment = *GetDocument().createDocumentFragment();
  fragment.appendChild(Text::Create(GetDocument(), "XYZ"));
  const ReplaceSelectionCommand::CommandOptions options =
      ReplaceSelectionCommand::kPreventNesting |
      ReplaceSelectionCommand::kSanitizeFragment |
      ReplaceSelectionCommand::kMatchStyle |
      ReplaceSelectionCommand::kSmartReplace;
  auto& command = *MakeGarbageCollected<ReplaceSelectionCommand>(
      GetDocument(), &fragment, options,
      InputEvent::InputType::kInsertFromPaste);

  EXPECT_TRUE(command.Apply());
  // Smart paste inserts a space before pasted text.
  EXPECT_EQ("<div contenteditable>abc<div>def XYZ|</div></div>",
            GetSelectionTextFromBody());
}

// http://crbug.com/1155687
TEST_F(ReplaceSelectionCommandTest, TableAndImages) {
  GetDocument().setDesignMode("on");
  SetBodyContent("<table>&#x20;<tbody></tbody>&#x20;</table>");
  Element* tbody = GetDocument().QuerySelector(AtomicString("tbody"));
  tbody->AppendChild(GetDocument().CreateRawElement(html_names::kImgTag));
  Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(Position(tbody, 1)).Build(),
      SetSelectionOptions());

  DocumentFragment* fragment = GetDocument().createDocumentFragment();
  fragment->AppendChild(GetDocument().CreateRawElement(html_names::kImgTag));
  auto& command = *MakeGarbageCollected<ReplaceSelectionCommand>(
      GetDocument(), fragment, ReplaceSelectionCommand::kPreventNesting,
      InputEvent::InputType::kNone);

  // Should not crash
  EXPECT_TRUE(command.Apply());
  EXPECT_EQ("<table> <tbody><img><img>|</tbody> </table>",
            GetSelectionTextFromBody());
}

// https://crbug.com/1186610
TEST_F(ReplaceSelectionCommandTest, InsertImageAfterEmptyBlockInInline) {
  GetDocument().setDesignMode("on");
  Selection().SetSelection(SetSelectionTextToBody("<span><div></div>|a</span>"),
                           SetSelectionOptions());

  DocumentFragment& fragment = *GetDocument().createDocumentFragment();
  fragment.appendChild(GetDocument().CreateRawElement(html_names::kImgTag));
  auto& command = *MakeGarbageCollected<ReplaceSelectionCommand>(
      GetDocument(), &fragment, ReplaceSelectionCommand::kPreventNesting,
      InputEvent::InputType::kNone);

  // Should not crash
  EXPECT_TRUE(command.Apply());
  EXPECT_EQ("<span><div></div></span><img>|<span>a</span>",
            GetSelectionTextFromBody());
}

// https://crbug.com/1173134
TEST_F(ReplaceSelectionCommandTest, InsertImageAfterWhiteSpace) {
  GetDocument().setDesignMode("on");
  Selection().SetSelection(
      SetSelectionTextToBody(
          "<button><div></div><svg></svg>&#x20;|</button>x<input>"),
      SetSelectionOptions());

  DocumentFragment& fragment = *GetDocument().createDocumentFragment();
  fragment.appendChild(GetDocument().CreateRawElement(html_names::kImgTag));
  auto& command = *MakeGarbageCollected<ReplaceSelectionCommand>(
      GetDocument(), &fragment, ReplaceSelectionCommand::kPreventNesting,
      InputEvent::InputType::kNone);

  // Should not crash
  EXPECT_TRUE(command.Apply());
  EXPECT_EQ("<button><div></div><svg></svg></button><img>|x<input>",
            GetSelectionTextFromBody());
}

// https://crbug.com/1246674
TEST_F(ReplaceSelectionCommandTest, InsertImageInNonEditableBlock1) {
  GetDocument().setDesignMode("on");
  Selection().SetSelection(
      SetSelectionTextToBody(
          "<div contenteditable=\"false\"><span contenteditable>"
          "a|b</span></div>"),
      SetSelectionOptions());

  DocumentFragment& fragment = *GetDocument().createDocumentFragment();
  fragment.appendChild(GetDocument().CreateRawElement(html_names::kImgTag));
  auto& command = *MakeGarbageCollected<ReplaceSelectionCommand>(
      GetDocument(), &fragment, ReplaceSelectionCommand::kPreventNesting,
      InputEvent::InputType::kNone);

  // Should not crash
  EXPECT_TRUE(command.Apply());
  EXPECT_EQ(
      "<div contenteditable=\"false\"><span contenteditable>"
      "a<img>|<br>b</span></div>",
      GetSelectionTextFromBody());
}

// https://crbug.com/1246674
TEST_F(ReplaceSelectionCommandTest, InsertImageInNonEditableBlock2) {
  GetDocument().setDesignMode("on");
  Selection().SetSelection(
      SetSelectionTextToBody("<strong xml:space><div contenteditable=\"false\">"
                             "<span contenteditable><div>a|b</div></span>"
                             "</div></strong>"),
      SetSelectionOptions());

  DocumentFragment& fragment = *GetDocument().createDocumentFragment();
  fragment.appendChild(GetDocument().CreateRawElement(html_names::kImgTag));
  auto& command = *MakeGarbageCollected<ReplaceSelectionCommand>(
      GetDocument(), &fragment, ReplaceSelectionCommand::kPreventNesting,
      InputEvent::InputType::kNone);

  // Should not crash
  EXPECT_TRUE(command.Apply());
  EXPECT_EQ(
      "<strong xml:space><div contenteditable=\"false\">"
      "<span contenteditable><div>a</div><img>|<div>b</div></span>"
      "</div></strong>",
      GetSelectionTextFromBody());
}
}  // namespace blink
