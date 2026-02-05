// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_a_element.h"

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class SVGAElementTest : public RenderingTest {};

// crbug.com/481373475
TEST_F(SVGAElementTest, DefaultEventHandlerCrash) {
  SetBodyInnerHTML(R"HTML(
<svg width=100 height=100>
<text x=10 y=30><a id="a" href="">link-1</a></text>
</svg>
)HTML");
  auto* target = GetElementById("a");
  target->DispatchSimulatedClick(nullptr,
                                 SimulatedClickCreationScope::kFromScript);
  // Pass if no crashes.
}

}  // namespace blink
