// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_selection.h"

#include <string>

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_focus_options.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_modifier.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/set_selection_options.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_paragraph_element.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_position.h"
#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_selection_test.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace test {

//
// Basic tests.
//

TEST_F(AccessibilitySelectionTest, FromCurrentSelection) {
  GetPage().GetSettings().SetScriptEnabled(true);
  SetBodyInnerHTML(R"HTML(
      <p id="paragraph1">Hello.</p>
      <p id="paragraph2">How are you?</p>
      )HTML");

  GetDocument().ExistingAXObjectCache()->UpdateAXForAllDocuments();
  ASSERT_FALSE(AXSelection::FromCurrentSelection(GetDocument()).IsValid());

  Element* const script_element =
      GetDocument().CreateRawElement(html_names::kScriptTag);
  ASSERT_NE(nullptr, script_element);
  script_element->setTextContent(R"SCRIPT(
      let text1 = document.querySelectorAll('p')[0].firstChild;
      let paragraph2 = document.querySelectorAll('p')[1];
      let range = document.createRange();
      range.setStart(text1, 3);
      range.setEnd(paragraph2, 1);
      let selection = getSelection();
      selection.removeAllRanges();
      selection.addRange(range);
      )SCRIPT");
  GetDocument().body()->AppendChild(script_element);

  const AXObject* ax_static_text_1 =
      GetAXObjectByElementId("paragraph1")->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text_1);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text_1->RoleValue());
  const AXObject* ax_paragraph_2 = GetAXObjectByElementId("paragraph2");
  ASSERT_NE(nullptr, ax_paragraph_2);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph_2->RoleValue());

  GetDocument().ExistingAXObjectCache()->UpdateAXForAllDocuments();
  const auto ax_selection = AXSelection::FromCurrentSelection(GetDocument());
  ASSERT_TRUE(ax_selection.IsValid());

  ASSERT_TRUE(ax_selection.Anchor().IsTextPosition());
  EXPECT_EQ(ax_static_text_1, ax_selection.Anchor().ContainerObject());
  EXPECT_EQ(3, ax_selection.Anchor().TextOffset());

  ASSERT_FALSE(ax_selection.Focus().IsTextPosition());
  EXPECT_EQ(ax_paragraph_2, ax_selection.Focus().ContainerObject());
  EXPECT_EQ(1, ax_selection.Focus().ChildIndex());

  EXPECT_EQ(
      "++<GenericContainer>\n"
      "++++<GenericContainer>\n"
      "++++++<Paragraph>\n"
      "++++++++<StaticText: Hel^lo.>\n"
      "++++++<Paragraph>\n"
      "++++++++<StaticText: How are you?>\n|",
      GetSelectionText(ax_selection));
}

TEST_F(AccessibilitySelectionTest, FromCurrentSelectionSelectAll) {
  SetBodyInnerHTML(R"HTML(
      <p id="paragraph1">Hello.</p>
      <p id="paragraph2">How are you?</p>
      )HTML");

  ASSERT_FALSE(AXSelection::FromCurrentSelection(GetDocument()).IsValid());
  Selection().SelectAll(SetSelectionBy::kUser);
  UpdateAllLifecyclePhasesForTest();
  ASSERT_NE(nullptr, GetAXRootObject());

  const auto ax_selection = AXSelection::FromCurrentSelection(GetDocument());
  ASSERT_TRUE(ax_selection.IsValid());

  ASSERT_FALSE(ax_selection.Anchor().IsTextPosition());

  AXObject* html_object = GetAXRootObject()->ChildAtIncludingIgnored(0);
  ASSERT_NE(nullptr, html_object);
  EXPECT_EQ(html_object, ax_selection.Anchor().ContainerObject());
  EXPECT_EQ(0, ax_selection.Anchor().ChildIndex());

  ASSERT_FALSE(ax_selection.Focus().IsTextPosition());
  EXPECT_EQ(html_object, ax_selection.Focus().ContainerObject());
  EXPECT_EQ(html_object->ChildCountIncludingIgnored(),
            ax_selection.Focus().ChildIndex());

  EXPECT_EQ(
      "++<GenericContainer>\n"
      "^++++<GenericContainer>\n"
      "++++++<Paragraph>\n"
      "++++++++<StaticText: Hello.>\n"
      "++++++<Paragraph>\n"
      "++++++++<StaticText: How are you?>\n|",
      GetSelectionText(ax_selection));
}

TEST_F(AccessibilitySelectionTest, ClearCurrentSelection) {
  GetPage().GetSettings().SetScriptEnabled(true);
  SetBodyInnerHTML(R"HTML(
      <p>Hello.</p>
      <p>How are you?</p>
      )HTML");

  Element* const script_element =
      GetDocument().CreateRawElement(html_names::kScriptTag);
  ASSERT_NE(nullptr, script_element);
  script_element->setTextContent(R"SCRIPT(
      let text1 = document.querySelectorAll('p')[0].firstChild;
      let paragraph2 = document.querySelectorAll('p')[1];
      let range = document.createRange();
      range.setStart(text1, 3);
      range.setEnd(paragraph2, 1);
      let selection = getSelection();
      selection.removeAllRanges();
      selection.addRange(range);
      )SCRIPT");
  GetDocument().body()->AppendChild(script_element);
  UpdateAllLifecyclePhasesForTest();

  SelectionInDOMTree selection = Selection().GetSelectionInDOMTree();
  ASSERT_FALSE(selection.IsNone());

  AXSelection::ClearCurrentSelection(GetDocument());
  selection = Selection().GetSelectionInDOMTree();
  EXPECT_TRUE(selection.IsNone());

  const auto ax_selection = AXSelection::FromCurrentSelection(GetDocument());
  EXPECT_FALSE(ax_selection.IsValid());
  EXPECT_EQ("", GetSelectionText(ax_selection));
}

TEST_F(AccessibilitySelectionTest, CancelSelect) {
  GetPage().GetSettings().SetScriptEnabled(true);
  SetBodyInnerHTML(R"HTML(
      <p id="paragraph1">Hello.</p>
      <p id="paragraph2">How are you?</p>
      )HTML");

  Element* const script_element =
      GetDocument().CreateRawElement(html_names::kScriptTag);
  ASSERT_NE(nullptr, script_element);
  script_element->setTextContent(R"SCRIPT(
      document.addEventListener("selectstart", (e) => {
        e.preventDefault();
      }, false);
      )SCRIPT");
  GetDocument().body()->AppendChild(script_element);
  UpdateAllLifecyclePhasesForTest();

  const AXObject* ax_static_text_1 =
      GetAXObjectByElementId("paragraph1")->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text_1);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text_1->RoleValue());
  const AXObject* ax_paragraph_2 = GetAXObjectByElementId("paragraph2");
  ASSERT_NE(nullptr, ax_paragraph_2);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph_2->RoleValue());

  AXSelection::Builder builder;
  AXSelection ax_selection =
      builder
          .SetAnchor(
              AXPosition::CreatePositionInTextObject(*ax_static_text_1, 3))
          .SetFocus(AXPosition::CreateLastPositionInObject(*ax_paragraph_2))
          .Build();

  EXPECT_FALSE(ax_selection.Select()) << "The operation has been cancelled.";
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(AXSelection::FromCurrentSelection(GetDocument()).IsValid());

  GetDocument().RemoveAllEventListeners();

  EXPECT_TRUE(ax_selection.Select()) << "The operation should now go through.";
  EXPECT_FALSE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_EQ(
      "++<GenericContainer>\n"
      "++++<GenericContainer>\n"
      "++++++<Paragraph>\n"
      "++++++++<StaticText: Hel^lo.>\n"
      "++++++<Paragraph>\n"
      "++++++++<StaticText: How are you?>\n|",
      GetSelectionText(AXSelection::FromCurrentSelection(GetDocument())));
}

TEST_F(AccessibilitySelectionTest, DocumentRangeMatchesSelection) {
  SetBodyInnerHTML(R"HTML(
      <p id="paragraph1">Hello.</p>
      <p id="paragraph2">How are you?</p>
      )HTML");

  const AXObject* ax_static_text_1 =
      GetAXObjectByElementId("paragraph1")->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text_1);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text_1->RoleValue());
  const AXObject* ax_paragraph_2 = GetAXObjectByElementId("paragraph2");
  ASSERT_NE(nullptr, ax_paragraph_2);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph_2->RoleValue());

  AXSelection::Builder builder;
  AXSelection ax_selection =
      builder
          .SetAnchor(
              AXPosition::CreatePositionInTextObject(*ax_static_text_1, 3))
          .SetFocus(AXPosition::CreateLastPositionInObject(*ax_paragraph_2))
          .Build();
  EXPECT_TRUE(ax_selection.Select());
  ASSERT_FALSE(Selection().GetSelectionInDOMTree().IsNone());
  ASSERT_NE(nullptr, Selection().DocumentCachedRange());
  EXPECT_EQ(String("lo.\n      How are you?"),
            Selection().DocumentCachedRange()->toString());
}

TEST_F(AccessibilitySelectionTest, SetSelectionInText) {
  SetBodyInnerHTML(R"HTML(<p id="paragraph">Hello</p>)HTML");

  const Node* text =
      GetDocument().QuerySelector(AtomicString("p"))->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());

  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue());

  const auto ax_base =
      AXPosition::CreatePositionInTextObject(*ax_static_text, 3);
  const auto ax_extent = AXPosition::CreatePositionAfterObject(*ax_static_text);

  AXSelection::Builder builder;
  const AXSelection ax_selection =
      builder.SetAnchor(ax_base).SetFocus(ax_extent).Build();
  const SelectionInDOMTree dom_selection = ax_selection.AsSelection();
  EXPECT_EQ(text, dom_selection.Anchor().AnchorNode());
  EXPECT_EQ(3, dom_selection.Anchor().OffsetInContainerNode());
  EXPECT_EQ(text, dom_selection.Focus().AnchorNode());
  EXPECT_EQ(5, dom_selection.Focus().OffsetInContainerNode());
  EXPECT_EQ(
      "++<GenericContainer>\n"
      "++++<GenericContainer>\n"
      "++++++<Paragraph>\n"
      "++++++++<StaticText: Hel^lo|>\n",
      GetSelectionText(ax_selection));
}

