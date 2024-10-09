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
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_test.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_id.h"

namespace blink {
namespace test {

using testing::Each;
using testing::Property;
using testing::SafeMatcherCast;

TEST_F(AccessibilityTest, GetClosestElementChecksStartingNode) {
  SetBodyInnerHTML(R"HTML(<button id="button">button</button>)HTML");

  const AXObject* button = GetAXObjectByElementId("button");
  ASSERT_NE(nullptr, button);
  const Element* closestElement = button->GetClosestElement();
  ASSERT_NE(nullptr, closestElement);

  EXPECT_TRUE(closestElement == button->GetElement());
}

TEST_F(AccessibilityTest, GetClosestElementSearchesAmongAncestors) {
  SetBodyInnerHTML(R"HTML(
        <style>
        button::before{
            content: "Content";
        }
        </style>
        <button id="button">button</button>
      )HTML");

  AXObject* button = GetAXObjectByElementId("button");
  button->LoadInlineTextBoxes();
  // Guaranteed to have no element since this should be the AX node created from
  // pseudo element content
  const AXObject* nodeWithNoElement =
      button->DeepestFirstChildIncludingIgnored()->ParentObject();
  ASSERT_EQ(nullptr, nodeWithNoElement->GetElement());

  EXPECT_EQ(nodeWithNoElement->GetClosestElement(),
            button->GetElement()->GetPseudoElement(kPseudoIdBefore));
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
  EXPECT_FALSE(button->IsIgnored());
  GetAXObjectCache().Remove(button->GetNode());
  EXPECT_TRUE(button->IsDetached());
  EXPECT_TRUE(button->IsIgnored());
  EXPECT_FALSE(button->IsIgnoredButIncludedInTree());
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
                   <div id="ignored_a" aria-hidden="true" lang="en-US"></div>
                   <p id="paragraph">hello<br id="br">there</p>
                   <span id="ignored_b" aria-hidden="true" lang="fr-CA"></span>
                   <button id="button">button</button>)HTML");

