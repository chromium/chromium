// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_ruby_base.h"

#include "third_party/blink/renderer/core/layout/layout_ruby_column.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutRubyBaseTest : public RenderingTest {};

// crbug.com/1503372

TEST_F(LayoutRubyBaseTest, AddChildNoBlockChildren) {
  SetBodyInnerHTML(R"HTML(
      <ruby id="target">abc<span style="display:table-cell"></span></ruby>
      )HTML");
  auto* run_box = To<LayoutRubyColumn>(
      GetLayoutObjectByElementId("target")->SlowFirstChild());
  auto* base_box = run_box->RubyBase();
  // Adding a table-cell should not move the prior Text to an anonymous block.
  EXPECT_TRUE(base_box->FirstChild()->IsText());
  EXPECT_EQ(EDisplay::kInlineTable,
            base_box->FirstChild()->NextSibling()->StyleRef().Display());
}

}  // namespace blink
