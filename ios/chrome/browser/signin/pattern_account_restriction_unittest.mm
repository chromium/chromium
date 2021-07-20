// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/pattern_account_restriction.h"

#include "base/values.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const std::string email1 = "foo@gmail.com";
const std::string email2 = "foo2@google.com";
const std::string email3 = "foo3@chromium.com";
}  // namespace

class PatternAccountRestrictionTest : public PlatformTest {};

// Tests that the PatternAccountRestriction filters email correctly when
// restrictions are set.
TEST_F(PatternAccountRestrictionTest, FilterEmailsWithRestrictions) {
  base::ListValue value;
  value.AppendString("*gmail.com");
  value.AppendString("*google.com");
  auto restriction = PatternAccountRestrictionFromValue(&value);

  CHECK_EQ(restriction->IsAccountRestricted(email1), false);
  CHECK_EQ(restriction->IsAccountRestricted(email2), false);
  CHECK_EQ(restriction->IsAccountRestricted(email3), true);
}

// Tests that the PatternAccountRestriction does not filter emails when
// restrictions are not set.
TEST_F(PatternAccountRestrictionTest, FilterEmailsWithoutRestriction) {
  base::ListValue value;
  auto restriction = PatternAccountRestrictionFromValue(&value);

  CHECK_EQ(restriction->IsAccountRestricted(email1), false);
  CHECK_EQ(restriction->IsAccountRestricted(email2), false);
  CHECK_EQ(restriction->IsAccountRestricted(email3), false);
}

// Tests that the pattern created by PatternFromString(chunk) correctlty matches
// the given email. The wildcard character '*' matches zero or more arbitrary
// characters.The escape character is '\', so to match actual '*' or '\'
// characters, put a '\' in front of them.
TEST_F(PatternAccountRestrictionTest, PatternMatchChunck) {
  auto pattern = PatternFromString("*gmail.com");
  CHECK_EQ(pattern->Match(email1), true);
  CHECK_EQ(pattern->Match(email2), false);
  CHECK_EQ(pattern->Match(email3), false);

  pattern = PatternFromString("gmail.com");
  CHECK_EQ(pattern->Match(email1), false);
  CHECK_EQ(pattern->Match(email2), false);
  CHECK_EQ(pattern->Match(email3), false);

  pattern = PatternFromString("foo*");
  CHECK_EQ(pattern->Match(email1), true);
  CHECK_EQ(pattern->Match(email2), true);
  CHECK_EQ(pattern->Match(email3), true);

  // "foo\\*@gmail.com" is actually "foo\*@gmail.com". The escape character '\'
  // is doubled here because it's also an escape character for std::string.
  pattern = PatternFromString("foo\\*@gmail.com");
  CHECK_EQ(pattern->Match(email1), false);
  CHECK_EQ(pattern->Match("foo*@gmail.com"), true);

  // "foo\\\\*" is "actually foo\\*".
  pattern = PatternFromString("foo\\\\*");
  CHECK_EQ(pattern->Match(email1), false);
  // "foo\\@gmail.com" is actually "foo\@gmail.com".
  CHECK_EQ(pattern->Match("foo\\@gmail.com"), true);
}
