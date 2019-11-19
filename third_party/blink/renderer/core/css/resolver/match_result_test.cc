// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/match_result.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class MatchResultTest : public testing::Test {
 protected:
  void SetUp() override;

  const CSSPropertyValueSet* PropertySet(unsigned index) const {
    return property_sets->at(index).Get();
  }

 private:
  Persistent<HeapVector<Member<MutableCSSPropertyValueSet>, 8>> property_sets;
};

void MatchResultTest::SetUp() {
  property_sets =
      MakeGarbageCollected<HeapVector<Member<MutableCSSPropertyValueSet>, 8>>();
  for (unsigned i = 0; i < 8; i++) {
    property_sets->push_back(
        MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode));
  }
}

void TestMatchedPropertiesRange(const MatchedPropertiesRange& range,
                                int expected_length,
                                const CSSPropertyValueSet** expected_sets) {
  EXPECT_EQ(expected_length, range.end() - range.begin());
  for (const auto& matched_properties : range)
    EXPECT_EQ(*expected_sets++, matched_properties.properties);
}

TEST_F(MatchResultTest, UARules) {
  const CSSPropertyValueSet* ua_sets[] = {PropertySet(0), PropertySet(1)};

  MatchResult result;
  result.AddMatchedProperties(ua_sets[0]);
  result.AddMatchedProperties(ua_sets[1]);
  result.FinishAddingUARules();
  result.FinishAddingUserRules();

  result.FinishAddingAuthorRulesForTreeScope();

  TestMatchedPropertiesRange(result.AllRules(), 2, ua_sets);
  TestMatchedPropertiesRange(result.UaRules(), 2, ua_sets);
  TestMatchedPropertiesRange(result.UserRules(), 0, nullptr);
  TestMatchedPropertiesRange(result.AuthorRules(), 0, nullptr);

  ImportantAuthorRanges importantAuthor(result);
  EXPECT_EQ(importantAuthor.end(), importantAuthor.begin());
  ImportantUserRanges importantUser(result);
  EXPECT_EQ(importantUser.end(), importantUser.begin());
}

TEST_F(MatchResultTest, UserRules) {
  const CSSPropertyValueSet* user_sets[] = {PropertySet(0), PropertySet(1)};

  MatchResult result;

  result.FinishAddingUARules();
  result.AddMatchedProperties(user_sets[0]);
  result.AddMatchedProperties(user_sets[1]);
  result.FinishAddingUserRules();
  result.FinishAddingAuthorRulesForTreeScope();

  TestMatchedPropertiesRange(result.AllRules(), 2, user_sets);
  TestMatchedPropertiesRange(result.UaRules(), 0, nullptr);
  TestMatchedPropertiesRange(result.UserRules(), 2, user_sets);
  TestMatchedPropertiesRange(result.AuthorRules(), 0, nullptr);

  ImportantAuthorRanges importantAuthor(result);
  EXPECT_EQ(importantAuthor.end(), importantAuthor.begin());
  ImportantUserRanges importantUser(result);
  EXPECT_EQ(importantUser.end(), ++importantUser.begin());
}

TEST_F(MatchResultTest, AuthorRules) {
  const CSSPropertyValueSet* author_sets[] = {PropertySet(0), PropertySet(1)};

  MatchResult result;

  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.AddMatchedProperties(author_sets[0]);
  result.AddMatchedProperties(author_sets[1]);
  result.FinishAddingAuthorRulesForTreeScope();

  TestMatchedPropertiesRange(result.AllRules(), 2, author_sets);
  TestMatchedPropertiesRange(result.UaRules(), 0, nullptr);
  TestMatchedPropertiesRange(result.UserRules(), 0, nullptr);
  TestMatchedPropertiesRange(result.AuthorRules(), 2, author_sets);

  ImportantAuthorRanges importantAuthor(result);
  EXPECT_EQ(importantAuthor.end(), ++importantAuthor.begin());
  ImportantUserRanges importantUser(result);
  EXPECT_EQ(importantUser.end(), importantUser.begin());
}

