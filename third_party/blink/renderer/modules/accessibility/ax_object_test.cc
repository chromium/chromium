// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_object.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_test.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "ui/accessibility/ax_mode.h"

namespace blink {
namespace test {

using testing::Each;
using testing::Property;
using testing::SafeMatcherCast;

TEST_F(AccessibilityTest, IsDescendantOf) {
  SetBodyInnerHTML(R"HTML(<button id="button">button</button>)HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const AXObject* button = GetAXObjectByElementId("button");
  ASSERT_NE(nullptr, button);

  EXPECT_TRUE(button->IsDescendantOf(*root));
  EXPECT_FALSE(root->IsDescendantOf(*root));
  EXPECT_FALSE(button->IsDescendantOf(*button));
  EXPECT_FALSE(root->IsDescendantOf(*button));
}

TEST_F(AccessibilityTest, IsAncestorOf) {
  SetBodyInnerHTML(R"HTML(<button id="button">button</button>)HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const AXObject* button = GetAXObjectByElementId("button");
  ASSERT_NE(nullptr, button);

  EXPECT_TRUE(root->IsAncestorOf(*button));
  EXPECT_FALSE(root->IsAncestorOf(*root));
  EXPECT_FALSE(button->IsAncestorOf(*button));
  EXPECT_FALSE(button->IsAncestorOf(*root));
}

TEST_F(AccessibilityTest, IsEditableInTextField) {
  SetBodyInnerHTML(R"HTML(
      <input type="text" id="input" value="Test">
      <textarea id="textarea">
        Test
      </textarea>)HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const AXObject* input = GetAXObjectByElementId("input");
  ASSERT_NE(nullptr, input);
  const AXObject* input_text =
      input->FirstChildIncludingIgnored()->UnignoredChildAt(0);
  ASSERT_NE(nullptr, input_text);
  ASSERT_EQ(ax::mojom::blink::Role::kStaticText, input_text->RoleValue());
  const AXObject* textarea = GetAXObjectByElementId("textarea");
  ASSERT_NE(nullptr, textarea);
  const AXObject* textarea_text =
      textarea->FirstChildIncludingIgnored()->UnignoredChildAt(0);
  ASSERT_NE(nullptr, textarea_text);
  ASSERT_EQ(ax::mojom::blink::Role::kStaticText, textarea_text->RoleValue());

  EXPECT_FALSE(root->IsEditable());
  EXPECT_TRUE(input->IsEditable());
  EXPECT_TRUE(input_text->IsEditable());
  EXPECT_TRUE(textarea->IsEditable());
  EXPECT_TRUE(textarea_text->IsEditable());

  EXPECT_FALSE(root->IsEditableRoot());
  EXPECT_FALSE(input->IsEditableRoot());
  EXPECT_FALSE(input_text->IsEditableRoot());
  EXPECT_FALSE(textarea->IsEditableRoot());
  EXPECT_FALSE(textarea_text->IsEditableRoot());

  EXPECT_FALSE(root->HasContentEditableAttributeSet());
  EXPECT_FALSE(input->HasContentEditableAttributeSet());
  EXPECT_FALSE(input_text->HasContentEditableAttributeSet());
  EXPECT_FALSE(textarea->HasContentEditableAttributeSet());
  EXPECT_FALSE(textarea_text->HasContentEditableAttributeSet());

  EXPECT_FALSE(root->IsMultiline());
  EXPECT_FALSE(input->IsMultiline());
  EXPECT_FALSE(input_text->IsMultiline());
  EXPECT_TRUE(textarea->IsMultiline());
  EXPECT_FALSE(textarea_text->IsMultiline());

  EXPECT_FALSE(root->IsRichlyEditable());
  EXPECT_FALSE(input->IsRichlyEditable());
  EXPECT_FALSE(input_text->IsRichlyEditable());
  EXPECT_FALSE(textarea->IsRichlyEditable());
  EXPECT_FALSE(textarea_text->IsRichlyEditable());
}

TEST_F(AccessibilityTest, IsEditableInTextFieldWithContentEditableTrue) {
  SetBodyInnerHTML(R"HTML(
      <!-- This is technically an authoring error, but we should still handle
           it correctly. -->
      <input type="text" id="input" value="Test" contenteditable="true">
      <textarea id="textarea" contenteditable="true">
        Test
      </textarea>)HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const AXObject* input = GetAXObjectByElementId("input");
  ASSERT_NE(nullptr, input);
  const AXObject* input_text =
      input->FirstChildIncludingIgnored()->UnignoredChildAt(0);
  ASSERT_NE(nullptr, input_text);
  ASSERT_EQ(ax::mojom::blink::Role::kStaticText, input_text->RoleValue());
  const AXObject* textarea = GetAXObjectByElementId("textarea");
  ASSERT_NE(nullptr, textarea);
  const AXObject* textarea_text =
      textarea->FirstChildIncludingIgnored()->UnignoredChildAt(0);
  ASSERT_NE(nullptr, textarea_text);
  ASSERT_EQ(ax::mojom::blink::Role::kStaticText, textarea_text->RoleValue());

  EXPECT_FALSE(root->IsEditable());
  EXPECT_TRUE(input->IsEditable());
  EXPECT_TRUE(input_text->IsEditable());
  EXPECT_TRUE(textarea->IsEditable());
  EXPECT_TRUE(textarea_text->IsEditable());

  EXPECT_FALSE(root->IsEditableRoot());
  EXPECT_FALSE(input->IsEditableRoot());
  EXPECT_FALSE(input_text->IsEditableRoot());
  EXPECT_FALSE(textarea->IsEditableRoot());
  EXPECT_FALSE(textarea_text->IsEditableRoot());

  EXPECT_FALSE(root->HasContentEditableAttributeSet());
  EXPECT_TRUE(input->HasContentEditableAttributeSet());
  EXPECT_FALSE(input_text->HasContentEditableAttributeSet());
  EXPECT_TRUE(textarea->HasContentEditableAttributeSet());
  EXPECT_FALSE(textarea_text->HasContentEditableAttributeSet());

  EXPECT_FALSE(root->IsMultiline());
  EXPECT_FALSE(input->IsMultiline());
  EXPECT_FALSE(input_text->IsMultiline());
  EXPECT_TRUE(textarea->IsMultiline());
  EXPECT_FALSE(textarea_text->IsMultiline());

  EXPECT_FALSE(root->IsRichlyEditable());
  EXPECT_FALSE(input->IsRichlyEditable());
  EXPECT_FALSE(input_text->IsRichlyEditable());
  EXPECT_FALSE(textarea->IsRichlyEditable());
  EXPECT_FALSE(textarea_text->IsRichlyEditable());
}

TEST_F(AccessibilityTest, IsEditableInContentEditable) {
  // On purpose, also add the textbox role to ensure that it won't affect the
  // contenteditable state.
  SetBodyInnerHTML(R"HTML(
      <div role="textbox" contenteditable="true" id="outerContenteditable">
        Test
        <div contenteditable="plaintext-only" id="innerContenteditable">
          Test
        </div>
      </div>)HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const AXObject* outer_contenteditable =
      GetAXObjectByElementId("outerContenteditable");
  ASSERT_NE(nullptr, outer_contenteditable);
  const AXObject* outer_contenteditable_text =
      outer_contenteditable->UnignoredChildAt(0);
  ASSERT_NE(nullptr, outer_contenteditable_text);
  ASSERT_EQ(ax::mojom::blink::Role::kStaticText,
            outer_contenteditable_text->RoleValue());
  const AXObject* inner_contenteditable =
      GetAXObjectByElementId("innerContenteditable");
  ASSERT_NE(nullptr, inner_contenteditable);
  const AXObject* inner_contenteditable_text =
      inner_contenteditable->UnignoredChildAt(0);
  ASSERT_NE(nullptr, inner_contenteditable_text);
  ASSERT_EQ(ax::mojom::blink::Role::kStaticText,
            inner_contenteditable_text->RoleValue());

  EXPECT_FALSE(root->IsEditable());
  EXPECT_TRUE(outer_contenteditable->IsEditable());
  EXPECT_TRUE(outer_contenteditable_text->IsEditable());
  EXPECT_TRUE(inner_contenteditable->IsEditable());
  EXPECT_TRUE(inner_contenteditable_text->IsEditable());

  EXPECT_FALSE(root->IsEditableRoot());
  EXPECT_TRUE(outer_contenteditable->IsEditableRoot());
  EXPECT_FALSE(outer_contenteditable_text->IsEditableRoot());
  EXPECT_TRUE(inner_contenteditable->IsEditableRoot());
  EXPECT_FALSE(inner_contenteditable_text->IsEditableRoot());

  EXPECT_FALSE(root->HasContentEditableAttributeSet());
  EXPECT_TRUE(outer_contenteditable->HasContentEditableAttributeSet());
  EXPECT_FALSE(outer_contenteditable_text->HasContentEditableAttributeSet());
  EXPECT_TRUE(inner_contenteditable->HasContentEditableAttributeSet());
  EXPECT_FALSE(inner_contenteditable_text->HasContentEditableAttributeSet());

  EXPECT_FALSE(root->IsMultiline());
  EXPECT_TRUE(outer_contenteditable->IsMultiline());
  EXPECT_FALSE(outer_contenteditable_text->IsMultiline());
  EXPECT_TRUE(inner_contenteditable->IsMultiline());
  EXPECT_FALSE(inner_contenteditable_text->IsMultiline());

  EXPECT_FALSE(root->IsRichlyEditable());
  EXPECT_TRUE(outer_contenteditable->IsRichlyEditable());
  EXPECT_TRUE(outer_contenteditable_text->IsRichlyEditable());
  // contenteditable="plaintext-only".
  EXPECT_FALSE(inner_contenteditable->IsRichlyEditable());
  EXPECT_FALSE(inner_contenteditable_text->IsRichlyEditable());
}

TEST_F(AccessibilityTest, IsEditableInCanvasFallback) {
  SetBodyInnerHTML(R"HTML(
      <canvas id="canvas" width="300" height="300">
        <input id="input" value="Test">
        <div contenteditable="true" id="outerContenteditable">
          Test
          <div contenteditable="plaintext-only" id="innerContenteditable">
            Test
          </div>
        </div>
      </canvas>)HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const AXObject* canvas = GetAXObjectByElementId("canvas");
  ASSERT_NE(nullptr, canvas);
  const AXObject* input = GetAXObjectByElementId("input");
  ASSERT_NE(nullptr, input);
  const AXObject* input_text =
      input->FirstChildIncludingIgnored()->UnignoredChildAt(0);
  ASSERT_NE(nullptr, input_text);
  ASSERT_EQ(ax::mojom::blink::Role::kStaticText, input_text->RoleValue());
  const AXObject* outer_contenteditable =
      GetAXObjectByElementId("outerContenteditable");
  ASSERT_NE(nullptr, outer_contenteditable);
  const AXObject* outer_contenteditable_text =
      outer_contenteditable->UnignoredChildAt(0);
  ASSERT_NE(nullptr, outer_contenteditable_text);
  ASSERT_EQ(ax::mojom::blink::Role::kStaticText,
            outer_contenteditable_text->RoleValue());
  const AXObject* inner_contenteditable =
      GetAXObjectByElementId("innerContenteditable");
  ASSERT_NE(nullptr, inner_contenteditable);
  const AXObject* inner_contenteditable_text =
      inner_contenteditable->UnignoredChildAt(0);
  ASSERT_NE(nullptr, inner_contenteditable_text);
  ASSERT_EQ(ax::mojom::blink::Role::kStaticText,
            inner_contenteditable_text->RoleValue());

  EXPECT_FALSE(root->IsEditable());
  EXPECT_FALSE(canvas->IsEditable());
  EXPECT_TRUE(input->IsEditable());
  EXPECT_TRUE(input_text->IsEditable());
  EXPECT_TRUE(outer_contenteditable->IsEditable());
  EXPECT_TRUE(outer_contenteditable_text->IsEditable());
  EXPECT_TRUE(inner_contenteditable->IsEditable());
  EXPECT_TRUE(inner_contenteditable_text->IsEditable());

  EXPECT_FALSE(root->IsEditableRoot());
  EXPECT_FALSE(canvas->IsEditableRoot());
  EXPECT_FALSE(input->IsEditableRoot());
  EXPECT_FALSE(input_text->IsEditableRoot());
  EXPECT_TRUE(outer_contenteditable->IsEditableRoot());
  EXPECT_FALSE(outer_contenteditable_text->IsEditableRoot());
  EXPECT_TRUE(inner_contenteditable->IsEditableRoot());
  EXPECT_FALSE(inner_contenteditable_text->IsEditableRoot());

  EXPECT_FALSE(root->HasContentEditableAttributeSet());
  EXPECT_FALSE(canvas->HasContentEditableAttributeSet());
  EXPECT_FALSE(input->HasContentEditableAttributeSet());
  EXPECT_FALSE(input_text->HasContentEditableAttributeSet());
  EXPECT_TRUE(outer_contenteditable->HasContentEditableAttributeSet());
  EXPECT_FALSE(outer_contenteditable_text->HasContentEditableAttributeSet());
  EXPECT_TRUE(inner_contenteditable->HasContentEditableAttributeSet());
  EXPECT_FALSE(inner_contenteditable_text->HasContentEditableAttributeSet());

  EXPECT_FALSE(root->IsMultiline());
  EXPECT_FALSE(canvas->IsMultiline());
  EXPECT_FALSE(input->IsMultiline());
  EXPECT_FALSE(input_text->IsMultiline());
  EXPECT_TRUE(outer_contenteditable->IsMultiline());
  EXPECT_FALSE(outer_contenteditable_text->IsMultiline());
  EXPECT_TRUE(inner_contenteditable->IsMultiline());
  EXPECT_FALSE(inner_contenteditable_text->IsMultiline());

  EXPECT_FALSE(root->IsRichlyEditable());
  EXPECT_FALSE(canvas->IsRichlyEditable());
  EXPECT_FALSE(input->IsRichlyEditable());
  EXPECT_FALSE(input_text->IsRichlyEditable());
  EXPECT_TRUE(outer_contenteditable->IsRichlyEditable());
  EXPECT_TRUE(outer_contenteditable_text->IsRichlyEditable());
  EXPECT_FALSE(inner_contenteditable->IsRichlyEditable());
  EXPECT_FALSE(inner_contenteditable_text->IsRichlyEditable());
}

TEST_F(AccessibilityTest, DetachedIsIgnored) {
  SetBodyInnerHTML(R"HTML(<button id="button">button</button>)HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  AXObject* button = GetAXObjectByElementId("button");
  ASSERT_NE(nullptr, button);

  EXPECT_FALSE(button->IsDetached());
  EXPECT_FALSE(button->AccessibilityIsIgnored());
  GetAXObjectCache().Remove(button->GetNode());
  EXPECT_TRUE(button->IsDetached());
  EXPECT_TRUE(button->AccessibilityIsIgnored());
  EXPECT_FALSE(button->AccessibilityIsIgnoredButIncludedInTree());
}

TEST_F(AccessibilityTest, UnignoredChildren) {
  SetBodyInnerHTML(R"HTML(This is a test with
                   <p role="presentation">
                     ignored objects
                   </p>
                   <p>
                     which are at multiple
                   </p>
                   <p role="presentation">
                     <p role="presentation">
                       depth levels
                     </p>
                     in the accessibility tree.
                   </p>)HTML");

  const AXObject* ax_body = GetAXRootObject()->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_body);

  ASSERT_EQ(5, ax_body->UnignoredChildCount());
  EXPECT_EQ(ax::mojom::blink::Role::kStaticText,
            ax_body->UnignoredChildAt(0)->RoleValue());
  EXPECT_EQ("This is a test with",
            ax_body->UnignoredChildAt(0)->ComputedName());
  EXPECT_EQ(ax::mojom::blink::Role::kStaticText,
            ax_body->UnignoredChildAt(1)->RoleValue());
  EXPECT_EQ("ignored objects", ax_body->UnignoredChildAt(1)->ComputedName());
  EXPECT_EQ(ax::mojom::blink::Role::kParagraph,
            ax_body->UnignoredChildAt(2)->RoleValue());
  EXPECT_EQ(ax::mojom::blink::Role::kStaticText,
            ax_body->UnignoredChildAt(3)->RoleValue());
  EXPECT_EQ("depth levels", ax_body->UnignoredChildAt(3)->ComputedName());
  EXPECT_EQ(ax::mojom::blink::Role::kStaticText,
            ax_body->UnignoredChildAt(4)->RoleValue());
  EXPECT_EQ("in the accessibility tree.",
            ax_body->UnignoredChildAt(4)->ComputedName());
}

TEST_F(AccessibilityTest, SimpleTreeNavigation) {
  SetBodyInnerHTML(R"HTML(<input id="input" type="text" value="value">
                   <div id="ignored_a" aria-hidden="true"></div>
                   <p id="paragraph">hello<br id="br">there</p>
                   <span id="ignored_b" aria-hidden="true"></span>
                   <button id="button">button</button>)HTML");

  const AXObject* body = GetAXBodyObject();
  ASSERT_NE(nullptr, body);
  const AXObject* input = GetAXObjectByElementId("input");
  ASSERT_NE(nullptr, input);
  ASSERT_NE(nullptr, GetAXObjectByElementId("ignored_a"));
  ASSERT_TRUE(GetAXObjectByElementId("ignored_a")->AccessibilityIsIgnored());
  const AXObject* paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, paragraph);
  const AXObject* br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, br);
  ASSERT_NE(nullptr, GetAXObjectByElementId("ignored_b"));
  ASSERT_TRUE(GetAXObjectByElementId("ignored_b")->AccessibilityIsIgnored());
  const AXObject* button = GetAXObjectByElementId("button");
  ASSERT_NE(nullptr, button);

  EXPECT_EQ(input, body->FirstChildIncludingIgnored());
  EXPECT_EQ(button, body->LastChildIncludingIgnored());

  ASSERT_NE(nullptr, paragraph->FirstChildIncludingIgnored());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            paragraph->FirstChildIncludingIgnored()->RoleValue());
  ASSERT_NE(nullptr, paragraph->LastChildIncludingIgnored());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            paragraph->LastChildIncludingIgnored()->RoleValue());
  ASSERT_NE(nullptr, paragraph->DeepestFirstChildIncludingIgnored());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            paragraph->DeepestFirstChildIncludingIgnored()->RoleValue());
  ASSERT_NE(nullptr, paragraph->DeepestLastChildIncludingIgnored());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            paragraph->DeepestLastChildIncludingIgnored()->RoleValue());

  EXPECT_EQ(paragraph->PreviousSiblingIncludingIgnored(),
            GetAXObjectByElementId("ignored_a"));
  EXPECT_EQ(GetAXObjectByElementId("ignored_a"),
            input->NextSiblingIncludingIgnored());
  ASSERT_NE(nullptr, br->NextSiblingIncludingIgnored());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            br->NextSiblingIncludingIgnored()->RoleValue());
  ASSERT_NE(nullptr, br->PreviousSiblingIncludingIgnored());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            br->PreviousSiblingIncludingIgnored()->RoleValue());

  EXPECT_EQ(paragraph->UnignoredPreviousSibling(), input);
  EXPECT_EQ(paragraph, input->UnignoredNextSibling());
  ASSERT_NE(nullptr, br->UnignoredNextSibling());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            br->UnignoredNextSibling()->RoleValue());
  ASSERT_NE(nullptr, br->UnignoredPreviousSibling());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            br->UnignoredPreviousSibling()->RoleValue());

  ASSERT_NE(nullptr, button->FirstChildIncludingIgnored());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            button->FirstChildIncludingIgnored()->RoleValue());
  ASSERT_NE(nullptr, button->LastChildIncludingIgnored());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            button->LastChildIncludingIgnored()->RoleValue());
  ASSERT_NE(nullptr, button->DeepestFirstChildIncludingIgnored());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            paragraph->DeepestFirstChildIncludingIgnored()->RoleValue());
}

