// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/cascade_expansion.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/resolver/match_result.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

using css_test_helpers::ParseDeclarationBlock;

class CascadeExpansionTest : public PageTestBase {
 public:
  CascadeExpansion ExpansionAt(const MatchResult& result,
                               size_t i,
                               CascadeFilter filter = CascadeFilter()) {
    return CascadeExpansion(result.GetMatchedProperties()[i], GetDocument(),
                            filter, i);
  }

  Vector<CSSPropertyID> AllProperties(CascadeFilter filter = CascadeFilter()) {
    Vector<CSSPropertyID> all;
    for (CSSPropertyID id : CSSPropertyIDList()) {
      const CSSProperty& property = CSSProperty::Get(id);
      if (property.IsShorthand())
        continue;
      if (!property.IsAffectedByAll())
        continue;
      if (filter.Rejects(property))
        continue;
      all.push_back(id);
    }
    return all;
  }
};

TEST_F(CascadeExpansionTest, UARules) {
  MatchResult result;
  result.AddMatchedProperties(ParseDeclarationBlock("cursor:help;top:1px"));
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.FinishAddingAuthorRulesForTreeScope(GetDocument());

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  auto e = ExpansionAt(result, 0);
  ASSERT_FALSE(e.AtEnd());
  EXPECT_EQ(CSSPropertyID::kCursor, e.Id());
  EXPECT_EQ(CascadeOrigin::kUserAgent, e.Priority().GetOrigin());
  e.Next();
  ASSERT_FALSE(e.AtEnd());
  EXPECT_EQ(CSSPropertyID::kTop, e.Id());
  EXPECT_EQ(CascadeOrigin::kUserAgent, e.Priority().GetOrigin());
  e.Next();
  EXPECT_TRUE(e.AtEnd());
}

TEST_F(CascadeExpansionTest, UserRules) {
  MatchResult result;
  result.FinishAddingUARules();
  result.AddMatchedProperties(ParseDeclarationBlock("cursor:help"));
  result.AddMatchedProperties(ParseDeclarationBlock("float:left"));
  result.FinishAddingUserRules();
  result.FinishAddingAuthorRulesForTreeScope(GetDocument());

  ASSERT_EQ(2u, result.GetMatchedProperties().size());

  {
    auto e = ExpansionAt(result, 0);
    ASSERT_FALSE(e.AtEnd());
    EXPECT_EQ(CSSPropertyID::kCursor, e.Id());
    EXPECT_EQ(CascadeOrigin::kUser, e.Priority().GetOrigin());
    e.Next();
    EXPECT_TRUE(e.AtEnd());
  }

  {
    auto e = ExpansionAt(result, 1);
    ASSERT_FALSE(e.AtEnd());
    EXPECT_EQ(CSSPropertyID::kFloat, e.Id());
    EXPECT_EQ(CascadeOrigin::kUser, e.Priority().GetOrigin());
    e.Next();
    EXPECT_TRUE(e.AtEnd());
  }
}

TEST_F(CascadeExpansionTest, AuthorRules) {
  MatchResult result;
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.AddMatchedProperties(ParseDeclarationBlock("cursor:help;top:1px"));
  result.AddMatchedProperties(ParseDeclarationBlock("float:left"));
  result.FinishAddingAuthorRulesForTreeScope(GetDocument());

  ASSERT_EQ(2u, result.GetMatchedProperties().size());

  {
    auto e = ExpansionAt(result, 0);
    ASSERT_FALSE(e.AtEnd());
    EXPECT_EQ(CSSPropertyID::kCursor, e.Id());
    EXPECT_EQ(CascadeOrigin::kAuthor, e.Priority().GetOrigin());
    e.Next();
    ASSERT_FALSE(e.AtEnd());
    EXPECT_EQ(CSSPropertyID::kTop, e.Id());
    EXPECT_EQ(CascadeOrigin::kAuthor, e.Priority().GetOrigin());
    e.Next();
    EXPECT_TRUE(e.AtEnd());
  }

  {
    auto e = ExpansionAt(result, 1);
    ASSERT_FALSE(e.AtEnd());
    EXPECT_EQ(CSSPropertyID::kFloat, e.Id());
    EXPECT_EQ(CascadeOrigin::kAuthor, e.Priority().GetOrigin());
    e.Next();
    EXPECT_TRUE(e.AtEnd());
  }
}

