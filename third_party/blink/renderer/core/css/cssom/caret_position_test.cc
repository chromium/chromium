// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/caret_position.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class CaretPositionTest : public PageTestBase {
 public:
  CaretPositionTest() = default;
};

TEST_F(CaretPositionTest, offsetNodeAndOffset) {
  SetBodyContent(
      "<div>"
      "<span id='s0'>s0</span>"
      "<span id='s1'>s1</span>"
      "<span id='s2'>s2</span>"
      "</div>");
  Element* const s0 = GetDocument().getElementById(AtomicString("s0"));
  auto* caret_position = MakeGarbageCollected<CaretPosition>(s0, 1);
  EXPECT_EQ(s0, caret_position->offsetNode());
  EXPECT_EQ(1u, caret_position->offset());
}

TEST_F(CaretPositionTest, getClientRect) {
  SetBodyContent(
      "<div>"
      "<span id='s0'>s0</span>"
      "<span id='s1'>s1</span>"
      "<span id='s2'>s2</span>"
      "</div>");
  Element* const s0 = GetDocument().getElementById(AtomicString("s0"));
  auto* caret_position = MakeGarbageCollected<CaretPosition>(s0, 1);
  EXPECT_EQ(s0, caret_position->offsetNode());
  EXPECT_EQ(1u, caret_position->offset());
  auto* range = Range::Create(GetDocument());
  range->setStart(s0, 1);
  range->setEnd(s0, 1);
  auto* range_client_rect = range->getBoundingClientRect();
  auto* caret_position_client_rect = caret_position->getClientRect();
  EXPECT_NE(nullptr, range_client_rect);
  EXPECT_NE(nullptr, caret_position_client_rect);
  EXPECT_EQ(*range_client_rect, *caret_position_client_rect);
}

}  // namespace blink
