// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"

#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

namespace blink {

class PositionWithAffinityTest : public EditingTestBase {};

TEST_F(PositionWithAffinityTest, OperatorBool) {
  SetBodyContent("foo");
  EXPECT_FALSE(static_cast<bool>(PositionWithAffinity()));
  EXPECT_TRUE(static_cast<bool>(
      PositionWithAffinity(Position(GetDocument().body(), 0))));
}

}  // namespace blink
