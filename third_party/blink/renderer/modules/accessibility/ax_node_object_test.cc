// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_node_object.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object-inl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_test.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_data.h"

namespace blink {
namespace test {

TEST_F(AccessibilityTest, TextOffsetInFormattingContextWithLayoutReplaced) {
  SetBodyInnerHTML(R"HTML(
      <p>
        Before <img id="replaced" alt="alt"> after.
      </p>)HTML");

  const AXObject* ax_replaced = GetAXObjectByElementId("replaced");
  ASSERT_NE(nullptr, ax_replaced);
  ASSERT_EQ(ax::mojom::Role::kImage, ax_replaced->RoleValue());
  ASSERT_EQ("alt", ax_replaced->ComputedName());
  // After white space is compressed, the word "before" plus a single white
  // space is of length 7.
  EXPECT_EQ(7, ax_replaced->TextOffsetInFormattingContext(0));
  EXPECT_EQ(8, ax_replaced->TextOffsetInFormattingContext(1));
}

TEST_F(AccessibilityTest, TextOffsetInFormattingContextWithLayoutInline) {
  SetBodyInnerHTML(R"HTML(
      <p>
        Before <a id="inline" href="#">link</a> after.
      </p>)HTML");

  const AXObject* ax_inline = GetAXObjectByElementId("inline");
  ASSERT_NE(nullptr, ax_inline);
  ASSERT_EQ(ax::mojom::Role::kLink, ax_inline->RoleValue());
  ASSERT_EQ("link", ax_inline->ComputedName());
  // After white space is compressed, the word "before" plus a single white
  // space is of length 7.
  EXPECT_EQ(7, ax_inline->TextOffsetInFormattingContext(0));
  EXPECT_EQ(8, ax_inline->TextOffsetInFormattingContext(1));
}

TEST_F(AccessibilityTest,
       TextOffsetInFormattingContextWithLayoutBlockFlowAtInlineLevel) {
  SetBodyInnerHTML(R"HTML(
      <p>
        Before
        <b id="block-flow" style="display: inline-block;">block flow</b>
        after.
      </p>)HTML");

  const AXObject* ax_block_flow = GetAXObjectByElementId("block-flow");
  ASSERT_NE(nullptr, ax_block_flow);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, ax_block_flow->RoleValue());
  // After white space is compressed, the word "before" plus a single white
  // space is of length 7.
  EXPECT_EQ(7, ax_block_flow->TextOffsetInFormattingContext(0));
  EXPECT_EQ(8, ax_block_flow->TextOffsetInFormattingContext(1));
}

TEST_F(AccessibilityTest,
       TextOffsetInFormattingContextWithLayoutBlockFlowAtBlockLevel) {
  // OffsetMapping does not support block flow objects that are at
  // block-level, so we do not support them as well.
  SetBodyInnerHTML(R"HTML(
      <p>
        Before
        <b id="block-flow" style="display: block;">block flow</b>
        after.
      </p>)HTML");

  const AXObject* ax_block_flow = GetAXObjectByElementId("block-flow");
  ASSERT_NE(nullptr, ax_block_flow);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, ax_block_flow->RoleValue());
  // Since block-level elements do not expose a count of the number of
  // characters from the beginning of their formatting context, we return the
  // same offset that was passed in.
  EXPECT_EQ(0, ax_block_flow->TextOffsetInFormattingContext(0));
  EXPECT_EQ(1, ax_block_flow->TextOffsetInFormattingContext(1));
}

TEST_F(AccessibilityTest, TextOffsetInFormattingContextWithLayoutText) {
  SetBodyInnerHTML(R"HTML(
      <p>
        Before <span id="span">text</span> after.
      </p>)HTML");

  const AXObject* ax_text =
      GetAXObjectByElementId("span")->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_text->RoleValue());
  ASSERT_EQ("text", ax_text->ComputedName());
  // After white space is compressed, the word "before" plus a single white
  // space is of length 7.
  EXPECT_EQ(7, ax_text->TextOffsetInFormattingContext(0));
  EXPECT_EQ(8, ax_text->TextOffsetInFormattingContext(1));
}

