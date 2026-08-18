// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_property_value_set.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class CSSPropertyValueSetTest : public PageTestBase {
 public:
  StyleRule* RuleAt(StyleSheetContents* sheet, wtf_size_t index) {
    return To<StyleRule>(sheet->ChildRules()[index].Get());
  }
};

TEST_F(CSSPropertyValueSetTest, MergeAndOverrideOnConflictCustomProperty) {
  auto* context = MakeGarbageCollected<CSSParserContext>(GetDocument());
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);

  String sheet_text = R"CSS(
    #first {
      color: red;
      --x:foo;
      --y:foo;
    }
    #second {
      color: green;
      --x:bar;
      --y:bar;
    }
  )CSS";

  CSSParser::ParseSheet(context, style_sheet, sheet_text,
                        CSSDeferPropertyParsing::kNo);
  StyleRule* rule0 = RuleAt(style_sheet, 0);
  StyleRule* rule1 = RuleAt(style_sheet, 1);
  MutableCSSPropertyValueSet& set0 = rule0->MutableProperties();
  MutableCSSPropertyValueSet& set1 = rule1->MutableProperties();

  EXPECT_EQ(3u, set0.PropertyCount());
  EXPECT_EQ("red", set0.GetPropertyValue(CSSPropertyID::kColor));
  EXPECT_EQ("foo", set0.GetPropertyValue(AtomicString("--x")));
  EXPECT_EQ("foo", set0.GetPropertyValue(AtomicString("--y")));
  EXPECT_EQ(3u, set1.PropertyCount());
  EXPECT_EQ("green", set1.GetPropertyValue(CSSPropertyID::kColor));
  EXPECT_EQ("bar", set1.GetPropertyValue(AtomicString("--x")));
  EXPECT_EQ("bar", set1.GetPropertyValue(AtomicString("--y")));

  set0.MergeAndOverrideOnConflict(&set1);

  EXPECT_EQ(3u, set0.PropertyCount());
  EXPECT_EQ("green", set0.GetPropertyValue(CSSPropertyID::kColor));
  EXPECT_EQ("bar", set0.GetPropertyValue(AtomicString("--x")));
  EXPECT_EQ("bar", set0.GetPropertyValue(AtomicString("--y")));
  EXPECT_EQ(3u, set1.PropertyCount());
  EXPECT_EQ("green", set1.GetPropertyValue(CSSPropertyID::kColor));
  EXPECT_EQ("bar", set1.GetPropertyValue(AtomicString("--x")));
  EXPECT_EQ("bar", set1.GetPropertyValue(AtomicString("--y")));
}

// https://crbug.com/1292163
TEST_F(CSSPropertyValueSetTest, ConflictingLonghandAndShorthand) {
  auto* context = MakeGarbageCollected<CSSParserContext>(GetDocument());
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);

  String sheet_text = R"CSS(
    #first {
      offset: none reverse 2turn;
      offset-path: initial;
    }
  )CSS";

  CSSParser::ParseSheet(context, style_sheet, sheet_text,
                        CSSDeferPropertyParsing::kNo);
  StyleRule* rule = RuleAt(style_sheet, 0);

  EXPECT_EQ(
      "offset-position: normal; offset-distance: 0px; "
      "offset-rotate: reverse 2turn; offset-anchor: auto; "
      "offset-path: initial;",
      rule->Properties().AsText());
}

TEST_F(CSSPropertyValueSetTest, TimelineShorthandWithMismatchedListLengths) {
  auto* properties =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);

  auto set_property = [&](CSSPropertyID property, const String& value) {
    return properties->ParseAndSetProperty(property, value, /*important=*/false,
                                           SecureContextMode::kInsecureContext,
                                           /*context_style_sheet=*/nullptr);
  };

  ASSERT_NE(MutableCSSPropertyValueSet::kParseError,
            set_property(CSSPropertyID::kScrollTimelineName, "--a, --b, --c"));
  ASSERT_NE(MutableCSSPropertyValueSet::kParseError,
            set_property(CSSPropertyID::kScrollTimelineAxis, "inline, inline"));
  EXPECT_EQ("", properties->GetPropertyValue(CSSPropertyID::kScrollTimeline));

  ASSERT_NE(MutableCSSPropertyValueSet::kParseError,
            set_property(CSSPropertyID::kScrollTimelineName, "--a, --b"));
  ASSERT_NE(MutableCSSPropertyValueSet::kParseError,
            set_property(CSSPropertyID::kScrollTimelineAxis,
                         "inline, inline, inline"));
  EXPECT_EQ("", properties->GetPropertyValue(CSSPropertyID::kScrollTimeline));

  ASSERT_NE(MutableCSSPropertyValueSet::kParseError,
            set_property(CSSPropertyID::kViewTimelineName, "--a, --b"));
  ASSERT_NE(MutableCSSPropertyValueSet::kParseError,
            set_property(CSSPropertyID::kViewTimelineAxis, "inline, inline"));
  ASSERT_NE(
      MutableCSSPropertyValueSet::kParseError,
      set_property(CSSPropertyID::kViewTimelineInset, "auto, auto, auto"));
  EXPECT_EQ("", properties->GetPropertyValue(CSSPropertyID::kViewTimeline));
}

