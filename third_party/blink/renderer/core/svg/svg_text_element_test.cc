// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/print_context.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class SVGTextElementTest : public RenderingTest {};

TEST_F(SVGTextElementTest, CreateLayoutObject) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;
  SetBodyInnerHTML(R"HTML(
      <svg xmlns="http://www.w3.org/2000/svg" width="400" height="400">
        <text id="t">Foo</text>
      </svg>)HTML");

  ScopedPrintContext print_context(&GetDocument().View()->GetFrame());
  print_context->BeginPrintMode(800, 600);
  const auto* object = GetLayoutObjectByElementId("t");
  // We use LayoutNGSVGText even in the printing mode, which forces the
  // legacy layout for now.
  EXPECT_TRUE(object->IsNGSVGText());
}

}  // namespace blink