TEST_F(AccessibilityTest, TextOffsetInFormattingContextWithLayoutBr) {
  SetBodyInnerHTML(R"HTML(
      <p>
        Before <br id="br"> after.
      </p>)HTML");

  const AXObject* ax_br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, ax_br);
  ASSERT_EQ(ax::mojom::Role::kLineBreak, ax_br->RoleValue());
  ASSERT_EQ("\n", ax_br->ComputedName());
  // After white space is compressed, the word "before" is of length 6.
  EXPECT_EQ(6, ax_br->TextOffsetInFormattingContext(0));
  EXPECT_EQ(7, ax_br->TextOffsetInFormattingContext(1));
}

TEST_F(AccessibilityTest, TextOffsetInFormattingContextWithLayoutFirstLetter) {
  SetBodyInnerHTML(R"HTML(
      <style>
        q::first-letter {
          color: red;
        }
      </style>
      <p>
        Before
        <q id="first-letter">1. Remaining part</q>
        after.
      </p>)HTML");

  const AXObject* ax_first_letter = GetAXObjectByElementId("first-letter");
  ASSERT_NE(nullptr, ax_first_letter);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, ax_first_letter->RoleValue());
  // After white space is compressed, the word "before" plus a single white
  // space is of length 7.
  EXPECT_EQ(7, ax_first_letter->TextOffsetInFormattingContext(0));
  EXPECT_EQ(8, ax_first_letter->TextOffsetInFormattingContext(1));
}

TEST_F(AccessibilityTest, FocusgroupOwnerImpliedRoleGenericContainer) {
  SetBodyInnerHTML(R"HTML(
      <div id="fg" focusgroup="toolbar">
        <button>Item</button>
      </div>)HTML");

  const AXObject* fg = GetAXObjectByElementId("fg");
  ASSERT_NE(nullptr, fg);
  // A generic container with focusgroup behavior should be promoted to the
  // minimum ARIA role for its behavior (toolbar).
  EXPECT_EQ(ax::mojom::Role::kToolbar, fg->RoleValue());
}

TEST_F(AccessibilityTest, FocusgroupOwnerDoesNotOverrideExplicitRole) {
  SetBodyInnerHTML(R"HTML(
      <div id="fg" role="list" focusgroup="toolbar">
        <div>Item</div>
      </div>)HTML");
  const AXObject* fg = GetAXObjectByElementId("fg");
  ASSERT_NE(nullptr, fg);
  // The explicit author role should be preserved (list) and not overridden by
  // focusgroup implied role inference.
  EXPECT_EQ(ax::mojom::Role::kList, fg->RoleValue());
}

TEST_F(AccessibilityTest, FocusgroupOwnerDoesNotOverrideNativeSemantics) {
  SetBodyInnerHTML(R"HTML(
      <ul id="fg" focusgroup="toolbar">
        <li>Item</li>
      </ul>)HTML");
  const AXObject* fg = GetAXObjectByElementId("fg");
  ASSERT_NE(nullptr, fg);
  // Native semantic list role should remain (list) and not be replaced by
  // toolbar.
  EXPECT_EQ(ax::mojom::Role::kList, fg->RoleValue());
}

TEST_F(AccessibilityTest, FocusgroupItemImpliedRoleTablist) {
  SetBodyInnerHTML(R"HTML(
      <div id="fg" focusgroup="tablist">
        <span tabindex="0" id="child">Item</span>
      </div>)HTML");
  const AXObject* child = GetAXObjectByElementId("child");
  ASSERT_NE(nullptr, child);
  EXPECT_EQ(ax::mojom::Role::kTab, child->ComputeFinalRoleForSerialization());
}

TEST_F(AccessibilityTest, FocusgroupItemImpliedRoleRadiogroup) {
  SetBodyInnerHTML(R"HTML(
      <div id="fg" focusgroup="radiogroup">
        <span tabindex="0" id="child">Item</span>
      </div>)HTML");
  const AXObject* child = GetAXObjectByElementId("child");
  ASSERT_NE(nullptr, child);
  EXPECT_EQ(ax::mojom::Role::kRadioButton,
            child->ComputeFinalRoleForSerialization());
}