  AXObject* body = GetAXBodyObject();
  ASSERT_NE(nullptr, body);
  body->LoadInlineTextBoxes();
  const AXObject* input = GetAXObjectByElementId("input");
  ASSERT_NE(nullptr, input);
  ASSERT_NE(nullptr, GetAXObjectByElementId("ignored_a"));
  ASSERT_TRUE(GetAXObjectByElementId("ignored_a")->IsIgnored());
  const AXObject* paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, paragraph);
  const AXObject* br = GetAXObjectByElementId("br");
  ASSERT_NE(nullptr, br);
  ASSERT_NE(nullptr, GetAXObjectByElementId("ignored_b"));
  ASSERT_TRUE(GetAXObjectByElementId("ignored_b")->IsIgnored());
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
  ASSERT_NE(nullptr, paragraph->FirstChildIncludingIgnored()->ParentObject());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            paragraph->DeepestFirstChildIncludingIgnored()
                ->ParentObject()
                ->RoleValue());
  ASSERT_NE(nullptr,
            paragraph->DeepestLastChildIncludingIgnored()->ParentObject());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            paragraph->DeepestLastChildIncludingIgnored()
                ->ParentObject()
                ->RoleValue());

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
  ASSERT_NE(nullptr,
            button->DeepestFirstChildIncludingIgnored()->ParentObject());
  EXPECT_EQ(ax::mojom::Role::kStaticText,
            paragraph->DeepestFirstChildIncludingIgnored()
                ->ParentObject()
                ->RoleValue());
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
  EXPECT_TRUE(span_1->IsIgnoredButIncludedInTree());
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

  AXObject* root = GetAXRootObject();
  root->LoadInlineTextBoxes();
  const AXObject* body = GetAXBodyObject();
  ASSERT_EQ(3, body->ChildCountIncludingIgnored());
  ASSERT_EQ(1, body->ChildAtIncludingIgnored(1)->ChildCountIncludingIgnored());

  ASSERT_FALSE(root->IsIgnored());
  ASSERT_TRUE(body->IsIgnored());
  const AXObject* obj_a = GetAXObjectByElementId("A");
  ASSERT_NE(nullptr, obj_a);
  ASSERT_FALSE(obj_a->IsIgnored());
  const AXObject* obj_a_text = obj_a->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, obj_a_text);
  EXPECT_EQ(ax::mojom::Role::kStaticText, obj_a_text->RoleValue());
  const AXObject* obj_b = GetAXObjectByElementId("B");
  ASSERT_NE(nullptr, obj_b);
  ASSERT_FALSE(obj_b->IsIgnored());
  const AXObject* obj_b_text = obj_b->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, obj_b_text);
  EXPECT_EQ(ax::mojom::Role::kStaticText, obj_b_text->RoleValue());
  const AXObject* obj_c = GetAXObjectByElementId("C");
  ASSERT_NE(nullptr, obj_c);
  ASSERT_FALSE(obj_c->IsIgnored());
  const AXObject* obj_c_text = obj_c->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, obj_c_text);
  EXPECT_EQ(ax::mojom::Role::kStaticText, obj_c_text->RoleValue());
  const AXObject* obj_ignored = body->ChildAtIncludingIgnored(1);
  ASSERT_NE(nullptr, obj_ignored);
  ASSERT_TRUE(obj_ignored->IsIgnored());

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
  EXPECT_EQ(obj_a_text, obj_b->UnignoredPreviousInPreOrder()->ParentObject());
  EXPECT_EQ(obj_b_text, obj_b->NextInPreOrderIncludingIgnored());
  EXPECT_EQ(obj_b_text, obj_b->UnignoredNextInPreOrder());

  EXPECT_EQ(obj_ignored, obj_c->PreviousSiblingIncludingIgnored());
  EXPECT_EQ(obj_b, obj_c->UnignoredPreviousSibling());
  EXPECT_EQ(nullptr, obj_c->NextSiblingIncludingIgnored());
  EXPECT_EQ(nullptr, obj_c->UnignoredNextSibling());

  EXPECT_EQ(
      obj_b_text,
      obj_c->PreviousInPreOrderIncludingIgnored()->ParentObjectUnignored());
  EXPECT_EQ(obj_b_text,
            obj_c->UnignoredPreviousInPreOrder()->ParentObjectUnignored());
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
  ASSERT_FALSE(ax_text_before->IsIgnored());
  const AXObject* ax_div = GetAXObjectByElementId("div");
  ASSERT_NE(nullptr, ax_div);
  const AXObject* ax_paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, ax_paragraph);
  const AXObject* ax_text_inside = ax_paragraph->FirstChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_text_inside);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_text_inside->RoleValue());
  ASSERT_FALSE(ax_text_inside->IsIgnored());
  const AXObject* ax_text_after = ax_link->LastChildIncludingIgnored();
  ASSERT_NE(nullptr, ax_text_after);
  ASSERT_EQ(ax::mojom::Role::kStaticText, ax_text_after->RoleValue());
  ASSERT_FALSE(ax_text_after->IsIgnored());

  //
  // Test parent / child relationships individually. This is easier to debug
  // than printing the whole accessibility tree as a string and comparing with
  // an expected tree.
  //

  // BlockInInline changes |ax_body| not to be ignored. See the design doc at
  // crbug.com/716930 for more details.
  EXPECT_EQ(ax_body, ax_link->ParentObjectUnignored());
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

TEST_F(AccessibilityTest, NextOnLine) {
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
  ScopedFreezeAXCache freeze(GetAXObjectCache());

  // Force computation of next/previous on line data, since this is not the
  // regular flow.
  GetAXObjectCache().ComputeNodesOnLine(span1->GetLayoutObject());
  ASSERT_NE(nullptr, span1);

  const AXObject* next = span1->NextOnLine();
  ASSERT_NE(nullptr, next);
  EXPECT_EQ("b", next->GetClosestNode()->textContent());
}

