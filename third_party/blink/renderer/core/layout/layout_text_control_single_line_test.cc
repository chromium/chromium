// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_text_control_single_line.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

namespace {

class LayoutTextControlSingleLineTest : public RenderingTest {};

TEST_F(LayoutTextControlSingleLineTest, VisualOverflowCleared) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #input {
        width: 50px; height: 50px; box-shadow: 5px 5px 5px black;
      }
    </style>
    <input id=input type="text"></input.
  )HTML");
  auto* input =
      ToLayoutTextControlSingleLine(GetLayoutObjectByElementId("input"));
#if defined(OS_MACOSX)
  EXPECT_EQ(LayoutRect(-3, -3, 72, 72), input->SelfVisualOverflowRect());
#else
  EXPECT_EQ(LayoutRect(-3, -3, 70, 72), input->SelfVisualOverflowRect());
#endif
  To<Element>(input->GetNode())
      ->setAttribute(html_names::kStyleAttr, "box-shadow: initial");
  GetDocument().View()->UpdateAllLifecyclePhases(
      DocumentLifecycle::LifecycleUpdateReason::kTest);
#if defined(OS_MACOSX)
  EXPECT_EQ(LayoutRect(0, 0, 56, 56), input->SelfVisualOverflowRect());
#else
  EXPECT_EQ(LayoutRect(0, 0, 54, 56), input->SelfVisualOverflowRect());
#endif
}

}  // anonymous namespace

}  // namespace blink
