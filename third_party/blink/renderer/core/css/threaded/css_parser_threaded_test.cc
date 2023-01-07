// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/threaded/multi_threaded_test_util.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class CSSParserThreadedTest : public MultiThreadedTest {
 public:
  static void TestSingle(CSSPropertyID prop, const String& text) {
    const CSSValue* value = CSSParser::ParseSingleValue(
        prop, text,
        StrictCSSParserContext(SecureContextMode::kInsecureContext));
    ASSERT_TRUE(value);
    EXPECT_EQ(text, value->CssText());
  }

  static MutableCSSPropertyValueSet* TestValue(CSSPropertyID prop,
                                               const String& text) {
    auto* style =
        MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
    CSSParser::ParseValue(style, prop, text, true);
    return style;
  }
};

TSAN_TEST_F(CSSParserThreadedTest, SinglePropertyFilter) {
  RunOnThreads([]() {
    TestSingle(CSSPropertyID::kFilter, "sepia(50%)");
    TestSingle(CSSPropertyID::kFilter, "blur(10px)");
    TestSingle(CSSPropertyID::kFilter, "brightness(50%) invert(100%)");
  });
}

TSAN_TEST_F(CSSParserThreadedTest, SinglePropertyFont) {
  RunOnThreads([]() {
    TestSingle(CSSPropertyID::kFontFamily, "serif");
    TestSingle(CSSPropertyID::kFontFamily, "monospace");
    TestSingle(CSSPropertyID::kFontFamily, "times");
    TestSingle(CSSPropertyID::kFontFamily, "arial");

    TestSingle(CSSPropertyID::kFontWeight, "normal");
    TestSingle(CSSPropertyID::kFontWeight, "bold");

    TestSingle(CSSPropertyID::kFontSize, "10px");
    TestSingle(CSSPropertyID::kFontSize, "20em");
  });
}

TSAN_TEST_F(CSSParserThreadedTest, ValuePropertyFont) {
  RunOnThreads([]() {
    MutableCSSPropertyValueSet* v =
        TestValue(CSSPropertyID::kFont, "15px arial");
    EXPECT_EQ(v->GetPropertyValue(CSSPropertyID::kFontFamily), "arial");
    EXPECT_EQ(v->GetPropertyValue(CSSPropertyID::kFontSize), "15px");
  });
}

TSAN_TEST_F(CSSParserThreadedTest, FontFaceDescriptor) {
  RunOnThreads([]() {
    auto* ctx = MakeGarbageCollected<CSSParserContext>(
        kCSSFontFaceRuleMode, SecureContextMode::kInsecureContext);
    const CSSValue* v = CSSParser::ParseFontFaceDescriptor(
        CSSPropertyID::kSrc, "url(myfont.ttf)", ctx);
    ASSERT_TRUE(v);
    EXPECT_EQ(v->CssText(), "url(\"myfont.ttf\")");
  });
}

}  // namespace blink
