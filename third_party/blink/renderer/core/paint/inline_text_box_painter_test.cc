// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/inline_text_box_painter.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/testing/selection_sample.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"

using testing::ElementsAre;

namespace blink {

using InlineTextBoxPainterTest = PaintControllerPaintTest;

INSTANTIATE_PAINT_TEST_SUITE_P(InlineTextBoxPainterTest);

TEST_P(InlineTextBoxPainterTest, LineBreak) {
  SetBodyInnerHTML("<span style='font-size: 20px'>A<br>B<br>C</span>");
  // 0: view background, 1: A, 2: B, 3: C
  EXPECT_EQ(4u, ContentDisplayItems().size());

  GetDocument().GetFrame()->Selection().SelectAll();
  UpdateAllLifecyclePhasesForTest();
  // 0: view background, 1: A, 2: <br>, 3: B, 4: <br>, 5: C
  EXPECT_EQ(6u, ContentDisplayItems().size());
}

}  // namespace blink
