// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/blink_ax_tree_source.h"

#include <optional>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_inner_elements.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object-inl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_selection_test.h"
#include "ui/accessibility/ax_tree_data.h"

namespace blink {

namespace {

// `BlinkAXTreeSource` only populates the selection offset types on Android.
std::optional<ax::mojom::OffsetType> ExpectedOffsetType(
    ax::mojom::OffsetType offset_type) {
#if BUILDFLAG(IS_ANDROID)
  return offset_type;
#else
  return std::nullopt;
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace

class BlinkAXTreeSourceTest : public AccessibilitySelectionTest {
 protected:
  BlinkAXTreeSourceTest() = default;

  void GetSelectionFromTreeSource(ui::AXTreeData* tree_data) {
    BlinkAXTreeSource* tree_source =
        BlinkAXTreeSource::Create(GetAXObjectCache());
    tree_source->Freeze();
    tree_source->GetTreeData(tree_data);
    tree_source->Thaw();
  }
};

TEST_F(BlinkAXTreeSourceTest, FocusNotAtomicTextField_ValidSelection) {
  SetBodyInnerHTML(R"HTML(
    <div id="content" tabindex="0">
      <p id="p1">Hello</p>
      <p id="p2">World</p>
    </div>
  )HTML");

  Element* content = GetElementById("content");
  content->Focus();

  AXSelection selection = SetSelectionText(R"HTML(
    <div id="content" tabindex="0">
      <p id="p1">He^llo</p>
      <p id="p2">Wo|rld</p>
    </div>
  )HTML");
  ASSERT_TRUE(selection.IsValid());
  selection.Select();

  GetAXObjectCache().UpdateAXForAllDocuments();

  ui::AXTreeData tree_data;
  GetSelectionFromTreeSource(&tree_data);

  EXPECT_NE(tree_data.sel_anchor_object_id, ui::kInvalidAXNodeID);
  EXPECT_NE(tree_data.sel_focus_object_id, ui::kInvalidAXNodeID);

  const AXObject* anchor_obj =
      GetAXObjectCache().ObjectFromAXID(tree_data.sel_anchor_object_id);
  const AXObject* focus_obj =
      GetAXObjectCache().ObjectFromAXID(tree_data.sel_focus_object_id);

  ASSERT_TRUE(anchor_obj);
  ASSERT_TRUE(focus_obj);

  EXPECT_EQ(anchor_obj->GetNode(), GetElementById("p1")->firstChild());
  EXPECT_EQ(focus_obj->GetNode(), GetElementById("p2")->firstChild());
  EXPECT_EQ(tree_data.sel_anchor_offset, 2);  // "He^llo" -> offset 2
  EXPECT_EQ(tree_data.sel_focus_offset, 2);   // "Wo|rld" -> offset 2
  EXPECT_EQ(tree_data.sel_anchor_offset_type,
            ExpectedOffsetType(ax::mojom::OffsetType::kText));
  EXPECT_EQ(tree_data.sel_focus_offset_type,
            ExpectedOffsetType(ax::mojom::OffsetType::kText));
  EXPECT_FALSE(tree_data.sel_is_backward);
}

TEST_F(BlinkAXTreeSourceTest,
       FocusAtomicTextField_InvalidDocumentSelection_ValidTextFieldSelection) {
  SetBodyInnerHTML(R"HTML(
    <input id="input" value="Hello World">
  )HTML");

  HTMLInputElement* input = To<HTMLInputElement>(GetElementById("input"));
  input->Focus();
  input->SetSelectionRange(3, 8);  // "Hel[lo Wo]rld"

  // Clear DOM selection to force document selection to be invalid,
  // but keep focus on input.
  Selection().Clear();

  GetAXObjectCache().UpdateAXForAllDocuments();

  ui::AXTreeData tree_data;
  GetSelectionFromTreeSource(&tree_data);

  // It should fallback to text field selection.
  // Since we cleared the DOM selection, the text field selection is also
  // cleared (collapsed at 0).
  EXPECT_NE(tree_data.sel_anchor_object_id, ui::kInvalidAXNodeID);
  EXPECT_NE(tree_data.sel_focus_object_id, ui::kInvalidAXNodeID);

  const AXObject* anchor_obj =
      GetAXObjectCache().ObjectFromAXID(tree_data.sel_anchor_object_id);
  const AXObject* focus_obj =
      GetAXObjectCache().ObjectFromAXID(tree_data.sel_focus_object_id);

  ASSERT_TRUE(anchor_obj);
  ASSERT_TRUE(focus_obj);

  EXPECT_EQ(anchor_obj->GetNode(), input);
  EXPECT_EQ(focus_obj->GetNode(), input);
  EXPECT_EQ(tree_data.sel_anchor_offset, 0);
  EXPECT_EQ(tree_data.sel_focus_offset, 0);
  EXPECT_EQ(tree_data.sel_anchor_offset_type,
            ExpectedOffsetType(ax::mojom::OffsetType::kText));
  EXPECT_EQ(tree_data.sel_focus_offset_type,
            ExpectedOffsetType(ax::mojom::OffsetType::kText));
}

TEST_F(BlinkAXTreeSourceTest,
       FocusAtomicTextField_ValidDocumentSelectionWithinTextField) {
  SetBodyInnerHTML(R"HTML(
    <input id="input" value="Hello World">
  )HTML");

  HTMLInputElement* input = To<HTMLInputElement>(GetElementById("input"));
  input->Focus();
  input->SetSelectionRange(3, 8);  // "Hel[lo Wo]rld"

  // We don't clear DOM selection here. The DOM selection will be inside the
  // shadow DOM.
  GetAXObjectCache().UpdateAXForAllDocuments();

  ui::AXTreeData tree_data;
  GetSelectionFromTreeSource(&tree_data);

  // It should redirect to text field selection (anchored to input itself).
  EXPECT_NE(tree_data.sel_anchor_object_id, ui::kInvalidAXNodeID);
  EXPECT_NE(tree_data.sel_focus_object_id, ui::kInvalidAXNodeID);

  const AXObject* anchor_obj =
      GetAXObjectCache().ObjectFromAXID(tree_data.sel_anchor_object_id);
  const AXObject* focus_obj =
      GetAXObjectCache().ObjectFromAXID(tree_data.sel_focus_object_id);

  ASSERT_TRUE(anchor_obj);
  ASSERT_TRUE(focus_obj);

  EXPECT_EQ(anchor_obj->GetNode(), input);
  EXPECT_EQ(focus_obj->GetNode(), input);
  EXPECT_EQ(tree_data.sel_anchor_offset, 3);
  EXPECT_EQ(tree_data.sel_focus_offset, 8);
  EXPECT_EQ(tree_data.sel_anchor_offset_type,
            ExpectedOffsetType(ax::mojom::OffsetType::kText));
  EXPECT_EQ(tree_data.sel_focus_offset_type,
            ExpectedOffsetType(ax::mojom::OffsetType::kText));
}

TEST_F(BlinkAXTreeSourceTest,
       FocusAtomicTextField_ValidDocumentSelectionOutsideTextField) {
  SetBodyInnerHTML(R"HTML(
    <div id="content">Hello World</div>
    <input id="input" value="Inner Text">
  )HTML");

  HTMLInputElement* input = To<HTMLInputElement>(GetElementById("input"));
  ASSERT_TRUE(input);

  // Focus the input first.
  input->Focus();

  // Update AX and layout because Focus() might have dirtied it.
  GetAXObjectCache().UpdateAXForAllDocuments();

  // Construct selection manually in 'content' div (outside input).
  Element* content = GetElementById("content");
  Node* text_node = content->firstChild();
  ASSERT_TRUE(text_node);
  ASSERT_TRUE(text_node->IsTextNode());

  const AXObject* ax_text = GetAXObject(*text_node);
  ASSERT_TRUE(ax_text);

  // "He^l|lo World" -> "Hello World"
  // H(0) e(1) ^ l(2) | l(3) o(4)
  const auto ax_anchor = AXPosition::CreatePositionInTextObject(*ax_text, 2);
  const auto ax_focus = AXPosition::CreatePositionInTextObject(*ax_text, 3);

  AXSelection::Builder selection_builder(GetAXObjectCache());
  selection_builder.SetAnchor(ax_anchor).SetFocus(ax_focus);
  AXSelection selection = selection_builder.Build();

  ASSERT_TRUE(selection.IsValid());
  selection.Select();

  // Verify focus is on input.
  ASSERT_EQ(GetDocument().FocusedElement(), input);
  ASSERT_EQ(GetAXObjectCache().FocusedObject()->GetNode(), input);

  GetAXObjectCache().UpdateAXForAllDocuments();

  ui::AXTreeData tree_data;
  GetSelectionFromTreeSource(&tree_data);

  // It should NOT redirect to text field selection because selection is
  // completely outside. It should keep the document selection.
  EXPECT_NE(tree_data.sel_anchor_object_id, ui::kInvalidAXNodeID);
  EXPECT_NE(tree_data.sel_focus_object_id, ui::kInvalidAXNodeID);

  const AXObject* anchor_obj =
      GetAXObjectCache().ObjectFromAXID(tree_data.sel_anchor_object_id);
  const AXObject* focus_obj =
      GetAXObjectCache().ObjectFromAXID(tree_data.sel_focus_object_id);

  ASSERT_TRUE(anchor_obj);
  ASSERT_TRUE(focus_obj);

  EXPECT_EQ(anchor_obj->GetNode(), text_node);
  EXPECT_EQ(focus_obj->GetNode(), text_node);
  EXPECT_EQ(tree_data.sel_anchor_offset, 2);
  EXPECT_EQ(tree_data.sel_focus_offset, 3);
  EXPECT_EQ(tree_data.sel_anchor_offset_type,
            ExpectedOffsetType(ax::mojom::OffsetType::kText));
  EXPECT_EQ(tree_data.sel_focus_offset_type,
            ExpectedOffsetType(ax::mojom::OffsetType::kText));
}

TEST_F(BlinkAXTreeSourceTest,
       FocusAtomicTextField_OutOfSyncDOMSelection_GracefulFallback) {
  // Use a simple textarea. The exact content does not matter, only that
  // we can get it out of sync by mutating the shadow DOM directly.
  SetBodyInnerHTML(R"HTML(
    <textarea id="textarea">Hello</textarea>
  )HTML");

  HTMLTextAreaElement* textarea =
      To<HTMLTextAreaElement>(GetElementById("textarea"));
  ASSERT_TRUE(textarea);

  // Get the inner editor element.
  TextControlInnerEditorElement* inner_editor = textarea->InnerEditorElement();
  ASSERT_TRUE(inner_editor);

  // Append a text node "My na" to the inner editor.
  inner_editor->appendChild(GetDocument().createTextNode("My na"));

  // Focus the textarea.
  textarea->Focus();

  // Select all.
  Selection().SelectAll();

  // Update AX.
  GetAXObjectCache().UpdateAXForAllDocuments();

  // This should not crash, and selection should be empty/invalid because
  // the DOM selection is out of sync with the text control's value.
  ui::AXTreeData tree_data;
  GetSelectionFromTreeSource(&tree_data);

  EXPECT_EQ(tree_data.sel_anchor_object_id, ui::kInvalidAXNodeID);
  EXPECT_EQ(tree_data.sel_focus_object_id, ui::kInvalidAXNodeID);
  EXPECT_EQ(tree_data.sel_anchor_offset_type, std::nullopt);
  EXPECT_EQ(tree_data.sel_focus_offset_type, std::nullopt);
}

TEST_F(BlinkAXTreeSourceTest, TreePositionSelectionAroundImage) {
  SetBodyInnerHTML(
      R"HTML(<div id="content" tabindex="0"><p id="p1">Hello</p><img id="img" alt="photo"><p id="p2">World</p></div>)HTML");

  Element* content = GetElementById("content");
  content->Focus();
  GetAXObjectCache().UpdateAXForAllDocuments();

  const AXObject* ax_img = GetAXObjectByElementId("img");
  ASSERT_TRUE(ax_img);

  // Select the image as a whole: from before it to after it. Both endpoints
  // are child indices in the image's parent, i.e. tree positions.
  AXSelection::Builder selection_builder(GetAXObjectCache());
  selection_builder.SetAnchor(AXPosition::CreatePositionBeforeObject(*ax_img))
      .SetFocus(AXPosition::CreatePositionAfterObject(*ax_img));
  AXSelection selection = selection_builder.Build();
  ASSERT_TRUE(selection.IsValid());
  selection.Select();

  GetAXObjectCache().UpdateAXForAllDocuments();

  ui::AXTreeData tree_data;
  GetSelectionFromTreeSource(&tree_data);

  ASSERT_NE(tree_data.sel_anchor_object_id, ui::kInvalidAXNodeID);
  ASSERT_NE(tree_data.sel_focus_object_id, ui::kInvalidAXNodeID);
  EXPECT_EQ(tree_data.sel_anchor_offset_type,
            ExpectedOffsetType(ax::mojom::OffsetType::kChild));
  EXPECT_EQ(tree_data.sel_focus_offset_type,
            ExpectedOffsetType(ax::mojom::OffsetType::kChild));
  EXPECT_EQ(tree_data.sel_focus_offset, tree_data.sel_anchor_offset + 1);
}

TEST_F(BlinkAXTreeSourceTest, MixedOffsetTypeSelection) {
  SetBodyInnerHTML(
      R"HTML(<div id="content" tabindex="0"><p id="p1">Hello</p><img id="img" alt="photo"><p id="p2">World</p></div>)HTML");

  Element* content = GetElementById("content");
  content->Focus();
  GetAXObjectCache().UpdateAXForAllDocuments();

  const AXObject* ax_img = GetAXObjectByElementId("img");
  ASSERT_TRUE(ax_img);
  Node* p2_text_node = GetElementById("p2")->firstChild();
  ASSERT_TRUE(p2_text_node);
  const AXObject* ax_p2_text = GetAXObject(*p2_text_node);
  ASSERT_TRUE(ax_p2_text);

  // Select from before `#img` (a tree position, `kChild`) to character 2 inside
  // `#p2`'s text node (a text position, `kText`).
  AXSelection::Builder selection_builder(GetAXObjectCache());
  selection_builder.SetAnchor(AXPosition::CreatePositionBeforeObject(*ax_img))
      .SetFocus(AXPosition::CreatePositionInTextObject(*ax_p2_text, 2));
  AXSelection selection = selection_builder.Build();
  ASSERT_TRUE(selection.IsValid());
  selection.Select();

  GetAXObjectCache().UpdateAXForAllDocuments();

  ui::AXTreeData tree_data;
  GetSelectionFromTreeSource(&tree_data);

  ASSERT_NE(tree_data.sel_anchor_object_id, ui::kInvalidAXNodeID);
  ASSERT_NE(tree_data.sel_focus_object_id, ui::kInvalidAXNodeID);
  EXPECT_EQ(tree_data.sel_anchor_offset_type,
            ExpectedOffsetType(ax::mojom::OffsetType::kChild));
  EXPECT_EQ(tree_data.sel_focus_offset_type,
            ExpectedOffsetType(ax::mojom::OffsetType::kText));
  EXPECT_EQ(tree_data.sel_focus_offset, 2);
}

}  // namespace blink