TEST_F(AccessibilityTest, FocusgroupItemImpliedRoleListbox) {
  SetBodyInnerHTML(R"HTML(
      <div id="fg" focusgroup="listbox">
        <span tabindex="0" id="child">Item</span>
      </div>)HTML");
  const AXObject* child = GetAXObjectByElementId("child");
  ASSERT_NE(nullptr, child);
  EXPECT_EQ(ax::mojom::Role::kListBoxOption,
            child->ComputeFinalRoleForSerialization());
}

TEST_F(AccessibilityTest, FocusgroupItemImpliedRoleMenu) {
  SetBodyInnerHTML(R"HTML(
      <div id="fg" focusgroup="menu">
        <span tabindex="0" id="child">Item</span>
      </div>)HTML");
  const AXObject* child = GetAXObjectByElementId("child");
  ASSERT_NE(nullptr, child);
  EXPECT_EQ(ax::mojom::Role::kMenuItem,
            child->ComputeFinalRoleForSerialization());
}

TEST_F(AccessibilityTest, FocusgroupItemImpliedRoleMenubar) {
  SetBodyInnerHTML(R"HTML(
      <div id="fg" focusgroup="menubar">
        <span tabindex="0" id="child">Item</span>
      </div>)HTML");
  const AXObject* child = GetAXObjectByElementId("child");
  ASSERT_NE(nullptr, child);
  EXPECT_EQ(ax::mojom::Role::kMenuItem,
            child->ComputeFinalRoleForSerialization());
}

TEST_F(AccessibilityTest, FocusgroupItemExplicitRolePreserved) {
  SetBodyInnerHTML(R"HTML(
      <div id="fg" focusgroup="tablist">
        <span tabindex="0" id="child" role="listitem">Item</span>
      </div>)HTML");
  const AXObject* child = GetAXObjectByElementId("child");
  ASSERT_NE(nullptr, child);
  // Explicit author role should not be overridden by implied mapping.
  EXPECT_EQ(ax::mojom::Role::kListItem,
            child->ComputeFinalRoleForSerialization());
}

TEST_F(AccessibilityTest, FocusgroupItemNativeSemanticsPreserved) {
  SetBodyInnerHTML(R"HTML(
      <div id="fg" focusgroup="radiogroup">
        <button id="child">Button</button>
      </div>)HTML");
  const AXObject* child = GetAXObjectByElementId("child");
  ASSERT_NE(nullptr, child);
  // Native button semantics should remain, not replaced by radio.
  EXPECT_EQ(ax::mojom::Role::kButton,
            child->ComputeFinalRoleForSerialization());
}

TEST_F(AccessibilityTest,
       TextOffsetInFormattingContextWithCSSGeneratedContent) {
  SetBodyInnerHTML(R"HTML(
      <style>
        q::before {
          content: "<";
          color: blue;
        }
        q::after {
          content: ">";
          color: red;
        }
      </style>
      <p>
        Before <q id="css-generated">CSS generated</q> after.
      </p>)HTML");

  const AXObject* ax_css_generated = GetAXObjectByElementId("css-generated");
  ASSERT_NE(nullptr, ax_css_generated);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, ax_css_generated->RoleValue());
  // After white space is compressed, the word "before" plus a single white
  // space is of length 7.
  EXPECT_EQ(7, ax_css_generated->TextOffsetInFormattingContext(0));
  EXPECT_EQ(8, ax_css_generated->TextOffsetInFormattingContext(1));
}

TEST_F(AccessibilityTest, ScrollButtonAccessibilityRole) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #carousel {
      scroll-marker-group: after;

      &::scroll-button(inline-start) {
        content: '<';
      }
      &::scroll-button(inline-end) {
        content: '>' / 'next';
      }
    }
    </style>

    <div id=carousel>
      <div>One</div>
      <div>Two</div>
    </div>)HTML");

  const AXObject* left_button =
      GetAXObjectByElementId("carousel", kPseudoIdScrollButtonInlineStart);
  ASSERT_NE(nullptr, left_button);
  ASSERT_EQ(ax::mojom::Role::kButton, left_button->RoleValue());

  const AXObject* right_button =
      GetAXObjectByElementId("carousel", kPseudoIdScrollButtonInlineEnd);
  ASSERT_NE(nullptr, right_button);
  ASSERT_EQ(ax::mojom::Role::kButton, right_button->RoleValue());
}

