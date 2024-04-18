// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_position.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {
namespace test {

namespace {

constexpr char kCSSBeforeAndAfter[] = R"HTML(
    <style>
      q::before {
        content: "«";
        color: blue;
      }
      q::after {
        content: "»";
        color: red;
      }
    </style>
    <q id="quote">Hello there,</q> she said.
    )HTML";

constexpr char kHTMLTable[] = R"HTML(
    <p id="before">Before table.</p>
    <table id="table" border="1">
      <thead id="thead">
        <tr id="headerRow">
          <th id="firstHeaderCell">Number</th>
          <th>Month</th>
          <th id="lastHeaderCell">Expenses</th>
        </tr>
      </thead>
      <tbody id="tbody">
        <tr id="firstRow">
          <th id="firstCell">1</th>
          <td>Jan</td>
          <td>100</td>
        </tr>
        <tr>
          <th>2</th>
          <td>Feb</td>
          <td>150</td>
        </tr>
        <tr id="lastRow">
          <th>3</th>
          <td>Mar</td>
          <td id="lastCell">200</td>
        </tr>
      </tbody>
    </table>
    <p id="after">After table.</p>
    )HTML";

constexpr char kAOM[] = R"HTML(
    <p id="before">Before virtual AOM node.</p>
    <div id="aomParent"></div>
    <p id="after">After virtual AOM node.</p>
    <script>
      let parent = document.getElementById("aomParent");
      let node = MakeGarbageCollected<AccessibleNode>();
      node.role = "button";
      node.label = "Button";
      parent.accessibleNode.appendChild(node);
    </script>
    )HTML";

constexpr char kMap[] = R"HTML(
    <br id="br">
    <map id="map">
      <area shape="rect" coords="0,0,10,10" href="about:blank">
    </map>
    )HTML";
}  // namespace

//
// Basic tests.
//

TEST_F(AccessibilityTest, PositionInText) {
  SetBodyInnerHTML(R"HTML(<p id="paragraph">Hello</p>)HTML");
  const Node* text = GetElementById("paragraph")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreatePositionInTextObject(*ax_static_text, 3);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(3, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

// To prevent surprises when comparing equality of two |AXPosition|s, position
// before text object should be the same as position in text object at offset 0.
TEST_F(AccessibilityTest, PositionBeforeText) {
  SetBodyInnerHTML(R"HTML(<p id="paragraph">Hello</p>)HTML");
  const Node* text = GetElementById("paragraph")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreatePositionBeforeObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(0, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, PositionBeforeTextWithFirstLetterCSSRule) {
  SetBodyInnerHTML(
      R"HTML(<style>p ::first-letter { color: red; font-size: 200%; }</style>
      <p id="paragraph">Hello</p>)HTML");
  const Node* text = GetElementById("paragraph")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreatePositionBeforeObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(0, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

// To prevent surprises when comparing equality of two |AXPosition|s, position
// after text object should be the same as position in text object at offset
// text length.
TEST_F(AccessibilityTest, PositionAfterText) {
  SetBodyInnerHTML(R"HTML(<p id="paragraph">Hello</p>)HTML");
  const Node* text = GetElementById("paragraph")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreatePositionAfterObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(5, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, PositionBeforeLineBreak) {
  SetBodyInnerHTML(R"HTML(Hello<br id="br">there)HTML");
  const AXObject* ax_br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, ax_br);
  ASSERT_EQ(ax::mojom::Role::kLineBreak, ax_br->RoleValue());
  const AXObject* ax_div = ax_br->ParentObjectUnignored();
  ASSERT_NE(nullptr, ax_div);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, ax_div->RoleValue());

  const auto ax_position = AXPosition::CreatePositionBeforeObject(*ax_br);
  EXPECT_FALSE(ax_position.IsTextPosition());
  EXPECT_EQ(ax_div, ax_position.ContainerObject());
  EXPECT_EQ(1, ax_position.ChildIndex());
  EXPECT_EQ(ax_br, ax_position.ChildAfterTreePosition());

  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(GetDocument().body(), position.AnchorNode());
  EXPECT_EQ(1, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
}

TEST_F(AccessibilityTest, PositionAfterLineBreak) {
  SetBodyInnerHTML(R"HTML(Hello<br id="br">there)HTML");
  GetAXRootObject()->LoadInlineTextBoxes();
  const AXObject* ax_br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, ax_br);
  ASSERT_EQ(ax::mojom::Role::kLineBreak, ax_br->RoleValue());
  const AXObject* ax_static_text =
      GetAXRootObject()->DeepestLastChildIncludingIgnored()->ParentObject();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue());

  const auto ax_position = AXPosition::CreatePositionAfterObject(*ax_br);
  EXPECT_EQ(ax_static_text, ax_position.ContainerObject());
  EXPECT_TRUE(ax_position.IsTextPosition());
  EXPECT_EQ(0, ax_position.TextOffset());

  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(ax_static_text->GetNode(), position.AnchorNode());
  EXPECT_TRUE(position.GetPosition().IsOffsetInAnchor());
  EXPECT_EQ(0, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
}

TEST_F(AccessibilityTest, FirstPositionInDivContainer) {
  SetBodyInnerHTML(R"HTML(<div id="div">Hello<br>there</div>)HTML");
  const Element* div = GetElementById("div");
  ASSERT_NE(nullptr, div);
  const AXObject* ax_div = GetAXObjectByElementId("div");
  ASSERT_NE(nullptr, ax_div);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, ax_div->RoleValue());
  const AXObject* ax_static_text = ax_div->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue());

  // "Before object" positions that are anchored to before a text object are
  // always converted to a "text position" before the object's first unignored
  // character.
  const auto ax_position = AXPosition::CreateFirstPositionInObject(*ax_div);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(div->firstChild(), position.AnchorNode());
  EXPECT_TRUE(position.GetPosition().IsOffsetInAnchor());
  EXPECT_EQ(0, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_TRUE(ax_position_from_dom.IsTextPosition());
  EXPECT_EQ(ax_static_text, ax_position_from_dom.ContainerObject());
  EXPECT_EQ(0, ax_position_from_dom.TextOffset());
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, LastPositionInDivContainer) {
  SetBodyInnerHTML(R"HTML(<div id="div">Hello<br>there</div>
                   <div>Next div</div>)HTML");
  const Element* div = GetElementById("div");
  ASSERT_NE(nullptr, div);
  const AXObject* ax_div = GetAXObjectByElementId("div");
  ASSERT_NE(nullptr, ax_div);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, ax_div->RoleValue());

  const auto ax_position = AXPosition::CreateLastPositionInObject(*ax_div);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(div, position.AnchorNode());
  EXPECT_TRUE(position.GetPosition().IsAfterChildren());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, FirstPositionInTextContainer) {
  SetBodyInnerHTML(R"HTML(<div id="div">Hello</div>)HTML");
  const Node* text = GetElementById("div")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_static_text =
      GetAXObjectByElementId("div")->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreateFirstPositionInObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(0, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, LastPositionInTextContainer) {
  SetBodyInnerHTML(R"HTML(<div id="div">Hello</div>)HTML");
  const Node* text = GetElementById("div")->lastChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_static_text =
      GetAXObjectByElementId("div")->LastChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreateLastPositionInObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(5, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

//
// Test comparing two AXPosition objects based on their position in the
// accessibility tree.
//

TEST_F(AccessibilityTest, AXPositionComparisonOperators) {
  SetBodyInnerHTML(R"HTML(<input id="input" type="text" value="value">
                   <p id="paragraph">hello<br>there</p>)HTML");

  const AXObject* body = GetAXBodyObject();
  ASSERT_NE(nullptr, body);
  const auto root_first = AXPosition::CreateFirstPositionInObject(*body);
  const auto root_last = AXPosition::CreateLastPositionInObject(*body);

  const AXObject* input = GetAXObjectByElementId("input");
  ASSERT_NE(nullptr, input);
  const auto input_before = AXPosition::CreatePositionBeforeObject(*input);
  const auto input_after = AXPosition::CreatePositionAfterObject(*input);

  const AXObject* paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, paragraph);
  ASSERT_NE(nullptr, paragraph->FirstChildIncludingIgnored());
  ASSERT_NE(nullptr, paragraph->LastChildIncludingIgnored());
  const auto paragraph_before = AXPosition::CreatePositionBeforeObject(
      *paragraph->FirstChildIncludingIgnored());
  const auto paragraph_after = AXPosition::CreatePositionAfterObject(
      *paragraph->LastChildIncludingIgnored());
  const auto paragraph_start = AXPosition::CreatePositionInTextObject(
      *paragraph->FirstChildIncludingIgnored(), 0);
  const auto paragraph_end = AXPosition::CreatePositionInTextObject(
      *paragraph->LastChildIncludingIgnored(), 5);

  EXPECT_TRUE(root_first == root_first);
  EXPECT_TRUE(root_last == root_last);
  EXPECT_FALSE(root_first != root_first);
  EXPECT_TRUE(root_first != root_last);

  EXPECT_TRUE(root_first < root_last);
  EXPECT_TRUE(root_first <= root_first);
  EXPECT_TRUE(root_last > root_first);
  EXPECT_TRUE(root_last >= root_last);

  EXPECT_TRUE(input_before == root_first);
  EXPECT_TRUE(input_after > root_first);
  EXPECT_TRUE(input_after >= root_first);
  EXPECT_FALSE(input_before < root_first);
  EXPECT_TRUE(input_before <= root_first);

  //
  // Text positions.
  //

  EXPECT_TRUE(paragraph_before == paragraph_start);
  EXPECT_TRUE(paragraph_after == paragraph_end);
  EXPECT_TRUE(paragraph_start < paragraph_end);
}

TEST_F(AccessibilityTest, AXPositionOperatorBool) {
  SetBodyInnerHTML(R"HTML(Hello)HTML");
  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const auto root_first = AXPosition::CreateFirstPositionInObject(*root);
  EXPECT_TRUE(static_cast<bool>(root_first));
  // The following should create an after children position on the root so it
  // should be valid.
  EXPECT_TRUE(static_cast<bool>(root_first.CreateNextPosition()));
  EXPECT_FALSE(static_cast<bool>(root_first.CreatePreviousPosition()));
}

//
// Test converting to and from visible text with white space.
// The accessibility tree is based on visible text with white space compressed,
// vs. the DOM tree where white space is preserved.
//

TEST_F(AccessibilityTest, PositionInTextWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<p id="paragraph">     Hello     </p>)HTML");
  const Node* text = GetElementById("paragraph")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreatePositionInTextObject(*ax_static_text, 3);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(8, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, PositionBeforeTextWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<p id="paragraph">     Hello     </p>)HTML");
  const Node* text = GetElementById("paragraph")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreatePositionBeforeObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(5, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, PositionAfterTextWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<p id="paragraph">     Hello     </p>)HTML");
  const Node* text = GetElementById("paragraph")->lastChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->LastChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreatePositionAfterObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(10, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, PositionBeforeLineBreakWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(Hello     <br id="br">     there)HTML");
  const AXObject* ax_br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, ax_br);
  ASSERT_EQ(ax::mojom::Role::kLineBreak, ax_br->RoleValue());
  const AXObject* ax_div = ax_br->ParentObjectUnignored();
  ASSERT_NE(nullptr, ax_div);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, ax_div->RoleValue());

  const auto ax_position = AXPosition::CreatePositionBeforeObject(*ax_br);
  EXPECT_FALSE(ax_position.IsTextPosition());
  EXPECT_EQ(ax_div, ax_position.ContainerObject());
  EXPECT_EQ(1, ax_position.ChildIndex());
  EXPECT_EQ(ax_br, ax_position.ChildAfterTreePosition());

  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(GetDocument().body(), position.AnchorNode());
  EXPECT_EQ(1, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
}

TEST_F(AccessibilityTest, PositionAfterLineBreakWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(Hello     <br id="br">     there)HTML");
  GetAXRootObject()->LoadInlineTextBoxes();
  const AXObject* ax_br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, ax_br);
  ASSERT_EQ(ax::mojom::Role::kLineBreak, ax_br->RoleValue());
  const AXObject* ax_static_text =
      GetAXRootObject()->DeepestLastChildIncludingIgnored()->ParentObject();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue());

  const auto ax_position = AXPosition::CreatePositionAfterObject(*ax_br);
  EXPECT_EQ(ax_static_text, ax_position.ContainerObject());
  EXPECT_TRUE(ax_position.IsTextPosition());
  EXPECT_EQ(0, ax_position.TextOffset());

  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(ax_static_text->GetNode(), position.AnchorNode());
  EXPECT_TRUE(position.GetPosition().IsOffsetInAnchor());
  // Any white space in the DOM should have been skipped.
  EXPECT_EQ(5, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
}

TEST_F(AccessibilityTest, FirstPositionInDivContainerWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<div id="div">     Hello<br>there     </div>)HTML");
  const Element* div = GetElementById("div");
  ASSERT_NE(nullptr, div);
  const AXObject* ax_div = GetAXObjectByElementId("div");
  ASSERT_NE(nullptr, ax_div);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, ax_div->RoleValue());
  const AXObject* ax_static_text = ax_div->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue());

  // "Before object" positions that are anchored to before a text object are
  // always converted to a "text position" before the object's first unignored
  // character.
  const auto ax_position = AXPosition::CreateFirstPositionInObject(*ax_div);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(div->firstChild(), position.AnchorNode());
  EXPECT_TRUE(position.GetPosition().IsOffsetInAnchor());
  // Any white space in the DOM should have been skipped.
  EXPECT_EQ(5, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_TRUE(ax_position_from_dom.IsTextPosition());
  EXPECT_EQ(ax_static_text, ax_position_from_dom.ContainerObject());
  EXPECT_EQ(0, ax_position_from_dom.TextOffset());
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, LastPositionInDivContainerWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<div id="div">     Hello<br>there     </div>
                   <div>Next div</div>)HTML");
  const Element* div = GetElementById("div");
  ASSERT_NE(nullptr, div);
  const AXObject* ax_div = GetAXObjectByElementId("div");
  ASSERT_NE(nullptr, ax_div);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, ax_div->RoleValue());

  const auto ax_position = AXPosition::CreateLastPositionInObject(*ax_div);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(div, position.AnchorNode());
  EXPECT_TRUE(position.GetPosition().IsAfterChildren());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, FirstPositionInTextContainerWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<div id="div">     Hello     </div>)HTML");
  const Node* text = GetElementById("div")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_static_text =
      GetAXObjectByElementId("div")->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreateFirstPositionInObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  // Any white space in the DOM should have been skipped.
  EXPECT_EQ(5, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, LastPositionInTextContainerWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<div id="div">     Hello     </div>)HTML");
  const Node* text = GetElementById("div")->lastChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_static_text =
      GetAXObjectByElementId("div")->LastChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue());

  const auto ax_position =
      AXPosition::CreateLastPositionInObject(*ax_static_text);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_EQ(10, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(nullptr, ax_position_from_dom.ChildAfterTreePosition());
}