TEST_F(AccessibilitySelectionTest, SetSelectionInMultilineTextarea) {
// On Android we use an ifdef to disable inline text boxes.
#if !BUILDFLAG(IS_ANDROID)
  ui::AXMode mode(ui::kAXModeComplete);
  mode.set_mode(ui::AXMode::kInlineTextBoxes, true);
  ax_context_->SetAXMode(mode);
  GetAXObjectCache().MarkDocumentDirty();
  GetAXObjectCache().UpdateAXForAllDocuments();

  LoadAhem();

  SetBodyInnerHTML(R"HTML(
    <textarea id="txt" style="width:80px; height:81px; font-family: Ahem; font-size: 4;">hello text go blue</textarea>
    )HTML");
  // This HTML generates the following ax tree:
  // id#=13 rootWebArea
  // ++id#=14 genericContainer
  // ++++id#=15 genericContainer
  // ++++++id#=16 textField
  // ++++++++id#=17 genericContainer
  // ++++++++++id#=18 staticText name='hello text go blue<newline>'
  // ++++++++++++id#=20 inlineTextBox name='hello'
  // ++++++++++++id#=22 inlineTextBox name='text'
  // ++++++++++++id#=22 inlineTextBox name='go'
  // ++++++++++++id#=22 inlineTextBox name='blue'

  Element* const textarea =
      GetDocument().QuerySelector(AtomicString("textarea"));
  ASSERT_NE(nullptr, textarea);
  ASSERT_TRUE(IsTextControl(textarea));
  textarea->Focus(FocusOptions::Create());
  ASSERT_TRUE(textarea->IsFocusedElementInDocument());

  const AXObject* ax_textarea = GetAXObjectByElementId("txt");
  ASSERT_NE(nullptr, ax_textarea);
  ASSERT_EQ(ax::mojom::Role::kTextField, ax_textarea->RoleValue());

  AXObject* ax_inline_text_box = ax_textarea->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax_inline_text_box->RoleValue(),
            ax::mojom::Role::kGenericContainer);

  ax_inline_text_box = ax_inline_text_box->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax_inline_text_box->ComputedName(), "hello text go blue");
  ASSERT_EQ(ax_inline_text_box->RoleValue(), ax::mojom::Role::kStaticText);

  ax_inline_text_box = ax_inline_text_box->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax_inline_text_box->ComputedName(), "hello");
  ASSERT_EQ(ax_inline_text_box->RoleValue(), ax::mojom::Role::kInlineTextBox);

  ax_inline_text_box = ax_inline_text_box->NextSiblingIncludingIgnored()
                           ->NextSiblingIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax_inline_text_box->RoleValue(), ax::mojom::Role::kInlineTextBox);
  ASSERT_EQ(ax_inline_text_box->ComputedName(), "text");

  ax_inline_text_box = ax_inline_text_box->NextSiblingIncludingIgnored()
                           ->NextSiblingIncludingIgnored();
  ASSERT_NE(nullptr, ax_inline_text_box);
  ASSERT_EQ(ax_inline_text_box->RoleValue(), ax::mojom::Role::kInlineTextBox);
  ASSERT_EQ(ax_inline_text_box->ComputedName(), "go");

  const auto ax_base =
      AXPosition::CreatePositionInTextObject(*ax_inline_text_box, 0);
  const auto ax_extent =
      AXPosition::CreatePositionInTextObject(*ax_inline_text_box, 2);

  AXSelection::Builder builder;
  AXSelection ax_selection =
      builder.SetAnchor(ax_base).SetFocus(ax_extent).Build();

  EXPECT_TRUE(ax_selection.Select());

  // Even though the selection is set to offsets 0,4 "text" in the inline text
  // box, the selection needs to end up in offsets 12,16 on the whole textarea
  // so that "text" is the selection.
  EXPECT_EQ(11u, ToTextControl(*textarea).selectionStart());
  EXPECT_EQ(13u, ToTextControl(*textarea).selectionEnd());
#endif  // !BUILDFLAG(IS_ANDROID)
}

TEST_F(AccessibilitySelectionTest, SetSelectionInTextWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<p id="paragraph">     Hello</p>)HTML");

  const Node* text =
      GetDocument().QuerySelector(AtomicString("p"))->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());

  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue());

  const auto ax_base =
      AXPosition::CreatePositionInTextObject(*ax_static_text, 3);
  const auto ax_extent = AXPosition::CreatePositionAfterObject(*ax_static_text);

  AXSelection::Builder builder;
  const AXSelection ax_selection =
      builder.SetAnchor(ax_base).SetFocus(ax_extent).Build();
  const SelectionInDOMTree dom_selection = ax_selection.AsSelection();
  EXPECT_EQ(text, dom_selection.Anchor().AnchorNode());
  EXPECT_EQ(8, dom_selection.Anchor().OffsetInContainerNode());
  EXPECT_EQ(text, dom_selection.Focus().AnchorNode());
  EXPECT_EQ(10, dom_selection.Focus().OffsetInContainerNode());
  EXPECT_EQ(
      "++<GenericContainer>\n"
      "++++<GenericContainer>\n"
      "++++++<Paragraph>\n"
      "++++++++<StaticText: Hel^lo|>\n",
      GetSelectionText(ax_selection));
}

TEST_F(AccessibilitySelectionTest, SetSelectionAcrossLineBreak) {
  SetBodyInnerHTML(R"HTML(
      <p id="paragraph">Hello<br id="br">How are you.</p>
      )HTML");

  const Node* paragraph = GetDocument().QuerySelector(AtomicString("p"));
  ASSERT_NE(nullptr, paragraph);
  ASSERT_TRUE(IsA<HTMLParagraphElement>(paragraph));
  const Node* br = GetDocument().QuerySelector(AtomicString("br"));
  ASSERT_NE(nullptr, br);
  ASSERT_TRUE(IsA<HTMLBRElement>(br));
  const Node* line2 =
      GetDocument().QuerySelector(AtomicString("p"))->lastChild();
  ASSERT_NE(nullptr, line2);
  ASSERT_TRUE(line2->IsTextNode());

  const AXObject* ax_br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, ax_br);
  ASSERT_EQ(ax::mojom::Role::kLineBreak, ax_br->RoleValue());
  const AXObject* ax_line2 =
      GetAXObjectByElementId("paragraph")->LastChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_line2);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_line2->RoleValue());

  const auto ax_base = AXPosition::CreatePositionBeforeObject(*ax_br);
  const auto ax_extent = AXPosition::CreatePositionInTextObject(*ax_line2, 0);

  AXSelection::Builder builder;
  const AXSelection ax_selection =
      builder.SetAnchor(ax_base).SetFocus(ax_extent).Build();
  const SelectionInDOMTree dom_selection = ax_selection.AsSelection();
  EXPECT_EQ(paragraph, dom_selection.Anchor().AnchorNode());
  EXPECT_EQ(1, dom_selection.Anchor().OffsetInContainerNode());
  EXPECT_EQ(line2, dom_selection.Focus().AnchorNode());
  EXPECT_EQ(0, dom_selection.Focus().OffsetInContainerNode());

  // The selection anchor marker '^' should be before the line break and the
  // selection focus marker '|' should be after it.
  EXPECT_EQ(
      "++<GenericContainer>\n"
      "++++<GenericContainer>\n"
      "++++++<Paragraph>\n"
      "++++++++<StaticText: Hello>\n"
      "^++++++++<LineBreak: \n>\n"
      "|++++++++<StaticText: |How are you.>\n",
      GetSelectionText(ax_selection));
}

TEST_F(AccessibilitySelectionTest, SetSelectionAcrossLineBreakInEditableText) {
  SetBodyInnerHTML(R"HTML(
      <p contenteditable id="paragraph">Hello<br id="br">How are you.</p>
      )HTML");

  const Node* paragraph = GetDocument().QuerySelector(AtomicString("p"));
  ASSERT_NE(nullptr, paragraph);
  ASSERT_TRUE(IsA<HTMLParagraphElement>(paragraph));
  const Node* br = GetDocument().QuerySelector(AtomicString("br"));
  ASSERT_NE(nullptr, br);
  ASSERT_TRUE(IsA<HTMLBRElement>(br));
  const Node* line2 =
      GetDocument().QuerySelector(AtomicString("p"))->lastChild();
  ASSERT_NE(nullptr, line2);
  ASSERT_TRUE(line2->IsTextNode());

  const AXObject* ax_br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, ax_br);
  ASSERT_EQ(ax::mojom::Role::kLineBreak, ax_br->RoleValue());
  const AXObject* ax_line2 =
      GetAXObjectByElementId("paragraph")->LastChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_line2);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_line2->RoleValue());

  const auto ax_base = AXPosition::CreatePositionBeforeObject(*ax_br);
  // In the case of text objects, the deep equivalent position should always be
  // returned, i.e. a text position before the first character.
  const auto ax_extent = AXPosition::CreatePositionBeforeObject(*ax_line2);

  AXSelection::Builder builder;
  const AXSelection ax_selection =
      builder.SetAnchor(ax_base).SetFocus(ax_extent).Build();
  const SelectionInDOMTree dom_selection = ax_selection.AsSelection();
  EXPECT_EQ(paragraph, dom_selection.Anchor().AnchorNode());
  EXPECT_EQ(1, dom_selection.Anchor().OffsetInContainerNode());
  EXPECT_EQ(line2, dom_selection.Focus().AnchorNode());
  EXPECT_EQ(0, dom_selection.Focus().OffsetInContainerNode());

  // The selection anchor marker '^' should be before the line break and the
  // selection focus marker '|' should be after it.
  EXPECT_EQ(
      "++<GenericContainer>\n"
      "++++<GenericContainer>\n"
      "++++++<Paragraph>\n"
      "++++++++<StaticText: Hello>\n"
      "^++++++++<LineBreak: \n>\n"
      "|++++++++<StaticText: |How are you.>\n",
      GetSelectionText(ax_selection));
}

//
// Get selection tests.
// Retrieving a selection with endpoints which have corresponding ignored
// objects in the accessibility tree, e.g. which are display:none, should shrink
// or extend the |AXSelection| to valid endpoints.
// Note: aria-describedby adds hidden target subtrees to the a11y tree as
// "ignored but included in tree".
//

