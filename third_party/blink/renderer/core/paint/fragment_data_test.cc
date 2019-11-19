// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/fragment_data.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class FragmentDataTest : public RenderingTest {
 protected:
  bool HasRareData(const FragmentData& data) { return !!data.rare_data_; }
};

TEST_F(FragmentDataTest, SelectionVisualRect) {
  FragmentData fragment;

  // Default SelectionVisualRect should not create RareData.
  fragment.SetVisualRect(IntRect(10, 20, 30, 400));
  fragment.SetSelectionVisualRect(IntRect());
  EXPECT_FALSE(HasRareData(fragment));
  EXPECT_EQ(IntRect(), fragment.SelectionVisualRect());

  // Non-Default SelectionVisualRect creates RareData.
  fragment.SetSelectionVisualRect(IntRect(1, 2, 3, 4));
  EXPECT_TRUE(HasRareData(fragment));
  EXPECT_EQ(IntRect(1, 2, 3, 4), fragment.SelectionVisualRect());

  // PaintProperties should store default SelectionVisualRect once it's created.
  fragment.SetSelectionVisualRect(IntRect());
  EXPECT_TRUE(HasRareData(fragment));
  EXPECT_EQ(IntRect(), fragment.SelectionVisualRect());
}

TEST_F(FragmentDataTest, PreEffectClipProperties) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        width: 400px; height: 400px; position: absolute;
        clip: rect(0, 50px, 100px, 0);
        clip-path: inset(0%);
      }
    </style>
    <div id='target'></div>
  )HTML");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  const ObjectPaintProperties* properties =
      target->FirstFragment().PaintProperties();
  EXPECT_TRUE(properties->ClipPathClip());
  EXPECT_TRUE(properties->CssClip());
  EXPECT_EQ(properties->ClipPathClip(), properties->CssClip()->Parent());
  EXPECT_EQ(properties->ClipPathClip()->Parent(),
            &target->FirstFragment().PreEffectProperties().Clip());
}

}  // namespace blink