TEST_F(AccessibilityTest, LangAttrInteresting) {
  SetBodyInnerHTML(R"HTML(
      <div id="A"><span>some text</span></div>
      <div id="B"><span lang='en'>some text</span></div>
      )HTML");

  const AXObject* obj_a = GetAXObjectByElementId("A");
  ASSERT_NE(nullptr, obj_a);
  ASSERT_EQ(obj_a->ChildCountIncludingIgnored(), 1);

  // A.span will be excluded from tree as it isn't semantically interesting.
  // Instead its kStaticText child will be promoted.
  const AXObject* span_1 = obj_a->ChildAtIncludingIgnored(0);
  ASSERT_NE(nullptr, span_1);
  EXPECT_EQ(ax::mojom::Role::kStaticText, span_1->RoleValue());

  const AXObject* obj_b = GetAXObjectByElementId("B");
  ASSERT_NE(nullptr, obj_b);
  ASSERT_EQ(obj_b->ChildCountIncludingIgnored(), 1);

  // B.span will be present as the lang attribute is semantically interesting.
  const AXObject* span_2 = obj_b->ChildAtIncludingIgnored(0);
  ASSERT_NE(nullptr, span_2);
  EXPECT_EQ(ax::mojom::Role::kGenericContainer, span_2->RoleValue());
}