TEST_F(AccessibilitySelectionTest, SetSelectionInDisplayNone) {
  SetBodyInnerHTML(R"HTML(
      <div id="main" role="main" aria-describedby="hidden1 hidden2">
        <p id="beforeHidden">Before display:none.</p>
        <p id="hidden1" style="display:none">Display:none 1.</p>
        <p id="betweenHidden">In between two display:none elements.</p>
        <p id="hidden2" style="display:none">Display:none 2.</p>
        <p id="afterHidden">After display:none.</p>
      </div>
      )HTML");

  const Node* hidden_1 = GetElementById("hidden1");
  ASSERT_NE(nullptr, hidden_1);
  const Node* hidden_2 = GetElementById("hidden2");
  ASSERT_NE(nullptr, hidden_2);

  const AXObject* ax_main = GetAXObjectByElementId("main");
  ASSERT_NE(nullptr, ax_main);
  ASSERT_EQ(ax::mojom::Role::kMain, ax_main->RoleValue());
  const AXObject* ax_before = GetAXObjectByElementId("beforeHidden");
  ASSERT_NE(nullptr, ax_before);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_before->RoleValue());
  const AXObject* ax_hidden1 = GetAXObjectByElementId("hidden1");
  ASSERT_NE(nullptr, ax_hidden1);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_hidden1->RoleValue());
  ASSERT_TRUE(ax_hidden1->IsIgnored());
  ASSERT_TRUE(ax_hidden1->IsIncludedInTree());
  const AXObject* ax_hidden1_text = ax_hidden1->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_hidden1_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_hidden1_text->RoleValue());
  ASSERT_TRUE(ax_hidden1_text->IsIgnored());
  ASSERT_TRUE(ax_hidden1_text->IsIncludedInTree());
  const AXObject* ax_between = GetAXObjectByElementId("betweenHidden");
  ASSERT_NE(nullptr, ax_between);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_between->RoleValue());
  const AXObject* ax_hidden2 = GetAXObjectByElementId("hidden2");
  ASSERT_NE(nullptr, ax_hidden2);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_hidden2->RoleValue());
  ASSERT_TRUE(ax_hidden2->IsIgnored());
  ASSERT_TRUE(ax_hidden2->IsIncludedInTree());
  const AXObject* ax_hidden2_text = ax_hidden2->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_hidden2_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_hidden2_text->RoleValue());
  ASSERT_TRUE(ax_hidden2_text->IsIgnored());
  ASSERT_TRUE(ax_hidden2_text->IsIncludedInTree());
  const AXObject* ax_after = GetAXObjectByElementId("afterHidden");
  ASSERT_NE(nullptr, ax_after);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_after->RoleValue());

  const auto hidden_1_first = Position::FirstPositionInNode(*hidden_1);
  const auto hidden_2_first = Position::FirstPositionInNode(*hidden_2);
  const auto selection = SelectionInDOMTree::Builder()
                             .SetBaseAndExtent(hidden_1_first, hidden_2_first)
                             .Build();

  const auto ax_selection_shrink = AXSelection::FromSelection(
      selection, AXSelectionBehavior::kShrinkToValidRange);
  const auto ax_selection_extend = AXSelection::FromSelection(
      selection, AXSelectionBehavior::kExtendToValidRange);

  // The "display: none" content is included in the AXTree as an ignored node,
  // so shrunk selection should include those AXObjects. The tree in the browser
  // process also includes those ignored nodes, and the position will be
  // adjusted according to AXPosition rules; in particular, a position anchored
  // before a text node is explicitly moved to before the first character of the
  // text object.
  ASSERT_TRUE(ax_selection_shrink.Anchor().IsTextPosition());
  EXPECT_EQ(ax_hidden1_text, ax_selection_shrink.Anchor().ContainerObject());
  EXPECT_EQ(0, ax_selection_shrink.Anchor().TextOffset());
  ASSERT_TRUE(ax_selection_shrink.Focus().IsTextPosition());
  EXPECT_EQ(ax_hidden2_text, ax_selection_shrink.Focus().ContainerObject());
  EXPECT_EQ(0, ax_selection_shrink.Focus().TextOffset());

  // The extended selection should start in the "display: none" content because
  // they are included in the AXTree. Similarly to above, the position will be
  // adjusted to point to the first character of the text object.
  ASSERT_TRUE(ax_selection_extend.Anchor().IsTextPosition());
  EXPECT_EQ(ax_hidden1_text, ax_selection_extend.Anchor().ContainerObject());
  EXPECT_EQ(0, ax_selection_extend.Anchor().TextOffset());
  ASSERT_TRUE(ax_selection_extend.Focus().IsTextPosition());
  EXPECT_EQ(ax_hidden2_text, ax_selection_extend.Focus().ContainerObject());
  EXPECT_EQ(0, ax_selection_extend.Focus().TextOffset());

  // Even though the two AX selections have different anchors and foci, the text
  // selected in the accessibility tree should not differ, because any
  // differences in the equivalent DOM selections concern elements that are
  // display:none. However, the AX selections should still differ if converted
  // to DOM selections.
  const std::string selection_text(
      "++<GenericContainer>\n"
      "++++<GenericContainer>\n"
      "++++++<Main>\n"
      "++++++++<Paragraph>\n"
      "++++++++++<StaticText: Before display:none.>\n"
      "++++++++<Paragraph>\n"
      "^++++++++++<StaticText: ^Display:none 1.>\n"
      "++++++++<Paragraph>\n"
      "++++++++++<StaticText: In between two display:none elements.>\n"
      "++++++++<Paragraph>\n"
      "|++++++++++<StaticText: |Display:none 2.>\n"
      "++++++++<Paragraph>\n"
      "++++++++++<StaticText: After display:none.>\n");
  EXPECT_EQ(selection_text, GetSelectionText(ax_selection_shrink));
  EXPECT_EQ(selection_text, GetSelectionText(ax_selection_extend));
}

//
// Set selection tests.
// Setting the selection from an |AXSelection| that has endpoints which are not
// present in the layout tree should shrink or extend the selection to visible
// endpoints.
//

TEST_F(AccessibilitySelectionTest, SetSelectionAroundListBullet) {
  SetBodyInnerHTML(R"HTML(
      <div role="main">
        <ul>
          <li id="item1">Item 1.</li>
          <li id="item2">Item 2.</li>
        </ul>
      </div>
      )HTML");

  const Node* item_1 = GetElementById("item1");
  ASSERT_NE(nullptr, item_1);
  ASSERT_FALSE(item_1->IsTextNode());
  const Node* text_1 = item_1->firstChild();
  ASSERT_NE(nullptr, text_1);
  ASSERT_TRUE(text_1->IsTextNode());
  const Node* item_2 = GetElementById("item2");
  ASSERT_NE(nullptr, item_2);
  ASSERT_FALSE(item_2->IsTextNode());
  const Node* text_2 = item_2->firstChild();
  ASSERT_NE(nullptr, text_2);
  ASSERT_TRUE(text_2->IsTextNode());

  const AXObject* ax_item_1 = GetAXObjectByElementId("item1");
  ASSERT_NE(nullptr, ax_item_1);
  ASSERT_EQ(ax::mojom::Role::kListItem, ax_item_1->RoleValue());
  const AXObject* ax_bullet_1 = ax_item_1->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_bullet_1);
  ASSERT_EQ(ax::mojom::Role::kListMarker, ax_bullet_1->RoleValue());
  const AXObject* ax_item_2 = GetAXObjectByElementId("item2");
  ASSERT_NE(nullptr, ax_item_2);
  ASSERT_EQ(ax::mojom::Role::kListItem, ax_item_2->RoleValue());
  const AXObject* ax_text_2 = ax_item_2->LastChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_text_2);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_text_2->RoleValue());

  AXSelection::Builder builder;
  AXSelection ax_selection =
      builder.SetAnchor(AXPosition::CreateFirstPositionInObject(*ax_bullet_1))
          .SetFocus(AXPosition::CreateLastPositionInObject(*ax_text_2))
          .Build();

  // The list bullet is not included in the DOM tree. Shrinking the
  // |AXSelection| should skip over it by creating an anchor before the first
  // child of the first <li>, i.e. the text node containing the text "Item 1.".
  // This should be further optimized to a text position at the start of the
  // text object inside the first <li>.
  ax_selection.Select(AXSelectionBehavior::kShrinkToValidRange);
  const SelectionInDOMTree shrunk_selection =
      Selection().GetSelectionInDOMTree();

  EXPECT_EQ(text_1, shrunk_selection.Anchor().AnchorNode());
  ASSERT_TRUE(shrunk_selection.Anchor().IsOffsetInAnchor());
  EXPECT_EQ(0, shrunk_selection.Anchor().OffsetInContainerNode());
  ASSERT_TRUE(shrunk_selection.Focus().IsOffsetInAnchor());
  EXPECT_EQ(text_2, shrunk_selection.Focus().AnchorNode());
  EXPECT_EQ(7, shrunk_selection.Focus().OffsetInContainerNode());

  // The list bullet is not included in the DOM tree. Extending the
  // |AXSelection| should move the anchor to before the first <li>.
  ax_selection.Select(AXSelectionBehavior::kExtendToValidRange);
  const SelectionInDOMTree extended_selection =
      Selection().GetSelectionInDOMTree();

  ASSERT_TRUE(extended_selection.Anchor().IsOffsetInAnchor());
  EXPECT_EQ(item_1->parentNode(), extended_selection.Anchor().AnchorNode());
  EXPECT_EQ(static_cast<int>(item_1->NodeIndex()),
            extended_selection.Anchor().OffsetInContainerNode());
  ASSERT_TRUE(extended_selection.Focus().IsOffsetInAnchor());
  EXPECT_EQ(text_2, extended_selection.Focus().AnchorNode());
  EXPECT_EQ(7, extended_selection.Focus().OffsetInContainerNode());

  std::string expectations;
  expectations =
      "++<GenericContainer>\n"
      "++++<GenericContainer>\n"
      "++++++<Main>\n"
      "++++++++<List>\n"
      "++++++++++<ListItem>\n"
      "++++++++++++<ListMarker: \xE2\x80\xA2 >\n"
      "^++++++++++++++<StaticText: ^\xE2\x80\xA2 >\n"
      "++++++++++++<StaticText: Item 1.>\n"
      "++++++++++<ListItem>\n"
      "++++++++++++<ListMarker: \xE2\x80\xA2 >\n"
      "++++++++++++++<StaticText: \xE2\x80\xA2 >\n"
      "++++++++++++<StaticText: Item 2.|>\n";

  // The |AXSelection| should remain unaffected by any shrinking and should
  // include both list bullets.
  EXPECT_EQ(expectations, GetSelectionText(ax_selection));
}

//
// Tests that involve selection inside, outside, and spanning text controls.
//

TEST_F(AccessibilitySelectionTest, FromCurrentSelectionInTextField) {
  GetPage().GetSettings().SetScriptEnabled(true);
  SetBodyInnerHTML(R"HTML(
      <input id="input" value="Inside text field.">
      )HTML");

  ASSERT_FALSE(AXSelection::FromCurrentSelection(GetDocument()).IsValid());

  Element* const script_element =
      GetDocument().CreateRawElement(html_names::kScriptTag);
  ASSERT_NE(nullptr, script_element);
  script_element->setTextContent(R"SCRIPT(
      let input = document.querySelector('input');
      input.focus();
      input.selectionStart = 0;
      input.selectionEnd = input.value.length;
      )SCRIPT");
  GetDocument().body()->AppendChild(script_element);
  UpdateAllLifecyclePhasesForTest();

  const Element* input = GetDocument().QuerySelector(AtomicString("input"));
  ASSERT_NE(nullptr, input);
  ASSERT_TRUE(IsTextControl(input));

  const AXObject* ax_input = GetAXObjectByElementId("input");
  ASSERT_NE(nullptr, ax_input);
  ASSERT_EQ(ax::mojom::Role::kTextField, ax_input->RoleValue());

  const auto ax_selection =
      AXSelection::FromCurrentSelection(ToTextControl(*input));
  ASSERT_TRUE(ax_selection.IsValid());

  ASSERT_TRUE(ax_selection.Anchor().IsTextPosition());
  EXPECT_EQ(ax_input, ax_selection.Anchor().ContainerObject());
  EXPECT_EQ(0, ax_selection.Anchor().TextOffset());
  EXPECT_EQ(TextAffinity::kDownstream, ax_selection.Anchor().Affinity());
  ASSERT_TRUE(ax_selection.Focus().IsTextPosition());
  EXPECT_EQ(ax_input, ax_selection.Focus().ContainerObject());
  EXPECT_EQ(18, ax_selection.Focus().TextOffset());
  EXPECT_EQ(TextAffinity::kDownstream, ax_selection.Focus().Affinity());
}

