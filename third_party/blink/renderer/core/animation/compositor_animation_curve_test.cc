// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/compositor_animation_curve.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/compositor_animation_color_curve.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class CompositorAnimationCurveTest : public PageTestBase {
 public:
  CompositorAnimationCurveTest() = default;

  using ColorKeyframe = CompositorAnimationColorCurve::TypedKeyframe;

  // Each test is expected to create a single animation, though the animation
  // may affect multiple properties.
  Animation* GetAnimation() {
    Element* target = GetDocument().getElementById(AtomicString("target"));
    UpdateAllLifecyclePhasesForTest();
    ElementAnimations* element_animations = target->GetElementAnimations();
    EXPECT_TRUE(element_animations);
    EXPECT_EQ(element_animations->Animations().size(), 1u);
    return element_animations->Animations().begin()->key;
  }

  scoped_refptr<CompositorAnimationColorCurve> ExtractColorCurve(
      CSSPropertyID id) {
    Animation* animation = GetAnimation();
    return CompositorAnimationColorCurve::Create(animation,
                                                 CSSPropertyName(id));
  }
};

TEST_F(CompositorAnimationCurveTest, KeywordColors) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes colorize {
        from { background-color: black; }
        to { background-color: white; }
      }
      #target {
        height: 100px;
        width: 100x;
        animation: colorize 1s linear;
      }
    </style>
    <div id='target'></div>
  )HTML");

  scoped_refptr<CompositorAnimationColorCurve> curve =
      ExtractColorCurve(CSSPropertyID::kBackgroundColor);
  EXPECT_EQ(curve->Size(), 2u);
  EXPECT_FALSE(curve->HasStyleDependency());

  const ColorKeyframe& first = curve->GetTypedKeyframe(0);
  const ColorKeyframe& last = curve->GetTypedKeyframe(1);

  EXPECT_EQ(first.offset, 0);
  EXPECT_EQ(first.value, Color::kBlack);
  EXPECT_EQ(last.offset, 1);
  EXPECT_EQ(last.value, Color::kWhite);

  // If we force and update anyways, we get a new smart pointer to the same
  // underlying object.
  scoped_refptr<CompositorAnimationCurve> updated_curve =
      curve->UpdateKeyframeSnapshot(GetAnimation());
  EXPECT_EQ(curve.get(), updated_curve.get());
}

TEST_F(CompositorAnimationCurveTest, VariableSubstitedColors) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes colorize {
        from { background-color: var(--color1); }
        to { background-color: var(--color2); }
      }
      #target {
        --color1: red;
        --color2: green;
        height: 100px;
        width: 100x;
        animation: colorize 1s linear;
      }
      #target.updated {
        --color1: black;
        --color2: white;
      }
    }
    </style>
    <div id='target'></div>
  )HTML");

  scoped_refptr<CompositorAnimationColorCurve> curve =
      ExtractColorCurve(CSSPropertyID::kBackgroundColor);
  EXPECT_EQ(curve->Size(), 2u);
  EXPECT_TRUE(curve->HasStyleDependency());

  const ColorKeyframe& first = curve->GetTypedKeyframe(0);
  const ColorKeyframe& last = curve->GetTypedKeyframe(1);

  // Variable references are resolved.
  EXPECT_EQ(first.offset, 0);
  EXPECT_EQ(first.value, Color(255, 0, 0));
  EXPECT_EQ(last.offset, 1);
  EXPECT_EQ(last.value, Color(0, 128, 0));

  // None of the keyframe values were affected by the snapshot update.
  // both smart pointers refer to the same underlying  object.
  scoped_refptr<CompositorAnimationCurve> updated_curve =
      curve->UpdateKeyframeSnapshot(GetAnimation());
  EXPECT_EQ(curve.get(), updated_curve.get());

  GetElementById("target")->classList().add({"updated"}, ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  // This time the snapshot update generates a new curve.
  updated_curve = curve->UpdateKeyframeSnapshot(GetAnimation());
  EXPECT_NE(curve.get(), updated_curve.get());
  EXPECT_EQ(curve->GetTypedKeyframe(0).value, Color(255, 0, 0));
  EXPECT_EQ(curve->GetTypedKeyframe(1).value, Color(0, 128, 0));
  CompositorAnimationColorCurve* color_curve =
      static_cast<CompositorAnimationColorCurve*>(updated_curve.get());
  EXPECT_EQ(color_curve->GetTypedKeyframe(0).value, Color::kBlack);
  EXPECT_EQ(color_curve->GetTypedKeyframe(1).value, Color::kWhite);
}

TEST_F(CompositorAnimationCurveTest, UnsupportedColorValue) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes text-reveal {
        from { background-color: currentcolor; }
        to { background-color: transparent; }
      }
      #target {
        height: 100px;
        width: 100x;
        animation: text-reveal 1s linear;
      }
    }
    </style>
    <div id='target'></div>
  )HTML");

  scoped_refptr<CompositorAnimationColorCurve> curve =
      ExtractColorCurve(CSSPropertyID::kBackgroundColor);
  EXPECT_TRUE(!curve);
}

TEST_F(CompositorAnimationCurveTest, ColorCurveWithNeutralKeyframe) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes colorize {
        to { background-color: green; }
      }
      #target {
        height: 100px;
        width: 100x;
        background-color: red;
        animation: colorize 1s linear;
      }
    }
    </style>
    <div id='target'></div>
  )HTML");

  scoped_refptr<CompositorAnimationColorCurve> curve =
      ExtractColorCurve(CSSPropertyID::kBackgroundColor);
  EXPECT_TRUE(!curve);
}

}  // end namespace blink
