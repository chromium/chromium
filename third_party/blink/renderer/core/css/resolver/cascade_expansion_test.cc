// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/cascade_expansion.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_expansion-inl.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/resolver/match_result.h"
#include "third_party/blink/renderer/core/css/rule_set.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

using css_test_helpers::ParseDeclarationBlock;

namespace {

// This list does not necessarily need to be exhaustive.
const CSSPropertyID kVisitedPropertySamples[] = {
    CSSPropertyID::kInternalVisitedColor,
    CSSPropertyID::kInternalVisitedBackgroundColor,
    CSSPropertyID::kInternalVisitedBorderBlockEndColor,
    CSSPropertyID::kInternalVisitedBorderBlockStartColor,
    CSSPropertyID::kInternalVisitedBorderBottomColor,
    CSSPropertyID::kInternalVisitedBorderInlineEndColor,
    CSSPropertyID::kInternalVisitedBorderInlineStartColor,
    CSSPropertyID::kInternalVisitedBorderLeftColor,
    CSSPropertyID::kInternalVisitedBorderRightColor,
    CSSPropertyID::kInternalVisitedBorderTopColor,
    CSSPropertyID::kInternalVisitedCaretColor,
    CSSPropertyID::kInternalVisitedColumnRuleColor,
    CSSPropertyID::kInternalVisitedFill,
    CSSPropertyID::kInternalVisitedOutlineColor,
    CSSPropertyID::kInternalVisitedStroke,
    CSSPropertyID::kInternalVisitedTextDecorationColor,
    CSSPropertyID::kInternalVisitedTextEmphasisColor,
    CSSPropertyID::kInternalVisitedTextFillColor,
    CSSPropertyID::kInternalVisitedTextStrokeColor,
};

}  // namespace

class CascadeExpansionTest : public PageTestBase {
 public:
  struct ExpansionResult : public GarbageCollected<ExpansionResult> {
    CascadePriority priority;
    CSSPropertyRef ref;

    explicit ExpansionResult(const CSSProperty& property) : ref(property) {}

    void Trace(Visitor* visitor) const { visitor->Trace(ref); }
  };

  HeapVector<Member<ExpansionResult>> ExpansionAt(const MatchResult& result,
                                                  wtf_size_t i) {
    HeapVector<Member<ExpansionResult>> ret;
    ExpandCascade(
        result.GetMatchedProperties()[i], GetDocument(), i,
        [this, &ret](CascadePriority cascade_priority,
                     const AtomicString& name) {
          ExpansionResult* er = MakeGarbageCollected<ExpansionResult>(
              CustomProperty(name, GetDocument()));
          er->priority = cascade_priority;
          ret.push_back(er);
        },
        [&ret](CascadePriority cascade_priority, CSSPropertyID id) {
          ExpansionResult* er =
              MakeGarbageCollected<ExpansionResult>(CSSProperty::Get(id));
          er->priority = cascade_priority;
          ret.push_back(er);
        });
    return ret;
  }

  Vector<CSSPropertyID> AllProperties(CascadeFilter filter = CascadeFilter()) {
    Vector<CSSPropertyID> all;
    for (CSSPropertyID id : CSSPropertyIDList()) {
      const CSSProperty& property = CSSProperty::Get(id);
      if (!IsInAllExpansion(id)) {
        continue;
      }
      if (filter.Rejects(property)) {
        continue;
      }
      all.push_back(id);
    }
    return all;
  }

  Vector<CSSPropertyID> VisitedPropertiesInExpansion(
      const MatchedProperties& matched_properties,
      wtf_size_t i) {
    Vector<CSSPropertyID> visited;

    ExpandCascade(
        matched_properties, GetDocument(), i,
        [](CascadePriority cascade_priority [[maybe_unused]],
           const AtomicString& name [[maybe_unused]]) {
          // Do nothing.
        },
        [&visited](CascadePriority cascade_priority [[maybe_unused]],
                   CSSPropertyID id) {
          const CSSProperty& css_property = CSSProperty::Get(id);
          if (css_property.IsVisited()) {
            visited.push_back(css_property.PropertyID());
          }
        });

    return visited;
  }
};