TEST_F(AccessibilityTest, LangAttrInterestingHidden) {
  SetBodyInnerHTML(R"HTML(
      <div id="A"><span lang='en' aria-hidden='true'>some text</span></div>
      )HTML");

  const AXObject* obj_a = GetAXObjectByElementId("A");
  ASSERT_NE(nullptr, obj_a);
  ASSERT_EQ(obj_a->ChildCountIncludingIgnored(), 1);

  // A.span will be present as the lang attribute is semantically interesting.
  const AXObject* span_1 = obj_a->ChildAtIncludingIgnored(0);
  ASSERT_NE(nullptr, span_1);
  EXPECT_EQ(ax::mojom::Role::kGenericContainer, span_1->RoleValue());
  EXPECT_TRUE(span_1->AccessibilityIsIgnoredButIncludedInTree());
}

TEST_F(AccessibilityTest, TreeNavigationWithIgnoredContainer) {
  // Setup the following tree :
  // ++A
  // ++IGNORED
  // ++++B
  // ++C
  // So that nodes [A, B, C] are siblings
  SetBodyInnerHTML(R"HTML(
      <p id="A">some text</p>
      <div>
        <p id="B">nested text</p>
      </div>
      <p id="C">more text</p>
      )HTML");

  const AXObject* root = GetAXRootObject();
  const AXObject* body = GetAXBodyObject();
  ASSERT_EQ(3, body->ChildCountIncludingIgnored());
  ASSERT_EQ(1, body->ChildAtIncludingIgnored(1)->ChildCountIncludingIgnored());

  ASSERT_FALSE(root->AccessibilityIsIgnored());
  ASSERT_TRUE(body->AccessibilityIsIgnored());
  const AXObject* obj_a = GetAXObjectByElementId("A");
  ASSERT_NE(nullptr, obj_a);
  ASSERT_FALSE(obj_a->AccessibilityIsIgnored());
  const AXObject* obj_a_text = obj_a->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, obj_a_text);
  EXPECT_EQ(ax::mojom::Role::kStaticText, obj_a_text->RoleValue());
  const AXObject* obj_b = GetAXObjectByElementId("B");
  ASSERT_NE(nullptr, obj_b);
  ASSERT_FALSE(obj_b->AccessibilityIsIgnored());
  const AXObject* obj_b_text = obj_b->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, obj_b_text);
  EXPECT_EQ(ax::mojom::Role::kStaticText, obj_b_text->RoleValue());
  const AXObject* obj_c = GetAXObjectByElementId("C");
  ASSERT_NE(nullptr, obj_c);
  ASSERT_FALSE(obj_c->AccessibilityIsIgnored());
  const AXObject* obj_c_text = obj_c->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, obj_c_text);
  EXPECT_EQ(ax::mojom::Role::kStaticText, obj_c_text->RoleValue());
  const AXObject* obj_ignored = body->ChildAtIncludingIgnored(1);
  ASSERT_NE(nullptr, obj_ignored);
  ASSERT_TRUE(obj_ignored->AccessibilityIsIgnored());

  EXPECT_EQ(root, obj_a->ParentObjectUnignored());
  EXPECT_EQ(body, obj_a->ParentObjectIncludedInTree());
  EXPECT_EQ(root, obj_b->ParentObjectUnignored());
  EXPECT_EQ(obj_ignored, obj_b->ParentObjectIncludedInTree());
  EXPECT_EQ(root, obj_c->ParentObjectUnignored());
  EXPECT_EQ(body, obj_c->ParentObjectIncludedInTree());

  EXPECT_EQ(obj_b, obj_ignored->FirstChildIncludingIgnored());

  EXPECT_EQ(nullptr, obj_a->PreviousSiblingIncludingIgnored());
  EXPECT_EQ(nullptr, obj_a->UnignoredPreviousSibling());
  EXPECT_EQ(obj_ignored, obj_a->NextSiblingIncludingIgnored());
  EXPECT_EQ(obj_b, obj_a->UnignoredNextSibling());

  EXPECT_EQ(body, obj_a->PreviousInPreOrderIncludingIgnored());
  EXPECT_EQ(root, obj_a->UnignoredPreviousInPreOrder());
  EXPECT_EQ(obj_a_text, obj_a->NextInPreOrderIncludingIgnored());
  EXPECT_EQ(obj_a_text, obj_a->UnignoredNextInPreOrder());

  EXPECT_EQ(nullptr, obj_b->PreviousSiblingIncludingIgnored());
  EXPECT_EQ(obj_a, obj_b->UnignoredPreviousSibling());
  EXPECT_EQ(nullptr, obj_b->NextSiblingIncludingIgnored());
  EXPECT_EQ(obj_c, obj_b->UnignoredNextSibling());

  EXPECT_EQ(obj_ignored, obj_b->PreviousInPreOrderIncludingIgnored());
  EXPECT_EQ(obj_a_text, obj_b->UnignoredPreviousInPreOrder());
  EXPECT_EQ(obj_b_text, obj_b->NextInPreOrderIncludingIgnored());
  EXPECT_EQ(obj_b_text, obj_b->UnignoredNextInPreOrder());

  EXPECT_EQ(obj_ignored, obj_c->PreviousSiblingIncludingIgnored());
  EXPECT_EQ(obj_b, obj_c->UnignoredPreviousSibling());
  EXPECT_EQ(nullptr, obj_c->NextSiblingIncludingIgnored());
  EXPECT_EQ(nullptr, obj_c->UnignoredNextSibling());

  EXPECT_EQ(obj_b_text, obj_c->PreviousInPreOrderIncludingIgnored());
  EXPECT_EQ(obj_b_text, obj_c->UnignoredPreviousInPreOrder());
  EXPECT_EQ(obj_c_text, obj_c->NextInPreOrderIncludingIgnored());
  EXPECT_EQ(obj_c_text, obj_c->UnignoredNextInPreOrder());
}