// Test that DOM positions in white space will be collapsed to the first or last
// valid offset in an |AXPosition|.
TEST_F(AccessibilityTest, AXPositionFromDOMPositionWithWhiteSpace) {
  SetBodyInnerHTML(R"HTML(<div id="div">     Hello     </div>)HTML");
  const Node* text = GetElementById("div")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  ASSERT_EQ(15U, text->textContent().length());
  const AXObject* ax_static_text =
      GetAXObjectByElementId("div")->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue());

  const Position position_at_start(*text, 0);
  const auto ax_position_at_start = AXPosition::FromPosition(position_at_start);
  EXPECT_TRUE(ax_position_at_start.IsTextPosition());
  EXPECT_EQ(ax_static_text, ax_position_at_start.ContainerObject());
  EXPECT_EQ(0, ax_position_at_start.TextOffset());
  EXPECT_EQ(nullptr, ax_position_at_start.ChildAfterTreePosition());

  const Position position_after_white_space(*text, 5);
  const auto ax_position_after_white_space =
      AXPosition::FromPosition(position_after_white_space);
  EXPECT_TRUE(ax_position_after_white_space.IsTextPosition());
  EXPECT_EQ(ax_static_text, ax_position_after_white_space.ContainerObject());
  EXPECT_EQ(0, ax_position_after_white_space.TextOffset());
  EXPECT_EQ(nullptr, ax_position_after_white_space.ChildAfterTreePosition());

  const Position position_at_end(*text, 15);
  const auto ax_position_at_end = AXPosition::FromPosition(position_at_end);
  EXPECT_TRUE(ax_position_at_end.IsTextPosition());
  EXPECT_EQ(ax_static_text, ax_position_at_end.ContainerObject());
  EXPECT_EQ(5, ax_position_at_end.TextOffset());
  EXPECT_EQ(nullptr, ax_position_at_end.ChildAfterTreePosition());

  const Position position_before_white_space(*text, 10);
  const auto ax_position_before_white_space =
      AXPosition::FromPosition(position_before_white_space);
  EXPECT_TRUE(ax_position_before_white_space.IsTextPosition());
  EXPECT_EQ(ax_static_text, ax_position_before_white_space.ContainerObject());
  EXPECT_EQ(5, ax_position_before_white_space.TextOffset());
  EXPECT_EQ(nullptr, ax_position_before_white_space.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, AXPositionsWithPreservedLeadingWhitespace) {
  SetBodyInnerHTML(R"HTML(
    <div id="div" style="white-space: pre-wrap;">   Bar</div>
    )HTML");

  const Node* text = GetElementById("div")->firstChild();
  ASSERT_NE(nullptr, text);
  EXPECT_TRUE(text->IsTextNode());
  EXPECT_EQ(6U, text->textContent().length());

  const Position position_at_start(*text, 0);
  const auto ax_position_at_start = AXPosition::FromPosition(position_at_start);
  EXPECT_TRUE(ax_position_at_start.IsTextPosition());
  EXPECT_EQ(0, ax_position_at_start.TextOffset());

  // If we didn't adjust for the break opportunity, the accessible text offset
  // would be 4 instead of 3.
  const Position position_after_white_space(*text, 3);
  const auto ax_position_after_white_space =
      AXPosition::FromPosition(position_after_white_space);
  EXPECT_TRUE(ax_position_after_white_space.IsTextPosition());
  EXPECT_EQ(3, ax_position_after_white_space.TextOffset());

  // If we didn't adjust for the break opportunity, the accessible text offset
  // would be 7 instead of 6.
  const Position position_at_end(*text, 6);
  const auto ax_position_at_end = AXPosition::FromPosition(position_at_end);
  EXPECT_TRUE(ax_position_at_end.IsTextPosition());
  EXPECT_EQ(6, ax_position_at_end.TextOffset());
}

TEST_F(AccessibilityTest, AXPositionsWithPreservedLeadingWhitespaceAndBreak) {
  SetBodyInnerHTML(R"HTML(
    <div><span id="foo" style="white-space:pre-wrap;"> Foo</span>
    <br>
    <span id="bar" style="white-space:pre-wrap;">   Bar</span></div>
    )HTML");

  const Node* span = GetElementById("foo");
  ASSERT_NE(nullptr, span);
  EXPECT_EQ(4U, span->textContent().length());

  const Node* text = span->firstChild();
  ASSERT_NE(nullptr, text);
  EXPECT_TRUE(text->IsTextNode());
  EXPECT_EQ(4U, text->textContent().length());

  const Position position_at_start_1(*text, 0);
  const auto ax_position_at_start_1 =
      AXPosition::FromPosition(position_at_start_1);
  EXPECT_TRUE(ax_position_at_start_1.IsTextPosition());
  EXPECT_EQ(0, ax_position_at_start_1.TextOffset());

  // If we didn't adjust for the break opportunity, the accessible text offset
  // would be 2 instead of 1.
  const Position position_after_white_space_1(*text, 1);
  const auto ax_position_after_white_space_1 =
      AXPosition::FromPosition(position_after_white_space_1);
  EXPECT_TRUE(ax_position_after_white_space_1.IsTextPosition());
  EXPECT_EQ(1, ax_position_after_white_space_1.TextOffset());

  // If we didn't adjust for the break opportunity, the accessible text offset
  // would be 5 instead of 4.
  const Position position_at_end_1(*text, 4);
  const auto ax_position_at_end_1 = AXPosition::FromPosition(position_at_end_1);
  EXPECT_TRUE(ax_position_at_end_1.IsTextPosition());
  EXPECT_EQ(4, ax_position_at_end_1.TextOffset());

  span = GetElementById("bar");
  ASSERT_NE(nullptr, span);
  EXPECT_EQ(6U, span->textContent().length());

  text = span->firstChild();
  ASSERT_NE(nullptr, text);
  EXPECT_TRUE(text->IsTextNode());
  EXPECT_EQ(6U, text->textContent().length());

  const Position position_at_start_2(*text, 0);
  const auto ax_position_at_start_2 =
      AXPosition::FromPosition(position_at_start_2);
  EXPECT_TRUE(ax_position_at_start_2.IsTextPosition());
  EXPECT_EQ(0, ax_position_at_start_2.TextOffset());

  // If we didn't adjust for the break opportunity, the accessible text offset
  // would be 4 instead of 3.
  const Position position_after_white_space_2(*text, 3);
  const auto ax_position_after_white_space_2 =
      AXPosition::FromPosition(position_after_white_space_2);
  EXPECT_TRUE(ax_position_after_white_space_2.IsTextPosition());
  EXPECT_EQ(3, ax_position_after_white_space_2.TextOffset());

  // If we didn't adjust for the break opportunity, the accessible text offset
  // would be 7 instead of 6.
  const Position position_at_end_2(*text, 6);
  const auto ax_position_at_end_2 = AXPosition::FromPosition(position_at_end_2);
  EXPECT_TRUE(ax_position_at_end_2.IsTextPosition());
  EXPECT_EQ(6, ax_position_at_end_2.TextOffset());
}