TEST_F(CascadeExpansionTest, UARules) {
  MatchResult result;
  result.AddMatchedProperties(ParseDeclarationBlock("cursor:help;top:1px"),
                              {.origin = CascadeOrigin::kUserAgent});

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  auto e = ExpansionAt(result, 0);
  ASSERT_EQ(2u, e.size());
  EXPECT_EQ(CSSPropertyID::kCursor, e[0]->ref.GetProperty().PropertyID());
  EXPECT_EQ(CascadeOrigin::kUserAgent, e[0]->priority.GetOrigin());
  EXPECT_EQ(CSSPropertyID::kTop, e[1]->ref.GetProperty().PropertyID());
  EXPECT_EQ(CascadeOrigin::kUserAgent, e[1]->priority.GetOrigin());
}

TEST_F(CascadeExpansionTest, UserRules) {
  MatchResult result;
  result.AddMatchedProperties(ParseDeclarationBlock("cursor:help"),
                              {.origin = CascadeOrigin::kUser});
  result.AddMatchedProperties(ParseDeclarationBlock("float:left"),
                              {.origin = CascadeOrigin::kUser});

  ASSERT_EQ(2u, result.GetMatchedProperties().size());

  {
    auto e = ExpansionAt(result, 0);
    ASSERT_EQ(1u, e.size());
    EXPECT_EQ(CSSPropertyID::kCursor, e[0]->ref.GetProperty().PropertyID());
    EXPECT_EQ(CascadeOrigin::kUser, e[0]->priority.GetOrigin());
  }

  {
    auto e = ExpansionAt(result, 1);
    ASSERT_EQ(1u, e.size());
    EXPECT_EQ(CSSPropertyID::kFloat, e[0]->ref.GetProperty().PropertyID());
    EXPECT_EQ(CascadeOrigin::kUser, e[0]->priority.GetOrigin());
  }
}

TEST_F(CascadeExpansionTest, AuthorRules) {
  MatchResult result;
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(ParseDeclarationBlock("cursor:help;top:1px"),
                              {.origin = CascadeOrigin::kAuthor});
  result.AddMatchedProperties(ParseDeclarationBlock("float:left"),
                              {.origin = CascadeOrigin::kAuthor});

  ASSERT_EQ(2u, result.GetMatchedProperties().size());

  {
    auto e = ExpansionAt(result, 0);
    ASSERT_EQ(2u, e.size());
    EXPECT_EQ(CSSPropertyID::kCursor, e[0]->ref.GetProperty().PropertyID());
    EXPECT_EQ(CascadeOrigin::kAuthor, e[0]->priority.GetOrigin());
    EXPECT_EQ(CSSPropertyID::kTop, e[1]->ref.GetProperty().PropertyID());
    EXPECT_EQ(CascadeOrigin::kAuthor, e[1]->priority.GetOrigin());
  }

  {
    auto e = ExpansionAt(result, 1);
    ASSERT_EQ(1u, e.size());
    EXPECT_EQ(CSSPropertyID::kFloat, e[0]->ref.GetProperty().PropertyID());
    EXPECT_EQ(CascadeOrigin::kAuthor, e[0]->priority.GetOrigin());
  }
}

