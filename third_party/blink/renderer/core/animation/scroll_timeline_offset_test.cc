// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/scroll_timeline_offset.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/animation_test_helpers.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline_offset.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class ScrollTimelineOffsetTest : public PageTestBase {
 public:
  ScrollTimelineOffset* ScrollBasedOffsetFrom(String string) {
    return ScrollTimelineOffset::Create(
        animation_test_helpers::OffsetFromString(GetDocument(), string));
  }

  ScrollTimelineOffset* ElementBasedOffsetFrom(Element* target,
                                               String edge,
                                               double threshold) {
    auto* inner = CreateElementBasedOffset(target, edge, threshold);
    if (!inner)
      return nullptr;
    ScrollTimelineOffsetValue param;
    param.SetScrollTimelineElementBasedOffset(inner);
    return ScrollTimelineOffset::Create(param);
  }

 private:
  ScrollTimelineElementBasedOffset* CreateElementBasedOffset(Element* target,
                                                             String edge,
                                                             double threshold) {
    auto* value = ScrollTimelineElementBasedOffset::Create();
    value->setTarget(target);
    value->setEdge(edge);
    value->setThreshold(threshold);
    return value;
  }
};

TEST_F(ScrollTimelineOffsetTest, Equality) {
  GetDocument().body()->setInnerHTML("<i id=e1></i><i id=e2></i>");
  UpdateAllLifecyclePhasesForTest();
  Element* e1 = GetDocument().getElementById("e1");
  Element* e2 = GetDocument().getElementById("e2");

  ASSERT_TRUE(e1);
  ASSERT_TRUE(e2);

  EXPECT_EQ(*ScrollBasedOffsetFrom("10px"), *ScrollBasedOffsetFrom("10px"));
  EXPECT_EQ(*ScrollBasedOffsetFrom("10%"), *ScrollBasedOffsetFrom("10%"));
  EXPECT_EQ(*ElementBasedOffsetFrom(e1, "start", 0),
            *ElementBasedOffsetFrom(e1, "start", 0));

  // Different types of offset:
  EXPECT_NE(*ScrollBasedOffsetFrom("10px"),
            *ElementBasedOffsetFrom(e1, "start", 0));
  EXPECT_NE(*ElementBasedOffsetFrom(e1, "start", 0),
            *ScrollBasedOffsetFrom("10px"));

  // Different unit:
  EXPECT_NE(*ScrollBasedOffsetFrom("10px"), *ScrollBasedOffsetFrom("10%"));
  EXPECT_NE(*ScrollBasedOffsetFrom("10em"), *ScrollBasedOffsetFrom("10px"));

  // Different value:
  EXPECT_NE(*ScrollBasedOffsetFrom("10em"), *ScrollBasedOffsetFrom("50em"));
  EXPECT_NE(*ScrollBasedOffsetFrom("10px"), *ScrollBasedOffsetFrom("10.5px"));

  // Different target:
  EXPECT_NE(*ElementBasedOffsetFrom(e1, "start", 0),
            *ElementBasedOffsetFrom(e2, "start", 0));

  // Different edge:
  EXPECT_NE(*ElementBasedOffsetFrom(e1, "start", 0),
            *ElementBasedOffsetFrom(e1, "end", 0));

  // Different threshold:
  EXPECT_NE(*ElementBasedOffsetFrom(e1, "start", 0),
            *ElementBasedOffsetFrom(e1, "start", 1));
}

}  // namespace blink
