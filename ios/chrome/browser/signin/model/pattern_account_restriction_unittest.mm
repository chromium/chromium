// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/pattern_account_restriction.h"

#import "base/values.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {
const std::string email1 = "foo@gmail.com";
const std::string email2 = "foo2@google.com";
const std::string email3 = "foo3@chromium.com";
}  // namespace

class PatternAccountRestrictionTest : public PlatformTest {};

// Tests that the PatternAccountRestriction filters email correctly when
// restrictions are set.
TEST_F(PatternAccountRestrictionTest, FilterEmailsWithRestrictions) {
  base::Value::List list;
  list.Append("*gmail.com");
  list.Append("*google.com");
  auto restriction = PatternAccountRestrictionFromValue(list);

  EXPECT_EQ(restriction->IsAccountRestricted(email1), false);
  EXPECT_EQ(restriction->IsAccountRestricted(email2), false);
  EXPECT_EQ(restriction->IsAccountRestricted(email3), true);
}

// Tests that the PatternAccountRestriction does not filter emails when
// restrictions are not set.
TEST_F(PatternAccountRestrictionTest, FilterEmailsWithoutRestriction) {
  base::Value::List list;
  auto restriction = PatternAccountRestrictionFromValue(list);

  EXPECT_EQ(restriction->IsAccountRestricted(email1), false);
  EXPECT_EQ(restriction->IsAccountRestricted(email2), false);
  EXPECT_EQ(restriction->IsAccountRestricted(email3), false);
}

// Tests that the PatternAccountRestriction does not filter emails when the
// restriction is not correctly formatted.
TEST_F(PatternAccountRestrictionTest, FilterEmailsWithBadPattern) {
  base::Value::List list;
  list.Append("*gmail.com\\");
  list.Append("*google.com");
  auto restriction = PatternAccountRestrictionFromValue(list);

  EXPECT_EQ(restriction->IsAccountRestricted(email1), true);
  EXPECT_EQ(restriction->IsAccountRestricted(email2), false);
  EXPECT_EQ(restriction->IsAccountRestricted(email3), true);
}

// Tests that the pattern created by PatternFromString(chunk) correctlty matches
// the given email. The wildcard character '*' matches zero or more arbitrary
// characters.The escape character is '\', so to match actual '*' or '\'
// characters, put a '\' in front of them.
TEST_F(PatternAccountRestrictionTest, PatternMatchChunck) {
  auto pattern = PatternFromString("*gmail.com");
  EXPECT_EQ(pattern->Match(email1), true);
  EXPECT_EQ(pattern->Match(email2), false);
  EXPECT_EQ(pattern->Match(email3), false);

  pattern = PatternFromString("gmail.com");
  EXPECT_EQ(pattern->Match(email1), false);
  EXPECT_EQ(pattern->Match(email2), false);
  EXPECT_EQ(pattern->Match(email3), false);

  pattern = PatternFromString("foo*");
  EXPECT_EQ(pattern->Match(email1), true);
  EXPECT_EQ(pattern->Match(email2), true);
  EXPECT_EQ(pattern->Match(email3), true);

  // "foo\\*@gmail.com" is actually "foo\*@gmail.com". The escape character '\'
  // is doubled here because it's also an escape character for std::string.
  pattern = PatternFromString("foo\\*@gmail.com");
  EXPECT_EQ(pattern->Match(email1), false);
  EXPECT_EQ(pattern->Match("foo*@gmail.com"), true);

  // "foo\\\\*" is "actually foo\\*".
  pattern = PatternFromString("foo\\\\*");
  EXPECT_EQ(pattern->Match(email1), false);
  // "foo\\@gmail.com" is actually "foo\@gmail.com".
  EXPECT_EQ(pattern->Match("foo\\@gmail.com"), true);
}

// Tests that valid patterns are correctly identified.
TEST_F(PatternAccountRestrictionTest, ValidPattern) {
  base::Value value{base::Value::Type::LIST};
  value.GetList().Append("*gmail.com");
  value.GetList().Append("myemail@gmail.com");
  value.GetList().Append("myemail\\*@gmail.com");
  value.GetList().Append("\\\\google.com");

  EXPECT_TRUE(ArePatternsValid(&value));
}

// Tests that invalid patterns are correctly identified.
TEST_F(PatternAccountRestrictionTest, InvalidPattern) {
  base::Value value{base::Value::Type::LIST};
  value.GetList().Append("*gmail.com\\");
  value.GetList().Append("*google.com");

  EXPECT_FALSE(ArePatternsValid(&value));
}

// Tests that empty patterns are correctly identified.
TEST_F(PatternAccountRestrictionTest, EmptyPattern) {
  base::Value value{base::Value::Type::LIST};

  EXPECT_TRUE(ArePatternsValid(&value));
}