TEST_F(CascadeExpansionTest, AllOriginRules) {
  MatchResult result;
  result.AddMatchedProperties(ParseDeclarationBlock("font-size:2px"),
                              {.origin = CascadeOrigin::kUserAgent});
  result.AddMatchedProperties(ParseDeclarationBlock("cursor:help;top:1px"),
                              {.origin = CascadeOrigin::kUser});
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(ParseDeclarationBlock("left:1px"),
                              {.origin = CascadeOrigin::kAuthor});
  result.AddMatchedProperties(ParseDeclarationBlock("float:left"),
                              {.origin = CascadeOrigin::kAuthor});
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(ParseDeclarationBlock("bottom:2px"),
                              {.origin = CascadeOrigin::kAuthor});

  ASSERT_EQ(5u, result.GetMatchedProperties().size());

  {
    auto e = ExpansionAt(result, 0);
    ASSERT_EQ(1u, e.size());
    EXPECT_EQ(CSSPropertyID::kFontSize, e[0]->ref.GetProperty().PropertyID());
    EXPECT_EQ(CascadeOrigin::kUserAgent, e[0]->priority.GetOrigin());
  }

  {
    auto e = ExpansionAt(result, 1);
    ASSERT_EQ(2u, e.size());
    EXPECT_EQ(CSSPropertyID::kCursor, e[0]->ref.GetProperty().PropertyID());
    EXPECT_EQ(CascadeOrigin::kUser, e[0]->priority.GetOrigin());
    EXPECT_EQ(CSSPropertyID::kTop, e[1]->ref.GetProperty().PropertyID());
    EXPECT_EQ(CascadeOrigin::kUser, e[1]->priority.GetOrigin());
  }

  {
    auto e = ExpansionAt(result, 2);
    ASSERT_EQ(1u, e.size());
    EXPECT_EQ(CSSPropertyID::kLeft, e[0]->ref.GetProperty().PropertyID());
    EXPECT_EQ(CascadeOrigin::kAuthor, e[0]->priority.GetOrigin());
  }

  {
    auto e = ExpansionAt(result, 3);
    ASSERT_EQ(1u, e.size());
    EXPECT_EQ(CSSPropertyID::kFloat, e[0]->ref.GetProperty().PropertyID());
    EXPECT_EQ(CascadeOrigin::kAuthor, e[0]->priority.GetOrigin());
  }

  {
    auto e = ExpansionAt(result, 4);
    ASSERT_EQ(1u, e.size());
    EXPECT_EQ(CSSPropertyID::kBottom, e[0]->ref.GetProperty().PropertyID());
    EXPECT_EQ(CascadeOrigin::kAuthor, e[0]->priority.GetOrigin());
  }
}

TEST_F(CascadeExpansionTest, Name) {
  MatchResult result;
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(ParseDeclarationBlock("--x:1px;--y:2px"),
                              {.origin = CascadeOrigin::kAuthor});
  result.AddMatchedProperties(ParseDeclarationBlock("float:left"),
                              {.origin = CascadeOrigin::kAuthor});

  ASSERT_EQ(2u, result.GetMatchedProperties().size());

  {
    auto e = ExpansionAt(result, 0);
    ASSERT_EQ(2u, e.size());
    EXPECT_EQ(CSSPropertyName(AtomicString("--x")),
              e[0]->ref.GetProperty().GetCSSPropertyName());
    EXPECT_EQ(CSSPropertyID::kVariable, e[0]->ref.GetProperty().PropertyID());
    EXPECT_EQ(CSSPropertyName(AtomicString("--y")),
              e[1]->ref.GetProperty().GetCSSPropertyName());
    EXPECT_EQ(CSSPropertyID::kVariable, e[1]->ref.GetProperty().PropertyID());
  }

  {
    auto e = ExpansionAt(result, 1);
    ASSERT_EQ(1u, e.size());
    EXPECT_EQ(CSSPropertyName(CSSPropertyID::kFloat),
              e[0]->ref.GetProperty().GetCSSPropertyName());
    EXPECT_EQ(CSSPropertyID::kFloat, e[0]->ref.GetProperty().PropertyID());
  }
}

TEST_F(CascadeExpansionTest, LinkOmitted) {
  MatchResult result;
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(ParseDeclarationBlock("color:red"),
                              {
                                  .link_match_type = CSSSelector::kMatchVisited,
                                  .origin = CascadeOrigin::kAuthor,
                              });

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  auto e = ExpansionAt(result, 0);
  ASSERT_EQ(1u, e.size());
  EXPECT_EQ(CSSPropertyID::kInternalVisitedColor,
            e[0]->ref.GetProperty().PropertyID());
}

