// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/layout/list/layout_list_item.h"
#include "third_party/blink/renderer/modules/accessibility/ax_node_object.h"
#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_test.h"

namespace blink {

class AXLayoutObjectTest : public AccessibilityTest {
 protected:
  static LayoutObject* GetListMarker(const LayoutObject& list_item) {
    if (list_item.IsLayoutListItem()) {
      return To<LayoutListItem>(list_item).Marker();
    }
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
};

TEST_F(AXLayoutObjectTest, IsNotEditableInsideListmarker) {
  SetBodyInnerHTML("<div contenteditable><li id=t>ab");
  // The layout tree is:
  //    LayoutBlockFlow {DIV} at (0,0) size 784x20
  //      LayoutListItem {LI} at (0,0) size 784x20
  //        LayoutInsideListMarker {::marker} at (-1,0) size 7x19
  //          LayoutText (anonymous) at (-1,0) size 7x19
  //            text run at (-1,0) width 7: "\x{2022} "
  //        LayoutText {#text} at (22,0) size 15x19
  //          text run at (22,0) width 15: "ab"
  LayoutObject& list_item = *GetElementById("t")->GetLayoutObject();
  LayoutObject& list_marker = *GetListMarker(list_item);

  const AXObject* ax_list_item = GetAXObject(&list_item);
  ASSERT_NE(nullptr, ax_list_item);
  EXPECT_TRUE(ax_list_item->GetLayoutObject() != nullptr);
  EXPECT_TRUE(ax_list_item->IsEditable());
  EXPECT_TRUE(ax_list_item->IsRichlyEditable());

  const AXObject* ax_list_marker = GetAXObject(&list_marker);
  ASSERT_NE(nullptr, ax_list_marker);
  EXPECT_TRUE(ax_list_item->GetLayoutObject() != nullptr);
  EXPECT_FALSE(ax_list_marker->IsEditable());
  EXPECT_FALSE(ax_list_marker->IsRichlyEditable());
}

TEST_F(AXLayoutObjectTest, IsNotEditableOutsideListmarker) {
  SetBodyInnerHTML("<ol contenteditable><li id=t>ab");
  // THe layout tree is:
  //    LayoutBlockFlow {OL} at (0,0) size 784x20
  //      LayoutListItem {LI} at (40,0) size 744x20
  //        LayoutOutsideListMarker {::marker} at (-16,0) size 16x20
  //          LayoutText (anonymous) at (0,0) size 16x19
  //            text run at (0,0) width 16: "1. "
  //        LayoutText {#text} at (0,0) size 15x19
  //          text run at (0,0) width 15: "ab"
  LayoutObject& list_item = *GetElementById("t")->GetLayoutObject();
  LayoutObject& list_marker = *GetListMarker(list_item);

  const AXObject* ax_list_item = GetAXObject(&list_item);
  ASSERT_NE(nullptr, ax_list_item);
  EXPECT_TRUE(ax_list_item->GetLayoutObject() != nullptr);
  EXPECT_TRUE(ax_list_item->IsEditable());
  EXPECT_TRUE(ax_list_item->IsRichlyEditable());

  const AXObject* ax_list_marker = GetAXObject(&list_marker);
  ASSERT_NE(nullptr, ax_list_marker);
  EXPECT_TRUE(ax_list_marker->GetLayoutObject() != nullptr);
  EXPECT_FALSE(ax_list_marker->IsEditable());
  EXPECT_FALSE(ax_list_marker->IsRichlyEditable());
}

TEST_F(AXLayoutObjectTest, GetValueForControlWithTextTransform) {
  SetBodyInnerHTML(
      "<select id='t' style='text-transform:uppercase'>"
      "<option>abc</select>");
  const AXObject* ax_select = GetAXObjectByElementId("t");
  ASSERT_NE(nullptr, ax_select);
  EXPECT_TRUE(ax_select->GetLayoutObject() != nullptr);
  EXPECT_EQ("ABC", ax_select->GetValueForControl());
}

TEST_F(AXLayoutObjectTest, GetValueForControlWithTextSecurity) {
  SetBodyInnerHTML(
      "<select id='t' style='-webkit-text-security:disc'>"
      "<option>abc</select>");
  const AXObject* ax_select = GetAXObjectByElementId("t");
  ASSERT_NE(nullptr, ax_select);
  EXPECT_TRUE(ax_select->GetLayoutObject() != nullptr);
  // U+2022 -> \xE2\x80\xA2 in UTF-8
  EXPECT_EQ("\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2",
            ax_select->GetValueForControl().Utf8());
}

// Test AX hit test for user-agent shadow DOM, which should ignore the shadow
// Node at the given point, and select the host Element instead.
TEST_F(AXLayoutObjectTest, AccessibilityHitTest) {
  SetBodyInnerHTML(
      "<style>"
      "  .A{display:flex;flex:100%;margin-top:-37px;height:34px}"
      "  .B{display:flex;flex:1;flex-wrap:wrap}"
      "  .C{flex:100%;height:34px}"
      "</style>"
      "<div class='B'>"
      "<div class='C'></div>"
      "<input class='A' aria-label='Search' role='combobox'>"
      "</div>");
  const AXObject* ax_root = GetAXRootObject();
  ASSERT_NE(nullptr, ax_root);
  // (8, 5) initially hits the editable DIV inside <input>.
  const gfx::Point position(8, 5);
  AXObject* hit_test_result = ax_root->AccessibilityHitTest(position);
  EXPECT_NE(nullptr, hit_test_result);
  EXPECT_EQ(hit_test_result->RoleValue(),
            ax::mojom::Role::kTextFieldWithComboBox);
}

// Tests AX hit test for open / closed shadow DOM, which should select the
// shadow Node under the given point (as opposed to taking the host Element,
// which is the case for user-agent shadow DOM).
TEST_F(AXLayoutObjectTest, AccessibilityHitTestShadowDOM) {
  auto run_test = [&](ShadowRootMode root_type) {
    SetBodyInnerHTML(
        "<style>"
        "#host_a{position:absolute;}"
        "</style>"
        "<div id='host_a'>"
        "</div>");
    auto* host_a = GetElementById("host_a");
    auto& shadow_a = host_a->AttachShadowRootForTesting(root_type);
    shadow_a.setInnerHTML(
        "<style>"
        "label {"
        "  display: inline-block;"
        "  height: 100px;"
        "  width: 100px;"
        "}"
        "input {"
        "  appearance: none;"
        "  height: 0;"
        "  width: 0;"
        "}"
        "</style>"
        "<label id='label1' role='radio'>"
        "  <input type='radio' name='radio-main'>"
        "</label>"
        "<label id='label2' role='radio'>"
        "  <input type='radio' name='radio-main'>"
        "</label>"
        "<label id='label3' role='radio'>"
        "  <input type='radio' name='radio-main'>"
        "</label>",
        ASSERT_NO_EXCEPTION);
    const AXObject* ax_root = GetAXRootObject();
    ASSERT_NE(nullptr, ax_root);
    // (50, 50) initially hits #label1.
    AXObject* hit_test_result = ax_root->AccessibilityHitTest({50, 50});
    EXPECT_EQ(hit_test_result->RoleValue(), ax::mojom::Role::kRadioButton);
  };

  run_test(ShadowRootMode::kOpen);
  run_test(ShadowRootMode::kClosed);
}

// https://crbug.com/1167596
TEST_F(AXLayoutObjectTest, GetListStyleDecimalLeadingZeroAsCustomCounterStyle) {
  using ListStyle = ax::mojom::blink::ListStyle;

  SetBodyInnerHTML(R"HTML(
  <ul>
    <li id="target" style="list-style-type: decimal-leading-zero"></li>
  </ul>
  )HTML");

  EXPECT_EQ(ListStyle::kNumeric,
            GetAXObjectByElementId("target")->GetListStyle());
}
// https://crbug.com/1167596
TEST_F(AXLayoutObjectTest, GetListStyleOverriddenDecimalLeadingZero) {
  using ListStyle = ax::mojom::blink::ListStyle;

  SetBodyInnerHTML(R"HTML(
  <style>
  @counter-style decimal-leading-zero { system: extends upper-roman; }
  </style>
  <ul>
    <li id="target" style="list-style-type: decimal-leading-zero"></li>
  </ul>
  )HTML");

  ListStyle expected =
      RuntimeEnabledFeatures::CSSAtRuleCounterStyleSpeakAsDescriptorEnabled()
          ? ListStyle::kNumeric
          : ListStyle::kOther;
  EXPECT_EQ(expected, GetAXObjectByElementId("target")->GetListStyle());
}

TEST_F(AXLayoutObjectTest, GetPredefinedListStyleWithSpeakAs) {
  ScopedCSSAtRuleCounterStyleSpeakAsDescriptorForTest enabled(true);

  using ListStyle = ax::mojom::blink::ListStyle;

  SetBodyInnerHTML(R"HTML(
  <ul>
    <li id="none" style="list-style-type: none"></li>

    <li id="string" style="list-style-type: '-'"></li>

    <li id="disc" style="list-style-type: disc"></li>
    <li id="circle" style="list-style-type: circle"></li>
    <li id="square" style="list-style-type: square"></li>

    <li id="disclosure-open" style="list-style-type: disclosure-open"></li>
    <li id="disclosure-closed" style="list-style-type: disclosure-closed"></li>

    <li id="decimal" style="list-style-type: decimal"></li>
    <li id="decimal-zero" style="list-style-type: decimal-leading-zero"></li>
    <li id="roman" style="list-style-type: lower-roman"></li>
    <li id="armenian" style="list-style-type: lower-armenian"></li>
    <li id="persian" style="list-style-type: persian"></li>
    <li id="chinese" style="list-style-type: simp-chinese-formal"></li>

    <li id="alpha" style="list-style-type: lower-alpha"></li>
  </ul>
  )HTML");

  EXPECT_EQ(ListStyle::kNone, GetAXObjectByElementId("none")->GetListStyle());
  EXPECT_EQ(ListStyle::kOther,
            GetAXObjectByElementId("string")->GetListStyle());
  EXPECT_EQ(ListStyle::kDisc, GetAXObjectByElementId("disc")->GetListStyle());
  EXPECT_EQ(ListStyle::kCircle,
            GetAXObjectByElementId("circle")->GetListStyle());
  EXPECT_EQ(ListStyle::kSquare,
            GetAXObjectByElementId("square")->GetListStyle());
  EXPECT_EQ(ListStyle::kOther,
            GetAXObjectByElementId("disclosure-open")->GetListStyle());
  EXPECT_EQ(ListStyle::kOther,
            GetAXObjectByElementId("disclosure-closed")->GetListStyle());
  EXPECT_EQ(ListStyle::kNumeric,
            GetAXObjectByElementId("decimal")->GetListStyle());
  EXPECT_EQ(ListStyle::kNumeric,
            GetAXObjectByElementId("decimal-zero")->GetListStyle());
  EXPECT_EQ(ListStyle::kNumeric,
            GetAXObjectByElementId("roman")->GetListStyle());
  EXPECT_EQ(ListStyle::kNumeric,
            GetAXObjectByElementId("armenian")->GetListStyle());
  EXPECT_EQ(ListStyle::kNumeric,
            GetAXObjectByElementId("persian")->GetListStyle());
  EXPECT_EQ(ListStyle::kNumeric,
            GetAXObjectByElementId("chinese")->GetListStyle());
  EXPECT_EQ(ListStyle::kOther, GetAXObjectByElementId("alpha")->GetListStyle());
}

TEST_F(AXLayoutObjectTest, GetCustomListStyleWithSpeakAs) {
  ScopedCSSAtRuleCounterStyleSpeakAsDescriptorForTest enabled(true);

  using ListStyle = ax::mojom::blink::ListStyle;

  SetBodyInnerHTML(R"HTML(
  <style>
    @counter-style explicit-bullets {
      system: extends decimal;
      speak-as: bullets;
    }
    @counter-style explicit-numbers {
      system: extends disc;
      speak-as: numbers;
    }
    @counter-style explicit-words {
      system: extends decimal;
      speak-as: words;
    }
    @counter-style disc-reference {
      system: extends decimal;
      speak-as: disc;
    }
    @counter-style decimal-reference {
      system: extends disc;
      speak-as: decimal;
    }
    @counter-style alpha-reference {
      system: extends decimal;
      speak-as: lower-alpha;
    }
  </style>
  <ul>
    <li id="bullets" style="list-style-type: explicit-bullets"></li>
    <li id="numbers" style="list-style-type: explicit-numbers"></li>
    <li id="words" style="list-style-type: explicit-words"></li>
    <li id="disc" style="list-style-type: disc-reference"></li>
    <li id="decimal" style="list-style-type: decimal-reference"></li>
    <li id="alpha" style="list-style-type: alpha-reference"></li>
  </ul>
  )HTML");

  EXPECT_EQ(ListStyle::kDisc,
            GetAXObjectByElementId("bullets")->GetListStyle());
  EXPECT_EQ(ListStyle::kNumeric,
            GetAXObjectByElementId("numbers")->GetListStyle());
  EXPECT_EQ(ListStyle::kOther, GetAXObjectByElementId("words")->GetListStyle());
  EXPECT_EQ(ListStyle::kDisc, GetAXObjectByElementId("disc")->GetListStyle());
  EXPECT_EQ(ListStyle::kNumeric,
            GetAXObjectByElementId("decimal")->GetListStyle());
  EXPECT_EQ(ListStyle::kOther, GetAXObjectByElementId("alpha")->GetListStyle());
}

}  // namespace blink