TEST_F(AccessibilityTest, AXPositionsInSVGTextWithXCoordinates) {
  SetBodyInnerHTML(R"HTML(
    <div>
    <svg version="1.1" baseProfile="basic" xmlns="http://www.w3.org/2000/svg"
         xmlns:xlink="http://www.w3.org/1999/xlink" viewBox="0 0 160 120">
    <text id="text" x="0 10 20 30 40 50 60 70 80 90 100 110">Hel<tspan>lo </tspan>
      <tspan id="tspan">world</tspan>!</text>
    </svg>
    </div>
    )HTML");

  // Check the text node containing "Hel"
  const Node* text = GetElementById("text")->firstChild();
  ASSERT_NE(nullptr, text);
  EXPECT_TRUE(text->IsTextNode());
  EXPECT_EQ(3U, text->textContent().length());
  EXPECT_EQ("Hel", text->textContent().Utf8());

  const Position position_at_h(*text, 0);
  const auto ax_position_at_h = AXPosition::FromPosition(position_at_h);
  EXPECT_TRUE(ax_position_at_h.IsTextPosition());
  EXPECT_EQ(0, ax_position_at_h.TextOffset());

  // If we didn't adjust for isolate characters, the accessible text offset
  // would be 7 instead of 3.
  const Position position_after_l(*text, 3);
  const auto ax_position_after_l = AXPosition::FromPosition(position_after_l);
  EXPECT_TRUE(ax_position_after_l.IsTextPosition());
  EXPECT_EQ(3, ax_position_after_l.TextOffset());

  // Check the text node child of the first tspan containing "lo "
  text = text->nextSibling()->firstChild();
  ASSERT_NE(nullptr, text);
  EXPECT_TRUE(text->IsTextNode());
  EXPECT_EQ(3U, text->textContent().length());
  EXPECT_EQ("lo ", text->textContent().Utf8());

  // If we didn't adjust for isolate characters, the accessible text offset
  // would be 3 instead of 1.
  const Position position_at_o(*text, 1);
  const auto ax_position_at_o = AXPosition::FromPosition(position_at_o);
  EXPECT_TRUE(ax_position_at_o.IsTextPosition());
  EXPECT_EQ(1, ax_position_at_o.TextOffset());

  // Check the text node child of the second tspan containing "world"
  text = GetElementById("tspan")->firstChild();
  ASSERT_NE(nullptr, text);
  EXPECT_TRUE(text->IsTextNode());
  EXPECT_EQ(5U, text->textContent().length());
  EXPECT_EQ("world", text->textContent().Utf8());

  // If we didn't adjust for isolate characters, the accessible text offset
  // would be 12 instead of 4.
  const Position position_at_d(*text, 4);
  const auto ax_position_at_d = AXPosition::FromPosition(position_at_d);
  EXPECT_TRUE(ax_position_at_d.IsTextPosition());
  EXPECT_EQ(4, ax_position_at_d.TextOffset());

  // Check the text node containing "!"
  text = GetElementById("text")->lastChild();
  ASSERT_NE(nullptr, text);
  EXPECT_TRUE(text->IsTextNode());
  EXPECT_EQ(1U, text->textContent().length());
  EXPECT_EQ("!", text->textContent().Utf8());

  const Position position_at_end(*text, 1);
  const auto ax_position_at_end = AXPosition::FromPosition(position_at_end);
  EXPECT_TRUE(ax_position_at_end.IsTextPosition());
  EXPECT_EQ(1, ax_position_at_end.TextOffset());
}

//
// Test affinity.
// We need to distinguish between the caret at the end of one line and the
// beginning of the next.
//

TEST_F(AccessibilityTest, PositionInTextWithAffinity) {
  SetBodyInnerHTML(R"HTML(<p id="paragraph">Hello</p>)HTML");
  const Node* text = GetElementById("paragraph")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue());

  // Converting from AX to DOM positions should maintain affinity.
  const auto ax_position = AXPosition::CreatePositionInTextObject(
      *ax_static_text, 3, TextAffinity::kUpstream);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(TextAffinity::kUpstream, position.Affinity());

  // Converting from DOM to AX positions should maintain affinity.
  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(TextAffinity::kUpstream, ax_position.Affinity());
}

//
// Test converting to and from accessibility positions with offsets in HTML
// labels. HTML labels are ignored in the accessibility tree when associated
// with checkboxes and radio buttons.
//

TEST_F(AccessibilityTest, PositionInHTMLLabel) {
  SetBodyInnerHTML(R"HTML(
      <label id="label" for="input">
        Label text.
      </label>
      <p id="paragraph">Intervening paragraph.</p>
      <input id="input">
      )HTML");

  const Node* label = GetElementById("label");
  ASSERT_NE(nullptr, label);
  const Node* label_text = label->firstChild();
  ASSERT_NE(nullptr, label_text);
  ASSERT_TRUE(label_text->IsTextNode());
  const Node* paragraph = GetElementById("paragraph");
  ASSERT_NE(nullptr, paragraph);

  const AXObject* ax_body = GetAXBodyObject();
  ASSERT_NE(nullptr, ax_body);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, ax_body->RoleValue());

  const AXObject* ax_label = GetAXObjectByElementId("label");
  ASSERT_NE(nullptr, ax_label);
  ASSERT_FALSE(ax_label->IsIgnored());
  const AXObject* ax_label_text = ax_label->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_label_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_label_text->RoleValue());
  const AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, ax_paragraph);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph->RoleValue());

  const auto position_before_label = Position::BeforeNode(*label);
  const auto ax_position_before_label =
      AXPosition::FromPosition(position_before_label, TextAffinity::kDownstream,
                               AXPositionAdjustmentBehavior::kMoveLeft);
  EXPECT_FALSE(ax_position_before_label.IsTextPosition());
  EXPECT_EQ(ax_body, ax_position_before_label.ContainerObject());
  EXPECT_EQ(0, ax_position_before_label.ChildIndex());
  EXPECT_EQ(ax_label, ax_position_before_label.ChildAfterTreePosition());

  const auto position_before_text = Position::BeforeNode(*label_text);
  const auto position_in_text = Position::FirstPositionInNode(*label_text);
  const auto position_after_label = Position::AfterNode(*label);
  for (const auto& position :
       {position_before_text, position_in_text, position_after_label}) {
    const auto ax_position =
        AXPosition::FromPosition(position, TextAffinity::kDownstream,
                                 AXPositionAdjustmentBehavior::kMoveLeft);
    EXPECT_TRUE(ax_position.IsTextPosition());
    EXPECT_EQ(ax_label_text, ax_position.ContainerObject());
    EXPECT_EQ(nullptr, ax_position.ChildAfterTreePosition());
  }
  const auto position_before_paragraph = Position::BeforeNode(*paragraph);
  const auto ax_position_before_paragraph = AXPosition::FromPosition(
      position_before_paragraph, TextAffinity::kDownstream,
      AXPositionAdjustmentBehavior::kMoveLeft);
  EXPECT_FALSE(ax_position_before_paragraph.IsTextPosition());
  EXPECT_EQ(ax_body, ax_position_before_paragraph.ContainerObject());
  EXPECT_EQ(1, ax_position_before_paragraph.ChildIndex());
  EXPECT_EQ(ax_paragraph,
            ax_position_before_paragraph.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, PositionInHTMLLabelIgnored) {
  SetBodyInnerHTML(R"HTML(
      <label id="label" for="input">
        Label text.
      </label>
      <p id="paragraph">Intervening paragraph.</p>
      <input id="input" type="checkbox" checked>
      )HTML");

  // For reference, this is the accessibility tree generated:
  // rootWebArea
  // ++genericContainer ignored
  // ++++genericContainer ignored
  // ++++++labelText ignored
  // ++++++++staticText ignored name='Label text.'
  // ++++++paragraph
  // ++++++++staticText name='Intervening paragraph.'
  // ++++++++++inlineTextBox name='Intervening paragraph.'
  // ++++++checkBox focusable name='Label text.'

  const Node* label = GetElementById("label");
  ASSERT_NE(nullptr, label);
  const Node* label_text = label->firstChild();
  ASSERT_NE(nullptr, label_text);
  ASSERT_TRUE(label_text->IsTextNode());
  const Node* paragraph = GetElementById("paragraph");
  ASSERT_NE(nullptr, paragraph);

  const AXObject* ax_body = GetAXBodyObject();
  ASSERT_NE(nullptr, ax_body);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, ax_body->RoleValue());

  // The HTML label element should be ignored.
  const AXObject* ax_label = GetAXObjectByElementId("label");
  ASSERT_NE(nullptr, ax_label);
  ASSERT_TRUE(ax_label->IsIgnored());
  const AXObject* ax_label_text = ax_label->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_label_text);
  ASSERT_TRUE(ax_label_text->IsIgnored());
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_label_text->RoleValue());
  const AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, ax_paragraph);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_paragraph->RoleValue());

  // The label element produces an ignored, but included node in the
  // accessibility tree. The position is set right before it.
  const auto position_before = Position::BeforeNode(*label);
  const auto ax_position_before =
      AXPosition::FromPosition(position_before, TextAffinity::kDownstream,
                               AXPositionAdjustmentBehavior::kMoveLeft);
  EXPECT_FALSE(ax_position_before.IsTextPosition());
  EXPECT_EQ(ax_body, ax_position_before.ContainerObject());
  EXPECT_EQ(0, ax_position_before.ChildIndex());
  EXPECT_EQ(ax_label, ax_position_before.ChildAfterTreePosition());

  const auto position_from_ax_before =
      ax_position_before.ToPositionWithAffinity();
  EXPECT_EQ(GetDocument().body(), position_from_ax_before.AnchorNode());
  EXPECT_EQ(1, position_from_ax_before.GetPosition().OffsetInContainerNode());
  EXPECT_EQ(label,
            position_from_ax_before.GetPosition().ComputeNodeAfterPosition());

  // A position anchored before a text node is explicitly moved to before the
  // first character of the text object. That's why these two positions are
  // effectively the same.
  const auto position_before_text = Position::BeforeNode(*label_text);
  const auto position_in_text = Position::FirstPositionInNode(*label_text);

  // This position points to the empty text node between the label and the
  // paragraph. That's invalid so it's moved the closest node to the left
  // (because we used AXPositionAdjustmentBehavior::kMoveLeft), landing in the
  // last character of the label text.
  const auto position_after = Position::AfterNode(*label);

  for (const auto& position :
       {position_before_text, position_in_text, position_after}) {
    const auto ax_position =
        AXPosition::FromPosition(position, TextAffinity::kDownstream,
                                 AXPositionAdjustmentBehavior::kMoveLeft);
    EXPECT_TRUE(ax_position.IsTextPosition());
    EXPECT_EQ(ax_label_text, ax_position.ContainerObject());
    EXPECT_EQ(nullptr, ax_position.ChildAfterTreePosition());

    const auto position_from_ax = ax_position.ToPositionWithAffinity();
    EXPECT_EQ(label_text, position_from_ax.AnchorNode());
    EXPECT_EQ(nullptr,
              position_from_ax.GetPosition().ComputeNodeAfterPosition());

    if (position == position_after) {
      // this position excludes whitespace
      EXPECT_EQ(11, ax_position.TextOffset());
      // this position includes the whitespace before "Label text."
      EXPECT_EQ(20, position_from_ax.GetPosition().OffsetInContainerNode());
    } else {
      // this position excludes whitespace
      EXPECT_EQ(0, ax_position.TextOffset());
      // this position includes the whitespace before "Label text."
      EXPECT_EQ(9, position_from_ax.GetPosition().OffsetInContainerNode());
    }
  }
}

