// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/events/mutation_event_suppression_scope.h"

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class MutationEventSuppressionScopeTest : public RenderingTest {
 public:
  MutationEventSuppressionScopeTest() = default;
  ~MutationEventSuppressionScopeTest() override = default;
};

TEST_F(MutationEventSuppressionScopeTest, NestedScopes) {
  EXPECT_FALSE(GetDocument().ShouldSuppressMutationEvents());

  {
    MutationEventSuppressionScope outer_scope(GetDocument());
    EXPECT_TRUE(GetDocument().ShouldSuppressMutationEvents());

    {
      MutationEventSuppressionScope inner_scope(GetDocument());
      EXPECT_TRUE(GetDocument().ShouldSuppressMutationEvents());
    }

    EXPECT_TRUE(GetDocument().ShouldSuppressMutationEvents());
  }

  EXPECT_FALSE(GetDocument().ShouldSuppressMutationEvents());
}

}  // namespace blink
