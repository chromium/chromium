// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_text_content_element.h"

#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"

namespace blink {

class SVGTextContentElementTest : public EditingTestBase {};

TEST_F(SVGTextContentElementTest, selectSubStringNotCrash) {
  SetBodyContent("<svg><text style='visibility:hidden;'>Text</text></svg>");
  auto* elem = To<SVGTextContentElement>(GetDocument().QuerySelector("text"));
  VisiblePosition start = VisiblePosition::FirstPositionInNode(
      *const_cast<SVGTextContentElement*>(elem));
  EXPECT_TRUE(start.IsNull());
  // Pass if selecting hidden text is not crashed.
  elem->selectSubString(0, 1, ASSERT_NO_EXCEPTION);
}

}  // namespace blink
