// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class NGLayoutOverflowCalculatorTest : public RenderingTest {};

TEST_F(NGLayoutOverflowCalculatorTest,
       NewLayoutOverflowDifferentAndAlreadyScrollsFlex) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div style="display: flex; width: 100px; height: 100px; padding: 10px; overflow: auto;">
      <div style="min-width: 120px; height: 10px;"></div>
    </div>
  )HTML");
  RunDocumentLifecycle();

  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kNewLayoutOverflowDifferentAndAlreadyScrollsFlex));
}

TEST_F(NGLayoutOverflowCalculatorTest,
       NewLayoutOverflowDifferentAndAlreadyScrollsBlock) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div style="width: 100px; height: 100px; padding: 10px; overflow: auto;">
      <div style="min-width: 120px; height: 10px;"></div>
    </div>
  )HTML");
  RunDocumentLifecycle();

  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kNewLayoutOverflowDifferentAndAlreadyScrollsBlock));
}

TEST_F(NGLayoutOverflowCalculatorTest, NewLayoutOverflowDifferentFlex) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div style="display: flex; width: 100px; height: 100px; padding: 10px; overflow: auto;">
      <div style="min-width: 110px; height: 10px;"></div>
    </div>
  )HTML");
  RunDocumentLifecycle();

  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kNewLayoutOverflowDifferentFlex));
}

TEST_F(NGLayoutOverflowCalculatorTest, NewLayoutOverflowDifferentBlock) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div style="width: 100px; height: 100px; padding: 10px; overflow: auto;">
      <div style="min-width: 110px; height: 10px;"></div>
    </div>
  )HTML");
  RunDocumentLifecycle();

  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kNewLayoutOverflowDifferentBlock));
}

}  // namespace blink