TEST_F(CascadeExpansionTest, InternalVisited) {
  MatchResult result;
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(ParseDeclarationBlock("color:red"),
                              {.origin = CascadeOrigin::kAuthor});

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  auto e = ExpansionAt(result, 0);
  ASSERT_EQ(2u, e.size());
  EXPECT_EQ(CSSPropertyID::kColor, e[0]->ref.GetProperty().PropertyID());
  EXPECT_EQ(CSSPropertyID::kInternalVisitedColor,
            e[1]->ref.GetProperty().PropertyID());
}

TEST_F(CascadeExpansionTest, InternalVisitedOmitted) {
  MatchResult result;
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(ParseDeclarationBlock("color:red"),
                              {
                                  .link_match_type = CSSSelector::kMatchLink,
                                  .origin = CascadeOrigin::kAuthor,
                              });

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  auto e = ExpansionAt(result, 0);
  ASSERT_EQ(1u, e.size());
  EXPECT_EQ(CSSPropertyID::kColor, e[0]->ref.GetProperty().PropertyID());
}

TEST_F(CascadeExpansionTest, InternalVisitedWithTrailer) {
  MatchResult result;
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(ParseDeclarationBlock("color:red;left:1px"),
                              {.origin = CascadeOrigin::kAuthor});

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  auto e = ExpansionAt(result, 0);
  ASSERT_EQ(3u, e.size());
  EXPECT_EQ(CSSPropertyID::kColor, e[0]->ref.GetProperty().PropertyID());
  EXPECT_EQ(CSSPropertyID::kInternalVisitedColor,
            e[1]->ref.GetProperty().PropertyID());
  EXPECT_EQ(CSSPropertyID::kLeft, e[2]->ref.GetProperty().PropertyID());
}

TEST_F(CascadeExpansionTest, All) {
  MatchResult result;
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(ParseDeclarationBlock("all:unset"),
                              {.origin = CascadeOrigin::kAuthor});

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  const Vector<CSSPropertyID> all = AllProperties();
  auto e = ExpansionAt(result, 0);

  ASSERT_EQ(all.size(), e.size());

  int index = 0;
  for (CSSPropertyID expected : all) {
    EXPECT_EQ(expected, e[index++]->ref.GetProperty().PropertyID());
  }
}

TEST_F(CascadeExpansionTest, InlineAll) {
  MatchResult result;
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(
      ParseDeclarationBlock("left:1px;all:unset;right:1px"),
      {.origin = CascadeOrigin::kAuthor});

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  const Vector<CSSPropertyID> all = AllProperties();

  auto e = ExpansionAt(result, 0);
  ASSERT_EQ(all.size() + 2, e.size());

  EXPECT_EQ(CSSPropertyID::kLeft, e[0]->ref.GetProperty().PropertyID());

  int index = 1;
  for (CSSPropertyID expected : all) {
    EXPECT_EQ(expected, e[index++]->ref.GetProperty().PropertyID());
  }

  EXPECT_EQ(CSSPropertyID::kRight, e[index++]->ref.GetProperty().PropertyID());
}

TEST_F(CascadeExpansionTest, FilterFirstLetter) {
  MatchResult result;
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(
      ParseDeclarationBlock("object-fit:unset;font-size:1px"),
      {
          .valid_property_filter =
              static_cast<uint8_t>(ValidPropertyFilter::kFirstLetter),
          .origin = CascadeOrigin::kAuthor,
      });

  auto e = ExpansionAt(result, 0);
  ASSERT_EQ(1u, e.size());
  EXPECT_EQ(CSSPropertyID::kFontSize, e[0]->ref.GetProperty().PropertyID());
}

TEST_F(CascadeExpansionTest, FilterFirstLine) {
  MatchResult result;
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(
      ParseDeclarationBlock("display:none;font-size:1px"),
      {
          .valid_property_filter =
              static_cast<uint8_t>(ValidPropertyFilter::kFirstLine),
          .origin = CascadeOrigin::kAuthor,
      });

  auto e = ExpansionAt(result, 0);
  ASSERT_EQ(1u, e.size());
  EXPECT_EQ(CSSPropertyID::kFontSize, e[0]->ref.GetProperty().PropertyID());
}