TEST_F(AccessibilitySelectionTest, FromCurrentSelectionInTextarea) {
  GetPage().GetSettings().SetScriptEnabled(true);
  SetBodyInnerHTML(R"HTML(
      <textarea id="textarea">
        Inside
        textarea
        field.
      </textarea>
      )HTML");

  ASSERT_FALSE(AXSelection::FromCurrentSelection(GetDocument()).IsValid());

  Element* const script_element =
      GetDocument().CreateRawElement(html_names::kScriptTag);
  ASSERT_NE(nullptr, script_element);
  script_element->setTextContent(R"SCRIPT(
      let textarea = document.querySelector('textarea');
      textarea.focus();
      textarea.selectionStart = 0;
      textarea.selectionEnd = textarea.textLength;
      )SCRIPT");
  GetDocument().body()->AppendChild(script_element);
  UpdateAllLifecyclePhasesForTest();

  const Element* textarea =
      GetDocument().QuerySelector(AtomicString("textarea"));
  ASSERT_NE(nullptr, textarea);
  ASSERT_TRUE(IsTextControl(textarea));

  const AXObject* ax_textarea = GetAXObjectByElementId("textarea");
  ASSERT_NE(nullptr, ax_textarea);
  ASSERT_EQ(ax::mojom::Role::kTextField, ax_textarea->RoleValue());

  const auto ax_selection =
      AXSelection::FromCurrentSelection(ToTextControl(*textarea));
  ASSERT_TRUE(ax_selection.IsValid());

  ASSERT_TRUE(ax_selection.Anchor().IsTextPosition());
  EXPECT_EQ(ax_textarea, ax_selection.Anchor().ContainerObject());
  EXPECT_EQ(0, ax_selection.Anchor().TextOffset());
  EXPECT_EQ(TextAffinity::kDownstream, ax_selection.Anchor().Affinity());
  ASSERT_TRUE(ax_selection.Focus().IsTextPosition());
  EXPECT_EQ(ax_textarea, ax_selection.Focus().ContainerObject());
  EXPECT_EQ(53, ax_selection.Focus().TextOffset());
  EXPECT_EQ(TextAffinity::kDownstream, ax_selection.Focus().Affinity());
}

TEST_F(AccessibilitySelectionTest, FromCurrentSelectionInTextareaWithAffinity) {
  // Even though the base of the selection in this test is at a position with an
  // upstream affinity, only a downstream affinity should be exposed, because an
  // upstream affinity is currently exposed in core editing only when the
  // selection is caret.
  SetBodyInnerHTML(R"HTML(
      <textarea id="textarea"
          rows="2" cols="15"
          style="font-family: monospace; width: 15ch;">
        InsideTextareaField.
      </textarea>
      )HTML");

  ASSERT_FALSE(AXSelection::FromCurrentSelection(GetDocument()).IsValid());

  Element* const textarea =
      GetDocument().QuerySelector(AtomicString("textarea"));
  ASSERT_NE(nullptr, textarea);
  ASSERT_TRUE(IsTextControl(textarea));
  const TextControlElement& text_control = ToTextControl(*textarea);

  // This test should only be testing accessibility code. Ordinarily we should
  // be setting up the test using Javascript in order to avoid depending on the
  // internal implementation of DOM selection. However, the only way I found to
  // get an upstream affinity is to send the "end" key which might be unreliable
  // on certain platforms, so we modify the selection using Blink internal
  // functions instead.
  textarea->Focus();
  Selection().Modify(SelectionModifyAlteration::kMove,
                     SelectionModifyDirection::kBackward,
                     TextGranularity::kDocumentBoundary, SetSelectionBy::kUser);
  Selection().Modify(SelectionModifyAlteration::kExtend,
                     SelectionModifyDirection::kForward,
                     TextGranularity::kLineBoundary, SetSelectionBy::kUser);
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(TextAffinity::kDownstream, text_control.Selection().Affinity());

  const AXObject* ax_textarea = GetAXObjectByElementId("textarea");
  ASSERT_NE(nullptr, ax_textarea);
  ASSERT_EQ(ax::mojom::Role::kTextField, ax_textarea->RoleValue());

  const auto ax_selection = AXSelection::FromCurrentSelection(text_control);
  ASSERT_TRUE(ax_selection.IsValid());

  EXPECT_TRUE(ax_selection.Anchor().IsTextPosition());
  EXPECT_EQ(ax_textarea, ax_selection.Anchor().ContainerObject());
  EXPECT_EQ(0, ax_selection.Anchor().TextOffset());
  EXPECT_EQ(TextAffinity::kDownstream, ax_selection.Anchor().Affinity());
  EXPECT_TRUE(ax_selection.Focus().IsTextPosition());
  EXPECT_EQ(ax_textarea, ax_selection.Focus().ContainerObject());
  EXPECT_EQ(8, ax_selection.Focus().TextOffset());
  EXPECT_EQ(TextAffinity::kDownstream, ax_selection.Focus().Affinity());
}

TEST_F(AccessibilitySelectionTest,
       FromCurrentSelectionInTextareaWithCollapsedSelectionAndAffinity) {
  SetBodyInnerHTML(R"HTML(
      <textarea id="textarea"
          rows="2" cols="15"
          style="font-family: monospace; width: 15ch;">
        InsideTextareaField.
      </textarea>
      )HTML");

  ASSERT_FALSE(AXSelection::FromCurrentSelection(GetDocument()).IsValid());

  Element* const textarea =
      GetDocument().QuerySelector(AtomicString("textarea"));
  ASSERT_NE(nullptr, textarea);
  ASSERT_TRUE(IsTextControl(textarea));
  const TextControlElement& text_control = ToTextControl(*textarea);

  // This test should only be testing accessibility code. Ordinarily we should
  // be setting up the test using Javascript in order to avoid depending on the
  // internal implementation of DOM selection. However, the only way I found to
  // get an upstream affinity is to send the "end" key which might be unreliable
  // on certain platforms, so we modify the selection using Blink internal
  // functions instead.
  textarea->Focus();
  Selection().Modify(SelectionModifyAlteration::kMove,
                     SelectionModifyDirection::kBackward,
                     TextGranularity::kDocumentBoundary, SetSelectionBy::kUser);
  Selection().Modify(SelectionModifyAlteration::kMove,
                     SelectionModifyDirection::kForward,
                     TextGranularity::kLineBoundary, SetSelectionBy::kUser);
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(TextAffinity::kUpstream, text_control.Selection().Affinity());

  const AXObject* ax_textarea = GetAXObjectByElementId("textarea");
  ASSERT_NE(nullptr, ax_textarea);
  ASSERT_EQ(ax::mojom::Role::kTextField, ax_textarea->RoleValue());

  const auto ax_selection = AXSelection::FromCurrentSelection(text_control);
  ASSERT_TRUE(ax_selection.IsValid());

  EXPECT_TRUE(ax_selection.Anchor().IsTextPosition());
  EXPECT_EQ(ax_textarea, ax_selection.Anchor().ContainerObject());
  EXPECT_EQ(8, ax_selection.Anchor().TextOffset());
  EXPECT_EQ(TextAffinity::kUpstream, ax_selection.Anchor().Affinity());
  EXPECT_TRUE(ax_selection.Focus().IsTextPosition());
  EXPECT_EQ(ax_textarea, ax_selection.Focus().ContainerObject());
  EXPECT_EQ(8, ax_selection.Focus().TextOffset());
  EXPECT_EQ(TextAffinity::kUpstream, ax_selection.Focus().Affinity());
}

TEST_F(AccessibilitySelectionTest,
       FromCurrentSelectionInContentEditableWithSoftLineBreaks) {
  GetPage().GetSettings().SetScriptEnabled(true);
  SetBodyInnerHTML(R"HTML(
      <div id="contenteditable" role="textbox" contenteditable
          style="max-width: 5px; overflow-wrap: normal;">
        Inside contenteditable field.
      </div>
      )HTML");

  ASSERT_FALSE(AXSelection::FromCurrentSelection(GetDocument()).IsValid());

  // We want to select all the text in the content editable, but not the
  // editable itself.
  Element* const script_element =
      GetDocument().CreateRawElement(html_names::kScriptTag);
  ASSERT_NE(nullptr, script_element);
  script_element->setTextContent(R"SCRIPT(
      const contenteditable = document.querySelector('div[contenteditable]');
      contenteditable.focus();
      const firstLine = contenteditable.firstChild;
      const lastLine = contenteditable.lastChild;
      const range = document.createRange();
      range.setStart(firstLine, 0);
      range.setEnd(lastLine, lastLine.nodeValue.length);
      const selection = getSelection();
      selection.removeAllRanges();
      selection.addRange(range);
      )SCRIPT");
  GetDocument().body()->AppendChild(script_element);
  UpdateAllLifecyclePhasesForTest();

  const AXObject* ax_contenteditable =
      GetAXObjectByElementId("contenteditable");
  ASSERT_NE(nullptr, ax_contenteditable);
  ASSERT_EQ(ax::mojom::Role::kTextField, ax_contenteditable->RoleValue());
  const AXObject* ax_static_text =
      ax_contenteditable->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  // Guard against the structure of the accessibility tree unexpectedly
  // changing, causing a hard to debug test failure.
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue())
      << "A content editable with only text inside it should have static text "
         "children.";
  // Guard against both ComputedName().length() and selection extent offset
  // returning 0.
  ASSERT_LT(0u, ax_static_text->ComputedName().length());

  const auto ax_selection = AXSelection::FromCurrentSelection(GetDocument());
  ASSERT_TRUE(ax_selection.IsValid());

  ASSERT_TRUE(ax_selection.Anchor().IsTextPosition());
  EXPECT_EQ(ax_static_text, ax_selection.Anchor().ContainerObject());
  EXPECT_EQ(0, ax_selection.Anchor().TextOffset());
  ASSERT_TRUE(ax_selection.Focus().IsTextPosition());
  EXPECT_EQ(ax_static_text, ax_selection.Focus().ContainerObject());
  EXPECT_EQ(ax_static_text->ComputedName().length(),
            static_cast<unsigned>(ax_selection.Focus().TextOffset()));
}

