// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_replaced.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutReplacedTest : public RenderingTest {};

TEST_F(LayoutReplacedTest, InvalidateAfterAddingBorderRadius) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        width: 100px; height: 100px;
      }
    </style>
    <img id=target style="width: 100px; height: 100px"/>
  )HTML");
  Element* target_element = GetDocument().getElementById("target");
  LayoutObject* layout_object = target_element->GetLayoutObject();
  ASSERT_FALSE(layout_object->StyleRef().HasBorderRadius());

  target_element->setAttribute(html_names::kStyleAttr, "border-radius: 10px");

  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(layout_object->NeedsPaintPropertyUpdate());
}

}  // namespace blink
