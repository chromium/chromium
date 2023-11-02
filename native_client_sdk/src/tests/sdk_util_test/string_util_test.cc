// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "sdk_util/string_util.h"

using namespace sdk_util;
using namespace testing;

TEST(StringUtilTest, SplitString) {
  std::vector<std::string> r;

  SplitString(std::string(), ',', &r);
  EXPECT_THAT(r, ElementsAre());
  r.clear();

  SplitString("a,b,c", ',', &r);
  EXPECT_THAT(r, ElementsAre("a", "b", "c"));
  r.clear();

  SplitString("a, b, c", ',', &r);
  EXPECT_THAT(r, ElementsAre("a", " b", " c"));
  r.clear();

  SplitString("a,,c", ',', &r);
  EXPECT_THAT(r, ElementsAre("a", "", "c"));
  r.clear();

  SplitString("foo", '*', &r);
  EXPECT_THAT(r, ElementsAre("foo"));
  r.clear();

  SplitString("foo ,", ',', &r);
  EXPECT_THAT(r, ElementsAre("foo ", ""));
  r.clear();

  SplitString(",", ',', &r);
  EXPECT_THAT(r, ElementsAre("", ""));
  r.clear();

  SplitString("\t\ta\t", '\t', &r);
  EXPECT_THAT(r, ElementsAre("", "", "a", ""));
  r.clear();
}

TEST(StringUtilTest, StringWithNoDelimiter) {
  std::vector<std::string> results;
  SplitString("alongwordwithnodelimiter", 'D', &results);
  EXPECT_THAT(results, ElementsAre("alongwordwithnodelimiter"));
}

TEST(StringUtilTest, LeadingDelimitersSkipped) {
  std::vector<std::string> results;
  SplitString("DDDoneDtwoDthree", 'D', &results);
  EXPECT_THAT(results, ElementsAre("", "", "", "one", "two", "three"));
}

TEST(StringUtilTest, ConsecutiveDelimitersSkipped) {
  std::vector<std::string> results;
  SplitString("unoDDDdosDtresDDcuatro", 'D', &results);
  EXPECT_THAT(results, ElementsAre("uno", "", "", "dos", "tres", "", "cuatro"));
}

TEST(StringUtilTest, TrailingDelimitersSkipped) {
  std::vector<std::string> results;
  SplitString("unDdeuxDtroisDquatreDDD", 'D', &results);
  EXPECT_THAT(results,
              ElementsAre("un", "deux", "trois", "quatre", "", "", ""));
}

// Note: Unlike base/strings/string_split, whitespace is not stripped.
// SplitString is expected to behave like SplitStringDontTrim in Chromium's
// 'base' module.
TEST(StringUtilTest, StringSplitDontTrim) {
  std::vector<std::string> r;

  SplitString("   ", '*', &r);
  EXPECT_THAT(r, ElementsAre("   "));

  SplitString("\t  \ta\t ", '\t', &r);
  EXPECT_THAT(r, ElementsAre("", "  ", "a", " "));

  SplitString("\ta\t\nb\tcc", '\n', &r);
  EXPECT_THAT(r, ElementsAre("\ta\t", "b\tcc"));
}