TEST_F(AccessibilitySelectionTest,
       FromCurrentSelectionInContentEditableSelectFirstSoftLineBreak) {
  GetPage().GetSettings().SetScriptEnabled(true);
  // There should be no white space between the opening tag of the content
  // editable and the text inside it, otherwise selection offsets would be
  // wrong.
  SetBodyInnerHTML(R"HTML(
      <div id="contenteditable" role="textbox" contenteditable
          style="max-width: 5px; overflow-wrap: normal;">Line one.
      </div>
      )HTML");

  ASSERT_FALSE(AXSelection::FromCurrentSelection(GetDocument()).IsValid());

  Element* const script_element =
      GetDocument().CreateRawElement(html_names::kScriptTag);
  ASSERT_NE(nullptr, script_element);
  script_element->setTextContent(R"SCRIPT(
      const contenteditable = document.querySelector('div[contenteditable]');
      contenteditable.focus();
      const text = contenteditable.firstChild;
      const range = document.createRange();
      range.setStart(text, 4);
      range.setEnd(text, 4);
      const selection = getSelection();
      selection.removeAllRanges();
      selection.addRange(range);
      selection.modify('extend', 'forward', 'character');
      )SCRIPT");
  GetDocument().body()->AppendChild(script_element);
  UpdateAllLifecyclePhasesForTest();

  const AXObject* ax_contenteditable =
      GetAXObjectByElementId("contenteditable");
  ASSERT_NE(nullptr, ax_contenteditable);
  ASSERT_EQ(ax::mojom::Role::kTextField, ax_contenteditable->RoleValue());
  const AXObject* ax_static_text =
      ax_contenteditable->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  // Guard against the structure of the accessibility tree unexpectedly
  // changing, causing a hard to debug test failure.
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue())
      << "A content editable with only text inside it should have static text "
         "children.";

  const auto ax_selection = AXSelection::FromCurrentSelection(GetDocument());
  ASSERT_TRUE(ax_selection.IsValid());

  ASSERT_TRUE(ax_selection.Anchor().IsTextPosition());
  EXPECT_EQ(ax_static_text, ax_selection.Anchor().ContainerObject());
  EXPECT_EQ(4, ax_selection.Anchor().TextOffset());
  ASSERT_TRUE(ax_selection.Focus().IsTextPosition());
  EXPECT_EQ(ax_static_text, ax_selection.Focus().ContainerObject());
  EXPECT_EQ(5, ax_selection.Focus().TextOffset());
}

TEST_F(AccessibilitySelectionTest,
       FromCurrentSelectionInContentEditableSelectFirstHardLineBreak) {
  GetPage().GetSettings().SetScriptEnabled(true);
  // There should be no white space between the opening tag of the content
  // editable and the text inside it, otherwise selection offsets would be
  // wrong.
  SetBodyInnerHTML(R"HTML(
      <div id="contenteditable" role="textbox" contenteditable
          style="max-width: 5px; overflow-wrap: normal;">Inside<br>contenteditable.
      </div>
      )HTML");

  ASSERT_FALSE(AXSelection::FromCurrentSelection(GetDocument()).IsValid());

  Element* const script_element =
      GetDocument().CreateRawElement(html_names::kScriptTag);
  ASSERT_NE(nullptr, script_element);
  script_element->setTextContent(R"SCRIPT(
      const contenteditable = document.querySelector('div[contenteditable]');
      contenteditable.focus();
      const firstLine = contenteditable.firstChild;
      const range = document.createRange();
      range.setStart(firstLine, 6);
      range.setEnd(firstLine, 6);
      const selection = getSelection();
      selection.removeAllRanges();
      selection.addRange(range);
      selection.modify('extend', 'forward', 'character');
      )SCRIPT");
  GetDocument().body()->AppendChild(script_element);
  UpdateAllLifecyclePhasesForTest();

  const AXObject* ax_contenteditable =
      GetAXObjectByElementId("contenteditable");
  ASSERT_NE(nullptr, ax_contenteditable);
  ASSERT_EQ(ax::mojom::Role::kTextField, ax_contenteditable->RoleValue());
  ASSERT_EQ(3, ax_contenteditable->UnignoredChildCount())
      << "The content editable should have two lines with a line break between "
         "them.";
  const AXObject* ax_static_text_2 = ax_contenteditable->UnignoredChildAt(2);
  ASSERT_NE(nullptr, ax_static_text_2);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text_2->RoleValue());

  const auto ax_selection = AXSelection::FromCurrentSelection(GetDocument());
  ASSERT_TRUE(ax_selection.IsValid());

  ASSERT_FALSE(ax_selection.Anchor().IsTextPosition());
  EXPECT_EQ(ax_contenteditable, ax_selection.Anchor().ContainerObject());
  EXPECT_EQ(1, ax_selection.Anchor().ChildIndex());
  ASSERT_TRUE(ax_selection.Focus().IsTextPosition());
  EXPECT_EQ(ax_static_text_2, ax_selection.Focus().ContainerObject());
  EXPECT_EQ(0, ax_selection.Focus().TextOffset());
}

TEST_F(AccessibilitySelectionTest, ClearCurrentSelectionInTextField) {
  GetPage().GetSettings().SetScriptEnabled(true);
  SetBodyInnerHTML(R"HTML(
      <input id="input" value="Inside text field.">
      )HTML");

  ASSERT_FALSE(AXSelection::FromCurrentSelection(GetDocument()).IsValid());

  Element* const script_element =
      GetDocument().CreateRawElement(html_names::kScriptTag);
  ASSERT_NE(nullptr, script_element);
  script_element->setTextContent(R"SCRIPT(
      let input = document.querySelector('input');
      input.focus();
      input.selectionStart = 0;
      input.selectionEnd = input.textLength;
      )SCRIPT");
  GetDocument().body()->AppendChild(script_element);
  UpdateAllLifecyclePhasesForTest();

  SelectionInDOMTree selection = Selection().GetSelectionInDOMTree();
  ASSERT_FALSE(selection.IsNone());

  AXSelection::ClearCurrentSelection(GetDocument());
  selection = Selection().GetSelectionInDOMTree();
  EXPECT_TRUE(selection.IsNone());

  const auto ax_selection = AXSelection::FromCurrentSelection(GetDocument());
  EXPECT_FALSE(ax_selection.IsValid());
  EXPECT_EQ("", GetSelectionText(ax_selection));
}

TEST_F(AccessibilitySelectionTest, ClearCurrentSelectionInTextarea) {
  GetPage().GetSettings().SetScriptEnabled(true);
  SetBodyInnerHTML(R"HTML(
      <textarea id="textarea">
        Inside
        textarea
        field.
      </textarea>
      )HTML");

  ASSERT_FALSE(AXSelection::FromCurrentSelection(GetDocument()).IsValid());

  Element* const script_element =
      GetDocument().CreateRawElement(html_names::kScriptTag);
  ASSERT_NE(nullptr, script_element);
  script_element->setTextContent(R"SCRIPT(
      let textarea = document.querySelector('textarea');
      textarea.focus();
      textarea.selectionStart = 0;
      textarea.selectionEnd = textarea.textLength;
      )SCRIPT");
  GetDocument().body()->AppendChild(script_element);
  UpdateAllLifecyclePhasesForTest();

  SelectionInDOMTree selection = Selection().GetSelectionInDOMTree();
  ASSERT_FALSE(selection.IsNone());

  AXSelection::ClearCurrentSelection(GetDocument());
  selection = Selection().GetSelectionInDOMTree();
  EXPECT_TRUE(selection.IsNone());

  const auto ax_selection = AXSelection::FromCurrentSelection(GetDocument());
  EXPECT_FALSE(ax_selection.IsValid());
  EXPECT_EQ("", GetSelectionText(ax_selection));
}

TEST_F(AccessibilitySelectionTest, ForwardSelectionInTextField) {
  SetBodyInnerHTML(R"HTML(
      <input id="input" value="Inside text field.">
      )HTML");

  Element* const input = GetDocument().QuerySelector(AtomicString("input"));
  ASSERT_NE(nullptr, input);
  ASSERT_TRUE(IsTextControl(input));
  input->Focus(FocusOptions::Create());
  ASSERT_TRUE(input->IsFocusedElementInDocument());

  const AXObject* ax_input = GetAXObjectByElementId("input");
  ASSERT_NE(nullptr, ax_input);
  ASSERT_EQ(ax::mojom::Role::kTextField, ax_input->RoleValue());

  // Forward selection.
  AXSelection::Builder builder;
  AXSelection ax_selection =
      builder.SetAnchor(AXPosition::CreateFirstPositionInObject(*ax_input))
          .SetFocus(AXPosition::CreateLastPositionInObject(*ax_input))
          .Build();

  EXPECT_TRUE(ax_selection.Select());

  EXPECT_EQ(0u, ToTextControl(*input).selectionStart());
  EXPECT_EQ(18u, ToTextControl(*input).selectionEnd());
  EXPECT_EQ("forward", ToTextControl(*input).selectionDirection());

  // Ensure that the selection that was just set could be successfully
  // retrieved.
  GetDocument().ExistingAXObjectCache()->UpdateAXForAllDocuments();
  const auto ax_current_selection =
      AXSelection::FromCurrentSelection(ToTextControl(*input));
  EXPECT_EQ(ax_selection, ax_current_selection);
}

TEST_F(AccessibilitySelectionTest, BackwardSelectionInTextField) {
  SetBodyInnerHTML(R"HTML(
      <input id="input" value="Inside text field.">
      )HTML");

  Element* const input = GetDocument().QuerySelector(AtomicString("input"));
  ASSERT_NE(nullptr, input);
  ASSERT_TRUE(IsTextControl(input));
  input->Focus(FocusOptions::Create());
  ASSERT_TRUE(input->IsFocusedElementInDocument());

  const AXObject* ax_input = GetAXObjectByElementId("input");
  ASSERT_NE(nullptr, ax_input);
  ASSERT_EQ(ax::mojom::Role::kTextField, ax_input->RoleValue());

  // Backward selection.
  AXSelection::Builder builder;
  AXSelection ax_selection =
      builder.SetAnchor(AXPosition::CreatePositionInTextObject(*ax_input, 10))
          .SetFocus(AXPosition::CreatePositionInTextObject(*ax_input, 3))
          .Build();

  EXPECT_TRUE(ax_selection.Select());

  EXPECT_EQ(3u, ToTextControl(*input).selectionStart());
  EXPECT_EQ(10u, ToTextControl(*input).selectionEnd());
  EXPECT_EQ("backward", ToTextControl(*input).selectionDirection());

  // Ensure that the selection that was just set could be successfully
  // retrieved.
  GetDocument().ExistingAXObjectCache()->UpdateAXForAllDocuments();
  const auto ax_current_selection =
      AXSelection::FromCurrentSelection(ToTextControl(*input));
  EXPECT_EQ(ax_selection, ax_current_selection);
}

