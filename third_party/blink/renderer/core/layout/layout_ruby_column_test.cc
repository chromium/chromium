// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_ruby_column.h"

#include "third_party/blink/renderer/core/layout/layout_ruby_base.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutRubyColumnTest : public RenderingTest {};

// crbug.com/1461993
TEST_F(LayoutRubyColumnTest, StylePropagation) {
  if (RuntimeEnabledFeatures::RubyLineBreakableEnabled()) {
    return;
  }
  SetBodyInnerHTML(R"HTML(<ruby id="target">Hello<rt>hola</rt></ruby>)HTML");
  auto* run_box = To<LayoutRubyColumn>(
      GetLayoutObjectByElementId("target")->SlowFirstChild());

  GetElementById("target")->setAttribute(html_names::kStyleAttr,
                                         AtomicString("background-color:red"));
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_FALSE(run_box->RubyBase()->NeedsLayout());
}

}  // namespace blink