TEST_F(AccessibilityTest, NextOnLineInlineBlock) {
  // Note the spans must be in the same line or we could get other unwanted
  // behavior. See https://crbug.com/1511390 for details.
  SetBodyInnerHTML(R"HTML(
    <div contenteditable="true" style="outline: 1px solid;">
        <div>first line</div>
        <span id="this">this line </span><span style="display: inline-block"><span style="display: block;">is</span></span><span> broken.</span>
        <div>last line</div>
    </div>
  )HTML");
  const AXObject* this_object = GetAXObjectByElementId("this");
  ScopedFreezeAXCache freeze(GetAXObjectCache());

  // Force computation of next/previous on line data, since this is not the
  // regular flow.
  GetAXObjectCache().ComputeNodesOnLine(this_object->GetLayoutObject());
  ASSERT_NE(nullptr, this_object);

  const AXObject* next = this_object->NextOnLine();
  ASSERT_NE(nullptr, next);
  EXPECT_EQ("is", next->GetNode()->textContent());

  next = next->NextOnLine();
  ASSERT_NE(nullptr, next);
  EXPECT_EQ(" broken.", next->GetClosestNode()->textContent());

  AXObject* prev = next->PreviousOnLine();
  ASSERT_NE(nullptr, prev);
  EXPECT_EQ("is", prev->GetNode()->textContent());

  prev = prev->PreviousOnLine();
  ASSERT_NE(nullptr, prev);
  EXPECT_EQ("this line ", prev->GetClosestNode()->textContent());
}

TEST_F(AccessibilityTest, NextAndPreviousOnLineInert) {
  // Spans need to be in the same line: see https://crbug.com/1511390.
  SetBodyInnerHTML(R"HTML(
    <div>
    <div>first line</div>
    <span id="span1">go </span><span inert>inert1</span><span inert>inert2</span><span>blue</span>
    <div>last line</div>
    </div>
  )HTML");
  const AXObject* span1 = GetAXObjectByElementId("span1");
  ScopedFreezeAXCache freeze(GetAXObjectCache());

  // Force computation of next/previous on line data, since this is not the
  // regular flow.
  GetAXObjectCache().ComputeNodesOnLine(span1->GetLayoutObject());
  ASSERT_NE(nullptr, span1);
  EXPECT_EQ("go ", span1->GetNode()->textContent());

  const AXObject* next = span1->NextOnLine();
  ASSERT_NE(nullptr, next);
  EXPECT_EQ("blue", next->GetClosestNode()->textContent());

  // Now we go backwards.

  const AXObject* previous = next->PreviousOnLine();
  EXPECT_EQ("go ", previous->GetClosestNode()->textContent());
}