TEST_F(AccessibilitySelectionTest, SelectingTheWholeOfTheTextField) {
  SetBodyInnerHTML(R"HTML(
      <p id="before">Before text field.</p>
      <input id="input" value="Inside text field.">
      <p id="after">After text field.</p>
      )HTML");

  Element* const input = GetDocument().QuerySelector(AtomicString("input"));
  ASSERT_NE(nullptr, input);
  ASSERT_TRUE(IsTextControl(input));
  ASSERT_TRUE(ToTextControl(*input).SetSelectionRange(
      3u, 10u, kSelectionHasBackwardDirection));

  const AXObject* ax_before = GetAXObjectByElementId("before");
  ASSERT_NE(nullptr, ax_before);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_before->RoleValue());
  const AXObject* ax_input = GetAXObjectByElementId("input");
  ASSERT_NE(nullptr, ax_input);
  ASSERT_EQ(ax::mojom::Role::kTextField, ax_input->RoleValue());

  // Light tree only selection. Selects the whole of the text field.
  AXSelection::Builder builder;
  AXSelection ax_selection =
      builder.SetAnchor(AXPosition::CreatePositionBeforeObject(*ax_before))
          .SetFocus(AXPosition::CreatePositionAfterObject(*ax_input))
          .Build();

  EXPECT_TRUE(ax_selection.Select());

  const SelectionInDOMTree dom_selection = Selection().GetSelectionInDOMTree();
  EXPECT_EQ(GetDocument().body(), dom_selection.Anchor().AnchorNode());
  EXPECT_EQ(1, dom_selection.Anchor().OffsetInContainerNode());
  EXPECT_EQ(GetElementById("before"),
            dom_selection.Anchor().ComputeNodeAfterPosition());
  EXPECT_EQ(GetDocument().body(), dom_selection.Focus().AnchorNode());
  EXPECT_EQ(5, dom_selection.Focus().OffsetInContainerNode());
  EXPECT_EQ(GetElementById("after"),
            dom_selection.Focus().ComputeNodeAfterPosition());

  // The selection in the text field should remain unchanged because the field
  // is not focused.
  EXPECT_EQ(3u, ToTextControl(*input).selectionStart());
  EXPECT_EQ(10u, ToTextControl(*input).selectionEnd());
  EXPECT_EQ("backward", ToTextControl(*input).selectionDirection());
}

TEST_F(AccessibilitySelectionTest, SelectEachConsecutiveCharacterInTextField) {
  SetBodyInnerHTML(R"HTML(
      <input id="input" value="Inside text field.">
      )HTML");

  Element* const input = GetDocument().QuerySelector(AtomicString("input"));
  ASSERT_NE(nullptr, input);
  ASSERT_TRUE(IsTextControl(input));
  TextControlElement& text_control = ToTextControl(*input);
  ASSERT_LE(1u, text_control.InnerEditorValue().length());

  const AXObject* ax_input = GetAXObjectByElementId("input");
  ASSERT_NE(nullptr, ax_input);
  ASSERT_EQ(ax::mojom::Role::kTextField, ax_input->RoleValue());

  for (unsigned int i = 0; i < text_control.InnerEditorValue().length() - 1;
       ++i) {
    GetDocument().ExistingAXObjectCache()->UpdateAXForAllDocuments();
    AXSelection::Builder builder;
    AXSelection ax_selection =
        builder.SetAnchor(AXPosition::CreatePositionInTextObject(*ax_input, i))
            .SetFocus(AXPosition::CreatePositionInTextObject(*ax_input, i + 1))
            .Build();

    testing::Message message;
    message << "While selecting forward character "
            << static_cast<char>(text_control.InnerEditorValue()[i])
            << " at position " << i << " in text field.";
    SCOPED_TRACE(message);
    EXPECT_TRUE(ax_selection.Select());

    EXPECT_EQ(i, text_control.selectionStart());
    EXPECT_EQ(i + 1, text_control.selectionEnd());
    EXPECT_EQ("forward", text_control.selectionDirection());
  }

  for (unsigned int i = text_control.InnerEditorValue().length(); i > 0; --i) {
    GetDocument().ExistingAXObjectCache()->UpdateAXForAllDocuments();
    AXSelection::Builder builder;
    AXSelection ax_selection =
        builder.SetAnchor(AXPosition::CreatePositionInTextObject(*ax_input, i))
            .SetFocus(AXPosition::CreatePositionInTextObject(*ax_input, i - 1))
            .Build();

    testing::Message message;
    message << "While selecting backward character "
            << static_cast<char>(text_control.InnerEditorValue()[i])
            << " at position " << i << " in text field.";
    SCOPED_TRACE(message);
    EXPECT_TRUE(ax_selection.Select());

    EXPECT_EQ(i - 1, text_control.selectionStart());
    EXPECT_EQ(i, text_control.selectionEnd());
    EXPECT_EQ("backward", text_control.selectionDirection());
  }
}

TEST_F(AccessibilitySelectionTest,
       SelectEachConsecutiveCharacterInEmailFieldWithInvalidAddress) {
  GetPage().GetSettings().SetScriptEnabled(true);
  String valid_email = "valid@example.com";
  SetBodyInnerHTML(R"HTML(
      <input id="input" type="email" value=)HTML" +
                   valid_email + R"HTML(>
      )HTML");

  // Add three spaces to the start of the address to make it invalid.
  Element* const script_element =
      GetDocument().CreateRawElement(html_names::kScriptTag);
  ASSERT_NE(nullptr, script_element);
  script_element->setTextContent(R"SCRIPT(
      let input = document.querySelector('input');
      input.focus();
      input.value = input.value.padStart(3, ' ');
      input.selectionStart = 0;
      input.selectionEnd = input.value.length;
      )SCRIPT");
  GetDocument().body()->AppendChild(script_element);
  UpdateAllLifecyclePhasesForTest();

  Element* const input = GetDocument().QuerySelector(AtomicString("input"));
  ASSERT_NE(nullptr, input);
  ASSERT_TRUE(IsTextControl(input));
  TextControlElement& text_control = ToTextControl(*input);
  // The "value" attribute should not contain the extra spaces.
  ASSERT_EQ(valid_email.length(), text_control.Value().length());

  const AXObject* ax_input = GetAXObjectByElementId("input");
  ASSERT_NE(nullptr, ax_input);
  ASSERT_EQ(ax::mojom::Role::kTextField, ax_input->RoleValue());

  // The address can still be navigated using cursor left / right, even though
  // it's invalid.
  for (unsigned int i = 0; i < text_control.InnerEditorValue().length() - 1;
       ++i) {
    GetDocument().ExistingAXObjectCache()->UpdateAXForAllDocuments();
    AXSelection::Builder builder;
    AXSelection ax_selection =
        builder.SetAnchor(AXPosition::CreatePositionInTextObject(*ax_input, i))
            .SetFocus(AXPosition::CreatePositionInTextObject(*ax_input, i + 1))
            .Build();

    testing::Message message;
    message << "While selecting forward character "
            << static_cast<char>(text_control.InnerEditorValue()[i])
            << " at position " << i << " in text field.";
    SCOPED_TRACE(message);
    EXPECT_TRUE(ax_selection.Select());

    EXPECT_EQ(i, text_control.selectionStart());
    EXPECT_EQ(i + 1, text_control.selectionEnd());
    EXPECT_EQ("forward", text_control.selectionDirection());
  }

  for (unsigned int i = text_control.InnerEditorValue().length(); i > 0; --i) {
    GetDocument().ExistingAXObjectCache()->UpdateAXForAllDocuments();
    AXSelection::Builder builder;
    AXSelection ax_selection =
        builder.SetAnchor(AXPosition::CreatePositionInTextObject(*ax_input, i))
            .SetFocus(AXPosition::CreatePositionInTextObject(*ax_input, i - 1))
            .Build();

    testing::Message message;
    message << "While selecting backward character "
            << static_cast<char>(text_control.InnerEditorValue()[i])
            << " at position " << i << " in text field.";
    SCOPED_TRACE(message);
    EXPECT_TRUE(ax_selection.Select());

    EXPECT_EQ(i - 1, text_control.selectionStart());
    EXPECT_EQ(i, text_control.selectionEnd());
    EXPECT_EQ("backward", text_control.selectionDirection());
  }
}

TEST_F(AccessibilitySelectionTest, InvalidSelectionInTextField) {
  SetBodyInnerHTML(R"HTML(
      <p id="before">Before text field.</p>
      <input id="input" value="Inside text field.">
      <p id="after">After text field.</p>
      )HTML");

  Element* const input = GetDocument().QuerySelector(AtomicString("input"));
  ASSERT_NE(nullptr, input);
  ASSERT_TRUE(IsTextControl(input));
  ASSERT_TRUE(ToTextControl(*input).SetSelectionRange(
      3u, 10u, kSelectionHasBackwardDirection));

  const AXObject* ax_before = GetAXObjectByElementId("before");
  ASSERT_NE(nullptr, ax_before);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_before->RoleValue());
  const AXObject* ax_input = GetAXObjectByElementId("input");
  ASSERT_NE(nullptr, ax_input);
  ASSERT_EQ(ax::mojom::Role::kTextField, ax_input->RoleValue());
  const AXObject* ax_after = GetAXObjectByElementId("after");
  ASSERT_NE(nullptr, ax_after);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_after->RoleValue());

  GetDocument().ExistingAXObjectCache()->UpdateAXForAllDocuments();
  {
    // Light tree only selection. Selects the whole of the text field.
    AXSelection::Builder builder;
    AXSelection ax_selection =
        builder.SetAnchor(AXPosition::CreatePositionBeforeObject(*ax_before))
            .SetFocus(AXPosition::CreatePositionAfterObject(*ax_input))
            .Build();
    ax_selection.Select();
  }

  // Invalid selection because it crosses a user agent shadow tree boundary.
  AXSelection::Builder builder;
  AXSelection ax_selection =
      builder.SetAnchor(AXPosition::CreatePositionInTextObject(*ax_input, 0))
          .SetFocus(AXPosition::CreatePositionBeforeObject(*ax_after))
          .Build();

  EXPECT_FALSE(ax_selection.IsValid());

  // The selection in the light DOM should remain unchanged.
  const SelectionInDOMTree dom_selection = Selection().GetSelectionInDOMTree();
  EXPECT_EQ(GetDocument().body(), dom_selection.Anchor().AnchorNode());
  EXPECT_EQ(1, dom_selection.Anchor().OffsetInContainerNode());
  EXPECT_EQ(GetElementById("before"),
            dom_selection.Anchor().ComputeNodeAfterPosition());
  EXPECT_EQ(GetDocument().body(), dom_selection.Focus().AnchorNode());
  EXPECT_EQ(5, dom_selection.Focus().OffsetInContainerNode());
  EXPECT_EQ(GetElementById("after"),
            dom_selection.Focus().ComputeNodeAfterPosition());

  // The selection in the text field should remain unchanged because the field
  // is not focused.
  EXPECT_EQ(3u, ToTextControl(*input).selectionStart());
  EXPECT_EQ(10u, ToTextControl(*input).selectionEnd());
  EXPECT_EQ("backward", ToTextControl(*input).selectionDirection());
}