TEST_F(MatchResultTest, AllRules) {
  const CSSPropertyValueSet* all_sets[] = {PropertySet(0), PropertySet(1),
                                           PropertySet(2), PropertySet(3),
                                           PropertySet(4), PropertySet(5)};
  const CSSPropertyValueSet** ua_sets = &all_sets[0];
  const CSSPropertyValueSet** user_sets = &all_sets[2];
  const CSSPropertyValueSet** author_sets = &all_sets[4];

  MatchResult result;

  result.AddMatchedProperties(ua_sets[0]);
  result.AddMatchedProperties(ua_sets[1]);
  result.FinishAddingUARules();

  result.AddMatchedProperties(user_sets[0]);
  result.AddMatchedProperties(user_sets[1]);
  result.FinishAddingUserRules();

  result.AddMatchedProperties(author_sets[0]);
  result.AddMatchedProperties(author_sets[1]);
  result.FinishAddingAuthorRulesForTreeScope();

  TestMatchedPropertiesRange(result.AllRules(), 6, all_sets);
  TestMatchedPropertiesRange(result.UaRules(), 2, ua_sets);
  TestMatchedPropertiesRange(result.UserRules(), 2, user_sets);
  TestMatchedPropertiesRange(result.AuthorRules(), 2, author_sets);

  ImportantAuthorRanges importantAuthor(result);
  EXPECT_EQ(importantAuthor.end(), ++importantAuthor.begin());
  ImportantUserRanges importantUser(result);
  EXPECT_EQ(importantUser.end(), ++importantUser.begin());
}

TEST_F(MatchResultTest, AuthorRulesMultipleScopes) {
  const CSSPropertyValueSet* author_sets[] = {PropertySet(0), PropertySet(1),
                                              PropertySet(2), PropertySet(3)};

  MatchResult result;

  result.FinishAddingUARules();
  result.FinishAddingUserRules();

  result.AddMatchedProperties(author_sets[0]);
  result.AddMatchedProperties(author_sets[1]);
  result.FinishAddingAuthorRulesForTreeScope();

  result.AddMatchedProperties(author_sets[2]);
  result.AddMatchedProperties(author_sets[3]);
  result.FinishAddingAuthorRulesForTreeScope();

  TestMatchedPropertiesRange(result.AllRules(), 4, author_sets);
  TestMatchedPropertiesRange(result.UaRules(), 0, nullptr);
  TestMatchedPropertiesRange(result.UserRules(), 0, nullptr);
  TestMatchedPropertiesRange(result.AuthorRules(), 4, author_sets);

  ImportantAuthorRanges importantAuthor(result);

  auto iter = importantAuthor.begin();
  EXPECT_NE(importantAuthor.end(), iter);
  TestMatchedPropertiesRange(*iter, 2, &author_sets[2]);

  ++iter;
  EXPECT_NE(importantAuthor.end(), iter);
  TestMatchedPropertiesRange(*iter, 2, author_sets);

  ++iter;
  EXPECT_EQ(importantAuthor.end(), iter);

  ImportantUserRanges importantUser(result);
  EXPECT_EQ(importantUser.end(), importantUser.begin());
}

TEST_F(MatchResultTest, AllRulesMultipleScopes) {
  const CSSPropertyValueSet* all_sets[] = {
      PropertySet(0), PropertySet(1), PropertySet(2), PropertySet(3),
      PropertySet(4), PropertySet(5), PropertySet(6), PropertySet(7)};
  const CSSPropertyValueSet** ua_sets = &all_sets[0];
  const CSSPropertyValueSet** user_sets = &all_sets[2];
  const CSSPropertyValueSet** author_sets = &all_sets[4];

  MatchResult result;

  result.AddMatchedProperties(ua_sets[0]);
  result.AddMatchedProperties(ua_sets[1]);
  result.FinishAddingUARules();

  result.AddMatchedProperties(user_sets[0]);
  result.AddMatchedProperties(user_sets[1]);
  result.FinishAddingUserRules();

  result.AddMatchedProperties(author_sets[0]);
  result.AddMatchedProperties(author_sets[1]);
  result.FinishAddingAuthorRulesForTreeScope();

  result.AddMatchedProperties(author_sets[2]);
  result.AddMatchedProperties(author_sets[3]);
  result.FinishAddingAuthorRulesForTreeScope();

  TestMatchedPropertiesRange(result.AllRules(), 8, all_sets);
  TestMatchedPropertiesRange(result.UaRules(), 2, ua_sets);
  TestMatchedPropertiesRange(result.UserRules(), 2, user_sets);
  TestMatchedPropertiesRange(result.AuthorRules(), 4, author_sets);

  ImportantAuthorRanges importantAuthor(result);

  ImportantAuthorRangeIterator iter = importantAuthor.begin();
  EXPECT_NE(importantAuthor.end(), iter);
  TestMatchedPropertiesRange(*iter, 2, &author_sets[2]);

  ++iter;
  EXPECT_NE(importantAuthor.end(), iter);
  TestMatchedPropertiesRange(*iter, 2, author_sets);

  ++iter;
  EXPECT_EQ(importantAuthor.end(), iter);

  ImportantUserRanges importantUser(result);
  EXPECT_EQ(importantUser.end(), ++importantUser.begin());
}

}  // namespace blink
