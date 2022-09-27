// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_list_marker.h"

#include "third_party/blink/renderer/core/layout/layout_list_item.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class LayoutListMarkerTest : public RenderingTest,
                             private ScopedLayoutNGForTest {
 public:
  // LayoutListMarker is for legacy layout only
  LayoutListMarkerTest() : ScopedLayoutNGForTest(false) {}
};

// https://crbug.com/1167174
TEST_F(LayoutListMarkerTest, ListStyleTypeNoneTextAlternative) {
  SetBodyInnerHTML(R"HTML(
    <style>
      li {
        list-style-type: none;
        list-style-image: linear-gradient(black, white);
      }
    </style>
    <ul>
      <li id="target">foo</li>
    </ul>
  )HTML");

  Element* target = GetElementById("target");
  LayoutObject* marker =
      ListMarker::MarkerFromListItem(target->GetLayoutObject());

  // Should not crash
  EXPECT_EQ("", To<LayoutListMarker>(marker)->TextAlternative());
}

}  // namespace blink