TEST_F(CSSPropertyValueSetTest, TimelineShorthandWithInitialLonghands) {
  auto* properties =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);

  auto set_property = [&](CSSPropertyID property, const String& value) {
    return properties->ParseAndSetProperty(property, value, /*important=*/false,
                                           SecureContextMode::kInsecureContext,
                                           /*context_style_sheet=*/nullptr);
  };

  ASSERT_NE(MutableCSSPropertyValueSet::kParseError,
            set_property(CSSPropertyID::kScrollTimelineName, "--a, --b"));
  ASSERT_NE(MutableCSSPropertyValueSet::kParseError,
            set_property(CSSPropertyID::kScrollTimelineAxis, "block"));
  EXPECT_EQ("--a, --b",
            properties->GetPropertyValue(CSSPropertyID::kScrollTimeline));

  ASSERT_NE(MutableCSSPropertyValueSet::kParseError,
            set_property(CSSPropertyID::kViewTimelineName, "--a, --b"));
  ASSERT_NE(MutableCSSPropertyValueSet::kParseError,
            set_property(CSSPropertyID::kViewTimelineAxis, "block"));
  ASSERT_NE(MutableCSSPropertyValueSet::kParseError,
            set_property(CSSPropertyID::kViewTimelineInset, "1px, 2px"));
  EXPECT_EQ("--a 1px, --b 2px",
            properties->GetPropertyValue(CSSPropertyID::kViewTimeline));

  ASSERT_NE(MutableCSSPropertyValueSet::kParseError,
            set_property(CSSPropertyID::kViewTimelineAxis, "inline, inline"));
  ASSERT_NE(MutableCSSPropertyValueSet::kParseError,
            set_property(CSSPropertyID::kViewTimelineInset, "auto"));
  EXPECT_EQ("--a inline, --b inline",
            properties->GetPropertyValue(CSSPropertyID::kViewTimeline));
}

TEST_F(CSSPropertyValueSetTest, SetPropertyReturnValue) {
  MutableCSSPropertyValueSet* properties =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  EXPECT_EQ(MutableCSSPropertyValueSet::kChangedPropertySet,
            properties->ParseAndSetProperty(CSSPropertyID::kColor, "red",
                                            /*important=*/false,
                                            SecureContextMode::kInsecureContext,
                                            /*context_style_sheet=*/nullptr));
  EXPECT_EQ(MutableCSSPropertyValueSet::kUnchanged,
            properties->ParseAndSetProperty(CSSPropertyID::kColor, "red",
                                            /*important=*/false,
                                            SecureContextMode::kInsecureContext,
                                            /*context_style_sheet=*/nullptr));
  EXPECT_EQ(MutableCSSPropertyValueSet::kChangedPropertySet,
            properties->ParseAndSetProperty(
                CSSPropertyID::kBackgroundColor, "white",
                /*important=*/false, SecureContextMode::kInsecureContext,
                /*context_style_sheet=*/nullptr));
  EXPECT_EQ(MutableCSSPropertyValueSet::kModifiedExisting,
            properties->ParseAndSetProperty(CSSPropertyID::kColor, "green",
                                            /*important=*/false,
                                            SecureContextMode::kInsecureContext,
                                            /*context_style_sheet=*/nullptr));
  EXPECT_EQ(MutableCSSPropertyValueSet::kChangedPropertySet,
            properties->ParseAndSetProperty(CSSPropertyID::kColor, "",
                                            /*important=*/false,
                                            SecureContextMode::kInsecureContext,
                                            /*context_style_sheet=*/nullptr));
}

