// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/space_split_string.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(SpaceSplitStringTest, Set) {
  test::TaskEnvironment task_environment;
  SpaceSplitString tokens;

  tokens.Set(AtomicString("foo"));
  EXPECT_EQ(1u, tokens.size());
  EXPECT_EQ(AtomicString("foo"), tokens[0]);

  tokens.Set(AtomicString(" foo\t"));
  EXPECT_EQ(1u, tokens.size());
  EXPECT_EQ(AtomicString("foo"), tokens[0]);

  tokens.Set(AtomicString("foo foo\t"));
  EXPECT_EQ(1u, tokens.size());
  EXPECT_EQ(AtomicString("foo"), tokens[0]);

  tokens.Set(AtomicString("foo foo  foo"));
  EXPECT_EQ(1u, tokens.size());
  EXPECT_EQ(AtomicString("foo"), tokens[0]);

  tokens.Set(AtomicString("foo foo bar foo"));
  EXPECT_EQ(2u, tokens.size());
  EXPECT_EQ(AtomicString("foo"), tokens[0]);
  EXPECT_EQ(AtomicString("bar"), tokens[1]);
}

TEST(SpaceSplitStringTest, SerializeToString) {
  test::TaskEnvironment task_environment;
  SpaceSplitString tokens;

  EXPECT_EQ("", tokens.SerializeToString());

  tokens.Set(AtomicString("foo"));
  EXPECT_EQ("foo", tokens.SerializeToString());

  tokens.Set(AtomicString("foo bar"));
  EXPECT_EQ("foo bar", tokens.SerializeToString());

  tokens.Set(AtomicString("foo"));
  tokens.Add(AtomicString("bar"));
  EXPECT_EQ("foo bar", tokens.SerializeToString());

  tokens.Set(AtomicString("bar"));
  tokens.Add(AtomicString("foo"));
  EXPECT_EQ("bar foo", tokens.SerializeToString());
}
}
