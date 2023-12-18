/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/overflow_model.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

PhysicalRect InitialScrollableOverflow() {
  return PhysicalRect(10, 10, 80, 80);
}

PhysicalRect InitialVisualOverflow() {
  return PhysicalRect(0, 0, 100, 100);
}

class BoxOverflowModelTest : public testing::Test {
 protected:
  BoxOverflowModelTest()
      : scrollable_overflow_(InitialScrollableOverflow()),
        visual_overflow_(InitialVisualOverflow()) {}
  test::TaskEnvironment task_environment_;
  BoxScrollableOverflowModel scrollable_overflow_;
  BoxVisualOverflowModel visual_overflow_;
};

TEST_F(BoxOverflowModelTest, InitialOverflowRects) {
  EXPECT_EQ(InitialScrollableOverflow(),
            scrollable_overflow_.ScrollableOverflowRect());
  EXPECT_EQ(InitialVisualOverflow(), visual_overflow_.SelfVisualOverflowRect());
  EXPECT_TRUE(visual_overflow_.ContentsVisualOverflowRect().IsEmpty());
}

TEST_F(BoxOverflowModelTest, AddSelfVisualOverflowOutsideExpandsRect) {
  visual_overflow_.AddSelfVisualOverflow(PhysicalRect(150, -50, 10, 10));
  EXPECT_EQ(PhysicalRect(0, -50, 160, 150),
            visual_overflow_.SelfVisualOverflowRect());
}

TEST_F(BoxOverflowModelTest, AddSelfVisualOverflowInsideDoesNotAffectRect) {
  visual_overflow_.AddSelfVisualOverflow(PhysicalRect(0, 10, 90, 90));
  EXPECT_EQ(InitialVisualOverflow(), visual_overflow_.SelfVisualOverflowRect());
}

TEST_F(BoxOverflowModelTest, AddSelfVisualOverflowEmpty) {
  BoxVisualOverflowModel visual_overflow(PhysicalRect(0, 0, 600, 0));
  visual_overflow.AddSelfVisualOverflow(PhysicalRect(100, -50, 100, 100));
  visual_overflow.AddSelfVisualOverflow(PhysicalRect(300, 300, 0, 10000));
  EXPECT_EQ(PhysicalRect(100, -50, 100, 100),
            visual_overflow.SelfVisualOverflowRect());
}

TEST_F(BoxOverflowModelTest,
       AddSelfVisualOverflowDoesNotAffectContentsVisualOverflow) {
  visual_overflow_.AddSelfVisualOverflow(PhysicalRect(300, 300, 300, 300));
  EXPECT_TRUE(visual_overflow_.ContentsVisualOverflowRect().IsEmpty());
}

TEST_F(BoxOverflowModelTest, AddContentsVisualOverflowFirstCall) {
  visual_overflow_.AddContentsVisualOverflow(PhysicalRect(0, 0, 10, 10));
  EXPECT_EQ(PhysicalRect(0, 0, 10, 10),
            visual_overflow_.ContentsVisualOverflowRect());
}

TEST_F(BoxOverflowModelTest, AddContentsVisualOverflowUnitesRects) {
  visual_overflow_.AddContentsVisualOverflow(PhysicalRect(0, 0, 10, 10));
  visual_overflow_.AddContentsVisualOverflow(PhysicalRect(80, 80, 10, 10));
  EXPECT_EQ(PhysicalRect(0, 0, 90, 90),
            visual_overflow_.ContentsVisualOverflowRect());
}

TEST_F(BoxOverflowModelTest, AddContentsVisualOverflowRectWithinRect) {
  visual_overflow_.AddContentsVisualOverflow(PhysicalRect(0, 0, 10, 10));
  visual_overflow_.AddContentsVisualOverflow(PhysicalRect(2, 2, 5, 5));
  EXPECT_EQ(PhysicalRect(0, 0, 10, 10),
            visual_overflow_.ContentsVisualOverflowRect());
}

TEST_F(BoxOverflowModelTest, AddContentsVisualOverflowEmpty) {
  visual_overflow_.AddContentsVisualOverflow(PhysicalRect(0, 0, 10, 10));
  visual_overflow_.AddContentsVisualOverflow(PhysicalRect(20, 20, 0, 0));
  EXPECT_EQ(PhysicalRect(0, 0, 10, 10),
            visual_overflow_.ContentsVisualOverflowRect());
}

TEST_F(BoxOverflowModelTest, MoveAffectsSelfVisualOverflow) {
  visual_overflow_.Move(LayoutUnit(500), LayoutUnit(100));
  EXPECT_EQ(PhysicalRect(500, 100, 100, 100),
            visual_overflow_.SelfVisualOverflowRect());
}

TEST_F(BoxOverflowModelTest, MoveAffectsContentsVisualOverflow) {
  visual_overflow_.AddContentsVisualOverflow(PhysicalRect(0, 0, 10, 10));
  visual_overflow_.Move(LayoutUnit(500), LayoutUnit(100));
  EXPECT_EQ(PhysicalRect(500, 100, 10, 10),
            visual_overflow_.ContentsVisualOverflowRect());
}

}  // anonymous namespace
}  // namespace blink
