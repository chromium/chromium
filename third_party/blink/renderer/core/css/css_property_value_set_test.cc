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

  // Custom property values are compared by instance rather than by value
  // (due to performance constraints), so we don't get a kUnchanged
  // return value here.
  EXPECT_EQ(MutableCSSPropertyValueSet::kModifiedExisting,
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

}  // namespace blink
