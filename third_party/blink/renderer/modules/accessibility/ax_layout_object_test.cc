// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_layout_object.h"

#include "third_party/blink/renderer/core/layout/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"
#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_test.h"

namespace blink {

class AXLayoutObjectTest : public AccessibilityTest {
 protected:
  static LayoutObject* GetListMarker(const LayoutObject& list_item) {
    if (list_item.IsListItem())
      return ToLayoutListItem(list_item).Marker();
    if (list_item.IsLayoutNGListItem())
      return ToLayoutNGListItem(list_item).Marker();
    NOTREACHED();
    return nullptr;
  }
};

TEST_F(AXLayoutObjectTest, IsEditableInsideListmarker) {
  SetBodyInnerHTML("<div contenteditable><li id=t>ab");
  // The layout tree is:
  //    LayoutNGBlockFlow {DIV} at (0,0) size 784x20
  //      LayoutNGListItem {LI} at (0,0) size 784x20
  //        LayoutNGInsideListMarker {::marker} at (-1,0) size 7x19
  //          LayoutText (anonymous) at (-1,0) size 7x19
  //            text run at (-1,0) width 7: "\x{2022} "
  //        LayoutText {#text} at (22,0) size 15x19
  //          text run at (22,0) width 15: "ab"
  LayoutObject& list_item = *GetElementById("t")->GetLayoutObject();
  LayoutObject& list_marker = *GetListMarker(list_item);

  const AXObject* ax_list_item = GetAXObject(&list_item);
  ASSERT_NE(nullptr, ax_list_item);
  EXPECT_TRUE(IsA<AXLayoutObject>(ax_list_item));
  EXPECT_TRUE(ax_list_item->IsEditable());
  EXPECT_TRUE(ax_list_item->IsRichlyEditable());

  const AXObject* ax_list_marker = GetAXObject(&list_marker);
  ASSERT_NE(nullptr, ax_list_marker);
  EXPECT_TRUE(IsA<AXLayoutObject>(ax_list_item));
  EXPECT_TRUE(ax_list_marker->IsEditable());
  EXPECT_TRUE(ax_list_marker->IsRichlyEditable());
}

TEST_F(AXLayoutObjectTest, IsEditableOutsideListmarker) {
  SetBodyInnerHTML("<ol contenteditable><li id=t>ab");
  // THe layout tree is:
  //    LayoutNGBlockFlow {OL} at (0,0) size 784x20
  //      LayoutNGListItem {LI} at (40,0) size 744x20
  //        LayoutNGOutsideListMarker {::marker} at (-16,0) size 16x20
  //          LayoutText (anonymous) at (0,0) size 16x19
  //            text run at (0,0) width 16: "1. "
  //        LayoutText {#text} at (0,0) size 15x19
  //          text run at (0,0) width 15: "ab"
  LayoutObject& list_item = *GetElementById("t")->GetLayoutObject();
  LayoutObject& list_marker = *GetListMarker(list_item);

  const AXObject* ax_list_item = GetAXObject(&list_item);
  ASSERT_NE(nullptr, ax_list_item);
  EXPECT_TRUE(IsA<AXLayoutObject>(ax_list_item));
  EXPECT_TRUE(ax_list_item->IsEditable());
  EXPECT_TRUE(ax_list_item->IsRichlyEditable());

  const AXObject* ax_list_marker = GetAXObject(&list_marker);
  ASSERT_NE(nullptr, ax_list_marker);
  EXPECT_TRUE(IsA<AXLayoutObject>(ax_list_item));
  EXPECT_TRUE(ax_list_marker->IsEditable());
  EXPECT_TRUE(ax_list_marker->IsRichlyEditable());
}

TEST_F(AXLayoutObjectTest, StringValueTextTransform) {
  SetBodyInnerHTML(
      "<select id='t' style='text-transform:uppercase'>"
      "<option>abc</select>");
  const AXObject* ax_select = GetAXObjectByElementId("t");
  ASSERT_NE(nullptr, ax_select);
  EXPECT_TRUE(IsA<AXLayoutObject>(ax_select));
  EXPECT_EQ("ABC", ax_select->StringValue());
}

TEST_F(AXLayoutObjectTest, StringValueTextSecurity) {
  SetBodyInnerHTML(
      "<select id='t' style='-webkit-text-security:disc'>"
      "<option>abc</select>");
  const AXObject* ax_select = GetAXObjectByElementId("t");
  ASSERT_NE(nullptr, ax_select);
  EXPECT_TRUE(IsA<AXLayoutObject>(ax_select));
  // U+2022 -> \xE2\x80\xA2 in UTF-8
  EXPECT_EQ("\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2",
            ax_select->StringValue().Utf8());
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
  const IntPoint position(8, 5);
  AXObject* hit_test_result = ax_root->AccessibilityHitTest(position);
  EXPECT_NE(nullptr, hit_test_result);
  EXPECT_EQ(hit_test_result->RoleValue(),
            ax::mojom::Role::kTextFieldWithComboBox);
}

// Tests AX hit test for open / closed shadow DOM, which should select the
// shadow Node under the given point (as opposed to taking the host Element,
// which is the case for user-agent shadow DOM).
TEST_F(AXLayoutObjectTest, AccessibilityHitTestShadowDOM) {
  auto run_test = [&](ShadowRootType root_type) {
    SetBodyInnerHTML(
        "<style>"
        "#host_a{position:absolute;}"
        "</style>"
        "<div id='host_a'>"
        "</div>");
    auto* host_a = GetElementById("host_a");
    auto& shadow_a = host_a->AttachShadowRootInternal(root_type);
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

  run_test(ShadowRootType::kOpen);
  run_test(ShadowRootType::kClosed);
}

}  // namespace blink