TEST_F(AccessibilityTest, TreeNavigationWithContinuations) {
  // Continuations found in the layout tree should not appear in the
  // accessibility tree. For example, the following accessibility tree should
  // result from the following HTML.
  //
  // WebArea
  // ++HTMLElement
  // ++++BodyElement
  // ++++++Link
  // ++++++++StaticText "Before block element."
  // ++++++++GenericContainer
  // ++++++++++Paragraph
  // ++++++++++++StaticText "Inside block element."
  // ++++++++StaticText "After block element."
  SetBodyInnerHTML(R"HTML(
      <a id="link" href="#">
        Before block element.
        <div id="div">
          <p id="paragraph">
            Inside block element.
          </p>
        </div>
        After block element.
      </a>
      )HTML");

  const AXObject* ax_root = GetAXRootObject();
  ASSERT_NE(nullptr, ax_root);
  const AXObject* ax_body = GetAXBodyObject();
  ASSERT_NE(nullptr, ax_body);
  const AXObject* ax_link = GetAXObjectByElementId("link");
  ASSERT_NE(nullptr, ax_link);
  const AXObject* ax_text_before = ax_link->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_text_before);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_text_before->RoleValue());
  ASSERT_FALSE(ax_text_before->AccessibilityIsIgnored());
  const AXObject* ax_div = GetAXObjectByElementId("div");
  ASSERT_NE(nullptr, ax_div);
  const AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, ax_paragraph);
  const AXObject* ax_text_inside = ax_paragraph->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_text_inside);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_text_inside->RoleValue());
  ASSERT_FALSE(ax_text_inside->AccessibilityIsIgnored());
  const AXObject* ax_text_after = ax_link->LastChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_text_after);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_text_after->RoleValue());
  ASSERT_FALSE(ax_text_after->AccessibilityIsIgnored());

  //
  // Test parent / child relationships individually. This is easier to debug
  // than printing the whole accessibility tree as a string and comparing with
  // an expected tree.
  //

  // BlockInInline changes |ax_body| not to be ignored. See the design doc at
  // crbug.com/716930 for more details.
  EXPECT_EQ(RuntimeEnabledFeatures::LayoutNGBlockInInlineEnabled() ? ax_body
                                                                   : ax_root,
            ax_link->ParentObjectUnignored());
  EXPECT_EQ(ax_body, ax_link->ParentObjectIncludedInTree());

  EXPECT_EQ(ax_link, ax_text_before->ParentObjectUnignored());
  EXPECT_EQ(ax_link, ax_text_before->ParentObjectIncludedInTree());
  EXPECT_EQ(ax_link, ax_div->ParentObjectUnignored());
  EXPECT_EQ(ax_link, ax_div->ParentObjectIncludedInTree());
  EXPECT_EQ(ax_link, ax_text_after->ParentObjectUnignored());
  EXPECT_EQ(ax_link, ax_text_after->ParentObjectIncludedInTree());

  EXPECT_EQ(ax_div, ax_link->ChildAtIncludingIgnored(1));
  EXPECT_EQ(ax_div, ax_link->UnignoredChildAt(1));

  EXPECT_EQ(nullptr, ax_text_before->PreviousSiblingIncludingIgnored());
  EXPECT_EQ(nullptr, ax_text_before->UnignoredPreviousSibling());
  EXPECT_EQ(ax_div, ax_text_before->NextSiblingIncludingIgnored());
  EXPECT_EQ(ax_div, ax_text_before->UnignoredNextSibling());
  EXPECT_EQ(ax_div, ax_text_after->PreviousSiblingIncludingIgnored());
  EXPECT_EQ(ax_div, ax_text_after->UnignoredPreviousSibling());
  EXPECT_EQ(nullptr, ax_text_after->NextSiblingIncludingIgnored());
  EXPECT_EQ(nullptr, ax_text_after->UnignoredNextSibling());

  EXPECT_EQ(ax_paragraph, ax_div->ChildAtIncludingIgnored(0));
  EXPECT_EQ(ax_paragraph, ax_div->UnignoredChildAt(0));

  EXPECT_EQ(ax_div, ax_paragraph->ParentObjectUnignored());
  EXPECT_EQ(ax_div, ax_paragraph->ParentObjectIncludedInTree());
  EXPECT_EQ(ax_paragraph, ax_text_inside->ParentObjectUnignored());
  EXPECT_EQ(ax_paragraph, ax_text_inside->ParentObjectIncludedInTree());
}

TEST_F(AccessibilityTest, TreeNavigationWithInlineTextBoxes) {
  SetBodyInnerHTML(R"HTML(
      Before paragraph element.
      <p id="paragraph">
        Inside paragraph element.
      </p>
      After paragraph element.
      )HTML");

  AXObject* ax_root = GetAXRootObject();
  ASSERT_NE(nullptr, ax_root);
  ax_root->LoadInlineTextBoxes();

  const AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, ax_paragraph);
  const AXObject* ax_text_inside = ax_paragraph->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_text_inside);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_text_inside->RoleValue());
  const AXObject* ax_text_before = ax_paragraph->UnignoredPreviousSibling();
  ASSERT_NE(nullptr, ax_text_before);
  ASSERT_EQ(ax::mojom::blink::Role::kStaticText, ax_text_before->RoleValue());
  const AXObject* ax_text_after = ax_paragraph->UnignoredNextSibling();
  ASSERT_NE(nullptr, ax_text_after);
  ASSERT_EQ(ax::mojom::blink::Role::kStaticText, ax_text_after->RoleValue());

  //
  // Verify parent / child relationships between static text and inline text
  // boxes.
  //

  EXPECT_EQ(1, ax_text_before->ChildCountIncludingIgnored());
  EXPECT_EQ(1, ax_text_before->UnignoredChildCount());
  const AXObject* ax_inline_before =
      ax_text_before->FirstChildIncludingIgnored();
  EXPECT_EQ(ax::mojom::blink::Role::kInlineTextBox,
            ax_inline_before->RoleValue());
  EXPECT_EQ(ax_text_before, ax_inline_before->ParentObjectIncludedInTree());
  EXPECT_EQ(ax_text_before, ax_inline_before->ParentObjectUnignored());

  EXPECT_EQ(1, ax_text_inside->ChildCountIncludingIgnored());
  EXPECT_EQ(1, ax_text_inside->UnignoredChildCount());
  const AXObject* ax_inline_inside =
      ax_text_inside->FirstChildIncludingIgnored();
  EXPECT_EQ(ax::mojom::blink::Role::kInlineTextBox,
            ax_inline_inside->RoleValue());
  EXPECT_EQ(ax_text_inside, ax_inline_inside->ParentObjectIncludedInTree());
  EXPECT_EQ(ax_text_inside, ax_inline_inside->ParentObjectUnignored());

  EXPECT_EQ(1, ax_text_after->ChildCountIncludingIgnored());
  EXPECT_EQ(1, ax_text_after->UnignoredChildCount());
  const AXObject* ax_inline_after = ax_text_after->FirstChildIncludingIgnored();
  EXPECT_EQ(ax::mojom::blink::Role::kInlineTextBox,
            ax_inline_after->RoleValue());
  EXPECT_EQ(ax_text_after, ax_inline_after->ParentObjectIncludedInTree());
  EXPECT_EQ(ax_text_after, ax_inline_after->ParentObjectUnignored());
}