TEST_F(CascadeExpansionTest, AllOriginRules) {
  MatchResult result;
  result.AddMatchedProperties(ParseDeclarationBlock("font-size:2px"));
  result.FinishAddingUARules();
  result.AddMatchedProperties(ParseDeclarationBlock("cursor:help;top:1px"));
  result.FinishAddingUserRules();
  result.AddMatchedProperties(ParseDeclarationBlock("left:1px"));
  result.AddMatchedProperties(ParseDeclarationBlock("float:left"));
  result.FinishAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(ParseDeclarationBlock("bottom:2px"));
  result.FinishAddingAuthorRulesForTreeScope(GetDocument());

  ASSERT_EQ(5u, result.GetMatchedProperties().size());

  {
    auto e = ExpansionAt(result, 0);
    ASSERT_FALSE(e.AtEnd());
    EXPECT_EQ(CSSPropertyID::kFontSize, e.Id());
    EXPECT_EQ(CascadeOrigin::kUserAgent, e.Priority().GetOrigin());
    e.Next();
    EXPECT_TRUE(e.AtEnd());
  }

  {
    auto e = ExpansionAt(result, 1);
    ASSERT_FALSE(e.AtEnd());
    EXPECT_EQ(CSSPropertyID::kCursor, e.Id());
    EXPECT_EQ(CascadeOrigin::kUser, e.Priority().GetOrigin());
    e.Next();
    ASSERT_FALSE(e.AtEnd());
    EXPECT_EQ(CSSPropertyID::kTop, e.Id());
    EXPECT_EQ(CascadeOrigin::kUser, e.Priority().GetOrigin());
    e.Next();
    EXPECT_TRUE(e.AtEnd());
  }

  {
    auto e = ExpansionAt(result, 2);
    ASSERT_FALSE(e.AtEnd());
    EXPECT_EQ(CSSPropertyID::kLeft, e.Id());
    EXPECT_EQ(CascadeOrigin::kAuthor, e.Priority().GetOrigin());
    e.Next();
    EXPECT_TRUE(e.AtEnd());
  }

  {
    auto e = ExpansionAt(result, 3);
    ASSERT_FALSE(e.AtEnd());
    EXPECT_EQ(CSSPropertyID::kFloat, e.Id());
    EXPECT_EQ(CascadeOrigin::kAuthor, e.Priority().GetOrigin());
    e.Next();
    EXPECT_TRUE(e.AtEnd());
  }

  {
    auto e = ExpansionAt(result, 4);
    ASSERT_FALSE(e.AtEnd());
    EXPECT_EQ(CSSPropertyID::kBottom, e.Id());
    EXPECT_EQ(CascadeOrigin::kAuthor, e.Priority().GetOrigin());
    e.Next();
    EXPECT_TRUE(e.AtEnd());
  }
}

TEST_F(CascadeExpansionTest, Name) {
  MatchResult result;
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.AddMatchedProperties(ParseDeclarationBlock("--x:1px;--y:2px"));
  result.AddMatchedProperties(ParseDeclarationBlock("float:left"));
  result.FinishAddingAuthorRulesForTreeScope(GetDocument());

  ASSERT_EQ(2u, result.GetMatchedProperties().size());

  {
    auto e = ExpansionAt(result, 0);
    ASSERT_FALSE(e.AtEnd());
    EXPECT_EQ(CSSPropertyName("--x"), e.Name());
    EXPECT_EQ(CSSPropertyID::kVariable, e.Id());
    e.Next();
    ASSERT_FALSE(e.AtEnd());
    EXPECT_EQ(CSSPropertyName("--y"), e.Name());
    EXPECT_EQ(CSSPropertyID::kVariable, e.Id());
    e.Next();
    EXPECT_TRUE(e.AtEnd());
  }

  {
    auto e = ExpansionAt(result, 1);
    ASSERT_FALSE(e.AtEnd());
    EXPECT_EQ(CSSPropertyName(CSSPropertyID::kFloat), e.Name());
    EXPECT_EQ(CSSPropertyID::kFloat, e.Id());
    e.Next();
    EXPECT_TRUE(e.AtEnd());
  }
}