TEST_F(AccessibilitySelectionTest, ForwardSelectionInTextarea) {
  SetBodyInnerHTML(R"HTML(
      <textarea id="textarea">
        Inside
        textarea
        field.
      </textarea>
      )HTML");

  Element* const textarea =
      GetDocument().QuerySelector(AtomicString("textarea"));
  ASSERT_NE(nullptr, textarea);
  ASSERT_TRUE(IsTextControl(textarea));
  textarea->Focus(FocusOptions::Create());
  ASSERT_TRUE(textarea->IsFocusedElementInDocument());

  const AXObject* ax_textarea = GetAXObjectByElementId("textarea");
  ASSERT_NE(nullptr, ax_textarea);
  ASSERT_EQ(ax::mojom::Role::kTextField, ax_textarea->RoleValue());

  // Forward selection.
  AXSelection::Builder builder;
  AXSelection ax_selection =
      builder.SetAnchor(AXPosition::CreateFirstPositionInObject(*ax_textarea))
          .SetFocus(AXPosition::CreateLastPositionInObject(*ax_textarea))
          .Build();

  EXPECT_TRUE(ax_selection.Select());

  EXPECT_EQ(0u, ToTextControl(*textarea).selectionStart());
  EXPECT_EQ(53u, ToTextControl(*textarea).selectionEnd());
  EXPECT_EQ("forward", ToTextControl(*textarea).selectionDirection());

  // Ensure that the selection that was just set could be successfully
  // retrieved.
  GetDocument().ExistingAXObjectCache()->UpdateAXForAllDocuments();
  const auto ax_current_selection =
      AXSelection::FromCurrentSelection(ToTextControl(*textarea));
  EXPECT_EQ(ax_selection, ax_current_selection);
}

TEST_F(AccessibilitySelectionTest, BackwardSelectionInTextarea) {
  SetBodyInnerHTML(R"HTML(
      <textarea id="textarea">
        Inside
        textarea
        field.
      </textarea>
      )HTML");

  Element* const textarea =
      GetDocument().QuerySelector(AtomicString("textarea"));
  ASSERT_NE(nullptr, textarea);
  ASSERT_TRUE(IsTextControl(textarea));
  textarea->Focus(FocusOptions::Create());
  ASSERT_TRUE(textarea->IsFocusedElementInDocument());

  const AXObject* ax_textarea = GetAXObjectByElementId("textarea");
  ASSERT_NE(nullptr, ax_textarea);
  ASSERT_EQ(ax::mojom::Role::kTextField, ax_textarea->RoleValue());

  // Backward selection.
  AXSelection::Builder builder;
  GetDocument().ExistingAXObjectCache()->UpdateAXForAllDocuments();
  AXSelection ax_selection =
      builder
          .SetAnchor(AXPosition::CreatePositionInTextObject(*ax_textarea, 10))
          .SetFocus(AXPosition::CreatePositionInTextObject(*ax_textarea, 3))
          .Build();

  EXPECT_TRUE(ax_selection.Select());

  EXPECT_EQ(3u, ToTextControl(*textarea).selectionStart());
  EXPECT_EQ(10u, ToTextControl(*textarea).selectionEnd());
  EXPECT_EQ("backward", ToTextControl(*textarea).selectionDirection());

  // Ensure that the selection that was just set could be successfully
  // retrieved.
  GetDocument().ExistingAXObjectCache()->UpdateAXForAllDocuments();
  const auto ax_current_selection =
      AXSelection::FromCurrentSelection(ToTextControl(*textarea));
  EXPECT_EQ(ax_selection, ax_current_selection);
}

TEST_F(AccessibilitySelectionTest, SelectTheWholeOfTheTextarea) {
  SetBodyInnerHTML(R"HTML(
      <p id="before">Before textarea field.</p>
      <textarea id="textarea">
        Inside
        textarea
        field.
      </textarea>
      <p id="after">After textarea field.</p>
      )HTML");

  Element* const textarea =
      GetDocument().QuerySelector(AtomicString("textarea"));
  ASSERT_NE(nullptr, textarea);
  ASSERT_TRUE(IsTextControl(textarea));
  ASSERT_TRUE(ToTextControl(*textarea).SetSelectionRange(
      3u, 10u, kSelectionHasBackwardDirection));

  const AXObject* ax_before = GetAXObjectByElementId("before");
  ASSERT_NE(nullptr, ax_before);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_before->RoleValue());
  const AXObject* ax_textarea = GetAXObjectByElementId("textarea");
  ASSERT_NE(nullptr, ax_textarea);
  ASSERT_EQ(ax::mojom::Role::kTextField, ax_textarea->RoleValue());

  // Light tree only selection. Selects the whole of the textarea field.
  GetDocument().ExistingAXObjectCache()->UpdateAXForAllDocuments();
  AXSelection::Builder builder;
  AXSelection ax_selection =
      builder.SetAnchor(AXPosition::CreatePositionBeforeObject(*ax_before))
          .SetFocus(AXPosition::CreatePositionAfterObject(*ax_textarea))
          .Build();

  EXPECT_TRUE(ax_selection.Select());

  const SelectionInDOMTree dom_selection = Selection().GetSelectionInDOMTree();
  EXPECT_EQ(GetDocument().body(), dom_selection.Anchor().AnchorNode());
  EXPECT_EQ(1, dom_selection.Anchor().OffsetInContainerNode());
  EXPECT_EQ(GetElementById("before"),
            dom_selection.Anchor().ComputeNodeAfterPosition());
  EXPECT_EQ(GetDocument().body(), dom_selection.Focus().AnchorNode());
  EXPECT_EQ(5, dom_selection.Focus().OffsetInContainerNode());
  EXPECT_EQ(GetElementById("after"),
            dom_selection.Focus().ComputeNodeAfterPosition());

  // The selection in the textarea field should remain unchanged because the
  // field is not focused.
  EXPECT_EQ(3u, ToTextControl(*textarea).selectionStart());
  EXPECT_EQ(10u, ToTextControl(*textarea).selectionEnd());
  EXPECT_EQ("backward", ToTextControl(*textarea).selectionDirection());
}

TEST_F(AccessibilitySelectionTest, SelectEachConsecutiveCharacterInTextarea) {
  SetBodyInnerHTML(R"HTML(
      <textarea id="textarea">
        Inside
        textarea
        field.
      </textarea>
      )HTML");

  Element* const textarea =
      GetDocument().QuerySelector(AtomicString("textarea"));
  ASSERT_NE(nullptr, textarea);
  ASSERT_TRUE(IsTextControl(textarea));
  TextControlElement& text_control = ToTextControl(*textarea);
  ASSERT_LE(1u, text_control.Value().length());

  const AXObject* ax_textarea = GetAXObjectByElementId("textarea");
  ASSERT_NE(nullptr, ax_textarea);
  ASSERT_EQ(ax::mojom::Role::kTextField, ax_textarea->RoleValue());

  for (unsigned int i = 0; i < text_control.Value().length() - 1; ++i) {
    GetDocument().ExistingAXObjectCache()->UpdateAXForAllDocuments();
    AXSelection::Builder builder;
    AXSelection ax_selection =
        builder
            .SetAnchor(AXPosition::CreatePositionInTextObject(*ax_textarea, i))
            .SetFocus(
                AXPosition::CreatePositionInTextObject(*ax_textarea, i + 1))
            .Build();

    testing::Message message;
    message << "While selecting forward character "
            << static_cast<char>(text_control.Value()[i]) << " at position "
            << i << " in textarea.";
    SCOPED_TRACE(message);
    EXPECT_TRUE(ax_selection.Select());

    EXPECT_EQ(i, text_control.selectionStart());
    EXPECT_EQ(i + 1, text_control.selectionEnd());
    EXPECT_EQ("forward", text_control.selectionDirection());
  }

  for (unsigned int i = text_control.Value().length(); i > 0; --i) {
    GetDocument().ExistingAXObjectCache()->UpdateAXForAllDocuments();
    AXSelection::Builder builder;
    AXSelection ax_selection =
        builder
            .SetAnchor(AXPosition::CreatePositionInTextObject(*ax_textarea, i))
            .SetFocus(
                AXPosition::CreatePositionInTextObject(*ax_textarea, i - 1))
            .Build();

    testing::Message message;
    message << "While selecting backward character "
            << static_cast<char>(text_control.Value()[i]) << " at position "
            << i << " in textarea.";
    SCOPED_TRACE(message);
    EXPECT_TRUE(ax_selection.Select());

    EXPECT_EQ(i - 1, text_control.selectionStart());
    EXPECT_EQ(i, text_control.selectionEnd());
    EXPECT_EQ("backward", text_control.selectionDirection());
  }
}

