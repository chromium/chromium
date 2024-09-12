// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animation_input_helpers.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/properties/shorthands.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

class AnimationAnimationInputHelpersTest : public PageTestBase {
 public:
  CSSPropertyID KeyframeAttributeToCSSProperty(const String& property) {
    return AnimationInputHelpers::KeyframeAttributeToCSSProperty(property,
                                                                 *document);
  }

  String PropertyHandleToKeyframeAttribute(
      const CSSProperty& property,
      bool is_presentation_attribute = false) {
    PropertyHandle handle(property, is_presentation_attribute);
    return AnimationInputHelpers::PropertyHandleToKeyframeAttribute(handle);
  }

  String PropertyHandleToKeyframeAttribute(AtomicString property) {
    PropertyHandle handle(property);
    return AnimationInputHelpers::PropertyHandleToKeyframeAttribute(handle);
  }

  String PropertyHandleToKeyframeAttribute(QualifiedName property) {
    PropertyHandle handle(property);
    return AnimationInputHelpers::PropertyHandleToKeyframeAttribute(handle);
  }

  scoped_refptr<TimingFunction> ParseTimingFunction(
      const String& string,
      ExceptionState& exception_state) {
    return AnimationInputHelpers::ParseTimingFunction(string, document,
                                                      exception_state);
  }

  void TimingFunctionRoundTrips(const String& string) {
    DummyExceptionStateForTesting exception_state;
    scoped_refptr<TimingFunction> timing_function =
        ParseTimingFunction(string, exception_state);
    EXPECT_FALSE(exception_state.HadException());
    EXPECT_NE(nullptr, timing_function);
    EXPECT_EQ(string, timing_function->ToString());
  }

  void TimingFunctionThrows(const String& string) {
    DummyExceptionStateForTesting exception_state;
    scoped_refptr<TimingFunction> timing_function =
        ParseTimingFunction(string, exception_state);
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(ESErrorType::kTypeError, exception_state.CodeAs<ESErrorType>());
  }

 protected:
  void SetUp() override {
    PageTestBase::SetUp(gfx::Size());
    document = &GetDocument();
  }

  void TearDown() override {
    document.Release();
    ThreadState::Current()->CollectAllGarbageForTesting();
  }

  Persistent<Document> document;
};

TEST_F(AnimationAnimationInputHelpersTest, ParseKeyframePropertyAttributes) {
  EXPECT_EQ(CSSPropertyID::kLineHeight,
            KeyframeAttributeToCSSProperty("lineHeight"));
  EXPECT_EQ(CSSPropertyID::kBorderTopWidth,
            KeyframeAttributeToCSSProperty("borderTopWidth"));
  EXPECT_EQ(CSSPropertyID::kWidth, KeyframeAttributeToCSSProperty("width"));
  EXPECT_EQ(CSSPropertyID::kFloat, KeyframeAttributeToCSSProperty("float"));
  EXPECT_EQ(CSSPropertyID::kFloat, KeyframeAttributeToCSSProperty("cssFloat"));
  EXPECT_EQ(CSSPropertyID::kInvalid, KeyframeAttributeToCSSProperty("--"));
  EXPECT_EQ(CSSPropertyID::kVariable, KeyframeAttributeToCSSProperty("---"));
  EXPECT_EQ(CSSPropertyID::kVariable, KeyframeAttributeToCSSProperty("--x"));
  EXPECT_EQ(CSSPropertyID::kVariable,
            KeyframeAttributeToCSSProperty("--webkit-custom-property"));

  EXPECT_EQ(CSSPropertyID::kInvalid, KeyframeAttributeToCSSProperty(""));
  EXPECT_EQ(CSSPropertyID::kInvalid, KeyframeAttributeToCSSProperty("-"));
  EXPECT_EQ(CSSPropertyID::kInvalid,
            KeyframeAttributeToCSSProperty("line-height"));
  EXPECT_EQ(CSSPropertyID::kInvalid,
            KeyframeAttributeToCSSProperty("border-topWidth"));
  EXPECT_EQ(CSSPropertyID::kInvalid, KeyframeAttributeToCSSProperty("Width"));
  EXPECT_EQ(CSSPropertyID::kInvalid,
            KeyframeAttributeToCSSProperty("-epub-text-transform"));
  EXPECT_EQ(CSSPropertyID::kInvalid,
            KeyframeAttributeToCSSProperty("EpubTextTransform"));
  EXPECT_EQ(CSSPropertyID::kInvalid,
            KeyframeAttributeToCSSProperty("-internal-marquee-repetition"));
  EXPECT_EQ(CSSPropertyID::kInvalid,
            KeyframeAttributeToCSSProperty("InternalMarqueeRepetition"));
  EXPECT_EQ(CSSPropertyID::kInvalid,
            KeyframeAttributeToCSSProperty("-webkit-filter"));
  EXPECT_EQ(CSSPropertyID::kInvalid,
            KeyframeAttributeToCSSProperty("-webkit-transform"));
  EXPECT_EQ(CSSPropertyID::kInvalid,
            KeyframeAttributeToCSSProperty("webkitTransform"));
  EXPECT_EQ(CSSPropertyID::kInvalid,
            KeyframeAttributeToCSSProperty("WebkitTransform"));
}