//
// Objects with "display: none" or the "hidden" attribute are accessibility
// ignored.
//

TEST_F(AccessibilityTest, PositionInIgnoredObject) {
  // Note: aria-describedby adds hidden target subtrees to the a11y tree as
  // "ignored but included in tree".
  SetBodyInnerHTML(R"HTML(
      <div id="hidden" hidden aria-describedby="hidden">Hidden.</div><p id="visible">Visible.</p>
      )HTML");

  const Node* hidden = GetElementById("hidden");
  ASSERT_NE(nullptr, hidden);
  const Node* visible = GetElementById("visible");
  ASSERT_NE(nullptr, visible);

  const AXObject* ax_root = GetAXRootObject();
  ASSERT_NE(nullptr, ax_root);
  ASSERT_EQ(ax::mojom::Role::kRootWebArea, ax_root->RoleValue());
  ASSERT_EQ(1, ax_root->ChildCountIncludingIgnored());

  const AXObject* ax_html = ax_root->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_html);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, ax_html->RoleValue());
  ASSERT_EQ(1, ax_html->ChildCountIncludingIgnored());

  const AXObject* ax_body = GetAXBodyObject();
  ASSERT_NE(nullptr, ax_body);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, ax_body->RoleValue());
  ASSERT_EQ(2, ax_body->ChildCountIncludingIgnored());

  const AXObject* ax_hidden = GetAXObjectByElementId("hidden");
  ASSERT_NE(nullptr, ax_hidden);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, ax_hidden->RoleValue());
  ASSERT_TRUE(ax_hidden->IsIgnoredButIncludedInTree());

  const AXObject* ax_visible = GetAXObjectByElementId("visible");
  ASSERT_NE(nullptr, ax_visible);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_visible->RoleValue());

  // The fact that there is a hidden object before |visible| should not affect
  // setting a position before it.
  const auto ax_position_before_visible =
      AXPosition::CreatePositionBeforeObject(*ax_visible);
  const auto position_before_visible =
      ax_position_before_visible.ToPositionWithAffinity();
  EXPECT_EQ(GetDocument().body(), position_before_visible.AnchorNode());
  EXPECT_EQ(2, position_before_visible.GetPosition().OffsetInContainerNode());
  EXPECT_EQ(visible,
            position_before_visible.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_before_visible_from_dom =
      AXPosition::FromPosition(position_before_visible);
  EXPECT_EQ(ax_position_before_visible, ax_position_before_visible_from_dom);
  EXPECT_EQ(ax_visible,
            ax_position_before_visible_from_dom.ChildAfterTreePosition());

  // A position at the beginning of the body will appear to be before the hidden
  // element in the DOM.
  const auto ax_position_first =
      AXPosition::CreateFirstPositionInObject(*ax_root);
  const auto position_first = ax_position_first.ToPositionWithAffinity();
  EXPECT_EQ(GetDocument(), position_first.AnchorNode());
  EXPECT_TRUE(position_first.GetPosition().IsBeforeChildren());

  EXPECT_EQ(GetDocument().documentElement(),
            position_first.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_first_from_dom =
      AXPosition::FromPosition(position_first);
  EXPECT_EQ(ax_position_first, ax_position_first_from_dom);

  EXPECT_EQ(ax_html, ax_position_first_from_dom.ChildAfterTreePosition());

  // A DOM position before |hidden| should convert to an accessibility position
  // before |hidden| because the node is ignored but included in the tree.
  const auto position_before = Position::BeforeNode(*hidden);
  const auto ax_position_before_from_dom =
      AXPosition::FromPosition(position_before);
  EXPECT_EQ(ax_body, ax_position_before_from_dom.ContainerObject());
  EXPECT_EQ(0, ax_position_before_from_dom.ChildIndex());
  EXPECT_EQ(ax_hidden, ax_position_before_from_dom.ChildAfterTreePosition());

  // A DOM position after |hidden| should convert to an accessibility position
  // before |visible|.
  const auto position_after = Position::AfterNode(*hidden);
  const auto ax_position_after_from_dom =
      AXPosition::FromPosition(position_after);
  EXPECT_EQ(ax_body, ax_position_after_from_dom.ContainerObject());
  EXPECT_EQ(1, ax_position_after_from_dom.ChildIndex());
  EXPECT_EQ(ax_visible, ax_position_after_from_dom.ChildAfterTreePosition());
}

//
// Aria-hidden can cause things in the DOM to be hidden from accessibility.
//

TEST_F(AccessibilityTest, BeforePositionInARIAHiddenShouldNotSkipARIAHidden) {
  // Note: aria-describedby adds hidden target subtrees to the a11y tree as
  // "ignored but included in tree".
  SetBodyInnerHTML(R"HTML(
      <div role="main" id="container" aria-describedby="ariaHidden">
        <p id="before">Before aria-hidden.</p>
        <p id="ariaHidden" aria-hidden="true">Aria-hidden.</p>
        <p id="after">After aria-hidden.</p>
      </div>
      )HTML");

  const Node* container = GetElementById("container");
  ASSERT_NE(nullptr, container);
  const Node* after = GetElementById("after");
  ASSERT_NE(nullptr, after);
  const Node* hidden = GetElementById("ariaHidden");
  ASSERT_NE(nullptr, hidden);

  const AXObject* ax_before = GetAXObjectByElementId("before");
  ASSERT_NE(nullptr, ax_before);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_before->RoleValue());
  const AXObject* ax_after = GetAXObjectByElementId("after");
  ASSERT_NE(nullptr, ax_after);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_after->RoleValue());
  const AXObject* ax_hidden = GetAXObjectByElementId("ariaHidden");
  ASSERT_NE(nullptr, ax_hidden);
  ASSERT_TRUE(ax_hidden->IsIgnored());

  const auto ax_position = AXPosition::CreatePositionAfterObject(*ax_before);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(container, position.AnchorNode());
  EXPECT_EQ(3, position.GetPosition().OffsetInContainerNode());
  EXPECT_EQ(hidden, position.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(ax_hidden, ax_position_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest,
       PreviousPositionAfterARIAHiddenShouldNotSkipARIAHidden) {
  // Note: aria-describedby adds hidden target subtrees to the a11y tree as
  // "ignored but included in tree".
  SetBodyInnerHTML(R"HTML(
      <p id="before">Before aria-hidden.</p>
      <p id="ariaHidden" aria-describedby="ariaHidden" aria-hidden="true">Aria-hidden.</p>
      <p id="after">After aria-hidden.</p>
      )HTML");

  const Node* hidden = GetElementById("ariaHidden");
  ASSERT_NE(nullptr, hidden);
  ASSERT_NE(nullptr, hidden->firstChild());
  const Node* after = GetElementById("after");
  ASSERT_NE(nullptr, after);

  const AXObject* ax_after = GetAXObjectByElementId("after");
  ASSERT_NE(nullptr, ax_after);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_after->RoleValue());
  ASSERT_NE(nullptr, GetAXObjectByElementId("ariaHidden"));
  ASSERT_TRUE(GetAXObjectByElementId("ariaHidden")->IsIgnored());

  const auto ax_position = AXPosition::CreatePositionBeforeObject(*ax_after);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(GetDocument().body(), position.AnchorNode());
  EXPECT_EQ(5, position.GetPosition().OffsetInContainerNode());
  EXPECT_EQ(after, position.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(ax_after, ax_position_from_dom.ChildAfterTreePosition());

  const auto ax_position_previous = ax_position.CreatePreviousPosition();
  const auto position_previous = ax_position_previous.ToPositionWithAffinity();
  EXPECT_EQ(hidden->firstChild(), position_previous.AnchorNode());
  EXPECT_EQ(12, position_previous.GetPosition().OffsetInContainerNode());
  EXPECT_EQ(nullptr,
            position_previous.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_previous_from_dom =
      AXPosition::FromPosition(position_previous);
  EXPECT_EQ(ax_position_previous, ax_position_previous_from_dom);
  EXPECT_EQ(nullptr, ax_position_previous_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, FromPositionInARIAHidden) {
  // Note: aria-describedby adds hidden target subtrees to the a11y tree as
  // "ignored but included in tree".
  SetBodyInnerHTML(R"HTML(
      <div role="main" id="container">
        <p id="before">Before aria-hidden.</p>
        <p id="ariaHidden" aria-describedby="ariaHidden" aria-hidden="true">Aria-hidden.</p>
        <p id="after">After aria-hidden.</p>
      </div>
      )HTML");

  const Node* hidden = GetElementById("ariaHidden");
  ASSERT_NE(nullptr, hidden);

  const AXObject* ax_container = GetAXObjectByElementId("container");
  ASSERT_NE(nullptr, ax_container);
  ASSERT_EQ(ax::mojom::Role::kMain, ax_container->RoleValue());
  ASSERT_EQ(3, ax_container->ChildCountIncludingIgnored());
  const AXObject* ax_before = GetAXObjectByElementId("before");
  ASSERT_NE(nullptr, ax_before);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_before->RoleValue());
  const AXObject* ax_after = GetAXObjectByElementId("after");
  ASSERT_NE(nullptr, ax_after);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_after->RoleValue());
  const AXObject* ax_hidden = GetAXObjectByElementId("ariaHidden");
  ASSERT_NE(nullptr, ax_hidden);
  ASSERT_TRUE(ax_hidden->IsIgnored());

  const auto position_first = Position::FirstPositionInNode(*hidden);
  // Since "ax_hidden" has a static text child, the AXPosition should move to an
  // equivalent position on the static text child.
  auto ax_position_left =
      AXPosition::FromPosition(position_first, TextAffinity::kDownstream,
                               AXPositionAdjustmentBehavior::kMoveLeft);
  EXPECT_TRUE(ax_position_left.IsValid());
  EXPECT_TRUE(ax_position_left.IsTextPosition());
  EXPECT_EQ(ax_hidden->FirstChildIncludingIgnored(),
            ax_position_left.ContainerObject());
  EXPECT_EQ(0, ax_position_left.TextOffset());

  // In this case, the adjustment behavior should not affect the outcome because
  // there is an equivalent AXPosition in the static text child.
  auto ax_position_right =
      AXPosition::FromPosition(position_first, TextAffinity::kDownstream,
                               AXPositionAdjustmentBehavior::kMoveRight);
  EXPECT_TRUE(ax_position_right.IsValid());
  EXPECT_TRUE(ax_position_right.IsTextPosition());
  EXPECT_EQ(ax_hidden->FirstChildIncludingIgnored(),
            ax_position_right.ContainerObject());
  EXPECT_EQ(0, ax_position_right.TextOffset());

  const auto position_before = Position::BeforeNode(*hidden);
  ax_position_left =
      AXPosition::FromPosition(position_before, TextAffinity::kDownstream,
                               AXPositionAdjustmentBehavior::kMoveLeft);
  EXPECT_TRUE(ax_position_left.IsValid());
  EXPECT_FALSE(ax_position_left.IsTextPosition());
  EXPECT_EQ(ax_container, ax_position_left.ContainerObject());
  EXPECT_EQ(1, ax_position_left.ChildIndex());
  EXPECT_EQ(ax_hidden, ax_position_left.ChildAfterTreePosition());

  // Since an AXPosition before "ax_hidden" is valid, i.e. it does not need to
  // be adjusted, then adjustment behavior should not make a difference in the
  // outcome.
  ax_position_right =
      AXPosition::FromPosition(position_before, TextAffinity::kDownstream,
                               AXPositionAdjustmentBehavior::kMoveRight);
  EXPECT_TRUE(ax_position_right.IsValid());
  EXPECT_FALSE(ax_position_right.IsTextPosition());
  EXPECT_EQ(ax_container, ax_position_right.ContainerObject());
  EXPECT_EQ(1, ax_position_right.ChildIndex());
  EXPECT_EQ(ax_hidden, ax_position_right.ChildAfterTreePosition());

  // The DOM node right after "hidden" is accessibility ignored, so we should
  // see an adjustment in the relevant direction.
  const auto position_after = Position::AfterNode(*hidden);
  ax_position_left =
      AXPosition::FromPosition(position_after, TextAffinity::kDownstream,
                               AXPositionAdjustmentBehavior::kMoveLeft);
  EXPECT_TRUE(ax_position_left.IsValid());
  EXPECT_TRUE(ax_position_left.IsTextPosition());
  EXPECT_EQ(ax_hidden->FirstChildIncludingIgnored(),
            ax_position_left.ContainerObject());
  EXPECT_EQ(12, ax_position_left.TextOffset());

  ax_position_right =
      AXPosition::FromPosition(position_after, TextAffinity::kDownstream,
                               AXPositionAdjustmentBehavior::kMoveRight);
  EXPECT_TRUE(ax_position_right.IsValid());
  EXPECT_FALSE(ax_position_right.IsTextPosition());
  EXPECT_EQ(ax_container, ax_position_right.ContainerObject());
  EXPECT_EQ(2, ax_position_right.ChildIndex());
  EXPECT_EQ(ax_after, ax_position_right.ChildAfterTreePosition());
}

//
// Canvas fallback can cause things to be in the accessibility tree that are not
// in the layout tree.
//

TEST_F(AccessibilityTest, PositionInCanvas) {
  SetBodyInnerHTML(R"HTML(
      <canvas id="canvas1" width="100" height="100">Fallback text</canvas>
      <canvas id="canvas2" width="100" height="100">
      <button id="button">Fallback button</button>
    </canvas>
    )HTML");

  const Node* canvas_1 = GetElementById("canvas1");
  ASSERT_NE(nullptr, canvas_1);
  const Node* text = canvas_1->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  const Node* canvas_2 = GetElementById("canvas2");
  ASSERT_NE(nullptr, canvas_2);
  const Node* button = GetElementById("button");
  ASSERT_NE(nullptr, button);

  const AXObject* ax_canvas_1 = GetAXObjectByElementId("canvas1");
  ASSERT_NE(nullptr, ax_canvas_1);
  ASSERT_EQ(ax::mojom::Role::kCanvas, ax_canvas_1->RoleValue());
  const AXObject* ax_text = ax_canvas_1->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_text->RoleValue());
  const AXObject* ax_canvas_2 = GetAXObjectByElementId("canvas2");
  ASSERT_NE(nullptr, ax_canvas_2);
  ASSERT_EQ(ax::mojom::Role::kCanvas, ax_canvas_2->RoleValue());
  const AXObject* ax_button = GetAXObjectByElementId("button");
  ASSERT_NE(nullptr, ax_button);
  ASSERT_EQ(ax::mojom::Role::kButton, ax_button->RoleValue());

  // The first child of "canvas1" is a text object. Creating a "before children"
  // position in this canvas should return the equivalent text position anchored
  // to before the first character of the text object.
  const auto ax_position_1 =
      AXPosition::CreateFirstPositionInObject(*ax_canvas_1);
  EXPECT_TRUE(ax_position_1.IsTextPosition());
  EXPECT_EQ(ax_text, ax_position_1.ContainerObject());
  EXPECT_EQ(0, ax_position_1.TextOffset());

  const auto position_1 = ax_position_1.ToPositionWithAffinity();
  EXPECT_EQ(text, position_1.AnchorNode());
  EXPECT_TRUE(position_1.GetPosition().IsOffsetInAnchor());
  EXPECT_EQ(0, position_1.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom_1 = AXPosition::FromPosition(position_1);
  EXPECT_EQ(ax_position_1, ax_position_from_dom_1);

  const auto ax_position_2 = AXPosition::CreatePositionBeforeObject(*ax_text);
  EXPECT_TRUE(ax_position_2.IsTextPosition());
  EXPECT_EQ(ax_text, ax_position_2.ContainerObject());
  EXPECT_EQ(0, ax_position_2.TextOffset());

  const auto position_2 = ax_position_2.ToPositionWithAffinity();
  EXPECT_EQ(text, position_2.AnchorNode());
  EXPECT_EQ(0, position_2.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom_2 = AXPosition::FromPosition(position_2);
  EXPECT_EQ(ax_position_2, ax_position_from_dom_2);

  const auto ax_position_3 =
      AXPosition::CreateLastPositionInObject(*ax_canvas_2);
  EXPECT_FALSE(ax_position_3.IsTextPosition());
  EXPECT_EQ(ax_canvas_2, ax_position_3.ContainerObject());
  EXPECT_EQ(1, ax_position_3.ChildIndex());
  EXPECT_EQ(nullptr, ax_position_3.ChildAfterTreePosition());

  const auto position_3 = ax_position_3.ToPositionWithAffinity();
  EXPECT_EQ(canvas_2, position_3.AnchorNode());
  // There is a line break between the start of the canvas and the button.
  EXPECT_EQ(2, position_3.GetPosition().ComputeOffsetInContainerNode());

  const auto ax_position_from_dom_3 = AXPosition::FromPosition(position_3);
  EXPECT_EQ(ax_position_3, ax_position_from_dom_3);

  const auto ax_position_4 = AXPosition::CreatePositionBeforeObject(*ax_button);
  EXPECT_FALSE(ax_position_4.IsTextPosition());
  EXPECT_EQ(ax_canvas_2, ax_position_4.ContainerObject());
  EXPECT_EQ(0, ax_position_4.ChildIndex());
  EXPECT_EQ(ax_button, ax_position_4.ChildAfterTreePosition());

  const auto position_4 = ax_position_4.ToPositionWithAffinity();
  EXPECT_EQ(canvas_2, position_4.AnchorNode());
  // There is a line break between the start of the canvas and the button.
  EXPECT_EQ(1, position_4.GetPosition().ComputeOffsetInContainerNode());
  EXPECT_EQ(button, position_4.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_from_dom_4 = AXPosition::FromPosition(position_4);
  EXPECT_EQ(ax_position_4, ax_position_from_dom_4);
}

//
// Some layout objects, e.g. list bullets and CSS::before/after content, appear
// in the accessibility tree but are not present in the DOM.
//

TEST_F(AccessibilityTest, PositionBeforeListMarker) {
  SetBodyInnerHTML(R"HTML(
      <ul id="list">
        <li id="listItem">Item.</li>
      </ul>
      )HTML");

  const Node* list = GetElementById("list");
  ASSERT_NE(nullptr, list);
  const Node* item = GetElementById("listItem");
  ASSERT_NE(nullptr, item);
  const Node* text = item->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());

  const AXObject* ax_item = GetAXObjectByElementId("listItem");
  ASSERT_NE(nullptr, ax_item);
  ASSERT_EQ(ax::mojom::Role::kListItem, ax_item->RoleValue());
  ASSERT_EQ(2, ax_item->ChildCountIncludingIgnored());
  const AXObject* ax_marker = ax_item->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_marker);
  ASSERT_EQ(ax::mojom::Role::kListMarker, ax_marker->RoleValue());

  //
  // Test adjusting invalid DOM positions to the left.
  //

  const auto ax_position_1 = AXPosition::CreateFirstPositionInObject(*ax_item);
  EXPECT_EQ(ax_item, ax_position_1.ContainerObject());
  EXPECT_FALSE(ax_position_1.IsTextPosition());
  EXPECT_EQ(0, ax_position_1.ChildIndex());
  EXPECT_EQ(ax_marker, ax_position_1.ChildAfterTreePosition());

  const auto position_1 = ax_position_1.ToPositionWithAffinity(
      AXPositionAdjustmentBehavior::kMoveLeft);
  EXPECT_EQ(list, position_1.AnchorNode());
  // There is a line break between the start of the list and the first item.
  EXPECT_EQ(1, position_1.GetPosition().OffsetInContainerNode());
  EXPECT_EQ(item, position_1.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_from_dom_1 = AXPosition::FromPosition(position_1);
  EXPECT_EQ(
      ax_position_1.AsValidDOMPosition(AXPositionAdjustmentBehavior::kMoveLeft),
      ax_position_from_dom_1);
  EXPECT_EQ(ax_item, ax_position_from_dom_1.ChildAfterTreePosition());

  const auto ax_position_2 = AXPosition::CreatePositionBeforeObject(*ax_marker);
  EXPECT_EQ(ax_item, ax_position_2.ContainerObject());
  EXPECT_FALSE(ax_position_2.IsTextPosition());
  EXPECT_EQ(0, ax_position_2.ChildIndex());
  EXPECT_EQ(ax_marker, ax_position_2.ChildAfterTreePosition());

  const auto position_2 = ax_position_2.ToPositionWithAffinity(
      AXPositionAdjustmentBehavior::kMoveLeft);
  EXPECT_EQ(list, position_2.AnchorNode());
  // There is a line break between the start of the list and the first item.
  EXPECT_EQ(1, position_2.GetPosition().OffsetInContainerNode());
  EXPECT_EQ(item, position_2.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_from_dom_2 = AXPosition::FromPosition(position_2);
  EXPECT_EQ(
      ax_position_2.AsValidDOMPosition(AXPositionAdjustmentBehavior::kMoveLeft),
      ax_position_from_dom_2);
  EXPECT_EQ(ax_item, ax_position_from_dom_2.ChildAfterTreePosition());

  //
  // Test adjusting the same invalid positions to the right.
  //

  const auto position_3 = ax_position_1.ToPositionWithAffinity(
      AXPositionAdjustmentBehavior::kMoveRight);
  EXPECT_EQ(text, position_3.AnchorNode());
  EXPECT_TRUE(position_3.GetPosition().IsOffsetInAnchor());
  EXPECT_EQ(0, position_3.GetPosition().OffsetInContainerNode());

  const auto position_4 = ax_position_2.ToPositionWithAffinity(
      AXPositionAdjustmentBehavior::kMoveRight);
  EXPECT_EQ(text, position_4.AnchorNode());
  EXPECT_TRUE(position_4.GetPosition().IsOffsetInAnchor());
  EXPECT_EQ(0, position_4.GetPosition().OffsetInContainerNode());
}

TEST_F(AccessibilityTest, PositionAfterListMarker) {
  SetBodyInnerHTML(R"HTML(
      <ol>
        <li id="listItem">Item.</li>
      </ol>
      )HTML");

  const Node* item = GetElementById("listItem");
  ASSERT_NE(nullptr, item);
  const Node* text = item->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());

  const AXObject* ax_item = GetAXObjectByElementId("listItem");
  ASSERT_NE(nullptr, ax_item);
  ASSERT_EQ(ax::mojom::Role::kListItem, ax_item->RoleValue());
  ASSERT_EQ(2, ax_item->ChildCountIncludingIgnored());
  const AXObject* ax_marker = ax_item->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_marker);
  ASSERT_EQ(ax::mojom::Role::kListMarker, ax_marker->RoleValue());
  const AXObject* ax_text = ax_item->LastChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_text->RoleValue());

  const auto ax_position = AXPosition::CreatePositionAfterObject(*ax_marker);
  const auto position = ax_position.ToPositionWithAffinity();
  EXPECT_EQ(text, position.AnchorNode());
  EXPECT_TRUE(position.GetPosition().IsOffsetInAnchor());
  EXPECT_EQ(0, position.GetPosition().OffsetInContainerNode());

  const auto ax_position_from_dom = AXPosition::FromPosition(position);
  EXPECT_EQ(ax_position, ax_position_from_dom);
  EXPECT_EQ(ax_text, ax_position_from_dom.ContainerObject());
  EXPECT_TRUE(ax_position_from_dom.IsTextPosition());
  EXPECT_EQ(0, ax_position_from_dom.TextOffset());
}