TEST_F(AccessibilityTest, ScrollButtonAndMarkerGroupParent) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #carousel {
      scroll-marker-group: after;
      overflow: scroll;

      &::scroll-button(inline-start) {
        content: '<';
      }
      & > .item {
        &::scroll-marker {
          content: '';
        }
      }
    }
    </style>

    <div id=wrapper>
      <div id=carousel>
        <div class=item>One</div>
        <div class=item>Two</div>
      </div>
    </div>)HTML");

  const AXObject* wrapper = GetAXObjectByElementId("wrapper");
  ASSERT_NE(nullptr, wrapper);

  // Check that button's parent is wrapper.
  const AXObject* button =
      GetAXObjectByElementId("carousel", kPseudoIdScrollButtonInlineStart);
  ASSERT_NE(nullptr, button);

  const Node* button_parent = AXObject::GetParentNodeForComputeParent(
      button->AXObjectCache(), button->GetNode());
  EXPECT_EQ(button_parent, wrapper->GetNode());

  // Check that marker group's parent is wrapper.
  const AXObject* marker_group =
      GetAXObjectByElementId("carousel", kPseudoIdScrollMarkerGroupAfter);
  ASSERT_NE(nullptr, marker_group);

  const Node* marker_group_parent = AXObject::GetParentNodeForComputeParent(
      marker_group->AXObjectCache(), marker_group->GetNode());
  EXPECT_EQ(marker_group_parent, wrapper->GetNode());
}

TEST_F(AccessibilityTest,
       TreeItemWithAriaCheckedShouldNotHaveImplicitAriaSelected) {
  SetBodyInnerHTML(R"HTML(
      <div role="tree" aria-multiselectable="true">
        <button role="treeitem" aria-checked="true" id="item1">Item 1</button>
        <button role="treeitem" aria-checked="false" id="item2">Item 2</button>
      </div>)HTML");

  const AXObject* item1 = GetAXObjectByElementId("item1");
  ASSERT_NE(nullptr, item1);
  EXPECT_EQ(ax::mojom::Role::kTreeItem, item1->RoleValue());
  // When aria-checked is present and tree is multiselectable,
  // implicit aria-selected should NOT be provided per spec.
  EXPECT_EQ(kSelectedStateUndefined, item1->IsSelected());

  const AXObject* item2 = GetAXObjectByElementId("item2");
  ASSERT_NE(nullptr, item2);
  EXPECT_EQ(ax::mojom::Role::kTreeItem, item2->RoleValue());
  EXPECT_EQ(kSelectedStateUndefined, item2->IsSelected());
}

TEST_F(AccessibilityTest,
       SingleSelectTreeWithAriaCheckedShouldNotHaveImplicitAriaSelected) {
  SetBodyInnerHTML(R"HTML(
      <div role="tree">
        <button role="treeitem" aria-checked="true" id="item1">Item 1</button>
        <button role="treeitem" aria-checked="false" id="item2">Item 2</button>
      </div>)HTML");

  const AXObject* item1 = GetAXObjectByElementId("item1");
  ASSERT_NE(nullptr, item1);
  EXPECT_EQ(ax::mojom::Role::kTreeItem, item1->RoleValue());
  // When aria-checked is present on any item, implicit aria-selected
  // should NOT be provided for any items per spec.
  EXPECT_EQ(kSelectedStateUndefined, item1->IsSelected());

  const AXObject* item2 = GetAXObjectByElementId("item2");
  ASSERT_NE(nullptr, item2);
  EXPECT_EQ(ax::mojom::Role::kTreeItem, item2->RoleValue());
  EXPECT_EQ(kSelectedStateUndefined, item2->IsSelected());
}

