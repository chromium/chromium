// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_entity_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(HTMLEntityParserTest, ConsumeHTMLEntityIncomplete) {
  test::TaskEnvironment task_environment;
  String original("am");  // Incomplete by purpose.
  SegmentedString src(original);

  DecodedHTMLEntity entity;
  bool not_enough_characters = false;
  bool success = ConsumeHTMLEntity(src, entity, not_enough_characters);
  EXPECT_TRUE(not_enough_characters);
  EXPECT_FALSE(success);

  // consumeHTMLEntity should recover the original SegmentedString state if
  // failed.
  EXPECT_EQ(original, src.ToString());
}

}  // namespace blink