// TODO(nektar) Fix test to work with ignored containers of pseudo content.
TEST_F(AccessibilityTest, DISABLED_PositionInCSSContent) {
  SetBodyInnerHTML(kCSSBeforeAndAfter);

  const Node* quote = GetElementById("quote");
  ASSERT_NE(nullptr, quote);
  // CSS text nodes are not in the DOM tree.
  const Node* text = quote->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_FALSE(text->IsPseudoElement());
  ASSERT_TRUE(text->IsTextNode());

  const AXObject* ax_quote = GetAXObjectByElementId("quote");
  ASSERT_NE(nullptr, ax_quote);
  ASSERT_TRUE(ax_quote->IsIgnored());
  const AXObject* ax_quote_parent = ax_quote->ParentObjectUnignored();
  ASSERT_NE(nullptr, ax_quote_parent);
  ASSERT_EQ(4, ax_quote_parent->UnignoredChildCount());
  const AXObject* ax_css_before = ax_quote_parent->UnignoredChildAt(0);
  ASSERT_NE(nullptr, ax_css_before);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_css_before->RoleValue());
  const AXObject* ax_text = ax_quote_parent->UnignoredChildAt(1);
  ASSERT_NE(nullptr, ax_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_text->RoleValue());
  const AXObject* ax_css_after = ax_quote_parent->UnignoredChildAt(2);
  ASSERT_NE(nullptr, ax_css_after);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_css_after->RoleValue());

  const auto ax_position_before =
      AXPosition::CreateFirstPositionInObject(*ax_css_before);
  EXPECT_TRUE(ax_position_before.IsTextPosition());
  EXPECT_EQ(0, ax_position_before.TextOffset());
  EXPECT_EQ(nullptr, ax_position_before.ChildAfterTreePosition());
  const auto position_before = ax_position_before.ToPositionWithAffinity(
      AXPositionAdjustmentBehavior::kMoveRight);
  EXPECT_EQ(text, position_before.AnchorNode());
  EXPECT_EQ(0, position_before.GetPosition().OffsetInContainerNode());

  const auto ax_position_after =
      AXPosition::CreateLastPositionInObject(*ax_css_after);
  EXPECT_TRUE(ax_position_after.IsTextPosition());
  EXPECT_EQ(2, ax_position_after.TextOffset());
  EXPECT_EQ(nullptr, ax_position_after.ChildAfterTreePosition());
  const auto position_after = ax_position_after.ToPositionWithAffinity(
      AXPositionAdjustmentBehavior::kMoveLeft);
  EXPECT_EQ(text, position_after.AnchorNode());
  EXPECT_EQ(12, position_after.GetPosition().OffsetInContainerNode());
}