TEST_F(AccessibilitySelectionTest, InvalidSelectionInTextarea) {
  SetBodyInnerHTML(R"HTML(
      <p id="before">Before textarea field.</p>
      <textarea id="textarea">
        Inside
        textarea
        field.
      </textarea>
      <p id="after">After textarea field.</p>
      )HTML");

  Element* const textarea =
      GetDocument().QuerySelector(AtomicString("textarea"));
  ASSERT_NE(nullptr, textarea);
  ASSERT_TRUE(IsTextControl(textarea));
  ASSERT_TRUE(ToTextControl(*textarea).SetSelectionRange(
      3u, 10u, kSelectionHasBackwardDirection));

  const AXObject* ax_before = GetAXObjectByElementId("before");
  ASSERT_NE(nullptr, ax_before);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_before->RoleValue());
  const AXObject* ax_textarea = GetAXObjectByElementId("textarea");
  ASSERT_NE(nullptr, ax_textarea);
  ASSERT_EQ(ax::mojom::Role::kTextField, ax_textarea->RoleValue());
  const AXObject* ax_after = GetAXObjectByElementId("after");
  ASSERT_NE(nullptr, ax_after);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_after->RoleValue());

  {
    // Light tree only selection. Selects the whole of the textarea field.
    AXSelection::Builder builder;
    AXSelection ax_selection =
        builder.SetAnchor(AXPosition::CreatePositionBeforeObject(*ax_before))
            .SetFocus(AXPosition::CreatePositionAfterObject(*ax_textarea))
            .Build();
    ax_selection.Select();
  }

  // Invalid selection because it crosses a user agent shadow tree boundary.
  AXSelection::Builder builder;
  AXSelection ax_selection =
      builder.SetAnchor(AXPosition::CreatePositionInTextObject(*ax_textarea, 0))
          .SetFocus(AXPosition::CreatePositionBeforeObject(*ax_after))
          .Build();

  EXPECT_FALSE(ax_selection.IsValid());

  // The selection in the light DOM should remain unchanged.
  const SelectionInDOMTree dom_selection = Selection().GetSelectionInDOMTree();
  EXPECT_EQ(GetDocument().body(), dom_selection.Anchor().AnchorNode());
  EXPECT_EQ(1, dom_selection.Anchor().OffsetInContainerNode());
  EXPECT_EQ(GetElementById("before"),
            dom_selection.Anchor().ComputeNodeAfterPosition());
  EXPECT_EQ(GetDocument().body(), dom_selection.Focus().AnchorNode());
  EXPECT_EQ(5, dom_selection.Focus().OffsetInContainerNode());
  EXPECT_EQ(GetElementById("after"),
            dom_selection.Focus().ComputeNodeAfterPosition());

  // The selection in the textarea field should remain unchanged because the
  // field is not focused.
  EXPECT_EQ(3u, ToTextControl(*textarea).selectionStart());
  EXPECT_EQ(10u, ToTextControl(*textarea).selectionEnd());
  EXPECT_EQ("backward", ToTextControl(*textarea).selectionDirection());
}

TEST_F(AccessibilitySelectionTest,
       FromCurrentSelectionInContenteditableWithAffinity) {
  SetBodyInnerHTML(R"HTML(
      <div role="textbox" contenteditable id="contenteditable"
          style="font-family: monospace; width: 15ch;">
        InsideContenteditableTextboxField.
      </div>
      )HTML");

  ASSERT_FALSE(AXSelection::FromCurrentSelection(GetDocument()).IsValid());

  Element* const contenteditable =
      GetDocument().QuerySelector(AtomicString("div[role=textbox]"));
  ASSERT_NE(nullptr, contenteditable);

  // This test should only be testing accessibility code. Ordinarily we should
  // be setting up the test using Javascript in order to avoid depending on the
  // internal implementation of DOM selection. However, the only way I found to
  // get an upstream affinity is to send the "end" key which might be unreliable
  // on certain platforms, so we modify the selection using Blink internal
  // functions instead.
  contenteditable->Focus();
  Selection().Modify(SelectionModifyAlteration::kMove,
                     SelectionModifyDirection::kBackward,
                     TextGranularity::kDocumentBoundary, SetSelectionBy::kUser);
  Selection().Modify(SelectionModifyAlteration::kMove,
                     SelectionModifyDirection::kForward,
                     TextGranularity::kLineBoundary, SetSelectionBy::kUser);
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(TextAffinity::kUpstream,
            Selection().GetSelectionInDOMTree().Affinity());

  const AXObject* ax_contenteditable =
      GetAXObjectByElementId("contenteditable");
  ASSERT_NE(nullptr, ax_contenteditable);
  ASSERT_EQ(ax::mojom::Role::kTextField, ax_contenteditable->RoleValue());
  const AXObject* ax_text = ax_contenteditable->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_text->RoleValue());

  GetDocument().ExistingAXObjectCache()->UpdateAXForAllDocuments();
  const auto ax_selection = AXSelection::FromCurrentSelection(GetDocument());
  ASSERT_TRUE(ax_selection.IsValid());

  EXPECT_TRUE(ax_selection.Anchor().IsTextPosition());
  EXPECT_EQ(ax_text, ax_selection.Anchor().ContainerObject());
  EXPECT_LE(15, ax_selection.Anchor().TextOffset());
  EXPECT_GT(static_cast<int>(ax_text->ComputedName().length()),
            ax_selection.Anchor().TextOffset());
  EXPECT_EQ(TextAffinity::kUpstream, ax_selection.Anchor().Affinity());
  EXPECT_TRUE(ax_selection.Focus().IsTextPosition());
  EXPECT_EQ(ax_text, ax_selection.Focus().ContainerObject());
  EXPECT_LE(15, ax_selection.Focus().TextOffset());
  EXPECT_GT(static_cast<int>(ax_text->ComputedName().length()),
            ax_selection.Focus().TextOffset());
  EXPECT_EQ(TextAffinity::kUpstream, ax_selection.Focus().Affinity());
}

TEST_F(AccessibilitySelectionTest,
       SelectEachConsecutiveCharacterInContenteditable) {
  // The text should wrap after each word.
  SetBodyInnerHTML(R"HTML(
      <div id="contenteditable" contenteditable role="textbox"
          style="max-width: 5px; overflow-wrap: normal;">
        This is a test.
      </div>
      )HTML");

  const Element* contenteditable =
      GetDocument().QuerySelector(AtomicString("div[contenteditable]"));
  ASSERT_NE(nullptr, contenteditable);
  const Node* text = contenteditable->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());

  const AXObject* ax_contenteditable =
      GetAXObjectByElementId("contenteditable");
  ASSERT_NE(nullptr, ax_contenteditable);
  ASSERT_EQ(1, ax_contenteditable->ChildCountIncludingIgnored());
  const AXObject* ax_static_text =
      ax_contenteditable->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue());
  String computed_name = ax_static_text->ComputedName();
  ASSERT_LE(1u, computed_name.length());

  for (unsigned int i = 0; i < computed_name.length() - 1; ++i) {
    AXSelection::Builder builder;
    AXSelection ax_selection =
        builder
            .SetAnchor(
                AXPosition::CreatePositionInTextObject(*ax_static_text, i))
            .SetFocus(
                AXPosition::CreatePositionInTextObject(*ax_static_text, i + 1))
            .Build();

    testing::Message message;
    message << "While selecting forward character "
            << std::u16string(1, computed_name[i]) << " at position " << i
            << " in contenteditable.";
    SCOPED_TRACE(message);
    EXPECT_TRUE(ax_selection.Select());

    const SelectionInDOMTree dom_selection =
        Selection().GetSelectionInDOMTree();
    EXPECT_EQ(text, dom_selection.Anchor().AnchorNode());
    EXPECT_EQ(text, dom_selection.Focus().AnchorNode());
    // The discrepancy between DOM and AX text offsets is due to the fact that
    // there is some white space in the DOM that is compressed in the
    // accessibility tree.
    EXPECT_EQ(static_cast<int>(i + 9),
              dom_selection.Anchor().OffsetInContainerNode());
    EXPECT_EQ(static_cast<int>(i + 10),
              dom_selection.Focus().OffsetInContainerNode());
  }

  for (unsigned int i = computed_name.length(); i > 0; --i) {
    AXSelection::Builder builder;
    AXSelection ax_selection =
        builder
            .SetAnchor(
                AXPosition::CreatePositionInTextObject(*ax_static_text, i))
            .SetFocus(
                AXPosition::CreatePositionInTextObject(*ax_static_text, i - 1))
            .Build();

    testing::Message message;
    message << "While selecting backward character "
            << std::u16string(1, computed_name[i]) << " at position " << i
            << " in contenteditable.";
    SCOPED_TRACE(message);
    EXPECT_TRUE(ax_selection.Select());

    const SelectionInDOMTree dom_selection =
        Selection().GetSelectionInDOMTree();
    EXPECT_EQ(text, dom_selection.Anchor().AnchorNode());
    EXPECT_EQ(text, dom_selection.Focus().AnchorNode());
    // The discrepancy between DOM and AX text offsets is due to the fact that
    // there is some white space in the DOM that is compressed in the
    // accessibility tree.
    EXPECT_EQ(static_cast<int>(i + 9),
              dom_selection.Anchor().OffsetInContainerNode());
    EXPECT_EQ(static_cast<int>(i + 8),
              dom_selection.Focus().OffsetInContainerNode());
  }
}

TEST_F(AccessibilitySelectionTest, SelectionWithEqualBaseAndExtent) {
  SetBodyInnerHTML(R"HTML(
      <select id="sel"><option>1</option></select>
      )HTML");
  AXObject* ax_sel =
      GetAXObjectByElementId("sel")->FirstChildIncludingIgnored();
  AXPosition ax_position = AXPosition::CreatePositionBeforeObject(*ax_sel);
  AXSelection::Builder builder;
  AXSelection ax_selection =
      builder.SetAnchor(ax_position).SetFocus(ax_position).Build();
}

TEST_F(AccessibilitySelectionTest, InvalidSelectionOnAShadowRoot) {
  GetPage().GetSettings().SetScriptEnabled(true);
  SetBodyInnerHTML(R"HTML(
		<div id="container">
		</div>
	)HTML");
  Element* const script_element =
      GetDocument().CreateRawElement(html_names::kScriptTag);
  script_element->setTextContent(R"SCRIPT(
      var container = document.getElementById("container");
			var shadow = container.attachShadow({mode: 'open'});
			var button = document.createElement("button");
			button.id = "button";
			shadow.appendChild(button);
      )SCRIPT");
  GetDocument().body()->AppendChild(script_element);
  UpdateAllLifecyclePhasesForTest();

  Node* shadow_root = GetElementById("container")->GetShadowRoot();
  const Position base = Position::EditingPositionOf(shadow_root, 0);
  const Position extent = Position::EditingPositionOf(shadow_root, 1);

  const auto selection =
      SelectionInDOMTree::Builder().SetBaseAndExtent(base, extent).Build();
  EXPECT_FALSE(AXSelection::FromSelection(selection).IsValid());
}

//
// Declarative tests.
//

TEST_F(AccessibilitySelectionTest, ARIAHidden) {
  RunSelectionTest("aria-hidden");
}

TEST_F(AccessibilitySelectionTest, List) {
  RunSelectionTest("list");
}

TEST_F(AccessibilitySelectionTest, ParagraphPresentational) {
  // The focus of the selection is an "after children" position on a paragraph
  // with role="presentation" and in which the last child is an empty div. In
  // other words, both the paragraph and its last child are ignored in the
  // accessibility tree. In order to become valid, the focus should move to
  // before the next unignored child of the presentational paragraph's unignored
  // parent, which in this case is another paragraph that comes after the
  // presentational one.
  RunSelectionTest("paragraph-presentational");
}

TEST_F(AccessibilitySelectionTest, SVG) {
  RunSelectionTest("svg");
}

TEST_F(AccessibilitySelectionTest, Table) {
  RunSelectionTest("table");
}

}  // namespace test
}  // namespace blink
