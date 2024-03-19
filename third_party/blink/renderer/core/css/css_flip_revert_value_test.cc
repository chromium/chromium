// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_flip_revert_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using CSSFlipRevertValue = cssvalue::CSSFlipRevertValue;

TEST(CSSFlipRevertValueTest, CssText) {
  EXPECT_EQ("-internal-flip-revert(left)",
            MakeGarbageCollected<CSSFlipRevertValue>(CSSPropertyID::kLeft,
                                                     TryTacticTransform())
                ->CssText());
}

TEST(CSSFlipRevertValueTest, Equals) {
  EXPECT_EQ(*MakeGarbageCollected<CSSFlipRevertValue>(CSSPropertyID::kLeft,
                                                      TryTacticTransform()),
            *MakeGarbageCollected<CSSFlipRevertValue>(CSSPropertyID::kLeft,
                                                      TryTacticTransform()));
}

TEST(CSSFlipRevertValueTest, NotEquals) {
  EXPECT_NE(*MakeGarbageCollected<CSSFlipRevertValue>(CSSPropertyID::kLeft,
                                                      TryTacticTransform()),
            *MakeGarbageCollected<CSSFlipRevertValue>(CSSPropertyID::kRight,
                                                      TryTacticTransform()));
  EXPECT_NE(
      *MakeGarbageCollected<CSSFlipRevertValue>(CSSPropertyID::kLeft,
                                                TryTacticTransform()),
      *MakeGarbageCollected<CSSFlipRevertValue>(
          CSSPropertyID::kLeft,
          TryTacticTransform(TryTacticList{
              TryTactic::kFlipBlock, TryTactic::kNone, TryTactic::kNone})));
}

}  // namespace blink