// TODO(nektar) Fix test to work with ignored containers of pseudo content.
TEST_F(AccessibilityTest, DISABLED_PositionInCSSImageContent) {
  constexpr char css_content_no_text[] = R"HTML(
   <style>
   .heading::before {
    content: url(data:image/gif;base64,);
   }
   </style>
   <h1 id="heading" class="heading">Heading</h1>)HTML";
  SetBodyInnerHTML(css_content_no_text);

  const Node* heading = GetElementById("heading");
  ASSERT_NE(nullptr, heading);

  const AXObject* ax_heading = GetAXObjectByElementId("heading");
  ASSERT_NE(nullptr, ax_heading);
  ASSERT_EQ(ax::mojom::Role::kHeading, ax_heading->RoleValue());
  ASSERT_EQ(2, ax_heading->ChildCountIncludingIgnored());

  const AXObject* ax_css_before = ax_heading->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_css_before);
  ASSERT_EQ(ax::mojom::Role::kImage, ax_css_before->RoleValue());

  const auto ax_position_before =
      AXPosition::CreateFirstPositionInObject(*ax_css_before);
  const auto position = ax_position_before.ToPositionWithAffinity(
      AXPositionAdjustmentBehavior::kMoveLeft);
  EXPECT_EQ(GetDocument().body(), position.AnchorNode());
  EXPECT_EQ(3, position.GetPosition().OffsetInContainerNode());
}