TEST_F(AccessibilityTest, AXObjectComparisonOperators) {
  SetBodyInnerHTML(R"HTML(<input id="input" type="text" value="value">
                   <p id="paragraph">hello<br id="br">there</p>
                   <button id="button">button</button>)HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const AXObject* input = GetAXObjectByElementId("input");
  ASSERT_NE(nullptr, input);
  const AXObject* paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, paragraph);
  const AXObject* br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, br);
  const AXObject* button = GetAXObjectByElementId("button");
  ASSERT_NE(nullptr, button);

  EXPECT_TRUE(*root == *root);
  EXPECT_FALSE(*root != *root);
  EXPECT_FALSE(*root < *root);
  EXPECT_TRUE(*root <= *root);
  EXPECT_FALSE(*root > *root);
  EXPECT_TRUE(*root >= *root);

  EXPECT_TRUE(*input > *root);
  EXPECT_TRUE(*input >= *root);
  EXPECT_FALSE(*input < *root);
  EXPECT_FALSE(*input <= *root);

  EXPECT_TRUE(*input != *root);
  EXPECT_TRUE(*input < *paragraph);
  EXPECT_TRUE(*br > *input);
  EXPECT_TRUE(*paragraph < *br);
  EXPECT_TRUE(*br >= *paragraph);

  EXPECT_TRUE(*paragraph < *button);
  EXPECT_TRUE(*button > *br);
  EXPECT_FALSE(*button < *button);
  EXPECT_TRUE(*button <= *button);
  EXPECT_TRUE(*button >= *button);
  EXPECT_FALSE(*button > *button);
}

TEST_F(AccessibilityTest, AXObjectUnignoredAncestorsIterator) {
  SetBodyInnerHTML(
      R"HTML(<p id="paragraph"><b role="none" id="bold"><br id="br"></b></p>)HTML");

  AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  AXObject* paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, paragraph);
  AXObject* bold = GetAXObjectByElementId("bold");
  ASSERT_NE(nullptr, bold);
  AXObject* br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, br);
  ASSERT_EQ(ax::mojom::Role::kLineBreak, br->RoleValue());

  AXObject::AncestorsIterator iter = br->UnignoredAncestorsBegin();
  EXPECT_EQ(*paragraph, *iter);
  EXPECT_EQ(ax::mojom::Role::kParagraph, iter->RoleValue());
  EXPECT_EQ(*root, *++iter);
  EXPECT_EQ(*root, *iter++);
  EXPECT_EQ(br->UnignoredAncestorsEnd(), ++iter);
}

TEST_F(AccessibilityTest, AxNodeObjectContainsHtmlAnchorElementUrl) {
  SetBodyInnerHTML(R"HTML(<a id="anchor" href="http://test.com">link</a>)HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const AXObject* anchor = GetAXObjectByElementId("anchor");
  ASSERT_NE(nullptr, anchor);

  // Passing a malformed string to KURL returns an empty URL, so verify the
  // AXObject's URL is non-empty first to catch errors in the test itself.
  EXPECT_FALSE(anchor->Url().IsEmpty());
  EXPECT_EQ(anchor->Url(), KURL("http://test.com"));
}

TEST_F(AccessibilityTest, AxNodeObjectContainsSvgAnchorElementUrl) {
  SetBodyInnerHTML(R"HTML(
    <svg>
      <a id="anchor" xlink:href="http://test.com"></a>
    </svg>
  )HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const AXObject* anchor = GetAXObjectByElementId("anchor");
  ASSERT_NE(nullptr, anchor);

  EXPECT_FALSE(anchor->Url().IsEmpty());
  EXPECT_EQ(anchor->Url(), KURL("http://test.com"));
}

TEST_F(AccessibilityTest, AxNodeObjectContainsImageUrl) {
  SetBodyInnerHTML(R"HTML(<img id="anchor" src="http://test.png" />)HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const AXObject* anchor = GetAXObjectByElementId("anchor");
  ASSERT_NE(nullptr, anchor);

  EXPECT_FALSE(anchor->Url().IsEmpty());
  EXPECT_EQ(anchor->Url(), KURL("http://test.png"));
}

TEST_F(AccessibilityTest, AxNodeObjectContainsInPageLinkTarget) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(<a id="anchor" href="#target">link</a>)HTML");

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const AXObject* anchor = GetAXObjectByElementId("anchor");
  ASSERT_NE(nullptr, anchor);

  EXPECT_FALSE(anchor->Url().IsEmpty());
  EXPECT_EQ(anchor->Url(), KURL("http://test.com/#target"));
}

TEST_F(AccessibilityTest, AxNodeObjectInPageLinkTargetNonAscii) {
  GetDocument().SetURL(KURL("http://test.com"));
  // ö is U+00F6 which URI encodes to %C3%B6
  //
  // This file is forced to be UTF-8 by the build system,
  // the uR"" will create char16_t[] of UTF-16,
  // WTF::String will wrap the char16_t* as UTF-16.
  // All this is checked by ensuring a match against u"\u00F6".
  //
  // TODO(1117212): The escaped version currently takes precedence.
  //  <h1 id="%C3%B6">O2</h1>
  SetBodyInnerHTML(
      uR"HTML(
    <a href="#ö" id="anchor">O</a>
    <h1 id="ö">O</h1>"
    <a href="#t%6Fp" id="top_test">top</a>"
    <a href="#" id="empty_test">also top</a>");
  )HTML");

  {
    // anchor
    const AXObject* anchor = GetAXObjectByElementId("anchor");
    ASSERT_NE(nullptr, anchor);

    EXPECT_FALSE(anchor->Url().IsEmpty());
    EXPECT_EQ(anchor->Url(), KURL(u"http://test.com/#\u00F6"));

    const AXObject* target = anchor->InPageLinkTarget();
    ASSERT_NE(nullptr, target);

    auto* targetElement = DynamicTo<Element>(target->GetNode());
    ASSERT_NE(nullptr, target);
    ASSERT_TRUE(targetElement->HasID());
    EXPECT_EQ(targetElement->IdForStyleResolution(), String(u"\u00F6"));
  }

  {
    // top_test
    const AXObject* anchor = GetAXObjectByElementId("top_test");
    ASSERT_NE(nullptr, anchor);

    EXPECT_FALSE(anchor->Url().IsEmpty());
    EXPECT_EQ(anchor->Url(), KURL(u"http://test.com/#t%6Fp"));

    const AXObject* target = anchor->InPageLinkTarget();
    ASSERT_NE(nullptr, target);

    EXPECT_EQ(&GetDocument(), target->GetNode());
  }

  {
    // empty_test
    const AXObject* anchor = GetAXObjectByElementId("empty_test");
    ASSERT_NE(nullptr, anchor);

    EXPECT_FALSE(anchor->Url().IsEmpty());
    EXPECT_EQ(anchor->Url(), KURL(u"http://test.com/#"));

    const AXObject* target = anchor->InPageLinkTarget();
    ASSERT_NE(nullptr, target);

    EXPECT_EQ(&GetDocument(), target->GetNode());
  }
}

TEST_P(ParameterizedAccessibilityTest, NextOnLine) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html {
      font-size: 10px;
    }
    /* TODO(kojii): |NextOnLine| doesn't work for culled-inline.
       Ensure spans are not culled to avoid hitting the case. */
    span {
      background: gray;
    }
    </style>
    <div><span id="span1">a</span><span>b</span></div>
  )HTML");
  const AXObject* span1 = GetAXObjectByElementId("span1");
  ASSERT_NE(nullptr, span1);

  const AXObject* next = span1->NextOnLine();
  ASSERT_NE(nullptr, next);
  EXPECT_EQ("b", next->GetNode()->textContent());
}

TEST_F(AccessibilityTest, TableRowAndCellIsLineBreakingObject) {
  SetBodyInnerHTML(R"HTML(
      <table id="table">
      <caption>Caption</caption>
        <tr id="row">
          <td id="cell">Cell</td>
        </tr>
      </table>
      )HTML");

  const AXObject* table = GetAXObjectByElementId("table");
  ASSERT_NE(nullptr, table);
  ASSERT_EQ(ax::mojom::Role::kTable, table->RoleValue());
  EXPECT_TRUE(table->IsLineBreakingObject());

  const AXObject* row = GetAXObjectByElementId("row");
  ASSERT_NE(nullptr, row);
  ASSERT_EQ(ax::mojom::Role::kRow, row->RoleValue());
  EXPECT_TRUE(row->IsLineBreakingObject());

  const AXObject* cell = GetAXObjectByElementId("cell");
  ASSERT_NE(nullptr, cell);
  ASSERT_EQ(ax::mojom::Role::kCell, cell->RoleValue());
  EXPECT_TRUE(cell->IsLineBreakingObject());
}

