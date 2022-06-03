// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/computed_style_css_value_mapping.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class ComputedStyleCSSValueMappingTest : public PageTestBase {};

TEST_F(ComputedStyleCSSValueMappingTest, GetVariablesOnOldStyle) {
  using css_test_helpers::RegisterProperty;

  GetDocument().body()->setInnerHTML("<div id=target style='--x:red'></div>");
  UpdateAllLifecyclePhasesForTest();

  Element* target = GetDocument().getElementById("target");
  ASSERT_TRUE(target);

  auto before = ComputedStyleCSSValueMapping::GetVariables(
      target->ComputedStyleRef(), GetDocument().GetPropertyRegistry());
  EXPECT_EQ(1u, before.size());
  EXPECT_TRUE(before.Contains("--x"));
  EXPECT_FALSE(before.Contains("--y"));

  RegisterProperty(GetDocument(), "--y", "<length>", "0px", false);

  // Registering a property should not affect variables reported on a
  // ComputedStyle created pre-registration.
  auto after = ComputedStyleCSSValueMapping::GetVariables(
      target->ComputedStyleRef(), GetDocument().GetPropertyRegistry());
  EXPECT_EQ(1u, after.size());
  EXPECT_TRUE(after.Contains("--x"));
  EXPECT_FALSE(after.Contains("--y"));
}

}  // namespace blink