TEST_F(AccessibilityTest, NextOnLineAriaHidden) {
  // Note the spans must be in the same line or we could get other unwanted
  // behavior. See https://crbug.com/1511390 for details.
  SetBodyInnerHTML(R"HTML(
    <div contenteditable="true" style="outline: 1px solid;">
        <div>first line</div>
        <span id="this">this line </span><span aria-hidden="true">is</span><span> broken.</span>
        <div>last line</div>
    </div>
  )HTML");
  const AXObject* this_object = GetAXObjectByElementId("this");
  ScopedFreezeAXCache freeze(GetAXObjectCache());

  // Force computation of next/previous on line data, since this is not the
  // regular flow.
  GetAXObjectCache().ComputeNodesOnLine(this_object->GetLayoutObject());
  ASSERT_NE(nullptr, this_object);

  const AXObject* next = this_object->NextOnLine();
  ASSERT_NE(nullptr, next);
  EXPECT_EQ(" broken.", next->GetClosestNode()->textContent());

  const AXObject* prev = next->PreviousOnLine();
  ASSERT_NE(nullptr, prev);
  EXPECT_EQ("this line ", prev->GetClosestNode()->textContent());
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

TEST_F(AccessibilityTest, TestSetRangeValueVideoControlSlider) {
  SetBodyInnerHTML(R"HTML(
      <body>
        <video id="vid" src="bear.webm"></video>
      </body>
      )HTML");

  AXObject* video = GetAXObjectByElementId("vid");

  Node* video_node = video->GetNode();
  ASSERT_NE(nullptr, video_node);
  auto* video_element = DynamicTo<HTMLMediaElement>(video_node);
  ASSERT_NE(nullptr, video_node);
  Node* timeline_node =
      video_element->GetMediaControls()->TimelineLayoutObject()->GetNode();
  ASSERT_NE(nullptr, timeline_node);
  AXObjectCache* cache = timeline_node->GetDocument().ExistingAXObjectCache();
  ASSERT_NE(nullptr, cache);
  AXObject* video_slider = cache->ObjectFromAXID(timeline_node->GetDomNodeId());

  ASSERT_NE(nullptr, video_slider);
  ASSERT_EQ(video_slider->RoleValue(), ax::mojom::blink::Role::kSlider);

  float value = 0.0f;
  EXPECT_TRUE(video_slider->ValueForRange(&value));
  EXPECT_EQ(0.0f, value);

  std::string value_to_set("1.0");
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kSetValue;
  action_data.value = value_to_set;
  action_data.target_node_id = video_slider->AXObjectID();

  EXPECT_TRUE(video_slider->PerformAction(action_data));

  EXPECT_TRUE(video_slider->ValueForRange(&value));
  EXPECT_EQ(1.0f, value);
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
      GetElementById("host")->AttachShadowRootForTesting(ShadowRootMode::kOpen);
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

TEST_F(AccessibilityTest, ListMarkerIsNotLineBreakingObject) {
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
  // Clear inline text boxes and refresh the tree.
  ui::AXMode mode(ui::kAXModeComplete);
  mode.set_mode(ui::AXMode::kInlineTextBoxes, false);
  ax_context_->SetAXMode(mode);
  GetAXObjectCache().MarkDocumentDirty();
  GetAXObjectCache().UpdateAXForAllDocuments();

  SetBodyInnerHTML(R"HTML(
     <select id="sel"><option>1</option></select>
    )HTML");

  AXObject* ax_select = GetAXObjectByElementId("sel");
  ax_select->SetNeedsToUpdateChildren();
  GetAXObjectCache().UpdateAXForAllDocuments();

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
  SetBodyInnerHTML(R"HTML(
  <svg width="800" height="600" xmlns="http://www.w3.org/2000/svg">
    <text id="t1" x="100">Text1</text>
    <text id="t2" x="50">Text1</text>
  </svg>)HTML");

  AXObject* text1 = GetAXObjectByElementId("t1");
  ASSERT_NE(text1, nullptr);
  AXObject* text2 = GetAXObjectByElementId("t2");
  ASSERT_NE(text2, nullptr);
  PhysicalRect bounds1 = text1->GetBoundsInFrameCoordinates();
  PhysicalRect bounds2 = text2->GetBoundsInFrameCoordinates();

  // Check if bounding boxes for SVG <text> respect to positioning
  // attributes such as 'x'.
  EXPECT_GT(bounds1.X(), bounds2.X());
}

TEST_F(AccessibilityTest, ComputeIsInertReason) {
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
    AXObject* object = GetAXObjectCache().Get(node);
    ASSERT_NE(object, nullptr);
    AXObject::IgnoredReasons reasons;
    ASSERT_TRUE(object->ComputeIsInert(&reasons));
    ASSERT_EQ(reasons.size(), 1u);
    ASSERT_EQ(reasons[0].reason, expectation);
  };
  auto AssertNotInert = [&](Node* node) {
    AXObject* object = GetAXObjectCache().Get(node);
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
  Element* element = document.QuerySelector(AtomicString("main"));
  while (element) {
    Node* node = element->firstChild();
    AXObject* ax_node = GetAXObjectCache().Get(node);

    // The text indicates the expected inert root, which is the nearest HTML
    // element ancestor with the 'inert' attribute.
    AtomicString selector(node->textContent().Impl());
    Element* inert_root = document.QuerySelector(selector);
    AXObject* ax_inert_root = GetAXObjectCache().Get(inert_root);

    AXObject::IgnoredReasons reasons;
    ASSERT_TRUE(ax_node->ComputeIsInert(&reasons));
    ASSERT_EQ(reasons.size(), 1u);
    ASSERT_EQ(reasons[0].reason, kAXInertSubtree);
    ASSERT_EQ(reasons[0].related_object.Get(), ax_inert_root);

    element = ElementTraversal::FirstChild(*element);
  }
}

TEST_F(AccessibilityTest, CanSetFocusInCanvasFallbackContent) {
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

TEST_F(AccessibilityTest, ScrollerFocusability) {
  SetBodyInnerHTML(R"HTML(
    <div id=scroller style="overflow:scroll;height:50px;">
      <div id=content style="height:1000px"></div>
    </div>
  )HTML");
  auto* scroller = GetAXObjectByElementId("scroller");
  auto* scroller_node = scroller->GetNode();
  EXPECT_TRUE(scroller_node);
  ASSERT_FALSE(scroller_node->IsFocused());

  ui::AXActionData action_data;
  action_data.action = ax::mojom::blink::Action::kDoDefault;
  const ui::AXTreeID div_child_tree_id = ui::AXTreeID::CreateNewAXTreeID();
  action_data.target_node_id = scroller->AXObjectID();
  action_data.child_tree_id = div_child_tree_id;
  scroller->PerformAction(action_data);

  ASSERT_TRUE(scroller_node->IsFocused());
}

TEST_F(AccessibilityTest, CanComputeAsNaturalParent) {
  SetBodyInnerHTML(R"HTML(M<img usemap="#map"><map name="map"><hr><progress>
    <div><input type="range">M)HTML");

  Element* elem = GetDocument().QuerySelector(AtomicString("img"));
  EXPECT_FALSE(AXObject::CanComputeAsNaturalParent(elem));
  elem = GetDocument().QuerySelector(AtomicString("map"));
  EXPECT_FALSE(AXObject::CanComputeAsNaturalParent(elem));
  elem = GetDocument().QuerySelector(AtomicString("hr"));
  EXPECT_FALSE(AXObject::CanComputeAsNaturalParent(elem));
  elem = GetDocument().QuerySelector(AtomicString("progress"));
  EXPECT_FALSE(AXObject::CanComputeAsNaturalParent(elem));
  elem = GetDocument().QuerySelector(AtomicString("input"));
  EXPECT_FALSE(AXObject::CanComputeAsNaturalParent(elem));
  elem = GetDocument().QuerySelector(AtomicString("div"));
  EXPECT_TRUE(AXObject::CanComputeAsNaturalParent(elem));
  elem = GetDocument().QuerySelector(AtomicString("input"));
  EXPECT_FALSE(AXObject::CanComputeAsNaturalParent(elem));
}

TEST_F(AccessibilityTest, StitchChildTree) {
  // Nodes that are descendants of the node at which a child tree was stitched
  // (the host node) make all descendants accessibility ignored, hence the
  // "ignored text" and "ignoredButton" nomenclature. The child tree will take
  // their place.
  //
  // If the host node is accessibility ignored, it should be altered to become
  // unignored, unless the host node was "ignored but included in tree" whereby
  // a change is not necessary.
  SetBodyInnerHTML(R"HTML(
      <!-- role="banner" so that it is included in the tree. -->
      <div id="div">
        <p id="paragraph">Ignored text.</P>
      </div>
      <input id="button" type="button" value="Test"
          style="display: none;" lang="fr-CA">  <!-- lang includes in tree -->
      <canvas id="canvas" aria-hidden="true" lang="fr-CA">
        <input id="ignoredButton" type="button" aria-hidden="false" value="Test">
        <p aria-hidden="false>More fallback content.</p>
      </canvas>)HTML");

  AXObject* root = GetAXRootObject();
  ASSERT_NE(nullptr, root);
  root->LoadInlineTextBoxes();

  AXObject* div = GetAXObjectByElementId("div");
  ASSERT_NE(nullptr, div);
  AXObject* paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_NE(nullptr, paragraph);
  AXObject* paragraph_text = paragraph->DeepestFirstChildIncludingIgnored();
  ASSERT_NE(nullptr, paragraph_text);
  ASSERT_EQ(paragraph_text->RoleValue(),
            ax::mojom::blink::Role::kInlineTextBox);
  AXObject* button = GetAXObjectByElementId("button");
  ASSERT_NE(nullptr, button);
  AXObject* canvas = GetAXObjectByElementId("canvas");
  ASSERT_NE(nullptr, canvas);
  AXObject* ignored_button = GetAXObjectByElementId("ignoredButton");
  ASSERT_NE(nullptr, ignored_button);

  EXPECT_TRUE(div->IsIncludedInTree());
  EXPECT_TRUE(div->IsVisible());
  EXPECT_EQ(1, div->ChildCountIncludingIgnored());
  EXPECT_TRUE(paragraph->IsIncludedInTree());
  EXPECT_TRUE(paragraph->IsVisible());
  EXPECT_TRUE(paragraph_text->IsIncludedInTree());
  EXPECT_TRUE(paragraph_text->IsVisible());
  EXPECT_TRUE(button->IsIgnored());
  EXPECT_FALSE(button->IsVisible());
  EXPECT_TRUE(canvas->IsIgnored());
  EXPECT_FALSE(canvas->IsVisible());
  EXPECT_EQ(1, canvas->ChildCountIncludingIgnored());
  EXPECT_TRUE(ignored_button->IsIncludedInTree());
  EXPECT_FALSE(ignored_button->IsVisible());

  ui::AXActionData action_data;
  action_data.action = ax::mojom::blink::Action::kStitchChildTree;

  const ui::AXTreeID div_child_tree_id = ui::AXTreeID::CreateNewAXTreeID();
  action_data.target_node_id = div->AXObjectID();
  action_data.child_tree_id = div_child_tree_id;
  div->PerformAction(action_data);

  const ui::AXTreeID button_child_tree_id = ui::AXTreeID::CreateNewAXTreeID();
  action_data.target_node_id = button->AXObjectID();
  action_data.child_tree_id = button_child_tree_id;
  button->PerformAction(action_data);

  const ui::AXTreeID canvas_child_tree_id = ui::AXTreeID::CreateNewAXTreeID();
  action_data.target_node_id = canvas->AXObjectID();
  action_data.child_tree_id = canvas_child_tree_id;
  canvas->PerformAction(action_data);

  ScopedFreezeAXCache freeze(GetAXObjectCache());

  ui::AXNodeData div_node_data;
  div->Serialize(&div_node_data, ui::AXMode::kScreenReader);
  ui::AXNodeData button_node_data;
  button->Serialize(&button_node_data, ui::AXMode::kScreenReader);
  ui::AXNodeData canvas_node_data;
  canvas->Serialize(&canvas_node_data, ui::AXMode::kScreenReader);

  EXPECT_EQ(div_child_tree_id.ToString(),
            div_node_data.GetStringAttribute(
                ax::mojom::blink::StringAttribute::kChildTreeId));
  EXPECT_EQ(button_child_tree_id.ToString(),
            button_node_data.GetStringAttribute(
                ax::mojom::blink::StringAttribute::kChildTreeId));
  EXPECT_EQ(canvas_child_tree_id.ToString(),
            canvas_node_data.GetStringAttribute(
                ax::mojom::blink::StringAttribute::kChildTreeId));

  // Fetch the hosting nodes again to ensure that we have their latest
  // incarnations, if any.
  div = GetAXObjectByElementId("div");
  ASSERT_NE(nullptr, div);
  button = GetAXObjectByElementId("button");
  ASSERT_NE(nullptr, button);
  canvas = GetAXObjectByElementId("canvas");
  ASSERT_NE(nullptr, canvas);

  EXPECT_TRUE(div->IsIncludedInTree());
  EXPECT_TRUE(div->IsVisible());
  EXPECT_EQ(0, div->ChildCountIncludingIgnored());
  EXPECT_TRUE(button->IsIncludedInTree())
      << "`button` should switch from ignored due to `display:none`, to "
         "included in the tree.";
  EXPECT_FALSE(button->IsVisible())
      << "The visibility state should not change, only the inclusion in the "
         "tree.";
  EXPECT_EQ(0, button->ChildCountIncludingIgnored());
  EXPECT_TRUE(canvas->IsIgnoredButIncludedInTree());
  EXPECT_FALSE(canvas->IsVisible())
      << "The visibility state should not change, only the inclusion in the "
         "tree.";
  EXPECT_EQ(0, canvas->ChildCountIncludingIgnored());

  // Try to re-create the pruned objects and check that they are still pruned.
  paragraph = GetAXObjectByElementId("paragraph");
  ASSERT_EQ(nullptr, paragraph);
  ignored_button = GetAXObjectByElementId("ignoredButton");
  ASSERT_EQ(nullptr, ignored_button);
}

TEST_F(AccessibilityTest, UpdateTreeUpdatesInheritedLiveProperty) {
  SetBodyInnerHTML(R"HTML(
      <main id="main">
        <p>some text</p>
        <div>
          <blockquote>
            <mark id="mark">
              nested text
            </mark>
          </blockquote>
        </div>
      </main>
      )HTML");

  AXObject* main = GetAXObjectByElementId("main");
  ASSERT_NE(nullptr, main);

  main->GetElement()->setAttribute(html_names::kAriaLiveAttr, "polite",
                                   ASSERT_NO_EXCEPTION);
  GetAXObjectCache().UpdateAXForAllDocuments();

  AXObject* mark = GetAXObjectByElementId("mark");
  ASSERT_NE(nullptr, mark);
  // Ensure the new live region status has propagated to a deep descendant.
  ASSERT_NE(nullptr, mark->ContainerLiveRegionStatus());
}

TEST_F(AccessibilityTest, UpdateTreeUpdatesInheritedAriaHiddenProperty) {
  SetBodyInnerHTML(R"HTML(
      <main id="main">
        <p>some text</p>
        <div>
          <blockquote>
            <mark id="mark">
              nested text
            </mark>
          </blockquote>
        </div>
      </main>
      )HTML");

  AXObject* main = GetAXObjectByElementId("main");
  ASSERT_NE(nullptr, main);

  main->GetElement()->setAttribute(html_names::kAriaHiddenAttr, "true",
                                   ASSERT_NO_EXCEPTION);
  GetAXObjectCache().UpdateAXForAllDocuments();

  AXObject* mark = GetAXObjectByElementId("mark");
  ASSERT_NE(nullptr, mark);
  // Ensure that aria-hidden has propagated to a deep descendant.
  ASSERT_TRUE(mark->IsAriaHidden());

  main = GetAXObjectByElementId("main");
  main->GetElement()->removeAttribute(html_names::kAriaHiddenAttr);
  GetAXObjectCache().UpdateAXForAllDocuments();

  // Ensure that clearing aria-hidden has propagated to a deep descendant.
  mark = GetAXObjectByElementId("mark");
  ASSERT_FALSE(mark->IsAriaHidden());
}

TEST_F(AccessibilityTest, UpdateTreeUpdatesInheritedInertProperty) {
  SetBodyInnerHTML(R"HTML(
      <main id="main">
        <p>some text</p>
        <div>
          <blockquote>
            <mark id="mark">
              nested text
            </mark>
          </blockquote>
        </div>
      </main>
      )HTML");

  AXObject* main = GetAXObjectByElementId("main");
  ASSERT_NE(nullptr, main);

  main->GetElement()->setAttribute(html_names::kInertAttr, "true",
                                   ASSERT_NO_EXCEPTION);
  GetAXObjectCache().UpdateAXForAllDocuments();

  AXObject* mark = GetAXObjectByElementId("mark");
  ASSERT_NE(nullptr, mark);
  // Ensure inertness has propagated to a deep descendant.
  ASSERT_TRUE(mark->IsInert());
}

TEST_F(AccessibilityTest, UpdateTreeUpdatesInheritedDisabledProperty) {
  SetBodyInnerHTML(R"HTML(
      <fieldset id="fieldset">
        <p>some text</p>
        <div>
          <blockquote>
            <mark id="mark">
              nested text
            </mark>
          </blockquote>
        </div>
      </fieldset>
      )HTML");

  AXObject* fieldset = GetAXObjectByElementId("fieldset");
  ASSERT_NE(nullptr, fieldset);

  fieldset->GetElement()->setAttribute(html_names::kAriaDisabledAttr, "true",
                                       ASSERT_NO_EXCEPTION);
  GetAXObjectCache().UpdateAXForAllDocuments();

  AXObject* mark = GetAXObjectByElementId("mark");
  ASSERT_NE(nullptr, mark);
  // Ensure that "ancestor is disabled" has propagated to a deep descendant.
  ASSERT_TRUE(mark->IsDescendantOfDisabledNode());
}

}  // namespace test
}  // namespace blink