TEST_F(CascadeExpansionTest, Value) {
  MatchResult result;
  result.AddMatchedProperties(ParseDeclarationBlock("background-color:red"));
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.FinishAddingAuthorRulesForTreeScope(GetDocument());

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  auto e = ExpansionAt(result, 0);
  ASSERT_FALSE(e.AtEnd());
  EXPECT_EQ(CSSPropertyID::kBackgroundColor, e.Id());
  EXPECT_EQ("red", e.Value().CssText());
  e.Next();
  ASSERT_FALSE(e.AtEnd());
  EXPECT_EQ(CSSPropertyID::kInternalVisitedBackgroundColor, e.Id());
  EXPECT_EQ("red", e.Value().CssText());
  e.Next();
  EXPECT_TRUE(e.AtEnd());
}

TEST_F(CascadeExpansionTest, LinkOmitted) {
  MatchResult result;
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.AddMatchedProperties(ParseDeclarationBlock("color:red"),
                              CSSSelector::kMatchVisited);
  result.FinishAddingAuthorRulesForTreeScope(GetDocument());

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  auto e = ExpansionAt(result, 0);
  ASSERT_FALSE(e.AtEnd());
  EXPECT_EQ(CSSPropertyID::kInternalVisitedColor, e.Id());
  e.Next();
  EXPECT_TRUE(e.AtEnd());
}

TEST_F(CascadeExpansionTest, InternalVisited) {
  MatchResult result;
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.AddMatchedProperties(ParseDeclarationBlock("color:red"));
  result.FinishAddingAuthorRulesForTreeScope(GetDocument());

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  auto e = ExpansionAt(result, 0);
  ASSERT_FALSE(e.AtEnd());
  EXPECT_EQ(CSSPropertyID::kColor, e.Id());
  e.Next();
  ASSERT_FALSE(e.AtEnd());
  EXPECT_EQ(CSSPropertyID::kInternalVisitedColor, e.Id());
  e.Next();
  EXPECT_TRUE(e.AtEnd());
}

TEST_F(CascadeExpansionTest, InternalVisitedOmitted) {
  MatchResult result;
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.AddMatchedProperties(ParseDeclarationBlock("color:red"),
                              CSSSelector::kMatchLink);
  result.FinishAddingAuthorRulesForTreeScope(GetDocument());

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  auto e = ExpansionAt(result, 0);
  ASSERT_FALSE(e.AtEnd());
  EXPECT_EQ(CSSPropertyID::kColor, e.Id());
  e.Next();
  EXPECT_TRUE(e.AtEnd());
}

TEST_F(CascadeExpansionTest, InternalVisitedWithTrailer) {
  MatchResult result;
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.AddMatchedProperties(ParseDeclarationBlock("color:red;left:1px"));
  result.FinishAddingAuthorRulesForTreeScope(GetDocument());

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  auto e = ExpansionAt(result, 0);
  ASSERT_FALSE(e.AtEnd());
  EXPECT_EQ(CSSPropertyID::kColor, e.Id());
  e.Next();
  ASSERT_FALSE(e.AtEnd());
  EXPECT_EQ(CSSPropertyID::kInternalVisitedColor, e.Id());
  e.Next();
  ASSERT_FALSE(e.AtEnd());
  EXPECT_EQ(CSSPropertyID::kLeft, e.Id());
  e.Next();
  EXPECT_TRUE(e.AtEnd());
}

