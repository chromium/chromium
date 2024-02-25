// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_progress.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutProgressTest : public RenderingTest {
 public:
  static bool IsAnimationTimerActive(const LayoutProgress* o) {
    return o->IsAnimationTimerActive();
  }
  static bool IsAnimatiing(const LayoutProgress* o) { return o->IsAnimating(); }
};

TEST_F(LayoutProgressTest, AnimationScheduling) {
  RenderingTest::SetBodyInnerHTML(
      "<progress id=\"progressElement\" value=0.3 max=1.0></progress>");
  UpdateAllLifecyclePhasesForTest();
  Element* progress_element = GetElementById("progressElement");
  auto* layout_progress =
      To<LayoutProgress>(progress_element->GetLayoutObject());

  // Verify that we do not schedule a timer for a determinant progress element
  EXPECT_FALSE(LayoutProgressTest::IsAnimationTimerActive(layout_progress));
  EXPECT_FALSE(LayoutProgressTest::IsAnimatiing(layout_progress));

  progress_element->removeAttribute(html_names::kValueAttr);
  UpdateAllLifecyclePhasesForTest();

  // Verify that we schedule a timer for an indeterminant progress element
  EXPECT_TRUE(LayoutProgressTest::IsAnimationTimerActive(layout_progress));
  EXPECT_TRUE(LayoutProgressTest::IsAnimatiing(layout_progress));

  progress_element->setAttribute(html_names::kValueAttr, AtomicString("0.7"));
  UpdateAllLifecyclePhasesForTest();

  // Verify that we cancel the timer for a determinant progress element
  EXPECT_FALSE(LayoutProgressTest::IsAnimationTimerActive(layout_progress));
  EXPECT_FALSE(LayoutProgressTest::IsAnimatiing(layout_progress));
}

}  // namespace blink
