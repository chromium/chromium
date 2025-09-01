// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class LayoutTextControlInnerEditorTest : public RenderingTest {};

TEST_F(LayoutTextControlInnerEditorTest, AddChildWithoutTrailingLf) {
  if (!RuntimeEnabledFeatures::TextareaMultipleIfcsEnabled()) {
    return;
  }
  SetBodyInnerHTML("<textarea id=ta>foo\nbar</textarea>");
  const auto* ta = GetLayoutBoxByElementId("ta");
  const auto* inner_editor = To<LayoutBlockFlow>(ta->FirstChildBox());
  ASSERT_TRUE(inner_editor);

  // The first anonymous block should have a LayoutText and a LayoutBR.
  const auto* child = inner_editor->FirstChild();
  ASSERT_TRUE(child);
  EXPECT_TRUE(child->IsAnonymousBlockFlow());
  const auto* grand_child = child->SlowFirstChild();
  ASSERT_TRUE(grand_child);
  EXPECT_TRUE(grand_child->IsText());
  grand_child = grand_child->NextSibling();
  ASSERT_TRUE(grand_child);
  EXPECT_TRUE(grand_child->IsBR());
  EXPECT_FALSE(grand_child->NextSibling());

  // The second anonymous block should have only a LayoutText.
  child = child->NextSibling();
  ASSERT_TRUE(child);
  EXPECT_TRUE(child->IsAnonymousBlockFlow());
  grand_child = child->SlowFirstChild();
  ASSERT_TRUE(grand_child);
  EXPECT_TRUE(grand_child->IsText());
  EXPECT_FALSE(grand_child->NextSibling());

  child = child->NextSibling();
  EXPECT_FALSE(child);
}

TEST_F(LayoutTextControlInnerEditorTest, AddChildWithTrailingLf) {
  if (!RuntimeEnabledFeatures::TextareaMultipleIfcsEnabled()) {
    return;
  }
  SetBodyInnerHTML("<textarea id=ta>foo\nbar\n</textarea>");
  const auto* ta = GetLayoutBoxByElementId("ta");
  const auto* inner_editor = To<LayoutBlockFlow>(ta->FirstChildBox());
  ASSERT_TRUE(inner_editor);

  // The first anonymous block should have a LayoutText and a LayoutBR.
  const auto* child = inner_editor->FirstChild();
  ASSERT_TRUE(child);
  EXPECT_TRUE(child->IsAnonymousBlockFlow());
  const auto* grand_child = child->SlowFirstChild();
  ASSERT_TRUE(grand_child);
  EXPECT_TRUE(grand_child->IsText());
  grand_child = grand_child->NextSibling();
  ASSERT_TRUE(grand_child);
  EXPECT_TRUE(grand_child->IsBR());
  EXPECT_FALSE(grand_child->NextSibling());

  // The second anonymous block should have a LayoutText and a LayoutBR.
  child = child->NextSibling();
  ASSERT_TRUE(child);
  EXPECT_TRUE(child->IsAnonymousBlockFlow());
  grand_child = child->SlowFirstChild();
  ASSERT_TRUE(grand_child);
  EXPECT_TRUE(grand_child->IsText());
  grand_child = grand_child->NextSibling();
  ASSERT_TRUE(grand_child);
  EXPECT_TRUE(grand_child->IsBR());
  EXPECT_FALSE(grand_child->NextSibling());

  // The third anonymous block should have only a placeholder break.
  child = child->NextSibling();
  ASSERT_TRUE(child);
  EXPECT_TRUE(child->IsAnonymousBlockFlow());
  grand_child = child->SlowFirstChild();
  ASSERT_TRUE(grand_child);
  EXPECT_TRUE(grand_child->IsBR());
  EXPECT_FALSE(grand_child->NextSibling());

  EXPECT_FALSE(child->NextSibling());
}

TEST_F(LayoutTextControlInnerEditorTest, RemoveChildWithoutTrailingLf) {
  if (!RuntimeEnabledFeatures::TextareaMultipleIfcsEnabled()) {
    return;
  }
  SetBodyInnerHTML("<textarea id=ta>foo\nbar</textarea>");
  const auto* ta = GetLayoutBoxByElementId("ta");
  const auto* inner_editor = To<LayoutBlockFlow>(ta->FirstChildBox());
  ASSERT_TRUE(inner_editor);
  auto* inner_editor_element = To<Element>(inner_editor->GetNode());

  // There should be two anonymous blocks before editing.
  const auto* child = inner_editor->FirstChild();
  EXPECT_TRUE(child->IsAnonymousBlockFlow());
  child = child->NextSibling();
  EXPECT_TRUE(child);
  EXPECT_TRUE(child->IsAnonymousBlockFlow());
  EXPECT_FALSE(child->NextSibling());

  // Remove "bar".
  auto* child_node = inner_editor_element->lastChild();
  ASSERT_TRUE(child_node);
  EXPECT_TRUE(child_node->IsTextNode());
  child_node->remove();
  UpdateAllLifecyclePhasesForTest();

  // There should be one anonymous block after editing.
  child = inner_editor->FirstChild();
  EXPECT_TRUE(child->IsAnonymousBlockFlow());
  EXPECT_FALSE(child->NextSibling());
}

// crbug.com/416795534
TEST_F(LayoutTextControlInnerEditorTest, AddChildBeforeTestRenderingHolder) {
  if (!RuntimeEnabledFeatures::TextareaMultipleIfcsEnabled()) {
    return;
  }
  SetBodyInnerHTML("<textarea id=ta>A\n</textarea>");
  GetElementById("ta")->Focus();
  GetDocument().execCommand(
      "inserthtml", false,
      "<style> :first-letter { max-width: initial; }</style>",
      ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();
  // Pass if no crashes.
}

// crbug.com/422003155
TEST_F(LayoutTextControlInnerEditorTest, EnableDynamic) {
  SetBodyInnerHTML("<textarea id=ta disabled></textarea>");
  auto* ta = GetElementById("ta");
  const auto* inner_editor =
      To<LayoutBlockFlow>(ta->GetLayoutBox()->FirstChildBox());
  ASSERT_TRUE(inner_editor);

  ta->SetBooleanAttribute(html_names::kDisabledAttr, false);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_GT(inner_editor->StitchedSize().height, LayoutUnit());
}

}  // namespace blink