TEST_F(CSSPropertyValueSetTest, SetCustomPropertyReturnValue) {
  MutableCSSPropertyValueSet* properties =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  EXPECT_EQ(MutableCSSPropertyValueSet::kChangedPropertySet,
            properties->ParseAndSetCustomProperty(
                AtomicString("--my-property"), "red", /*important=*/false,
                SecureContextMode::kInsecureContext,
                /*context_style_sheet=*/nullptr,
                /*is_animation_tainted=*/false));

  // Custom property values are compared by value, so we get a kUnchanged
  // return value here.
  EXPECT_EQ(MutableCSSPropertyValueSet::kUnchanged,
            properties->ParseAndSetCustomProperty(
                AtomicString("--my-property"), "red", /*important=*/false,
                SecureContextMode::kInsecureContext,
                /*context_style_sheet=*/nullptr,
                /*is_animation_tainted=*/false));

  EXPECT_EQ(MutableCSSPropertyValueSet::kChangedPropertySet,
            properties->ParseAndSetCustomProperty(
                AtomicString("--your-property"), "white",
                /*important=*/false, SecureContextMode::kInsecureContext,
                /*context_style_sheet=*/nullptr,
                /*is_animation_tainted=*/false));
  EXPECT_EQ(MutableCSSPropertyValueSet::kModifiedExisting,
            properties->ParseAndSetCustomProperty(
                AtomicString("--my-property"), "green",
                /*important=*/false, SecureContextMode::kInsecureContext,
                /*context_style_sheet=*/nullptr,
                /*is_animation_tainted=*/false));
  EXPECT_EQ(MutableCSSPropertyValueSet::kChangedPropertySet,
            properties->ParseAndSetCustomProperty(
                AtomicString("--my-property"), "", /*important=*/false,
                SecureContextMode::kInsecureContext,
                /*context_style_sheet=*/nullptr,
                /*is_animation_tainted=*/false));
}

TEST_F(CSSPropertyValueSetTest, RemoveEquivalentProperties) {
  auto* context = MakeGarbageCollected<CSSParserContext>(GetDocument());
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);

  String sheet_text = R"CSS(
    #first {
      color: red;
      width: 10px;
      --x:foo;
    }
    #second {
      color: red;
      width: 20px;
      --x:foo;
    }
  )CSS";

  CSSParser::ParseSheet(context, style_sheet, sheet_text,
                        CSSDeferPropertyParsing::kNo);
  MutableCSSPropertyValueSet& set0 = RuleAt(style_sheet, 0)->MutableProperties();
  MutableCSSPropertyValueSet& set1 = RuleAt(style_sheet, 1)->MutableProperties();

  // 'color' is equivalent in both sets and should be removed; 'width' differs
  // and should be kept. Custom properties are kept unconditionally, so '--x'
  // remains even though its value matches.
  set0.RemoveEquivalentProperties(&set1);

  EXPECT_EQ(2u, set0.PropertyCount());
  EXPECT_FALSE(set0.HasProperty(CSSPropertyID::kColor));
  EXPECT_EQ("10px", set0.GetPropertyValue(CSSPropertyID::kWidth));
  // The custom property is preserved even though it had the same value in both
  // sets, because PropertyMatches() cannot disambiguate custom properties.
  EXPECT_EQ("foo", set0.GetPropertyValue(AtomicString("--x")));
}

// Removing an equivalent 'all' must drop only 'all' (and clear the HasAll
// bit), leaving non-equivalent longhands intact.
TEST_F(CSSPropertyValueSetTest, RemoveEquivalentPropertiesWithAll) {
  auto* context = MakeGarbageCollected<CSSParserContext>(GetDocument());
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(context);

  String sheet_text = R"CSS(
    #first {
      all: revert;
      width: 10px;
    }
    #second {
      all: revert;
      width: 20px;
    }
  )CSS";

  CSSParser::ParseSheet(context, style_sheet, sheet_text,
                        CSSDeferPropertyParsing::kNo);
  MutableCSSPropertyValueSet& set0 = RuleAt(style_sheet, 0)->MutableProperties();
  MutableCSSPropertyValueSet& set1 = RuleAt(style_sheet, 1)->MutableProperties();

  // 'all' was equivalent and removed; 'width' differs and is kept.
  ASSERT_TRUE(set0.HasProperty(CSSPropertyID::kAll));
  ASSERT_TRUE(set0.HasAllProperty());
  set0.RemoveEquivalentProperties(&set1);

  EXPECT_EQ(1u, set0.PropertyCount());
  EXPECT_FALSE(set0.HasProperty(CSSPropertyID::kAll));
  // The HasAll bit must be cleared, not just the entry in the property vector.
  EXPECT_FALSE(set0.HasAllProperty());
  EXPECT_EQ("10px", set0.GetPropertyValue(CSSPropertyID::kWidth));
}

}  // namespace blink