// TODO(nektar) Fix test to work with ignored containers of pseudo content.
TEST_F(AccessibilityTest, DISABLED_PositionInTableWithCSSContent) {
  SetBodyInnerHTML(kHTMLTable);

  // Add some CSS content, i.e. a plus symbol before and a colon after each
  // table header cell.
  Element* const style_element =
      GetDocument().CreateRawElement(html_names::kStyleTag);
  ASSERT_NE(nullptr, style_element);
  style_element->setTextContent(R"STYLE(
      th::before {
        content: "+";
      }
      th::after {
        content: ":";
      }
      )STYLE");
  GetDocument().body()->insertBefore(style_element,
                                     GetDocument().body()->firstChild());
  UpdateAllLifecyclePhasesForTest();

  const Node* first_header_cell = GetElementById("firstHeaderCell");
  ASSERT_NE(nullptr, first_header_cell);
  const Node* last_header_cell = GetElementById("lastHeaderCell");
  ASSERT_NE(nullptr, last_header_cell);

  // CSS text nodes are not in the DOM tree.
  const Node* first_header_cell_text = first_header_cell->firstChild();
  ASSERT_NE(nullptr, first_header_cell_text);
  ASSERT_FALSE(first_header_cell_text->IsPseudoElement());
  ASSERT_TRUE(first_header_cell_text->IsTextNode());
  const Node* last_header_cell_text = last_header_cell->firstChild();
  ASSERT_NE(nullptr, last_header_cell_text);
  ASSERT_FALSE(last_header_cell_text->IsPseudoElement());
  ASSERT_TRUE(last_header_cell_text->IsTextNode());

  const AXObject* ax_first_header_cell =
      GetAXObjectByElementId("firstHeaderCell");
  ASSERT_NE(nullptr, ax_first_header_cell);
  ASSERT_EQ(ax::mojom::Role::kColumnHeader, ax_first_header_cell->RoleValue());
  const AXObject* ax_last_header_cell =
      GetAXObjectByElementId("lastHeaderCell");
  ASSERT_NE(nullptr, ax_last_header_cell);
  ASSERT_EQ(ax::mojom::Role::kColumnHeader, ax_last_header_cell->RoleValue());

  ASSERT_EQ(3, ax_first_header_cell->ChildCountIncludingIgnored());
  // Get grandchild text, not the child ignored generic container.
  AXObject* const ax_first_cell_css_before =
      ax_first_header_cell->FirstChildIncludingIgnored()
          ->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_first_cell_css_before);
  ASSERT_EQ(ax::mojom::Role::kStaticText,
            ax_first_cell_css_before->RoleValue());

  ASSERT_EQ(3, ax_last_header_cell->ChildCountIncludingIgnored());
  // Get grandchild text, not the child ignored generic container.
  AXObject* const ax_last_cell_css_after =
      ax_last_header_cell->FirstChildIncludingIgnored()
          ->LastChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_last_cell_css_after);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_last_cell_css_after->RoleValue());

  // The first position inside the first header cell should be before the plus
  // symbol inside the CSS content. It should be valid in the accessibility tree
  // but not valid in the DOM tree.
  auto ax_position_before =
      AXPosition::CreateFirstPositionInObject(*ax_first_header_cell);
  EXPECT_TRUE(ax_position_before.IsTextPosition());
  EXPECT_EQ(0, ax_position_before.TextOffset());
  auto position_before = ax_position_before.ToPositionWithAffinity(
      AXPositionAdjustmentBehavior::kMoveRight);
  EXPECT_EQ(first_header_cell_text, position_before.AnchorNode());
  EXPECT_EQ(0, position_before.GetPosition().OffsetInContainerNode());

  // Same situation as above, but explicitly create a text position inside the
  // CSS content, instead of having it implicitly created by
  // CreateFirstPositionInObject.
  ax_position_before =
      AXPosition::CreateFirstPositionInObject(*ax_first_cell_css_before);
  EXPECT_TRUE(ax_position_before.IsTextPosition());
  EXPECT_EQ(0, ax_position_before.TextOffset());
  position_before = ax_position_before.ToPositionWithAffinity(
      AXPositionAdjustmentBehavior::kMoveRight);
  EXPECT_EQ(first_header_cell_text, position_before.AnchorNode());
  EXPECT_EQ(0, position_before.GetPosition().OffsetInContainerNode());

  // Same situation as above, but now create a text position inside the inline
  // text box representing the CSS content after the last header cell.
  ax_first_cell_css_before->LoadInlineTextBoxes();
  ASSERT_NE(nullptr, ax_first_cell_css_before->FirstChildIncludingIgnored());
  ax_position_before = AXPosition::CreateFirstPositionInObject(
      *ax_first_cell_css_before->FirstChildIncludingIgnored());
  EXPECT_TRUE(ax_position_before.IsTextPosition());
  EXPECT_EQ(0, ax_position_before.TextOffset());
  position_before = ax_position_before.ToPositionWithAffinity(
      AXPositionAdjustmentBehavior::kMoveRight);
  EXPECT_EQ(first_header_cell_text, position_before.AnchorNode());
  EXPECT_EQ(0, position_before.GetPosition().OffsetInContainerNode());

  // An "after children" position inside the last header cell should be after
  // the CSS content that displays a colon. It should be valid in the
  // accessibility tree but not valid in the DOM tree.
  auto ax_position_after =
      AXPosition::CreateLastPositionInObject(*ax_last_header_cell);
  EXPECT_FALSE(ax_position_after.IsTextPosition());
  EXPECT_EQ(3, ax_position_after.ChildIndex());
  auto position_after = ax_position_after.ToPositionWithAffinity(
      AXPositionAdjustmentBehavior::kMoveLeft);
  EXPECT_EQ(last_header_cell_text, position_after.AnchorNode());
  EXPECT_EQ(8, position_after.GetPosition().OffsetInContainerNode());

  // Similar to the last case, but explicitly create a text position inside the
  // CSS content after the last header cell.
  ax_position_after =
      AXPosition::CreateLastPositionInObject(*ax_last_cell_css_after);
  EXPECT_TRUE(ax_position_after.IsTextPosition());
  EXPECT_EQ(1, ax_position_after.TextOffset());
  position_after = ax_position_after.ToPositionWithAffinity(
      AXPositionAdjustmentBehavior::kMoveLeft);
  EXPECT_EQ(last_header_cell_text, position_after.AnchorNode());
  EXPECT_EQ(8, position_after.GetPosition().OffsetInContainerNode());

  // Same situation as above, but now create a text position inside the inline
  // text box representing the CSS content after the last header cell.
  ax_last_cell_css_after->LoadInlineTextBoxes();
  ASSERT_NE(nullptr, ax_last_cell_css_after->FirstChildIncludingIgnored());
  ax_position_after = AXPosition::CreateLastPositionInObject(
      *ax_last_cell_css_after->FirstChildIncludingIgnored());
  EXPECT_TRUE(ax_position_after.IsTextPosition());
  EXPECT_EQ(1, ax_position_after.TextOffset());
  position_after = ax_position_after.ToPositionWithAffinity(
      AXPositionAdjustmentBehavior::kMoveLeft);
  EXPECT_EQ(last_header_cell_text, position_after.AnchorNode());
  EXPECT_EQ(8, position_after.GetPosition().OffsetInContainerNode());
}

//
// Objects deriving from |AXMockObject|, e.g. table columns, are in the
// accessibility tree but are neither in the DOM or layout trees.
// Same for virtual nodes created using the Accessibility Object Model (AOM).
//