TEST_F(AccessibilityTest,
       PreservedWhitespaceWithInitialLineBreakIsLineBreakingObject) {
  SetBodyInnerHTML(R"HTML(
      <span style="white-space: pre-line" id="preserved">
        First Paragraph
        Second Paragraph
        Third Paragraph
      </span>)HTML");

  const AXObject* preserved_span = GetAXObjectByElementId("preserved");
  ASSERT_NE(nullptr, preserved_span);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, preserved_span->RoleValue());
  ASSERT_EQ(1, preserved_span->UnignoredChildCount());
  EXPECT_FALSE(preserved_span->IsLineBreakingObject());

  AXObject* preserved_text = preserved_span->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, preserved_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, preserved_text->RoleValue());
  EXPECT_TRUE(preserved_text->IsLineBreakingObject())
      << "This text node starts with a line break character, so it should be a "
         "paragraph boundary.";

  // Expect 7 kInlineTextBox children.
  // 3 lines of text, and 4 newlines including one a the start of the text.
  preserved_text->LoadInlineTextBoxes();
  ASSERT_EQ(7, preserved_text->UnignoredChildCount());
  ASSERT_THAT(preserved_text->UnignoredChildren(),
              Each(SafeMatcherCast<AXObject*>(
                  Property("AXObject::RoleValue()", &AXObject::RoleValue,
                           ax::mojom::Role::kInlineTextBox))));

  ASSERT_EQ(preserved_text->UnignoredChildAt(0)->ComputedName(), "\n");
  EXPECT_TRUE(preserved_text->UnignoredChildAt(0)->IsLineBreakingObject());
  ASSERT_EQ(preserved_text->UnignoredChildAt(1)->ComputedName(),
            "First Paragraph");
  EXPECT_FALSE(preserved_text->UnignoredChildAt(1)->IsLineBreakingObject());
  ASSERT_EQ(preserved_text->UnignoredChildAt(2)->ComputedName(), "\n");
  EXPECT_TRUE(preserved_text->UnignoredChildAt(2)->IsLineBreakingObject());
  ASSERT_EQ(preserved_text->UnignoredChildAt(3)->ComputedName(),
            "Second Paragraph");
  EXPECT_FALSE(preserved_text->UnignoredChildAt(3)->IsLineBreakingObject());
  ASSERT_EQ(preserved_text->UnignoredChildAt(4)->ComputedName(), "\n");
  EXPECT_TRUE(preserved_text->UnignoredChildAt(4)->IsLineBreakingObject());
  ASSERT_EQ(preserved_text->UnignoredChildAt(5)->ComputedName(),
            "Third Paragraph");
  EXPECT_FALSE(preserved_text->UnignoredChildAt(5)->IsLineBreakingObject());
  ASSERT_EQ(preserved_text->UnignoredChildAt(6)->ComputedName(), "\n");
  EXPECT_TRUE(preserved_text->UnignoredChildAt(6)->IsLineBreakingObject());
}

TEST_F(AccessibilityTest, DivWithFirstLetterIsLineBreakingObject) {
  SetBodyInnerHTML(R"HTML(
      <style>div::first-letter { color: "red"; }</style>
      <div id="firstLetter">First letter</div>)HTML");

  const AXObject* div = GetAXObjectByElementId("firstLetter");
  ASSERT_NE(nullptr, div);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, div->RoleValue());
  ASSERT_EQ(1, div->UnignoredChildCount());
  EXPECT_TRUE(div->IsLineBreakingObject());

  AXObject* div_text = div->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, div_text);
  ASSERT_EQ(ax::mojom::Role::kStaticText, div_text->RoleValue());
  EXPECT_FALSE(div_text->IsLineBreakingObject());

  div_text->LoadInlineTextBoxes();
  ASSERT_EQ(1, div_text->UnignoredChildCount());
  ASSERT_EQ(ax::mojom::Role::kInlineTextBox,
            div_text->UnignoredChildAt(0)->RoleValue());
  ASSERT_EQ(div_text->UnignoredChildAt(0)->ComputedName(), "First letter");
  EXPECT_FALSE(div_text->UnignoredChildAt(0)->IsLineBreakingObject());
}

TEST_F(AccessibilityTest, SlotIsLineBreakingObject) {
  // Even though a <span>, <b> and <i> element are not line-breaking, a
  // paragraph element in the shadow DOM should be.
  const char* body_content = R"HTML(
      <span id="host">
        <b slot="slot1" id="slot1">slot1</b>
        <i slot="slot2" id="slot2">slot2</i>
      </span>)HTML";
  const char* shadow_content = R"HTML(
      <p><slot name="slot1"></slot></p>
      <p><slot name="slot2"></slot></p>
      )HTML";
  SetBodyContent(body_content);
  ShadowRoot& shadow_root =
      GetElementById("host")->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.setInnerHTML(String::FromUTF8(shadow_content),
                           ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  const AXObject* host = GetAXObjectByElementId("host");
  ASSERT_NE(nullptr, host);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, host->RoleValue());
  EXPECT_FALSE(host->IsLineBreakingObject());
  EXPECT_TRUE(host->ParentObjectUnignored()->IsLineBreakingObject());

  const AXObject* slot1 = GetAXObjectByElementId("slot1");
  ASSERT_NE(nullptr, slot1);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, slot1->RoleValue());
  EXPECT_FALSE(slot1->IsLineBreakingObject());
  EXPECT_TRUE(slot1->ParentObjectUnignored()->IsLineBreakingObject());

  const AXObject* slot2 = GetAXObjectByElementId("slot2");
  ASSERT_NE(nullptr, slot2);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, slot2->RoleValue());
  EXPECT_FALSE(slot2->IsLineBreakingObject());
  EXPECT_TRUE(slot2->ParentObjectUnignored()->IsLineBreakingObject());
}

TEST_F(AccessibilityTest, LineBreakInDisplayLockedIsLineBreakingObject) {
  SetBodyInnerHTML(R"HTML(
      <div id="spacer"
          style="height: 30000px; contain-intrinsic-size: 1px 30000px;"></div>
      <p id="lockedContainer" style="content-visibility: auto">
        Line 1
        <br id="br" style="content-visibility: hidden">
        Line 2
      </p>
      )HTML");

  const AXObject* paragraph = GetAXObjectByElementId("lockedContainer");
  ASSERT_NE(nullptr, paragraph);
  ASSERT_EQ(ax::mojom::Role::kParagraph, paragraph->RoleValue());
  ASSERT_EQ(3, paragraph->UnignoredChildCount());
  ASSERT_EQ(paragraph->GetNode(),
            DisplayLockUtilities::LockedInclusiveAncestorPreventingPaint(
                *paragraph->GetNode()))
      << "The <p> element should be display locked.";
  EXPECT_TRUE(paragraph->IsLineBreakingObject());

  const AXObject* br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, br);
  ASSERT_EQ(ax::mojom::Role::kGenericContainer, br->RoleValue())
      << "The <br> child should be display locked and thus have a generic "
         "role.";
  ASSERT_EQ(paragraph->GetNode(),
            DisplayLockUtilities::LockedInclusiveAncestorPreventingPaint(
                *br->GetNode()))
      << "The <br> child should be display locked.";
  EXPECT_TRUE(br->IsLineBreakingObject());
}

TEST_P(ParameterizedAccessibilityTest, ListMarkerIsNotLineBreakingObject) {
  SetBodyInnerHTML(R"HTML(
      <style>
        ul li::marker {
          content: "X";
        }
      </style>
      <ul id="unorderedList">
        <li id="unorderedListItem">.....
          Unordered item 1
        </li>
      </ul>
      <ol id="orderedList">
        <li id="orderedListItem">
          Ordered item 1
        </li>
      </ol>
      )HTML");

  const AXObject* unordered_list = GetAXObjectByElementId("unorderedList");
  ASSERT_NE(nullptr, unordered_list);
  ASSERT_EQ(ax::mojom::Role::kList, unordered_list->RoleValue());
  EXPECT_TRUE(unordered_list->IsLineBreakingObject());

  const AXObject* unordered_list_item =
      GetAXObjectByElementId("unorderedListItem");
  ASSERT_NE(nullptr, unordered_list_item);
  ASSERT_EQ(ax::mojom::Role::kListItem, unordered_list_item->RoleValue());
  EXPECT_TRUE(unordered_list_item->IsLineBreakingObject());

  const AXObject* unordered_list_marker =
      unordered_list_item->UnignoredChildAt(0);
  ASSERT_NE(nullptr, unordered_list_marker);
  ASSERT_EQ(ax::mojom::Role::kListMarker, unordered_list_marker->RoleValue());
  EXPECT_FALSE(unordered_list_marker->IsLineBreakingObject());

  const AXObject* ordered_list = GetAXObjectByElementId("orderedList");
  ASSERT_NE(nullptr, ordered_list);
  ASSERT_EQ(ax::mojom::Role::kList, ordered_list->RoleValue());
  EXPECT_TRUE(ordered_list->IsLineBreakingObject());

  const AXObject* ordered_list_item = GetAXObjectByElementId("orderedListItem");
  ASSERT_NE(nullptr, ordered_list_item);
  ASSERT_EQ(ax::mojom::Role::kListItem, ordered_list_item->RoleValue());
  EXPECT_TRUE(ordered_list_item->IsLineBreakingObject());

  const AXObject* ordered_list_marker = ordered_list_item->UnignoredChildAt(0);
  ASSERT_NE(nullptr, ordered_list_marker);
  ASSERT_EQ(ax::mojom::Role::kListMarker, ordered_list_marker->RoleValue());
  EXPECT_FALSE(ordered_list_marker->IsLineBreakingObject());
}