TEST_F(CascadeExpansionTest, All) {
  MatchResult result;
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.AddMatchedProperties(ParseDeclarationBlock("all:unset"));
  result.FinishAddingAuthorRulesForTreeScope(GetDocument());

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  auto e = ExpansionAt(result, 0);

  for (CSSPropertyID expected : AllProperties()) {
    ASSERT_FALSE(e.AtEnd());
    EXPECT_EQ(expected, e.Id());
    e.Next();
  }

  EXPECT_TRUE(e.AtEnd());
}

TEST_F(CascadeExpansionTest, InlineAll) {
  MatchResult result;
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.AddMatchedProperties(
      ParseDeclarationBlock("left:1px;all:unset;right:1px"));
  result.FinishAddingAuthorRulesForTreeScope(GetDocument());

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  auto e = ExpansionAt(result, 0);

  ASSERT_FALSE(e.AtEnd());
  EXPECT_EQ(CSSPropertyID::kLeft, e.Id());
  e.Next();

  for (CSSPropertyID expected : AllProperties()) {
    ASSERT_FALSE(e.AtEnd());
    EXPECT_EQ(expected, e.Id());
    e.Next();
  }

  ASSERT_FALSE(e.AtEnd());
  EXPECT_EQ(CSSPropertyID::kRight, e.Id());
  e.Next();

  EXPECT_TRUE(e.AtEnd());
}

TEST_F(CascadeExpansionTest, FilterNormalNonInherited) {
  MatchResult result;
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.AddMatchedProperties(ParseDeclarationBlock("font-size:1px;left:1px"));
  result.FinishAddingAuthorRulesForTreeScope(GetDocument());

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  CascadeFilter filter(CSSProperty::kInherited, false);

  auto e = ExpansionAt(result, 0, filter);

  ASSERT_FALSE(e.AtEnd());
  EXPECT_EQ(CSSPropertyID::kFontSize, e.Id());
  e.Next();

  EXPECT_TRUE(e.AtEnd());
}

TEST_F(CascadeExpansionTest, FilterInternalVisited) {
  MatchResult result;
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.AddMatchedProperties(ParseDeclarationBlock("color:red"));
  result.FinishAddingAuthorRulesForTreeScope(GetDocument());

  CascadeFilter filter(CSSProperty::kVisited, true);

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  auto e = ExpansionAt(result, 0, filter);
  ASSERT_FALSE(e.AtEnd());
  EXPECT_EQ(CSSPropertyID::kColor, e.Id());
  e.Next();
  EXPECT_TRUE(e.AtEnd());
}

TEST_F(CascadeExpansionTest, FilterFirstLetter) {
  MatchResult result;
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.AddMatchedProperties(
      ParseDeclarationBlock("object-fit:unset;font-size:1px"),
      CSSSelector::kMatchAll, ValidPropertyFilter::kFirstLetter);
  result.FinishAddingAuthorRulesForTreeScope(GetDocument());

  auto e = ExpansionAt(result, 0);
  ASSERT_FALSE(e.AtEnd());
  EXPECT_EQ(CSSPropertyID::kFontSize, e.Id());
  e.Next();
  EXPECT_TRUE(e.AtEnd());
}

TEST_F(CascadeExpansionTest, FilterCue) {
  MatchResult result;
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.AddMatchedProperties(
      ParseDeclarationBlock("object-fit:unset;font-size:1px"),
      CSSSelector::kMatchAll, ValidPropertyFilter::kCue);
  result.FinishAddingAuthorRulesForTreeScope(GetDocument());

  auto e = ExpansionAt(result, 0);
  ASSERT_FALSE(e.AtEnd());
  EXPECT_EQ(CSSPropertyID::kFontSize, e.Id());
  e.Next();
  EXPECT_TRUE(e.AtEnd());
}

TEST_F(CascadeExpansionTest, FilterMarker) {
  MatchResult result;
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.AddMatchedProperties(
      ParseDeclarationBlock("object-fit:unset;font-size:1px"),
      CSSSelector::kMatchAll, ValidPropertyFilter::kMarker);
  result.FinishAddingAuthorRulesForTreeScope(GetDocument());

  auto e = ExpansionAt(result, 0);
  ASSERT_FALSE(e.AtEnd());
  EXPECT_EQ(CSSPropertyID::kFontSize, e.Id());
  e.Next();
  EXPECT_TRUE(e.AtEnd());
}

