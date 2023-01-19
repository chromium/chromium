// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/accessibility/blink_ax_event_intent.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/hash_table_deleted_value_type.h"
#include "ui/accessibility/ax_enums.mojom-blink.h"

namespace blink {
namespace test {

TEST(BlinkAXEventIntentTest, Equality) {
  BlinkAXEventIntent intent1(ax::mojom::blink::Command::kInsert,
                             ax::mojom::blink::InputEventType::kInsertText);
  BlinkAXEventIntent intent2(ax::mojom::blink::Command::kSetSelection,
                             ax::mojom::blink::TextBoundary::kWordEnd,
                             ax::mojom::blink::MoveDirection::kForward);
  BlinkAXEventIntent intent3(ax::mojom::blink::Command::kSetSelection,
                             ax::mojom::blink::TextBoundary::kWordEnd,
                             ax::mojom::blink::MoveDirection::kForward);

  EXPECT_NE(BlinkAXEventIntentHashTraits::GetHash(intent1),
            BlinkAXEventIntentHashTraits::GetHash(intent2));
  EXPECT_NE(BlinkAXEventIntentHashTraits::GetHash(intent1),
            BlinkAXEventIntentHashTraits::GetHash(intent3));
  EXPECT_EQ(BlinkAXEventIntentHashTraits::GetHash(intent2),
            BlinkAXEventIntentHashTraits::GetHash(intent3));

  EXPECT_FALSE(BlinkAXEventIntentHashTraits::Equal(intent1, intent2));
  EXPECT_FALSE(BlinkAXEventIntentHashTraits::Equal(intent1, intent3));
  EXPECT_TRUE(BlinkAXEventIntentHashTraits::Equal(intent2, intent3));
}

TEST(BlinkAXEventIntentTest, EqualityWithEmptyValue) {
  BlinkAXEventIntent intent1(ax::mojom::blink::Command::kInsert,
                             ax::mojom::blink::InputEventType::kInsertText);
  // Empty values.
  BlinkAXEventIntent intent2;
  BlinkAXEventIntent intent3;

  EXPECT_NE(BlinkAXEventIntentHashTraits::GetHash(intent1),
            BlinkAXEventIntentHashTraits::GetHash(intent2));
  EXPECT_FALSE(BlinkAXEventIntentHashTraits::Equal(intent1, intent2));

  EXPECT_EQ(BlinkAXEventIntentHashTraits::GetHash(intent2),
            BlinkAXEventIntentHashTraits::GetHash(intent3));
  EXPECT_TRUE(BlinkAXEventIntentHashTraits::Equal(intent2, intent3));
}

TEST(BlinkAXEventIntentTest, EqualityWithDeletedValue) {
  BlinkAXEventIntent intent1(ax::mojom::blink::Command::kInsert,
                             ax::mojom::blink::InputEventType::kInsertText);
  BlinkAXEventIntent intent2(WTF::kHashTableDeletedValue);
  BlinkAXEventIntent intent3(WTF::kHashTableDeletedValue);

  EXPECT_NE(BlinkAXEventIntentHashTraits::GetHash(intent1),
            BlinkAXEventIntentHashTraits::GetHash(intent2));
  EXPECT_FALSE(BlinkAXEventIntentHashTraits::Equal(intent1, intent2));

  EXPECT_EQ(BlinkAXEventIntentHashTraits::GetHash(intent2),
            BlinkAXEventIntentHashTraits::GetHash(intent3));
  EXPECT_TRUE(BlinkAXEventIntentHashTraits::Equal(intent2, intent3));
}

}  // namespace test
}  // namespace blink