TEST_F(CascadeExpansionTest, FilterCue) {
  MatchResult result;
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(
      ParseDeclarationBlock("object-fit:unset;font-size:1px"),
      {
          .valid_property_filter =
              static_cast<uint8_t>(ValidPropertyFilter::kCue),
          .origin = CascadeOrigin::kAuthor,
      });

  auto e = ExpansionAt(result, 0);
  ASSERT_EQ(1u, e.size());
  EXPECT_EQ(CSSPropertyID::kFontSize, e[0]->ref.GetProperty().PropertyID());
}

TEST_F(CascadeExpansionTest, FilterMarker) {
  MatchResult result;
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(
      ParseDeclarationBlock("object-fit:unset;font-size:1px"),
      {
          .valid_property_filter =
              static_cast<uint8_t>(ValidPropertyFilter::kMarker),
          .origin = CascadeOrigin::kAuthor,
      });

  auto e = ExpansionAt(result, 0);
  ASSERT_EQ(1u, e.size());
  EXPECT_EQ(CSSPropertyID::kFontSize, e[0]->ref.GetProperty().PropertyID());
}

TEST_F(CascadeExpansionTest, FilterHighlightLegacy) {
  MatchResult result;
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(
      ParseDeclarationBlock(
          "display:block;background-color:lime;forced-color-adjust:none"),
      {
          .valid_property_filter =
              static_cast<uint8_t>(ValidPropertyFilter::kHighlightLegacy),
          .origin = CascadeOrigin::kAuthor,
      });

  auto e = ExpansionAt(result, 0);
  ASSERT_EQ(3u, e.size());
  EXPECT_EQ(CSSPropertyID::kBackgroundColor,
            e[0]->ref.GetProperty().PropertyID());
  EXPECT_EQ(CSSPropertyID::kInternalVisitedBackgroundColor,
            e[1]->ref.GetProperty().PropertyID());
  EXPECT_EQ(CSSPropertyID::kForcedColorAdjust,
            e[2]->ref.GetProperty().PropertyID());
}

TEST_F(CascadeExpansionTest, FilterHighlight) {
  MatchResult result;
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(
      ParseDeclarationBlock(
          "display:block;background-color:lime;forced-color-adjust:none"),
      {
          .valid_property_filter =
              static_cast<uint8_t>(ValidPropertyFilter::kHighlight),
          .origin = CascadeOrigin::kAuthor,
      });

  auto e = ExpansionAt(result, 0);
  ASSERT_EQ(2u, e.size());
  EXPECT_EQ(CSSPropertyID::kBackgroundColor,
            e[0]->ref.GetProperty().PropertyID());
  EXPECT_EQ(CSSPropertyID::kInternalVisitedBackgroundColor,
            e[1]->ref.GetProperty().PropertyID());
}

TEST_F(CascadeExpansionTest, FilterPositionFallback) {
  MatchResult result;
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(
      ParseDeclarationBlock("display:inline;position:static;left:auto"),
      {
          .valid_property_filter =
              static_cast<uint8_t>(ValidPropertyFilter::kPositionTry),
          .origin = CascadeOrigin::kAuthor,
      });
  auto e = ExpansionAt(result, 0);
  ASSERT_EQ(1u, e.size());
  EXPECT_EQ(CSSPropertyID::kLeft, e[0]->ref.GetProperty().PropertyID());
}

TEST_F(CascadeExpansionTest, Importance) {
  MatchResult result;
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(
      ParseDeclarationBlock("cursor:help;display:block !important"),
      {.origin = CascadeOrigin::kAuthor});

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  auto e = ExpansionAt(result, 0);
  ASSERT_EQ(2u, e.size());

  EXPECT_EQ(CSSPropertyID::kCursor, e[0]->ref.GetProperty().PropertyID());
  EXPECT_FALSE(e[0]->priority.IsImportant());
  EXPECT_EQ(CSSPropertyID::kDisplay, e[1]->ref.GetProperty().PropertyID());
  EXPECT_TRUE(e[1]->priority.IsImportant());
}

