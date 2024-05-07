// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_ruby_as_block.h"

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutRubyAsBlockTest : public RenderingTest {};

// crbug.com/338350893
TEST_F(LayoutRubyAsBlockTest, TextCombineCrash) {
  SetBodyInnerHTML(R"HTML(
      <div style="writing-mode:vertical-rl">
      <ruby id="target" style="display:block ruby; text-combine-upright:all;"></ruby>
      )HTML");
  auto* ruby = GetElementById("target");
  ruby->setInnerHTML("<ol></ol>a");
  UpdateAllLifecyclePhasesForTest();
  // Pass if no crashes.
}

}  // namespace blink
