// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/computed_style_property_map.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class ComputedStylePropertyMapTest : public PageTestBase {
 public:
  ComputedStylePropertyMapTest() = default;

 protected:
  ComputedStylePropertyMap* SetBodyStyle(const char* style) {
    GetDocument().body()->setAttribute(html_names::kStyleAttr,
                                       AtomicString(style));
    UpdateAllLifecyclePhasesForTest();
    return MakeGarbageCollected<ComputedStylePropertyMap>(GetDocument().body());
  }
};

TEST_F(ComputedStylePropertyMapTest, TransformMatrixZoom) {
  ComputedStylePropertyMap* map =
      SetBodyStyle("transform:matrix(1, 0, 0, 1, 100, 100);zoom:2");
  CSSStyleValue* style_value = map->get(GetDocument().GetExecutionContext(),
                                        "transform", ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(style_value);
  EXPECT_EQ("matrix(1, 0, 0, 1, 100, 100)", style_value->toString());
}

TEST_F(ComputedStylePropertyMapTest, TransformMatrix3DZoom) {
  ComputedStylePropertyMap* map = SetBodyStyle(
      "transform:matrix3d(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 100, 100, 100, "
      "1);zoom:2");
  CSSStyleValue* style_value = map->get(GetDocument().GetExecutionContext(),
                                        "transform", ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(style_value);
  EXPECT_EQ("matrix3d(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 100, 100, 100, 1)",
            style_value->toString());
}

TEST_F(ComputedStylePropertyMapTest, TransformPerspectiveZoom) {
  ComputedStylePropertyMap* map =
      SetBodyStyle("transform:perspective(100px);zoom:2");
  CSSStyleValue* style_value = map->get(GetDocument().GetExecutionContext(),
                                        "transform", ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(style_value);
  EXPECT_EQ("perspective(100px)", style_value->toString());
}

TEST_F(ComputedStylePropertyMapTest, TopWithAnchor) {
  ComputedStylePropertyMap* map =
      SetBodyStyle("position: absolute; top: anchor(bottom, 17px);");
  CSSStyleValue* style_value =
      map->get(GetDocument().GetExecutionContext(), "top", ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(style_value);
  EXPECT_EQ("anchor(bottom, 17px)", style_value->toString());
}

}  // namespace blink
