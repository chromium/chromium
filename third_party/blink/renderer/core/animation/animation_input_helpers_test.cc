// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animation_input_helpers.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"

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

  void TimingFunctionRoundTrips(const String& string,
                                ExceptionState& exception_state) {
    ASSERT_FALSE(exception_state.HadException());
    scoped_refptr<TimingFunction> timing_function =
        ParseTimingFunction(string, exception_state);
    EXPECT_FALSE(exception_state.HadException());
    EXPECT_NE(nullptr, timing_function);
    EXPECT_EQ(string, timing_function->ToString());
    exception_state.ClearException();
  }

  void TimingFunctionThrows(const String& string,
                            ExceptionState& exception_state) {
    ASSERT_FALSE(exception_state.HadException());
    scoped_refptr<TimingFunction> timing_function =
        ParseTimingFunction(string, exception_state);
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(ESErrorType::kTypeError, exception_state.CodeAs<ESErrorType>());
    exception_state.ClearException();
  }

 protected:
  void SetUp() override {
    PageTestBase::SetUp(IntSize());
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
  EXPECT_EQ(CSSPropertyID::kVariable, KeyframeAttributeToCSSProperty("--"));
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
  DummyExceptionStateForTesting exception_state;
  TimingFunctionThrows("", exception_state);
  TimingFunctionThrows("initial", exception_state);
  TimingFunctionThrows("inherit", exception_state);
  TimingFunctionThrows("unset", exception_state);

  TimingFunctionRoundTrips("ease", exception_state);
  TimingFunctionRoundTrips("linear", exception_state);
  TimingFunctionRoundTrips("ease-in", exception_state);
  TimingFunctionRoundTrips("ease-out", exception_state);
  TimingFunctionRoundTrips("ease-in-out", exception_state);
  TimingFunctionRoundTrips("cubic-bezier(0.1, 5, 0.23, 0)", exception_state);

  EXPECT_EQ("steps(1, start)",
            ParseTimingFunction("step-start", exception_state)->ToString());
  EXPECT_EQ("steps(1)",
            ParseTimingFunction("step-end", exception_state)->ToString());
  EXPECT_EQ(
      "steps(3, start)",
      ParseTimingFunction("steps(3, start)", exception_state)->ToString());
  EXPECT_EQ("steps(3)",
            ParseTimingFunction("steps(3, end)", exception_state)->ToString());
  EXPECT_EQ("steps(3)",
            ParseTimingFunction("steps(3)", exception_state)->ToString());

  TimingFunctionThrows("steps(3, nowhere)", exception_state);
  TimingFunctionThrows("steps(-3, end)", exception_state);
  TimingFunctionThrows("cubic-bezier(0.1, 0, 4, 0.4)", exception_state);
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
  EXPECT_EQ("--x", PropertyHandleToKeyframeAttribute("--x"));
  EXPECT_EQ("--test-prop", PropertyHandleToKeyframeAttribute("--test-prop"));

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
  EXPECT_EQ("calcMode", PropertyHandleToKeyframeAttribute(QualifiedName(
                            g_null_atom, "calcMode", g_null_atom)));
  EXPECT_EQ("overline-position",
            PropertyHandleToKeyframeAttribute(
                QualifiedName(g_null_atom, "overline-position", g_null_atom)));
}

}  // namespace blink
