// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline.h"

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutSVGInlineTest : public RenderingTest {};

TEST_F(LayoutSVGInlineTest, IsChildAllowed) {
  SetBodyInnerHTML(R"HTML(
<svg>
<text>
<textPath><a id="anchor"><textPath />)HTML");
  GetDocument().UpdateStyleAndLayoutTree();

  auto* a = GetLayoutObjectByElementId("anchor");
  // The second <textPath> is not added.
  EXPECT_FALSE(a->SlowFirstChild());
}

}  // namespace blink
