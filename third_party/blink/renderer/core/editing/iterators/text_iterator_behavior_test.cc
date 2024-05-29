// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/iterators/text_iterator_behavior.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(TextIteratorBehaviorTest, Basic) {
  EXPECT_TRUE(TextIteratorBehavior() == TextIteratorBehavior());
  EXPECT_FALSE(TextIteratorBehavior() != TextIteratorBehavior());
  EXPECT_EQ(TextIteratorBehavior::Builder()
                .SetEmitsImageAltText(true)
                .Build(),
            TextIteratorBehavior::Builder()
                .SetEmitsImageAltText(true)
                .Build());
}

TEST(TextIteratorBehaviorTest, Values) {
  EXPECT_TRUE(TextIteratorBehavior::Builder()
                  .SetDoesNotBreakAtReplacedElement(true)
                  .Build()
                  .DoesNotBreakAtReplacedElement());
  EXPECT_TRUE(TextIteratorBehavior::Builder()
                  .SetEmitsCharactersBetweenAllVisiblePositions(true)
                  .Build()
                  .EmitsCharactersBetweenAllVisiblePositions());
  EXPECT_TRUE(TextIteratorBehavior::Builder()
                  .SetEmitsImageAltText(true)
                  .Build()
                  .EmitsImageAltText());
  EXPECT_TRUE(TextIteratorBehavior::Builder()
                  .SetEmitsSpaceForNbsp(true)
                  .Build()
                  .EmitsSpaceForNbsp());
  EXPECT_TRUE(TextIteratorBehavior::Builder()
                  .SetEmitsObjectReplacementCharacter(true)
                  .Build()
                  .EmitsObjectReplacementCharacter());
  EXPECT_TRUE(TextIteratorBehavior::Builder()
                  .SetEmitsOriginalText(true)
                  .Build()
                  .EmitsOriginalText());
  EXPECT_TRUE(TextIteratorBehavior::Builder()
                  .SetEntersOpenShadowRoots(true)
                  .Build()
                  .EntersOpenShadowRoots());
  EXPECT_TRUE(TextIteratorBehavior::Builder()
                  .SetEntersTextControls(true)
                  .Build()
                  .EntersTextControls());
  EXPECT_TRUE(TextIteratorBehavior::Builder()
                  .SetExcludeAutofilledValue(true)
                  .Build()
                  .ExcludeAutofilledValue());
  EXPECT_TRUE(TextIteratorBehavior::Builder()
                  .SetForSelectionToString(true)
                  .Build()
                  .ForSelectionToString());
  EXPECT_TRUE(TextIteratorBehavior::Builder()
                  .SetForWindowFind(true)
                  .Build()
                  .ForWindowFind());
  EXPECT_TRUE(TextIteratorBehavior::Builder()
                  .SetIgnoresStyleVisibility(true)
                  .Build()
                  .IgnoresStyleVisibility());
  EXPECT_TRUE(TextIteratorBehavior::Builder()
                  .SetStopsOnFormControls(true)
                  .Build()
                  .StopsOnFormControls());
  EXPECT_TRUE(TextIteratorBehavior::Builder()
                  .SetDoesNotEmitSpaceBeyondRangeEnd(true)
                  .Build()
                  .DoesNotEmitSpaceBeyondRangeEnd());
  EXPECT_TRUE(TextIteratorBehavior::Builder()
                  .SetSuppressesExtraNewlineEmission(true)
                  .Build()
                  .SuppressesExtraNewlineEmission());
  EXPECT_TRUE(TextIteratorBehavior::Builder()
                  .SetIgnoresCSSTextTransforms(true)
                  .Build()
                  .IgnoresCSSTextTransforms());
}

}  // namespace blink