TEST_F(CascadeExpansionTest, FilterHighlight) {
  MatchResult result;
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.AddMatchedProperties(
      ParseDeclarationBlock("display:block;background-color:lime;"),
      CSSSelector::kMatchAll, ValidPropertyFilter::kHighlight);
  result.FinishAddingAuthorRulesForTreeScope(GetDocument());

  auto e = ExpansionAt(result, 0);
  ASSERT_FALSE(e.AtEnd());
  EXPECT_EQ(CSSPropertyID::kBackgroundColor, e.Id());
  e.Next();
  ASSERT_FALSE(e.AtEnd());
  EXPECT_EQ(CSSPropertyID::kInternalVisitedBackgroundColor, e.Id());
  e.Next();
  EXPECT_TRUE(e.AtEnd());
}

TEST_F(CascadeExpansionTest, FilterAllNonInherited) {
  MatchResult result;
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.AddMatchedProperties(ParseDeclarationBlock("all:unset"));
  result.FinishAddingAuthorRulesForTreeScope(GetDocument());

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  CascadeFilter filter(CSSProperty::kInherited, false);

  auto e = ExpansionAt(result, 0, filter);

  for (CSSPropertyID expected : AllProperties(filter)) {
    ASSERT_FALSE(e.AtEnd());
    EXPECT_EQ(expected, e.Id());
    e.Next();
  }

  EXPECT_TRUE(e.AtEnd());
}

TEST_F(CascadeExpansionTest, Importance) {
  MatchResult result;
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.AddMatchedProperties(
      ParseDeclarationBlock("cursor:help;display:block !important"));
  result.FinishAddingAuthorRulesForTreeScope(GetDocument());

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  auto e = ExpansionAt(result, 0);

  ASSERT_FALSE(e.AtEnd());
  EXPECT_EQ(CSSPropertyID::kCursor, e.Id());
  EXPECT_FALSE(e.Priority().IsImportant());
  e.Next();
  ASSERT_FALSE(e.AtEnd());
  EXPECT_EQ(CSSPropertyID::kDisplay, e.Id());
  EXPECT_TRUE(e.Priority().IsImportant());
  e.Next();

  EXPECT_TRUE(e.AtEnd());
}

TEST_F(CascadeExpansionTest, AllImportance) {
  MatchResult result;
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.AddMatchedProperties(ParseDeclarationBlock("all:unset !important"));
  result.FinishAddingAuthorRulesForTreeScope(GetDocument());

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  auto e = ExpansionAt(result, 0);

  for (CSSPropertyID expected : AllProperties()) {
    ASSERT_FALSE(e.AtEnd());
    EXPECT_EQ(expected, e.Id());
    EXPECT_TRUE(e.Priority().IsImportant());
    e.Next();
  }

  EXPECT_TRUE(e.AtEnd());
}

TEST_F(CascadeExpansionTest, AllNonImportance) {
  MatchResult result;
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.AddMatchedProperties(ParseDeclarationBlock("all:unset"));
  result.FinishAddingAuthorRulesForTreeScope(GetDocument());

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  auto e = ExpansionAt(result, 0);

  for (CSSPropertyID expected : AllProperties()) {
    ASSERT_FALSE(e.AtEnd());
    EXPECT_EQ(expected, e.Id());
    EXPECT_FALSE(e.Priority().IsImportant());
    e.Next();
  }

  EXPECT_TRUE(e.AtEnd());
}