TEST_F(CascadeExpansionTest, AllImportance) {
  MatchResult result;
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(ParseDeclarationBlock("all:unset !important"),
                              {.origin = CascadeOrigin::kAuthor});

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  const Vector<CSSPropertyID> all = AllProperties();
  auto e = ExpansionAt(result, 0);
  ASSERT_EQ(all.size(), e.size());

  int index = 0;
  for (CSSPropertyID expected : AllProperties()) {
    EXPECT_EQ(expected, e[index]->ref.GetProperty().PropertyID());
    EXPECT_TRUE(e[index]->priority.IsImportant());
    ++index;
  }
}

TEST_F(CascadeExpansionTest, AllNonImportance) {
  MatchResult result;
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(ParseDeclarationBlock("all:unset"),
                              {.origin = CascadeOrigin::kAuthor});

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  const Vector<CSSPropertyID> all = AllProperties();
  auto e = ExpansionAt(result, 0);
  ASSERT_EQ(all.size(), e.size());

  int index = 0;
  for (CSSPropertyID expected : AllProperties()) {
    EXPECT_EQ(expected, e[index]->ref.GetProperty().PropertyID());
    EXPECT_FALSE(e[index]->priority.IsImportant());
    ++index;
  }
}

TEST_F(CascadeExpansionTest, AllVisitedOnly) {
  MatchResult result;
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(ParseDeclarationBlock("all:unset"),
                              {
                                  .link_match_type = CSSSelector::kMatchVisited,
                                  .valid_property_filter = static_cast<uint8_t>(
                                      ValidPropertyFilter::kNoFilter),
                                  .origin = CascadeOrigin::kAuthor,
                              });

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  Vector<CSSPropertyID> visited =
      VisitedPropertiesInExpansion(result.GetMatchedProperties()[0], 0);

  for (CSSPropertyID id : kVisitedPropertySamples) {
    EXPECT_TRUE(visited.Contains(id))
        << CSSProperty::Get(id).GetPropertyNameString()
        << " should be in the expansion";
  }
}

TEST_F(CascadeExpansionTest, AllVisitedOrLink) {
  MatchResult result;
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(ParseDeclarationBlock("all:unset"),
                              {
                                  .link_match_type = CSSSelector::kMatchAll,
                                  .valid_property_filter = static_cast<uint8_t>(
                                      ValidPropertyFilter::kNoFilter),
                                  .origin = CascadeOrigin::kAuthor,
                              });

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  Vector<CSSPropertyID> visited =
      VisitedPropertiesInExpansion(result.GetMatchedProperties()[0], 0);

  for (CSSPropertyID id : kVisitedPropertySamples) {
    EXPECT_TRUE(visited.Contains(id))
        << CSSProperty::Get(id).GetPropertyNameString()
        << " should be in the expansion";
  }
}

TEST_F(CascadeExpansionTest, AllLinkOnly) {
  MatchResult result;
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(ParseDeclarationBlock("all:unset"),
                              {
                                  .link_match_type = CSSSelector::kMatchLink,
                                  .valid_property_filter = static_cast<uint8_t>(
                                      ValidPropertyFilter::kNoFilter),
                                  .origin = CascadeOrigin::kAuthor,
                              });

  ASSERT_EQ(1u, result.GetMatchedProperties().size());

  Vector<CSSPropertyID> visited =
      VisitedPropertiesInExpansion(result.GetMatchedProperties()[0], 0);
  EXPECT_EQ(visited.size(), 0u);
}

