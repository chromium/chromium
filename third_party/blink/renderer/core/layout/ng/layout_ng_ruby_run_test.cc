// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_ruby_run.h"

#include "third_party/blink/renderer/core/layout/ng/layout_ng_ruby_base.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutNGRubyRunTest : public RenderingTest {};

// crbug.com/1461993
TEST_F(LayoutNGRubyRunTest, StylePropagation) {
  SetBodyInnerHTML(R"HTML(<ruby id="target">Hello<rt>hola</rt></ruby>)HTML");
  auto* run_box = To<LayoutNGRubyRun>(
      GetLayoutObjectByElementId("target")->SlowFirstChild());

  GetElementById("target")->setAttribute(html_names::kStyleAttr,
                                         AtomicString("background-color:red"));
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_FALSE(run_box->RubyBase()->NeedsLayout());
}

}  // namespace blink
