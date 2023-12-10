// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/svg/svg_text_layout_attributes_builder.h"

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class SvgTextLayoutAttributesBuilderTest : public RenderingTest {};

TEST_F(SvgTextLayoutAttributesBuilderTest, TextPathCrash) {
  SetBodyInnerHTML(R"HTML(
<svg>
<text>
<textPath>
<a>
<textPath />)HTML");
  UpdateAllLifecyclePhasesForTest();
  // Pass if no crashes.
}

}  // namespace blink