TEST_F(CascadeExpansionTest, Position) {
  MatchResult result;
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(ParseDeclarationBlock("left:1px;top:1px"),
                              {.origin = CascadeOrigin::kAuthor});
  result.AddMatchedProperties(ParseDeclarationBlock("bottom:1px;right:1px"),
                              {.origin = CascadeOrigin::kAuthor});

  ASSERT_EQ(2u, result.GetMatchedProperties().size());

  {
    auto e = ExpansionAt(result, 0);
    ASSERT_EQ(2u, e.size());

    EXPECT_EQ(CSSPropertyID::kLeft, e[0]->ref.GetProperty().PropertyID());
    EXPECT_EQ(0u, DecodeMatchedPropertiesIndex(e[0]->priority.GetPosition()));
    EXPECT_EQ(0u, DecodeDeclarationIndex(e[0]->priority.GetPosition()));
    EXPECT_EQ(CSSPropertyID::kTop, e[1]->ref.GetProperty().PropertyID());
    EXPECT_EQ(0u, DecodeMatchedPropertiesIndex(e[1]->priority.GetPosition()));
    EXPECT_EQ(1u, DecodeDeclarationIndex(e[1]->priority.GetPosition()));
  }

  {
    auto e = ExpansionAt(result, 1);
    ASSERT_EQ(2u, e.size());

    EXPECT_EQ(CSSPropertyID::kBottom, e[0]->ref.GetProperty().PropertyID());
    EXPECT_EQ(1u, DecodeMatchedPropertiesIndex(e[0]->priority.GetPosition()));
    EXPECT_EQ(0u, DecodeDeclarationIndex(e[0]->priority.GetPosition()));
    EXPECT_EQ(CSSPropertyID::kRight, e[1]->ref.GetProperty().PropertyID());
    EXPECT_EQ(1u, DecodeMatchedPropertiesIndex(e[1]->priority.GetPosition()));
    EXPECT_EQ(1u, DecodeDeclarationIndex(e[1]->priority.GetPosition()));
  }
}

TEST_F(CascadeExpansionTest, MatchedPropertiesLimit) {
  constexpr wtf_size_t max = std::numeric_limits<uint16_t>::max();

  static_assert(kMaxMatchedPropertiesIndex == max,
                "Unexpected max. If the limit increased, evaluate whether it "
                "still makes sense to run this test");

  auto* set = ParseDeclarationBlock("left:1px");

  MatchResult result;
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  for (wtf_size_t i = 0; i < max + 3; ++i) {
    result.AddMatchedProperties(set, {.origin = CascadeOrigin::kAuthor});
  }

  ASSERT_EQ(max + 3u, result.GetMatchedProperties().size());

  for (wtf_size_t i = 0; i < max + 1; ++i) {
    EXPECT_GT(ExpansionAt(result, i).size(), 0u);
  }

  // The indices beyond the max should not yield anything.
  EXPECT_EQ(0u, ExpansionAt(result, max + 1).size());
  EXPECT_EQ(0u, ExpansionAt(result, max + 2).size());
}

TEST_F(CascadeExpansionTest, MatchedDeclarationsLimit) {
  constexpr wtf_size_t max = std::numeric_limits<uint16_t>::max();

  static_assert(kMaxDeclarationIndex == max,
                "Unexpected max. If the limit increased, evaluate whether it "
                "still makes sense to run this test");

  HeapVector<CSSPropertyValue> declarations(max + 2);

  // Actually give the indexes a value, such that the calls to
  // ExpansionAt() does not crash.
  for (wtf_size_t i = 0; i < max + 1; ++i) {
    declarations[i] = CSSPropertyValue(CSSPropertyName(CSSPropertyID::kColor),
                                       *cssvalue::CSSUnsetValue::Create());
  }

  MatchResult result;
  result.BeginAddingAuthorRulesForTreeScope(GetDocument());
  result.AddMatchedProperties(
      ImmutableCSSPropertyValueSet::Create(declarations.data(), max + 1,
                                           kHTMLStandardMode),
      {.origin = CascadeOrigin::kAuthor});
  result.AddMatchedProperties(
      ImmutableCSSPropertyValueSet::Create(declarations.data(), max + 2,
                                           kHTMLStandardMode),
      {.origin = CascadeOrigin::kAuthor});

  EXPECT_GT(ExpansionAt(result, 0).size(), 0u);
  EXPECT_EQ(ExpansionAt(result, 1).size(), 0u);
}

}  // namespace blink
