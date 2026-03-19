// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_marquee_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class HTMLMarqueeElementTest : public PageTestBase {};

TEST_F(HTMLMarqueeElementTest, GetMetricsWithExplicitSizesAndContent) {
  // Set up a marquee element with explicit width and height attributes,
  // and an inner div with its own explicit dimensions.
  SetBodyInnerHTML(R"HTML(
    <marquee id="testMarquee" width="200" height="50">
      <div id="contentDiv" style="width: 150px; height: 25px;">Content</div>
    </marquee>
  )HTML");
  auto* marquee = To<HTMLMarqueeElement>(GetElementById("testMarquee"));
  ASSERT_TRUE(marquee);

  HTMLMarqueeElement::Metrics metrics = marquee->GetMetrics();
  // The marquee's own dimensions should reflect the 'width' and 'height'
  // attributes.
  EXPECT_EQ(200.0, metrics.marquee_width);
  EXPECT_EQ(50.0, metrics.marquee_height);
  // The content dimensions (`content_width`, `content_height`) are derived from
  // the `mover_` element's computed style after it's temporarily set to
  // `-webkit-max-content`. Since the slotted content (`#contentDiv`) has
  // explicit dimensions, the `mover_`'s max-content size will be determined
  // by these explicit dimensions.
  EXPECT_EQ(150.0, metrics.content_width);
  EXPECT_EQ(25.0, metrics.content_height);
}

TEST_F(HTMLMarqueeElementTest, GetMetricsForEmptyMarquee) {
  // Test a marquee with no explicit dimensions or content.
  SetBodyInnerHTML(R"HTML(
    <marquee id="empty" style="display:inline; width:auto; height:auto;">
    </marquee>
  )HTML");
  auto* marquee = To<HTMLMarqueeElement>(GetElementById("empty"));
  ASSERT_TRUE(marquee);

  HTMLMarqueeElement::Metrics metrics = marquee->GetMetrics();
  // An empty marquee with no explicit dimensions will likely have zero computed
  // width and height, leading to zero metrics.
  EXPECT_EQ(metrics.marquee_width, 0.0);
  EXPECT_EQ(metrics.marquee_height, 0.0);
  EXPECT_EQ(metrics.content_width, 0.0);
  EXPECT_EQ(metrics.content_height, 0.0);
}

}  // namespace blink