TEST_F(AccessibilityTest, CheckNoDuplicateChildren) {
  GetPage().GetSettings().SetInlineTextBoxAccessibilityEnabled(false);
  SetBodyInnerHTML(R"HTML(
     <select id="sel"><option>1</option></select>
    )HTML");

  AXObject* ax_select = GetAXObjectByElementId("sel");
  ax_select->SetNeedsToUpdateChildren();
  ax_select->UpdateChildrenIfNecessary();

  ASSERT_EQ(
      ax_select->FirstChildIncludingIgnored()->ChildCountIncludingIgnored(), 1);
}

TEST_F(AccessibilityTest, InitRelationCacheLabelFor) {
  // Most other tests already have accessibility initialized
  // first, but we don't want to in this test.
  //
  // Get rid of the AXContext so the AXObjectCache is destroyed.
  ax_context_.reset(nullptr);

  SetBodyInnerHTML(R"HTML(
      <label for="a"></label>
      <input id="a">
      <input id="b">
    )HTML");

  // Now recreate an AXContext, simulating what happens if accessibility
  // is enabled after the document is loaded.
  ax_context_ = std::make_unique<AXContext>(GetDocument(), ui::kAXModeComplete);

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  const AXObject* input_a = GetAXObjectByElementId("a");
  ASSERT_NE(nullptr, input_a);
  const AXObject* input_b = GetAXObjectByElementId("b");
  ASSERT_NE(nullptr, input_b);
}

TEST_F(AccessibilityTest, InitRelationCacheAriaOwns) {
  // Most other tests already have accessibility initialized
  // first, but we don't want to in this test.
  //
  // Get rid of the AXContext so the AXObjectCache is destroyed.
  ax_context_.reset(nullptr);

  SetBodyInnerHTML(R"HTML(
      <ul id="ul" aria-owns="li"></ul>
      <div role="section" id="div">
        <li id="li"></li>
      </div>
    )HTML");

  // Now recreate an AXContext, simulating what happens if accessibility
  // is enabled after the document is loaded.
  ax_context_ = std::make_unique<AXContext>(GetDocument(), ui::kAXModeComplete);

  const AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);

  // Note: retrieve the LI first and check that its parent is not
  // the paragraph element. If we were to retrieve the UL element,
  // that would trigger the aria-owns check and wouln't allow us to
  // test whether the relation cache was initialized.
  const AXObject* li = GetAXObjectByElementId("li");
  ASSERT_NE(nullptr, li);

  const AXObject* div = GetAXObjectByElementId("div");
  ASSERT_NE(nullptr, div);
  EXPECT_NE(li->ParentObjectUnignored(), div);

  const AXObject* ul = GetAXObjectByElementId("ul");
  ASSERT_NE(nullptr, ul);

  EXPECT_EQ(li->ParentObjectUnignored(), ul);
}

TEST_F(AccessibilityTest, IsSelectedFromFocusSupported) {
  SetBodyInnerHTML(R"HTML(
      <input role="combobox" type="search" aria-expanded="true"
              aria-haspopup="true" aria-autocomplete="list1" aria-owns="list1">
      <ul id="list1" role="listbox">
        <li id="option1" role="option" tabindex="-1">Apple</li>
      </ul>
      <input role="combobox" type="search" aria-expanded="true"
              aria-haspopup="true" aria-autocomplete="list2" aria-owns="list2">
      <ul id="list2" role="listbox">
        <li id="option2" role="row" tabindex="-1">Apple</li>
      </ul>
      <input role="combobox" type="search" aria-expanded="true"
              aria-haspopup="true" aria-autocomplete="list3" aria-owns="list3">
      <ul id="list3" role="listbox">
        <li id="option3" role="option" tabindex="-1"
            aria-selected="false">Apple</li>
      </ul>
      <input role="combobox" type="search" aria-expanded="true"
              aria-haspopup="true" aria-autocomplete="list4" aria-owns="list4">
      <ul id="list4" role="listbox">
        <li id="option4" role="option" tabindex="-1"
            aria-selected="true">Apple</li>
        <li id="option5" role="option" tabindex="-1">Orange</li>
      </ul>
    )HTML");

  const AXObject* option1 = GetAXObjectByElementId("option1");
  ASSERT_NE(option1, nullptr);
  const AXObject* option2 = GetAXObjectByElementId("option2");
  ASSERT_NE(option2, nullptr);
  const AXObject* option3 = GetAXObjectByElementId("option3");
  ASSERT_NE(option3, nullptr);
  const AXObject* option4 = GetAXObjectByElementId("option4");
  ASSERT_NE(option4, nullptr);
  const AXObject* option5 = GetAXObjectByElementId("option5");
  ASSERT_NE(option5, nullptr);

  EXPECT_TRUE(option1->IsSelectedFromFocusSupported());
  EXPECT_FALSE(option2->IsSelectedFromFocusSupported());
  EXPECT_FALSE(option3->IsSelectedFromFocusSupported());
  EXPECT_FALSE(option4->IsSelectedFromFocusSupported());
  // TODO(crbug.com/1143451): #option5 should not support selection from focus
  // because #option4 is explicitly selected.
  EXPECT_TRUE(option5->IsSelectedFromFocusSupported());
}

TEST_F(AccessibilityTest, GetBoundsInFrameCoordinatesSvgText) {
  // This test doesn't work with the legacy SVG text.
  if (!RuntimeEnabledFeatures::SVGTextNGEnabled())
    return;
  SetBodyInnerHTML(R"HTML(
  <svg width="800" height="600" xmlns="http://www.w3.org/2000/svg">
    <text id="t1" x="100">Text1</text>
    <text id="t2" x="50">Text1</text>
  </svg>)HTML");

  AXObject* text1 = GetAXObjectByElementId("t1");
  ASSERT_NE(text1, nullptr);
  AXObject* text2 = GetAXObjectByElementId("t2");
  ASSERT_NE(text2, nullptr);
  LayoutRect bounds1 = text1->GetBoundsInFrameCoordinates();
  LayoutRect bounds2 = text2->GetBoundsInFrameCoordinates();

  // Check if bounding boxes for SVG <text> respect to positioning
  // attributes such as 'x'.
  EXPECT_GT(bounds1.X(), bounds2.X());
}