TEST_F(AccessibilityTest,
       MultiSelectTreeWithoutAriaCheckedShouldNotHaveImplicitAriaSelected) {
  SetBodyInnerHTML(R"HTML(
      <div role="tree" aria-multiselectable="true">
        <button role="treeitem" id="item1">Item 1</button>
        <button role="treeitem" id="item2">Item 2</button>
      </div>)HTML");

  const AXObject* item1 = GetAXObjectByElementId("item1");
  ASSERT_NE(nullptr, item1);
  EXPECT_EQ(ax::mojom::Role::kTreeItem, item1->RoleValue());
  // Multiselectable containers should not provide implicit aria-selected.
  EXPECT_EQ(kSelectedStateUndefined, item1->IsSelected());

  const AXObject* item2 = GetAXObjectByElementId("item2");
  ASSERT_NE(nullptr, item2);
  EXPECT_EQ(ax::mojom::Role::kTreeItem, item2->RoleValue());
  EXPECT_EQ(kSelectedStateUndefined, item2->IsSelected());
}

}  // namespace test

TEST_F(AccessibilityTest, RadioButtonsInGroupInTableRows) {
  SetBodyInnerHTML(R"HTML(
      <table>
        <tr>
          <th>Question</th>
          <th id="a1">1</th><th id="a2">2</th><th id="a3">3</th><th id="a4">4</th><th id="a5">5</th><th id="a6">No Answer</th>
        </tr>
        <tr>
          <th id="q1">Question 1?</th>
          <td><input aria-labelledby="a1" aria-describedby="q1" type="radio" name="1q" id="r1_1" /></td>
          <td><input aria-labelledby="a2" aria-describedby="q1" type="radio" name="1q" id="r1_2" /></td>
          <td><input aria-labelledby="a3" aria-describedby="q1" type="radio" name="1q" id="r1_3" /></td>
          <td><input aria-labelledby="a4" aria-describedby="q1" type="radio" name="1q" id="r1_4" /></td>
          <td><input aria-labelledby="a5" aria-describedby="q1" type="radio" name="1q" id="r1_5" /></td>
          <td><input aria-labelledby="a6" aria-describedby="q1" type="radio" name="1q" id="r1_6" /></td>
        </tr>
        <tr>
          <th id="q2">Question 2?</th>
          <td><input aria-labelledby="a1" aria-describedby="q2" type="radio" name="2q" id="r2_1" /></td>
          <td><input aria-labelledby="a2" aria-describedby="q2" type="radio" name="2q" id="r2_2" /></td>
          <td><input aria-labelledby="a3" aria-describedby="q3" type="radio" name="2q" id="r2_3" /></td>
          <td><input aria-labelledby="a4" aria-describedby="q2" type="radio" name="2q" id="r2_4" /></td>
          <td><input aria-labelledby="a5" aria-describedby="q2" type="radio" name="2q" id="r2_5" /></td>
          <td><input aria-labelledby="a6" aria-describedby="q2" type="radio" name="2q" id="r2_6" /></td>
        </tr>
      </table>
  )HTML");

  const AXObject* r1_1 = GetAXObjectByElementId("r1_1");
  ASSERT_NE(nullptr, r1_1);
  while (r1_1 && r1_1->RoleValue() != ax::mojom::Role::kRadioButton) {
    r1_1 = r1_1->FirstChildIncludingIgnored();
  }
  ASSERT_NE(nullptr, r1_1);
  EXPECT_EQ(ax::mojom::Role::kRadioButton, r1_1->RoleValue());

  const AXNodeObject* r1_1_node = To<AXNodeObject>(r1_1);
  AXObject::AXObjectVector group = r1_1_node->RadioButtonsInGroup();
  EXPECT_EQ(6u, group.size());

  ui::AXNodeData node_data;
  GetAXObjectCache().Freeze();
  r1_1->Serialize(&node_data, ui::kAXModeComplete);
  GetAXObjectCache().Thaw();
  // SetSize is not computed for radio buttons in AXNodeObject::SetSize unless
  // aria-setsize is present. It is computed in the browser process using
  // kRadioGroupIds.
  EXPECT_EQ(0, node_data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  std::vector<int32_t> radio_group_ids = node_data.GetIntListAttribute(
      ax::mojom::IntListAttribute::kRadioGroupIds);
  EXPECT_EQ(6u, radio_group_ids.size());
}

}  // namespace blink