TEST_F(AccessibilityTest, PositionBeforeAndAfterTable) {
  SetBodyInnerHTML(kHTMLTable);
  const Node* after = GetElementById("after");
  ASSERT_NE(nullptr, after);
  const AXObject* ax_table = GetAXObjectByElementId("table");
  ASSERT_NE(nullptr, ax_table);
  ASSERT_EQ(ax::mojom::Role::kTable, ax_table->RoleValue());
  const AXObject* ax_after = GetAXObjectByElementId("after");
  ASSERT_NE(nullptr, ax_after);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_after->RoleValue());

  const auto ax_position_before =
      AXPosition::CreatePositionBeforeObject(*ax_table);
  const auto position_before = ax_position_before.ToPositionWithAffinity();
  EXPECT_EQ(GetDocument().body(), position_before.AnchorNode());
  EXPECT_EQ(3, position_before.GetPosition().OffsetInContainerNode());
  const Node* table = position_before.GetPosition().ComputeNodeAfterPosition();
  ASSERT_NE(nullptr, table);
  EXPECT_EQ(GetElementById("table"), table);

  const auto ax_position_before_from_dom =
      AXPosition::FromPosition(position_before);
  EXPECT_EQ(ax_position_before, ax_position_before_from_dom);

  const auto ax_position_after =
      AXPosition::CreatePositionAfterObject(*ax_table);
  const auto position_after = ax_position_after.ToPositionWithAffinity();
  EXPECT_EQ(GetDocument().body(), position_after.AnchorNode());
  EXPECT_EQ(5, position_after.GetPosition().OffsetInContainerNode());
  const Node* node_after =
      position_after.GetPosition().ComputeNodeAfterPosition();
  EXPECT_EQ(after, node_after);

  const auto ax_position_after_from_dom =
      AXPosition::FromPosition(position_after);
  EXPECT_EQ(ax_position_after, ax_position_after_from_dom);
  EXPECT_EQ(ax_after, ax_position_after_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, PositionAtStartAndEndOfTable) {
  SetBodyInnerHTML(kHTMLTable);

  // In the accessibility tree, the thead and tbody elements are accessibility
  // ignored but included in the AXTree.
  // Calling CreateFirstPositionInObject and CreateLastPositionInObject with the
  // |table| element will create a position anchored to |table| which points to
  // the |thead| element and the last whitespace text node within the table
  // respectively.
  const Node* table = GetElementById("table");
  ASSERT_NE(nullptr, table);
  const Node* thead = GetElementById("thead");
  ASSERT_NE(nullptr, thead);
  const Node* header_row = GetElementById("headerRow");
  ASSERT_NE(nullptr, header_row);
  const Node* tbody = GetElementById("tbody");
  ASSERT_NE(nullptr, tbody);

  const AXObject* ax_table = GetAXObjectByElementId("table");
  ASSERT_NE(nullptr, ax_table);
  ASSERT_EQ(ax::mojom::Role::kTable, ax_table->RoleValue());
  const AXObject* ax_header_row = GetAXObjectByElementId("headerRow");
  ASSERT_NE(nullptr, ax_header_row);
  ASSERT_EQ(ax::mojom::Role::kRow, ax_header_row->RoleValue());

  const AXObject* ax_thead = GetAXObjectByElementId("thead");
  const auto ax_position_at_start =
      AXPosition::CreateFirstPositionInObject(*ax_table);
  const auto position_at_start = ax_position_at_start.ToPositionWithAffinity();
  EXPECT_EQ(table, position_at_start.AnchorNode());
  EXPECT_EQ(1, position_at_start.GetPosition().OffsetInContainerNode());
  EXPECT_EQ(thead, position_at_start.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_at_start_from_dom =
      AXPosition::FromPosition(position_at_start);
  EXPECT_EQ(ax_position_at_start, ax_position_at_start_from_dom);
  EXPECT_EQ(ax_thead, ax_position_at_start_from_dom.ChildAfterTreePosition());

  const auto ax_position_at_end =
      AXPosition::CreateLastPositionInObject(*ax_table);
  const auto position_at_end = ax_position_at_end.ToPositionWithAffinity();
  EXPECT_EQ(table, position_at_end.AnchorNode());
  // There are three rows and a line break before and after each one.
  EXPECT_EQ(4, position_at_end.GetPosition().OffsetInContainerNode());

  const auto ax_position_at_end_from_dom =
      AXPosition::FromPosition(position_at_end);
  EXPECT_EQ(ax_position_at_end, ax_position_at_end_from_dom);
  EXPECT_EQ(nullptr, ax_position_at_end_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, PositionInTableHeader) {
  SetBodyInnerHTML(kHTMLTable);

  const Node* header_row = GetElementById("headerRow");
  ASSERT_NE(nullptr, header_row);
  const Node* first_header_cell = GetElementById("firstHeaderCell");
  ASSERT_NE(nullptr, first_header_cell);

  const AXObject* ax_first_header_cell =
      GetAXObjectByElementId("firstHeaderCell");
  ASSERT_NE(nullptr, ax_first_header_cell);
  ASSERT_EQ(ax::mojom::Role::kColumnHeader, ax_first_header_cell->RoleValue());
  const AXObject* ax_last_header_cell =
      GetAXObjectByElementId("lastHeaderCell");
  ASSERT_NE(nullptr, ax_last_header_cell);
  ASSERT_EQ(ax::mojom::Role::kColumnHeader, ax_last_header_cell->RoleValue());

  const auto ax_position_before =
      AXPosition::CreatePositionBeforeObject(*ax_first_header_cell);
  const auto position_before = ax_position_before.ToPositionWithAffinity();
  EXPECT_EQ(header_row, position_before.AnchorNode());
  EXPECT_EQ(1, position_before.GetPosition().OffsetInContainerNode());
  EXPECT_EQ(first_header_cell,
            position_before.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_before_from_dom =
      AXPosition::FromPosition(position_before);
  EXPECT_EQ(ax_position_before, ax_position_before_from_dom);
  EXPECT_EQ(ax_first_header_cell,
            ax_position_before_from_dom.ChildAfterTreePosition());

  const auto ax_position_after =
      AXPosition::CreatePositionAfterObject(*ax_last_header_cell);
  const auto position_after = ax_position_after.ToPositionWithAffinity();
  EXPECT_EQ(header_row, position_after.AnchorNode());
  // There are three header cells and a line break before and after each one.
  EXPECT_EQ(6, position_after.GetPosition().OffsetInContainerNode());

  const auto ax_position_after_from_dom =
      AXPosition::FromPosition(position_after);
  EXPECT_EQ(ax_position_after, ax_position_after_from_dom);
  EXPECT_EQ(nullptr, ax_position_after_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, PositionInTableRow) {
  SetBodyInnerHTML(kHTMLTable);

  const Node* first_row = GetElementById("firstRow");
  ASSERT_NE(nullptr, first_row);
  const Node* first_cell = GetElementById("firstCell");
  ASSERT_NE(nullptr, first_cell);
  const Node* last_row = GetElementById("lastRow");
  ASSERT_NE(nullptr, last_row);

  const AXObject* ax_first_cell = GetAXObjectByElementId("firstCell");
  ASSERT_NE(nullptr, ax_first_cell);
  ASSERT_EQ(ax::mojom::Role::kRowHeader, ax_first_cell->RoleValue());
  const AXObject* ax_last_cell = GetAXObjectByElementId("lastCell");
  ASSERT_NE(nullptr, ax_last_cell);
  ASSERT_EQ(ax::mojom::Role::kCell, ax_last_cell->RoleValue());

  const auto ax_position_before =
      AXPosition::CreatePositionBeforeObject(*ax_first_cell);
  const auto position_before = ax_position_before.ToPositionWithAffinity();
  EXPECT_EQ(first_row, position_before.AnchorNode());
  EXPECT_EQ(1, position_before.GetPosition().OffsetInContainerNode());
  EXPECT_EQ(first_cell,
            position_before.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_before_from_dom =
      AXPosition::FromPosition(position_before);
  EXPECT_EQ(ax_position_before, ax_position_before_from_dom);
  EXPECT_EQ(ax_first_cell,
            ax_position_before_from_dom.ChildAfterTreePosition());

  const auto ax_position_after =
      AXPosition::CreatePositionAfterObject(*ax_last_cell);
  const auto position_after = ax_position_after.ToPositionWithAffinity();
  EXPECT_EQ(last_row, position_after.AnchorNode());
  // There are three cells on the last row and a line break before and after
  // each one.
  EXPECT_EQ(6, position_after.GetPosition().OffsetInContainerNode());

  const auto ax_position_after_from_dom =
      AXPosition::FromPosition(position_after);
  EXPECT_EQ(ax_position_after, ax_position_after_from_dom);
  EXPECT_EQ(nullptr, ax_position_after_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, DISABLED_PositionInVirtualAOMNode) {
  ScopedAccessibilityObjectModelForTest(true);
  SetBodyInnerHTML(kAOM);

  const Node* parent = GetElementById("aomParent");
  ASSERT_NE(nullptr, parent);
  const Node* after = GetElementById("after");
  ASSERT_NE(nullptr, after);

  const AXObject* ax_parent = GetAXObjectByElementId("aomParent");
  ASSERT_NE(nullptr, ax_parent);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, ax_parent->RoleValue());
  ASSERT_EQ(1, ax_parent->ChildCountIncludingIgnored());
  const AXObject* ax_button = ax_parent->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_button);
  ASSERT_EQ(ax::mojom::Role::kButton, ax_button->RoleValue());
  const AXObject* ax_after = GetAXObjectByElementId("after");
  ASSERT_NE(nullptr, ax_after);
  ASSERT_EQ(ax::mojom::Role::kParagraph, ax_after->RoleValue());

  const auto ax_position_before =
      AXPosition::CreatePositionBeforeObject(*ax_button);
  const auto position_before = ax_position_before.ToPositionWithAffinity();
  EXPECT_EQ(parent, position_before.AnchorNode());
  EXPECT_TRUE(position_before.GetPosition().IsBeforeChildren());
  EXPECT_EQ(nullptr, position_before.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_before_from_dom =
      AXPosition::FromPosition(position_before);
  EXPECT_EQ(ax_position_before, ax_position_before_from_dom);
  EXPECT_EQ(ax_button, ax_position_before_from_dom.ChildAfterTreePosition());

  const auto ax_position_after =
      AXPosition::CreatePositionAfterObject(*ax_button);
  const auto position_after = ax_position_after.ToPositionWithAffinity();
  EXPECT_EQ(after, position_after.AnchorNode());
  EXPECT_TRUE(position_after.GetPosition().IsBeforeChildren());
  EXPECT_EQ(nullptr, position_after.GetPosition().ComputeNodeAfterPosition());

  const auto ax_position_after_from_dom =
      AXPosition::FromPosition(position_after);
  EXPECT_EQ(ax_position_after, ax_position_after_from_dom);
  EXPECT_EQ(ax_after, ax_position_after_from_dom.ChildAfterTreePosition());
}

TEST_F(AccessibilityTest, PositionInInvalidMapLayout) {
  SetBodyInnerHTML(kMap);

  Node* br = GetElementById("br");
  ASSERT_NE(nullptr, br);
  Node* map = GetElementById("map");
  ASSERT_NE(nullptr, map);

  const AXObject* ax_map = GetAXObjectByElementId("map");
  ASSERT_EQ(nullptr, ax_map);  // No AXObject is created for a <map>.

  // Create an invalid layout by appending a child to the <br>
  br->appendChild(map);
  GetAXObjectCache().UpdateAXForAllDocuments();

  ax_map = GetAXObjectByElementId("map");
  ASSERT_EQ(nullptr, ax_map);

  const AXObject* ax_br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, ax_br);

  const auto ax_position_before =
      AXPosition::CreateFirstPositionInObject(*ax_br);
  const auto position_before = ax_position_before.ToPositionWithAffinity();
  EXPECT_EQ(br, position_before.AnchorNode());
  EXPECT_EQ(0, position_before.GetPosition().OffsetInContainerNode());

  const auto ax_position_after = AXPosition::CreateLastPositionInObject(*ax_br);
  const auto position_after = ax_position_after.ToPositionWithAffinity();
  EXPECT_EQ(br, position_after.AnchorNode());
  EXPECT_EQ(0, position_after.GetPosition().OffsetInContainerNode());
}

TEST_F(AccessibilityTest, ToPositionWithAffinityWithMultipleInlineTextBoxes) {
  // This test expects the starting offset of the last InlineTextBox object to
  // equate the sum of the previous inline text boxes' length, without the
  // collapsed white-spaces.
  //
  // "&#10" is a Line Feed ("\n").
  SetBodyInnerHTML(
      R"HTML(<style>p { white-space: pre-line; }</style>
      <p id="paragraph">Hello &#10; world</p>)HTML");

  const Node* text = GetElementById("paragraph")->firstChild();
  ASSERT_NE(nullptr, text);
  ASSERT_TRUE(text->IsTextNode());
  AXObject* ax_static_text =
      GetAXObjectByElementId("paragraph")->FirstChildIncludingIgnored();

  ASSERT_NE(nullptr, ax_static_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_static_text->RoleValue());

  ax_static_text->LoadInlineTextBoxes();
  ASSERT_EQ(3, ax_static_text->UnignoredChildCount());

  // The last inline text box should be:
  // "InlineTextBox" name="world"
  const AXObject* ax_last_inline_box =
      ax_static_text->LastChildIncludingIgnored();
  const auto ax_position =
      AXPosition::CreatePositionBeforeObject(*ax_last_inline_box);
  const auto position = ax_position.ToPositionWithAffinity();
  // The resulting DOM position should be:
  // DOM position #text "Hello \n world"@offsetInAnchor[8]
  ASSERT_TRUE(position.GetPosition().IsOffsetInAnchor());
  EXPECT_EQ(8, position.GetPosition().OffsetInContainerNode());
}

}  // namespace test
}  // namespace blink
