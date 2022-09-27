// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/fragment_data.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class FragmentDataTest : public RenderingTest {};

TEST_F(FragmentDataTest, PreClip) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        width: 400px; height: 400px; position: absolute;
        clip: rect(0, 50px, 100px, 0);
        clip-path: inset(0%);
        filter: blur(10px);
      }
    </style>
    <div id='target'></div>
  )HTML");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  const ObjectPaintProperties* properties =
      target->FirstFragment().PaintProperties();
  EXPECT_TRUE(properties->ClipPathClip());
  EXPECT_TRUE(properties->CssClip());
  EXPECT_TRUE(properties->PixelMovingFilterClipExpander());
  EXPECT_EQ(properties->CssClip(),
            properties->PixelMovingFilterClipExpander()->Parent());
  EXPECT_EQ(properties->ClipPathClip(), properties->CssClip()->Parent());
  EXPECT_EQ(properties->ClipPathClip()->Parent(),
            &target->FirstFragment().PreClip());
}

}  // namespace blink