TEST_F(AnimationAnimationInputHelpersTest, ParseAnimationTimingFunction) {
  TimingFunctionThrows("");
  TimingFunctionThrows("initial");
  TimingFunctionThrows("inherit");
  TimingFunctionThrows("unset");

  TimingFunctionRoundTrips("ease");
  TimingFunctionRoundTrips("linear");
  TimingFunctionRoundTrips("ease-in");
  TimingFunctionRoundTrips("ease-out");
  TimingFunctionRoundTrips("ease-in-out");
  TimingFunctionRoundTrips("cubic-bezier(0.1, 5, 0.23, 0)");

  EXPECT_EQ("steps(1, start)",
            ParseTimingFunction("step-start", ASSERT_NO_EXCEPTION)->ToString());
  EXPECT_EQ("steps(1)",
            ParseTimingFunction("step-end", ASSERT_NO_EXCEPTION)->ToString());
  EXPECT_EQ(
      "steps(3, start)",
      ParseTimingFunction("steps(3, start)", ASSERT_NO_EXCEPTION)->ToString());
  EXPECT_EQ(
      "steps(3)",
      ParseTimingFunction("steps(3, end)", ASSERT_NO_EXCEPTION)->ToString());
  EXPECT_EQ("steps(3)",
            ParseTimingFunction("steps(3)", ASSERT_NO_EXCEPTION)->ToString());

  TimingFunctionThrows("steps(3, nowhere)");
  TimingFunctionThrows("steps(-3, end)");
  TimingFunctionThrows("cubic-bezier(0.1, 0, 4, 0.4)");
}

TEST_F(AnimationAnimationInputHelpersTest, PropertyHandleToKeyframeAttribute) {
  // CSS properties.
  EXPECT_EQ("top", PropertyHandleToKeyframeAttribute(GetCSSPropertyTop()));
  EXPECT_EQ("lineHeight",
            PropertyHandleToKeyframeAttribute(GetCSSPropertyLineHeight()));
  EXPECT_EQ("cssFloat",
            PropertyHandleToKeyframeAttribute(GetCSSPropertyFloat()));
  EXPECT_EQ("cssOffset",
            PropertyHandleToKeyframeAttribute(GetCSSPropertyOffset()));

  // CSS custom properties.
  EXPECT_EQ("--x", PropertyHandleToKeyframeAttribute(AtomicString("--x")));
  EXPECT_EQ("--test-prop",
            PropertyHandleToKeyframeAttribute(AtomicString("--test-prop")));

  // Presentation attributes.
  EXPECT_EQ("svg-top",
            PropertyHandleToKeyframeAttribute(GetCSSPropertyTop(), true));
  EXPECT_EQ("svg-line-height", PropertyHandleToKeyframeAttribute(
                                   GetCSSPropertyLineHeight(), true));
  EXPECT_EQ("svg-float",
            PropertyHandleToKeyframeAttribute(GetCSSPropertyFloat(), true));
  EXPECT_EQ("svg-offset",
            PropertyHandleToKeyframeAttribute(GetCSSPropertyOffset(), true));

  // SVG attributes.
  EXPECT_EQ("calcMode", PropertyHandleToKeyframeAttribute(
                            QualifiedName(AtomicString("calcMode"))));
  EXPECT_EQ("overline-position",
            PropertyHandleToKeyframeAttribute(
                QualifiedName(AtomicString("overline-position"))));
}

}  // namespace blink