TEST_F(CascadeExpansionTest, Position) {
  MatchResult result;
  result.FinishAddingUARules();
  result.FinishAddingUserRules();
  result.AddMatchedProperties(ParseDeclarationBlock("left:1px;top:1px"));
  result.AddMatchedProperties(ParseDeclarationBlock("bottom:1px;right:1px"));
  result.FinishAddingAuthorRulesForTreeScope(GetDocument());

  ASSERT_EQ(2u, result.GetMatchedProperties().size());

  {
    auto e = ExpansionAt(result, 0);

    ASSERT_FALSE(e.AtEnd());
    EXPECT_EQ(CSSPropertyID::kLeft, e.Id());
    EXPECT_EQ(0u, DecodeMatchedPropertiesIndex(e.Priority().GetPosition()));
    EXPECT_EQ(0u, DecodeDeclarationIndex(e.Priority().GetPosition()));
    e.Next();
    ASSERT_FALSE(e.AtEnd());
    EXPECT_EQ(CSSPropertyID::kTop, e.Id());
    EXPECT_EQ(0u, DecodeMatchedPropertiesIndex(e.Priority().GetPosition()));
    EXPECT_EQ(1u, DecodeDeclarationIndex(e.Priority().GetPosition()));
    e.Next();

    EXPECT_TRUE(e.AtEnd());
  }

  {
    auto e = ExpansionAt(result, 1);

    ASSERT_FALSE(e.AtEnd());
    EXPECT_EQ(CSSPropertyID::kBottom, e.Id());
    EXPECT_EQ(1u, DecodeMatchedPropertiesIndex(e.Priority().GetPosition()));
    EXPECT_EQ(0u, DecodeDeclarationIndex(e.Priority().GetPosition()));
    e.Next();
    ASSERT_FALSE(e.AtEnd());
    EXPECT_EQ(CSSPropertyID::kRight, e.Id());
    EXPECT_EQ(1u, DecodeMatchedPropertiesIndex(e.Priority().GetPosition()));
    EXPECT_EQ(1u, DecodeDeclarationIndex(e.Priority().GetPosition()));
    e.Next();

    EXPECT_TRUE(e.AtEnd());
  }
}

TEST_F(CascadeExpansionTest, MatchedPropertiesLimit) {
  constexpr size_t max = std::numeric_limits<uint16_t>::max();

  static_assert(CascadeExpansion::kMaxMatchedPropertiesIndex == max,
                "Unexpected max. If the limit increased, evaluate whether it "
                "still makes sense to run this test");

  auto* set = ParseDeclarationBlock("left:1px");

  MatchResult result;
  for (size_t i = 0; i < max + 3; ++i)
    result.AddMatchedProperties(set);

  ASSERT_EQ(max + 3u, result.GetMatchedProperties().size());

  for (size_t i = 0; i < max + 1; ++i)
    EXPECT_FALSE(ExpansionAt(result, i).AtEnd());

  // The indices beyond the max should not yield anything.
  EXPECT_TRUE(ExpansionAt(result, max + 1).AtEnd());
  EXPECT_TRUE(ExpansionAt(result, max + 2).AtEnd());
}

TEST_F(CascadeExpansionTest, MatchedDeclarationsLimit) {
  constexpr size_t max = std::numeric_limits<uint16_t>::max();

  static_assert(CascadeExpansion::kMaxDeclarationIndex == max,
                "Unexpected max. If the limit increased, evaluate whether it "
                "still makes sense to run this test");

  HeapVector<CSSPropertyValue> declarations(max + 2);

  // Actually give the first index a value, such that the initial call to
  // Next() does not crash.
  declarations[0] = CSSPropertyValue(CSSPropertyName(CSSPropertyID::kColor),
                                     *cssvalue::CSSUnsetValue::Create());

  MatchResult result;
  result.AddMatchedProperties(ImmutableCSSPropertyValueSet::Create(
      declarations.data(), max + 1, kHTMLStandardMode));
  result.AddMatchedProperties(ImmutableCSSPropertyValueSet::Create(
      declarations.data(), max + 2, kHTMLStandardMode));

  EXPECT_FALSE(ExpansionAt(result, 0).AtEnd());
  EXPECT_TRUE(ExpansionAt(result, 1).AtEnd());
}

}  // namespace blink