TEST_F(AccessibilityTest, ComputeIsInertReason) {
  ScopedInertAttributeForTest enabled_scope(true);
  NonThrowableExceptionState exception_state;
  SetBodyInnerHTML(R"HTML(
    <div id="div1" inert>inert</div>
    <dialog id="dialog1">dialog</dialog>
    <dialog id="dialog2" inert>inert dialog</dialog>
    <p id="p1">fullscreen</p>
    <p id="p2" inert>inert fullscreen</p>
  )HTML");

  Document& document = GetDocument();
  Element* body = document.body();
  Element* div1 = GetElementById("div1");
  Node* div1_text = div1->firstChild();
  auto* dialog1 = To<HTMLDialogElement>(GetElementById("dialog1"));
  Node* dialog1_text = dialog1->firstChild();
  auto* dialog2 = To<HTMLDialogElement>(GetElementById("dialog2"));
  Node* dialog2_text = dialog2->firstChild();
  Element* p1 = GetElementById("p1");
  Node* p1_text = p1->firstChild();
  Element* p2 = GetElementById("p2");
  Node* p2_text = p2->firstChild();

  auto AssertInertReasons = [&](Node* node, AXIgnoredReason expectation) {
    AXObject* object = GetAXObjectCache().GetOrCreate(node);
    ASSERT_NE(object, nullptr);
    AXObject::IgnoredReasons reasons;
    ASSERT_TRUE(object->ComputeIsInert(&reasons));
    ASSERT_EQ(reasons.size(), 1u);
    ASSERT_EQ(reasons[0].reason, expectation);
  };
  auto AssertNotInert = [&](Node* node) {
    AXObject* object = GetAXObjectCache().GetOrCreate(node);
    ASSERT_NE(object, nullptr);
    AXObject::IgnoredReasons reasons;
    ASSERT_FALSE(object->ComputeIsInert(&reasons));
    ASSERT_EQ(reasons.size(), 0u);
  };
  auto EnterFullscreen = [&](Element* element) {
    LocalFrame::NotifyUserActivation(
        document.GetFrame(), mojom::UserActivationNotificationType::kTest);
    Fullscreen::RequestFullscreen(*element);
    Fullscreen::DidResolveEnterFullscreenRequest(document, /*granted*/ true);
  };
  auto ExitFullscreen = [&]() {
    Fullscreen::FullyExitFullscreen(document);
    Fullscreen::DidExitFullscreen(document);
  };

  AssertNotInert(body);
  AssertInertReasons(div1, kAXInertElement);
  AssertInertReasons(div1_text, kAXInertSubtree);
  AssertNotInert(dialog1);
  AssertNotInert(dialog1_text);
  AssertInertReasons(dialog2, kAXInertElement);
  AssertInertReasons(dialog2_text, kAXInertSubtree);
  AssertNotInert(p1);
  AssertNotInert(p1_text);
  AssertInertReasons(p2, kAXInertElement);
  AssertInertReasons(p2_text, kAXInertSubtree);

  dialog1->showModal(exception_state);

  AssertInertReasons(body, kAXActiveModalDialog);
  AssertInertReasons(div1, kAXInertElement);
  AssertInertReasons(div1_text, kAXInertSubtree);
  AssertNotInert(dialog1);
  AssertNotInert(dialog1_text);
  AssertInertReasons(dialog2, kAXInertElement);
  AssertInertReasons(dialog2_text, kAXInertSubtree);
  AssertInertReasons(p1, kAXActiveModalDialog);
  AssertInertReasons(p1_text, kAXActiveModalDialog);
  AssertInertReasons(p2, kAXInertElement);
  AssertInertReasons(p2_text, kAXInertSubtree);

  dialog2->showModal(exception_state);

  AssertInertReasons(body, kAXActiveModalDialog);
  AssertInertReasons(div1, kAXInertElement);
  AssertInertReasons(div1_text, kAXInertSubtree);
  AssertInertReasons(dialog1, kAXActiveModalDialog);
  AssertInertReasons(dialog1_text, kAXActiveModalDialog);
  AssertInertReasons(dialog2, kAXInertElement);
  AssertInertReasons(dialog2_text, kAXInertSubtree);
  AssertInertReasons(p1, kAXActiveModalDialog);
  AssertInertReasons(p1_text, kAXActiveModalDialog);
  AssertInertReasons(p2, kAXInertElement);
  AssertInertReasons(p2_text, kAXInertSubtree);

  EnterFullscreen(p1);

  AssertInertReasons(body, kAXActiveModalDialog);
  AssertInertReasons(div1, kAXInertElement);
  AssertInertReasons(div1_text, kAXInertSubtree);
  AssertInertReasons(dialog1, kAXActiveModalDialog);
  AssertInertReasons(dialog1_text, kAXActiveModalDialog);
  AssertInertReasons(dialog2, kAXInertElement);
  AssertInertReasons(dialog2_text, kAXInertSubtree);
  AssertInertReasons(p1, kAXActiveModalDialog);
  AssertInertReasons(p1_text, kAXActiveModalDialog);
  AssertInertReasons(p2, kAXInertElement);
  AssertInertReasons(p2_text, kAXInertSubtree);

  dialog1->close();
  dialog2->close();

  AssertInertReasons(body, kAXActiveFullscreenElement);
  AssertInertReasons(div1, kAXInertElement);
  AssertInertReasons(div1_text, kAXInertSubtree);
  AssertInertReasons(dialog1, kAXActiveFullscreenElement);
  AssertInertReasons(dialog1_text, kAXActiveFullscreenElement);
  AssertInertReasons(dialog2, kAXInertElement);
  AssertInertReasons(dialog2_text, kAXInertSubtree);
  AssertNotInert(p1);
  AssertNotInert(p1_text);
  AssertInertReasons(p2, kAXInertElement);
  AssertInertReasons(p2_text, kAXInertSubtree);

  ExitFullscreen();
  EnterFullscreen(p2);

  AssertInertReasons(body, kAXActiveFullscreenElement);
  AssertInertReasons(div1, kAXInertElement);
  AssertInertReasons(div1_text, kAXInertSubtree);
  AssertInertReasons(dialog1, kAXActiveFullscreenElement);
  AssertInertReasons(dialog1_text, kAXActiveFullscreenElement);
  AssertInertReasons(dialog2, kAXInertElement);
  AssertInertReasons(dialog2_text, kAXInertSubtree);
  AssertInertReasons(p1, kAXActiveFullscreenElement);
  AssertInertReasons(p1_text, kAXActiveFullscreenElement);
  AssertInertReasons(p2, kAXInertElement);
  AssertInertReasons(p2_text, kAXInertSubtree);

  ExitFullscreen();

  AssertNotInert(body);
  AssertInertReasons(div1, kAXInertElement);
  AssertInertReasons(div1_text, kAXInertSubtree);
  AssertNotInert(dialog1);
  AssertNotInert(dialog1_text);
  AssertInertReasons(dialog2, kAXInertElement);
  AssertInertReasons(dialog2_text, kAXInertSubtree);
  AssertNotInert(p1);
  AssertNotInert(p1_text);
  AssertInertReasons(p2, kAXInertElement);
  AssertInertReasons(p2_text, kAXInertSubtree);
}

TEST_F(AccessibilityTest, ComputeIsInertWithNonHTMLElements) {
  ScopedInertAttributeForTest enabled_scope(true);
  SetBodyInnerHTML(R"HTML(
    <main inert>
      main
      <foo inert>
        foo
        <svg inert>
          foo
          <foreignObject inert>
            foo
            <div inert>
              div
              <math inert>
                div
                <mi inert>
                  div
                  <span inert>
                    span
                  </span>
                </mi>
              </math>
            </div>
          </foreignObject>
        </svg>
      </foo>
    </main>
  )HTML");

  Document& document = GetDocument();
  Element* element = document.QuerySelector("main");
  while (element) {
    Node* node = element->firstChild();
    AXObject* ax_node = GetAXObjectCache().GetOrCreate(node);

    // The text indicates the expected inert root, which is the nearest HTML
    // element ancestor with the 'inert' attribute.
    AtomicString selector(node->textContent().Impl());
    Element* inert_root = document.QuerySelector(selector);
    AXObject* ax_inert_root = GetAXObjectCache().GetOrCreate(inert_root);

    AXObject::IgnoredReasons reasons;
    ASSERT_TRUE(ax_node->ComputeIsInert(&reasons));
    ASSERT_EQ(reasons.size(), 1u);
    ASSERT_EQ(reasons[0].reason, kAXInertSubtree);
    ASSERT_EQ(reasons[0].related_object.Get(), ax_inert_root);

    element = ElementTraversal::FirstChild(*element);
  }
}

TEST_F(AccessibilityTest, CanSetFocusInCanvasFallbackContent) {
  ScopedInertAttributeForTest enabled_scope(true);
  SetBodyInnerHTML(R"HTML(
    <canvas>
      <section>
        <div tabindex="-1" id="div"></div>
        <span tabindex="-1" id="span"></div>
        <a tabindex="-1" id="a"></a>
      </section>
      <section hidden>
        <div tabindex="-1" id="div-hidden"></div>
        <span tabindex="-1" id="span-hidden"></div>
        <a tabindex="-1" id="a-hidden"></a>
      </section>
      <section inert>
        <div tabindex="-1" id="div-inert"></div>
        <span tabindex="-1" id="span-inert"></div>
        <a tabindex="-1" id="a-inert"></a>
      </section>
      <section hidden inert>
        <div tabindex="-1" id="div-hidden-inert"></div>
        <span tabindex="-1" id="span-hidden-inert"></div>
        <a tabindex="-1" id="a-hidden-inert"></a>
      </section>
    </div>
  )HTML");

  // Elements being used as relevant canvas fallback content can be focusable.
  ASSERT_TRUE(GetAXObjectByElementId("div")->CanSetFocusAttribute());
  ASSERT_TRUE(GetAXObjectByElementId("span")->CanSetFocusAttribute());
  ASSERT_TRUE(GetAXObjectByElementId("a")->CanSetFocusAttribute());

  // But they are not focusable if in a display:none subtree...
  ASSERT_FALSE(GetAXObjectByElementId("div-hidden")->CanSetFocusAttribute());
  ASSERT_FALSE(GetAXObjectByElementId("span-hidden")->CanSetFocusAttribute());
  ASSERT_FALSE(GetAXObjectByElementId("a-hidden")->CanSetFocusAttribute());

  // ...nor if inert...
  ASSERT_FALSE(GetAXObjectByElementId("div-inert")->CanSetFocusAttribute());
  ASSERT_FALSE(GetAXObjectByElementId("span-inert")->CanSetFocusAttribute());
  ASSERT_FALSE(GetAXObjectByElementId("a-inert")->CanSetFocusAttribute());

  // ...nor a combination of both.
  ASSERT_FALSE(
      GetAXObjectByElementId("div-hidden-inert")->CanSetFocusAttribute());
  ASSERT_FALSE(
      GetAXObjectByElementId("span-hidden-inert")->CanSetFocusAttribute());
  ASSERT_FALSE(
      GetAXObjectByElementId("a-hidden-inert")->CanSetFocusAttribute());
}

}  // namespace test
}  // namespace blink
